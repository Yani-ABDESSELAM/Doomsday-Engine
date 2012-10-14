/**
 * @file library.h
 * Dynamic libraries. @ingroup base
 *
 * These functions provide roughly the same functionality as the ltdl library.
 * Since the ltdl library appears to be broken on Mac OS X, these will be used
 * instead when loading plugin libraries.
 *
 * During startup Doomsday loads multiple game plugins. However, only one can
 * exist in memory at a time because they contain many of the same globally
 * visible symbols. When a game is started, all game plugins are first released
 * from memory after which the chosen game plugin is reloaded (see
 * Library_ReleaseGames()).
 *
 * @todo Implement and use this class for Windows.
 *
 * @authors Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SYSTEM_UTILS_DYNAMIC_LIBRARY_H
#define LIBDENG_SYSTEM_UTILS_DYNAMIC_LIBRARY_H

#ifdef __cplusplus
extern "C" {
#endif

struct library_s; // The library instance (opaque).
typedef struct library_s Library;

/**
 * Initializes the library loader.
 */
void Library_Init(void);

/**
 * Release all resources associated with dynamic libraries. Must be called
 * when shutting down the engine.
 */
void Library_Shutdown(void);

/**
 * Closes the library handles of all game plugins. The library will be
 * reopened automatically when needed.
 */
void Library_ReleaseGames(void);

/**
 * Looks for dynamic libraries and calls @a func for each one.
 *
 * @return If all available libraries were iterated, returns 0. If @a func
 * returns a non-zero value to indicate aborting the iteration at some point,
 * that value is returned instead.
 */
int Library_IterateAvailableLibraries(int (*func)(const char* fileName, void* data), void* data);

/**
 * Loads a dynamic library.
 *
 * @param fileName  Name of the library to open.
 */
Library* Library_New(const char* fileName);

void Library_Delete(Library* lib);

/**
 * Looks up a symbol from the library.
 *
 * @param symbolName  Name of the symbol.
 *
 * @return @c NULL if the symbol is not defined. Otherwise the address of
 * the symbol.
 */
void* Library_Symbol(Library* lib, const char* symbolName);

/**
 * Returns the latest error message.
 */
const char* Library_LastError(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBDENG_SYSTEM_UTILS_DYNAMIC_LIBRARY_H */
