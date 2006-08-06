
//**************************************************************************
//**
//** h2_main.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

/*
 * H2_main.c: Hexen specifc Initialization.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "jhexen.h"
#include "d_net.h"
#include "g_update.h"
#include "g_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct execopt_s {
    char   *name;
    void    (*func) (char **args, int tag);
    int     requiredArgs;
    int     tag;
} execopt_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    R_ExecuteSetViewSize(void);
void    F_Drawer(void);
void    I_HideMouse(void);
void    S_InitScript(void);
void    G_Drawer(void);
void    H2_ConsoleBg(int *width, int *height);
void    H2_EndFrame(void);
int     D_PrivilegedResponder(event_t *event);
void    R_DrawPlayerSprites(ddplayer_t *viewplr);
void    G_ConsoleRegistration();
void    SB_HandleCheatNotification(int fromplayer, void *data, int length);
int     HU_PSpriteYOffset(player_t *pl);
void    InitMapMusicInfo(void);

// Map Data
void    P_SetupForThings(int num);
void    P_SetupForLines(int num);
void    P_SetupForSides(int num);
void    P_SetupForSectors(int num);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void HandleArgs();
static void ExecOptionSCRIPTS(char **args, int tag);
static void ExecOptionDEVMAPS(char **args, int tag);
static void ExecOptionSKILL(char **args, int tag);
static void ExecOptionPLAYDEMO(char **args, int tag);
static void WarpCheck(void);

#ifdef TIMEBOMB
static void DoTimeBomb(void);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean startupScreen;
extern int demosequence;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     verbose;

boolean DevMaps;                // true = Map development mode
char   *DevMapsDir = "";        // development maps directory
boolean nomonsters;             // checkparm of -nomonsters
boolean respawnparm;            // checkparm of -respawn
boolean randomclass;            // checkparm of -randclass
boolean debugmode;              // checkparm of -debug
boolean devparm;                // checkparm of -devparm
boolean nofullscreen;           // checkparm of -nofullscreen
boolean cdrom;                  // true if cd-rom mode active
boolean cmdfrag;                // true if a CMD_FRAG packet should be sent out
boolean singletics;             // debug flag to cancel adaptiveness
boolean artiskip;               // whether shift-enter skips an artifact
boolean netcheat;               // allow cheating in netgames (-netcheat)
boolean dontrender;             // don't render the player view (debug)
skill_t startskill;
int     startepisode;
int     startmap;

GameMode_t gamemode;
int     gamemodebits;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

// default font colours
const float deffontRGB[] = { 1.0f, 0.0f, 0.0f};
const float deffontRGB2[] = { 1.0f, 1.0f, 1.0f};

// Network games parameters.

boolean autostart;

//boolean advancedemo;
FILE   *debugfile;

char   *borderLumps[] = {
    "F_022",                    // background
    "bordt",                    // top
    "bordr",                    // right
    "bordb",                    // bottom
    "bordl",                    // left
    "bordtl",                   // top left
    "bordtr",                   // top right
    "bordbr",                   // bottom right
    "bordbl"                    // bottom left
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int WarpMap;

static execopt_t ExecOptions[] = {
    {"-scripts", ExecOptionSCRIPTS, 1, 0},
    {"-devmaps", ExecOptionDEVMAPS, 1, 0},
    {"-skill", ExecOptionSKILL, 1, 0},
    {"-playdemo", ExecOptionPLAYDEMO, 1, 0},
    {"-timedemo", ExecOptionPLAYDEMO, 1, 0},
    {NULL, NULL, 0, 0}          // Terminator
};

// CODE --------------------------------------------------------------------

/*
 * Attempt to change the current game mode. Can only be done when not
 * actually in a level.
 *
 * TODO: Doesn't actually do anything yet other than set the game mode
 *       global vars.
 *
 * @param mode          The game mode to change to.
 * @return boolean      (TRUE) if we changed game modes successfully.
 */
boolean D_SetGameMode(GameMode_t mode)
{
    gamemode = mode;

    if(gamestate == GS_LEVEL)
        return false;

    switch(mode)
    {
    case shareware: // Shareware (4-level demo)
        gamemodebits = GM_SHAREWARE;
        break;

    case registered: // HEXEN registered
        gamemodebits = GM_REGISTERED;
        break;

    case extended: // Deathkings
        gamemodebits = GM_REGISTERED|GM_EXTENDED;
        break;

    case indetermined: // Well, no IWAD found.
        gamemodebits = GM_INDETERMINED;
        break;

    default:
        Con_Error("D_SetGameMode: Unknown gamemode %i", mode);
    }

    return true;
}

/*
 * Set the game mode string.
 */
void G_IdentifyVersion(void)
{
    // Determine the game mode. Assume demo mode.
    strcpy(gameModeString, "hexen-demo");
    D_SetGameMode(shareware);

    if(W_CheckNumForName("MAP05") >= 0)
    {
        // Normal Hexen.
        strcpy(gameModeString, "hexen");
        D_SetGameMode(registered);
    }

    // This is not a very accurate test...
    if(W_CheckNumForName("MAP59") >= 0 && W_CheckNumForName("MAP60") >= 0)
    {
        // It must be Deathkings!
        strcpy(gameModeString, "hexen-dk");
        D_SetGameMode(extended);
    }
}

/*
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't
 * decide which one gets loaded or even see if the WADs are actually
 * there. The default location for IWADs is Data\GAMENAMETEXT\.
 */
void DetectIWADs(void)
{
    // The startup WADs.
    DD_AddIWAD("}data\\jhexen\\hexen.wad");
    DD_AddIWAD("}data\\hexen.wad");
    DD_AddIWAD("}hexen.wad");
    DD_AddIWAD("hexen.wad");
}

/*
 *  Pre Engine Initialization routine.
 *    All game-specific actions that should take place at this time go here.
 */
void H2_PreInit(void)
{
    int     i;

    D_SetGameMode(indetermined);

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.sbarscale = 20;
    cfg.dclickuse = false;
    cfg.inventoryNextOnUnuse = true;
    cfg.mouseSensiX = 8;
    cfg.mouseSensiY = 8;
    cfg.joyaxis[0] = JOYAXIS_TURN;
    cfg.joyaxis[1] = JOYAXIS_MOVE;
    cfg.screenblocks = cfg.setblocks = 10;
    cfg.hudShown[HUD_MANA] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARTI] = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.xhairSize = 1;
    for(i = 0; i < 4; i++)
        cfg.xhairColor[i] = 255;
    cfg.jumpEnabled = cfg.netJumping = true;     // true by default in Hexen
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.netMap = 1;
    cfg.netSkill = sk_medium;
    cfg.netColor = 8;           // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = 48;
    cfg.levelTitle = true;
    cfg.menuScale = .75f;
    cfg.menuColor[0] = deffontRGB[0];   // use the default colour by default.
    cfg.menuColor[1] = deffontRGB[1];
    cfg.menuColor[2] = deffontRGB[2];
    cfg.menuColor2[0] = deffontRGB2[0]; // use the default colour by default.
    cfg.menuColor2[1] = deffontRGB2[1];
    cfg.menuColor2[2] = deffontRGB2[2];
    cfg.menuEffects = 1;
    cfg.menuFog = 4;
    cfg.menuSlam = true;
    cfg.flashcolor[0] = 1.0f;
    cfg.flashcolor[1] = .5f;
    cfg.flashcolor[2] = .5f;
    cfg.flashspeed = 4;
    cfg.turningSkull = false;
        cfg.hudScale = .7f;
    cfg.hudColor[0] = deffontRGB[0];    // use the default colour by default.
    cfg.hudColor[1] = deffontRGB[1];
    cfg.hudColor[2] = deffontRGB[2];
    cfg.hudColor[3] = 1;
    cfg.hudIconAlpha = 1;
    cfg.usePatchReplacement = 2; // Use built-in replacements if available.
    cfg.cameraNoClip = true;
    cfg.bobView = cfg.bobWeapon = 1;

    cfg.statusbarAlpha = 1;
    cfg.statusbarCounterAlpha = 1;
    cfg.inventoryTimer = 5;

/*    cfg.automapPos = 5;
    cfg.automapWidth = 1.0f;
    cfg.automapHeight = 1.0f;*/

    cfg.automapL0[0] = 0.42f;   // Unseen areas
    cfg.automapL0[1] = 0.42f;
    cfg.automapL0[2] = 0.42f;

    cfg.automapL1[0] = 0.41f;   // onesided lines
    cfg.automapL1[1] = 0.30f;
    cfg.automapL1[2] = 0.15f;

    cfg.automapL2[0] = 0.82f;   // floor height change lines
    cfg.automapL2[1] = 0.70f;
    cfg.automapL2[2] = 0.52f;

    cfg.automapL3[0] = 0.47f;   // ceiling change lines
    cfg.automapL3[1] = 0.30f;
    cfg.automapL3[2] = 0.16f;

    cfg.automapBack[0] = 1.0f;
    cfg.automapBack[1] = 1.0f;
    cfg.automapBack[2] = 1.0f;
    cfg.automapBack[3] = 1.0f;
    cfg.automapLineAlpha = 1.0f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.counterCheatScale = .7f; //From jHeretic

    cfg.msgShow = true;
    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5 * TICSPERSEC;
    cfg.msgAlign = ALIGN_CENTER;
    cfg.msgBlink = 5;
    cfg.msgColor[0] = deffontRGB2[0];
    cfg.msgColor[1] = deffontRGB2[1];
    cfg.msgColor[2] = deffontRGB2[2];

    cfg.weaponOrder[0] = WP_FOURTH;
    cfg.weaponOrder[1] = WP_THIRD;
    cfg.weaponOrder[2] = WP_SECOND;
    cfg.weaponOrder[3] = WP_FIRST;

    // Hexen has a nifty "Ethereal Travel" screen, so don't show the
    // console during map setup.
    Con_SetInteger("con-show-during-setup", 0, true);

    // Do the common pre init routine;
    G_PreInit();
}

/*
 *  Post Engine Initialization routine.
 *    All game-specific actions that should take place at this time go here.
 */
void H2_PostInit(void)
{
    int     p;
    int     pClass;
    char    mapstr[6];

    // Common post init routine
    G_PostInit();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                gamemode == shareware? "*** Hexen 4-level Beta Demo ***\n"
                    : "Hexen\n");
    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    /* None */

    // get skill / episode / map from parms
    startepisode = 1;
    startskill = sk_medium;
    startmap = 1;

    // Game mode specific settings
    /* None */

    // Command line options
    HandleArgs();

    // Check the -class argument.
    pClass = PCLASS_FIGHTER;
    if((p = ArgCheck("-class")) != 0)
    {
        pClass = atoi(Argv(p + 1));
        if(pClass > PCLASS_MAGE || pClass < PCLASS_FIGHTER)
        {
            Con_Error("Invalid player class: %d\n", pClass);
        }
        Con_Message("\nPlayer Class: %d\n", pClass);
    }
    cfg.PlayerClass[consoleplayer] = pClass;

    InitMapMusicInfo();         // Init music fields in mapinfo

    Con_Message("S_InitScript\n");
    S_InitScript();

    Con_Message("SN_InitSequenceScript: Registering sound sequences.\n");
    SN_InitSequenceScript();

    // Check for command line warping. Follows P_Init() because the
    // MAPINFO.TXT script must be already processed.
    WarpCheck();

    // Are we autostarting?
    if(autostart)
    {
        Con_Message("Warp to Map %d (\"%s\":%d), Skill %d\n", WarpMap,
                    P_GetMapName(startmap), startmap, startskill + 1);
    }

    // Load a saved game?
    if((p = ArgCheckWith("-loadgame", 1)) != 0)
    {
        G_LoadGame(atoi(Argv(p + 1)));
    }

    // Check valid episode and map
    if((autostart || IS_NETGAME))
    {
        sprintf(mapstr,"MAP%2.2d", startmap);
        if(!W_CheckNumForName(mapstr))
        {
            startepisode = 1;
            startmap = 1;
        }
    }

    if(gameaction != ga_loadgame)
    {
        GL_Update(DDUF_FULLSCREEN | DDUF_BORDER);
        if(autostart || IS_NETGAME)
        {
            G_StartNewInit();
            G_InitNew(startskill, startepisode, startmap);
        }
        else
        {
            G_StartTitle();     // start up intro loop
        }
    }

}

//==========================================================================
//
// HandleArgs
//
//==========================================================================

static void HandleArgs()
{
    int     p;
    execopt_t *opt;

    nomonsters = ArgExists("-nomonsters");
    respawnparm = ArgExists("-respawn");
    randomclass = ArgExists("-randclass");
    devparm = ArgExists("-devparm");
    artiskip = ArgExists("-artiskip");
    debugmode = ArgExists("-debug");
    cfg.netDeathmatch = ArgExists("-deathmatch");
    cdrom = ArgExists("-cdrom");
    cmdfrag = ArgExists("-cmdfrag");
    nofullscreen = ArgExists("-nofullscreen");
    netcheat = ArgExists("-netcheat");
    dontrender = ArgExists("-noview");

    // Process command line options
    for(opt = ExecOptions; opt->name != NULL; opt++)
    {
        p = ArgCheck(opt->name);
        if(p && p < Argc() - opt->requiredArgs)
        {
            opt->func(ArgvPtr(p), opt->tag);
        }
    }
}

//==========================================================================
//
// WarpCheck
//
//==========================================================================

static void WarpCheck(void)
{
    int     p;
    int     map;

    p = ArgCheck("-warp");
    if(p && p < Argc() - 1)
    {
        WarpMap = atoi(Argv(p + 1));
        map = P_TranslateMap(WarpMap);
        if(map == -1)
        {                       // Couldn't find real map number
            startmap = 1;
            Con_Message("-WARP: Invalid map number.\n");
        }
        else
        {                       // Found a valid startmap
            startmap = map;
            autostart = true;
        }
    }
    else
    {
        WarpMap = 1;
        startmap = P_TranslateMap(1);
        if(startmap == -1)
        {
            startmap = 1;
        }
    }
}

//==========================================================================
//
// ExecOptionSKILL
//
//==========================================================================

static void ExecOptionSKILL(char **args, int tag)
{
    startskill = args[1][0] - '1';
    autostart = true;
}

//==========================================================================
//
// ExecOptionPLAYDEMO
//
//==========================================================================

static void ExecOptionPLAYDEMO(char **args, int tag)
{
    char    file[256];

    sprintf(file, "%s.lmp", args[1]);
    DD_AddStartupWAD(file);
    Con_Message("Playing demo %s.lmp.\n", args[1]);
}

//==========================================================================
//
// ExecOptionSCRIPTS
//
//==========================================================================

static void ExecOptionSCRIPTS(char **args, int tag)
{
    sc_FileScripts = true;
    sc_ScriptsDir = args[1];
}

//==========================================================================
//
// ExecOptionDEVMAPS
//
//==========================================================================

static void ExecOptionDEVMAPS(char **args, int tag)
{
    DevMaps = true;
    Con_Message("Map development mode enabled:\n");
    Con_Message("[config    ] = %s\n", args[1]);
    SC_OpenFileCLib(args[1]);
    SC_MustGetStringName("mapsdir");
    SC_MustGetString();
    Con_Message("[mapsdir   ] = %s\n", sc_String);
    DevMapsDir = malloc(strlen(sc_String) + 1);
    strcpy(DevMapsDir, sc_String);
    SC_MustGetStringName("scriptsdir");
    SC_MustGetString();
    Con_Message("[scriptsdir] = %s\n", sc_String);
    sc_FileScripts = true;
    sc_ScriptsDir = malloc(strlen(sc_String) + 1);
    strcpy(sc_ScriptsDir, sc_String);
    while(SC_GetString())
    {
        if(SC_Compare("file"))
        {
            SC_MustGetString();
            DD_AddStartupWAD(sc_String);
        }
        else
        {
            SC_ScriptError(NULL);
        }
    }
    SC_Close();
}


void H2_Shutdown(void)
{
}

void H2_Ticker(void)
{
    MN_Ticker();
    G_Ticker();
}

/*void G_ModifyDupTiccmd(ticcmd_t *cmd)
   {
   if(cmd->buttons & BT_SPECIAL) cmd->buttons = 0;
   } */

/*void H2_UpdateState(int step)
   {
   switch(step)
   {
   case DD_PRE:
   // Do a sound reset.
   S_Reset();
   break;

   case DD_POST:
   P_Init();
   SB_Init(); // Updates the status bar patches.
   MN_Init();
   S_InitScript();
   SN_InitSequenceScript();
   H2_SetGlowing();
   break;
   }
   } */
