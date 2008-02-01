/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * bsp_main.c: GL-friendly BSP node builder.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_bsp.h"
#include "de_misc.h"

#include "p_mapdata.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int bspFactor = 7;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Register the ccmds and cvars of the BSP builder. Called during engine
 * startup
 */
void BSP_Register(void)
{
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
}

/**
 * Initially create all half-edges, one for each side of a linedef.
 *
 * \pre Blockmap must be initialized before this is called!
 *
 * @return              The list of created half-edges.
 */
static superblock_t *createInitialHEdges(editmap_t *src)
{
    uint            startTime = Sys_GetRealTime();

    uint            i;
    int             bw, bh;
    hedge_t        *back, *front;
    superblock_t   *block;

    block = BSP_SuperBlockCreate();

    BSP_GetBMapBounds(&block->bbox[BOXLEFT], &block->bbox[BOXBOTTOM],
                      &bw, &bh);

    block->bbox[BOXRIGHT] = block->bbox[BOXLEFT]   + 128 * M_CeilPow2(bw);
    block->bbox[BOXTOP]   = block->bbox[BOXBOTTOM] + 128 * M_CeilPow2(bh);

    // Step through linedefs and get side numbers.
    for(i = 0; i < src->numLines; ++i)
    {
        line_t         *line = src->lines[i];

        front = back = NULL;

        // Ignore zero-length, overlapping and polyobj lines.
        if(!(line->buildData.mlFlags & MLF_ZEROLENGTH) && !line->buildData.overlap &&
           !(line->buildData.mlFlags & MLF_POLYOBJ))
        {
            // Check for Humungously long lines.
            if(ABS(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX]) >= 10000 ||
               ABS(line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]) >= 10000)
            {
                if(3000 >=
                   M_Length(line->v[0]->buildData.pos[VX] - line->v[1]->buildData.pos[VX],
                            line->v[0]->buildData.pos[VY] - line->v[1]->buildData.pos[VY]))
                {
                    Con_Message("Linedef #%d is VERY long, it may cause problems\n",
                                line->buildData.index);
                }
            }

            if(line->sides[FRONT])
            {
                side_t     *side = line->sides[FRONT];

                // Check for a bad sidedef.
                if(!side->sector)
                    Con_Message("Bad sidedef on linedef #%d (Z_CheckHeap error)\n",
                                line->buildData.index);

                front = HEdge_Create(line, line, line->v[0], line->v[1],
                                        side->sector, false);
                BSP_AddHEdgeToSuperBlock(block, front);
            }
            else
                Con_Message("Linedef #%d has no front sidedef!\n",
                            line->buildData.index);

            if(line->sides[BACK])
            {
                side_t     *side = line->sides[BACK];

                // Check for a bad sidedef.
                if(!side->sector)
                    Con_Message("Bad sidedef on linedef #%d (Z_CheckHeap error)\n",
                                line->buildData.index);

                back = HEdge_Create(line, line, line->v[1], line->v[0],
                                       side->sector, true);
                BSP_AddHEdgeToSuperBlock(block, back);

                if(front)
                {
                    // Half-edges always maintain a one-to-one relationship
                    // with their twins, so if one gets split, the other
                    // must be split also.
                    back->twin = front;
                    front->twin = back;
                }
            }
            else
            {
                if(line->buildData.mlFlags & MLF_TWOSIDED)
                {
                    Con_Message("Linedef #%d is 2s but has no back sidedef\n",
                                line->buildData.index);
                    line->buildData.mlFlags &= ~MLF_TWOSIDED;
                }

                // Handle the 'One-Sided Window' trick.
                if(line->buildData.windowEffect)
                {
                    hedge_t    *other;

                    other = HEdge_Create(front->lineDef, line,
                                            line->v[1], line->v[0],
                                            line->buildData.windowEffect, true);

                    BSP_AddHEdgeToSuperBlock(block, other);

                    // Setup the twin-ing (it's very strange to have a mini
                    // and a normal partnered together).
                    other->twin = front;
                    front->twin = other;
                }
            }
        }

        // \todo edge tips should be created when half-edges are created.
        {
        double x1 = line->v[0]->buildData.pos[VX];
        double y1 = line->v[0]->buildData.pos[VY];
        double x2 = line->v[1]->buildData.pos[VX];
        double y2 = line->v[1]->buildData.pos[VY];

        BSP_CreateVertexEdgeTip(line->v[0], x2 - x1, y2 - y1, back, front);
        BSP_CreateVertexEdgeTip(line->v[1], x1 - x2, y1 - y2, front, back);
        }
    }

/*
#if _DEBUG
for(i = 0; i < numVertices; ++i)
{
    vertex_t *vert = LookupVertex(i);
    edgetip_t *tip;

    Con_Message("EdgeTips for vertex %d:\n", i);

    for(tip = vert->tipSet; tip; tip = tip->next)
    {
        Con_Message("  Angle=%1.1f left=%d right=%d\n", tip->angle,
                    (tip->left? tip->left->index : -1),
                    (tip->right? tip->right->index : -1));
    }
}
#endif
*/

    // How much time did we spend?
    VERBOSE(Con_Message
            ("createInitialHEdges: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    return block;
}

/**
 * Build the BSP for the given map.
 *
 * @param map           The map to build the BSP for.
 * @return              @c true, if completed successfully.
 */
boolean BSP_Build(gamemap_t *dest, editmap_t *src)
{
    boolean             builtOK;
    uint                startTime;
    superblock_t       *hEdgeList;

    if(verbose >= 1)
    {
        Con_Message("BSP_Build: Processing map using tunable "
                    "factor of %d...\n", bspFactor);
    }

    // It begins...
    startTime = Sys_GetRealTime();

    BSP_InitSuperBlockAllocator();
    BSP_InitIntersectionAllocator();

    BSP_InitForNodeBuild(src);
    BSP_InitAnalyzer(src);

    BSP_DetectOverlappingLines(src);
    BSP_DetectWindowEffects(src);

    // Create initial half-edges.
    hEdgeList = createInitialHEdges(src);

    // Build the BSP.
    {
    uint                buildStartTime = Sys_GetRealTime();
    cutlist_t          *cutList;

    cutList = BSP_CutListCreate();

    // Recursively create nodes.
    src->rootNode = NULL;
    builtOK = BuildNodes(hEdgeList, &src->rootNode, 0, cutList);

    // The cutlist data is no longer needed.
    BSP_CutListDestroy(cutList);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("BuildNodes: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - buildStartTime) / 1000.0f));
    }

    BSP_SuperBlockDestroy(hEdgeList);

    if(builtOK)
    {   // Success!
        // Wind the BSP tree and link to the map.
        ClockwiseBspTree(src->rootNode);
        SaveMap(dest, src);

        Con_Message("BSP_Build: Built %d Nodes, %d Subsectors, %d Segs, %d Vertexes\n",
                    dest->numNodes, dest->numSubsectors, dest->numSegs,
                    dest->numVertexes);

        if(src->rootNode && !BinaryTree_IsLeaf(src->rootNode))
        {
            long            rHeight, lHeight;

            rHeight = (long)
                BinaryTree_GetHeight(BinaryTree_GetChild(src->rootNode, RIGHT));
            lHeight = (long)
                BinaryTree_GetHeight(BinaryTree_GetChild(src->rootNode, LEFT));

            Con_Message("  Balance %+ld (l%ld - r%ld).\n", lHeight - rHeight,
                        lHeight, rHeight);
        }
    }

    // Free temporary storage.
    FreeMap();
    BSP_ShutdownSuperBlockAllocator();
    BSP_ShutdownIntersectionAllocator();

    // How much time did we spend?
    VERBOSE(Con_Message("  Done in %.2f seconds.\n",
                        (Sys_GetRealTime() - startTime) / 1000.0f));

    return builtOK;
}
