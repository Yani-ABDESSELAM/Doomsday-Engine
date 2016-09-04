/** @file blockmapvisual.cpp  Graphical Blockmap Visual.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "render/blockmapvisual.h"

#include <de/aabox.h>
#include <de/concurrency.h>
#include <de/GLInfo>
#include <de/Vector>

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"

#include "world/blockmap.h"
#include "world/lineblockmap.h"
#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "ConvexSubspace"
#include "Face"
#include "HEdge"

#include "api_fontrender.h"
#include "render/rend_font.h"
#include "ui/ui_main.h"

using namespace de;
using namespace world;

byte bmapShowDebug; // 1 = mobjs, 2 = lines, 3 = BSP leafs, 4 = polyobjs. cvar
dfloat bmapDebugSize = 1.5f; // cvar

static void drawMobj(mobj_t const &mob)
{
    AABoxd const bounds = Mobj_Bounds(mob);

    LIBGUI_GL.glVertex2f(bounds.minX, bounds.minY);
    LIBGUI_GL.glVertex2f(bounds.maxX, bounds.minY);
    LIBGUI_GL.glVertex2f(bounds.maxX, bounds.maxY);
    LIBGUI_GL.glVertex2f(bounds.minX, bounds.maxY);
}

static void drawLine(Line const &line)
{
    LIBGUI_GL.glVertex2f(line.from().x(), line.from().y());
    LIBGUI_GL.glVertex2f(line.to  ().x(), line.to  ().y());
}

static void drawSubspace(ConvexSubspace const &subspace)
{
    dfloat const scale = de::max(bmapDebugSize, 1.f);
    dfloat const width = (DENG_GAMEVIEW_WIDTH / 16) / scale;

    Face const &poly = subspace.poly();
    HEdge *base = poly.hedge();
    HEdge *hedge = base;
    do
    {
        Vector2d start = hedge->origin();
        Vector2d end   = hedge->twin().origin();

        LIBGUI_GL.glBegin(GL_LINES);
            LIBGUI_GL.glVertex2f(start.x, start.y);
            LIBGUI_GL.glVertex2f(end.x, end.y);
        LIBGUI_GL.glEnd();

        ddouble length = (end - start).length();
        if (length > 0)
        {
            Vector2d const unit = (end - start) / length;
            Vector2d const normal(-unit.y, unit.x);

            GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC));
            glEnable(GL_TEXTURE_2D);
            GL_BlendMode(BM_ADD);

            LIBGUI_GL.glBegin(GL_QUADS);
                LIBGUI_GL.glTexCoord2f(0.75f, 0.5f);
                LIBGUI_GL.glVertex2f(start.x, start.y);
                LIBGUI_GL.glTexCoord2f(0.75f, 0.5f);
                LIBGUI_GL.glVertex2f(end.x, end.y);
                LIBGUI_GL.glTexCoord2f(0.75f, 1);
                LIBGUI_GL.glVertex2f(end.x - normal.x * width, end.y - normal.y * width);
                LIBGUI_GL.glTexCoord2f(0.75f, 1);
                LIBGUI_GL.glVertex2f(start.x - normal.x * width, start.y - normal.y * width);
            LIBGUI_GL.glEnd();

            glDisable(GL_TEXTURE_2D);
            GL_BlendMode(BM_NORMAL);
        }

        // Draw a bounding box for the leaf's face geometry.
        start = Vector2d(poly.bounds().minX, poly.bounds().minY);
        end   = Vector2d(poly.bounds().maxX, poly.bounds().maxY);

        LIBGUI_GL.glBegin(GL_LINES);
            LIBGUI_GL.glVertex2f(start.x, start.y);
            LIBGUI_GL.glVertex2f(  end.x, start.y);
            LIBGUI_GL.glVertex2f(  end.x, start.y);
            LIBGUI_GL.glVertex2f(  end.x,   end.y);
            LIBGUI_GL.glVertex2f(  end.x,   end.y);
            LIBGUI_GL.glVertex2f(start.x,   end.y);
            LIBGUI_GL.glVertex2f(start.x,   end.y);
            LIBGUI_GL.glVertex2f(start.x, start.y);
        LIBGUI_GL.glEnd();

    } while ((hedge = &hedge->next()) != base);
}

static int drawCellLines(Blockmap const &bmap, BlockmapCell const &cell, void *)
{
    LIBGUI_GL.glBegin(GL_LINES);
        bmap.forAllInCell(cell, [] (void *object)
        {
            Line &line = *(Line *)object;
            if (line.validCount() != validCount)
            {
                line.setValidCount(validCount);
                drawLine(line);
            }
            return LoopContinue;
        });
    LIBGUI_GL.glEnd();
    return false; // Continue iteration.
}

static int drawCellPolyobjs(Blockmap const &bmap, BlockmapCell const &cell, void *context)
{
    LIBGUI_GL.glBegin(GL_LINES);
        bmap.forAllInCell(cell, [&context] (void *object)
        {
            Polyobj &pob = *(Polyobj *)object;
            for (Line *line : pob.lines())
            {
                if (line->validCount() != validCount)
                {
                    line->setValidCount(validCount);
                    drawLine(*line);
                }
            }
            return LoopContinue;
        });
    LIBGUI_GL.glEnd();
    return false; // Continue iteration.
}

static int drawCellMobjs(Blockmap const &bmap, BlockmapCell const &cell, void *)
{
    LIBGUI_GL.glBegin(GL_QUADS);
        bmap.forAllInCell(cell, [] (void *object)
        {
            mobj_t &mob = *(mobj_t *)object;
            if (mob.validCount != validCount)
            {
                mob.validCount = validCount;
                drawMobj(mob);
            }
            return LoopContinue;
        });
    LIBGUI_GL.glEnd();
    return false; // Continue iteration.
}

static int drawCellSubspaces(Blockmap const &bmap, BlockmapCell const &cell, void *)
{
    bmap.forAllInCell(cell, [] (void *object)
    {
        ConvexSubspace *sub = (ConvexSubspace *)object;
        if (sub->validCount() != validCount)
        {
            sub->setValidCount(validCount);
            drawSubspace(*sub);
        }
        return LoopContinue;
    });
    return false; // Continue iteration.
}

static void drawBackground(Blockmap const &bmap)
{
    BlockmapCell const &dimensions = bmap.dimensions();

    // Scale modelview matrix so we can express cell geometry
    // using a cell-sized unit coordinate space.
    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPushMatrix();
    LIBGUI_GL.glScalef(bmap.cellSize(), bmap.cellSize(), 1);

    /*
     * Draw the translucent quad which represents the "used" cells.
     */
    LIBGUI_GL.glColor4f(.25f, .25f, .25f, .66f);
    LIBGUI_GL.glBegin(GL_QUADS);
        LIBGUI_GL.glVertex2f(0,            0);
        LIBGUI_GL.glVertex2f(dimensions.x, 0);
        LIBGUI_GL.glVertex2f(dimensions.x, dimensions.y);
        LIBGUI_GL.glVertex2f(0,            dimensions.y);
    LIBGUI_GL.glEnd();

    /*
     * Draw the "null cells" over the top.
     */
    LIBGUI_GL.glColor4f(0, 0, 0, .95f);
    BlockmapCell cell;
    for (cell.y = 0; cell.y < dimensions.y; ++cell.y)
    for (cell.x = 0; cell.x < dimensions.x; ++cell.x)
    {
        if (bmap.cellElementCount(cell)) continue;

        LIBGUI_GL.glBegin(GL_QUADS);
            LIBGUI_GL.glVertex2f(cell.x,     cell.y);
            LIBGUI_GL.glVertex2f(cell.x + 1, cell.y);
            LIBGUI_GL.glVertex2f(cell.x + 1, cell.y + 1);
            LIBGUI_GL.glVertex2f(cell.x,     cell.y + 1);
        LIBGUI_GL.glEnd();
    }

    // Restore previous GL state.
    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPopMatrix();
}

static void drawCellInfo(Vector2d const &origin_, char const *info)
{
    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Size2Raw size(16 + FR_TextWidth(info),
                  16 + FR_SingleLineHeight(info));

    Point2Raw origin(origin_.x, origin_.y);
    origin.x -= size.width / 2;
    UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += size.height / 2;
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(info, &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawBlockmapInfo(Vector2d const &origin_, Blockmap const &blockmap)
{
    glEnable(GL_TEXTURE_2D);

    Point2Raw origin(origin_.x, origin_.y);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Size2Raw size;
    size.width = 16 + FR_TextWidth("(+000.0, +000.0) (+000.0, +000.0)");
    int th = FR_SingleLineHeight("Info");
    size.height = th * 4 + 16;

    origin.x -= size.width;
    origin.y -= size.height;
    UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += 8 + th/2;

    UI_TextOutEx2("Blockmap", &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    Vector2ui const &bmapDimensions = blockmap.dimensions();
    char buf[80];
    dd_snprintf(buf, 80, "Dimensions:(%u, %u) #%li", bmapDimensions.x, bmapDimensions.y,
                                                    (long) bmapDimensions.y * bmapDimensions.x);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "Cell dimensions:(%.3f, %.3f)", blockmap.cellSize(), blockmap.cellSize());
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "(%+06.0f, %+06.0f) (%+06.0f, %+06.0f)",
                         blockmap.bounds().minX, blockmap.bounds().minY,
                         blockmap.bounds().maxX, blockmap.bounds().maxY);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawCellInfoBox(Vector2d const &origin, Blockmap const &blockmap,
    char const *objectTypeName, BlockmapCell const &cell)
{
    uint count = blockmap.cellElementCount(cell);
    char info[160]; dd_snprintf(info, 160, "Cell:(%u, %u) %s:#%u", cell.x, cell.y, objectTypeName, count);
    drawCellInfo(origin, info);
}

/**
 * @param bmap        Blockmap to be rendered.
 * @param followMobj  Mobj to center/focus the visual on. Can be @c 0.
 * @param cellDrawer  Blockmap cell content drawing callback. Can be @c 0.
 */
static void drawBlockmap(Blockmap const &bmap, mobj_t *followMobj,
    int (*cellDrawer) (Blockmap const &, BlockmapCell const &, void *))
{
    BlockmapCellBlock vCellBlock;
    BlockmapCell vCell;

    BlockmapCell const &dimensions = bmap.dimensions();
    Vector2d const cellDimensions = bmap.cellDimensions();

    if (followMobj)
    {
        // Determine the followed Mobj's blockmap coords.
        bool didClip;
        vCell = bmap.toCell(followMobj->origin, &didClip);

        if (didClip)
            followMobj = 0; // Outside the blockmap.

        if (followMobj)
        {
            // Determine the extended blockmap coords for the followed
            // Mobj's "touch" range.
            AABoxd mobBox = Mobj_Bounds(*followMobj);

            mobBox.minX -= DDMOBJ_RADIUS_MAX * 2;
            mobBox.minY -= DDMOBJ_RADIUS_MAX * 2;
            mobBox.maxX += DDMOBJ_RADIUS_MAX * 2;
            mobBox.maxY += DDMOBJ_RADIUS_MAX * 2;

            vCellBlock = bmap.toCellBlock(mobBox);
        }
    }

    if (followMobj)
    {
        // Orient on the center of the followed Mobj.
        LIBGUI_GL.glTranslated(-(vCell.x * cellDimensions.x), -(vCell.y * cellDimensions.y), 0);
    }
    else
    {
        // Orient on center of the Blockmap.
        LIBGUI_GL.glTranslated(-(cellDimensions.x * dimensions.x)/2,
                     -(cellDimensions.y * dimensions.y)/2, 0);
    }

    // First we'll draw a background showing the "null" cells.
    drawBackground(bmap);
    if (followMobj)
    {
        // Highlight cells the followed Mobj "touches".
        LIBGUI_GL.glBegin(GL_QUADS);

        BlockmapCell cell;
        for (cell.y = vCellBlock.min.y; cell.y < vCellBlock.max.y; ++cell.y)
        for (cell.x = vCellBlock.min.x; cell.x < vCellBlock.max.x; ++cell.x)
        {
            if (cell == vCell)
            {
                // The cell the followed Mobj is actually in.
                LIBGUI_GL.glColor4f(.66f, .66f, 1, .66f);
            }
            else
            {
                // A cell within the followed Mobj's extended collision range.
                LIBGUI_GL.glColor4f(.33f, .33f, .66f, .33f);
            }

            Vector2d const start = cellDimensions * cell;
            Vector2d const end   = start + cellDimensions;

            LIBGUI_GL.glVertex2d(start.x, start.y);
            LIBGUI_GL.glVertex2d(  end.x, start.y);
            LIBGUI_GL.glVertex2d(  end.x,   end.y);
            LIBGUI_GL.glVertex2d(start.x,   end.y);
        }

        LIBGUI_GL.glEnd();
    }

    /**
     * Draw the Gridmap visual.
     * @note Gridmap uses a cell unit size of [width:1,height:1], so we need to
     *       scale it up so it aligns correctly.
     */
    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPushMatrix();
    LIBGUI_GL.glScaled(cellDimensions.x, cellDimensions.y, 1);

    bmap.drawDebugVisual();

    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPopMatrix();

    /*
     * Draw the blockmap-linked data.
     * Translate the modelview matrix so that objects can be drawn using
     * the map space coordinates directly.
     */
    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPushMatrix();
    LIBGUI_GL.glTranslated(-bmap.origin().x, -bmap.origin().y, 0);

    if (cellDrawer)
    {
        if (followMobj)
        {
            /*
             * Draw cell contents color coded according to their range from the
             * followed Mobj.
             */

            // First, the cells outside the "touch" range (crimson).
            validCount++;
            LIBGUI_GL.glColor4f(.33f, 0, 0, .75f);
            BlockmapCell cell;
            for (cell.y = 0; cell.y < dimensions.y; ++cell.y)
            for (cell.x = 0; cell.x < dimensions.x; ++cell.x)
            {
                if (   cell.x >= vCellBlock.min.x && cell.x < vCellBlock.max.x
                    && cell.y >= vCellBlock.min.y && cell.y < vCellBlock.max.y)
                {
                    continue;
                }
                if (!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }

            // Next, the cells within the "touch" range (orange).
            validCount++;
            LIBGUI_GL.glColor3f(1, .5f, 0);
            for (cell.y = vCellBlock.min.y; cell.y < vCellBlock.max.y; ++cell.y)
            for (cell.x = vCellBlock.min.x; cell.x < vCellBlock.max.x; ++cell.x)
            {
                if (cell == vCell) continue;
                if (!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }

            // Lastly, the cell the followed Mobj is in (yellow).
            validCount++;
            LIBGUI_GL.glColor3f(1, 1, 0);
            if (bmap.cellElementCount(vCell))
            {
                cellDrawer(bmap, vCell, 0/*no params*/);
            }
        }
        else
        {
            // Draw all cells without color coding.
            validCount++;
            LIBGUI_GL.glColor4f(.33f, 0, 0, .75f);
            BlockmapCell cell;
            for (cell.y = 0; cell.y < dimensions.y; ++cell.y)
            for (cell.x = 0; cell.x < dimensions.x; ++cell.x)
            {
                if (!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }
        }
    }

    /*
     * Draw the followed mobj, if any.
     */
    if (followMobj)
    {
        validCount++;
        LIBGUI_GL.glColor3f(0, 1, 0);
        LIBGUI_GL.glBegin(GL_QUADS);
            drawMobj(*followMobj);
        LIBGUI_GL.glEnd();
    }

    // Undo the map coordinate space translation.
    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPopMatrix();
}

void Rend_BlockmapDebug()
{
    // Disabled?
    if (!bmapShowDebug || bmapShowDebug > 4) return;

    if (!App_World().hasMap()) return;
    Map &map = App_World().map();

    Blockmap const *blockmap;
    int (*cellDrawer) (Blockmap const &, BlockmapCell const &, void *);
    char const *objectTypeName;
    switch (bmapShowDebug)
    {
    default: // Mobjs.
        blockmap = &map.mobjBlockmap();
        cellDrawer = drawCellMobjs;
        objectTypeName = "Mobjs";
        break;

    case 2: // Lines.
        blockmap = &map.lineBlockmap();
        cellDrawer = drawCellLines;
        objectTypeName = "Lines";
        break;

    case 3: // BSP leafs.
        blockmap = &map.subspaceBlockmap();
        cellDrawer = drawCellSubspaces;
        objectTypeName = "Subspaces";
        break;

    case 4: // Polyobjs.
        blockmap = &map.polyobjBlockmap();
        cellDrawer = drawCellPolyobjs;
        objectTypeName = "Polyobjs";
        break;
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    /*
     * Draw the blockmap.
     */
    LIBGUI_GL.glMatrixMode(GL_PROJECTION);
    LIBGUI_GL.glPushMatrix();
    LIBGUI_GL.glLoadIdentity();
    LIBGUI_GL.glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);
    // Orient on the center of the window.
    LIBGUI_GL.glTranslatef((DENG_GAMEVIEW_WIDTH / 2), (DENG_GAMEVIEW_HEIGHT / 2), 0);

    // Uniform scaling factor for this visual.
    float scale = bmapDebugSize / de::max(DENG_GAMEVIEW_HEIGHT / 100, 1);
    LIBGUI_GL.glScalef(scale, -scale, 1);

    // If possible we'll tailor what we draw relative to the viewPlayer.
    mobj_t *followMobj = 0;
    if (viewPlayer && viewPlayer->publicData().mo)
    {
        followMobj = viewPlayer->publicData().mo;
    }

    // Draw!
    drawBlockmap(*blockmap, followMobj, cellDrawer);

    LIBGUI_GL.glMatrixMode(GL_PROJECTION);
    LIBGUI_GL.glPopMatrix();

    /*
     * Draw HUD info.
     */
    LIBGUI_GL.glMatrixMode(GL_PROJECTION);
    LIBGUI_GL.glPushMatrix();
    LIBGUI_GL.glLoadIdentity();
    LIBGUI_GL.glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    if (followMobj)
    {
        // About the cell the followed Mobj is in.
        bool didClip;
        BlockmapCell cell = blockmap->toCell(followMobj->origin, &didClip);
        if (!didClip)
        {
            drawCellInfoBox(Vector2d(DENG_GAMEVIEW_WIDTH / 2, 30), *blockmap,
                            objectTypeName, cell);
        }
    }

    // About the Blockmap itself.
    drawBlockmapInfo(Vector2d(DENG_GAMEVIEW_WIDTH - 10, DENG_GAMEVIEW_HEIGHT - 10),
                     *blockmap);

    LIBGUI_GL.glMatrixMode(GL_PROJECTION);
    LIBGUI_GL.glPopMatrix();
}
