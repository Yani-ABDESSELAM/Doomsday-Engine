/**\file p_bmap.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct blockmap_ringnode_s {
    void* object;
    struct blockmap_ringnode_s* prev;
    struct blockmap_ringnode_s* next;
} BlockmapRingNode;

typedef struct {
    BlockmapRingNode* ringNodes;
} BlockmapCell;

struct blockmap_s
{
    /// Minimal and Maximal points in map space coordinates.
    AABoxf bounds;

    /// Cell dimensions in map space coordinates.
    vec2_t cellSize;

    /// Gridmap which implements the blockmap itself.
    Gridmap* gridmap;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte bmapShowDebug = 0; // 1 = mobjs, 2 = linedefs, 3 = ssecs, 4 = polyobjs.
float bmapDebugSize = 1.5f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

uint Blockmap_CellX(Blockmap* bm, float x)
{
    uint result;
    assert(bm);
    Blockmap_ClipCellX(bm, &result, x);
    return result;
}

uint Blockmap_CellY(Blockmap* bm, float y)
{
    uint result;
    assert(bm);
    Blockmap_ClipCellY(bm, &result, y);
    return result;
}

boolean Blockmap_ClipCellX(Blockmap* bm, uint* outX, float x)
{
    boolean adjusted = false;
    assert(bm);
    if(x < bm->bounds.minX)
    {
        x = bm->bounds.minX;
        adjusted = true;
    }
    else if(x >= bm->bounds.maxX)
    {
        x = bm->bounds.maxX - 1;
        adjusted = true;
    }
    if(outX)
    {
        *outX = (uint)((x - bm->bounds.minX) / bm->cellSize[VX]);
    }
    return adjusted;
}

boolean Blockmap_ClipCellY(Blockmap* bm, uint* outY, float y)
{
    boolean adjusted = false;
    assert(bm);
    if(y < bm->bounds.minY)
    {
        y = bm->bounds.minY;
        adjusted = true;
    }
    else if(y >= bm->bounds.maxY)
    {
        y = bm->bounds.maxY - 1;
        adjusted = true;
    }
    if(outY)
    {
        *outY = (uint)((y - bm->bounds.minY) / bm->cellSize[VY]);
    }
    return adjusted;
}

boolean Blockmap_CellCoords(Blockmap* bm, uint coords[2], float const pos[2])
{
    assert(bm);
    if(coords && pos)
    {
        // Deliberate bitwise OR - we need to clip both X and Y.
        return Blockmap_ClipCellX(bm, &coords[0], pos[VX]) |
               Blockmap_ClipCellY(bm, &coords[1], pos[VY]);
    }
    return false;
}

boolean Blockmap_CellBlockCoords(Blockmap* bm, GridmapBlock* blockCoords, const AABoxf* box)
{
    assert(bm);
    if(blockCoords && box)
    {
        // Deliberate bitwise OR - we need to clip both Min and Max.
        return Blockmap_CellCoords(bm, blockCoords->min, box->min) |
               Blockmap_CellCoords(bm, blockCoords->max, box->max);
    }
    return false;
}

Blockmap* Blockmap_New(const pvec2_t min, const pvec2_t max, uint cellWidth,
    uint cellHeight, size_t sizeOfCell)
{
    Blockmap* bm = Z_Calloc(sizeof *bm, PU_MAPSTATIC, 0);
    uint width, height;
    if(!bm) Con_Error("Blockmap::New: Failed on allocation of %lu bytes for new Blockmap.", (unsigned long) sizeof *bm);

    V2_Copy(bm->bounds.min, min);
    V2_Copy(bm->bounds.max, max);
    bm->cellSize[VX] = cellWidth;
    bm->cellSize[VY] = cellHeight;

    width  = (uint)ceil((max[0] - min[0]) / (float)cellWidth);
    height = (uint)ceil((max[1] - min[1]) / (float)cellHeight);
    bm->gridmap = Gridmap_New(width, height, sizeOfCell, PU_MAPSTATIC);

    VERBOSE( Con_Message("Blockmap::New: Width:%u Height:%u\n", width, height) )
    return bm;
}

const pvec2_t Blockmap_Origin(Blockmap* bm)
{
    assert(bm);
    return bm->bounds.min;
}

const AABoxf* Blockmap_Bounds(Blockmap* bm)
{
    assert(bm);
    return &bm->bounds;
}

uint Blockmap_Width(Blockmap* bm)
{
    assert(bm);
    return Gridmap_Width(bm->gridmap);
}

uint Blockmap_Height(Blockmap* bm)
{
    assert(bm);
    return Gridmap_Height(bm->gridmap);
}

void Blockmap_Size(Blockmap* bm, uint v[2])
{
    assert(bm);
    Gridmap_Size(bm->gridmap, v);
}

float Blockmap_CellWidth(Blockmap* bm)
{
    assert(bm);
    return bm->cellSize[VX];
}

float Blockmap_CellHeight(Blockmap* bm)
{
    assert(bm);
    return bm->cellSize[VY];
}

const pvec2_t Blockmap_CellSize(Blockmap* bm)
{
    assert(bm);
    return bm->cellSize;
}

static void linkObjectToRing(void* object, BlockmapRingNode** link)
{
    BlockmapRingNode* tempLink;
    assert(object && link);

    if(!(*link))
    {
        // Create a new link at the current cell.
        *link = Z_Malloc(sizeof(BlockmapRingNode), PU_MAP, 0);
        (*link)->next = NULL;
        (*link)->prev = NULL;
        (*link)->object = object;
        return;
    }

    tempLink = *link;
    while(tempLink->next && tempLink->object)
    {
        tempLink = tempLink->next;
    }

    if(!tempLink->object)
    {
        tempLink->object = object;
        return;
    }

    tempLink->next = Z_Malloc(sizeof(BlockmapRingNode), PU_MAP, 0);
    tempLink->next->next = NULL;
    tempLink->next->prev = tempLink;
    tempLink->next->object = object;
}

/**
 * Unlink the given object from the specified cell ring (if indeed linked).
 *
 * @param object  Object to be unlinked.
 * @return  @c true iff the object was linked to the ring and was unlinked.
 */
static boolean unlinkObjectFromRing(void* object, BlockmapRingNode** list)
{
    BlockmapRingNode* iter;
    assert(object && list);

    iter = *list;
    while(iter && iter->object != object)
    {
        iter = iter->next;
    }

    if(iter)
    {
        iter->object = NULL;
        return true; // Object was unlinked.
    }
    return false; // object was not linked.
}

static int unlinkObjectInCell(void* ptr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    unlinkObjectFromRing(paramaters/*object ptr*/, &cell->ringNodes);
    return false; // Continue iteration.
}

static int linkObjectInCell(void* ptr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    linkObjectToRing(paramaters/*object ptr*/, &cell->ringNodes);
    return false; // Continue iteration.
}

boolean Blockmap_CreateCellAndLinkObjectXY(Blockmap* blockmap, uint x, uint y, void* object)
{
    BlockmapCell* cell;
    assert(blockmap && object);
    cell = (BlockmapCell*) Gridmap_CellXY(blockmap->gridmap, x, y, true);
    if(!cell) return false; // Outside the blockmap?
    linkObjectInCell((void*)cell, object);
    return true; // Link added.
}

boolean Blockmap_CreateCellAndLinkObject(Blockmap* blockmap, uint coords[2], void* object)
{
    assert(coords);
    return Blockmap_CreateCellAndLinkObjectXY(blockmap, coords[VX], coords[VY], object);
}

void Map_InitLineDefBlockmap(gamemap_t* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->lineDefBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE, sizeof(BlockmapCell));

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void Map_InitMobjBlockmap(gamemap_t* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->mobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE, sizeof(BlockmapCell));

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void Map_InitPolyobjBlockmap(gamemap_t* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->polyobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE, sizeof(BlockmapCell));

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void Map_InitSubsectorBlockmap(gamemap_t* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->subsectorBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE, sizeof(BlockmapCell));

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void Map_LinkMobjInBlockmap(gamemap_t* map, mobj_t* mo)
{
    Blockmap* blockmap = map->mobjBlockmap;
    uint coords[2];
    Blockmap_CellCoords(blockmap, coords, mo->pos);
    Blockmap_CreateCellAndLinkObject(blockmap, coords, mo);
}

boolean Map_UnlinkMobjInBlockmap(gamemap_t* map, mobj_t* mo)
{
    Blockmap* blockmap = map->mobjBlockmap;
    BlockmapCell* cell;
    boolean unlinked = false;
    uint coords[2];

    Blockmap_CellCoords(blockmap, coords, mo->pos);
    cell = (BlockmapCell*) Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        unlinked = unlinkObjectInCell((void*)cell, (void*)mo);
    }
    return unlinked;
}

void Map_LinkLineDefInBlockmap(gamemap_t* map, linedef_t* lineDef)
{
    vec2_t origin, cellSize, cell, from, to;
    GridmapBlock blockCoords;
    Blockmap* blockmap;
    uint x, y;
    assert(map);

    // LineDefs of Polyobjs don't get into the blockmap (presently...).
    if(lineDef->inFlags & LF_POLYOBJ) return;

    blockmap = map->lineDefBlockmap;
    V2_Copy(origin, Blockmap_Origin(blockmap));
    V2_Copy(cellSize, Blockmap_CellSize(blockmap));

    // Determine the block of cells we'll be working within.
    Blockmap_CellBlockCoords(blockmap, &blockCoords, &lineDef->aaBox);

    for(y = blockCoords.minY; y <= blockCoords.maxY; ++y)
    for(x = blockCoords.minX; x <= blockCoords.maxX; ++x)
    {
        if(lineDef->slopeType == ST_VERTICAL || lineDef->slopeType == ST_HORIZONTAL)
        {
            Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, lineDef);
            continue;
        }

        // Calculate cell origin.
        V2_Copy(cell, Blockmap_CellSize(blockmap));
        cell[VX] *= x; cell[VY] *= y;
        V2_Sum(cell, cell, Blockmap_Origin(blockmap));

        // Choose a cell diagonal to test.
        if(lineDef->slopeType == ST_POSITIVE)
        {
            // LineDef slope / vs \ cell diagonal.
            V2_Set(from, cell[VX], cell[VY] + cellSize[VY]);
            V2_Set(to,   cell[VX] + cellSize[VX], cell[VY]);
        }
        else
        {
            // LineDef slope \ vs / cell diagonal.
            V2_Set(from, cell[VX] + cellSize[VX], cell[VY] + cellSize[VY]);
            V2_Set(to,   cell[VX], cell[VY]);
        }

        // Would LineDef intersect this?
        if(P_PointOnLinedefSide(from, lineDef) != P_PointOnLinedefSide(to, lineDef))
        {
            Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, lineDef);
        }
    }
}

void Map_LinkSubsectorInBlockmap(gamemap_t* map, subsector_t* ssec)
{
    Blockmap* blockmap = map->subsectorBlockmap;
    GridmapBlock blockCoords;
    AABoxf aaBox;
    uint x, y;
    assert(map);

    // Subsectors without sectors don't get in.
    if(!ssec || !ssec->sector) return;

    aaBox.minX = ssec->aaBox.minX;
    aaBox.minY = ssec->aaBox.minY;
    aaBox.maxX = ssec->aaBox.maxX;
    aaBox.maxY = ssec->aaBox.maxY;
    Blockmap_CellBlockCoords(blockmap, &blockCoords, &aaBox);

    for(y = blockCoords.minY; y <= blockCoords.maxY; ++y)
    for(x = blockCoords.minX; x <= blockCoords.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, ssec);
    }
}

void Map_LinkPolyobjInBlockmap(gamemap_t* map, polyobj_t* po)
{
    Blockmap* blockmap = map->polyobjBlockmap;
    GridmapBlock blockCoords;
    uint x, y;

    P_PolyobjUpdateAABox(po);
    Blockmap_CellBlockCoords(blockmap, &blockCoords, &po->aaBox);

    for(y = blockCoords.minY; y <= blockCoords.maxY; ++y)
    for(x = blockCoords.minX; x <= blockCoords.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, po);
    }
}

void Map_UnlinkPolyobjInBlockmap(gamemap_t* map, polyobj_t* po)
{
    Blockmap* blockmap = map->polyobjBlockmap;
    GridmapBlock blockCoords;

    P_PolyobjUpdateAABox(po);
    Blockmap_CellBlockCoords(map->polyobjBlockmap, &blockCoords, &po->aaBox);
    Gridmap_BlockIterate2(blockmap->gridmap, &blockCoords, unlinkObjectInCell, (void*) po);
}

typedef struct bmapiterparams_s {
    int localValidCount;
    int (*func) (linedef_t*, void *);
    void* param;
} bmapiterparams_t;

static int blockmapCellLinesIterator(void* ptr, void* context)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    bmapiterparams_t* args = (bmapiterparams_t*) context;
    BlockmapRingNode* next, *link = cell->ringNodes;
    int result = false; // Continue iteration.
    while(link)
    {
        next = link->next;

        if(link->object)
        {
            linedef_t* lineDef = (linedef_t*)link->object;
            if(lineDef->validCount != args->localValidCount)
            {
                lineDef->validCount = args->localValidCount;
                result = args->func(lineDef, args->param);
                if(result) return result; // Stop iteration.
            }
        }

        link = next;
    }
    return result;
}

int Map_IterateCellLineDefs(gamemap_t* map, const uint coords[2],
    int (*func) (linedef_t*, void*), void* data)
{
    Blockmap* blockmap = map->lineDefBlockmap;
    BlockmapCell* cell = Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        bmapiterparams_t args;
        args.localValidCount = validCount;
        args.func = func;
        args.param = data;
        return blockmapCellLinesIterator(cell, &args);
    }
    return false;
}

int Map_IterateCellBlockLineDefs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*func) (linedef_t*, void*), void* data)
{
    Blockmap* blockmap = map->lineDefBlockmap;
    bmapiterparams_t args;
    args.localValidCount = validCount;
    args.func = func;
    args.param = data;
    return Gridmap_BlockIterate2(blockmap->gridmap, blockCoords, blockmapCellLinesIterator, (void*) &args);
}

typedef struct bmappoiterparams_s {
    int localValidCount;
    int (*func) (polyobj_t*, void *);
    void* param;
} bmappoiterparams_t;

static int blockmapCellPolyobjsIterator(void* ptr, void* context)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    bmappoiterparams_t* args = (bmappoiterparams_t*) context;
    BlockmapRingNode* next, *link = cell->ringNodes;
    int result = false; // Continue iteration.
    while(link)
    {
        next = link->next;

        if(link->object)
        {
            polyobj_t* polyobj = (polyobj_t*)link->object;
            if(polyobj->validCount != args->localValidCount)
            {
                polyobj->validCount = args->localValidCount;
                result = args->func(polyobj, args->param);
                if(result) return result; // Stop iteration.
            }
        }

        link = next;
    }
    return result;
}

int Map_IterateCellPolyobjs(gamemap_t* map, const uint coords[2],
    int (*func) (polyobj_t*, void*), void* data)
{
    Blockmap* blockmap = map->polyobjBlockmap;
    BlockmapCell* cell = Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        bmappoiterparams_t  args;
        args.localValidCount = validCount;
        args.func = func;
        args.param = data;
        return blockmapCellPolyobjsIterator(cell, (void*) &args);
    }
    return false;
}

int Map_IterateCellBlockPolyobjs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*func) (polyobj_t*, void*), void* data)
{
    Blockmap* blockmap = map->polyobjBlockmap;
    bmappoiterparams_t args;
    args.localValidCount = validCount;
    args.func = func;
    args.param = data;
    return Gridmap_BlockIterate2(blockmap->gridmap, blockCoords, blockmapCellPolyobjsIterator, (void*) &args);
}

typedef struct poiterparams_s {
    int (*func) (linedef_t*, void*);
    void* param;
} poiterparams_t;

int PTR_PolyobjLines(polyobj_t* po, void* data)
{
    poiterparams_t* args = (poiterparams_t*) data;
    return P_PolyobjLinesIterator(po, args->func, args->param);
}

int Map_IterateCellPolyobjLineDefsIterator(gamemap_t* map, const uint coords[2],
    int (*func) (linedef_t*, void*), void* data)
{
    Blockmap* blockmap = map->polyobjBlockmap;
    BlockmapCell* cell = Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        bmappoiterparams_t args;
        poiterparams_t poargs;
        poargs.func = func;
        poargs.param = data;

        args.localValidCount = validCount;
        args.func = PTR_PolyobjLines;
        args.param = &poargs;
        return blockmapCellPolyobjsIterator(cell, &args);
    }
    return false;
}

int Map_IterateCellBlockPolyobjLineDefs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*callback) (linedef_t*, void*), void* paramaters)
{
    Blockmap* blockmap = map->polyobjBlockmap;
    bmappoiterparams_t bpiParams;
    poiterparams_t piParams;

    piParams.func = callback;
    piParams.param = paramaters;

    bpiParams.localValidCount = validCount;
    bpiParams.func = PTR_PolyobjLines;
    bpiParams.param = &piParams;

    return Gridmap_BlockIterate2(blockmap->gridmap, blockCoords, blockmapCellPolyobjsIterator, (void*) &bpiParams);
}

typedef struct bmapmoiterparams_s {
    int localValidCount;
    int (*func) (mobj_t*, void *);
    void* param;
} bmapmoiterparams_t;

static int blockmapCellMobjsIterator(void* ptr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    bmapmoiterparams_t* args = (bmapmoiterparams_t*) paramaters;
    BlockmapRingNode* next, *link = cell->ringNodes;
    int result = false; // Continue iteration.
    while(link)
    {
        next = link->next;

        if(link->object)
        {
            mobj_t* mobj = (mobj_t*)link->object;
            if(mobj->validCount != args->localValidCount)
            {
                mobj->validCount = args->localValidCount;
                result = args->func(mobj, args->param);
                if(result) return result; // Stop iteration.
            }
        }

        link = next;
    }
    return result;
}

int Map_IterateCellMobjs(gamemap_t* map, const uint coords[2],
    int (*func) (mobj_t*, void*), void* data)
{
    Blockmap* blockmap = map->mobjBlockmap;
    BlockmapCell* cell = Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        bmapmoiterparams_t  args;
        args.localValidCount = validCount;
        args.func = func;
        args.param = data;
        return blockmapCellMobjsIterator(cell, (void*) &args);
    }
    return false;
}

int Map_IterateCellBlockMobjs(gamemap_t* map, const GridmapBlock* blockCoords,
    int (*func) (mobj_t*, void*), void* data)
{
    Blockmap* blockmap = map->mobjBlockmap;
    bmapmoiterparams_t args;
    args.localValidCount = validCount;
    args.func = func;
    args.param = data;
    return Gridmap_BlockIterate2(blockmap->gridmap, blockCoords, blockmapCellMobjsIterator, (void*) &args);
}

typedef struct sseciterparams_s {
    const AABoxf* box;
    sector_t* sector;
    int localValidCount;
    int (*func) (subsector_t*, void*);
    void* param;
} bmapsseciterparams_t;

static int blockmapCellSubsectorsIterator(void* ptr, void* context)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    bmapsseciterparams_t* args = (bmapsseciterparams_t*) context;
    BlockmapRingNode* next, *link = cell->ringNodes;
    int result = false; // Continue iteration.
    while(link)
    {
        next = link->next;

        if(link->object)
        {
            subsector_t* ssec = (subsector_t*)link->object;
            if(ssec->validCount != args->localValidCount)
            {
                boolean ok = true;

                ssec->validCount = args->localValidCount;

                // Check the sector restriction.
                if(args->sector && ssec->sector != args->sector)
                    ok = false;

                // Check the bounds.
                if(args->box &&
                   (ssec->aaBox.maxX < args->box->minX ||
                    ssec->aaBox.minX > args->box->maxX ||
                    ssec->aaBox.minY > args->box->maxY ||
                    ssec->aaBox.maxY < args->box->minY))
                    ok = false;

                if(ok)
                {
                    result = args->func(ssec, args->param);
                    if(result) return result; // Stop iteration.
                }
            }
        }

        link = next;
    }
    return result;
}

int Map_IterateCellSubsectors(gamemap_t* map, const uint coords[2],
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*func) (subsector_t*, void*), void* paramaters)
{
    Blockmap* blockmap = map->subsectorBlockmap;
    BlockmapCell* cell = Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        bmapsseciterparams_t args;
        args.localValidCount = localValidCount;
        args.func = func;
        args.param = paramaters;
        args.sector = sector;
        args.box = box;
        return blockmapCellSubsectorsIterator(cell, (void*) &args);
    }
    return false;
}

int Map_IterateCellBlockSubsectors(gamemap_t* map, const GridmapBlock* blockCoords,
    sector_t* sector,  const AABoxf* box, int localValidCount,
    int (*func) (subsector_t*, void*), void* data)
{
    Blockmap* blockmap = map->subsectorBlockmap;
    bmapsseciterparams_t args;
    args.localValidCount = localValidCount;
    args.func = func;
    args.param = data;
    args.sector = sector;
    args.box = box;
    return Gridmap_BlockIterate2(blockmap->gridmap, blockCoords, blockmapCellSubsectorsIterator, (void*) &args);
}

static int rendMobj(mobj_t* mo, void* paramaters)
{
    vec2_t start, end;
    V2_Set(start, mo->pos[VX] - mo->radius, mo->pos[VY] - mo->radius);
    V2_Set(end,   mo->pos[VX] + mo->radius, mo->pos[VY] + mo->radius);
    glVertex2f(start[VX], start[VY]);
    glVertex2f(  end[VX], start[VY]);
    glVertex2f(  end[VX],   end[VY]);
    glVertex2f(start[VX],   end[VY]);
    return false; // Continue iteration.
}

static int rendLineDef(linedef_t* line, void* paramaters)
{
    glVertex2f(line->L_v1pos[VX], line->L_v1pos[VY]);
    glVertex2f(line->L_v2pos[VX], line->L_v2pos[VY]);
    return false; // Continue iteration.
}

static int rendSubsector(subsector_t* ssec, void* paramaters)
{
    const float scale = MAX_OF(bmapDebugSize, 1);
    const float width = (theWindow->width / 16) / scale;
    float length, dx, dy, normal[2], unit[2];
    seg_t** segIter, *seg;
    vec2_t start, end;

    for(segIter = ssec->segs; *segIter; segIter++)
    {
        seg = *segIter;

        V2_Set(start, seg->SG_v1pos[VX], seg->SG_v1pos[VY]);
        V2_Set(end,   seg->SG_v2pos[VX], seg->SG_v2pos[VY]);

        glBegin(GL_LINES);
            glVertex2fv(start);
            glVertex2fv(end);
        glEnd();

        dx = end[VX] - start[VX];
        dy = end[VY] - start[VY];
        length = sqrt(dx * dx + dy * dy);
        if(length > 0)
        {
            unit[VX] = dx / length;
            unit[VY] = dy / length;
            normal[VX] = -unit[VY];
            normal[VY] = unit[VX];

            glBindTexture(GL_TEXTURE_2D, GL_PrepareLSTexture(LST_DYNAMIC));
            glEnable(GL_TEXTURE_2D);
            GL_BlendOp(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glBegin(GL_QUADS);
                glTexCoord2f(0.75f, 0.5f);
                glVertex2fv(start);
                glTexCoord2f(0.75f, 0.5f);
                glVertex2fv(end);
                glTexCoord2f(0.75f, 1);
                glVertex2f(end[VX] - normal[VX] * width,
                           end[VY] - normal[VY] * width);
                glTexCoord2f(0.75f, 1);
                glVertex2f(start[VX] - normal[VX] * width,
                           start[VY] - normal[VY] * width);
            glEnd();

            glDisable(GL_TEXTURE_2D);
            GL_BlendMode(BM_NORMAL);
        }

        // Draw the bounding box.
        V2_Set(start, ssec->aaBox.minX, ssec->aaBox.minY);
        V2_Set(end,   ssec->aaBox.maxX, ssec->aaBox.maxY);

        glBegin(GL_LINES);
            glVertex2f(start[VX], start[VY]);
            glVertex2f(  end[VX], start[VY]);
            glVertex2f(  end[VX], start[VY]);
            glVertex2f(  end[VX],   end[VY]);
            glVertex2f(  end[VX],   end[VY]);
            glVertex2f(start[VX],   end[VY]);
            glVertex2f(start[VX],   end[VY]);
            glVertex2f(start[VX], start[VY]);
        glEnd();
    }
    return false; // Continue iteration.
}

int rendCellLineDefs(void* cellPtr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*)cellPtr;
    if(cell && cell->ringNodes)
    {
        bmapiterparams_t biParams;
        biParams.localValidCount = validCount;
        biParams.func = rendLineDef;
        biParams.param = paramaters;

        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINES);
            blockmapCellLinesIterator(cell, (void*)&biParams);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }
    return false; // Continue iteration.
}

int rendCellPolyobjs(void* cellPtr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*)cellPtr;
    if(cell && cell->ringNodes)
    {
        bmappoiterparams_t bpiParams;
        poiterparams_t piParams;

        piParams.func = rendLineDef;
        piParams.param = paramaters;

        bpiParams.localValidCount = validCount;
        bpiParams.func = PTR_PolyobjLines;
        bpiParams.param = &piParams;

        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINES);
            blockmapCellPolyobjsIterator(cell, (void*)&bpiParams);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }
    return false; // Continue iteration.
}

int rendCellMobjs(void* cellPtr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*)cellPtr;
    if(cell && cell->ringNodes)
    {
        bmapmoiterparams_t bmiParams;
        bmiParams.localValidCount = validCount;
        bmiParams.func = rendMobj;
        bmiParams.param = paramaters;

        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            blockmapCellMobjsIterator(cell, (void*)&bmiParams);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }
    return false; // Continue iteration.
}

int rendCellSubsectors(void* cellPtr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*)cellPtr;
    if(cell && cell->ringNodes)
    {
        bmapsseciterparams_t bsiParams;
        bsiParams.localValidCount = validCount;
        bsiParams.func = rendSubsector;
        bsiParams.param = paramaters;
        bsiParams.sector = NULL;
        bsiParams.box = NULL;

        glDisable(GL_TEXTURE_2D);
            blockmapCellSubsectorsIterator(cell, (void*)&bsiParams);
        glEnable(GL_TEXTURE_2D);
    }
    return false; // Continue iteration.
}

void rendBlockmapBackground(Blockmap* blockmap)
{
    vec2_t start, end;
    uint x, y, bmapSize[2];
    assert(blockmap);

    Gridmap_Size(blockmap->gridmap, bmapSize);

    // Scale modelview matrix so we can express cell geometry
    // using a cell-sized unit coordinate space.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(blockmap->cellSize[VX], blockmap->cellSize[VY], 1);

    glDisable(GL_TEXTURE_2D);

    /**
     * Draw the translucent quad which represents the "used" cells.
     */
    V2_Set(start, 0, 0);
    V2_Set(end, bmapSize[VX], bmapSize[VY]);
    glColor4f(.25f, .25f, .25f, .66f);
    glBegin(GL_QUADS);
        glVertex2f(start[VX], start[VY]);
        glVertex2f(  end[VX], start[VY]);
        glVertex2f(  end[VX],   end[VY]);
        glVertex2f(start[VX],   end[VY]);
    glEnd();

    /**
     * Draw the "null cells" over the top.
     */
    glColor4f(0, 0, 0, .95f);
    for(y = 0; y < bmapSize[VY]; ++y)
    for(x = 0; x < bmapSize[VX]; ++x)
    {
        // If this cell has user data its not null.
        if(Gridmap_CellXY(blockmap->gridmap, x, y, false)) continue;

        glBegin(GL_QUADS);
            glVertex2f(x,   y);
            glVertex2f(x+1, y);
            glVertex2f(x+1, y+1);
            glVertex2f(x,   y+1);
        glEnd();
    }

    // Restore previous GL state.
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static void drawCellInfo(int x, int y, const char* info)
{
    const int w = FR_TextWidth(info)  + 16;
    const int h = FR_TextHeight(info) + 16;

    x -= w / 2;
    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx(info, x + 8, y + h / 2, false, true, UI_Color(UIC_TITLE), 1);
}

static void drawBlockmapInfo(int x, int y, Blockmap* blockmap)
{
    const int w = 16 + FR_TextWidth("(+000.0,+000.0)(+000.0,+000.0)");
    const int th = FR_TextHeight("a"), h = th * 4 + 16;
    uint bmapSize[2];
    char buf[80];
    assert(blockmap);

    // Align to the bottom right.
    x -= w;
    y -= h;

    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);
    x += 8;
    y += 8 + th/2;

    UI_TextOutEx("Blockmap", x, y, false, true, UI_Color(UIC_TITLE), 1);
    y += th;

    Blockmap_Size(blockmap, bmapSize);
    dd_snprintf(buf, 80, "Dimensions:[%u,%u] #%li", bmapSize[VX], bmapSize[VY],
        (long) bmapSize[VY] * bmapSize[VX]);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;

    dd_snprintf(buf, 80, "Cellsize:[%.3f,%.3f]", Blockmap_CellWidth(blockmap), Blockmap_CellHeight(blockmap));
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;

    dd_snprintf(buf, 80, "(%+06.0f,%+06.0f)(%+06.0f,%+06.0f)",
        Blockmap_Bounds(blockmap)->minX, Blockmap_Bounds(blockmap)->minY,
        Blockmap_Bounds(blockmap)->maxX, Blockmap_Bounds(blockmap)->maxY);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;
}

static int countLineDefLink(linedef_t* lineDef, void* paramaters)
{
    uint* count = (uint*)paramaters;
    *count += 1;
    return false; // Continue iteration.
}

static int countMobjLink(mobj_t* mobj, void* paramaters)
{
    uint* count = (uint*)paramaters;
    *count += 1;
    return false; // Continue iteration.
}

static int countPolyobjLink(polyobj_t* polyobj, void* paramaters)
{
    uint* count = (uint*)paramaters;
    *count += 1;
    return false; // Continue iteration.
}

static int countSubsectorLink(subsector_t* subsector, void* paramaters)
{
    uint* count = (uint*)paramaters;
    *count += 1;
    return false; // Continue iteration.
}

static void drawLineDefCellInfoBox(int x, int y, uint coords[2])
{
    gamemap_t* map = P_GetCurrentMap();
    uint count = 0;
    char info[160];
    validCount++;
    Map_IterateCellLineDefs(map, coords, countLineDefLink, (void*)&count);
    dd_snprintf(info, 160, "Cell:[%u,%u] LineDefs:#%u", coords[VX], coords[VY], count);
    drawCellInfo(x, y, info);
}

static void drawMobjCellInfoBox(int x, int y, uint coords[2])
{
    gamemap_t* map = P_GetCurrentMap();
    uint count = 0;
    char info[160];
    validCount++;
    Map_IterateCellMobjs(map, coords, countMobjLink, (void*)&count);
    dd_snprintf(info, 160, "Cell:[%u,%u] Mobjs:#%u", coords[VX], coords[VY], count);
    drawCellInfo(x, y, info);
}

static void drawPolyobjCellInfoBox(int x, int y, uint coords[2])
{
    gamemap_t* map = P_GetCurrentMap();
    uint count = 0;
    char info[160];
    validCount++;
    Map_IterateCellPolyobjs(map, coords, countPolyobjLink, (void*)&count);
    dd_snprintf(info, 160, "Cell:[%u,%u] Polyobjs:#%u", coords[VX], coords[VY], count);
    drawCellInfo(x, y, info);
}

static void drawSubsectorCellInfoBox(int x, int y, uint coords[2])
{
    gamemap_t* map = P_GetCurrentMap();
    uint count = 0;
    char info[160];
    validCount++;
    Map_IterateCellSubsectors(map, coords, NULL/*no sector requirement*/,
        NULL/*no subregion requirement*/, validCount, countSubsectorLink, (void*)&count);
    dd_snprintf(info, 160, "Cell:[%u,%u] Subsectors:#%u", coords[VX], coords[VY], count);
    drawCellInfo(x, y, info);
}

/**
 * @param blockmap  Blockmap to be rendered.
 * @param followMobj  Mobj to center/focus the visual on. Can be @c NULL.
 * @param cellDrawer  Blockmap cell content drawing callback. Can be @a NULL.
 */
static void rendBlockmap(Blockmap* blockmap, mobj_t* followMobj,
    int (*cellDrawer) (void* cellPtr, void* paramaters))
{
    uint x, y, vCoords[2];
    GridmapBlock vBlockCoords;
    vec2_t start, end, cellSize;
    void* cell;

    V2_Copy(cellSize, Blockmap_CellSize(blockmap));

    if(followMobj)
    {
        // Determine the followed Mobj's blockmap coords.
        if(Blockmap_CellCoords(blockmap, vCoords, followMobj->pos))
            followMobj = NULL; // Outside the blockmap.

        if(followMobj)
        {
            // Determine the extended blockmap coords for the followed
            // Mobj's "touch" range.
            const float radius = followMobj->radius + DDMOBJ_RADIUS_MAX * 2;
            AABoxf aaBox;
            V2_Set(start, followMobj->pos[VX] - radius, followMobj->pos[VY] - radius);
            V2_Set(end,   followMobj->pos[VX] + radius, followMobj->pos[VY] + radius);
            V2_InitBox(aaBox.arvec2, start);
            V2_AddToBox(aaBox.arvec2, end);
            Blockmap_CellBlockCoords(blockmap, &vBlockCoords, &aaBox);
        }
    }

    if(followMobj)
    {
        // Orient on the center of the followed Mobj.
        V2_Set(start, vCoords[VX] * cellSize[VX],
                      vCoords[VY] * cellSize[VY]);
        glTranslatef(-start[VX], -start[VY], 0);
    }
    else
    {
        // Orient on center of the Blockmap.
        glTranslatef(-(cellSize[VX] * Blockmap_Width(blockmap))/2,
                     -(cellSize[VY] * Blockmap_Height(blockmap))/2, 0);
    }

    // First we'll draw a background showing the "null" cells.
    rendBlockmapBackground(blockmap);
    if(followMobj)
    {
        // Highlight cells the followed Mobj "touches".
        glDisable(GL_TEXTURE_2D);

        for(y = vBlockCoords.minY; y <= vBlockCoords.maxY; ++y)
        for(x = vBlockCoords.minX; x <= vBlockCoords.maxX; ++x)
        {
            if(x == vCoords[VX] && y == vCoords[VY])
            {
                // The cell the followed Mobj is actually in.
                glColor4f(.66f, .66f, 1, .66f);
            }
            else
            {
                // A cell within the followed Mobj's extended collision range.
                glColor4f(.33f, .33f, .66f, .33f);
            }

            V2_Set(start, x * cellSize[VX], y * cellSize[VY]);
            V2_Set(end, cellSize[VX], cellSize[VY]);
            V2_Sum(end, end, start);

            glBegin(GL_QUADS);
                glVertex2f(start[VX], start[VY]);
                glVertex2f(  end[VX], start[VY]);
                glVertex2f(  end[VX],   end[VY]);
                glVertex2f(start[VX],   end[VY]);
            glEnd();
        }

        glEnable(GL_TEXTURE_2D);
    }

    /**
     * Draw the Gridmap visual.
     * \note Gridmap uses a cell unit size of [width:1,height:1], so we
     * need to scale it up so it aligns correctly.
     */
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(cellSize[VX], cellSize[VY], 1);

    Gridmap_Debug(blockmap->gridmap);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    /**
     * Draw the blockmap-linked data.
     * Translate the modelview matrix so that objects can be drawn using
     * the map space coordinates directly.
     */
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(-Blockmap_Origin(blockmap)[VX], -Blockmap_Origin(blockmap)[VY], 0);

    if(cellDrawer)
    {
        if(followMobj)
        {
            /**
             * Draw cell contents color coded according to their range
             * from the followed Mobj.
             */

            // First, the cells outside the "touch" range (crimson).
            validCount++;
            glColor4f(.33f, 0, 0, .75f);
            for(y = 0; y < Blockmap_Height(blockmap); ++y)
            for(x = 0; x < Blockmap_Width(blockmap); ++x)
            {
                if(x >= vBlockCoords.minX && x <= vBlockCoords.maxX &&
                   y >= vBlockCoords.minY && y <= vBlockCoords.maxY)
                    continue;

                cell = Gridmap_CellXY(blockmap->gridmap, x, y, false);
                if(cell)
                {
                    cellDrawer(cell, NULL/*no params*/);
                }
            }

            // Next, the cells within the "touch" range (orange).
            validCount++;
            glColor3f(1, .5f, 0);
            for(y = vBlockCoords.minY; y <= vBlockCoords.maxY; ++y)
            for(x = vBlockCoords.minX; x <= vBlockCoords.maxX; ++x)
            {
                if(x == vCoords[VX] && y == vCoords[VY]) continue;

                cell = Gridmap_CellXY(blockmap->gridmap, x, y, false);
                if(cell)
                {
                    cellDrawer(cell, NULL/*no params*/);
                }
            }

            // Lastly, the cell the followed Mobj is in (yellow).
            validCount++;
            glColor3f(1, 1, 0);
            cell = Gridmap_Cell(blockmap->gridmap, vCoords, false);
            if(cell)
            {
                cellDrawer(cell, NULL/*no params*/);
            }
        }
        else
        {
            // Draw all cells without color coding.
            validCount++;
            glColor4f(.33f, 0, 0, .75f);
            Gridmap_Iterate(blockmap->gridmap, cellDrawer);
        }
    }

    /**
     * Draw the followed mobj, if any.
     */
    if(followMobj)
    {
        glColor3f(0, 1, 0);
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            rendMobj(followMobj, NULL/*no params*/);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }

    // Undo the map coordinate space translation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Restore previous GL state.
    glEnable(GL_TEXTURE_2D);
}

void Rend_BlockmapDebug(void)
{
    int (*cellDrawer) (void* cellPtr, void* paramaters);
    void (*cellInfoDrawer) (int x, int y, uint coords[2]);
    mobj_t* followMobj = NULL;
    Blockmap* blockmap;
    gamemap_t* map;
    float scale;

    // Disabled?
    if(!bmapShowDebug || bmapShowDebug > 4) return;

    map = P_GetCurrentMap();
    if(!map) return;

    switch(bmapShowDebug)
    {
    default: // MobjLinks.
        if(!map->mobjBlockmap) return;

        blockmap = map->mobjBlockmap;
        cellDrawer = rendCellMobjs;
        cellInfoDrawer = drawMobjCellInfoBox;
        break;

    case 2: // LineDefs.
        if(!map->lineDefBlockmap) return;

        blockmap = map->lineDefBlockmap;
        cellDrawer = rendCellLineDefs;
        cellInfoDrawer = drawLineDefCellInfoBox;
        break;

    case 3: // Subsectors.
        if(!map->subsectorBlockmap) return;

        blockmap = map->subsectorBlockmap;
        cellDrawer = rendCellSubsectors;
        cellInfoDrawer = drawSubsectorCellInfoBox;
        break;

    case 4: // PolyobjLinks.
        if(!map->polyobjBlockmap) return;

        blockmap = map->polyobjBlockmap;
        cellDrawer = rendCellPolyobjs;
        cellInfoDrawer = drawPolyobjCellInfoBox;
        break;
    }

    /**
     * Draw the blockmap.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);
    // Orient on the center of the window.
    glTranslatef((theWindow->width / 2), (theWindow->height / 2), 0);

    // Uniform scaling factor for this visual.
    scale = bmapDebugSize / MAX_OF(theWindow->height / 100, 1);
    glScalef(scale, -scale, 1);

    // If possible we'll tailor what we draw relative to the viewPlayer.
    if(viewPlayer && viewPlayer->shared.mo)
        followMobj = viewPlayer->shared.mo;

    // Draw!
    rendBlockmap(blockmap, followMobj, cellDrawer);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    /**
     * Draw HUD info.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    if(followMobj)
    {
        // About the cell the followed Mobj is in.
        uint coords[2];
        if(!Blockmap_CellCoords(blockmap, coords, followMobj->pos))
        {
            const int x = theWindow->width / 2;
            const int y = 30;
            cellInfoDrawer(x, y, coords);
        }
    }

    // About the Blockmap itself.
    drawBlockmapInfo(theWindow->width - 10, theWindow->height - 10, blockmap);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
