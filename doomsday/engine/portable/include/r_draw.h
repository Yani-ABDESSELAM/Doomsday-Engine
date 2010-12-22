/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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
 * Misc drawing routines.
 */

#ifndef LIBDENG_REFRESH_DRAW_H
#define LIBDENG_REFRESH_DRAW_H

extern byte* translationTables;

void            R_InitTranslationTables(void);
void            R_UpdateTranslationTables(void);
void            R_InitViewBorder(void);
void            R_SetBorderGfx(char* const* gfx);
void            R_DrawViewBorder(void);

void            R_DrawPatch(patchtex_t* p, int x, int y);
void            R_DrawPatch2(patchtex_t* p, int x, int y, int w, int h);
void            R_DrawPatch3(patchtex_t* p, int x, int y, int w, int h, boolean useOffsets);

void            R_DrawPatchTiled(patchtex_t* p, int x, int y, int w, int h, DGLint wrapS, DGLint wrapT);

#endif /* LIBDENG_REFRESH_DRAW_H */
