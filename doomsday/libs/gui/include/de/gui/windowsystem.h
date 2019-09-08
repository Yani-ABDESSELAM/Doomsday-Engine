/** @file windowsystem.h  Window management subsystem.
 *
 * @todo Deferred window changes should be done using a queue-type solution
 * where it is possible to schedule multiple tasks into the future separately
 * for each window. Each window could have its own queue.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_WINDOWSYSTEM_H
#define LIBAPPFW_WINDOWSYSTEM_H

#include <de/System>
#include <de/String>
#include <de/Vector>
#include <de/Event>

#include "../Style"
#include "../BaseWindow"

namespace de {

#undef main

/**
 * Window management subsystem.
 *
 * The window system processes events produced by the input drivers. In
 * practice, the events are passed to the widgets in the windows.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC WindowSystem : public System
{
public:
    /// Required/referenced Window instance is missing. @ingroup errors
    DE_ERROR(MissingWindowError);

public:
    WindowSystem();

    /**
     * Sets a new style for the application.
     *
     * @param style  Style instance. Ownership taken.
     */
    void setStyle(Style *style);

    template <typename WindowType>
    WindowType *newWindow(const String &id) {
        DE_ASSERT(!find(id));
        WindowType *win = new WindowType(id);
        addWindow(id, win);
        return win;
    }

    void addWindow(const String &id, BaseWindow *window);

    /**
     * Returns @c true iff a main window is available.
     */
    static bool mainExists();

    /**
     * Returns the main window.
     */
    static BaseWindow &main();

    void setFocusedWindow(const String &id);

    static BaseWindow *focusedWindow();

    /**
     * Returns a pointer to the @em main window.
     *0
     * @see hasMain()
     */
    inline static BaseWindow *mainPtr() { return mainExists() ? &main() : nullptr; }

    /**
     * @return Number of windows.
     */
    dsize count() const;

    /**
     * Find a window.
     *
     * @param id  Window identifier. "main" for the main window.
     *
     * @return Window instance or @c NULL if not found.
     */
    BaseWindow *find(const String &id) const;

    LoopResult forAll(const std::function<LoopResult(BaseWindow *)> &func);

    /**
     * Closes all windows, including the main window.
     */
    void closeAll();

    /**
     * Returns the window system's UI style.
     */
    Style &style();

    /**
     * Poll SDL events and dispatch them to the window(s) that should handle them.
     */
    void pollAndDispatchEvents();

    /*
     * Dispatches a mouse position event with the latest mouse position. This
     * happens automatically whenever the mouse has moved and time advances.
     */
//    void dispatchLatestMousePosition(BaseWindow &window);

//    Vec2i latestMousePosition(const BaseWindow &window) const;

    // System.
//    bool processEvent(const Event &);
    void timeChanged(const Clock &);

public:
    static void setAppWindowSystem(WindowSystem &winSys);
    static WindowSystem &get();

protected:
    virtual void closingAllWindows();
//    virtual bool rootProcessEvent(const Event &event);
    virtual void rootUpdate(); // all windows

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_WINDOWSYSTEM_H
