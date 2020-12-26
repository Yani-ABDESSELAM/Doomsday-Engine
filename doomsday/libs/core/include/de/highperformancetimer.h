/** @file highperformancetimer.h  Timer for performance-critical use.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_HIGHPERFORMANCETIMER_H
#define LIBCORE_HIGHPERFORMANCETIMER_H

#include "de/time.h"

namespace de {

/**
 * Timer for high-performance use.
 * @ingroup core
 */
class DE_PUBLIC HighPerformanceTimer
{
public:
    HighPerformanceTimer();

    /**
     * Returns the amount of time elapsed since the creation of the timer.
     */
    TimeSpan elapsed() const;

    /**
     * Returns the time when the timer was started.
     */
    Time startedAt() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_HIGHPERFORMANCETIMER_H