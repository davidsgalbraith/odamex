// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1998-2006 by Randy Heit (ZDoom).
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
//		Game completion, final screen animation.
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "i_system.h"
#include "m_swap.h"
#include "z_zone.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "s_sound.h"
#include "gstrings.h"
#include "doomstat.h"
#include "r_state.h"
#include "hu_stuff.h"

#include "gi.h"

// Stage of animation:
//	0 = text, 1 = art screen, 2 = character cast
unsigned int	finalestage;

int	finalecount;

#define TEXTSPEED		2
#define TEXTWAIT		250

static const char*	finaletext;
static const char*	finaleflat;

void	F_StartCast (void);
void	F_CastTicker (void);
BOOL	F_CastResponder (event_t *ev);
void	F_CastDrawer (void);

//
// F_StartFinale
//
void F_StartFinale (char *music, char *flat, const char *text)
{
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;

	// Okay - IWAD dependend stuff.
	// This has been changed severly, and
	//	some stuff might have changed in the process.
	// [RH] More flexible now (even more severe changes)
	//  finaleflat, finaletext, and music are now
	//  determined in G_WorldDone() based on data in
	//  a level_info_t and a cluster_info_t.

	if (*music == 0)
		S_ChangeMusic (std::string(gameinfo.finaleMusic, 8),
			!(gameinfo.flags & GI_NOLOOPFINALEMUSIC));
	else
		S_ChangeMusic (std::string(music, 8), !(gameinfo.flags & GI_NOLOOPFINALEMUSIC));

	if (*flat == 0)
		finaleflat = gameinfo.finaleFlat;
	else
		finaleflat = flat;

	if (text)
		finaletext = text;
	else
		finaletext = "Empty message";

	finalestage = 0;
	finalecount = 0;
	V_SetBlend (0,0,0,0);
	S_StopAllChannels ();
}



BOOL F_Responder (event_t *event)
{
	if (finalestage == 2)
		return F_CastResponder (event);

	return false;
}


//
// F_Ticker
//
void F_Ticker (void)
{
	// denis - do this serverside only
	// check for skipping
	// [RH] Non-commercial can be skipped now, too
	if (serverside && finalecount > 50 && finalestage != 1) {
		// go on to the next level
		// [RH] or just reveal the entire message if we're still ticking it
		Players::iterator it = players.begin();
		for (;it != players.end();++it)
			if (it->cmd.buttons)
				break;

		if (it != players.end())
		{
			/*if (finalecount < (signed)(strlen (finaletext)*TEXTSPEED))
			{
				finalecount = strlen (finaletext)*TEXTSPEED;
			}
			else
			{*/
				if (!strncmp (level.nextmap, "EndGame", 7) ||
				(gamemode == retail_chex && !strncmp (level.nextmap, "E1M6", 4)))  	// [ML] Chex mode: game is over
				{																	// after E1M5
					if (level.nextmap[7] == 'C')
					{
						F_StartCast ();
					}
					else
					{
						finalecount = 0;
						finalestage = 1;
						wipegamestate = GS_FORCEWIPE;
						if (level.nextmap[7] == '3')
							S_StartMusic ("d_bunny");
					}
				}
				else
				{
					gameaction = ga_worlddone;
				}
			//}
		}
	}

	// advance animation
	finalecount++;

	if (finalestage == 2)
	{
		F_CastTicker ();
		return;
	}
}



//
// F_TextWrite
//
void F_TextWrite (void)
{
	const char*	ch;
	int 		c;
	int 		cx;
	int 		cy;

	screen->Clear(0, 0, screen->width, screen->height, 0);

	int background_width = screen->width;
	int background_height = screen->height;
	if (!screen->isProtectedRes() && screen->width * 3 > screen->height * 4)
		background_width = 4 * screen->height / 3;

	// center the background and text in 4:3 perspective
	int x1 = (screen->width - background_width) / 2;
	int y1 = (screen->height - background_height) / 2;
	int x2 = x1 + background_width - 1;
	int y2 = y1 + background_height - 1;

	// erase the entire screen to a tiled background
	texhandle_t flat_texhandle = texturemanager.getHandle(finaleflat, Texture::TEX_FLAT);
	if (flat_texhandle != TextureManager::NOT_FOUND_TEXTURE_HANDLE)
	{
		const Texture* flat_texture = texturemanager.getTexture(flat_texhandle);
		screen->FlatFill(flat_texture, x1, y1, x2, y2);
	}

	V_MarkRect(0, 0, screen->width, screen->height);

	// draw some of the text onto the screen
	const int initial_cx = 10 * CleanXfac + x1;
	const int initial_cy = 10 * CleanYfac + y1;
	const int cy_inc = 11 * CleanYfac;

	cx = initial_cx; 
	cy = initial_cy; 
	ch = finaletext;

	if (finalecount < 11)
		return;

	for (int count = (finalecount - 10) / TEXTSPEED; count; count--)
	{
		c = *ch++;
		if (!c)
			break;
		if (c == '\n')
		{
			cx = initial_cx;
			cy += cy_inc;
			continue;
		}

		int charwidth = doom_font->getTextWidth(c);
		if (cx + charwidth > x2)
			break;
		
		char dummy_str[2] = { c, 0 };
		doom_font->printText(screen, cx, cy, CR_RED, dummy_str);
		cx += charwidth;
	}
}

//
// Final DOOM 2 animation
// Casting by id Software.
//	 in order of appearance
//
typedef struct
{
	const char		*name;
	mobjtype_t	type;
} castinfo_t;

castinfo_t		castorder[] = {
	{NULL, MT_POSSESSED},
	{NULL, MT_SHOTGUY},
	{NULL, MT_CHAINGUY},
	{NULL, MT_TROOP},
	{NULL, MT_SERGEANT},
	{NULL, MT_SKULL},
	{NULL, MT_HEAD},
	{NULL, MT_KNIGHT},
	{NULL, MT_BRUISER},
	{NULL, MT_BABY},
	{NULL, MT_PAIN},
	{NULL, MT_UNDEAD},
	{NULL, MT_FATSO},
	{NULL, MT_VILE},
	{NULL, MT_SPIDER},
	{NULL, MT_CYBORG},
	{NULL, MT_PLAYER},

	{NULL, MT_UNKNOWNTHING}
};

int 			castnum;
int 			casttics;
int				castsprite;	// [RH] For overriding the player sprite with a skin
state_t*		caststate;
BOOL	 		castdeath;
int 			castframes;
int 			castonmelee;
BOOL	 		castattacking;


//
// F_StartCast
//
extern	gamestate_t 	wipegamestate;


void F_StartCast (void)
{
	// [RH] Set the names for the cast
	castorder[0].name = GStrings(CC_ZOMBIE);
	castorder[1].name = GStrings(CC_SHOTGUN);
	castorder[2].name = GStrings(CC_HEAVY);
	castorder[3].name = GStrings(CC_IMP);
	castorder[4].name = GStrings(CC_DEMON);
	castorder[5].name = GStrings(CC_LOST);
	castorder[6].name = GStrings(CC_CACO);
	castorder[7].name = GStrings(CC_HELL);
	castorder[8].name = GStrings(CC_BARON);
	castorder[9].name = GStrings(CC_ARACH);
	castorder[10].name = GStrings(CC_PAIN);
	castorder[11].name = GStrings(CC_REVEN);
	castorder[12].name = GStrings(CC_MANCU);
	castorder[13].name = GStrings(CC_ARCH);
	castorder[14].name = GStrings(CC_SPIDER);
	castorder[15].name = GStrings(CC_CYBER);
	castorder[16].name = GStrings(CC_HERO);

	wipegamestate = GS_FORCEWIPE;
	castnum = 0;
	caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	if (castorder[castnum].type == MT_PLAYER)
		castsprite = skins[consoleplayer().userinfo.skin].sprite;
	else
		castsprite = caststate->sprite;
	casttics = caststate->tics;
	castdeath = false;
	finalestage = 2;
	castframes = 0;
	castonmelee = 0;
	castattacking = false;
	S_ChangeMusic("d_evil", true);
}


//
// F_CastTicker
//
void F_CastTicker (void)
{
	int st;
	int atten;

	if (--casttics > 0)
		return; 				// not time to change state yet

	if (caststate->tics == -1 || caststate->nextstate == S_NULL)
	{
		// switch from deathstate to next monster
		castnum++;
		castdeath = false;
		if (castorder[castnum].name == NULL)
			castnum = 0;
		if (mobjinfo[castorder[castnum].type].seesound) {
			atten = ATTN_NONE;
			S_Sound (CHAN_VOICE, mobjinfo[castorder[castnum].type].seesound, 1, atten);
		}
		caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		if (castorder[castnum].type == MT_PLAYER)
			castsprite = skins[consoleplayer().userinfo.skin].sprite;
		else
			castsprite = caststate->sprite;
		castframes = 0;
	}
	else
	{
		const char *sfx;

		// just advance to next state in animation
		if (caststate == &states[S_PLAY_ATK1])
			goto stopattack;	// Oh, gross hack!
		st = caststate->nextstate;
		caststate = &states[st];
		castframes++;

		// sound hacks....
		switch (st)
		{
		  case S_PLAY_ATK1: 	sfx = "weapons/sshotf"; break;
		  case S_POSS_ATK2: 	sfx = "grunt/attack"; break;
		  case S_SPOS_ATK2: 	sfx = "shotguy/attack"; break;
		  case S_VILE_ATK2: 	sfx = "vile/start"; break;
		  case S_SKEL_FIST2:	sfx = "skeleton/swing"; break;
		  case S_SKEL_FIST4:	sfx = "skeleton/melee"; break;
		  case S_SKEL_MISS2:	sfx = "skeleton/attack"; break;
		  case S_FATT_ATK8:
		  case S_FATT_ATK5:
		  case S_FATT_ATK2: 	sfx = "fatso/attack"; break;
		  case S_CPOS_ATK2:
		  case S_CPOS_ATK3:
		  case S_CPOS_ATK4: 	sfx = "chainguy/attack"; break;
		  case S_TROO_ATK3: 	sfx = "imp/attack"; break;
		  case S_SARG_ATK2: 	sfx = "demon/melee"; break;
		  case S_BOSS_ATK2:
		  case S_BOS2_ATK2:
		  case S_HEAD_ATK2: 	sfx = "caco/attack"; break;
		  case S_SKULL_ATK2:	sfx = "skull/melee"; break;
		  case S_SPID_ATK2:
		  case S_SPID_ATK3: 	sfx = "spider/attack"; break;
		  case S_BSPI_ATK2: 	sfx = "baby/attack"; break;
		  case S_CYBER_ATK2:
		  case S_CYBER_ATK4:
		  case S_CYBER_ATK6:	sfx = "weapons/rocklf"; break;
		  case S_PAIN_ATK3: 	sfx = "skull/melee"; break;
		  default: sfx = 0; break;
		}

		if (sfx) {
			S_StopAllChannels ();
			S_Sound (CHAN_WEAPON, sfx, 1, ATTN_NONE);
		}
	}

	if (castframes == 12)
	{
		// go into attack frame
		castattacking = true;
		if (castonmelee)
			caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
		else
			caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
		castonmelee ^= 1;
		if (caststate == &states[S_NULL])
		{
			if (castonmelee)
				caststate=
					&states[mobjinfo[castorder[castnum].type].meleestate];
			else
				caststate=
					&states[mobjinfo[castorder[castnum].type].missilestate];
		}
	}

	if (castattacking)
	{
		if (castframes == 24
			||	caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
		{
		  stopattack:
			castattacking = false;
			castframes = 0;
			caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		}
	}

	casttics = caststate->tics;
	if (casttics == -1)
		casttics = 15;
}


//
// F_CastResponder
//

BOOL F_CastResponder (event_t* ev)
{
	if (ev->type != ev_keydown)
		return false;

	if (castdeath)
		return true;					// already in dying frames

	// go into death frame
	castdeath = true;
	caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
	casttics = caststate->tics;
	castframes = 0;
	castattacking = false;
	if (mobjinfo[castorder[castnum].type].deathsound) {
		if (castorder[castnum].type == MT_PLAYER) {
			static const char sndtemplate[] = "player/%s/death1";
			static const char *genders[] = { "male", "female", "cyborg" };
			char nametest[128];
			int sndnum;

			sprintf (nametest, sndtemplate, skins[consoleplayer().userinfo.skin].name);
			sndnum = S_FindSound (nametest);
			if (sndnum == -1) {
				sprintf (nametest, sndtemplate, genders[consoleplayer().userinfo.gender]);
				sndnum = S_FindSound (nametest);
				if (sndnum == -1)
					sndnum = S_FindSound ("player/male/death1");
			}
			S_SoundID (CHAN_VOICE, sndnum, 1, ATTN_NONE);
		} else
			S_Sound (CHAN_VOICE, mobjinfo[castorder[castnum].type].deathsound, 1, ATTN_NONE);
	}

	return true;
}

//
// F_CastDrawer
//
void F_CastDrawer (void)
{
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;

	texhandle_t background_texhandle = texturemanager.getHandle("BOSSBACK", Texture::TEX_PATCH);
	const Texture* background_texture = texturemanager.getTexture(background_texhandle);
	screen->DrawTextureFullScreen(background_texture);
	
	int textwidth = doom_font->getTextWidth(castorder[castnum].name);
	int x = (screen->width - textwidth) / 2;
	int y = (screen->height * 180) / 200;

	doom_font->printText(screen, x, y, CR_RED, castorder[castnum].name);

	// draw the current frame in the middle of the screen
	sprdef = &sprites[castsprite];
	sprframe = &sprdef->spriteframes[caststate->frame & FF_FRAMEMASK];

	texhandle_t sprite_texhandle = texturemanager.getHandle(sprframe->lump[0], Texture::TEX_SPRITE);
	const Texture* sprite_texture = texturemanager.getTexture(sprite_texhandle);

/*
	// TODO: add flipping drawing function
	if (sprframe->flip[0])
		screen->DrawTextureFlipped(sprite_texture, 160, 170);
	else
		screen->DrawTextureIndirect(sprite_texture, 160, 170);
*/

	screen->DrawTextureIndirect(sprite_texture, 160, 170);
}


//
// F_DrawPatchCol
//

// Palettized version 8bpp

void F_DrawPatchColP (int x, const patch_t *patch, int col, const DCanvas *scrn)
{
	byte*		source;
	byte*		dest;
	byte*		desttop;
	unsigned	count;
	int			repeat;
	int			c;
	unsigned	step;
	unsigned	invstep;
	float		mul;
	float		fx;
	byte		p;
	int			pitch;

	// [RH] figure out how many times to repeat this column
	// (for screens wider than 320 pixels)
	mul = scrn->width / (float)320;
	fx = (float)x;
	repeat = (int)(floor (mul*(fx+1)) - floor(mul*fx));
	if (repeat == 0)
		return;

	// [RH] Remap virtual-x to real-x
	x = (int)floor (mul*x);

	// [RH] Figure out per-row fixed-point step
	step = (200<<16) / scrn->height;
	invstep = (scrn->height<<16) / 200;

	tallpost_t *post = (tallpost_t *)((byte *)patch + LELONG(patch->columnofs[col]));
	desttop = scrn->buffer + x;
	pitch = scrn->pitch;

	// step through the posts in a column
	while (!post->end())
	{
		source = post->data();
		dest = desttop + ((post->topdelta*invstep)>>16)*pitch;
		count = (post->length * invstep) >> 16;
		c = 0;

		switch (repeat) {
			case 1:
				do {
					*dest = source[c>>16];
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 2:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 3:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 4:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest[3] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			default:
				{
					int count2;

					do {
						p = source[c>>16];
						for (count2 = repeat; count2; count2--) {
							dest[count2] = p;
						}
						dest += pitch;
						c += step;
					} while (--count);
				}
				break;
		}

		post = post->next();
	}
}

// Direct version 32bpp:

void F_DrawPatchColD (int x, const patch_t *patch, int col, const DCanvas *scrn)
{
	byte*		source;
	argb_t*		dest;
	argb_t*		desttop;
	unsigned	count;
	int			repeat;
	int			c;
	unsigned	step;
	unsigned	invstep;
	float		mul;
	float		fx;
	argb_t		p;
	int			pitch;

	// [RH] figure out how many times to repeat this column
	// (for screens wider than 320 pixels)
	mul = scrn->width / (float)320;
	fx = (float)x;
	repeat = (int)(floor (mul*(fx+1)) - floor(mul*fx));
	if (repeat == 0)
		return;

	// [RH] Remap virtual-x to real-x
	x = (int)floor (mul*x);

	// [RH] Figure out per-row fixed-point step
	step = (200<<16) / scrn->height;
	invstep = (scrn->height<<16) / 200;

	tallpost_t *post = (tallpost_t *)((byte *)patch + LELONG(patch->columnofs[col]));
	desttop = (argb_t *)scrn->buffer + x;
	pitch = scrn->pitch / sizeof(argb_t);

	shaderef_t pal = shaderef_t(&GetDefaultPalette()->maps, 0);

	// step through the posts in a column
	while (!post->end())
	{
		source = post->data();
		dest = desttop + ((post->topdelta*invstep)>>16)*pitch;
		count = (post->length * invstep) >> 16;
		c = 0;

		switch (repeat) {
			case 1:
				do {
					*dest = pal.shade(source[c>>16]);
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 2:
				do {
					p = pal.shade(source[c>>16]);
					dest[0] = p;
					dest[1] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 3:
				do {
					p = pal.shade(source[c>>16]);
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 4:
				do {
					p = pal.shade(source[c>>16]);
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest[3] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			default:
				{
					int count2;

					do {
						p = pal.shade(source[c>>16]);
						for (count2 = repeat; count2; count2--) {
							dest[count2] = p;
						}
						dest += pitch;
						c += step;
					} while (--count);
				}
				break;
		}

		post = post->next();
	}
}


//
// F_BunnyScroll
//
void F_BunnyScroll (void)
{
	int 		scrolled;
	int 		x;
	patch_t*	p1;
	patch_t*	p2;
	char		name[10];
	int 		stage;
	static int	laststage;

	p1 = W_CachePatch ("PFUB2");
	p2 = W_CachePatch ("PFUB1");

	V_MarkRect (0, 0, screen->width, screen->height);

	scrolled = 320 - (finalecount-230)/2;
	if (scrolled > 320)
		scrolled = 320;
	if (scrolled < 0)
		scrolled = 0;

	if (screen->is8bit())
	{
		for ( x=0 ; x<320 ; x++)
		{
			if (x+scrolled < 320)
				F_DrawPatchColP (x, p1, x+scrolled, screen);
			else
				F_DrawPatchColP (x, p2, x+scrolled - 320, screen);
		}
	}
	else
	{
	for ( x=0 ; x<320 ; x++)
	{
		if (x+scrolled < 320)
				F_DrawPatchColD (x, p1, x+scrolled, screen);
		else
				F_DrawPatchColD (x, p2, x+scrolled - 320, screen);
		}
	}

	if (finalecount < 1130)
		return;
	if (finalecount < 1180)
	{
		screen->DrawPatchIndirect (W_CachePatch ("END0"),
			(320-13*8)/2, (200-8*8)/2);
		laststage = 0;
		return;
	}

	stage = (finalecount-1180) / 5;
	if (stage > 6)
		stage = 6;
	if (stage > laststage)
	{
		S_Sound (CHAN_WEAPON, "weapons/pistol", 1, ATTN_NONE);
		laststage = stage;
	}

	sprintf (name,"END%i",stage);
	screen->DrawPatchIndirect (W_CachePatch (name),
		(320-13*8)/2, (200-8*8)/2);
}


//
// F_Drawer
//
void F_Drawer (void)
{
	switch (finalestage)
	{
		case 0:
			F_TextWrite ();
			break;

		case 1:
			switch (level.nextmap[7])
			{
				default:
				case '1':
					screen->DrawPatchIndirect (W_CachePatch (gameinfo.finalePage1), 0, 0);
					break;
				case '2':
					screen->DrawPatchIndirect (W_CachePatch (gameinfo.finalePage2), 0, 0);
					break;
				case '3':
					F_BunnyScroll ();
					break;
				case '4':
					screen->DrawPatchIndirect (W_CachePatch (gameinfo.finalePage3), 0, 0);
					break;
			}
			break;

		case 2:
			F_CastDrawer ();
			break;
	}
}

VERSION_CONTROL (f_finale_cpp, "$Id$")

