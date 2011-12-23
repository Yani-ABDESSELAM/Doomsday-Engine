/**\file p_objlink.h
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

/**
 * p_objlink.c: Objlink management.
 *
 * Object => Subsector contacts and object => Subsector spreading.
 */

#ifndef LIBDENG_OBJLINK_BLOCKMAP_H
#define LIBDENG_OBJLINK_BLOCKMAP_H

typedef enum {
    OT_MOBJ,
    OT_LUMOBJ,
    NUM_OBJ_TYPES
} objtype_t;

#define VALID_OBJTYPE(val) ((val) >= OT_MOBJ && (val) < NUM_OBJ_TYPES)

/**
 * Construct the objlink blockmap for the current map.
 */
void R_InitObjlinkBlockmapForMap(void);

/**
 * Initialize the object => Subsector contact lists, ready for linking to
 * objects. To be called at the beginning of a new world frame.
 */
void R_InitForNewFrame(void);

/**
 * To be called at the begining of a render frame to clear the objlink
 * blockmap prior to linking objects for the new viewer.
 */
void R_ClearObjlinksForFrame(void);

/**
 * Create a new object link of the specified @a type in the objlink blockmap.
 */
void R_ObjlinkCreate(void* object, objtype_t type);

/**
 * To be called at the beginning of a render frame to link all objects
 * into the objlink blockmap.
 */
void R_LinkObjs(void);

/**
 * Spread object => Subsector links for the given @a subsector. Note that
 * all object types will be spread at this time.
 */
void R_InitForSubsector(subsector_t* subsector);

typedef struct {
    void* obj;
    objtype_t type;
} linkobjtossecparams_t;

/**
 * Create a new object => Subsector contact in the objlink blockmap.
 * Can be used as an iterator.
 *
 * @params paramaters  @see linkobjtossecparams_t
 * @return  @c true (always).
 */
boolean RIT_LinkObjToSubsector(subsector_t* subsector, void* paramaters);

/**
 * Traverse the list of objects of the specified @a type which have been linked
 * with @a subsector for the current render frame.
 */
boolean R_IterateSubsectorContacts(subsector_t* subsector, objtype_t type,
    boolean (*func) (void* object, void* paramaters), void* paramaters);

#endif /* LIBDENG_OBJLINK_BLOCKMAP_H */
