/** @file textualsliderwidget.h  UI widget for a textual slider.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_UI_TEXTUALSLIDERWIDGET
#define LIBCOMMON_UI_TEXTUALSLIDERWIDGET

#include "sliderwidget.h"

namespace common {
namespace menu {

struct TextualSliderWidget : public SliderWidget
{
public:
    TextualSliderWidget();
    virtual ~TextualSliderWidget() {}

    void draw(Point2Raw const *origin);
    void updateGeometry(Page *pagePtr);
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_TEXTUALSLIDERWIDGET