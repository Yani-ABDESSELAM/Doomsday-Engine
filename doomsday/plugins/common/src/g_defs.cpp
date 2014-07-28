/** @file g_defs.cpp  Game definition lookup utilities.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "g_common.h"
#include "g_defs.h"

using namespace de;

ded_t &Defs()
{
    return *static_cast<ded_t *>(DD_GetVariable(DD_DEFS));
}

int GetDefInt(char const *def, int *returnVal)
{
    // Get the value.
    char *data;
    if(Def_Get(DD_DEF_VALUE, def, &data) < 0)
        return 0; // No such value...

    // Convert to integer.
    int val = strtol(data, 0, 0);
    if(returnVal) *returnVal = val;

    return val;
}

void GetDefState(char const *def, int *val)
{
    // Get the value.
    char *data;
    if(Def_Get(DD_DEF_VALUE, def, &data) < 0)
        return;

    // Get the state number.
    *val = Def_Get(DD_DEF_STATE, data, 0);
    if(*val < 0) *val = 0;
}

Record *P_EpisodeInfo(String id)
{
    return hexDefs.getEpisodeInfo(id);
}

Record *P_MapInfo(de::Uri const &mapUri)
{
    return hexDefs.getMapInfo(mapUri);
}

/// @todo fixme: What about the episode?
de::Uri P_TranslateMap(uint map)
{
    return hexDefs.translateMapWarpNumber(map);
}
