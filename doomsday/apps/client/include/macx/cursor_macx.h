/** @file cursor_macx.h  Native OS X mouse cursor functions.
 * @ingroup ui
 *
 * @authors Copyright © 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_MACX_CURSOR_H
#define CLIENT_MACX_CURSOR_H

/**
 * Show or hide the mouse cursor.
 *
 * @param show  @c true to show, @c false to hide.
 */
void Cursor_Show(bool show);

#endif // CLIENT_MACX_CURSOR_H