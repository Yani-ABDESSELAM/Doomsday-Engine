/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_EXTRES_H
#define LIBDENG_FILESYS_EXTRES_H

#include "m_string.h"
#include "m_filehash.h"

typedef uint resourcenamespaceid_t;

typedef struct {
    /// Unique symbolic name of this namespace (e.g., "packages:").
    const char* _name;
 
    /// Command line options for setting the resource path explicitly. Flag2 Takes precendence.
    const char* _overrideFlag, *_overrideFlag2;

    /// Resource search order (in order of greatest-importance, right to left) seperated by semicolon (e.g., "path1;path2;").
    const char* _searchPathTemplate;

    /// Auto-inited:
    ddstring_t _searchPathList;
    filehash_t* _fileHash;
} resourcenamespace_t;

/**
 * Convert a resourceclass_t constant into a string for error/debug messages.
 */
const char* F_ResourceClassStr(resourceclass_t rclass);

/**
 * \post Initial/default search paths registered and queries may begin.
 */
void F_InitResourceLocator(void);
void F_ShutdownResourceLocator(void);

/**
 * @return             @c true iff the value can be interpreted as a valid id.
 */
boolean F_IsValidResourceNamespaceId(int val);

/**
 * Given an id return the associated namespace object.
 */
resourcenamespace_t* F_ToResourceNamespace(resourcenamespaceid_t rni);

/**
 * @return             Number of resource namespaces.
 */
uint F_NumResourceNamespaces(void);

resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass);
resourcenamespaceid_t F_ResourceNamespaceForName(const char* name);
resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name);

/**
 * Add a new file path to the list of resource-locator search paths.
 */
boolean F_AddResourceSearchPath(resourceclass_t rclass, const char* newPath, boolean append);

/**
 * Clear resource-locator search paths for all namespaces.
 */
void F_ClearResourceSearchPaths(void);

/**
 * Clear resource-locator search paths for a specific resource namespace.
 */
void F_ClearResourceSearchPaths2(resourceclass_t rclass);

/**
 * @return              Ptr to a string containing the resource search path list.
 */
const ddstring_t* F_ResourceSearchPaths(resourceclass_t rclass);

/**
 * Attempt to locate an external file for the specified resource.
 *
 * @param rclass        Class of resource being searched for (if known).
 *
 * @param foundPath     If found, the fully qualified path will be written back here.
 *                      Can be @c NULL, changing this routine to only check that file
 *                      exists on the physical file system and can be read.
 *
 * @param searchPath    Path/name of the resource being searched for. Note that
 *                      the resource @a type specified significantly alters search
 *                      behavior. This allows text replacements of symbolic escape
 *                      sequences in the path, allowing access to the engine's view
 *                      of the virtual file systmem.
 *
 * @param suffix        Optional name suffix. If not @c NULL, append to @p name
 *                      and look for matches. If not found or not specified then
 *                      search for matches to @p name.
 *
 * @param foundPathLen  Size of @p foundPath in bytes.
 *
 * @return              @c true, iff a file was found.
 */
boolean F_FindResource(resourceclass_t rclass, char* foundPath, const char* searchPath,
    const char* suffix, size_t foundPathLength);

#endif /* LIBDENG_FILESYS_EXTRES_H */
