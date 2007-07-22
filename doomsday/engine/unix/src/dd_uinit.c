/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

/*
 * dd_uinit.c: Unix Initialization
 *
 * Load libraries and set up APIs.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <SDL.h>

#ifdef UNIX
#  include "sys_dylib.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

#include "dd_pinit.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint            windowIDX;   // Main window.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;

// CODE --------------------------------------------------------------------

static boolean loadGamePlugin(void)
{
	char       *gameName = NULL;
	char        libName[PATH_MAX];

	// First we need to locate the game library name among the command
	// line arguments.
	DD_CheckArg("-game", &gameName);

	// Was a game dll specified?
	if(!gameName)
	{
		DD_ErrorBox(true, "InitGame: No game library was specified.\n");
		return false;
	}

	// Now, load the library and get the API/exports.
#ifdef MACOSX
	strcpy(libName, gameName);
#else
	sprintf(libName, "lib%s", gameName);
	strcat(libName, ".so");
#endif
	if(!(app.hGame = lt_dlopenext(libName)))
	{
		DD_ErrorBox(true, "InitGame: Loading of %s failed (%s).\n", libName,
					lt_dlerror());
		return false;
	}

    if(!(app.GetGameAPI = (GETGAMEAPI) lt_dlsym(app.hGame, "GetGameAPI")))
	{
		DD_ErrorBox(true,
					"InitGame: Failed to get address of " "GetGameAPI (%s).\n",
					lt_dlerror());
		return false;
	}

	// Do the API transfer.
	DD_InitAPI();

	// Everything seems to be working...
	return true;
}

static lt_dlhandle *NextPluginHandle(void)
{
	int         i;

	for(i = 0; i < MAX_PLUGS; ++i)
	{
		if(!app.hPlugins[i])
            return &app.hPlugins[i];
	}
	return NULL;
}

/*
#if 0
int LoadPlugin(const char *pluginPath, lt_ptr data)
{
	filename_t name;
	lt_dlhandle plugin, *handle;
	void (*initializer)(void);

	// What is the actual file name?
	_splitpath(pluginPath, NULL, NULL, name, NULL);
	
	printf("LP: %s => %s\n", pluginPath, name);

	if(!strncmp(name, "libdp", 5))
	{
#if 0
		filename_t fullPath;
		
#ifdef DENG_LIBRARY_DIR
		sprintf(fullPath, DENG_LIBRARY_DIR "/%s", name);
#else
		strcpy(fullPath, pluginPath);
#endif
		//if(strchr(name, '.'))
			strcpy(name, pluginPath);
		else
		{
			strcpy(name, pluginPath);
			strcat(name, ".dylib");
			}
#endif
		// Try loading this one as a Doomsday plugin.
		if(NULL == (plugin = lt_dlopenext(pluginPath)))
		{
			printf("LoadPlugin: Failed to open %s!\n", pluginPath);
			return 0;
		}

		if(NULL == (initializer = lt_dlsym(plugin, "DP_Initialize")) ||
		   NULL == (handle = NextPluginHandle()))
		{
			printf("LoadPlugin: Failed to load %s!\n", pluginPath);
			lt_dlclose(plugin);
			return 0;
		}

		// This seems to be a Doomsday plugin.
		*handle = plugin;

		printf("LoadPlugin: %s\n", pluginPath);
		initializer();
	}
	
	return 0;
}
#endif
*/

int LoadPlugin(const char *pluginPath, lt_ptr data)
{
#ifndef MACOSX
	filename_t  name;
#endif
	lt_dlhandle plugin, *handle;
	void (*initializer)(void);
	
#ifndef MACOSX
	// What is the actual file name?
	_splitpath(pluginPath, NULL, NULL, name, NULL);
	if(!strncmp(name, "libdp", 5))
#endif
	{
		// Try loading this one as a Doomsday plugin.
		if(NULL == (plugin = lt_dlopenext(pluginPath)))
			return 0;
		
		if(NULL == (initializer = lt_dlsym(plugin, "DP_Initialize")) ||
		   NULL == (handle = NextPluginHandle()))
		{
			printf("LoadPlugin: Failed to load %s!\n", pluginPath);
			lt_dlclose(plugin);
			return 0;
		}
		
		// This seems to be a Doomsday plugin.
		*handle = plugin;
		
		printf("LoadPlugin: %s\n", pluginPath);
		initializer();
	}
	
	return 0;
}

/**
 * Loads all the plugins from the library directory.
 */
static boolean loadAllPlugins(void)
{
	// Try to load all libraries that begin with libdp.
	lt_dlforeachfile(NULL, LoadPlugin, NULL);
	return true;
}

static int initTimingSystem(void)
{
	// For timing, we use SDL under *nix, so get it initialized.
    return SDL_Init(SDL_INIT_TIMER);
}

static int initPluginSystem(void)
{
	// Initialize libtool's dynamic library routines.
	lt_dlinit();

#ifdef DENG_LIBRARY_DIR
	// The default directory is defined in the Makefile.  For
	// instance, "/usr/local/lib".
	lt_dladdsearchdir(DENG_LIBRARY_DIR);
#endif

    return TRUE;
}

int main(int argc, char **argv)
{
	char       *cmdLine;
	int         i, length;
    int         exitCode = 0;
    boolean     doShutdown = true;

	// Assemble a command line string.
	for(i = 0, length = 0; i < argc; ++i)
		length += strlen(argv[i]) + 1;

	// Allocate a large enough string.
	cmdLine = M_Malloc(length);

	for(i = 0, length = 0; i < argc; ++i)
	{
		strcpy(cmdLine + length, argv[i]);
		if(i < argc - 1)
			strcat(cmdLine, " ");
		length += strlen(argv[i]) + 1;
	}

	// Prepare the command line arguments.
	DD_InitCommandLine(cmdLine);
	M_Free(cmdLine);
	cmdLine = NULL;

    if(!DD_EarlyInit())
    {
        DD_ErrorBox(true, "Error during early init.");
    }
    else if(!initTimingSystem())
    {
        DD_ErrorBox(true, "Error initalizing timing system.");
    }
    else if(!initPluginSystem())
    {
        DD_ErrorBox(true, "Error initializing plugin system.");
    }
    // Load the rendering DLL.
	else if(!DD_InitDGL())
    {
		DD_ErrorBox(true, "Error loading rendering library.");
    }
	// Load the game plugin.
	else if(!loadGamePlugin())
    {
		DD_ErrorBox(true, "Error loading game library.");
    }
	// Load all other plugins that are found.
	else if(!loadAllPlugins())
    {
        DD_ErrorBox(true, "Error loading plugins.");
    }
    // Init memory zone.
	else if(!Z_Init())
    {
        DD_ErrorBox(true, "Error initializing memory zone.");
    }
    else if(0 == (windowIDX =
            Sys_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0, buf, NULL)))
    {
        DD_ErrorBox(true, "Error creating main window.");
    }
    else
    {   // All initialization complete.
	    char        buf[256];

        doShutdown = false;

        // Append the main window title with the game name and ensure it
        // is the at the foreground, with focus.
        DD_ComposeMainWindowTitle(buf);
	    Sys_SetWindowTitle(windowIDX, buf);

       // \todo Set foreground window and focus.
    }

    if(!doShutdown)
    {   // Fire up the engine. The game loop will also act as the message pump.
	    exitCode = DD_Main();
    }
    DD_Shutdown();

    // Bye!
    return exitCode;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
	int         i;

	// Shutdown all subsystems.
	DD_ShutdownAll();

	SDL_Quit();

	// Close the dynamic libraries.
	lt_dlclose(app.hGame);
	app.hGame = NULL;
	for(i = 0; app.hPlugins[i]; ++i)
		lt_dlclose(app.hPlugins[i]);
	memset(app.hPlugins, 0, sizeof(app.hPlugins));

	lt_dlexit();
}
