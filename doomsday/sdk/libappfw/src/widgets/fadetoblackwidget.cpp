/** @file fadetoblackwidget.cpp  Fade to/from black.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/FadeToBlackWidget"

namespace de {

DENG2_PIMPL_NOREF(FadeToBlackWidget)
{
    TimeDelta span = 1;
};

FadeToBlackWidget::FadeToBlackWidget() : d(new Instance)
{
    set(Background(Vector4f(0, 0, 0, 1)));
}

void FadeToBlackWidget::initFadeFromBlack(TimeDelta const &span)
{
    setOpacity(1);
    d->span = span;
}

void FadeToBlackWidget::start()
{
    setOpacity(0, d->span);
}

void FadeToBlackWidget::cancel()
{
    setOpacity(0);
}

bool FadeToBlackWidget::isDone() const
{
    return opacity().done();
}

void FadeToBlackWidget::disposeIfDone()
{
    if(isDone())
    {
        destroyLater(this);
    }
}

} // namespace de