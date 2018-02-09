/** @file globalshortcuts.cpp  Global keyboard shortkeys.
 *
 * @authors Copyright (c) 2014-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "globalshortcuts.h"
#include "gloomapp.h"

#include <de/KeyEvent>

using namespace de;

DENG2_PIMPL_NOREF(GlobalShortcuts)
{};

GlobalShortcuts::GlobalShortcuts()
    : Widget("shortcuts"), d(new Impl)
{}

bool GlobalShortcuts::handleEvent(Event const &event)
{
    if (event.isKeyDown())
    {
        KeyEvent const &key = event.as<KeyEvent>();
        if (key.modifiers().testFlag(KeyEvent::Control) && key.qtKey() == Qt::Key_Q)
        {
            GloomApp::app().quit();
            return true;
        }
    }
    return false;
}
