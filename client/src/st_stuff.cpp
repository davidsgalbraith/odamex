// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2013 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//		Status bar code.
//		Does the face/direction indicator animatin.
//		Does palette indicators as well (red pain/berserk, bright pickup)
//		[RH] Widget coordinates are relative to the console, not the screen!
//
//-----------------------------------------------------------------------------

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "m_random.h"
#include "w_wad.h"
#include "doomdef.h"
#include "g_game.h"
#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"
#include "p_local.h"
#include "p_inter.h"
#include "am_map.h"
#include "m_cheat.h"
#include "s_sound.h"
#include "v_video.h"
#include "v_text.h"
#include "doomstat.h"
#include "gstrings.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "version.h"
#include "cl_main.h"
#include "gi.h"
#include "cl_demo.h"

#include "p_ctf.h"


// ST_Start() has just been called
bool		st_firsttime;

// lump number for PLAYPAL
static int		lu_palette;

EXTERN_CVAR (idmypos)
EXTERN_CVAR (sv_allowredscreen)
EXTERN_CVAR (screenblocks)
EXTERN_CVAR (hud_fullhudtype)

CVAR_FUNC_IMPL (r_painintensity)
{
	if (var < 0.f)
		var.Set (0.f);
	if (var > 1.f)
		var.Set (1.f);
}

extern int SquareWidth;

static fixed_t ST_SCALEX, ST_SCALEY;

void ST_DrawTexture(const Texture* texture, int x0, int y0)
{
	int x = ST_X + ((ST_SCALEX * x0) >> FRACBITS);
	int y = ST_Y + ((ST_SCALEY * y0) >> FRACBITS);
	int w = (ST_SCALEX * texture->getWidth()) >> FRACBITS;
	int h = (ST_SCALEY * texture->getHeight()) >> FRACBITS;
	screen->DrawTextureStretched(texture, x, y, w, h);
}

void ST_AdjustStatusBarScale(bool scale)
{
	if (scale)
	{
		// Scale status bar height to screen.
		ST_HEIGHT = (32 * screen->height) / 200;
		// [AM] Scale status bar width according to height, unless there isn't
		//      enough room for it.  Fixes widescreen status bar scaling.
		// [ML] A couple of minor changes for true 4:3 correctness...
		ST_WIDTH = ST_HEIGHT*10;
		if (!screen->isProtectedRes())
        {
            ST_WIDTH = SquareWidth;
        }
	}
	else
	{
		// Do not stretch status bar
		ST_WIDTH = 320;
		ST_HEIGHT = 32;
	}

	if (consoleplayer().spectator && displayplayer_id == consoleplayer_id)
	{
		ST_X = 0;
		ST_Y = screen->height;
	}
	else
	{
		ST_X = (screen->width-ST_WIDTH)/2;
		ST_Y = screen->height - ST_HEIGHT;
	}

	ST_SCALEX = FixedDiv(ST_WIDTH << FRACBITS, 320*FRACUNIT);
	ST_SCALEY = FixedDiv(ST_HEIGHT << FRACBITS, 32*FRACUNIT);

	setsizeneeded = true;
	st_firsttime = true;
}


CVAR_FUNC_IMPL (st_scale)		// Stretch status bar to full screen width?
{
	ST_AdjustStatusBarScale(var != 0);
}

// [RH] Needed when status bar scale changes
extern BOOL setsizeneeded;
extern BOOL automapactive;

// functions in st_new.cpp
void ST_voteDraw (int y);
void ST_newDraw (void);
void ST_newDrawCTF (void);

// [RH] Base blending values (for e.g. underwater)
int BaseBlendR, BaseBlendG, BaseBlendB;
float BaseBlendA;

extern bool simulated_connection;

//
// STATUS BAR DATA
//


// N/256*100% probability
//	that the normal face state will change
#define ST_FACEPROBABILITY		96

// Location of status bar face
#define ST_FX					(143)
#define ST_FY					(0)

// Should be set to patch width
//	for tall numbers later on
#define ST_TALLNUMWIDTH 		(tallnum[0]->width)

#define ST_FACESX				(143)
#define ST_FACESY				(0)

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT			(1*TICRATE)
#define ST_OUCHCOUNT			(1*TICRATE)
#define ST_RAMPAGEDELAY 		(2*TICRATE)

#define ST_MUCHPAIN 			20


// Location and size of statistics,
//	justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//		 Problem is, is the stuff rendered
//		 into a buffer,
//		 or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH			3
#define ST_AMMOX				(44)
#define ST_AMMOY				(3)

// HEALTH number pos.
#define ST_HEALTHWIDTH			3
#define ST_HEALTHX				(90)
#define ST_HEALTHY				(3)

// Weapon pos.
#define ST_ARMSX				(111)
#define ST_ARMSY				(4)
#define ST_ARMSBGX				(104)
#define ST_ARMSBGY				(0)
#define ST_ARMSXSPACE			12
#define ST_ARMSYSPACE			10

// Flags pos.
#define ST_FLAGSBGX				(106)
#define ST_FLAGSBGY				(0)

// Frags pos.
#define ST_FRAGSX				(138)
#define ST_FRAGSY				(3)
#define ST_FRAGSWIDTH			2

// ARMOR number pos.
#define ST_ARMORWIDTH			3
#define ST_ARMORX				(221)
#define ST_ARMORY				(3)

// Flagbox positions.
#define ST_FLGBOXX				(236)
#define ST_FLGBOXY				(0)
#define ST_FLGBOXBLUX			(239)
#define ST_FLGBOXBLUY			(3)
#define ST_FLGBOXREDX			(239)
#define ST_FLGBOXREDY			(18)

// Key icon positions.
#define ST_KEY0WIDTH			8
#define ST_KEY0HEIGHT			5
#define ST_KEY0X				(239)
#define ST_KEY0Y				(3)
#define ST_KEY1WIDTH			ST_KEY0WIDTH
#define ST_KEY1X				(239)
#define ST_KEY1Y				(13)
#define ST_KEY2WIDTH			ST_KEY0WIDTH
#define ST_KEY2X				(239)
#define ST_KEY2Y				(23)

// Ammunition counter.
#define ST_AMMO0WIDTH			3
#define ST_AMMO0HEIGHT			6
#define ST_AMMO0X				(288)
#define ST_AMMO0Y				(5)
#define ST_AMMO1WIDTH			ST_AMMO0WIDTH
#define ST_AMMO1X				(288)
#define ST_AMMO1Y				(11)
#define ST_AMMO2WIDTH			ST_AMMO0WIDTH
#define ST_AMMO2X				(288)
#define ST_AMMO2Y				(23)
#define ST_AMMO3WIDTH			ST_AMMO0WIDTH
#define ST_AMMO3X				(288)
#define ST_AMMO3Y				(17)

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH		3
#define ST_MAXAMMO0HEIGHT		5
#define ST_MAXAMMO0X			(314)
#define ST_MAXAMMO0Y			(5)
#define ST_MAXAMMO1WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X			(314)
#define ST_MAXAMMO1Y			(11)
#define ST_MAXAMMO2WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X			(314)
#define ST_MAXAMMO2Y			(23)
#define ST_MAXAMMO3WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X			(314)
#define ST_MAXAMMO3Y			(17)

// pistol
#define ST_WEAPON0X 			(110)
#define ST_WEAPON0Y 			(4)

// shotgun
#define ST_WEAPON1X 			(122)
#define ST_WEAPON1Y 			(4)

// chain gun
#define ST_WEAPON2X 			(134)
#define ST_WEAPON2Y 			(4)

// missile launcher
#define ST_WEAPON3X 			(110)
#define ST_WEAPON3Y 			(13)

// plasma gun
#define ST_WEAPON4X 			(122)
#define ST_WEAPON4Y 			(13)

 // bfg
#define ST_WEAPON5X 			(134)
#define ST_WEAPON5Y 			(13)

// WPNS title
#define ST_WPNSX				(109)
#define ST_WPNSY				(23)

 // DETH title
#define ST_DETHX				(109)
#define ST_DETHY				(23)

// [RH] Turned these into variables
// Size of statusbar.
// Now ([RH] truly) sensitive for scaling.
int						ST_HEIGHT;
int						ST_WIDTH;
int						ST_X;
int						ST_Y;

// statusbar / hud graphics
const Texture*	medi;
const Texture*	armors[2];
const Texture*	ammos[4];
const Texture*	bigammos[4];
const Texture*	hudflagteam;
const Texture*	hudflagbhome;
const Texture*	hudflagrhome;
const Texture*	hudflagbtakenbyb;
const Texture*	hudflagbtakenbyr;
const Texture*	hudflagrtakenbyb;
const Texture*	hudflagrtakenbyr;
const Texture*	hudflaggtakenbyb;
const Texture*	hudflaggtakenbyr;
const Texture*	hudflagbdropped;
const Texture*	hudflagrdropped;
const Texture*	line_leftempty;
const Texture*	line_leftfull;
const Texture*	line_centerempty;
const Texture*	line_centerleft;
const Texture*	line_centerright;
const Texture*	line_centerfull;
const Texture*	line_rightempty;
const Texture*	line_rightfull;
const Texture*	tallminus;
const Texture*	tallpercent;
const Texture*	tallnum[10];
const Texture*	shortnum[10];
const Texture*	keys[NUMCARDS + NUMCARDS / 2];
const Texture*	faces[ST_NUMFACES];
const Texture*	armsbg;
const Texture*	flagsbg;
const Texture*	flagbox;
const Texture*	flagboxb;
const Texture*	flagboxr;
const Texture*	arms[6][2];
const Texture*	faceback;
const Texture*	faceclassic[4];
const Texture*	sbar;

// used for making messages go away
static int				st_msgcounter=0;

// whether in automap or first-person
static st_stateenum_t	st_gamestate;

// whether left-side main status bar is active
static bool			st_statusbaron;

// whether status bar chat is active
static bool			st_chat;

// value of st_chat before message popped up
static bool			st_oldchat;

// whether chat window has the cursor on
static bool			st_cursoron;

// !deathmatch && st_statusbaron
static bool			st_armson;

// !deathmatch
static bool			st_fragson;

// show flagbox blue indicator
static bool			st_flagboxbluon;

// show flagbox red indicator
static bool			st_flagboxredon;

// ready-weapon widget
static st_number_t		w_ready;

 // in deathmatch only, summary of frags stats
static st_number_t		w_frags;

// health widget
static st_percent_t 	w_health;

// weapon ownership widgets
static st_multicon_t	w_arms[6];

// face status widget
static st_multicon_t	w_faces;

// keycard widgets
static st_multicon_t	w_keyboxes[3];

// armor widget
static st_percent_t 	w_armor;

// ammo widgets
static st_number_t		w_ammo[4];

// max ammo widgets
static st_number_t		w_maxammo[4];

// flagbox blue indicator widget
static st_binicon_t		w_flagboxblu;

// flagbox red indicator widger
static st_binicon_t		w_flagboxred;

// number of frags so far in deathmatch
static int		st_fragscount;

// used to use appopriately pained face
static int		st_oldhealth = -1;

// used for evil grin
static bool		oldweaponsowned[NUMWEAPONS];

 // count until face changes
static int		st_facecount = 0;

// current face index, used by w_faces
// [RH] not static anymore
int				st_faceindex = 0;

// holds key-type for each key box on bar
static int		keyboxes[3];

// copy of player info
static int		st_health, st_armor;
static int		st_ammo[4], st_maxammo[4];
static int		st_weaponowned[6] = {0}, st_current_ammo;

// a random number per tick
static int		st_randomnumber;

// these are now in d_dehacked.cpp
extern byte cheat_mus_seq[9];
extern byte cheat_choppers_seq[11];
extern byte cheat_god_seq[6];
extern byte cheat_ammo_seq[6];
extern byte cheat_ammonokey_seq[5];
extern byte cheat_noclip_seq[11];
extern byte cheat_commercial_noclip_seq[7];
extern byte cheat_powerup_seq[7][10];
extern byte cheat_clev_seq[10];
extern byte cheat_mypos_seq[8];

// CTF...
extern flagdata CTFdata[NUMFLAGS];

// Now what?
cheatseq_t		cheat_mus = { cheat_mus_seq, 0 };
cheatseq_t		cheat_god = { cheat_god_seq, 0 };
cheatseq_t		cheat_ammo = { cheat_ammo_seq, 0 };
cheatseq_t		cheat_ammonokey = { cheat_ammonokey_seq, 0 };
cheatseq_t		cheat_noclip = { cheat_noclip_seq, 0 };
cheatseq_t		cheat_commercial_noclip = { cheat_commercial_noclip_seq, 0 };

cheatseq_t		cheat_powerup[7] =
{
	{ cheat_powerup_seq[0], 0 },
	{ cheat_powerup_seq[1], 0 },
	{ cheat_powerup_seq[2], 0 },
	{ cheat_powerup_seq[3], 0 },
	{ cheat_powerup_seq[4], 0 },
	{ cheat_powerup_seq[5], 0 },
	{ cheat_powerup_seq[6], 0 }
};

cheatseq_t		cheat_choppers = { cheat_choppers_seq, 0 };
cheatseq_t		cheat_clev = { cheat_clev_seq, 0 };
cheatseq_t		cheat_mypos = { cheat_mypos_seq, 0 };


//
// STATUS BAR CODE
//

//
// ST_LoadSprite
//
// Loads a status bar sprite and prevents it from being freed when the
// Zone memory manager is low on memory.
// 
static const Texture* ST_LoadSprite(const char* name)
{
	texhandle_t texhandle = texturemanager.getHandle(name, Texture::TEX_SPRITE);
	const Texture* texture = texturemanager.getTexture(texhandle);
	return texture;
}

//
// ST_UnloadSprite
//
// Allows a status bar sprite to be freed by the Zone memory manager
//
static void ST_UnloadSprite(const Texture* texture)
{
}

//
// ST_ShutdownSprites
//
void ST_ShutdownSprites(void)
{
	ST_UnloadSprite(medi);

	ST_UnloadSprite(hudflagteam);
	ST_UnloadSprite(hudflagbhome);
	ST_UnloadSprite(hudflagrhome);
	ST_UnloadSprite(hudflagbtakenbyb);
	ST_UnloadSprite(hudflagbtakenbyr);
	ST_UnloadSprite(hudflagrtakenbyb);
	ST_UnloadSprite(hudflagrtakenbyr);
	ST_UnloadSprite(hudflaggtakenbyb);
	ST_UnloadSprite(hudflaggtakenbyr);
	ST_UnloadSprite(hudflagbdropped);
	ST_UnloadSprite(hudflagrdropped);

	ST_UnloadSprite(line_leftempty);
	ST_UnloadSprite(line_leftfull);
	ST_UnloadSprite(line_centerempty);
	ST_UnloadSprite(line_centerleft);
	ST_UnloadSprite(line_centerright);
	ST_UnloadSprite(line_centerfull);
	ST_UnloadSprite(line_rightempty);
	ST_UnloadSprite(line_rightfull);

	for (int i = 0; i < 2; i++)
		ST_UnloadSprite(armors[i]);

	for (int i = 0; i < 4; i++)
	{
		ST_UnloadSprite(ammos[i]);
		ST_UnloadSprite(bigammos[i]);
	}

	ST_UnloadSprite(tallminus);
	ST_UnloadSprite(tallpercent);

	for (int i = 0; i < 10; i++)
	{
		ST_UnloadSprite(tallnum[i]);
		ST_UnloadSprite(shortnum[i]);
	}

	for (int i = 0; i < NUMCARDS + NUMCARDS / 2; i++)
		ST_UnloadSprite(keys[i]);

	for (int i = 0; i < ST_NUMFACES; i++)
		ST_UnloadSprite(faces[i]);

	ST_UnloadSprite(armsbg);
	ST_UnloadSprite(flagsbg);
	ST_UnloadSprite(flagbox);
	ST_UnloadSprite(flagboxb);
	ST_UnloadSprite(flagboxr);

	for (int i = 0; i < 6; i++)
	{
		ST_UnloadSprite(arms[i][0]);
		ST_UnloadSprite(arms[i][1]);
	}

	ST_UnloadSprite(sbar);
	ST_UnloadSprite(faceback);
	
	for (int i = 0; i < 4; i++)
		ST_UnloadSprite(faceclassic[i]);
}

//
// ST_InitSprites
//
void ST_InitSprites(void)
{
	char name[9];

	// load the player face sprites
	sprintf(name, "STF");
	int facenum = 0;
	for (int i = 0; i < ST_NUMPAINFACES; i++)
	{
		for (int j = 0; j < ST_NUMSTRAIGHTFACES; j++)
		{
			sprintf(name+3, "ST%d%d", i, j);
			faces[facenum++] = ST_LoadSprite(name);
		}
		sprintf(name+3, "TR%d0", i);		// turn right
		faces[facenum++] = ST_LoadSprite(name);
		sprintf(name+3, "TL%d0", i);		// turn left
		faces[facenum++] = ST_LoadSprite(name);
		sprintf(name+3, "OUCH%d", i);		// ouch!
		faces[facenum++] = ST_LoadSprite(name);
		sprintf(name+3, "EVL%d", i);		// evil grin ;)
		faces[facenum++] = ST_LoadSprite(name);
		sprintf(name+3, "KILL%d", i);		// pissed off
		faces[facenum++] = ST_LoadSprite(name);
	}
	strcpy(name+3, "GOD0");
	faces[facenum++] = ST_LoadSprite(name);
	strcpy(name+3, "DEAD0");
	faces[facenum++] = ST_LoadSprite(name);

	// load the key card sprites
	for (int i = 0; i < NUMCARDS + NUMCARDS / 2; i++)
	{
		sprintf(name, "STKEYS%d", i);
		keys[i] = ST_LoadSprite(name);
	}

	tallminus = ST_LoadSprite("STTMINUS");
	tallpercent = ST_LoadSprite("STTPRCNT");

	// load the tall & short number sprites
	for (int i = 0; i < 10; i++)
	{
		sprintf(name, "STTNUM%d", i);
		tallnum[i] = ST_LoadSprite(name);

		sprintf(name, "STYSNUM%d", i);
		shortnum[i] = ST_LoadSprite(name);
	}

	medi = ST_LoadSprite("MEDIA0");

	armors[0] = ST_LoadSprite("ARM1A0");
	armors[1] = ST_LoadSprite("ARM2A0");

	ammos[0] = ST_LoadSprite("CLIPA0");
	ammos[1] = ST_LoadSprite("SHELA0");
	ammos[2] = ST_LoadSprite("CELLA0");
	ammos[3] = ST_LoadSprite("ROCKA0");

	bigammos[0] = ST_LoadSprite("AMMOA0");
	bigammos[1] = ST_LoadSprite("SBOXA0");
	bigammos[2] = ST_LoadSprite("CELPA0");
	bigammos[3] = ST_LoadSprite("BROKA0");
	
	hudflagteam			= ST_LoadSprite("FLAGIT");
	hudflagbhome		= ST_LoadSprite("FLAGIC2B");
	hudflagrhome		= ST_LoadSprite("FLAGIC2R");
	hudflagbtakenbyb	= ST_LoadSprite("FLAGI3BB");
	hudflagbtakenbyr	= ST_LoadSprite("FLAGI3BR");
	hudflagrtakenbyb	= ST_LoadSprite("FLAGI3RB");
	hudflagrtakenbyr	= ST_LoadSprite("FLAGI3RR");
	hudflagbdropped		= ST_LoadSprite("FLAGIC4B");
	hudflagrdropped		= ST_LoadSprite("FLAGIC4R");

	flagsbg		= ST_LoadSprite("STFLAGS");
	flagbox		= ST_LoadSprite("STFLGBOX");
	flagboxb	= ST_LoadSprite("STFLGBXB");
	flagboxr	= ST_LoadSprite("STFLGBXR");

	// face backgrounds for different color players
	// [RH] only one face background used for all players
	//		different colors are accomplished with translations
	faceback = ST_LoadSprite("STFBANY");

	// [Nes] Classic vanilla lifebars.
	for (int i = 0; i < 4; i++)
	{
		sprintf(name, "STFB%d", i);
		faceclassic[i] = ST_LoadSprite(name);
	}

	// status bar background bits
	sbar = ST_LoadSprite("STBAR");
	armsbg = ST_LoadSprite("STARMS");

	// arms ownership widgets
	for (int i = 0; i < 6; i++)
	{
		sprintf(name, "STGNUM%d", i + 2);
		arms[i][0] = ST_LoadSprite(name);	// gray number
		arms[i][1] = shortnum[i + 2];		// yellow number
	}

	// horizontal rulers for scoreboards
	line_leftempty		= ST_LoadSprite("ODABARLE");
	line_leftfull		= ST_LoadSprite("ODABARLF");
	line_centerempty	= ST_LoadSprite("ODABARCE");
	line_centerleft		= ST_LoadSprite("ODABARCL");
	line_centerright	= ST_LoadSprite("ODABARCR");
	line_centerfull		= ST_LoadSprite("ODABARCF");
	line_rightempty		= ST_LoadSprite("ODABARRE");
	line_rightfull		= ST_LoadSprite("ODABARRF");
}

void ST_Stop(void);
void ST_createWidgets(void);

void ST_refreshBackground(void)
{
	if (st_statusbaron)
	{
		// [RH] If screen is wider than the status bar,
		//      draw stuff around status bar.
		if (screen->width > ST_WIDTH)
		{
			R_DrawBorder(0, ST_Y, ST_X - 1, screen->height - 1);
			R_DrawBorder(screen->width - ST_X, ST_Y, screen->width - 1, screen->height - 1);
		}

		ST_DrawTexture(sbar, 0, 0);

		if (sv_gametype == GM_CTF)
		{
			ST_DrawTexture(flagsbg, ST_FLAGSBGX, ST_FLAGSBGY);
			ST_DrawTexture(flagbox, ST_FLGBOXX, ST_FLGBOXY);
		}
		else if (sv_gametype == GM_COOP)
		{
			ST_DrawTexture(armsbg, ST_ARMSBGX, ST_ARMSBGY);
		}

		if (multiplayer)
		{
			if (!demoplayback || !democlassic)
			{
				// [RH] Always draw faceback with the player's color
				//		using a translation rather than a different patch.
				//V_ColorMap = translationtables + (displayplayer_id) * 256;
				V_ColorMap = translationref_t(translationtables + displayplayer_id * 256, displayplayer_id);
				// TODO: Add color translation
				ST_DrawTexture(faceback, ST_FX, ST_FY);
				
			}
			else
			{
				ST_DrawTexture(faceclassic[displayplayer_id-1], ST_FX, ST_FY);
			}
		}
	}
}


EXTERN_CVAR (sv_allowcheats)

// Checks whether cheats are enabled or not, returns true if they're NOT enabled
// and false if they ARE enabled (stupid huh? not my work [Russell])
BOOL CheckCheatmode (void)
{
	// [SL] 2012-04-04 - Don't allow cheat codes to be entered while playing
	// back a netdemo
	if (simulated_connection)
		return true;

	// [Russell] - Allow vanilla style "no message" in singleplayer when cheats
	// are disabled
	if (sv_skill == sk_nightmare && !multiplayer)
        return true;

	if ((multiplayer || sv_gametype != GM_COOP) && !sv_allowcheats)
	{
		Printf (PRINT_HIGH, "You must run the server with '+set sv_allowcheats 1' to enable this command.\n");
		return true;
	}
	else
	{
		return false;
	}
}

// Respond to keyboard input events, intercept cheats.
// [RH] Cheats eatkey the last keypress used to trigger them
bool ST_Responder (event_t *ev)
{
	  player_t *plyr = &consoleplayer();
	  bool eatkey = false;
	  int i;

	  // Filter automap on/off.
	  if (ev->type == ev_keyup && ((ev->data1 & 0xffff0000) == AM_MSGHEADER))
	  {
		switch(ev->data1)
		{
		case AM_MSGENTERED:
			st_gamestate = AutomapState;
			st_firsttime = true;
			break;

		case AM_MSGEXITED:
			st_gamestate = FirstPersonState;
			break;
		}
	}

	// if a user keypress...
    else if (ev->type == ev_keydown)
    {
        // 'dqd' cheat for toggleable god mode
        if (cht_CheckCheat(&cheat_god, (char)ev->data2))
        {
            if (CheckCheatmode ())
                return false;

            // [Russell] - give full health
            plyr->mo->health = deh.StartHealth;
            plyr->health = deh.StartHealth;

            AddCommandString("god");

            // Net_WriteByte (DEM_GENERICCHEAT);
            // Net_WriteByte (CHT_IDDQD);
            eatkey = true;
        }

        // 'fa' cheat for killer fucking arsenal
        else if (cht_CheckCheat(&cheat_ammonokey, (char)ev->data2))
        {
            if (CheckCheatmode ())
                return false;

            Printf(PRINT_HIGH, "Ammo (No keys) Added\n");

            plyr->armorpoints = deh.FAArmor;
            plyr->armortype = deh.FAAC;

            weapontype_t pendweap = plyr->pendingweapon;
            for (i = 0; i<NUMWEAPONS; i++)
                P_GiveWeapon (plyr, (weapontype_t)i, false);
            plyr->pendingweapon = pendweap;

            for (i=0; i<NUMAMMO; i++)
                plyr->ammo[i] = plyr->maxammo[i];

            MSG_WriteMarker(&net_buffer, clc_cheatpulse);
            MSG_WriteByte(&net_buffer, 1);

            eatkey = true;
        }

        // 'kfa' cheat for key full ammo
        else if (cht_CheckCheat(&cheat_ammo, (char)ev->data2))
        {
            if (CheckCheatmode ())
                return false;

            Printf(PRINT_HIGH, "Very Happy Ammo Added\n");

            plyr->armorpoints = deh.KFAArmor;
            plyr->armortype = deh.KFAAC;

            weapontype_t pendweap = plyr->pendingweapon;
            for (i = 0; i<NUMWEAPONS; i++)
                P_GiveWeapon (plyr, (weapontype_t)i, false);
            plyr->pendingweapon = pendweap;

            for (i=0; i<NUMAMMO; i++)
                plyr->ammo[i] = plyr->maxammo[i];

            for (i=0; i<NUMCARDS; i++)
                plyr->cards[i] = true;

            MSG_WriteMarker(&net_buffer, clc_cheatpulse);
            MSG_WriteByte(&net_buffer, 2);

            eatkey = true;
        }
        // [Russell] - Only doom 1/registered can have idspispopd and
        // doom 2/final can have idclip
        else if (cht_CheckCheat(&cheat_noclip, (char)ev->data2))
        {
            if (CheckCheatmode ())
                return false;

            if ((gamemode != shareware) && (gamemode != registered) &&
                (gamemode != retail))
                return false;

            AddCommandString("noclip");

            // Net_WriteByte (DEM_GENERICCHEAT);
            // Net_WriteByte (CHT_NOCLIP);
            eatkey = true;
        }
        else if (cht_CheckCheat(&cheat_commercial_noclip, (char)ev->data2))
        {
            if (CheckCheatmode ())
                return false;

            if (gamemode != commercial && gamemode != commercial_bfg)
                return false;

            AddCommandString("noclip");

            // Net_WriteByte (DEM_GENERICCHEAT);
            // Net_WriteByte (CHT_NOCLIP);
            eatkey = true;
        }
        // 'behold?' power-up cheats
        for (i=0; i<6; i++)
        {
            if (cht_CheckCheat(&cheat_powerup[i], (char)ev->data2))
            {
                if (CheckCheatmode ())
                    return false;

                Printf(PRINT_HIGH, "Power-up toggled\n");
                if (!plyr->powers[i])
                    P_GivePower( plyr, i);
                else if (i!=pw_strength)
                    plyr->powers[i] = 1;
                else
                    plyr->powers[i] = 0;

                MSG_WriteMarker(&net_buffer, clc_cheatpulse);
                MSG_WriteByte(&net_buffer, 3);
                MSG_WriteByte(&net_buffer, (byte)i);

                eatkey = true;
            }
        }

        // 'behold' power-up menu
        if (cht_CheckCheat(&cheat_powerup[6], (char)ev->data2))
        {
            if (CheckCheatmode ())
                return false;

            Printf (PRINT_HIGH, "%s\n", GStrings(STSTR_BEHOLD));

        }

        // 'choppers' invulnerability & chainsaw
        else if (cht_CheckCheat(&cheat_choppers, (char)ev->data2))
        {
            if (CheckCheatmode ())
                return false;

            Printf(PRINT_HIGH, "... Doesn't suck - GM\n");
            plyr->weaponowned[wp_chainsaw] = true;

            MSG_WriteMarker(&net_buffer, clc_cheatpulse);
            MSG_WriteByte(&net_buffer, 4);

            eatkey = true;
        }

        // 'clev' change-level cheat
        else if (cht_CheckCheat(&cheat_clev, (char)ev->data2))
        {
            char buf[16];
			//char *bb;

            cht_GetParam(&cheat_clev, buf);
            buf[2] = 0;

			// [ML] Chex mode: always set the episode number to 1.
			// FIXME: This is probably a horrible hack, it sure looks like one at least
			if (gamemode == retail_chex)
				sprintf(buf,"1%c",buf[1]);

            sprintf (buf + 3, "map %s\n", buf);
            AddCommandString (buf + 3);
            eatkey = true;
        }

        // 'mypos' for player position
        else if (cht_CheckCheat(&cheat_mypos, (char)ev->data2))
        {
            AddCommandString ("toggle idmypos");
            eatkey = true;
        }

        // 'idmus' change-music cheat
        else if (cht_CheckCheat(&cheat_mus, (char)ev->data2))
        {
            char buf[16];

            cht_GetParam(&cheat_mus, buf);
            buf[2] = 0;

            sprintf (buf + 3, "idmus %s\n", buf);
            AddCommandString (buf + 3);
            eatkey = true;
        }
    }

    return eatkey;
}

// Console cheats
BEGIN_COMMAND (god)
{
	if (CheckCheatmode ())
		return;

	consoleplayer().cheats ^= CF_GODMODE;

    if (consoleplayer().cheats & CF_GODMODE)
        Printf(PRINT_HIGH, "Degreelessness mode on\n");
    else
        Printf(PRINT_HIGH, "Degreelessness mode off\n");

	MSG_WriteMarker(&net_buffer, clc_cheat);
	MSG_WriteByte(&net_buffer, consoleplayer().cheats);
}
END_COMMAND (god)

BEGIN_COMMAND (notarget)
{
	if (CheckCheatmode () || connected)
		return;

	consoleplayer().cheats ^= CF_NOTARGET;

    if (consoleplayer().cheats & CF_NOTARGET)
        Printf(PRINT_HIGH, "Notarget on\n");
    else
        Printf(PRINT_HIGH, "Notarget off\n");

	MSG_WriteMarker(&net_buffer, clc_cheat);
	MSG_WriteByte(&net_buffer, consoleplayer().cheats);
}
END_COMMAND (notarget)

BEGIN_COMMAND (fly)
{
	if (!consoleplayer().spectator && CheckCheatmode ())
		return;

	consoleplayer().cheats ^= CF_FLY;

    if (consoleplayer().cheats & CF_FLY)
        Printf(PRINT_HIGH, "Fly mode on\n");
    else
        Printf(PRINT_HIGH, "Fly mode off\n");

	if (!consoleplayer().spectator)
	{
		MSG_WriteMarker(&net_buffer, clc_cheat);
		MSG_WriteByte(&net_buffer, consoleplayer().cheats);
	}
}
END_COMMAND (fly)

BEGIN_COMMAND (noclip)
{
	if (CheckCheatmode ())
		return;

	consoleplayer().cheats ^= CF_NOCLIP;

    if (consoleplayer().cheats & CF_NOCLIP)
        Printf(PRINT_HIGH, "No clipping mode on\n");
    else
        Printf(PRINT_HIGH, "No clipping mode off\n");

	MSG_WriteMarker(&net_buffer, clc_cheat);
	MSG_WriteByte(&net_buffer, consoleplayer().cheats);
}
END_COMMAND (noclip)

EXTERN_CVAR (chasedemo)

BEGIN_COMMAND (chase)
{
	if (demoplayback)
	{
		if (chasedemo)
		{
			chasedemo.Set (0.0f);
			for (Players::iterator it = players.begin();it != players.end();++it)
				it->cheats &= ~CF_CHASECAM;
		}
		else
		{
			chasedemo.Set (1.0f);
			for (Players::iterator it = players.begin();it != players.end();++it)
				it->cheats |= CF_CHASECAM;
		}
	}
	else
	{
		if (CheckCheatmode ())
			return;

		consoleplayer().cheats ^= CF_CHASECAM;

		MSG_WriteMarker(&net_buffer, clc_cheat);
		MSG_WriteByte(&net_buffer, consoleplayer().cheats);
	}
}
END_COMMAND (chase)

BEGIN_COMMAND (idmus)
{
	level_info_t *info;
	char *map;
	int l;

	if (argc > 1)
	{
		if (gameinfo.flags & GI_MAPxx)
		{
			l = atoi (argv[1]);
			if (l <= 99)
				map = CalcMapName (0, l);
			else
			{
				Printf (PRINT_HIGH, "%s\n", GStrings(STSTR_NOMUS));
				return;
			}
		}
		else
		{
			map = CalcMapName (argv[1][0] - '0', argv[1][1] - '0');
		}

		if ( (info = FindLevelInfo (map)) )
		{
			if (info->music[0])
			{
				S_ChangeMusic (std::string(info->music, 8), 1);
				Printf (PRINT_HIGH, "%s\n", GStrings(STSTR_MUS));
			}
		} else
			Printf (PRINT_HIGH, "%s\n", GStrings(STSTR_NOMUS));
	}
}
END_COMMAND (idmus)

BEGIN_COMMAND (give)
{
	if (CheckCheatmode ())
		return;

	if (argc < 2)
		return;

	std::string name = C_ArgCombine(argc - 1, (const char **)(argv + 1));
	if (name.length())
	{
		//Net_WriteByte (DEM_GIVECHEAT);
		//Net_WriteString (name.c_str());
		// todo
	}
}
END_COMMAND (give)

BEGIN_COMMAND (fov)
{
	if (CheckCheatmode () || !m_Instigator)
		return;

	if (argc != 2)
		Printf (PRINT_HIGH, "fov is %g\n", m_Instigator->player->fov);
	else
		m_Instigator->player->fov = atof (argv[1]);
}
END_COMMAND (fov)


int ST_calcPainOffset(void)
{
	int 		health;
	static int	lastcalc;
	static int	oldhealth = -1;

	health = displayplayer().health;

	if(health < -1)
		health = -1;
	else if(health > 100)
		health = 100;

	if (health != oldhealth)
	{
		lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
		oldhealth = health;
	}

	return lastcalc;
}


//
// This is a not-very-pretty routine which handles
//	the face states and their timing.
// the precedence of expressions is:
//	dead > evil grin > turned head > straight ahead
//
void ST_updateFaceWidget(void)
{
	int 		i;
	angle_t 	badguyangle;
	angle_t 	diffang;
	static int	lastattackdown = -1;
	static int	priority = 0;
	BOOL	 	doevilgrin;

	player_t *plyr = &displayplayer();

	if (priority < 10)
	{
		// dead
		if (!plyr->health)
		{
			priority = 9;
			st_faceindex = ST_DEADFACE;
			st_facecount = 1;
		}
	}

	if (priority < 9)
	{
		if (plyr->bonuscount)
		{
			// picking up bonus
			doevilgrin = false;

			for (i=0;i<NUMWEAPONS;i++)
			{
				if (oldweaponsowned[i] != plyr->weaponowned[i])
				{
					doevilgrin = true;
					oldweaponsowned[i] = plyr->weaponowned[i];
				}
			}
			if (doevilgrin)
			{
				// evil grin if just picked up weapon
				priority = 8;
				st_facecount = ST_EVILGRINCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
			}
		}

	}

	if (priority < 8)
	{
		if (plyr->damagecount
			&& plyr->attacker
			&& plyr->attacker != plyr->mo)
		{
			// being attacked
			priority = 7;

			if (st_oldhealth - plyr->health > ST_MUCHPAIN)
			{
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
			}
			else
			{
				badguyangle = R_PointToAngle2(plyr->mo->x,
											  plyr->mo->y,
											  plyr->attacker->x,
											  plyr->attacker->y);

				if (badguyangle > plyr->mo->angle)
				{
					// whether right or left
					diffang = badguyangle - plyr->mo->angle;
					i = diffang > ANG180;
				}
				else
				{
					// whether left or right
					diffang = plyr->mo->angle - badguyangle;
					i = diffang <= ANG180;
				} // confusing, aint it?


				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset();

				if (diffang < ANG45)
				{
					// head-on
					st_faceindex += ST_RAMPAGEOFFSET;
				}
				else if (i)
				{
					// turn face right
					st_faceindex += ST_TURNOFFSET;
				}
				else
				{
					// turn face left
					st_faceindex += ST_TURNOFFSET+1;
				}
			}
		}
	}

	if (priority < 7)
	{
		// getting hurt because of your own damn stupidity
		if (plyr->damagecount)
		{
			if (st_oldhealth - plyr->health > ST_MUCHPAIN)
			{
				priority = 7;
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
			}
			else
			{
				priority = 6;
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
			}

		}

	}

	if (priority < 6)
	{
		// rapid firing
		if (plyr->attackdown)
		{
			if (lastattackdown==-1)
				lastattackdown = ST_RAMPAGEDELAY;
			else if (!--lastattackdown)
			{
				priority = 5;
				st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
				st_facecount = 1;
				lastattackdown = 1;
			}
		}
		else
			lastattackdown = -1;

	}

	if (priority < 5)
	{
		// invulnerability
		if ((plyr->cheats & CF_GODMODE)
			|| plyr->powers[pw_invulnerability])
		{
			priority = 4;

			st_faceindex = ST_GODFACE;
			st_facecount = 1;

		}
	}

	// look left or look right if the facecount has timed out
	if (!st_facecount)
	{
		st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
		st_facecount = ST_STRAIGHTFACECOUNT;
		priority = 0;
	}

	st_facecount--;
}

void ST_updateWidgets(void)
{
	static int	largeammo = 1994; // means "n/a"
	int 		i;

	player_t *plyr = &displayplayer();

	// must redirect the pointer if the ready weapon has changed.
	//	if (w_ready.data != plyr->readyweapon)
	//	{
	if (weaponinfo[plyr->readyweapon].ammotype == am_noammo)
		w_ready.num = &largeammo;
	else
		w_ready.num = &plyr->ammo[weaponinfo[plyr->readyweapon].ammotype];
	//{
	// static int tic=0;
	// static int dir=-1;
	// if (!(tic&15))
	//	 plyr->ammo[weaponinfo[plyr->readyweapon].ammotype]+=dir;
	// if (plyr->ammo[weaponinfo[plyr->readyweapon].ammotype] == -100)
	//	 dir = 1;
	// tic++;
	// }
	w_ready.data = plyr->readyweapon;

	st_health = plyr->health;
	st_armor = plyr->armorpoints;

	for(i = 0; i < 4; i++)
	{
		st_ammo[i] = plyr->ammo[i];
		st_maxammo[i] = plyr->maxammo[i];
	}
	for(i = 0; i < 6; i++)
	{
		// denis - longwinded so compiler optimization doesn't skip it (fault in my gcc?)
		if(plyr->weaponowned[i+1])
			st_weaponowned[i] = 1;
		else
			st_weaponowned[i] = 0;
	}

	st_current_ammo = plyr->ammo[weaponinfo[plyr->readyweapon].ammotype];
	// if (*w_ready.on)
	//	STlib_updateNum(&w_ready, true);
	// refresh weapon change
	//	}

	// update keycard multiple widgets
	for (i=0;i<3;i++)
	{
		keyboxes[i] = plyr->cards[i] ? i : -1;

		// [RH] show multiple keys per box, too
		if (plyr->cards[i+3])
			keyboxes[i] = (keyboxes[i] == -1) ? i+3 : i+6;
	}

	// refresh everything if this is him coming back to life
	ST_updateFaceWidget();

	// used by w_arms[] widgets
	st_armson = st_statusbaron && sv_gametype == GM_COOP;

	// used by w_frags widget
	st_fragson = sv_gametype != GM_COOP && st_statusbaron;

	//	[Toke - CTF]
	if (sv_gametype == GM_CTF)
		st_fragscount = TEAMpoints[plyr->userinfo.team]; // denis - todo - scoring for ctf
	else
		st_fragscount = plyr->fragcount;	// [RH] Just use cumulative total

	if (sv_gametype == GM_CTF) {
		switch(CTFdata[it_blueflag].state)
		{
			case flag_home:
				st_flagboxbluon = true;
				break;
			case flag_carried:
				st_flagboxbluon = false;
				break;
			case flag_dropped:
				CTFdata[it_blueflag].sb_tick++;

				if (CTFdata[it_blueflag].sb_tick == 10)
					CTFdata[it_blueflag].sb_tick = 0;

				if (CTFdata[it_blueflag].sb_tick < 8)
					st_flagboxbluon = false;
				else
					st_flagboxbluon = true;
				break;
			default:
				break;
		}
		switch(CTFdata[it_redflag].state)
		{
			case flag_home:
				st_flagboxredon = true;
				break;
			case flag_carried:
				st_flagboxredon = false;
				break;
			case flag_dropped:
				CTFdata[it_redflag].sb_tick++;

				if (CTFdata[it_redflag].sb_tick == 10)
					CTFdata[it_redflag].sb_tick = 0;

				if (CTFdata[it_redflag].sb_tick < 8)
					st_flagboxredon = false;
				else
					st_flagboxredon = true;
				break;
			default:
				break;
		}
	}

	// get rid of chat window if up because of message
	if (!--st_msgcounter)
		st_chat = st_oldchat;

}

void ST_Ticker (void)
{
	st_randomnumber = M_Random();
	ST_updateWidgets();
	st_oldhealth = displayplayer().health;
}


void ST_drawWidgets(bool refresh)
{
	// used by w_arms[] widgets
	st_armson = st_statusbaron && sv_gametype == GM_COOP;

	// used by w_frags widget
	st_fragson = sv_gametype != GM_COOP && st_statusbaron;

	STlib_updateNum(&w_ready, refresh);

	for (int i = 0; i < 4; i++)
	{
		STlib_updateNum(&w_ammo[i], refresh);
		STlib_updateNum(&w_maxammo[i], refresh);
	}

	STlib_updatePercent(&w_health, refresh);
	STlib_updatePercent(&w_armor, refresh);

	for (int i = 0; i < 6; i++)
		STlib_updateMultIcon(&w_arms[i], refresh);

	STlib_updateMultIcon(&w_faces, refresh);

	if (sv_gametype != GM_CTF) // [Toke - CTF] Dont display keys in ctf mode
		for (int i = 0; i < 3; i++)
			STlib_updateMultIcon(&w_keyboxes[i], refresh);

	STlib_updateNum(&w_frags, refresh);

	if (sv_gametype == GM_CTF)
	{
		STlib_updateBinIcon(&w_flagboxblu, refresh);
		STlib_updateBinIcon(&w_flagboxred, refresh);
	}
}

void ST_doRefresh(void)
{
	st_firsttime = false;

	// draw status bar background to off-screen buff
	ST_refreshBackground();

	// and refresh all widgets
	ST_drawWidgets(true);
}

void ST_diffDraw(void)
{
	// update all widgets
	ST_drawWidgets(false);
}

void ST_Drawer (void)
{
	if (noisedebug)
		S_NoiseDebug ();

	bool spechud = consoleplayer().spectator && consoleplayer_id == displayplayer_id;

	if ((realviewheight == screen->height && viewactive) || spechud)
	{
		if (screenblocks < 12)
		{
			if (spechud)
				hud::SpectatorHUD();
			else if (hud_fullhudtype >= 1)
				hud::OdamexHUD();
			else
				hud::ZDoomHUD();
		}

		st_firsttime = true;
	}
	else
	{
		if (st_firsttime)
			ST_doRefresh();
		else
			ST_diffDraw();

		hud::DoomHUD();
	}

	// [AM] Voting HUD!
	ST_voteDraw(11 * CleanYfac);

	// [RH] Hey, it's somewhere to put the idmypos stuff!
	if (idmypos)
		Printf (PRINT_HIGH, "ang=%d;x,y,z=(%d,%d,%d)\n",
				displayplayer().camera->angle/FRACUNIT,
				displayplayer().camera->x/FRACUNIT,
				displayplayer().camera->y/FRACUNIT,
				displayplayer().camera->z/FRACUNIT);
}

void ST_loadData (void)
{
    lu_palette = W_GetNumForName ("PLAYPAL");

	ST_InitSprites();
}

void ST_unloadData(void)
{
	ST_ShutdownSprites();
}

void ST_initData(void)
{
	st_firsttime = true;

	st_gamestate = FirstPersonState;

	st_statusbaron = true;
	st_oldchat = st_chat = false;
	st_cursoron = false;

	st_faceindex = 0;

	st_oldhealth = -1;

	for (int i = 0; i < NUMWEAPONS; i++)
		oldweaponsowned[i] = displayplayer().weaponowned[i];

	for (int i = 0; i < 3; i++)
		keyboxes[i] = -1;

	STlib_init();
}



void ST_createWidgets(void)
{
	// ready weapon ammo
	STlib_initNum(&w_ready,
				  ST_AMMOX,
				  ST_AMMOY,
				  tallnum,
				  &st_current_ammo,
				  &st_statusbaron,
				  ST_AMMOWIDTH,
	              tallminus);

	// health percentage
	STlib_initPercent(&w_health,
					  ST_HEALTHX,
					  ST_HEALTHY,
					  tallnum,
					  &st_health,
					  &st_statusbaron,
	                  tallminus,
					  tallpercent);

	// weapons owned
	for(int i = 0; i < 6; i++)
	{
		STlib_initMultIcon(&w_arms[i],
						   ST_ARMSX+(i%3)*ST_ARMSXSPACE,
						   ST_ARMSY+(i/3)*ST_ARMSYSPACE,
						   arms[i],
						   &st_weaponowned[i],
						   &st_armson);
	}

	// frags sum
	STlib_initNum(&w_frags,
				  ST_FRAGSX,
				  ST_FRAGSY,
				  tallnum,
				  &st_fragscount,
				  &st_fragson,
				  ST_FRAGSWIDTH,
	              tallminus);

	// faces
	STlib_initMultIcon(&w_faces,
					   ST_FACESX,
					   ST_FACESY,
					   faces,
					   &st_faceindex,
					   &st_statusbaron);

	// armor percentage - should be colored later
	STlib_initPercent(&w_armor,
					  ST_ARMORX,
					  ST_ARMORY,
					  tallnum,
					  &st_armor,
					  &st_statusbaron,
	                  tallminus,
	                  tallpercent);

	// keyboxes 0-2
	STlib_initMultIcon(&w_keyboxes[0],
					   ST_KEY0X,
					   ST_KEY0Y,
					   keys,
					   &keyboxes[0],
					   &st_statusbaron);

	STlib_initMultIcon(&w_keyboxes[1],
					   ST_KEY1X,
					   ST_KEY1Y,
					   keys,
					   &keyboxes[1],
					   &st_statusbaron);

	STlib_initMultIcon(&w_keyboxes[2],
					   ST_KEY2X,
					   ST_KEY2Y,
					   keys,
					   &keyboxes[2],
					   &st_statusbaron);

	// flagbox indicator
	STlib_initBinIcon(&w_flagboxblu,
					  ST_FLGBOXBLUX,
					  ST_FLGBOXBLUY,
					  flagboxb,
					  &st_flagboxbluon,
					  &st_statusbaron);

	STlib_initBinIcon(&w_flagboxred,
					  ST_FLGBOXREDX,
					  ST_FLGBOXREDY,
					  flagboxr,
					  &st_flagboxredon,
					  &st_statusbaron);

	// ammo count (all four kinds)
	STlib_initNum(&w_ammo[0],
				  ST_AMMO0X,
				  ST_AMMO0Y,
				  shortnum,
				  &st_ammo[0],
				  &st_statusbaron,
				  ST_AMMO0WIDTH,
	              tallminus);

	STlib_initNum(&w_ammo[1],
				  ST_AMMO1X,
				  ST_AMMO1Y,
				  shortnum,
				  &st_ammo[1],
				  &st_statusbaron,
				  ST_AMMO1WIDTH,
	              tallminus);

	STlib_initNum(&w_ammo[2],
				  ST_AMMO2X,
				  ST_AMMO2Y,
				  shortnum,
				  &st_ammo[2],
				  &st_statusbaron,
				  ST_AMMO2WIDTH,
	              tallminus);

	STlib_initNum(&w_ammo[3],
				  ST_AMMO3X,
				  ST_AMMO3Y,
				  shortnum,
				  &st_ammo[3],
				  &st_statusbaron,
				  ST_AMMO3WIDTH,
	              tallminus);

	// max ammo count (all four kinds)
	STlib_initNum(&w_maxammo[0],
				  ST_MAXAMMO0X,
				  ST_MAXAMMO0Y,
				  shortnum,
				  &st_maxammo[0],
				  &st_statusbaron,
				  ST_MAXAMMO0WIDTH,
	              tallminus);

	STlib_initNum(&w_maxammo[1],
				  ST_MAXAMMO1X,
				  ST_MAXAMMO1Y,
				  shortnum,
				  &st_maxammo[1],
				  &st_statusbaron,
				  ST_MAXAMMO1WIDTH,
	              tallminus);

	STlib_initNum(&w_maxammo[2],
				  ST_MAXAMMO2X,
				  ST_MAXAMMO2Y,
				  shortnum,
				  &st_maxammo[2],
				  &st_statusbaron,
				  ST_MAXAMMO2WIDTH,
	              tallminus);

	STlib_initNum(&w_maxammo[3],
				  ST_MAXAMMO3X,
				  ST_MAXAMMO3Y,
				  shortnum,
				  &st_maxammo[3],
				  &st_statusbaron,
				  ST_MAXAMMO3WIDTH,
	              tallminus);

}

static bool	st_stopped = true;

void ST_Start (void)
{
	if (!st_stopped)
		ST_Stop();

	ST_initData();
	ST_createWidgets();
	st_stopped = false;
	st_firsttime = true;
	st_scale.Set (st_scale.cstring());
}

void ST_Stop (void)
{
	if (st_stopped)
		return;

	V_SetBlend (0,0,0,0);

	st_stopped = true;
}

void ST_Init()
{
	ST_loadData();
}

void ST_Shutdown()
{
}

VERSION_CONTROL (st_stuff_cpp, "$Id$")


