/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * r_data.h: Data Structures For Refresh
 */

#ifndef __DOOMSDAY_REFRESH_DATA_H__
#define __DOOMSDAY_REFRESH_DATA_H__

#include "gl_main.h"
#include "dd_def.h"
#include "p_think.h"
#include "m_nodepile.h"
#include "def_data.h"
#include "r_extres.h"

// Flags for decorations.
#define DCRF_NO_IWAD    0x1         // Don't use if from IWAD.
#define DCRF_PWAD       0x2         // Can use if from PWAD.
#define DCRF_EXTERNAL   0x4         // Can use if from external resource.

// Detail texture instance.
typedef struct detailtexinst_s {
    DGLuint         tex;
    float           contrast;

    struct detailtexinst_s* next;
} detailtexinst_t;

typedef struct {
    int             width, height;
    float           strength;
    float           scale;
    float           maxDist;
    lumpnum_t       lump;
    const char*     external;
    // Linked list of detail texture instances.
    // A unique texture is generated for each (rounded) contrast level.
    detailtexinst_t* instances;
} detailtex_t;

typedef struct gltexture_s {
    DGLuint         id;
    int             magMode;
    float           width, height;
    boolean         masked;
    struct {
        DGLuint         id;
        int             width, height;
        float           scale;
    } detail;
} gltexture_t;

typedef struct glcommand_vertex_s {
    float           s, t;
    int             index;
} glcommand_vertex_t;

#define RL_MAX_DIVS         64

// Rendpoly flags.
#define RPF_MASKED      0x0001     // Use the special list for masked textures.
#define RPF_SKY_MASK    0x0004     // A sky mask polygon.
#define RPF_LIGHT       0x0008     // A dynamic light.
#define RPF_DYNLIT      0x0010     // Normal list: poly is dynamically lit.
#define RPF_GLOW        0x0020     // Multiply original vtx colors.
#define RPF_DETAIL      0x0040     // Render with detail (incl. vtx distances)
#define RPF_SHADOW      0x0100
#define RPF_HORIZONTAL  0x0200
#define RPF_SHINY       0x0400     // Shiny surface.
#define RPF_DONE        0x8000     // This poly has already been drawn.

typedef enum {
    RP_NONE,
    RP_QUAD,                       // Wall segment.
    RP_DIVQUAD,                    // Divided wall segment.
    RP_FLAT                        // Floor or ceiling.
} rendpolytype_t;

typedef struct rvertex_s {
    float           pos[3];
} rvertex_t;

typedef struct rcolor_s {
    float           rgba[4];
} rcolor_t;

typedef struct walldiv_s {
    unsigned int    num;
    float           pos[RL_MAX_DIVS];
} walldiv_t;

typedef struct rendpoly_wall_s {
    float           length;
    walldiv_t       divs[2]; // For wall segments (two vertices).
} rendpoly_wall_t;

// rendpoly_params_t is only for convenience; the data written in the rendering
// list data buffer is taken from this struct.
typedef struct rendpoly_params_s {
    boolean         isWall;
    rendpolytype_t  type;
    short           flags; // RPF_*.
    float           texOrigin[2][3]; // Used in texture coordinate calculation.
    float           texOffset[2]; // Texture coordinates for left/top
                                  // (in real texcoords).
    gltexture_t     tex;
    gltexture_t     interTex;
    float           interPos; // Blending strength (0..1).
    uint            lightListIdx; // List of lights that affect this poly.
    blendmode_t     blendMode; // Primitive-specific blending mode.
    rendpoly_wall_t* wall; // Wall specific data if any.
} rendpoly_params_t;

// This is the dummy mobj_t used for blockring roots.
// It has some excess information since it has to be compatible with
// regular mobjs (otherwise the rings don't really work).
// Note: the thinker and pos data could be used for something else...
typedef struct linkmobj_s {
    thinker_t       thinker;
    float           pos[3];
    struct mobj_s*  bNext, *bPrev;
} linkmobj_t;

typedef struct shadowlink_s {
    struct shadowlink_s* next;
    linedef_t*      lineDef;
    byte            side;
} shadowlink_t;

typedef struct subplaneinfo_s {
    vertexillum_t*  illum;
    biastracker_t   tracker;
    uint            updated;
    biasaffection_t affected[MAX_BIAS_AFFECTED];
} subplaneinfo_t;

typedef float       colorcomp_t;
typedef colorcomp_t rgbcol_t[3];
typedef colorcomp_t rgbacol_t[4];

typedef struct {
    lumpnum_t       lump;
    short           offX; // block origin (allways UL), which has allready
    short           offY; // accounted for the patch's internal origin
} texpatch_t;

// Describes a rectangular texture, which is composed of one
// or more texpatch_t structures that arrange graphic patches.
typedef struct {
    char            name[9];
    short           width, height;
    short           patchCount;
    texpatch_t      patches[1]; // [patchcount] drawn back to front into the cached texture.
} texturedef_t;

typedef struct flat_s {
    lumpnum_t       lump;
} flat_t;

typedef struct {
    lumpnum_t       lump; // Real lump number.
    short           width, height, offX, offY;
    float           flareX, flareY, lumSize;
    rgbcol_t        autoLightColor;
    float           texCoord[2]; // Prepared texture coordinates.
    DGLuint         pspriteTex;
} spritetex_t;

// A translated sprite.
typedef struct {
    int             patch;
    DGLuint         tex;
    unsigned char*  table;
    float           flareX, flareY, lumSize;
    rgbcol_t        autoLightColor;
} transspr_t;

// Model skin.
typedef struct {
    char            path[256];
    DGLuint         tex;
} skintex_t;

// Patch flags.
#define PF_MONOCHROME         0x1
#define PF_UPSCALE_AND_SHARPEN 0x2

// A patchtex is a lumppatch that has been prepared for render.
typedef struct patchtex_s {
    lumpnum_t       lump;
    short           offX, offY;
    short           extraOffset[2]; // Only used with upscaled and sharpened patches.
    int             flags; // Possible modifier filters to apply (monochrome, scale+sharp)

    // Part 1
    DGLuint         tex; // Name of the associated DGL texture.
    short           width, height;

    // Part 2 (only used with textures larger than the max texture size).
    DGLuint         tex2;
    short           width2, height2;

    struct patchtex_s* next;
} patchtex_t;

// A rawtex is a lump raw graphic that has been prepared for render.
typedef struct rawtex_s {
    lumpnum_t       lump;

    // Part 1
    DGLuint         tex; // Name of the associated DGL texture.
    short           width, height;
    byte            masked;

    // Part 2 (only used with textures larger than the max texture size).
    DGLuint         tex2;
    short           width2, height2;
    byte            masked2;

    struct rawtex_s* next;
} rawtex_t;

typedef struct {
    float           approxDist; // Only an approximation.
    float           vector[3]; // Light direction vector.
    float           color[3]; // How intense the light is (0..1, RGB).
    float           offset;
    float           lightSide, darkSide; // Factors for world light.
    boolean         affectedByAmbient;
} vlight_t;

/**
 * Textures used in the lighting system.
 */
typedef enum lightingtexid_e {
    LST_DYNAMIC, // Round dynamic light
    LST_GRADIENT, // Top-down gradient
    LST_RADIO_CO, // FakeRadio closed/open corner shadow
    LST_RADIO_CC, // FakeRadio closed/closed corner shadow
    LST_RADIO_OO, // FakeRadio open/open shadow
    LST_RADIO_OE, // FakeRadio open/edge shadow
    NUM_LIGHTING_TEXTURES
} lightingtexid_t;

typedef enum flaretexid_e {
    FXT_FLARE, // (flare)
    FXT_BRFLARE, // (brFlare)
    FXT_BIGFLARE, // (bigFlare)
    NUM_FLARE_TEXTURES
} flaretexid_t;

/**
 * Textures used in world rendering.
 * eg a surface with a missing tex/flat is drawn using the "missing" graphic
 */
typedef enum ddtextureid_e {
    DDT_UNKNOWN, // Drawn if a texture/flat is unknown
    DDT_MISSING, // Drawn in place of HOMs in dev mode.
    DDT_BBOX, // Drawn when rendering bounding boxes
    DDT_GRAY, // For lighting debug.
    NUM_DD_TEXTURES
} ddtextureid_t;

typedef struct {
    DGLuint         tex;
} ddtexture_t;

extern nodeindex_t* linelinks;
extern blockmap_t* BlockMap;
extern blockmap_t* SSecBlockMap;
extern linkmobj_t* blockrings;
extern byte* rejectMatrix; // For fast sight rejection.
extern nodepile_t* mobjNodes, *lineNodes;

extern int viewwidth, viewheight;
extern int levelFullBright;
extern int glowingTextures;
extern byte precacheSprites, precacheSkins;

extern int numFlats;
extern flat_t** flats;

extern spritetex_t** spriteTextures;
extern int numSpriteTextures;

extern detailtex_t** detailTextures;
extern int numDetailTextures;

void            R_InitRendVerticesPool(void);
rvertex_t*      R_AllocRendVertices(uint num);
rcolor_t*       R_AllocRendColors(uint num);
void            R_FreeRendVertices(rvertex_t* rvertices);
void            R_FreeRendColors(rcolor_t* rcolors);
void            R_InfoRendVerticesPool(void);

void            R_InitData(void);
void            R_UpdateData(void);
void            R_ShutdownData(void);

void            R_UpdateSector(struct sector_s* sec, boolean forceUpdate);

void            R_PrecacheLevel(void);
void            R_PrecachePatch(lumpnum_t lump);

texturedef_t*   R_GetTextureDef(int num);
boolean         R_TextureIsFromIWAD(int num);

int             R_NewSpriteTexture(lumpnum_t lump, struct material_s** mat);

int             R_GetSkinTexIndex(const char* skin);
skintex_t*      R_GetSkinTexByIndex(int id);
int             R_RegisterSkin(char* skin, const char* modelfn, char* fullpath);
void            R_DeleteSkinTextures(void);
void            R_DestroySkins(void); // Called at shutdown.

void            R_InitAnimGroup(ded_group_t* def);

void            R_CreateDetailTexture(ded_detailtexture_t* def);
detailtex_t*    R_GetDetailTexture(lumpnum_t lump, const char* external);
void            R_DeleteDetailTextures(void);
void            R_DestroyDetailTextures(void); // Called at shutdown.

patchtex_t*     R_FindPatchTex(lumpnum_t lump); // May return NULL.
patchtex_t*     R_GetPatchTex(lumpnum_t lump); // Creates new entries.
patchtex_t**    R_CollectPatchTexs(int* count);

rawtex_t*       R_FindRawTex(lumpnum_t lump); // May return NULL.
rawtex_t*       R_GetRawTex(lumpnum_t lump); // Creates new entries.
rawtex_t**      R_CollectRawTexs(int* count);

boolean         R_IsAllowedDecoration(ded_decor_t* def, struct material_s* mat,
                                      boolean hasExternal);
boolean         R_IsValidLightDecoration(const ded_decorlight_t* lightDef);

#endif
