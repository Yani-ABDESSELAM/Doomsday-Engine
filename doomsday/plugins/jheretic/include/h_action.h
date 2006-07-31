#ifndef __JHERETIC_ACTIONS_H__
#define __JHERETIC_ACTIONS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "dd_share.h"

// These must correspond the action_t's in the actions array!
// Things that are needed in building ticcmds should be here.
typedef enum {
    // Game controls.
    A_TURNLEFT,
    A_TURNRIGHT,
    A_FORWARD,
    A_BACKWARD,
    A_STRAFELEFT,
    A_STRAFERIGHT,
    A_FIRE,
    A_USE,
    A_STRAFE,
    A_SPEED,
    A_FLYUP,
    A_FLYDOWN,
    A_FLYCENTER,
    A_LOOKUP,
    A_LOOKDOWN,
    A_LOOKCENTER,
    A_USEARTIFACT,
    A_MLOOK,
    A_JLOOK,
    A_NEXTWEAPON,
    A_PREVIOUSWEAPON,
    A_WEAPON1,
    A_WEAPON2,
    A_WEAPON3,
    A_WEAPON4,
    A_WEAPON5,
    A_WEAPON6,
    A_WEAPON7,
    A_WEAPON8,
    A_WEAPON9,

    // Item hotkeys.
    A_INVULNERABILITY,
    A_INVISIBILITY,
    A_HEALTH,
    A_SUPERHEALTH,
    A_TOMEOFPOWER,
    A_TORCH,
    A_FIREBOMB,
    A_EGG,
    A_FLY,
    A_TELEPORT,
    A_PANIC,

    // Game system actions.
    A_STOPDEMO,

    A_JUMP,

    A_MAPZOOMIN,
    A_MAPZOOMOUT,
    A_MAPPANUP,
    A_MAPPANDOWN,
    A_MAPPANLEFT,
    A_MAPPANRIGHT,

    A_WEAPONCYCLE1,
    NUM_ACTIONS
} haction_t;

// This is the actions array.
extern action_t actions[NUM_ACTIONS + 1];

#endif
