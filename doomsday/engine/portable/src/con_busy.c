/**\file con_busy.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_ui.h"
#include "de_misc.h"
#include "de_network.h"

#include "image.h"
#include "texturecontent.h"
#include "cbuffer.h"
#include "font.h"

// MACROS ------------------------------------------------------------------

#define SCREENSHOT_TEXTURE_SIZE 512

#define DOOMWIPESINE_NUMSAMPLES 320

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void Con_BusyLoop(void);
static void Con_BusyDrawer(void);
static void Con_BusyPrepareResources(void);
static void Con_BusyDeleteTextures(void);

static void seedDoomWipeSine(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int rTransition = (int) TS_CROSSFADE;
int rTransitionTics = 28;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean busyInited;
static BusyTask* busyTask; // Current task.
static thread_t busyThread;
static timespan_t busyTime;
static timespan_t accumulatedBusyTime; // Never cleared.
static volatile boolean busyDone;
static volatile boolean busyDoneCopy;
static volatile const char* busyError = NULL;
static fontid_t busyFont = 0;
static int busyFontHgt; // Height of the font.
static mutex_t busy_Mutex; // To prevent Data races in the busy thread.

static DGLuint texLoading[2];
static DGLuint texScreenshot; // Captured screenshot of the latest frame.

static boolean transitionInProgress = false;
static transitionstyle_t transitionStyle = 0;
static uint transitionStartTime = 0;
static float transitionPosition;

static float doomWipeSine[DOOMWIPESINE_NUMSAMPLES];
static float doomWipeSamples[SCREENWIDTH+1];

// CODE --------------------------------------------------------------------

static boolean animatedTransitionActive(int busyMode)
{
    return (!isDedicated && !netGame && !(busyMode & BUSYF_STARTUP) &&
            rTransitionTics > 0 && (busyMode & BUSYF_TRANSITION));
}

int Con_Busy2(BusyTask* task)
{
    boolean willAnimateTransition;
    int result;

    if(!task) return 0;

    if(novideo)
    {
        // Don't bother to go into busy mode.
        return task->retVal = task->worker(task->workerData);
    }

    if(!busyInited)
    {
        busy_Mutex = Sys_CreateMutex("BUSY_MUTEX");
    }

    if(busyInited)
    {
        Con_Error("Con_Busy: Already busy.\n");
    }

    // Activate the UI binding context so that any and all accumulated input
    // events are discarded when done.
    DD_ClearKeyRepeaters();
    B_ActivateContext(B_ContextByName(UI_BINDING_CONTEXT_NAME), true);

    Sys_Lock(busy_Mutex);
    busyDone = false;
    busyTask = task;
    willAnimateTransition = animatedTransitionActive(busyTask->mode);
    Sys_Unlock(busy_Mutex);

    // Load any textures needed in this mode.
    Con_BusyPrepareResources();

    busyInited = true;

    // Start the busy worker thread, which will proces things in the
    // background while we keep the user occupied with nice animations.
    busyThread = Sys_StartThread(busyTask->worker, busyTask->workerData);

    // Are we doing a transition effect?
    if(willAnimateTransition)
    {
        transitionStyle = rTransition;
        if(transitionStyle == TS_DOOM || transitionStyle == TS_DOOMSMOOTH)
            seedDoomWipeSine();
        transitionInProgress = true;
    }

    // Wait for the busy thread to stop.
    Con_BusyLoop();

    // Free resources.
    Con_BusyDeleteTextures();

    if(busyError)
    {
        Con_AbnormalShutdown((const char*) busyError);
    }

    if(!transitionInProgress)
    {
        // Clear any input events that might have accumulated whilst busy.
        DD_ClearEvents();
        B_ActivateContext(B_ContextByName(UI_BINDING_CONTEXT_NAME), false);
    }
    else
    {
        transitionStartTime = Sys_GetTime();
        transitionPosition = 0;
    }

    // Make sure the worker finishes before we continue.
    result = Sys_WaitThread(busyThread);
    busyThread = NULL;
    Sys_DestroyMutex(busy_Mutex);
    busyInited = false;

    task->retVal = result;

    // Make sure that any remaining deferred content gets uploaded.
    if(!(isDedicated || (task->mode & BUSYF_NO_UPLOADS)))
    {
        GL_ProcessDeferredTasks(0);
    }

    return result;
}

int Con_Busy(int mode, const char* _taskName, busyworkerfunc_t worker, void* workerData)
{
    char* taskName = NULL;
    BusyTask task;

    // Take a copy of the task name.
    if(_taskName && _taskName[0])
    {
        size_t len = strlen(_taskName);
        taskName = calloc(1, len + 1);
        dd_snprintf(taskName, len+1, "%s", _taskName);
    }

    // Initialize the task.
    memset(&task, 0, sizeof(task));
    task.worker = worker;
    task.workerData = workerData;
    task.name = taskName;
    task.mode = mode;

    // Lets get busy!
    Con_Busy2(&task);

    if(taskName)
    {
        free(taskName);
        taskName = NULL;
    }

    return task.retVal;
}

void Con_BusyList(BusyTask* tasks, int numTasks)
{
    const char* currentTaskName = NULL;
    BusyTask* task;
    int i, mode;

    if(!tasks) return; // Hmm, no work?

    // Process tasks.
    task = tasks;
    for(i = 0; i < numTasks; ++i, task++)
    {
        // If no name is specified for this task, continue using the name of the
        // the previous task.
        if(task->name)
        {
            if(task->name[0])
                currentTaskName = task->name;
            else // Clear the name.
                currentTaskName = NULL;
        }

        if(!task->worker)
        {
            // Null tasks are not processed; so signal success.
            task->retVal = 0;
            continue;
        }

        // Process the work.

        // Is the worker updating its progress?
        if(task->maxProgress > 0)
            Con_InitProgress2(task->maxProgress, task->progressStart, task->progressEnd);

        /// @kludge Force BUSYF_STARTUP here so that the animation of one task is
        ///         not drawn on top of the last frame of the previous.
        mode = task->mode | BUSYF_STARTUP;
        // kludge end

        // Busy mode invokes the worker on our behalf in a new thread.
        task->retVal = Con_Busy(mode, currentTaskName, task->worker, task->workerData);
    }
}

void Con_BusyWorkerError(const char* message)
{
    busyError = message;
    Con_BusyWorkerEnd();
}

void Con_BusyWorkerEnd(void)
{
    if(!busyInited) return;

    Sys_Lock(busy_Mutex);
    busyDone = true;
    Sys_Unlock(busy_Mutex);
}

boolean Con_IsBusy(void)
{
    return busyInited;
}

boolean Con_IsBusyWorker(void)
{
    boolean result;
    if(!Con_IsBusy()) return false;

    /// @todo Is locking necessary?
    Sys_Lock(busy_Mutex);
    result = Sys_ThreadId(busyThread) == Sys_CurrentThreadId();
    Sys_Unlock(busy_Mutex);
    return result;
}

static void Con_BusyPrepareResources(void)
{
    if(isDedicated || novideo) return;

    if(!(busyTask->mode & BUSYF_STARTUP))
    {
        // Not in startup, so take a copy of the current frame contents.
        Con_AcquireScreenshotTexture();
    }

    // Need to load the progress indicator?
    if(busyTask->mode & (BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR))
    {
        image_t image;

        // These must be real files in the base dir because virtual files haven't
        // been loaded yet when engine startup is done.
        if(GL_LoadImage(&image, "}data/graphics/loading1.png"))
        {
            texLoading[0] = GL_NewTextureWithParams(DGL_RGBA, image.size.width, image.size.height, image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }

        if(GL_LoadImage(&image, "}data/graphics/loading2.png"))
        {
            texLoading[1] = GL_NewTextureWithParams(DGL_RGBA, image.size.width, image.size.height, image.pixels, TXCF_NEVER_DEFER);
            GL_DestroyImage(&image);
        }
    }

    // Need to load any fonts for log messages etc?
    if((busyTask->mode & BUSYF_CONSOLE_OUTPUT) || busyTask->name)
    {
        // These must be real files in the base dir because virtual files haven't
        // been loaded yet when the engine startup is done.
        struct busyFont {
            const char* name;
            const char* path;
        } static const fonts[] = {
            { FN_SYSTEM_NAME":normal12", "}data/fonts/normal12.dfn" },
            { FN_SYSTEM_NAME":normal18", "}data/fonts/normal18.dfn" }
        };
        int fontIdx = !(theWindow->geometry.size.width > 640)? 0 : 1;
        Uri* uri = Uri_NewWithPath2(fonts[fontIdx].name, RC_NULL);
        font_t* font = R_CreateFontFromFile(uri, fonts[fontIdx].path);
        Uri_Delete(uri);
        if(font)
        {
            busyFont = Fonts_Id(font);
            FR_SetFont(busyFont);
            FR_LoadDefaultAttrib();
            busyFontHgt = FR_SingleLineHeight("Busy");
        }
    }
}

static void Con_BusyDeleteTextures(void)
{
    if(isDedicated)
        return;

    glDeleteTextures(2, (const GLuint*) texLoading);
    texLoading[0] = texLoading[1] = 0;

    // Don't release this yet if doing a transition.
    if(!transitionInProgress)
        Con_ReleaseScreenshotTexture();

    busyFont = 0;
}

/**
 * Take a screenshot and store it as a texture.
 */
void Con_AcquireScreenshotTexture(void)
{
    int oldMaxTexSize = GL_state.maxTexSize;
    uint8_t* frame;
#ifdef _DEBUG
    timespan_t startTime;
#endif

    if(texScreenshot)
    {
        Con_ReleaseScreenshotTexture();
    }

#ifdef _DEBUG
    startTime = Sys_GetRealSeconds();
#endif

    frame = malloc(theWindow->geometry.size.width * theWindow->geometry.size.height * 3);
    GL_Grab(0, 0, theWindow->geometry.size.width, theWindow->geometry.size.height, DGL_RGB, frame);
    GL_state.maxTexSize = SCREENSHOT_TEXTURE_SIZE; // A bit of a hack, but don't use too large a texture.
    texScreenshot = GL_NewTextureWithParams2(DGL_RGB, theWindow->geometry.size.width, theWindow->geometry.size.height,
        frame, TXCF_NEVER_DEFER|TXCF_NO_COMPRESSION|TXCF_UPLOAD_ARG_NOSMARTFILTER, 0, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
        GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    GL_state.maxTexSize = oldMaxTexSize;
    free(frame);

#ifdef _DEBUG
    printf("Con_AcquireScreenshotTexture: Took %.2f seconds.\n",
           Sys_GetRealSeconds() - startTime);
#endif
}

void Con_ReleaseScreenshotTexture(void)
{
    glDeleteTextures(1, (const GLuint*) &texScreenshot);
    texScreenshot = 0;
}

/**
 * The busy thread main function. Runs until busyDone set to true.
 */
static void Con_BusyLoop(void)
{
    boolean canDraw = !(isDedicated || novideo);
    boolean canUpload = (canDraw && !(busyTask->mode & BUSYF_NO_UPLOADS));
    timespan_t startTime = Sys_GetRealSeconds();
    timespan_t oldTime;

    if(canDraw)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, theWindow->geometry.size.width, theWindow->geometry.size.height, 0, -1, 1);
    }

    Sys_Lock(busy_Mutex);
    busyDoneCopy = busyDone;
    Sys_Unlock(busy_Mutex);

    while(!busyDoneCopy || (canUpload && GL_DeferredTaskCount() > 0) ||
          !Con_IsProgressAnimationCompleted())
    {
        Sys_Lock(busy_Mutex);
        busyDoneCopy = busyDone;
        Sys_Unlock(busy_Mutex);

        Sys_Sleep(20);

        if(canUpload)
        {   // Make sure that any deferred content gets uploaded.
            GL_ProcessDeferredTasks(15);
        }

        // Update the time.
        oldTime = busyTime;
        busyTime = Sys_GetRealSeconds() - startTime;
        if(busyTime > oldTime)
        {
            accumulatedBusyTime += busyTime - oldTime;
        }

        // Time for an update?
        if(canDraw)
            Con_BusyDrawer();
    }

    if(verbose)
    {
        Con_Message("Con_Busy: Was busy for %.2lf seconds.\n", busyTime);
    }

    if(canDraw)
    {
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }
}

/**
 * Draws the captured screenshot as a background, or just clears the screen if no
 * screenshot is available.
 */
static void Con_DrawScreenshotBackground(float x, float y, float width, float height)
{
    if(texScreenshot)
    {
        GL_BindTextureUnmanaged(texScreenshot, GL_LINEAR);
        glEnable(GL_TEXTURE_2D);

        glColor3ub(255, 255, 255);
        glBegin(GL_QUADS);
            glTexCoord2f(0, 1);
            glVertex2f(x, y);
            glTexCoord2f(1, 1);
            glVertex2f(x + width, y);
            glTexCoord2f(1, 0);
            glVertex2f(x + width, y + height);
            glTexCoord2f(0, 0);
            glVertex2f(x, y + height);
        glEnd();

        glDisable(GL_TEXTURE_2D);
    }
    else
    {
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

/**
 * @param 0...1, to indicate how far things have progressed.
 */
static void Con_BusyDrawIndicator(float x, float y, float radius, float pos)
{
    const float col[4] = {1.f, 1.f, 1.f, .25f};
    int i = 0, edgeCount = 0;
    int backW, backH;

    backW = backH = (radius * 2);

    pos = MINMAX_OF(0, pos, 1);
    edgeCount = MAX_OF(1, pos * 30);

    // Draw a background.
    GL_BlendMode(BM_NORMAL);

    glBegin(GL_TRIANGLE_FAN);
        // Center.
        glColor4ub(0, 0, 0, 140);
        glVertex2f(x, y);
        glColor4ub(0, 0, 0, 0);
        // Vertices along the edge.
        glVertex2f(x, y - backH);
        glVertex2f(x + backW*.5f, y - backH*.8f);
        glVertex2f(x + backW*.8f, y - backH*.5f);
        glVertex2f(x + backW, y);
        glVertex2f(x + backW*.8f, y + backH*.5f);
        glVertex2f(x + backW*.5f, y + backH*.8f);
        glVertex2f(x, y + backH);
        glVertex2f(x - backW*.5f, y + backH*.8f);
        glVertex2f(x - backW*.8f, y + backH*.5f);
        glVertex2f(x - backW, y);
        glVertex2f(x - backW*.8f, y - backH*.5f);
        glVertex2f(x - backW*.5f, y - backH*.8f);
        glVertex2f(x, y - backH);
    glEnd();

    // Draw the frame.
    glEnable(GL_TEXTURE_2D);

    GL_BindTextureUnmanaged(texLoading[0], GL_LINEAR);
    glColor4fv(col);
    GL_DrawRectf2(x - radius, y - radius, radius*2, radius*2);

    // Rotate around center.
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(.5f, .5f, 0.f);
    glRotatef(-accumulatedBusyTime * 20, 0.f, 0.f, 1.f);
    glTranslatef(-.5f, -.5f, 0.f);

    // Draw a fan.
    glColor4f(col[0], col[1], col[2], .5f);
    GL_BindTextureUnmanaged(texLoading[1], GL_LINEAR);
    glBegin(GL_TRIANGLE_FAN);
    // Center.
    glTexCoord2f(.5f, .5f);
    glVertex2f(x, y);
    // Vertices along the edge.
    for(i = 0; i <= edgeCount; ++i)
    {
        float angle = 2 * PI * pos * (i / (float)edgeCount) + PI/2;
        glTexCoord2f(.5f + cos(angle)*.5f, .5f + sin(angle)*.5f);
        glVertex2f(x + cos(angle)*radius*1.05f, y + sin(angle)*radius*1.05f);
    }
    glEnd();

    glMatrixMode(GL_TEXTURE);
    glPopMatrix();

    // Draw the task name.
    if(busyTask->name)
    {
        FR_SetFont(busyFont);
        FR_LoadDefaultAttrib();
        FR_SetColorAndAlpha(1.f, 1.f, 1.f, .66f);
        FR_DrawTextXY3(busyTask->name, x+radius*1.15f, y, ALIGN_LEFT, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);
}

#define LINE_COUNT 4

/**
 * @return              Number of new lines since the old ones.
 */
static int GetBufLines(CBuffer* buffer, cbline_t const **oldLines)
{
    cbline_t const*     bufLines[LINE_COUNT + 1];
    int                 count;
    int                 newCount = 0;
    int                 i, k;

    count = CBuffer_GetLines2(buffer, LINE_COUNT, -LINE_COUNT, bufLines, BLF_OMIT_RULER|BLF_OMIT_EMPTYLINE);
    for(i = 0; i < count; ++i)
    {
        for(k = 0; k < 2 * LINE_COUNT - newCount; ++k)
        {
            if(oldLines[k] == bufLines[i])
            {
                goto lineIsNotNew;
            }
        }

        newCount++;
        for(k = 0; k < (2 * LINE_COUNT - 1); ++k)
            oldLines[k] = oldLines[k+1];

        //memmove(oldLines, oldLines + 1, sizeof(cbline_t*) * (2 * LINE_COUNT - 1));
        oldLines[2 * LINE_COUNT - 1] = bufLines[i];

lineIsNotNew:;
    }
    return newCount;
}

/**
 * Draws a number of console output to the bottom of the screen.
 * \fixme: Wow. I had some weird time hacking the smooth scrolling. Cleanup would be
 *         good some day. -jk
 */
void Con_BusyDrawConsoleOutput(void)
{
    CBuffer*          buffer;
    static cbline_t const* visibleBusyLines[2 * LINE_COUNT];
    static float        scroll = 0;
    static float        scrollStartTime = 0;
    static float        scrollEndTime = 0;
    static double       lastNewTime = 0;
    static double       timeSinceLastNew = 0;
    double              nowTime = 0;
    float               y, topY;
    uint                i, newCount;

    buffer = Con_HistoryBuffer();
    newCount = GetBufLines(buffer, visibleBusyLines);
    nowTime = Sys_GetRealSeconds();
    if(newCount > 0)
    {
        timeSinceLastNew = nowTime - lastNewTime;
        lastNewTime = nowTime;
        if(nowTime < scrollEndTime)
        {
            // Abort last scroll.
            scroll = 0;
            scrollStartTime = nowTime;
            scrollEndTime = nowTime;
        }
        else
        {
            double              interval =
                MIN_OF(timeSinceLastNew/2, 1.3);

            // Begin new scroll.
            scroll = newCount;
            scrollStartTime = nowTime;
            scrollEndTime = nowTime + interval;
        }
    }

    GL_BlendMode(BM_NORMAL);

    // Dark gradient as background.
    glBegin(GL_QUADS);
    glColor4ub(0, 0, 0, 0);
    y = theWindow->geometry.size.height - (LINE_COUNT + 3) * busyFontHgt;
    glVertex2f(0, y);
    glVertex2f(theWindow->geometry.size.width, y);
    glColor4ub(0, 0, 0, 128);
    glVertex2f(theWindow->geometry.size.width, theWindow->geometry.size.height);
    glVertex2f(0, theWindow->geometry.size.height);
    glEnd();

    glEnable(GL_TEXTURE_2D);

    // The text lines.
    topY = y = theWindow->geometry.size.height - busyFontHgt * (2 * LINE_COUNT + .5f);
    if(newCount > 0 ||
       (nowTime >= scrollStartTime && nowTime < scrollEndTime && scrollEndTime > scrollStartTime))
    {
        if(scrollEndTime - scrollStartTime > 0)
            y += scroll * (scrollEndTime - nowTime) / (scrollEndTime - scrollStartTime) *
                busyFontHgt;
    }

    FR_SetFont(busyFont);
    FR_LoadDefaultAttrib();
    FR_SetColor(1.f, 1.f, 1.f);

    for(i = 0; i < 2 * LINE_COUNT; ++i, y += busyFontHgt)
    {
        const cbline_t* line = visibleBusyLines[i];
        float alpha = 1;

        if(!line)
            continue;

        alpha = ((y - topY) / busyFontHgt) - (LINE_COUNT - 1);
        if(alpha < LINE_COUNT)
            alpha = MINMAX_OF(0, alpha/2, 1);
        else
            alpha = 1 - (alpha - LINE_COUNT);

        FR_SetAlpha(alpha);
        FR_DrawTextXY3(line->text, theWindow->geometry.size.width/2, y, ALIGN_TOP, DTF_ONLY_SHADOW);
    }

    glDisable(GL_TEXTURE_2D);

#undef LINE_COUNT
}

/**
 * Busy drawer function. The entire frame is drawn here.
 */
static void Con_BusyDrawer(void)
{
    float pos = 0;

    Con_DrawScreenshotBackground(0, 0, theWindow->geometry.size.width, theWindow->geometry.size.height);

    // Indefinite activity?
    if((busyTask->mode & BUSYF_ACTIVITY) || (busyTask->mode & BUSYF_PROGRESS_BAR))
    {
        if(busyTask->mode & BUSYF_ACTIVITY)
            pos = 1;
        else // The progress is animated elsewhere.
            pos = Con_GetProgress();

        Con_BusyDrawIndicator(theWindow->geometry.size.width/2,
                              theWindow->geometry.size.height/2,
                              theWindow->geometry.size.height/12, pos);
    }

    // Output from the console?
    if(busyTask->mode & BUSYF_CONSOLE_OUTPUT)
    {
        Con_BusyDrawConsoleOutput();
    }

    Sys_UpdateWindow(windowIDX);
}

boolean Con_TransitionInProgress(void)
{
    return transitionInProgress;
}

void Con_TransitionTicker(timespan_t ticLength)
{
    if(isDedicated) return;
    if(!Con_TransitionInProgress()) return;

    transitionPosition = (float)(Sys_GetTime() - transitionStartTime) / rTransitionTics;
    if(transitionPosition >= 1)
    {
        // Clear any input events that might have accumulated during the transition.
        DD_ClearEvents();
        B_ActivateContext(B_ContextByName(UI_BINDING_CONTEXT_NAME), false);

        Con_ReleaseScreenshotTexture();
        transitionInProgress = false;
    }
}

static void seedDoomWipeSine(void)
{
    int i;

    doomWipeSine[0] = (128 - RNG_RandByte()) / 128.f;
    for(i = 1; i < DOOMWIPESINE_NUMSAMPLES; ++i)
    {
        float r = (128 - RNG_RandByte()) / 512.f;
        doomWipeSine[i] = MINMAX_OF(-1, doomWipeSine[i-1] + r, 1);
    }
}

static float sampleDoomWipeSine(float point)
{
    float sample = doomWipeSine[(uint)ROUND((DOOMWIPESINE_NUMSAMPLES-1) * MINMAX_OF(0, point, 1))];
    float offset = SCREENHEIGHT * transitionPosition * transitionPosition;
    return offset + (SCREENHEIGHT/2) * (transitionPosition + sample) * transitionPosition * transitionPosition;
}

static void sampleDoomWipe(void)
{
    int i;
    for(i = 0; i <= SCREENWIDTH; ++i)
    {
        doomWipeSamples[i] = MAX_OF(0, sampleDoomWipeSine((float) i / SCREENWIDTH));
    }
}

void Con_DrawTransition(void)
{
    if(isDedicated) return;
    if(!Con_TransitionInProgress()) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, SCREENWIDTH, SCREENHEIGHT, 0, -1, 1);

    GL_BindTextureUnmanaged(texScreenshot, GL_LINEAR);
    glEnable(GL_TEXTURE_2D);

    switch(transitionStyle)
    {
    case TS_DOOMSMOOTH: {
        float topAlpha, s, div, colWidth = 1.f / SCREENWIDTH;
        int x, y, h, i;

        sampleDoomWipe();
        div = 1 - .25f * transitionPosition;
        topAlpha = 1 - transitionPosition;
        topAlpha *= topAlpha;

        h = SCREENHEIGHT * (1 - div);
        x = 0;
        s = 0;

        glBegin(GL_QUAD_STRIP);
        for(i = 0; i <= SCREENWIDTH; ++i, x++, s += colWidth)
        {
            y = doomWipeSamples[i];

            glColor4f(1, 1, 1, topAlpha);
            glTexCoord2f(s, 1); glVertex2i(x, y);
            glColor4f(1, 1, 1, 1);
            glTexCoord2f(s, div); glVertex2i(x, y + h);
        }
        glEnd();

        x = 0;
        s = 0;

        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUAD_STRIP);
        for(i = 0; i <= SCREENWIDTH; ++i, x++, s += colWidth)
        {
            y = doomWipeSamples[i] + h;

            glTexCoord2f(s, div); glVertex2i(x, y);
            glTexCoord2f(s, 0); glVertex2i(x, y + (SCREENHEIGHT - h));
        }
        glEnd();
        break;
      }
    case TS_DOOM: {
        // As above but drawn with whole pixel columns.
        float s = 0, colWidth = 1.f / SCREENWIDTH;
        int x = 0, y, i;

        sampleDoomWipe();

        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        for(i = 0; i <= SCREENWIDTH; ++i, x++, s+= colWidth)
        {
            y = doomWipeSamples[i];

            glTexCoord2f(s, 1); glVertex2i(x, y);
            glTexCoord2f(s+colWidth, 1); glVertex2i(x+1, y);
            glTexCoord2f(s+colWidth, 0); glVertex2i(x+1, y+SCREENHEIGHT);
            glTexCoord2f(s, 0); glVertex2i(x, y+SCREENHEIGHT);
        }
        glEnd();
        break;
      }
    case TS_CROSSFADE:
        glColor4f(1, 1, 1, 1 - transitionPosition);

        glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(0, 0);
            glTexCoord2f(0, 0); glVertex2f(0, SCREENHEIGHT);
            glTexCoord2f(1, 0); glVertex2f(SCREENWIDTH, SCREENHEIGHT);
            glTexCoord2f(1, 1); glVertex2f(SCREENWIDTH, 0);
        glEnd();
        break;
    }

    GL_SetNoTexture();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
