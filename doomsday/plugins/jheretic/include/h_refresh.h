/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008 Daniel Swanson <danij@dengine.net>
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
 * h_refresh.h:
 */

#ifndef __H_REFRESH_H__
#define __H_REFRESH_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "r_common.h" // For dpatch_t

#include "p_mobj.h"

void            R_InitRefresh(void);
void            H_Display(void);
void            H_Display2(void);

void            R_SetViewSize(int blocks, int detail);

void            R_DrawSpecialFilter(void);
void            R_DrawLevelTitle(int x, int y, float alpha,
                                 dpatch_t *font, boolean center);

void            P_SetDoomsdayFlags(mobj_t *mo);
void            R_SetAllDoomsdayFlags(void);

#endif
