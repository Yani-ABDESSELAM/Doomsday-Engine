/**
 * @file gamemap.h
 * Gamemap. @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_GAMEMAP_H
#define LIBDENG_GAMEMAP_H

#include "p_particle.h"

struct thinkerlist_s;
struct clmoinfo_s;
struct generators_s;

/// Size of Blockmap blocks in map units. Must be an integer power of two.
#define MAPBLOCKUNITS               (128)

struct blockmap_s;
struct gridmapblock_s;

/**
 * The client mobjs are stored into a hash for quickly locating a ClMobj by its identifier.
 */
#define CLIENT_MOBJ_HASH_SIZE       (256)

typedef struct cmhash_s {
    struct clmoinfo_s *first, *last;
} cmhash_t;

#define CLIENT_MAX_MOVERS          1024 // Definitely enough!

typedef enum {
    CPT_FLOOR,
    CPT_CEILING
} clplanetype_t;

struct clplane_s;
struct clpolyobj_s;

typedef struct gamemap_s {
    Uri* uri;
    char uniqueId[256];

    float bBox[4];

    struct thinkers_s {
        int idtable[2048]; // 65536 bits telling which IDs are in use.
        unsigned short iddealer;

        size_t numLists;
        struct thinkerlist_s** lists;
        boolean inited;
    } thinkers;

    struct generators_s* generators;

    // Client only data:
    cmhash_t clMobjHash[CLIENT_MOBJ_HASH_SIZE];

    struct clplane_s* clActivePlanes[CLIENT_MAX_MOVERS];
    struct clpolyobj_s* clActivePolyobjs[CLIENT_MAX_MOVERS];
    // End client only data.

    uint numVertexes;
    vertex_t* vertexes;

    uint numHEdges;
    HEdge* hedges;

    uint numSectors;
    sector_t* sectors;

    uint numBspLeafs;
    BspLeaf* bspLeafs;

    uint numBspNodes;
    BspNode* bspNodes;

    uint numLineDefs;
    linedef_t* lineDefs;

    uint numSideDefs;
    sidedef_t* sideDefs;

    uint numPolyObjs;
    polyobj_t** polyObjs;

    gameobjdata_t gameObjData;

    planelist_t trackedPlanes;
    surfacelist_t scrollingSurfaces;
    surfacelist_t decoratedSurfaces;
    surfacelist_t glowingSurfaces;

    struct blockmap_s* mobjBlockmap;
    struct blockmap_s* polyobjBlockmap;
    struct blockmap_s* lineDefBlockmap;
    struct blockmap_s* bspLeafBlockmap;

    nodepile_t mobjNodes, lineNodes; // All kinds of wacky links.
    nodeindex_t* lineLinks; // Indices to roots.

    float globalGravity; // The defined gravity for this map.
    float effectiveGravity; // The effective gravity for this map.

    int ambientLightLevel; // Ambient lightlevel for the current map.

    /// Current LOS trace state.
    /// @todo Refactor to support concurrent traces.
    TraceOpening traceOpening;
    divline_t traceLOS;
} GameMap;

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const Uri* GameMap_Uri(GameMap* map);

/// @return  The old 'unique' identifier of the map.
const char* GameMap_OldUniqueId(GameMap* map);

void GameMap_Bounds(GameMap* map, float* min, float* max);

/**
 * Retrieve the current effective gravity multiplier for this map.
 *
 * @param map  GameMap instance.
 * @return  Effective gravity multiplier for this map.
 */
float GameMap_Gravity(GameMap* map);

/**
 * Change the effective gravity multiplier for this map.
 *
 * @param map  GameMap instance.
 * @param gravity  New gravity multiplier.
 * @return  Same as @a map for caller convenience.
 */
GameMap* GameMap_SetGravity(GameMap* map, float gravity);

/**
 * Return the effective gravity multiplier to that originally defined for this map.
 *
 * @param map  GameMap instance.
 * @return  Same as @a map for caller convenience.
 */
GameMap* GameMap_RestoreGravity(GameMap* map);

/**
 * Retrieve an immutable copy of the LOS trace line.
 *
 * @param map  GameMap instance.
 */
const divline_t* GameMap_TraceLOS(GameMap* map);

/**
 * Retrieve an immutable copy of the LOS TraceOpening state.
 *
 * @param map  GameMap instance.
 */
const TraceOpening* GameMap_TraceOpening(GameMap* map);

/**
 * Update the TraceOpening state for according to the opening defined by the
 * inner-minimal planes heights which intercept @a linedef
 *
 * If @a lineDef is not owned by the map this is a no-op.
 *
 * @param map  GameMap instance.
 * @param lineDef  LineDef to configure the opening for.
 */
void GameMap_SetTraceOpening(GameMap* map, linedef_t* lineDef);

/**
 * Retrieve the map-global ambient light level.
 *
 * @param map  GameMap instance.
 * @return  Ambient light level.
 */
int GameMap_AmbientLightLevel(GameMap* map);

/**
 * Lookup a Vertex by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the vertex.
 * @return  Pointer to Vertex with this index else @c NULL if @a idx is not valid.
 */
vertex_t* GameMap_Vertex(GameMap* map, uint idx);

/**
 * Lookup a LineDef by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the linedef.
 * @return  Pointer to LineDef with this index else @c NULL if @a idx is not valid.
 */
linedef_t* GameMap_LineDef(GameMap* map, uint idx);

/**
 * Lookup a SideDef by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the sidedef.
 * @return  Pointer to SideDef with this index else @c NULL if @a idx is not valid.
 */
sidedef_t* GameMap_SideDef(GameMap* map, uint idx);

/**
 * Lookup a Sector by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the sector.
 * @return  Pointer to Sector with this index else @c NULL if @a idx is not valid.
 */
sector_t* GameMap_Sector(GameMap* map, uint idx);

/**
 * Lookup a BspLeaf by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the bsp leaf.
 * @return  Pointer to BspLeaf with this index else @c NULL if @a idx is not valid.
 */
BspLeaf* GameMap_BspLeaf(GameMap* map, uint idx);

/**
 * Lookup a HEdge by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the hedge.
 * @return  Pointer to HEdge with this index else @c NULL if @a idx is not valid.
 */
HEdge* GameMap_HEdge(GameMap* map, uint idx);

/**
 * Lookup a BspNode by its unique index.
 *
 * @param map  GameMap instance.
 * @param idx  Unique index of the bsp node.
 * @return  Pointer to BspNode with this index else @c NULL if @a idx is not valid.
 */
BspNode* GameMap_BspNode(GameMap* map, uint idx);

/**
 * Lookup the unique index for @a vertex.
 *
 * @param map  GameMap instance.
 * @param vtx  Vertex to lookup.
 * @return  Unique index for the Vertex else @c -1 if not present.
 */
int GameMap_VertexIndex(GameMap* map, vertex_t* vtx);

/**
 * Lookup the unique index for @a lineDef.
 *
 * @param map  GameMap instance.
 * @param line  LineDef to lookup.
 * @return  Unique index for the LineDef else @c -1 if not present.
 */
int GameMap_LineDefIndex(GameMap* map, linedef_t* line);

/**
 * Lookup the unique index for @a sideDef.
 *
 * @param map  GameMap instance.
 * @param side  SideDef to lookup.
 * @return  Unique index for the SideDef else @c -1 if not present.
 */
int GameMap_SideDefIndex(GameMap* map, sidedef_t* side);

/**
 * Lookup the unique index for @a sector.
 *
 * @param map  GameMap instance.
 * @param sector  Sector to lookup.
 * @return  Unique index for the Sector else @c -1 if not present.
 */
int GameMap_SectorIndex(GameMap* map, sector_t* sector);

/**
 * Lookup the unique index for @a bspLeaf.
 *
 * @param map  GameMap instance.
 * @param bspLeaf  BspLeaf to lookup.
 * @return  Unique index for the BspLeaf else @c -1 if not present.
 */
int GameMap_BspLeafIndex(GameMap* map, BspLeaf* bspLeaf);

/**
 * Lookup the unique index for @a hedge.
 *
 * @param map  GameMap instance.
 * @param hedge  HEdge to lookup.
 * @return  Unique index for the HEdge else @c -1 if not present.
 */
int GameMap_HEdgeIndex(GameMap* map, HEdge* hedge);

/**
 * Lookup the unique index for @a node.
 *
 * @param map  GameMap instance.
 * @param bspNode  BspNode to lookup.
 * @return  Unique index for the BspNode else @c -1 if not present.
 */
int GameMap_BspNodeIndex(GameMap* map, BspNode* bspNode);

/**
 * Retrieve the number of Vertex instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Vertexes.
 */
uint GameMap_VertexCount(GameMap* map);

/**
 * Retrieve the number of LineDef instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number LineDef.
 */
uint GameMap_LineDefCount(GameMap* map);

/**
 * Retrieve the number of SideDef instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number SideDef.
 */
uint GameMap_SideDefCount(GameMap* map);

/**
 * Retrieve the number of Sector instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Sector.
 */
uint GameMap_SectorCount(GameMap* map);

/**
 * Retrieve the number of BspLeaf instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number BspLeaf.
 */
uint GameMap_BspLeafCount(GameMap* map);

/**
 * Retrieve the number of HEdge instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number HEdge.
 */
uint GameMap_HEdgeCount(GameMap* map);

/**
 * Retrieve the number of BspNode instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number BspNode.
 */
uint GameMap_BspNodeCount(GameMap* map);

/**
 * Retrieve the number of Polyobj instances owned by this.
 *
 * @param map  GameMap instance.
 * @return  Number Polyobj.
 */
uint GameMap_PolyobjCount(GameMap* map);

/**
 * Lookup a Polyobj in the map by unique ID.
 *
 * @param map  GameMap instance.
 * @param id  Unique identifier of the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* GameMap_PolyobjByID(GameMap* map, uint id);

/**
 * Lookup a Polyobj in the map by tag.
 *
 * @param map  GameMap instance.
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* GameMap_PolyobjByTag(GameMap* map, int tag);

/**
 * Lookup a Polyobj in the map by origin.
 *
 * @param map  GameMap instance.
 * @param tag  Tag associated with the Polyobj to be found.
 * @return  Found Polyobj instance else @c NULL.
 */
polyobj_t* GameMap_PolyobjByOrigin(GameMap* map, void* ddMobjBase);

/**
 * Have the thinker lists been initialized yet?
 * @param map           GameMap instance.
 */
boolean GameMap_ThinkerListInited(GameMap* map);

/**
 * Init the thinker lists.
 *
 * @param map  GameMap instance.
 * @params flags  0x1 = Init public thinkers.
 *                0x2 = Init private (engine-internal) thinkers.
 */
void GameMap_InitThinkerLists(GameMap* map, byte flags);

/**
 * Iterate the list of thinkers making a callback for each.
 *
 * @param map  GameMap instance.
 * @param thinkFunc  If not @c NULL, only make a callback for thinkers
 *                   whose function matches this.
 * @param flags  Thinker filter flags.
 * @param callback  The callback to make. Iteration will continue
 *                  until a callback returns a non-zero value.
 * @param context  Passed to the callback function.
 */
int GameMap_IterateThinkers(GameMap* map, think_t thinkFunc, byte flags,
    int (*callback) (thinker_t* th, void*), void* context);

/**
 * @param map  GameMap instance.
 * @param thinker  Thinker to be added.
 * @param makePublic  @c true = @a thinker will be visible publically
 *                    via the Doomsday public API thinker interface(s).
 */
void GameMap_ThinkerAdd(GameMap* map, thinker_t* thinker, boolean makePublic);

/**
 * Deallocation is lazy -- it will not actually be freed until its
 * thinking turn comes up.
 *
 * @param map  GameMap instance.
 */
void GameMap_ThinkerRemove(GameMap* map, thinker_t* thinker);

/**
 * Locates a mobj by it's unique identifier in the map.
 * @param map  GameMap instance.
 */
struct mobj_s* GameMap_MobjByID(GameMap* map, int id);

/**
 * @param map  GameMap instance.
 */
boolean GameMap_IsUsedMobjID(GameMap* map, thid_t id);

/**
 * @param map  GameMap instance.
 */
void GameMap_SetMobjID(GameMap* map, thid_t id, boolean state);

/**
 * @param map  GameMap instance.
 */
void GameMap_InitClMobjs(GameMap* map);

/**
 * Called when the client is shut down. Unlinks everything from the
 * sectors and the blockmap and clears the clmobj list.
 */
void GameMap_DestroyClMobjs(GameMap* map);

/**
 * Deletes hidden, unpredictable or nulled mobjs for which we have not received
 * updates in a while.
 *
 * @param map  GameMap instance.
 */
void GameMap_ExpireClMobjs(GameMap* map);

/**
 * Reset the client status. To be called when the map changes.
 *
 * @param map  GameMap instance.
 */
void GameMap_ClMobjReset(GameMap* map);

/**
 * Iterate the client mobj hash, exec the callback on each. Abort if callback
 * returns @c false.
 *
 * @param map  GameMap instance.
 * @return  If the callback returns @c false.
 */
boolean GameMap_ClMobjIterator(GameMap* map, boolean (*callback) (struct mobj_s*, void*), void* context);

/**
 * Allocate a new client-side plane mover.
 *
 * @param map  GameMap instance.
 * @return  The new mover or @c NULL if arguments are invalid.
 */
struct clplane_s* GameMap_NewClPlane(GameMap* map, uint sectornum, clplanetype_t type,
    float dest, float speed);

/**
 * Retrieve a pointer to the Generators collection for this map.
 * If no collection has yet been constructed a new empty collection will be
 * initialized as a result of this call.
 *
 * @param map  GameMap instance.
 * @return  Generators collection for this map.
 */
struct generators_s* GameMap_Generators(GameMap* map);

/**
 * Retrieve a pointer to the decorated surface list for this map.
 *
 * @param map  GameMap instance.
 * @return  List of decorated surfaces.
 */
surfacelist_t* GameMap_DecoratedSurfaces(GameMap* map);

/**
 * Retrieve a pointer to the glowing surface list for this map.
 *
 * @param map  GameMap instance.
 * @return  List of glowing surfaces.
 */
surfacelist_t* GameMap_GlowingSurfaces(GameMap* map);

/**
 * Retrieve a pointer to the scrolling surface list for this map.
 *
 * @param map  GameMap instance.
 * @return  List of scrolling surfaces.
 */
surfacelist_t* GameMap_ScrollingSurfaces(GameMap* map);

/**
 * Retrieve a pointer to the tracked plane list for this map.
 *
 * @param map  GameMap instance.
 * @return  List of tracked planes.
 */
planelist_t* GameMap_TrackedPlanes(GameMap* map);

/**
 * Initialize all Polyobjs in the map. To be called after map load.
 *
 * @param map  GameMap instance.
 */
void GameMap_InitPolyobjs(GameMap* map);

/**
 * Initialize the node piles and link rings. To be called after map load.
 *
 * @param map  GameMap instance.
 */
void GameMap_InitNodePiles(GameMap* map);

/**
 * Construct an initial (empty) Mobj Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitMobjBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkMobjInBlockmap(GameMap* map, struct mobj_s* mo);
boolean GameMap_UnlinkMobjInBlockmap(GameMap* map, struct mobj_s* mo);

int GameMap_IterateCellMobjs(GameMap* map, const uint coords[2],
    int (*callback) (struct mobj_s*, void*), void* parameters);
int GameMap_IterateCellBlockMobjs(GameMap* map, const struct gridmapblock_s* blockCoords,
    int (*callback) (struct mobj_s*, void*), void* parameters);

int GameMap_MobjsBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (struct mobj_s*, void*), void* parameters);

/**
 * Construct an initial (empty) LineDef Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitLineDefBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkLineDefInBlockmap(GameMap* map, linedef_t* lineDef);

int GameMap_IterateCellLineDefs(GameMap* map, const uint coords[2],
    int (*callback) (linedef_t*, void*), void* parameters);
int GameMap_IterateCellBlockLineDefs(GameMap* map, const struct gridmapblock_s* blockCoords,
    int (*callback) (linedef_t*, void*), void* parameters);

int GameMap_LineDefIterator(GameMap* map, int (*callback) (linedef_t*, void*), void* parameters);

int GameMap_IterateCellPolyobjLineDefs(GameMap* map, const uint coords[2],
    int (*callback) (linedef_t*, void*), void* parameters);
int GameMap_IterateCellBlockPolyobjLineDefs(GameMap* map, const struct gridmapblock_s* blockCoords,
    int (*callback) (linedef_t*, void*), void* parameters);

int GameMap_LineDefsBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (linedef_t*, void*), void* parameters);

int GameMap_PolyobjLinesBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (linedef_t*, void*), void* parameters);

/**
 * LineDefs and Polyobj LineDefs (note Polyobj LineDefs are iterated first).
 *
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to GameMap_IterateCellLineDefs(), then make one or more calls to it.
 */
int GameMap_AllLineDefsBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (linedef_t*, void*), void* parameters);

/**
 * Construct an initial (empty) BspLeaf Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitBspLeafBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkBspLeafInBlockmap(GameMap* map, BspLeaf* bspLeaf);

int GameMap_IterateCellBspLeafs(GameMap* map, const uint coords[2],
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* parameters);
int GameMap_IterateCellBlockBspLeafs(GameMap* map, const struct gridmapblock_s* blockCoords,
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* parameters);

int GameMap_BspLeafsBoxIterator(GameMap* map, const AABoxf* box, sector_t* sector,
    int (*callback) (BspLeaf*, void*), void* parameters);

int GameMap_BspLeafIterator(GameMap* map, int (*callback) (BspLeaf*, void*), void* parameters);

/**
 * Construct an initial (empty) Polyobj Blockmap for this map.
 *
 * @param min  Minimal coordinates for the map.
 * @param max  Maximal coordinates for the map.
 */
void GameMap_InitPolyobjBlockmap(GameMap* map, const_pvec2_t min, const_pvec2_t max);

void GameMap_LinkPolyobjInBlockmap(GameMap* map, polyobj_t* po);
void  GameMap_UnlinkPolyobjInBlockmap(GameMap* map, polyobj_t* po);

int GameMap_IterateCellPolyobjs(GameMap* map, const uint coords[2],
    int (*callback) (polyobj_t*, void*), void* parameters);
int GameMap_IterateCellBlockPolyobjs(GameMap* map, const struct gridmapblock_s* blockCoords,
    int (*callback) (polyobj_t*, void*), void* parameters);

/**
 * The validCount flags are used to avoid checking polys that are marked in
 * multiple mapblocks, so increment validCount before the first call, then
 * make one or more calls to it.
 */
int GameMap_PolyobjsBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (struct polyobj_s*, void*), void* parameters);

int GameMap_PolyobjIterator(GameMap* map, int (*callback) (polyobj_t*, void*), void* parameters);

int GameMap_VertexIterator(GameMap* map, int (*callback) (vertex_t*, void*), void* parameters);

int GameMap_SideDefIterator(GameMap* map, int (*callback) (sidedef_t*, void*), void* parameters);

int GameMap_SectorIterator(GameMap* map, int (*callback) (sector_t*, void*), void* parameters);

int GameMap_HEdgeIterator(GameMap* map, int (*callback) (HEdge*, void*), void* parameters);

int GameMap_BspNodeIterator(GameMap* map, int (*callback) (BspNode*, void*), void* parameters);

/**
 * Traces a line between @a from and @a to, making a callback for each
 * interceptable object linked within Blockmap cells which cover the path this
 * defines.
 */
int GameMap_PathTraverse2(GameMap* map, float const from[2], float const to[2],
    int flags, traverser_t callback, void* parameters);
int GameMap_PathTraverse(GameMap* map, float const from[2], float const to[2],
    int flags, traverser_t callback/* void* parameters=NULL*/);

int GameMap_PathXYTraverse2(GameMap* map, float fromX, float fromY, float toX, float toY,
    int flags, traverser_t callback, void* parameters);
int GameMap_PathXYTraverse(GameMap* map, float fromX, float fromY, float toX, float toY,
    int flags, traverser_t callback);

/**
 * Determine the BSP leaf on the back side of the BS partition that lies in front
 * of the specified point within the map's coordinate space.
 *
 * @note Always returns a valid BspLeaf although the point may not actually lay
 *       within it (however it is on the same side of the space partition)!
 *
 * @param map  GameMap instance.
 * @param x  X coordinate of the point to test.
 * @param y  Y coordinate of the point to test.
 * @return  BspLeaf instance for that BSP node's leaf.
 */
BspLeaf* GameMap_BspLeafAtPoint(GameMap* map, float point[2]);
BspLeaf* GameMap_BspLeafAtPointXY(GameMap* map, float x, float y);

#endif /// LIBDENG_GAMEMAP_H
