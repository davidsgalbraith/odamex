// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: sv_mobj.cpp 1832 2010-09-01 23:59:33Z mike $
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2015 by The Odamex Team.
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
//	Moving object handling. Spawn functions.
//
//-----------------------------------------------------------------------------

#include "m_alloc.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sound.h"
#include "doomstat.h"
#include "doomtype.h"
#include "v_video.h"
#include "c_cvars.h"
#include "m_vectors.h"
#include "p_mobj.h"
#include "sv_main.h"
#include "p_ctf.h"
#include "g_game.h"
#include "p_acs.h"

EXTERN_CVAR(sv_nomonsters)
EXTERN_CVAR(sv_maxplayers)

void G_PlayerReborn(player_t &player);
void CTF_RememberFlagPos(mapthing2_t *mthing);

void P_SetSpectatorFlags(player_t &player)
{
	player.spectator = true;

	if (player.mo)
	{
		player.mo->flags |= MF_SPECTATOR;
		player.mo->flags &= ~MF_SOLID;
		player.mo->flags2 |= MF2_FLY;
	}
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//	between levels.
//
void P_SpawnPlayer(player_t& player, mapthing2_t* mthing)
{
	// denis - clients should not control spawning
	if (!serverside)
		return;

	// [RH] Things 4001-? are also multiplayer starts. Just like 1-4.
	//		To make things simpler, figure out which player is being
	//		spawned here.

	// not playing?
	if (!player.ingame())
		return;

	if (player.playerstate == PST_REBORN || player.playerstate == PST_ENTER)
		G_PlayerReborn(player);

	AActor* mobj;
//	if (player.deadspectator && player.mo)
//		mobj = new AActor(player.mo->x, player.mo->y, ONFLOORZ, MT_PLAYER);
//	else
//		mobj = new AActor(mthing->x << FRACBITS, mthing->y << FRACBITS, ONFLOORZ, MT_PLAYER);
	mobj = new AActor(mthing->x << FRACBITS, mthing->y << FRACBITS, ONFLOORZ, MT_PLAYER);

	// set color translations for player sprites
	// [RH] Different now: MF_TRANSLATION is not used.
	//		  mobj->translation = translationtables + 256*playernum;

//	if (player.deadspectator && player.mo)
//	{
//		mobj->angle = player.mo->angle;
//		mobj->pitch = player.mo->pitch;
//	}
//	else
//	{
//		mobj->angle = ANG45 * (mthing->angle/45);
//		mobj->pitch = 0;
//	}
	mobj->angle = ANG45 * (mthing->angle/45);
	mobj->pitch = 0;


	mobj->pitch = 0;
	mobj->player = &player;
	mobj->health = player.health;

	player.fov = 90.0f;
	player.mo = player.camera = mobj->ptr();
	player.playerstate = PST_LIVE;
	player.refire = 0;
	player.damagecount = 0;
	player.bonuscount = 0;
	player.extralight = 0;
	player.fixedcolormap = 0;
	player.viewheight = VIEWHEIGHT;
	player.xviewshift = 0;
	player.attacker = AActor::AActorPtr();

	// Set up some special spectator stuff
	if (player.spectator)
		P_SetSpectatorFlags(player);

	// [RH] Allow chasecam for demo watching
	//if ((demoplayback || demonew) && chasedemo)
	//	player.cheats = CF_CHASECAM;

	// setup gun psprite
	P_SetupPsprites(&player);

	// give all cards in death match mode
	if (sv_gametype != GM_COOP)
	{
		for (int i = 0; i < NUMCARDS; i++)
			player.cards[i] = true;
	}

	if (serverside)
	{
		// [RH] If someone is in the way, kill them
		P_TeleportMove(mobj, mobj->x, mobj->y, mobj->z, true);

		// [BC] Do script stuff
		if (level.behavior)
		{
			if (player.playerstate == PST_ENTER)
				level.behavior->StartTypedScripts(SCRIPT_Enter, player.mo);
			else if (player.playerstate == PST_REBORN)
				level.behavior->StartTypedScripts(SCRIPT_Respawn, player.mo);
		}

		// send new objects
		SV_SpawnMobj(mobj);
	}
}

/**
 * Stub
 */
void P_ShowSpawns(mapthing2_t* mthing) { }

VERSION_CONTROL (sv_mobj_cpp, "$Id: sv_mobj.cpp 1832 2010-09-01 23:59:33Z mike $")

