/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * dstrings.h:
 */

#ifndef __HERETICSTRINGS__
#define __HERETICSTRINGS__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#define GET_TXT(x)      ((*_api_InternalData.text)? (*_api_InternalData.text)[x].text : "")

#define NUM_QUITMESSAGES   0

#define PRESSKEY            GET_TXT(TXT_PRESSKEY)
#define PRESSYN             GET_TXT(TXT_PRESSYN)
#define TXT_PAUSED          GET_TXT(TXT_TXT_PAUSED)
#define QUITMSG             GET_TXT(TXT_QUITMSG)
#define LOADNET             GET_TXT(TXT_LOADNET)
#define QLOADNET            GET_TXT(TXT_QLOADNET)
#define QSAVESPOT           GET_TXT(TXT_QSAVESPOT)
#define SAVEDEAD            GET_TXT(TXT_SAVEDEAD)
#define SAVEOUTMAP          GET_TXT(TXT_SAVEOUTMAP)
#define QSPROMPT            GET_TXT(TXT_QSPROMPT)
#define QLPROMPT            GET_TXT(TXT_QLPROMPT)
#define NEWGAME             GET_TXT(TXT_NEWGAME)
#define NIGHTMARE           GET_TXT(TXT_NIGHTMARE)
#define SWSTRING            GET_TXT(TXT_SWSTRING)
#define MSGOFF              GET_TXT(TXT_MSGOFF)
#define MSGON               GET_TXT(TXT_MSGON)
#define NETEND              GET_TXT(TXT_NETEND)
#define ENDGAME             GET_TXT(TXT_ENDGAME)
#define ENDNOGAME           GET_TXT(TXT_ENDNOGAME)

#define SUICIDEOUTMAP       GET_TXT(TXT_SUICIDEOUTMAP)
#define SUICIDEASK          GET_TXT(TXT_SUICIDEASK)

#define DOSY                GET_TXT(TXT_DOSY)
#define DETAILHI            GET_TXT(TXT_DETAILHI)
#define DETAILLO            GET_TXT(TXT_DETAILLO)
#define GAMMALVL0           GET_TXT(TXT_GAMMALVL0)
#define GAMMALVL1           GET_TXT(TXT_GAMMALVL1)
#define GAMMALVL2           GET_TXT(TXT_GAMMALVL2)
#define GAMMALVL3           GET_TXT(TXT_GAMMALVL3)
#define GAMMALVL4           GET_TXT(TXT_GAMMALVL4)
#define EMPTYSTRING         GET_TXT(TXT_EMPTYSTRING)

// Pickup Key messages:
#define TXT_GOTBLUEKEY      GET_TXT(TXT_TXT_GOTBLUEKEY)
#define TXT_GOTYELLOWKEY    GET_TXT(TXT_TXT_GOTYELLOWKEY)
#define TXT_GOTGREENKEY     GET_TXT(TXT_TXT_GOTGREENKEY)

// Inventory items:
#define TXT_INV_HEALTH      GET_TXT(TXT_TXT_INV_HEALTH)
#define TXT_INV_FLY         GET_TXT(TXT_TXT_INV_FLY)
#define TXT_INV_INVULNERABILITY GET_TXT(TXT_TXT_INV_INVULNERABILITY)
#define TXT_INV_TOMEOFPOWER GET_TXT(TXT_TXT_INV_TOMEOFPOWER)
#define TXT_INV_INVISIBILITY GET_TXT(TXT_TXT_INV_INVISIBILITY)
#define TXT_INV_EGG         GET_TXT(TXT_TXT_INV_EGG)
#define TXT_INV_SUPERHEALTH GET_TXT(TXT_TXT_INV_SUPERHEALTH)
#define TXT_INV_TORCH       GET_TXT(TXT_TXT_INV_TORCH)
#define TXT_INV_FIREBOMB    GET_TXT(TXT_TXT_INV_FIREBOMB)
#define TXT_INV_TELEPORT    GET_TXT(TXT_TXT_INV_TELEPORT)

// Items

#define TXT_ITEMHEALTH      GET_TXT(TXT_TXT_ITEMHEALTH)
#define TXT_ITEMBAGOFHOLDING GET_TXT(TXT_TXT_ITEMBAGOFHOLDING)
#define TXT_ITEMSHIELD1     GET_TXT(TXT_TXT_ITEMSHIELD1)
#define TXT_ITEMSHIELD2     GET_TXT(TXT_TXT_ITEMSHIELD2)
#define TXT_ITEMSUPERMAP    GET_TXT(TXT_TXT_ITEMSUPERMAP)

// Ammo

#define TXT_AMMOGOLDWAND1   GET_TXT(TXT_TXT_AMMOGOLDWAND1)
#define TXT_AMMOGOLDWAND2   GET_TXT(TXT_TXT_AMMOGOLDWAND2)
#define TXT_AMMOMACE1       GET_TXT(TXT_TXT_AMMOMACE1)
#define TXT_AMMOMACE2       GET_TXT(TXT_TXT_AMMOMACE2)
#define TXT_AMMOCROSSBOW1   GET_TXT(TXT_TXT_AMMOCROSSBOW1)
#define TXT_AMMOCROSSBOW2   GET_TXT(TXT_TXT_AMMOCROSSBOW2)
#define TXT_AMMOBLASTER1    GET_TXT(TXT_TXT_AMMOBLASTER1)
#define TXT_AMMOBLASTER2    GET_TXT(TXT_TXT_AMMOBLASTER2)
#define TXT_AMMOSKULLROD1   GET_TXT(TXT_TXT_AMMOSKULLROD1)
#define TXT_AMMOSKULLROD2   GET_TXT(TXT_TXT_AMMOSKULLROD2)
#define TXT_AMMOPHOENIXROD1 GET_TXT(TXT_TXT_AMMOPHOENIXROD1)
#define TXT_AMMOPHOENIXROD2 GET_TXT(TXT_TXT_AMMOPHOENIXROD2)

// Key names:
#define KEY1                GET_TXT(TXT_KEY1)
#define KEY2                GET_TXT(TXT_KEY2)
#define KEY3                GET_TXT(TXT_KEY3)

// Weapon names:
#define TXT_WPNMACE         GET_TXT(TXT_TXT_WPNMACE)
#define TXT_WPNCROSSBOW     GET_TXT(TXT_TXT_WPNCROSSBOW)
#define TXT_WPNBLASTER      GET_TXT(TXT_TXT_WPNBLASTER)
#define TXT_WPNSKULLROD     GET_TXT(TXT_TXT_WPNSKULLROD)
#define TXT_WPNPHOENIXROD   GET_TXT(TXT_TXT_WPNPHOENIXROD)
#define TXT_WPNGAUNTLETS    GET_TXT(TXT_TXT_WPNGAUNTLETS)

#define TXT_CHEATGODON      GET_TXT(TXT_TXT_CHEATGODON)
#define TXT_CHEATGODOFF     GET_TXT(TXT_TXT_CHEATGODOFF)
#define TXT_CHEATNOCLIPON   GET_TXT(TXT_TXT_CHEATNOCLIPON)
#define TXT_CHEATNOCLIPOFF  GET_TXT(TXT_TXT_CHEATNOCLIPOFF)
#define TXT_CHEATWEAPONS    GET_TXT(TXT_TXT_CHEATWEAPONS)
#define TXT_CHEATFLIGHTON   GET_TXT(TXT_TXT_CHEATFLIGHTON)
#define TXT_CHEATFLIGHTOFF  GET_TXT(TXT_TXT_CHEATFLIGHTOFF)
#define TXT_CHEATPOWERON    GET_TXT(TXT_TXT_CHEATPOWERON)
#define TXT_CHEATPOWEROFF   GET_TXT(TXT_TXT_CHEATPOWEROFF)
#define TXT_CHEATHEALTH     GET_TXT(TXT_TXT_CHEATHEALTH)
#define TXT_CHEATKEYS       GET_TXT(TXT_TXT_CHEATKEYS)
#define TXT_CHEATSOUNDON    GET_TXT(TXT_TXT_CHEATSOUNDON)
#define TXT_CHEATSOUNDOFF   GET_TXT(TXT_TXT_CHEATSOUNDOFF)
#define TXT_CHEATTICKERON   GET_TXT(TXT_TXT_CHEATTICKERON)
#define TXT_CHEATTICKEROFF  GET_TXT(TXT_TXT_CHEATTICKEROFF)
#define TXT_CHEATINVITEMS1  GET_TXT(TXT_TXT_CHEATINVITEMS1)
#define TXT_CHEATINVITEMS2  GET_TXT(TXT_TXT_CHEATINVITEMS2)
#define TXT_CHEATINVITEMS3  GET_TXT(TXT_TXT_CHEATINVITEMS3)
#define TXT_CHEATITEMSFAIL  GET_TXT(TXT_TXT_CHEATITEMSFAIL)
#define TXT_CHEATWARP       GET_TXT(TXT_TXT_CHEATWARP)
#define TXT_CHEATSCREENSHOT GET_TXT(TXT_TXT_CHEATSCREENSHOT)
#define TXT_CHEATCHICKENON  GET_TXT(TXT_TXT_CHEATCHICKENON)
#define TXT_CHEATCHICKENOFF GET_TXT(TXT_TXT_CHEATCHICKENOFF)
#define TXT_CHEATMASSACRE   GET_TXT(TXT_TXT_CHEATMASSACRE)
#define TXT_CHEATIDDQD      GET_TXT(TXT_TXT_CHEATIDDQD)
#define TXT_CHEATIDKFA      GET_TXT(TXT_TXT_CHEATIDKFA)

#define TXT_NEEDBLUEKEY     GET_TXT(TXT_TXT_NEEDBLUEKEY)
#define TXT_NEEDGREENKEY    GET_TXT(TXT_TXT_NEEDGREENKEY)
#define TXT_NEEDYELLOWKEY   GET_TXT(TXT_TXT_NEEDYELLOWKEY)

#define TXT_GAMESAVED       GET_TXT(TXT_TXT_GAMESAVED)

#define HUSTR_CHATMACRO1    GET_TXT(TXT_HUSTR_CHATMACRO1)
#define HUSTR_CHATMACRO2    GET_TXT(TXT_HUSTR_CHATMACRO2)
#define HUSTR_CHATMACRO3    GET_TXT(TXT_HUSTR_CHATMACRO3)
#define HUSTR_CHATMACRO4    GET_TXT(TXT_HUSTR_CHATMACRO4)
#define HUSTR_CHATMACRO5    GET_TXT(TXT_HUSTR_CHATMACRO5)
#define HUSTR_CHATMACRO6    GET_TXT(TXT_HUSTR_CHATMACRO6)
#define HUSTR_CHATMACRO7    GET_TXT(TXT_HUSTR_CHATMACRO7)
#define HUSTR_CHATMACRO8    GET_TXT(TXT_HUSTR_CHATMACRO8)
#define HUSTR_CHATMACRO9    GET_TXT(TXT_HUSTR_CHATMACRO9)
#define HUSTR_CHATMACRO0    GET_TXT(TXT_HUSTR_CHATMACRO0)

#define HUSTR_TALKTOSELF1   GET_TXT(TXT_HUSTR_TALKTOSELF1)
#define HUSTR_TALKTOSELF2   GET_TXT(TXT_HUSTR_TALKTOSELF2)
#define HUSTR_TALKTOSELF3   GET_TXT(TXT_HUSTR_TALKTOSELF3)
#define HUSTR_TALKTOSELF4   GET_TXT(TXT_HUSTR_TALKTOSELF4)
#define HUSTR_TALKTOSELF5   GET_TXT(TXT_HUSTR_TALKTOSELF5)

#define HUSTR_MESSAGESENT   GET_TXT(TXT_HUSTR_MESSAGESENT)

// The following should NOT be changed unless it seems
// just AWFULLY necessary

#define HUSTR_PLRGREEN      GET_TXT(TXT_HUSTR_PLRGREEN)
#define HUSTR_PLRINDIGO     GET_TXT(TXT_HUSTR_PLRINDIGO)
#define HUSTR_PLRBROWN      GET_TXT(TXT_HUSTR_PLRBROWN)
#define HUSTR_PLRRED        GET_TXT(TXT_HUSTR_PLRRED)

#define HUSTR_KEYGREEN      'g'
#define HUSTR_KEYINDIGO     'i'
#define HUSTR_KEYBROWN      'b'
#define HUSTR_KEYRED        'r'

#define AMSTR_FOLLOWON      GET_TXT(TXT_AMSTR_FOLLOWON)
#define AMSTR_FOLLOWOFF     GET_TXT(TXT_AMSTR_FOLLOWOFF)
#define AMSTR_ROTATEON      GET_TXT(TXT_AMSTR_ROTATEON)
#define AMSTR_ROTATEOFF     GET_TXT(TXT_AMSTR_ROTATEOFF)

#define AMSTR_GRIDON        GET_TXT(TXT_AMSTR_GRIDON)
#define AMSTR_GRIDOFF       GET_TXT(TXT_AMSTR_GRIDOFF)

#define AMSTR_MARKEDSPOT    GET_TXT(TXT_AMSTR_MARKEDSPOT)
#define AMSTR_MARKSCLEARED  GET_TXT(TXT_AMSTR_MARKSCLEARED)

#define STSTR_DQDON         GET_TXT(TXT_STSTR_DQDON)
#define STSTR_DQDOFF        GET_TXT(TXT_STSTR_DQDOFF)

#define STSTR_KFAADDED      GET_TXT(TXT_STSTR_KFAADDED)

#define STSTR_NCON          GET_TXT(TXT_STSTR_NCON)
#define STSTR_NCOFF         GET_TXT(TXT_STSTR_NCOFF)

#define STSTR_BEHOLD        GET_TXT(TXT_STSTR_BEHOLD)
#define STSTR_BEHOLDX       GET_TXT(TXT_STSTR_BEHOLDX)

#define STSTR_CHOPPERS      GET_TXT(TXT_STSTR_CHOPPERS)
#define STSTR_CLEV          GET_TXT(TXT_STSTR_CLEV)

#define E1TEXT              GET_TXT(TXT_E1TEXT)
#define E2TEXT              GET_TXT(TXT_E2TEXT)
#define E3TEXT              GET_TXT(TXT_E3TEXT)
#define E4TEXT              GET_TXT(TXT_E4TEXT)
#define E5TEXT              GET_TXT(TXT_E5TEXT)

#define DELETESAVEGAME_CONFIRM GET_TXT(TXT_DELETESAVEGAME_CONFIRM)
#define REBORNLOAD_CONFIRM  GET_TXT(TXT_REBORNLOAD_CONFIRM)

#endif