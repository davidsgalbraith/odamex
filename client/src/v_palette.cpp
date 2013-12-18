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
//	V_PALETTE
//
//-----------------------------------------------------------------------------


#include <cstring>
#include <math.h>
#include <cstddef>

#include "doomstat.h"
#include "v_video.h"
#include "v_colormap.h"
#include "v_gamma.h"
#include "m_alloc.h"
#include "r_main.h"		// For lighting constants
#include "w_wad.h"
#include "z_zone.h"
#include "i_video.h"
#include "c_dispatch.h"
#include "g_level.h"
#include "st_stuff.h"

/* Reimplement old way of doing red/gold colors, from Chocolate Doom - ML */

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS		1
#define STARTBONUSPALS		9
#define NUMREDPALS			8
#define NUMBONUSPALS		4
// Radiation suit, green shift.
#define RADIATIONPAL		13

EXTERN_CVAR(gammalevel)
EXTERN_CVAR(vid_gammatype)
EXTERN_CVAR(r_painintensity)
EXTERN_CVAR(sv_allowredscreen)

void BuildColoredLights (byte *maps, int lr, int lg, int lb, int fr, int fg, int fb);
static void DoBlending (argb_t *from, argb_t *to, unsigned count, int tor, int tog, int tob, int toa);
void V_ForceBlend (int blendr, int blendg, int blendb, int blenda);

dyncolormap_t NormalLight;

static int lu_palette;
static int current_palette;
static float current_blend[4];

palette_t DefPal;
palette_t *FirstPal;

argb_t IndexedPalette[256];


/* Current color blending values */
int		BlendR, BlendG, BlendB, BlendA;

/**************************/
/* Gamma correction stuff */
/**************************/

static DoomGammaStrategy doomgammastrat;
static ZDoomGammaStrategy zdoomgammastrat;
GammaStrategy* gammastrat = &doomgammastrat;

CVAR_FUNC_IMPL(vid_gammatype)
{
	int sanitized_var = clamp(var.value(), 0.0f, 1.0f);
	if (var == sanitized_var)
	{
		if (vid_gammatype == GAMMA_ZDOOM)
			gammastrat = &zdoomgammastrat;
		else
			gammastrat = &doomgammastrat;

		gammalevel.Set(gammalevel);
	}
	else
	{
		var.Set(sanitized_var);
	}
}

byte newgamma[256];
static bool gamma_initialized = false;

static void V_UpdateGammaLevel(float level)
{
	static float lastgammalevel = 0.0f;
	static int lasttype = -1;			// ensure this gets set up the first time
	int type = vid_gammatype;

	if (lastgammalevel != level || lasttype != type)
	{
		// Only recalculate the gamma table if the new gamma
		// value is different from the old one.

		lastgammalevel = level;
		lasttype = type;

		gammastrat->generateGammaTable(newgamma, level);
		GammaAdjustPalettes();

		if (!screen)
			return;
		if (screen->is8bit())
			V_ForceBlend(BlendR, BlendG, BlendB, BlendA);
		else
			RefreshPalettes();
	}
}

CVAR_FUNC_IMPL(gammalevel)
{
	float sanitized_var = clamp(var.value(), gammastrat->min(), gammastrat->max());
	if (var == sanitized_var)
		V_UpdateGammaLevel(var);
	else
		var.Set(sanitized_var);
}

BEGIN_COMMAND(bumpgamma)
{
	gammalevel.Set(gammastrat->increment(gammalevel));

	if (gammalevel.value() == gammastrat->min())
	    Printf (PRINT_HIGH, "Gamma correction off\n");
	else
	    Printf (PRINT_HIGH, "Gamma correction level %g\n", gammalevel.value());
}
END_COMMAND(bumpgamma)


// [Russell] - Restore original screen palette from current gamma level
void V_RestoreScreenPalette(void)
{
    if (screen && screen->is8bit())
		V_ForceBlend(BlendR, BlendG, BlendB, BlendA);
}

/****************************/
/* Palette management stuff */
/****************************/

bool InternalCreatePalette (palette_t *palette, const char *name, byte *colors,
							unsigned numcolors, unsigned flags)
{
	unsigned i;

	if (numcolors > 256)
		numcolors = 256;
	else if (numcolors == 0)
		return false;

	strncpy (palette->name.name, name, 8);
	palette->flags = flags;
	palette->usecount = 1;
	palette->maps.colormap = NULL;
	palette->maps.shademap = NULL;

	M_Free(palette->basecolors);

	palette->basecolors = (argb_t *)Malloc (numcolors * 2 * sizeof(argb_t));
	palette->colors = palette->basecolors + numcolors;
	palette->numcolors = numcolors;

	if (numcolors == 1)
		palette->shadeshift = 0;
	else if (numcolors <= 2)
		palette->shadeshift = 1;
	else if (numcolors <= 4)
		palette->shadeshift = 2;
	else if (numcolors <= 8)
		palette->shadeshift = 3;
	else if (numcolors <= 16)
		palette->shadeshift = 4;
	else if (numcolors <= 32)
		palette->shadeshift = 5;
	else if (numcolors <= 64)
		palette->shadeshift = 6;
	else if (numcolors <= 128)
		palette->shadeshift = 7;
	else
		palette->shadeshift = 8;

	for (i = 0; i < numcolors; i++, colors += 3)
		palette->basecolors[i] = MAKERGB(colors[0],colors[1],colors[2]);

	GammaAdjustPalette (palette);

	return true;
}

palette_t *InitPalettes (const char *name)
{
	byte *colors;

	//if (DefPal.usecount)
	//	return &DefPal;

	current_palette = -1;
	current_blend[0] = current_blend[1] = current_blend[2] = current_blend[3] = 255.0f;

    lu_palette = W_GetNumForName ("PLAYPAL");

	if ( (colors = (byte *)W_CacheLumpName (name, PU_CACHE)) )
		if (InternalCreatePalette (&DefPal, name, colors, 256,
									PALETTEF_SHADE|PALETTEF_BLEND|PALETTEF_DEFAULT)) {
			return &DefPal;
		}
	return NULL;
}

palette_t *GetDefaultPalette (void)
{
	return &DefPal;
}

// MakePalette()
//	input: colors: ptr to 256 3-byte RGB values
//		   flags:  the flags for the new palette
//
palette_t *MakePalette (byte *colors, char *name, unsigned flags)
{
	palette_t *pal;

	pal = (palette_t *)Malloc (sizeof (palette_t));

	if (InternalCreatePalette (pal, name, colors, 256, flags)) {
		pal->next = FirstPal;
		pal->prev = NULL;
		FirstPal = pal;

		return pal;
	} else {
		M_Free(pal);
		return NULL;
	}
}

// LoadPalette()
//	input: name:  the name of the palette lump
//		   flags: the flags for the palette
//
//	This function will try and find an already loaded
//	palette and return that if possible.
palette_t *LoadPalette (char *name, unsigned flags)
{
	palette_t *pal;

	if (!(pal = FindPalette (name, flags))) {
		// Palette doesn't already exist. Create a new one.
		byte *colors = (byte *)W_CacheLumpName (name, PU_CACHE);

		pal = MakePalette (colors, name, flags);
	} else {
		pal->usecount++;
	}
	return pal;
}

// LoadAttachedPalette()
//	input: name:  the name of a graphic whose palette should be loaded
//		   type:  the type of graphic whose palette is being requested
//		   flags: the flags for the palette
//
//	This function looks through the PALETTES lump for a palette
//	associated with the given graphic and returns that if possible.
palette_t *LoadAttachedPalette (char *name, int type, unsigned flags);

// FreePalette()
//	input: palette: the palette to free
//
//	This function decrements the palette's usecount and frees it
//	when it hits zero.
void FreePalette (palette_t *palette)
{
	if (!(--palette->usecount)) {
		if (!(palette->flags & PALETTEF_DEFAULT)) {
			if (!palette->prev)
				FirstPal = palette->next;
			else
				palette->prev->next = palette->next;

			M_Free(palette->basecolors);

			M_Free(palette->colormapsbase);

			M_Free(palette);
		}
	}
}


palette_t *FindPalette (char *name, unsigned flags)
{
	palette_t *pal = FirstPal;
	union {
		char	s[9];
		int		x[2];
	} name8;

	int			v1;
	int			v2;

	// make the name into two integers for easy compares
	strncpy (name8.s,name,8);

	v1 = name8.x[0];
	v2 = name8.x[1];

	while (pal) {
		if (pal->name.nameint[0] == v1 && pal->name.nameint[1] == v2) {
			if ((flags == (unsigned)~0) || (flags == pal->flags))
				return pal;
		}
		pal = pal->next;
	}
	return NULL;
}


// This is based (loosely) on the ColorShiftPalette()
// function from the dcolors.c file in the Doom utilities.
static void DoBlending (argb_t *from, argb_t *to, unsigned count, int tor, int tog, int tob, int toa)
{
	if (toa == 0)
	{
		if (from != to)
			memcpy(to, from, count * sizeof(argb_t));
	}
	else
	{
		for (unsigned i = 0; i < count; i++)
		{
			int r = RPART(*from);
			int g = GPART(*from);
			int b = BPART(*from);
			from++;
			int dr = tor - r;
			int dg = tog - g;
			int db = tob - b;
			*to++ = MAKERGB (r + ((dr*toa)>>8),
							 g + ((dg*toa)>>8),
							 b + ((db*toa)>>8));
		}
	}
}

static void DoBlendingWithGamma (DWORD *from, DWORD *to, unsigned count, int tor, int tog, int tob, int toa)
{
	for (unsigned i = 0; i < count; i++)
	{
		int r = RPART(*from);
		int g = GPART(*from);
		int b = BPART(*from);
		from++;
		int dr = tor - r;
		int dg = tog - g;
		int db = tob - b;
		*to++ = MAKERGB (newgamma[r + ((dr*toa)>>8)],
						 newgamma[g + ((dg*toa)>>8)],
						 newgamma[b + ((db*toa)>>8)]);
	}
}


argb_t V_LightWithGamma(const dyncolormap_t* dyncolormap, argb_t color, int intensity)
{
	argb_t lightcolor;

	if (dyncolormap)
		lightcolor = dyncolormap->color;			// use dynamic lighting if availible
	else
		lightcolor = MAKERGB(0xFF, 0xFF, 0xFF);		// white light

	return MAKERGB(
		newgamma[(RPART(color) * RPART(lightcolor) * intensity) >> 16],
		newgamma[(GPART(color) * GPART(lightcolor) * intensity) >> 16],
		newgamma[(BPART(color) * BPART(lightcolor) * intensity) >> 16]);
}



static const float lightScale(float a)
{
	// NOTE(jsd): Revised inverse logarithmic scale; near-perfect match to COLORMAP lump's scale
	// 1 - ((Exp[1] - Exp[a*2 - 1]) / (Exp[1] - Exp[-1]))
	static float e1 = exp(1.0f);
	static float e1sube0 = e1 - exp(-1.0f);

	float newa = clamp(1.0f - (e1 - (float)exp(a * 2.0f - 1.0f)) / e1sube0, 0.0f, 1.0f);
	return newa;
}


//
// V_FindDynamicColormap
//
// Finds the dynamic colormap that contains shademap
//
dyncolormap_t* V_FindDynamicColormap(const shademap_t* shademap)
{
	if (shademap != &GetDefaultPalette()->maps)
	{
		// Find the dynamic colormap by the shademap pointer:
		dyncolormap_t* colormap = &NormalLight;

		do
		{
			if (shademap == colormap->maps.map())
				return colormap;
		} while ( (colormap = colormap->next) );
	}

	return NULL;
}


void BuildLightRamp (shademap_t &maps)
{
	int l;
	// Build light ramp:
	for (l = 0; l < 256; ++l)
	{
		int a = (int)(255 * lightScale(l / 255.0f));
		maps.ramp[l] = a;
	}
}

void BuildDefaultColorAndShademap (palette_t *pal, shademap_t &maps)
{
	byte  *color;
	argb_t *shade;
	argb_t colors[256];

	unsigned int r = RPART (level.fadeto);
	unsigned int g = GPART (level.fadeto);
	unsigned int b = BPART (level.fadeto);

	BuildLightRamp(maps);

	// build normal light mappings
	for (unsigned int i = 0; i < NUMCOLORMAPS; i++)
	{
		byte a = maps.ramp[i * 255 / NUMCOLORMAPS];

		DoBlending          (pal->basecolors, colors, pal->numcolors, r, g, b, a);
		DoBlendingWithGamma (colors, maps.shademap + (i << pal->shadeshift), pal->numcolors, r, g, b, a);

		color = maps.colormap + (i << pal->shadeshift);
		for (unsigned int j = 0; j < pal->numcolors; j++)
		{
			color[j] = BestColor(
					pal->basecolors,
			  		RPART(colors[j]),
					GPART(colors[j]),
					BPART(colors[j]),
					pal->numcolors);
		}
	}

	// build special maps (e.g. invulnerability)
	int grayint;
	color = maps.colormap + (NUMCOLORMAPS << pal->shadeshift);
	shade = maps.shademap + (NUMCOLORMAPS << pal->shadeshift);

	for (unsigned int i = 0; i < pal->numcolors; i++)
	{
		grayint = (int)(255.0f * clamp(1.0f -
						(RPART(pal->basecolors[i]) * 0.00116796875f +
						 GPART(pal->basecolors[i]) * 0.00229296875f +
			 			 BPART(pal->basecolors[i]) * 0.0005625f), 0.0f, 1.0f));
		const int graygammaint = newgamma[grayint];
		color[i] = BestColor (pal->basecolors, grayint, grayint, grayint, pal->numcolors);
		shade[i] = MAKERGB(graygammaint, graygammaint, graygammaint);
	}
}

void BuildDefaultShademap (palette_t *pal, shademap_t &maps)
{
	argb_t *shade;
	argb_t colors[256];

	unsigned int r = RPART(level.fadeto);
	unsigned int g = GPART(level.fadeto);
	unsigned int b = BPART(level.fadeto);

	BuildLightRamp(maps);

	// build normal light mappings
	for (unsigned int i = 0; i < NUMCOLORMAPS; i++)
	{
		byte a = maps.ramp[i * 255 / NUMCOLORMAPS];

		DoBlending          (pal->basecolors, colors, pal->numcolors, r, g, b, a);
		DoBlendingWithGamma (colors, maps.shademap + (i << pal->shadeshift), pal->numcolors, r, g, b, a);
	}

	// build special maps (e.g. invulnerability)
	int grayint;
	shade = maps.shademap + (NUMCOLORMAPS << pal->shadeshift);

	for (unsigned int i = 0; i < pal->numcolors; i++)
	{
		grayint = (int)(255.0f * clamp(1.0f -
			(RPART(pal->basecolors[i]) * 0.00116796875f +
			 GPART(pal->basecolors[i]) * 0.00229296875f +
			 BPART(pal->basecolors[i]) * 0.0005625f), 0.0f, 1.0f));
		const int graygammaint = newgamma[grayint];
		shade[i] = MAKERGB(graygammaint, graygammaint, graygammaint);
	}
}

void RefreshPalette (palette_t *pal)
{
	if (pal->flags & PALETTEF_SHADE)
	{
		if (pal->maps.colormap && pal->maps.colormap - pal->colormapsbase >= 256) {
			M_Free(pal->maps.colormap);
		}
		pal->colormapsbase = (byte *)Realloc (pal->colormapsbase, (NUMCOLORMAPS + 1) * 256 + 255);
		pal->maps.colormap = (byte *)(((ptrdiff_t)(pal->colormapsbase) + 255) & ~0xff);
		pal->maps.shademap = (argb_t *)Realloc (pal->maps.shademap, (NUMCOLORMAPS + 1)*256*sizeof(argb_t) + 255);

		BuildDefaultColorAndShademap(pal, pal->maps);
	}

	if (pal == &DefPal)
	{
		NormalLight.maps = shaderef_t(&DefPal.maps, 0);
		NormalLight.color = MAKERGB(255,255,255);
		NormalLight.fade = level.fadeto;
	}
}

void RefreshPalettes (void)
{
	palette_t *pal = FirstPal;

	RefreshPalette (&DefPal);
	while (pal) {
		RefreshPalette (pal);
		pal = pal->next;
	}
}


void GammaAdjustPalette (palette_t *pal)
{
	unsigned i, color;

	if (!(pal->colors && pal->basecolors))
		return;

	if (!gamma_initialized)
		V_UpdateGammaLevel(gammalevel);

	for (i = 0; i < pal->numcolors; i++)
	{
		color = pal->basecolors[i];
		pal->colors[i] = MAKERGB(newgamma[RPART(color)], newgamma[GPART(color)], newgamma[BPART(color)]);
	}
}

void GammaAdjustPalettes (void)
{
	palette_t *pal = FirstPal;

	GammaAdjustPalette(&DefPal);
	while (pal)
	{
		GammaAdjustPalette(pal);
		pal = pal->next;
	}
}

//
// V_AddBlend
//
// [RH] This is from Q2.
//
void V_AddBlend(float r, float g, float b, float a, float* v_blend)
{
	float a2, a3;

	if (a <= 0.0f)
		return;
	a2 = v_blend[3] + (1.0f - v_blend[3]) * a;	// new total alpha
	a3 = v_blend[3] / a2;		// fraction of color from old

	v_blend[0] = v_blend[0] * a3 + r*(1.0f - a3);
	v_blend[1] = v_blend[1] * a3 + g*(1.0f - a3);
	v_blend[2] = v_blend[2] * a3 + b*(1.0f - a3);
	v_blend[3] = a2;
}

void V_SetBlend (int blendr, int blendg, int blendb, int blenda)
{
	// Don't do anything if the new blend is the same as the old
	if ((blenda == 0 && BlendA == 0) ||
		(blendr == BlendR &&
		 blendg == BlendG &&
		 blendb == BlendB &&
		 blenda == BlendA))
		return;

	V_ForceBlend(blendr, blendg, blendb, blenda);
}

void V_ForceBlend (int blendr, int blendg, int blendb, int blenda)
{
	BlendR = blendr;
	BlendG = blendg;
	BlendB = blendb;
	BlendA = blenda;

	// blend the palette for 8-bit mode
	// shademap_t::shade takes care of blending
	// [SL] actually, an alpha overlay is drawn on top of the rendered screen
	// in R_RenderPlayerView
	if (screen->is8bit())
	{
		DoBlending(DefPal.colors, IndexedPalette, DefPal.numcolors,
					newgamma[BlendR], newgamma[BlendG], newgamma[BlendB], BlendA);
		I_SetPalette(IndexedPalette);
	}
}

BEGIN_COMMAND (testblend)
{
	int color;
	float amt;

	if (argc < 3)
	{
		Printf (PRINT_HIGH, "testblend <color> <amount>\n");
	}
	else
	{
		std::string colorstring = V_GetColorStringByName (argv[1]);

		if (colorstring.length())
			color = V_GetColorFromString (NULL, colorstring.c_str());
		else
			color = V_GetColorFromString (NULL, argv[1]);

		amt = (float)atof (argv[2]);
		if (amt > 1.0f)
			amt = 1.0f;
		else if (amt < 0.0f)
			amt = 0.0f;
		//V_SetBlend (RPART(color), GPART(color), BPART(color), (int)(amt * 256.0f));
		BaseBlendR = RPART(color);
		BaseBlendG = GPART(color);
		BaseBlendB = BPART(color);
		BaseBlendA = amt;
	}
}
END_COMMAND (testblend)

BEGIN_COMMAND (testfade)
{

	int color;

	if (argc < 2)
	{
		Printf (PRINT_HIGH, "testfade <color>\n");
	}
	else
	{
		std::string colorstring = V_GetColorStringByName (argv[1]);
		if (colorstring.length())
			color = V_GetColorFromString (NULL, colorstring.c_str());
		else
			color = V_GetColorFromString (NULL, argv[1]);

		level.fadeto = color;
		RefreshPalettes();
		NormalLight.maps = shaderef_t(&DefPal.maps, 0);
	}
}
END_COMMAND (testfade)

/****** Colorspace Conversion Functions ******/

// Code from http://www.cs.rit.edu/~yxv4997/t_convert.html

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//				if s == 0, then h = -1 (undefined)

// Green Doom guy colors:
// RGB - 0: {    .46  1 .429 } 7: {    .254 .571 .206 } 15: {    .0317 .0794 .0159 }
// HSV - 0: { 116.743 .571 1 } 7: { 112.110 .639 .571 } 15: { 105.071  .800 .0794 }
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v)
{
	float min, max, delta, foo;

	if (r == g && g == b) {
		*h = 0;
		*s = 0;
		*v = r;
		return;
	}

	foo = r < g ? r : g;
	min = (foo < b) ? foo : b;
	foo = r > g ? r : g;
	max = (foo > b) ? foo : b;

	*v = max;									// v

	delta = max - min;

	*s = delta / max;							// s

	if (r == max)
		*h = (g - b) / delta;					// between yellow & magenta
	else if (g == max)
		*h = 2 + (b - r) / delta;				// between cyan & yellow
	else
		*h = 4 + (r - g) / delta;				// between magenta & cyan

	*h *= 60;									// degrees
	if (*h < 0)
		*h += 360;
}

void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v)
{
	int i;
	float f, p, q, t;

	if (s == 0) {
		// achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;									// sector 0 to 5
	i = (int)floor (h);
	f = h - i;									// factorial part of h
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));

	switch (i) {
		case 0:		*r = v; *g = t; *b = p; break;
		case 1:		*r = q; *g = v; *b = p; break;
		case 2:		*r = p; *g = v; *b = t; break;
		case 3:		*r = p; *g = q; *b = v; break;
		case 4:		*r = t; *g = p; *b = v; break;
		default:	*r = v; *g = p; *b = q; break;
	}
}

/****** Colored Lighting Stuffs (Sorry, 8-bit only) ******/

// Builds NUMCOLORMAPS colormaps lit with the specified color
void BuildColoredLights (shademap_t *maps, int lr, int lg, int lb, int r, int g, int b)
{
	unsigned int l,c;
	byte	*color;
	argb_t  *shade;

	// The default palette is assumed to contain the maps for white light.
	if (!maps)
		return;

	BuildLightRamp(*maps);

	// build normal (but colored) light mappings
	for (l = 0; l < NUMCOLORMAPS; l++) {
		byte a = maps->ramp[l * 255 / NUMCOLORMAPS];

		// Write directly to the shademap for blending:
		argb_t *colors = maps->shademap + (256 * l);
		DoBlending (DefPal.basecolors, colors, DefPal.numcolors, r, g, b, a);

		// Build the colormap and shademap:
		color = maps->colormap + 256*l;
		shade = maps->shademap + 256*l;
		for (c = 0; c < 256; c++) {
			shade[c] = MAKERGB(
				newgamma[(RPART(colors[c])*lr)/255],
				newgamma[(GPART(colors[c])*lg)/255],
				newgamma[(BPART(colors[c])*lb)/255]
			);
			color[c] = BestColor(
				DefPal.basecolors,
				RPART(shade[c]),
				GPART(shade[c]),
				BPART(shade[c]),
				256
			);
		}
	}
}

dyncolormap_t *GetSpecialLights (int lr, int lg, int lb, int fr, int fg, int fb)
{
	unsigned int color = MAKERGB (lr, lg, lb);
	unsigned int fade = MAKERGB (fr, fg, fb);
	dyncolormap_t *colormap = &NormalLight;

	// Bah! Simple linear search because I want to get this done.
	while (colormap) {
		if (color == colormap->color && fade == colormap->fade)
			return colormap;
		else
			colormap = colormap->next;
	}

	// Not found. Create it.
	colormap = (dyncolormap_t *)Z_Malloc (sizeof(*colormap), PU_LEVEL, 0);
	shademap_t *maps = new shademap_t();
	maps->colormap = (byte *)Z_Malloc (NUMCOLORMAPS*256*sizeof(byte)+3+255, PU_LEVEL, 0);
	maps->colormap = (byte *)(((ptrdiff_t)maps->colormap + 255) & ~0xff);
	maps->shademap = (argb_t *)Z_Malloc (NUMCOLORMAPS*256*sizeof(argb_t)+3+255, PU_LEVEL, 0);
	maps->shademap = (argb_t *)(((ptrdiff_t)maps->shademap + 255) & ~0xff);

	colormap->maps = shaderef_t(maps, 0);
	colormap->color = color;
	colormap->fade = fade;
	colormap->next = NormalLight.next;
	NormalLight.next = colormap;

	BuildColoredLights (maps, lr, lg, lb, fr, fg, fb);

	return colormap;
}

BEGIN_COMMAND (testcolor)
{
	int color;

	if (argc < 2)
	{
		Printf (PRINT_HIGH, "testcolor <color>\n");
	}
	else
	{
		std::string colorstring = V_GetColorStringByName (argv[1]);

		if (colorstring.length())
			color = V_GetColorFromString (NULL, colorstring.c_str());
		else
			color = V_GetColorFromString (NULL, argv[1]);

		BuildColoredLights ((shademap_t *)NormalLight.maps.map(), RPART(color), GPART(color), BPART(color),
			RPART(level.fadeto), GPART(level.fadeto), BPART(level.fadeto));
	}
}
END_COMMAND (testcolor)

//
// V_DoPaletteEffects
//
// Handles changing the palette or the BlendR/G/B/A globals based on damage
// the player has taken, any power-ups, or environment such as deep water.
//
void V_DoPaletteEffects()
{
	player_t* plyr = &displayplayer();

	if (screen->is8bit())
	{
		int		palette;

		float cnt = (float)plyr->damagecount;
		if (!multiplayer || sv_allowredscreen)
			cnt *= r_painintensity;

		// slowly fade the berzerk out
		if (plyr->powers[pw_strength])
			cnt = MAX(cnt, 12.0f - float(plyr->powers[pw_strength] >> 6));

		if (cnt)
		{
			palette = ((int)cnt + 7) >> 3;

			if (gamemode == retail_chex)
				palette = RADIATIONPAL;
			else
			{
				if (palette >= NUMREDPALS)
					palette = NUMREDPALS-1;

				palette += STARTREDPALS;

				if (palette < 0)
					palette = 0;
			}
		}
		else if (plyr->bonuscount)
		{
			palette = (plyr->bonuscount+7)>>3;

			if (palette >= NUMBONUSPALS)
				palette = NUMBONUSPALS-1;

			palette += STARTBONUSPALS;
		}
		else if (plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet] & 8)
			palette = RADIATIONPAL;
		else
			palette = 0;

		if (palette != current_palette)
		{
			current_palette = palette;
			byte* pal = (byte *)W_CacheLumpNum(lu_palette, PU_CACHE) + palette * 768;
			I_SetOldPalette(pal);
		}
	}
	else
	{
		float blend[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float cnt;

		V_AddBlend(BaseBlendR / 255.0f, BaseBlendG / 255.0f, BaseBlendB / 255.0f, BaseBlendA, blend);

		// 32bpp gets color blending approximated to the original palettes:
		if (plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet] & 8)
			V_AddBlend(0.0f, 1.0f, 0.0f, 0.125f, blend);

		if (plyr->bonuscount)
		{
			cnt = (float)(plyr->bonuscount << 3);
			V_AddBlend(0.8431f, 0.7294f, 0.2706f, cnt > 128 ? 0.5f : cnt / 255.0f, blend);
		}

		// NOTE(jsd): rewritten to better match 8bpp behavior
		// 0 <= damagecount <= 100
		cnt = (float)plyr->damagecount*3.5f;
		if (!multiplayer || sv_allowredscreen)
			cnt *= r_painintensity;

		// slowly fade the berzerk out
		if (plyr->powers[pw_strength])
			cnt = MAX(cnt, 128.0f - float((plyr->powers[pw_strength]>>3) & (~0x1f)));
	
		cnt = clamp(cnt, 0.0f, 237.0f);

		if (cnt > 0.0f)
			V_AddBlend (1.0f, 0.0f, 0.0f, (cnt + 18.0f) / 255.0f, blend);

		V_AddBlend(plyr->BlendR, plyr->BlendG, plyr->BlendB, plyr->BlendA, blend);

		if (memcmp(blend, current_blend, sizeof(blend)))
	        memcpy(current_blend, blend, sizeof(blend));

		V_SetBlend ((int)(blend[0] * 255.0f), (int)(blend[1] * 255.0f),
					(int)(blend[2] * 255.0f), (int)(blend[3] * 256.0f));
	}
}

VERSION_CONTROL (v_palette_cpp, "$Id$")

