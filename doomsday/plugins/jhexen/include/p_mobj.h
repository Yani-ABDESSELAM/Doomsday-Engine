/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * p_mobj.h: Map Objects, MObj, definition and handling.
 */

#ifndef __P_MOBJ_H__
#define __P_MOBJ_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define MTF_EASY        1
#define MTF_NORMAL      2
#define MTF_HARD        4
#define MTF_AMBUSH      8
#define MTF_DORMANT     16
#define MTF_FIGHTER     32
#define MTF_CLERIC      64
#define MTF_MAGE        128
#define MTF_GSINGLE     256
#define MTF_GCOOP       512
#define MTF_GDEATHMATCH 1024

typedef struct {
    short           tid;
    float           pos[2];
    float           height;
    angle_t         angle;
    int             type;
    int             flags;
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
} spawnspot_t;

/**
 * Mobj flags
 *
 * \attention IMPORTANT - Keep this current!!!
 * LEGEND:
 * p    = Flag is persistent (never changes in-game).
 * i    = Internal use (not to be used in defintions).
 *
 * \todo Persistent flags (p) don't need to be included in save games or sent to
 * clients in netgames. We should collect those in to a const flags setting which
 * is set only once when the mobj is spawned.
 *
 * \todo All flags for internal use only (i) should be put in another var and the
 * flags removed from those defined in GAME/objects.DED
 */

// --- mobj.flags ---

#define MF_SPECIAL      1          // call P_SpecialThing when touched
#define MF_SOLID        2
#define MF_SHOOTABLE    4
#define MF_NOSECTOR     8          // don't use the sector links
                                   // (invisible but touchable)
#define MF_NOBLOCKMAP   16         // don't use the blocklinks
                                   // (inert but displayable)
#define MF_AMBUSH       32
#define MF_JUSTHIT      64         // try to attack right back
#define MF_JUSTATTACKED 128        // take at least one step before attacking
#define MF_SPAWNCEILING 256        // hang from ceiling instead of floor
#define MF_NOGRAVITY    512        // don't apply gravity every tic

// movement flags
#define MF_DROPOFF      0x400      // allow jumps from high places
#define MF_PICKUP       0x800      // for players to pick up items
#define MF_NOCLIP       0x1000     // player cheat
#define MF_SLIDE        0x2000     // keep info about sliding along walls
#define MF_FLOAT        0x4000     // allow moves to any height, no gravity
#define MF_TELEPORT     0x8000     // don't cross lines or look at heights
#define MF_MISSILE      0x10000    // don't hit same species, explode on block

#define MF_ALTSHADOW    0x20000    // alternate fuzzy draw
#define MF_SHADOW       0x40000    // use fuzzy draw (shadow demons / invis)
#define MF_NOBLOOD      0x80000    // don't bleed when shot (use puff)
#define MF_CORPSE       0x100000   // don't stop moving halfway off a step
#define MF_INFLOAT      0x200000   // floating to a height for a move, don't
                                   // auto float to target's height

#define MF_COUNTKILL    0x400000   // count towards intermission kill total
#define MF_ICECORPSE    0x800000   // a frozen corpse (for blasting)

#define MF_SKULLFLY     0x1000000  // skull in flight
#define MF_NOTDMATCH    0x2000000  // don't spawn in death match (key cards)

//#define   MF_TRANSLATION  0xc000000   // if 0x4 0x8 or 0xc, use a translation
#define MF_TRANSLATION  0x1c000000 // use a translation table (>>MF_TRANSHIFT)
#define MF_TRANSSHIFT   26         // table for player colormaps

#define MF_LOCAL            0x20000000

// If this flag is set, the sprite is aligned with the view plane.
#define MF_BRIGHTEXPLODE    0x40000000  // Make this brightshadow when exploding.
#define MF_VIEWALIGN        0x80000000
#define MF_BRIGHTSHADOW     (MF_SHADOW|MF_ALTSHADOW)

// --- mobj.flags2 ---

#define MF2_LOGRAV          0x00000001  // alternate gravity setting
#define MF2_WINDTHRUST      0x00000002  // gets pushed around by the wind
                                        // specials
#define MF2_FLOORBOUNCE     0x00000004  // bounces off the floor
#define MF2_BLASTED         0x00000008  // missile will pass through ghosts
#define MF2_FLY             0x00000010  // fly mode is active
#define MF2_FLOORCLIP       0x00000020  // if feet are allowed to be clipped
#define MF2_SPAWNFLOAT      0x00000040  // spawn random float z
#define MF2_NOTELEPORT      0x00000080  // does not teleport
#define MF2_RIP             0x00000100  // missile rips through solid
                                        // targets
#define MF2_PUSHABLE        0x00000200  // can be pushed by other moving
                                        // mobjs
#define MF2_SLIDE           0x00000400  // slides against walls
#define MF2_ONMOBJ          0x00000800  // mobj is resting on top of another
                                        // mobj
#define MF2_PASSMOBJ        0x00001000  // Enable z block checking.  If on,
                                        // this flag will allow the mobj to
                                        // pass over/under other mobjs.
#define MF2_CANNOTPUSH      0x00002000  // cannot push other pushable mobjs
#define MF2_DROPPED         0x00004000  // dropped by a demon
#define MF2_BOSS            0x00008000  // mobj is a major boss
#define MF2_FIREDAMAGE      0x00010000  // does fire damage
#define MF2_NODMGTHRUST     0x00020000  // does not thrust target when
                                        // damaging
#define MF2_TELESTOMP       0x00040000  // mobj can stomp another
#define MF2_FLOATBOB        0x00080000  // use float bobbing z movement
#define MF2_DONTDRAW        0x00100000  // don't generate a vissprite
#define MF2_IMPACT          0x00200000  // an MF_MISSILE mobj can activate
                                        // SPAC_IMPACT
#define MF2_PUSHWALL        0x00400000  // mobj can push walls
#define MF2_MCROSS          0x00800000  // can activate monster cross lines
#define MF2_PCROSS          0x01000000  // can activate projectile cross lines
#define MF2_CANTLEAVEFLOORPIC 0x02000000    // stay within a certain floor type
#define MF2_NONSHOOTABLE    0x04000000  // mobj is totally non-shootable,
                                        // but still considered solid
#define MF2_INVULNERABLE    0x08000000  // mobj is invulnerable
#define MF2_DORMANT         0x10000000  // thing is dormant
#define MF2_ICEDAMAGE       0x20000000  // does ice damage
#define MF2_SEEKERMISSILE   0x40000000  // is a seeker (for reflection)
#define MF2_REFLECTIVE      0x80000000  // reflects missiles

// --- mobj.flags3 ---

#define MF3_NOINFIGHT       0x00000001  // Mobj will never be targeted for in-fighting

typedef struct mobj_s {
    // Defined in dd_share.h; required mobj elements.
    DD_BASE_MOBJ_ELEMENTS()
        // Hexen-specific data:
    struct player_s *player;       // only valid if type == MT_PLAYER

    int             floorPic;      // contacted sec floorpic
    mobjinfo_t     *info;          // &mobjInfo[mobj->type]
    int             damage;        // For missiles
    int             flags;
    int             flags2;        // Heretic flags
    int             flags3;
    int             special1;      // Special info
    int             special2;      // Special info
    int             health;
    int             moveDir;       // 0-7
    int             moveCount;     // when 0, select a new dir
    struct mobj_s  *target;        // thing being chased/attacked (or NULL)
    // also the originator for missiles
    // used by player to freeze a bit after
    // teleporting
    int             threshold;     // if > 0, the target will be chased
    // no matter what (even if shot)
    int             lastLook;      // player number last looked for
    short           tid;           // thing identifier
    byte            special;       // special
    byte            args[5];       // special arguments
    int             turnTime;      // $visangle-facetarget
    int             alpha;         // $mobjalpha

    // Thing being chased/attacked for tracers.
    struct mobj_s  *tracer;

    // Used by lightning zap
    struct mobj_s  *lastEnemy;
} mobj_t;

extern spawnspot_t* things;

void            P_ExplodeMissile(mobj_t *mo);

#endif
