/**
 * @file displaymode.cpp
 * Platform-independent display mode management. @ingroup gl
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "displaymode.h"

#ifdef MACOSX
#  include "displaymode_macx.h"
#endif

#include <vector>
#include <set>
#include <algorithm>

#include <qDebug>

static bool inited = false;

struct Mode : public DisplayMode
{
    Mode()
    {
        memset(static_cast<DisplayMode*>(this), 0, sizeof(DisplayMode));
    }

    Mode(const DisplayMode& dm)
    {
        memcpy(static_cast<DisplayMode*>(this), &dm, sizeof(dm));
    }

    Mode(int i)
    {
        DisplayMode_Native_GetMode(i, this);
        updateRatio();
    }

    static Mode fromCurrent()
    {
        Mode m;
        DisplayMode_Native_GetCurrentMode(&m);
        m.updateRatio();
        return m;
    }

    bool operator == (const Mode& other) const
    {
        return width == other.width && height == other.height &&
               depth == other.depth && refreshRate == other.refreshRate;
    }

    bool operator < (const Mode& b) const
    {
        if(height == b.height)
        {
            if(width == b.width)
            {
                if(depth == b.depth)
                {
                    return refreshRate < b.refreshRate;
                }
                return depth < b.depth;
            }
            return width < b.width;
        }
        return height < b.height;
    }

    void updateRatio()
    {
        ratioX = width;
        ratioY = height;

        // Reduce until we must resort to fractions.
        forever
        {
            bool divved = false;
            for(int div = 2; div <= qMin(ratioX, ratioY); div++)
            {
                int dx = ratioX / div;
                if(dx * div != ratioX) continue;
                int dy = ratioY / div;
                if(dy * div != ratioY) continue;
                divved = true;
                ratioX = dx;
                ratioY = dy;
                break;
            }
            if(!divved) break;
        }

        if(ratioX == 8 && ratioY == 5)
        {
            // This is commonly referred to as 16:10.
            ratioX *= 2;
            ratioY *= 2;
        }
    }

    void debugPrint() const
    {
        qDebug() << "size" << width << "x" << height << "depth" << depth << "rate"
                 << refreshRate << "ratio" << ratioX << ":" << ratioY;
    }
};

typedef std::set<Mode> Modes;
static Modes modes;
static Mode originalMode;

int DisplayMode_Init(void)
{
    if(inited) return true;

    DisplayMode_Native_Init();

    for(int i = 0; i < DisplayMode_Native_Count(); ++i)
    {
        modes.insert(Mode(i));
    }

    originalMode = Mode::fromCurrent();

#ifdef _DEBUG
    qDebug() << "Current mode is:";
    originalMode.debugPrint();

    qDebug() << "All available modes:";
    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i)
    {
        i->debugPrint();
    }
#endif

    inited = true;
    return true;
}

void DisplayMode_Shutdown(void)
{
    if(!inited) return;

    qDebug() << "Restoring original display mode due to shutdown.";

    // Back to the original mode.
    DisplayMode_Change(&originalMode);

    modes.clear();

    DisplayMode_Native_Shutdown();
    inited = false;
}

const DisplayMode* DisplayMode_OriginalMode(void)
{
    return &originalMode;
}

const DisplayMode* DisplayMode_Current(void)
{
    static Mode currentMode;
    // Update it with current mode.
    currentMode = Mode::fromCurrent();
    return &currentMode;
}

int DisplayMode_Count(void)
{
    return (int) modes.size();
}

const DisplayMode* DisplayMode_ByIndex(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < (int) modes.size());

    int pos = 0;
    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i, ++pos)
    {
        if(pos == index)
        {
            return &*i;
        }
    }

    Q_ASSERT(false);
    return 0; // unreachable
}

template <typename T>
T squared(const T& v) { return v * v; }

const DisplayMode* DisplayMode_FindClosest(int width, int height, int depth, float freq)
{
    int bestScore = -1;
    const DisplayMode* best = 0;

    for(Modes::iterator i = modes.begin(); i != modes.end(); ++i)
    {
        int score = squared(i->width - width) + squared(i->height - height) + squared(i->depth - depth);
        if(freq)
        {
            score += squared(i->refreshRate - freq);
        }

        if(!best || score < bestScore)
        {
            /*
#ifdef _DEBUG
            i->debugPrint();
            qDebug() << "Score for" << width << "x" << height << "pixels, depth:" << depth << "bpp, freq:" << freq << "Hz is" << score;
#endif
            */

            bestScore = score;
            best = &*i;
        }
    }
    return best;
}

boolean DisplayMode_IsEqual(const DisplayMode* a, const DisplayMode* b)
{
    return Mode(*a) == Mode(*b);
}

int DisplayMode_Change(const DisplayMode* mode)
{
    if(Mode::fromCurrent() == *mode)
    {
        qDebug() << "DisplayMode: Requested mode is the same as current, ignoring.";

        // Already in this mode.
        return true;
    }
    return DisplayMode_Native_Change(mode);
}
