/** @file gloomapp.cpp  Test application.
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

#include "gloomapp.h"
#include "appwindowsystem.h"
#include "../gloom/gloomworld.h"
#include "../gloom/gloomwidget.h"
#include "../gloom/world/user.h"

#include <de/DisplayMode>
#include <de/FileSystem>
#include <de/PackageLoader>
#include <de/ScriptSystem>

using namespace de;
using namespace gloom;

DENG2_PIMPL(GloomApp)
{
    std::unique_ptr<AppWindowSystem> winSys;
    std::unique_ptr<AudioSystem>     audioSys;
    ImageBank                        images;
    GloomWorld                       world;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        // Windows will be closed; OpenGL context will be gone.
        self().glDeinit();
    }

    void loadAllShaders()
    {
        // Load all the shader program definitions.
        FS::FoundFiles found;
        self().findInPackages("shaders.dei", found);
        DENG2_FOR_EACH(FS::FoundFiles, i, found)
        {
            LOG_MSG("Loading shader definitions from %s") << (*i)->description();
            self().shaders().addFromInfo(**i);
        }
    }
};

GloomApp::GloomApp(int &argc, char **argv)
    : BaseGuiApp(argc, argv), d(new Impl(this))
{
    setMetadata("Deng Team", "dengine.net", "Gloom Test", "1.0");
    setUnixHomeFolderName(".test_gloom");
}

void GloomApp::initialize()
{
    DisplayMode_Init();
    addInitPackage("net.dengine.gloom");
    initSubsystems(App::DisablePlugins);

    // Create subsystems.
    {
        d->winSys.reset(new AppWindowSystem);
        addSystem(*d->winSys);

        d->audioSys.reset(new AudioSystem);
        addSystem(*d->audioSys);
    }

    d->loadAllShaders();

    // Load resource banks.
    {
        const Package &base = App::packageLoader().package("net.dengine.gloom");
        d->images  .addFromInfo(base.root().locate<File>("images.dei"));
        waveforms().addFromInfo(base.root().locate<File>("audio.dei"));
    }

    // Create the window.
    MainWindow *win = d->winSys->newWindow<MainWindow>("main");

    win->root().find("gloomwidget")->as<GloomWidget>().setWorld(&d->world);

    scriptSystem().importModule("bootstrap");
    win->show();
}

GloomApp &GloomApp::app()
{
    return *static_cast<GloomApp *>(DENG2_APP);
}

AppWindowSystem &GloomApp::windowSystem()
{
    return *app().d->winSys;
}

AudioSystem &GloomApp::audioSystem()
{
    return *app().d->audioSys;
}

MainWindow &GloomApp::main()
{
    return windowSystem().main();
}

ImageBank &GloomApp::images()
{
    return app().d->images;
}
