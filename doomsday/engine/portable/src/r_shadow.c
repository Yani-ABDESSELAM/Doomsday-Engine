/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

/*
 * r_shadow.c: Runtime Map Shadowing (FakeRadio)
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "p_bmap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct boundary_s {
    vec2_t  left, right;
    vec2_t  a, b;
} boundary_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static zblockset_t *shadowLinksBlockSet;

static boundary_t *boundaries = NULL;
static byte       *overlaps = NULL;
static uint        boundaryNum;

// CODE --------------------------------------------------------------------

/**
 * Line1 and line2 are the (dx,dy)s for two lines, connected at the
 * origin (0,0).  Dist1 and dist2 are the distances from these lines.
 * The returned point (in 'point') is dist1 away from line1 and dist2
 * from line2, while also being the nearest point to the origin (in
 * case the lines are parallel).
 */
void R_CornerNormalPoint(const pvec2_t line1, float dist1, const pvec2_t line2,
                         float dist2, pvec2_t point, pvec2_t lp, pvec2_t rp)
{
    float   len1, len2, plen;
    vec2_t  origin = { 0, 0 }, norm1, norm2;

    // Length of both lines.
    len1 = V2_Length(line1);
    len2 = V2_Length(line2);

    // Calculate normals for both lines.
    V2_Set(norm1, -line1[VY] / len1 * dist1, line1[VX] / len1 * dist1);
    V2_Set(norm2, line2[VY] / len2 * dist2, -line2[VX] / len2 * dist2);

    // Are the lines parallel?  If so, they won't connect at any
    // point, and it will be impossible to determine a corner point.
    if(V2_IsParallel(line1, line2))
    {
        // Just use a normal as the point.
        if(point)
            V2_Copy(point, norm1);
        if(lp)
            V2_Copy(lp, norm1);
        if(rp)
            V2_Copy(rp, norm1);
        return;
    }

    // Find the intersection of normal-shifted lines.  That'll be our
    // corner point.
    if(point)
        V2_Intersection(norm1, line1, norm2, line2, point);

    // Do we need to calculate the extended points, too?  Check that
    // the extensions don't bleed too badly outside the legal shadow
    // area.
    if(lp)
    {
        V2_Intersection(origin, line1, norm2, line2, lp);
        plen = V2_Length(lp);
        if(plen > 0 && plen > len1)
        {
            V2_Scale(lp, len1 / plen);
        }
    }
    if(rp)
    {
        V2_Intersection(norm1, line1, origin, line2, rp);
        plen = V2_Length(rp);
        if(plen > 0 && plen > len2)
        {
            V2_Scale(rp, len2 / plen);
        }
    }
}

void R_ShadowDelta(pvec2_t delta, line_t *line, sector_t *frontSector)
{
    if(line->L_frontsector == frontSector)
    {
        delta[VX] = line->dx;
        delta[VY] = line->dy;
    }
    else
    {
        delta[VX] = -line->dx;
        delta[VY] = -line->dy;
    }
}

/**
 * Returns a pointer to the sector the shadow polygon belongs in.
 */
sector_t *R_GetShadowSector(shadowpoly_t *poly, uint pln, boolean getLinked)
{
    if(getLinked)
        return R_GetLinkedSector(poly->ssec, pln);
    return (poly->seg->SG_frontsector);
}

boolean R_ShadowCornerDeltas(pvec2_t left, pvec2_t right, shadowpoly_t *poly,
                             boolean leftCorner)
{
    sector_t *sector = R_GetShadowSector(poly, 0, false);
    line_t *neighbor;
    int     side = !(poly->flags & SHPF_FRONTSIDE);

    // The line itself.
    R_ShadowDelta(leftCorner ? right : left, poly->seg->linedef, sector);

    // The neighbor.
    neighbor = R_FindLineNeighbor(poly->ssec->sector, poly->seg->linedef,
                                  poly->seg->linedef->vo[side^!leftCorner],
                                  !leftCorner, NULL);
    if(!neighbor)
    {
        // Should never happen...
        Con_Message("R_ShadowCornerDeltas: No %s neighbor for line %u!\n",
                    leftCorner? "left":"right",
                    (uint) GET_LINE_IDX(poly->seg->linedef));
        return false;
    }

    R_ShadowDelta(leftCorner ? left : right, neighbor, sector);

    // The left side is always flipped.
    V2_Scale(left, -1);
    return true;
}

/**
 * @return          The width (world units) of the shadow edge.
 *                  It is scaled depending on the length of the edge.
 */
float R_ShadowEdgeWidth(const pvec2_t edge)
{
    float       length = V2_Length(edge);
    float       normalWidth = 20;   //16;
    float       maxWidth = 60;
    float       w;

    // A long edge?
    if(length > 600)
    {
        w = length - 600;
        if(w > 1000)
            w = 1000;
        return normalWidth + w / 1000 * maxWidth;
    }

    return normalWidth;
}

/**
 * Sets the shadow edge offsets. If the associated line does not have
 * neighbors, it can't have a shadow.
 */
void R_ShadowEdges(shadowpoly_t *poly)
{
    vec2_t      left, right;
    int         edge, side = !(poly->flags & SHPF_FRONTSIDE);
    uint        i, count;
    line_t     *line = poly->seg->linedef;
    lineowner_t *base, *p, *boundryOwner = NULL;
    boolean     done;

    for(edge = 0; edge < 2; ++edge) // left and right
    {
        // The inside corner:
        if(R_ShadowCornerDeltas(left, right, poly, edge == 0))
        {
            R_CornerNormalPoint(left, R_ShadowEdgeWidth(left), right,
                                R_ShadowEdgeWidth(right), poly->inoffset[edge],
                                edge == 0 ? poly->extoffset[edge] : NULL,
                                edge == 1 ? poly->extoffset[edge] : NULL);
        }
        else
        {   // An error in the map. Set the inside corner to the extoffset.
            V2_Copy(poly->inoffset[edge], poly->extoffset[edge]);
        }

        // The back extended offset(s):
        // Determine how many we'll need.
        base = R_GetVtxLineOwner(line->L_v(edge^side), line);
        count = 0;
        done = false;
        p = base->link[!edge];
        while(p != base && !done)
        {
            if(!(p->line->L_frontside && p->line->L_backside &&
                 p->line->L_frontsector == p->line->L_backsector))
            {
                if(count == 0)
                {   // Found the boundry line.
                    boundryOwner = p;
                }

                if(!p->line->L_frontside || !p->line->L_backside)
                    done = true; // Found the last one.
                else
                    count++;
            }

            if(!done)
                p = p->link[!edge];
        }

        // It is not always possible to calculate the back-extended
        // offset.
        if(count == 0)
        {
            // No back-extended, just use the plain extended offset.
            V2_Copy(poly->bextoffset[edge][0].offset,
                    poly->extoffset[edge]);
        }
        else
        {
            // We need at least one back extended offset.
            sector_t   *sector = R_GetShadowSector(poly, 0, false);
            line_t     *neighbor;
            boolean     leftCorner = (edge == 0);
            pvec2_t     delta;
            sector_t   *orientSec;

            // The line itself.
            R_ShadowDelta(leftCorner ? right : left, line, sector);

            // The left side is always flipped.
            if(!leftCorner)
                V2_Scale(left, -1);

            if(boundryOwner->line->L_frontsector == sector)
                orientSec = (boundryOwner->line->L_backside? boundryOwner->line->L_backsector : NULL);
            else
                orientSec = (boundryOwner->line->L_frontside? boundryOwner->line->L_frontsector : NULL);

            p = boundryOwner;
            for(i = 0; i < count && i < MAX_BEXOFFSETS; ++i)
            {
                byte        otherside;
                // Get the next back neighbor.
                neighbor = NULL;
                p = p->link[!edge];
                do
                {
                    if(!(p->line->L_frontside && p->line->L_backside &&
                         p->line->L_frontsector == p->line->L_backsector))
                        neighbor = p->line;
                    else
                        p = p->link[!edge];
                } while(!neighbor);

                // The back neighbor delta.
                delta = (leftCorner ? left : right);
                otherside = neighbor->L_v1 == line->L_v(edge^!side);
                if(neighbor->L_side(otherside) &&
                   orientSec == neighbor->L_sector(otherside))
                {
                    delta[VX] = neighbor->dx;
                    delta[VY] = neighbor->dy;
                }
                else
                {
                    delta[VX] = -neighbor->dx;
                    delta[VY] = -neighbor->dy;
                }

                // The left side is always flipped.
                if(leftCorner)
                    V2_Scale(left, -1);

                R_CornerNormalPoint(left, R_ShadowEdgeWidth(left),
                                    right, R_ShadowEdgeWidth(right),
                                    poly->bextoffset[edge][i].offset,
                                    NULL, NULL);

                // Update orientSec ready for the next iteration?
                if(i < count - 1)
                {
                    if(neighbor->L_frontsector == orientSec)
                        orientSec = (neighbor->L_backside? neighbor->L_backsector : NULL);
                    else
                        orientSec = (neighbor->L_frontside? neighbor->L_frontsector : NULL);
                }
            }
        }
    }
}

/**
 * Link a shadowpoly to a subsector.
 */
void R_LinkShadow(shadowpoly_t *poly, subsector_t *subsector)
{
    shadowlink_t *link;

#ifdef _DEBUG
// Check the links for dupes!
{
shadowlink_t *i;

for(i = subsector->shadows; i; i = i->next)
    if(i->poly == poly)
        Con_Error("R_LinkShadow: Already here!!\n");
}
#endif

    // We'll need to allocate a new link.
    link = Z_BlockNewElement(shadowLinksBlockSet);

    // The links are stored into a linked list.
    link->next = subsector->shadows;
    subsector->shadows = link;
    link->poly = poly;
}

/**
 * If the shadow polygon (parm) contacts the subsector, link the poly
 * to the subsector's shadow list.
 */
boolean RIT_ShadowSubsectorLinker(subsector_t *subsector, void *parm)
{
    shadowpoly_t *poly = parm;
#if 0
    vec2_t  corners[4], a, b, mid;
    int     i, j;
#endif

    R_LinkShadow(poly, subsector);
    return true; // DJS - is this meant to be here?

#if 0 // currently unused
    // Use the extended points, they are wider than inoffsets.
    V2_Set(corners[0], FIX2FLT(poly->outer[0]->pos[VX]), FIX2FLT(poly->outer[0]->pos[VY]));
    V2_Set(corners[1], FIX2FLT(poly->outer[1]->pos[VX]), FIX2FLT(poly->outer[1]->pos[VY]));
    V2_Sum(corners[2], corners[1], poly->extoffset[1]);
    V2_Sum(corners[3], corners[0], poly->extoffset[0]);

    V2_Sum(mid, corners[0], corners[2]);
    V2_Scale(mid, .5f);

    for(i = 0; i < 4; ++i)
    {
        V2_Subtract(corners[i], corners[i], mid);
        V2_Scale(corners[i], .995f);
        V2_Sum(corners[i], corners[i], mid);
    }

    // Any of the corner points in the subsector?
    for(i = 0; i < 4; ++i)
    {
        if(R_PointInSubsector
           (FRACUNIT * corners[i][VX], FRACUNIT * corners[i][VY]) == subsector)
        {
            // Good enough.
            R_LinkShadow(poly, subsector);
            return true;
        }
    }

    // Do a more elaborate line intersection test.  It's possible that
    // the shadow's corners are outside a subsector, but the shadow
    // still contacts the subsector.

    for(j = 0; j < subsector->numverts; ++j)
    {
        V2_Set(a, subsector->verts[j].x, subsector->verts[j].y);
        V2_Set(b, subsector->verts[(j + 1) % subsector->numverts].x,
               subsector->verts[(j + 1) % subsector->numverts].y);

        for(i = 0; i < 4; ++i)
        {
            /*          k = V2_Intercept(a, b, corners[i], corners[(i + 1) % 4], NULL);
               m = V2_Intercept(corners[i], corners[(i + 1) % 4], a, b, NULL);

               // Is the intersection between points 'a' and 'b'.
               if(k >= 0 && k <= 1 && m >= 0 && m <= 1) */

            if(V2_Intercept2
               (a, b, corners[i], corners[(i + 1) % 4], NULL, NULL, NULL))
            {
                // There is contact!
                R_LinkShadow(poly, subsector);
                return true;
            }
        }
    }

    // Continue with the iteration, maybe some other subsectors will
    // contact it.
    return true;
#endif
}

/**
 * Moves inoffset appropriately.
 *
 * @return          <code>true</code> if overlap resolving should continue
 *                  to another round of iteration.
 */
boolean R_ResolveStep(const pvec2_t outer, const pvec2_t inner,
                      pvec2_t offset)
{
#define RESOLVE_STEP 2

    float       span, distance;
    boolean     iterCont = true;

    span = distance = V2_Distance(outer, inner);
    if(span == 0)
        return false;

    distance /= 2;
    iterCont = false;

    V2_Scale(offset, distance / span);
    return iterCont;

#undef RESOLVE_STEP
}

/**
 * The array of polys given as the parameter contains the shadow polygons
 * of one sector. If the polygons overlap, we will iteratively resolve the
 * overlaps by moving the inner corner points closer to the outer corner
 * points. Other corner points remain unmodified.
 */
void R_ResolveOverlaps(shadowpoly_t *polys, uint count, sector_t *sector)
{
#define OVERLAP_LEFT    0x01
#define OVERLAP_RIGHT   0x02
#define OVERLAP_ALL     (OVERLAP_LEFT | OVERLAP_RIGHT)
#define EPSILON         .01f

    boolean     done;
    int         tries;
    uint        i, k;
    boundary_t *bound;
    float       s, t;
    vec2_t      a, b;
    line_t     *line;

    // Were any polygons provided?
    if(!count)
        return;

    // Need to enlarge the boundry+overlap buffers?
    if(count > boundaryNum)
    {
        boundaries = M_Realloc(boundaries, sizeof(*boundaries) * count);
        overlaps = M_Realloc(overlaps, count);
        boundaryNum = count;
    }

    // We don't want to stay here forever.
    done = false;
    for(tries = 0; tries < 100 && !done; ++tries)
    {
        // We will set this to false if we notice that overlaps still
        // exist.
        done = true;

        // Calculate the boundaries.
        for(i = 0, bound = boundaries; i < count; ++i, bound++)
        {
            V2_Set(bound->left, polys[i].outer[0]->V_pos[VX],
                   polys[i].outer[0]->V_pos[VY]);
            V2_Sum(bound->a, polys[i].inoffset[0], bound->left);

            V2_Set(bound->right, polys[i].outer[1]->V_pos[VX],
                   polys[i].outer[1]->V_pos[VY]);
            V2_Sum(bound->b, polys[i].inoffset[1], bound->right);
        }
        memset(overlaps, 0, count);

        // Find the overlaps.
        for(i = 0, bound = boundaries; i < count; ++i, bound++)
        {
            for(k = 0; k < sector->linecount; ++k)
            {
                line = sector->Lines[k];
                if(polys[i].seg->linedef == line)
                    continue;

                if(line->flags & LINEF_SELFREF)
                    continue;

                if((overlaps[i] & OVERLAP_ALL) == OVERLAP_ALL)
                    break;

                V2_Set(a, line->L_v1pos[VX], line->L_v1pos[VY]);
                V2_Set(b, line->L_v2pos[VX], line->L_v2pos[VY]);

                // Try the left edge of the shadow.
                V2_Intercept2(bound->left, bound->a, a, b, NULL, &s, &t);
                if(s > 0 && s < 1 && t >= EPSILON && t <= 1 - EPSILON)
                    overlaps[i] |= OVERLAP_LEFT;

                // Try the right edge of the shadow.
                V2_Intercept2(bound->right, bound->b, a, b, NULL, &s, &t);
                if(s > 0 && s < 1 && t >= EPSILON && t <= 1 - EPSILON)
                    overlaps[i] |= OVERLAP_RIGHT;
            }
        }

        // Adjust the overlapping inner points.
        for(i = 0, bound = boundaries; i < count; ++i, bound++)
        {
            if(overlaps[i] & OVERLAP_LEFT)
            {
                if(R_ResolveStep(bound->left, bound->a,
                                 polys[i].inoffset[0]))
                    done = false;
            }

            if(overlaps[i] & OVERLAP_RIGHT)
            {
                if(R_ResolveStep(bound->right, bound->b,
                                 polys[i].inoffset[1]))
                    done = false;
            }
        }
    }

#undef OVERLAP_LEFT
#undef OVERLAP_RIGHT
#undef OVERLAP_ALL
#undef EPSILON
}

/**
 * New shadowpolys will be allocated from the storage.
 *
 * @param storage       If <code>NULL</code> the number of polys required
 *                      will be returned else.
 */
uint R_MakeShadowEdges(shadowpoly_t *storage)
{
    uint        i, counter;
    shadowpoly_t *poly, *sectorFirst, *allocator = storage;

    if(allocator)
        boundaryNum = 0;

    for(i = 0, counter = 0; i < numsectors; ++i)
    {
        sector_t   *sec = SECTOR_PTR(i);
        subsector_t **ssecp;

        sectorFirst = allocator;

        // Use validCount to make sure we only allocate one shadowpoly
        // per line, side.
        ++validCount;

        // Iterate all the subsectors of the sector.
        ssecp = sec->subsectors;
        while(*ssecp)
        {
            subsector_t *ssec = *ssecp;
            seg_t     **segp;

            // Iterate all the segs of the subsector.
            segp = ssec->segs;
            while(*segp)
            {
                seg_t      *seg = *segp;
                uint        fidx = GET_SECTOR_IDX(seg->SG_frontsector);
                uint        bidx = GET_SECTOR_IDX(seg->SG_backsector);

                // Minisegs and benign linedefs don't get shadows, even then, only one.
                if(seg->linedef &&
                   !((seg->linedef->validCount == validCount) ||
                     (seg->linedef->flags & LINEF_SELFREF)))
                {
                    line_t     *line = seg->linedef;
                    boolean     frontside = (line->L_frontsector == sec);

                    // If the line hasn't got two neighbors, it won't get a
                    // shadow.
                    if(!(line->vo[0]->LO_next->line == line ||
                         line->vo[1]->LO_next->line == line))
                    {
                        // This side will get a shadow.  Increment counter (we'll
                        // return this count).
                        counter++;
                        line->validCount = validCount;

                        if(allocator)
                        {
                            // Get a new shadow poly.
                            poly = allocator++;

                            poly->seg = seg;
                            poly->ssec = ssec;
                            poly->flags = (frontside? SHPF_FRONTSIDE : 0);
                            poly->visframe = frameCount - 1;

                            // The outer vertices are just the beginning and end of
                            // the line.
                            R_OrderVertices(line, sec, poly->outer);
                            R_ShadowEdges(poly);
                        }
                    }
                }

                *segp++;
            }

            *ssecp++;
        }

        if(allocator)
        {
            // If shadows were created, make sure they don't overlap
            // each other.
            R_ResolveOverlaps(sectorFirst, allocator - sectorFirst, sec);
        }
    }

    // If we have resolved overlaps; free tempoary storage used in process.
    if(allocator)
    {
        if(boundaries)
        {
            M_Free(boundaries);
            boundaries = NULL;
        }
        if(overlaps)
        {
            M_Free(overlaps);
            overlaps = NULL;
        }
    }
    return counter;
}

/**
 * Calculate sector edge shadow points, create the shadow polygons and
 * link them to the subsectors.
 */
void R_InitSectorShadows(void)
{
    uint        startTime = Sys_GetRealTime();

    uint        i, maxCount;
    shadowpoly_t *shadows, *poly;
    vec2_t      bounds[2], point;

    // Find out the number of shadowpolys we'll require.
    maxCount = R_MakeShadowEdges(NULL);

    // Allocate just enough memory.
    shadows = Z_Calloc(sizeof(shadowpoly_t) * maxCount, PU_LEVELSTATIC, NULL);
    VERBOSE(Con_Printf("R_InitSectorShadows: %i shadowpolys.\n", maxCount));

    // This'll make 'em for real.
    R_MakeShadowEdges(shadows);

    /*
     * The algorithm:
     *
     * 1. Use the subsector blockmap to look for all the blocks that are
     *    within the shadow's bounding box.
     *
     * 2. Check the subsectors whose sector == shadow's sector.
     *
     * 3. If one of the shadow's points is in the subsector, or the
     *    shadow's edges cross one of the subsector's edges (not parallel),
     *    link the shadow to the subsector.
     */
    shadowLinksBlockSet = Z_BlockCreate(sizeof(shadowlink_t), 1024, PU_LEVEL);

    for(i = 0, poly = shadows; i < maxCount; ++i, poly++)
    {
        V2_Set(point, poly->outer[0]->V_pos[VX], poly->outer[0]->V_pos[VY]);
        V2_InitBox(bounds, point);

        // Use the extended points, they are wider than inoffsets.
        V2_Sum(point, point, poly->extoffset[0]);
        V2_AddToBox(bounds, point);

        V2_Set(point, poly->outer[1]->V_pos[VX], poly->outer[1]->V_pos[VY]);
        V2_AddToBox(bounds, point);

        V2_Sum(point, point, poly->extoffset[1]);
        V2_AddToBox(bounds, point);

        P_SubsectorBoxIteratorv(bounds, R_GetShadowSector(poly, 0, false),
                                RIT_ShadowSubsectorLinker, poly);
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitSectorShadows: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
