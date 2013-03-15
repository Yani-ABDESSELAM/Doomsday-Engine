/** @file hedge.cpp Map Geometry Half-Edge.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
//#include "de_console.h"
//#include "de_play.h"
#include "map/linedef.h"
#include "map/sidedef.h"
#include "map/vertex.h"
#include "map/r_world.h"
#include <de/Log>
#include <QtAlgorithms>

#include "map/hedge.h"

using namespace de;

HEdge::HEdge() : MapElement(DMU_HEDGE)
{
    std::memset(v, 0, sizeof(v));
    next = 0;
    prev = 0;
    twin = 0;
    bspLeaf = 0;
    line = 0;
    sector = 0;
    angle = 0;
    side = 0;
    length = 0;
    offset = 0;
    std::memset(bsuf, 0, sizeof(bsuf));
    frameFlags = 0;
    index = 0;
}

HEdge::HEdge(HEdge const &other)
    : MapElement(DMU_HEDGE)
{
    v[0] = other.v[0];
    v[1] = other.v[1];
    next = other.next;
    prev = other.prev;
    twin = other.twin;
    bspLeaf = other.bspLeaf;
    line = other.line;
    sector = other.sector;
    angle = other.angle;
    side = other.side;
    length = other.length;
    offset = other.offset;
    std::memcpy(bsuf, other.bsuf, sizeof(bsuf));
    frameFlags = other.frameFlags;
    index = other.index;
}

HEdge::~HEdge()
{
#ifdef __CLIENT__
    for(uint i = 0; i < 3; ++i)
    {
        if(bsuf[i])
        {
            SB_DestroySurface(bsuf[i]);
        }
    }
#endif
}

static walldivnode_t *findWallDivNodeByZOrigin(walldivs_t *wallDivs, coord_t height)
{
    DENG2_ASSERT(wallDivs);
    for(uint i = 0; i < wallDivs->num; ++i)
    {
        walldivnode_t *node = &wallDivs->nodes[i];
        if(node->height == height)
            return node;
    }
    return NULL;
}

static void addWallDivNodesForPlaneIntercepts(HEdge const *hedge, walldivs_t *wallDivs,
    SideDefSection section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    bool const isTwoSided = (hedge->line && hedge->line->hasFrontSideDef() && hedge->line->hasBackSideDef())? true:false;
    bool const clockwise = !doRight;
    LineDef const *line = hedge->line;
    Sector const *frontSec = line->sectorPtr(hedge->side);

    // Check for neighborhood division?
    if(section == SS_MIDDLE && isTwoSided) return;
    if(!hedge->line) return;
    if(hedge->line->isFromPolyobj()) return;

    // Polyobj edges are never split.
    if(hedge->line && (hedge->line->isFromPolyobj())) return;

    // Only edges at sidedef ends can/should be split.
    if(!((hedge == &HEDGE_SIDE(hedge)->leftHEdge()  && !doRight) ||
         (hedge == &HEDGE_SIDE(hedge)->rightHEdge() &&  doRight)))
        return;

    if(bottomZ >= topZ) return; // Obviously no division.

    // Retrieve the start owner node.
    LineOwner *base = R_GetVtxLineOwner(&line->vertex(hedge->side^doRight), line);
    LineOwner *own = base;
    bool stopScan = false;
    do
    {
        own = own->_link[clockwise];

        if(own == base)
        {
            stopScan = true;
        }
        else
        {
            LineDef *iter = &own->line();

            if(iter->isSelfReferencing())
                continue;

            uint i = 0;
            do
            {   // First front, then back.
                Sector *scanSec = NULL;
                if(!i && iter->hasFrontSideDef() && iter->frontSectorPtr() != frontSec)
                    scanSec = iter->frontSectorPtr();
                else if(i && iter->hasBackSideDef() && iter->backSectorPtr() != frontSec)
                    scanSec = iter->backSectorPtr();

                if(scanSec)
                {
                    if(scanSec->ceiling().visHeight() - scanSec->floor().visHeight() > 0)
                    {
                        for(uint j = 0; j < scanSec->planeCount() && !stopScan; ++j)
                        {
                            Plane const &plane = scanSec->plane(j);

                            if(plane.visHeight() > bottomZ && plane.visHeight() < topZ)
                            {
                                if(!findWallDivNodeByZOrigin(wallDivs, plane.visHeight()))
                                {
                                    WallDivs_Append(wallDivs, plane.visHeight());

                                    // Have we reached the div limit?
                                    if(wallDivs->num == WALLDIVS_MAX_NODES)
                                        stopScan = true;
                                }
                            }

                            if(!stopScan)
                            {
                                // Clip a range bound to this height?
                                if(plane.type() == Plane::Floor && plane.visHeight() > bottomZ)
                                    bottomZ = plane.visHeight();
                                else if(plane.type() == Plane::Ceiling && plane.visHeight() < topZ)
                                    topZ = plane.visHeight();

                                // All clipped away?
                                if(bottomZ >= topZ)
                                    stopScan = true;
                            }
                        }
                    }
                    else
                    {
                        /**
                         * A zero height sector is a special case. In this
                         * instance, the potential division is at the height
                         * of the back ceiling. This is because elsewhere
                         * we automatically fix the case of a floor above a
                         * ceiling by lowering the floor.
                         */
                        coord_t z = scanSec->ceiling().visHeight();

                        if(z > bottomZ && z < topZ)
                        {
                            if(!findWallDivNodeByZOrigin(wallDivs, z))
                            {
                                WallDivs_Append(wallDivs, z);

                                // All clipped away.
                                stopScan = true;
                            }
                        }
                    }
                }
            } while(!stopScan && ++i < 2);

            // Stop the scan when a single sided line is reached.
            if(!iter->hasFrontSideDef() || !iter->hasBackSideDef())
                stopScan = true;
        }
    } while(!stopScan);
}

static int sortWallDivNode(void const *e1, void const *e2)
{
    coord_t const h1 = ((walldivnode_t *)e1)->height;
    coord_t const h2 = ((walldivnode_t *)e2)->height;
    if(h1 > h2) return  1;
    if(h2 > h1) return -1;
    return 0;
}

static void buildWallDiv(walldivs_t *wallDivs, HEdge const *hedge,
   SideDefSection section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    wallDivs->num = 0;

    // Nodes are arranged according to their Z axis height in ascending order.
    // The first node is the bottom.
    WallDivs_Append(wallDivs, bottomZ);

    // Add nodes for intercepts.
    addWallDivNodesForPlaneIntercepts(hedge, wallDivs, section, bottomZ, topZ, doRight);

    // The last node is the top.
    WallDivs_Append(wallDivs, topZ);

    if(!(wallDivs->num > 2)) return;

    // Sorting is required. This shouldn't take too long...
    // There seldom are more than two or three nodes.
    qsort(wallDivs->nodes, wallDivs->num, sizeof(*wallDivs->nodes), sortWallDivNode);

    WallDivs_AssertSorted(wallDivs);
    WallDivs_AssertInRange(wallDivs, bottomZ, topZ);
}

bool HEdge::prepareWallDivs(SideDefSection section, Sector *frontSec, Sector *backSec,
    walldivs_t *leftWallDivs, walldivs_t *rightWallDivs, pvec2f_t matOffset) const
{
    int lineFlags = line? line->flags() : 0;
    SideDef *frontDef = HEDGE_SIDEDEF(this);
    SideDef *backDef  = twin? HEDGE_SIDEDEF(twin) : 0;
    coord_t low, hi;
    boolean visible = R_FindBottomTop2(section, lineFlags, frontSec, backSec, frontDef, backDef,
                                       &low, &hi, matOffset);
    matOffset[0] += float(offset);
    if(!visible) return false;

    buildWallDiv(leftWallDivs,  this, section, low, hi, false/*is-left-edge*/);
    buildWallDiv(rightWallDivs, this, section, low, hi, true/*is-right-edge*/);

    return true;
}

coord_t HEdge::pointDistance(const_pvec2d_t point, coord_t *offset) const
{
    coord_t direction[2]; V2d_Subtract(direction, v[1]->origin(), v[0]->origin());
    return V2d_PointLineDistance(point, v[0]->origin(), direction, offset);
}

coord_t HEdge::pointOnSide(const_pvec2d_t point) const
{
    DENG2_ASSERT(point);
    coord_t direction[2]; V2d_Subtract(direction, v[1]->origin(), v[0]->origin());
    return V2d_PointOnLineSide(point, v[0]->origin(), direction);
}

int HEdge::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_HEDGE_V, &v[0], &args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_HEDGE_V, &v[1], &args, 0);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_HEDGE_LENGTH, &length, &args, 0);
        break;
    case DMU_OFFSET:
        DMU_GetValue(DMT_HEDGE_OFFSET, &offset, &args, 0);
        break;
    case DMU_SIDEDEF: {
        SideDef *sideDef = HEDGE_SIDEDEF(this);
        DMU_GetValue(DMT_HEDGE_SIDEDEF, &sideDef, &args, 0);
        break; }
    case DMU_LINEDEF:
        DMU_GetValue(DMT_HEDGE_LINEDEF, &line, &args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        DMU_GetValue(DMT_HEDGE_SECTOR, &sector, &args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector *sec = HEDGE_BACK_SECTOR(this);
        DMU_GetValue(DMT_HEDGE_SECTOR, &sec, &args, 0);
        break; }
    case DMU_ANGLE:
        DMU_GetValue(DMT_HEDGE_ANGLE, &angle, &args, 0);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("HEdge::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

int HEdge::setProperty(setargs_t const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError("HEdge::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
}

// WallDivs ----------------------------------------------------------------
/// @todo Move the following to another file

coord_t WallDivNode_Height(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    return node->height;
}

walldivnode_t *WallDivNode_Next(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx + 1 >= node->divs->num) return 0;
    return &node->divs->nodes[idx+1];
}

walldivnode_t *WallDivNode_Prev(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx == 0) return 0;
    return &node->divs->nodes[idx-1];
}

uint WallDivs_Size(walldivs_t const *wd)
{
    DENG2_ASSERT(wd);
    return wd->num;
}

walldivnode_t *WallDivs_First(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    return &wd->nodes[0];
}

walldivnode_t *WallDivs_Last(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    return &wd->nodes[wd->num-1];
}

walldivs_t *WallDivs_Append(walldivs_t *wd, coord_t height)
{
    DENG2_ASSERT(wd);
    struct walldivnode_s *node = &wd->nodes[wd->num++];
    node->divs = wd;
    node->height = height;
    return wd;
}

void WallDivs_AssertSorted(walldivs_t *wd)
{
#ifdef DENG_DEBUG
    walldivnode_t *node = WallDivs_First(wd);
    coord_t highest = WallDivNode_Height(node);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        DENG2_ASSERT(node->height >= highest);
        highest = node->height;
    }
#else
    DENG_UNUSED(wd);
#endif
}

void WallDivs_AssertInRange(walldivs_t *wd, coord_t low, coord_t hi)
{
#ifdef DENG_DEBUG
    DENG2_ASSERT(wd);
    walldivnode_t *node = WallDivs_First(wd);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        DENG2_ASSERT(node->height >= low && node->height <= hi);
    }
#else
    DENG2_UNUSED3(wd, ow, hi);
#endif
}

#ifdef DENG_DEBUG
void WallDivs_DebugPrint(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    LOG_DEBUG("WallDivs [%p]:") << wd;
    for(uint i = 0; i < wd->num; ++i)
    {
        walldivnode_t *node = &wd->nodes[i];
        LOG_DEBUG("  %i: %f") << i << node->height;
    }
}
#endif
