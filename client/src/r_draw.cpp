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
//		The actual span/column drawing functions.
//		Here find the main potential for optimization,
//		 e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#include <stddef.h>
#include <assert.h>
#include <algorithm>

#include "SDL_cpuinfo.h"
#include "r_intrin.h"

#include "m_alloc.h"
#include "doomdef.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"

#include "gi.h"

#undef RANGECHECK

// status bar height at bottom of screen
// [RH] status bar position at bottom of screen
extern	int		ST_Y;

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//	not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//	and we need only the base address,
//	and the total size == width*height*depth/8.,
//

extern "C" {
drawcolumn_t dcol;
drawspan_t dspan;
}



byte*			viewimage;
extern "C" {
int 			viewwidth;
int 			viewheight;
}
int 			scaledviewwidth;
int 			viewwindowx;
int 			viewwindowy;
byte**			ylookup;
int* 			columnofs;

extern "C" {
int				realviewwidth;		// [RH] Physical width of view window
int				realviewheight;		// [RH] Physical height of view window
int				detailxshift;		// [RH] X shift for horizontal detail level
int				detailyshift;		// [RH] Y shift for vertical detail level
}

// [RH] Pointers to the different column drawers.
//		These get changed depending on the current
//		screen depth.
void (*R_FillColumn)(drawcolumn_t&);
void (*R_FillMaskedColumn)(drawcolumn_t&);
void (*R_DrawColumn)(drawcolumn_t&);
void (*R_DrawMaskedColumn)(drawcolumn_t&);
void (*R_DrawFuzzColumn)(drawcolumn_t&);
void (*R_DrawFuzzMaskedColumn)(drawcolumn_t&);
void (*R_DrawTranslucentColumn)(drawcolumn_t&);
void (*R_DrawTranslucentMaskedColumn)(drawcolumn_t&);
void (*R_DrawTranslatedColumn)(drawcolumn_t&);
void (*R_DrawTranslatedMaskedColumn)(drawcolumn_t&);
void (*R_DrawTranslatedTranslucentColumn)(drawcolumn_t&);
void (*R_DrawTranslatedTranslucentMaskedColumn)(drawcolumn_t&);

void (*R_DrawSpan)(drawspan_t&);
void (*R_DrawSlopeSpan)(drawspan_t&);
void (*R_FillSpan)(drawspan_t&);
void (*R_FillTranslucentSpan)(drawspan_t&);

// Possibly vectorized functions:
void (*R_DrawSpanD)(drawspan_t&);
void (*R_DrawSlopeSpanD)(drawspan_t&);
void (*r_dimpatchD)(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h);

// ============================================================================
//
// Fuzz Table
//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
// from adjacent ones to left and right.
// Used with an all black colormap, this
// could create the SHADOW effect,
// i.e. spectres and invisible players.
//
// ============================================================================

#define FUZZTABLE	64		// [RH] FUZZTABLE changed from 50 to 64
#define FUZZOFF		(screen->pitch)

static int fuzzoffset[FUZZTABLE];
static int fuzzpos = 0;

static const signed char fuzzinit[FUZZTABLE] = {
	1,-1, 1,-1, 1, 1,-1, 1,
	1,-1, 1, 1, 1,-1, 1, 1,
	1,-1,-1,-1,-1, 1,-1,-1,
	1, 1, 1, 1,-1, 1,-1, 1,
	1,-1,-1, 1, 1,-1,-1,-1,
   -1, 1, 1, 1, 1,-1, 1, 1,
   -1, 1, 1, 1,-1, 1, 1, 1,
   -1, 1, 1,-1, 1, 1,-1, 1
};

void R_InitFuzzTable (void)
{
	int i;
	int fuzzoff;

	screen->Lock ();
	fuzzoff = FUZZOFF << detailyshift;
	screen->Unlock ();

	for (i = 0; i < FUZZTABLE; i++)
		fuzzoffset[i] = fuzzinit[i] * fuzzoff;
}


// ============================================================================
//
// Translucency Table
//
// ============================================================================

/*
[RH] This translucency algorithm is based on DOSDoom 0.65's, but uses
a 32k RGB table instead of an 8k one. At least on my machine, it's
slightly faster (probably because it uses only one shift instead of
two), and it looks considerably less green at the ends of the
translucency range. The extra size doesn't appear to be an issue.

The following note is from DOSDoom 0.65:

New translucency algorithm, by Erik Sandberg:

Basically, we compute the red, green and blue values for each pixel, and
hen use a RGB table to check which one of the palette colours that best
represents those RGB values. The RGB table is 8k big, with 4 R-bits,
5 G-bits and 4 B-bits. A 4k table gives a bit too bad precision, and a 32k
table takes up more memory and results in more cache misses, so an 8k
table seemed to be quite ultimate.

The computation of the RGB for each pixel is accelerated by using two
1k tables for each translucency level.
The xth element of one of these tables contains the r, g and b values for
the colour x, weighted for the current translucency level (for example,
the weighted rgb values for background colour at 75% translucency are 1/4
of the original rgb values). The rgb values are stored as three
low-precision fixed point values, packed into one long per colour:
Bit 0-4:   Frac part of blue  (5 bits)
Bit 5-8:   Int  part of blue  (4 bits)
Bit 9-13:  Frac part of red   (5 bits)
Bit 14-17: Int  part of red   (4 bits)
Bit 18-22: Frac part of green (5 bits)
Bit 23-27: Int  part of green (5 bits)
Bit 28-31: All zeros          (4 bits)

The point of this format is that the two colours now can be added, and
then be converted to a RGB table index very easily: First, we just set
all the frac bits and the four upper zero bits to 1. It's now possible
to get the RGB table index by anding the current value >> 5 with the
current value >> 19. When asm-optimised, this should be the fastest
algorithm that uses RGB tables.
*/


// ============================================================================
//
// Indexed-color Translation Table
//
// Used to draw player sprites with the green colorramp mapped to others.
// Could be used with different translation tables, e.g. the lighter colored
// version of the BaronOfHell, the HellKnight, uses identical sprites, kinda
// brightened up.
//
// ============================================================================

byte* translationtables;
argb_t translationRGB[MAXPLAYERS+1][16];
byte *Ranges;
static byte *translationtablesmem = NULL;

//
// R_InitTranslationTables
//
// Creates the translation tables to map
//	the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables (void)
{
	static const char ranges[23][8] = {
		"CRBRICK",
		"CRTAN",
		"CRGRAY",
		"CRGREEN",
		"CRBROWN",
		"CRGOLD",
		"CRRED",
		"CRBLUE2",
		{ 'C','R','O','R','A','N','G','E' },
		"CRGRAY", // "White"
		{ 'C','R','Y','E','L','L','O','W' },
		"CRRED", // "Untranslated"
		"CRGRAY", // "Black"
		"CRBLUE",
		"CRTAN", // "Cream"
		"CRGREEN", // "Olive"
		"CRGREEN", // "Dark Green"
		"CRRED", // "Dark Red"
		"CRBROWN", // "Dark Brown"
		"CRRED", // "Purple"
		"CRGRAY", // "Dark Gray"
		"CRBLUE" // "Cyan"
	};
	
    R_FreeTranslationTables();
	
	translationtablesmem = new byte[256*(MAXPLAYERS+3+22)+255]; // denis - fixme - magic numbers?

	// [Toke - fix13]
	// denis - cleaned this up somewhat
	translationtables = (byte *)(((ptrdiff_t)translationtablesmem + 255) & ~255);
	
	// [RH] Each player now gets their own translation table
	//		(soon to be palettes). These are set up during
	//		netgame arbitration and as-needed rather than
	//		in here. We do, however load some text translation
	//		tables from our PWAD (ala BOOM).

	for (int i = 0; i < 256; i++)
		translationtables[i] = i;

	// Set up default translationRGB tables:
	palette_t *pal = GetDefaultPalette();
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		for (int j = 0x70; j < 0x80; ++j)
			translationRGB[i][j - 0x70] = pal->basecolors[j];
	}

	for (int i = 1; i < MAXPLAYERS+3; i++)
		memcpy (translationtables + i*256, translationtables, 256);

	// create translation tables for dehacked patches that expect them
	for (int i = 0x70; i < 0x80; i++) {
		// map green ramp to gray, brown, red
		translationtables[i+(MAXPLAYERS+0)*256] = 0x60 + (i&0xf);
		translationtables[i+(MAXPLAYERS+1)*256] = 0x40 + (i&0xf);
		translationtables[i+(MAXPLAYERS+2)*256] = 0x20 + (i&0xf);
	}

	Ranges = translationtables + (MAXPLAYERS+3)*256;
	for (int i = 0; i < 22; i++)
		W_ReadLump (W_GetNumForName (ranges[i]), Ranges + 256 * i);

}

void R_FreeTranslationTables (void)
{
    delete[] translationtablesmem;
    translationtablesmem = NULL;
}

// [Nes] Vanilla player translation table.
void R_BuildClassicPlayerTranslation (int player, int color)
{
	palette_t *pal = GetDefaultPalette();
	int i;
	
	if (color == 1) // Indigo
		for (i = 0x70; i < 0x80; i++)
		{
			translationtables[i+(player * 256)] = 0x60 + (i&0xf);
			translationRGB[player][i - 0x70] = pal->basecolors[translationtables[i+(player * 256)]];
		}
	else if (color == 2) // Brown
		for (i = 0x70; i < 0x80; i++)
		{
			translationtables[i+(player * 256)] = 0x40 + (i&0xf);	
			translationRGB[player][i - 0x70] = pal->basecolors[translationtables[i+(player * 256)]];
		}
	else if (color == 3) // Red
		for (i = 0x70; i < 0x80; i++)
		{
			translationtables[i+(player * 256)] = 0x20 + (i&0xf);	
			translationRGB[player][i - 0x70] = pal->basecolors[translationtables[i+(player * 256)]];
		}
}

void R_CopyTranslationRGB (int fromplayer, int toplayer)
{
	for (int i = 0x70; i < 0x80; ++i)
	{
		translationRGB[toplayer][i - 0x70] = translationRGB[fromplayer][i - 0x70];
		translationtables[i+(toplayer * 256)] = translationtables[i+(fromplayer * 256)];
	}
}

// [RH] Create a player's translation table based on
//		a given mid-range color.
void R_BuildPlayerTranslation (int player, int color)
{
	palette_t *pal = GetDefaultPalette();
	byte *table = &translationtables[player * 256];
	int i;
	float r = (float)RPART(color) / 255.0f;
	float g = (float)GPART(color) / 255.0f;
	float b = (float)BPART(color) / 255.0f;
	float h, s, v;
	float sdelta, vdelta;

	RGBtoHSV (r, g, b, &h, &s, &v);

	s -= 0.23f;
	if (s < 0.0f)
		s = 0.0f;
	sdelta = 0.014375f;

	v += 0.1f;
	if (v > 1.0f)
		v = 1.0f;
	vdelta = -0.05882f;

	for (i = 0x70; i < 0x80; i++) {
		HSVtoRGB (&r, &g, &b, h, s, v);

		// Set up RGB values for 32bpp translation:
		translationRGB[player][i - 0x70] = MAKERGB(
			(int)(r * 255.0f),
			(int)(g * 255.0f),
			(int)(b * 255.0f)
		);

		table[i] = BestColor (pal->basecolors,
							  (int)(r * 255.0f),
							  (int)(g * 255.0f),
							  (int)(b * 255.0f),
							  pal->numcolors);
		s += sdelta;
		if (s > 1.0f) {
			s = 1.0f;
			sdelta = 0.0f;
		}

		v += vdelta;
		if (v < 0.0f) {
			v = 0.0f;
			vdelta = 0.0f;
		}
	}
}


// ============================================================================
//
// Spans
//
// With DOOM style restrictions on view orientation,
// the floors and ceilings consist of horizontal slices
// or spans with constant z depth.
// However, rotation around the world z axis is possible,
// thus this mapping, while simpler and faster than
// perspective correct texture mapping, has to traverse
// the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
// and the inner loop has to step in texture space u and v.
//
// ============================================================================


// ============================================================================
//
// Generic Drawers
//
// Templated versions of column and span drawing functions
//
// ============================================================================

//
// R_BlankColumn
//
// [SL] - Does nothing (obviously). Used when a column drawing function
// pointer should not draw anything.
//
void R_BlankColumn(drawcolumn_t& drawcolumn)
{
}

//
// R_BlankSpan
//
// [SL] - Does nothing (obviously). Used when a span drawing function
// pointer should not draw anything.
//
void R_BlankSpan(drawspan_t& drawspan)
{
}

//
// R_FillColumnGeneric
//
// Templated version of a function to fill a column with a solid color. 
// The data type of the destination pixels and a color-remapping functor
// are passed as template parameters.
//
template<typename PIXEL_T, typename COLORFUNC>
static forceinline void R_FillColumnGeneric(drawcolumn_t& drawcolumn)
{
#ifdef RANGECHECK 
	if (drawcolumn.x >= screen->width || drawcolumn.yl < 0 || drawcolumn.yh >= screen->height)
	{
		Printf (PRINT_HIGH, "R_FillColumn: %i to %i at %i\n", drawcolumn.yl, drawcolumn.yh, drawcolumn.x);
		return;
	}
#endif

	int count = drawcolumn.yh - drawcolumn.yl + 1;
	if (count <= 0)
		return;

	int color = drawcolumn.color;
	int pitch = drawcolumn.pitch / sizeof(PIXEL_T);
	PIXEL_T* dest = (PIXEL_T*)drawcolumn.dest;

	COLORFUNC colorfunc(drawcolumn);

	do {
		colorfunc(color, dest);
		dest += pitch;
	} while (--count);
} 

//
// R_FillMaskedColumnGeneric
//
// Templated version of a function to fill a column with a solid color using
// masking.  The data type of the destination pixels and a color-remapping
// functor are passed as template parameters.
//
template<typename PIXEL_T, typename COLORFUNC>
static forceinline void R_FillMaskedColumnGeneric(drawcolumn_t& drawcolumn)
{
#ifdef RANGECHECK 
	if (drawcolumn.x >= screen->width || drawcolumn.yl < 0 || drawcolumn.yh >= screen->height)
	{
		Printf (PRINT_HIGH, "R_FillMaskedColumn: %i to %i at %i\n", drawcolumn.yl, drawcolumn.yh, drawcolumn.x);
		return;
	}
#endif

	int count = drawcolumn.yh - drawcolumn.yl + 1;
	if (count <= 0)
		return;

	const byte* mask = drawcolumn.mask;
	int color = drawcolumn.color;
	int pitch = drawcolumn.pitch / sizeof(PIXEL_T);
	PIXEL_T* dest = (PIXEL_T*)drawcolumn.dest;

	const fixed_t fracstep = drawcolumn.iscale; 
	fixed_t frac = drawcolumn.texturefrac;

	COLORFUNC colorfunc(drawcolumn);

	do
	{
		PIXEL_T tempdest;
		colorfunc(color, &tempdest);

		// [SL] negating an unsigned number is a quick way to expand 0x01 to 0xFF
		PIXEL_T sourcemask = -mask[frac >> FRACBITS];
		PIXEL_T destmask = ~sourcemask;
		// [SL] perform masking without branching
		*dest = (*dest & destmask) | (tempdest & sourcemask);

		dest += pitch; frac += fracstep;
	} while (--count);
}

//
// R_DrawColumnGeneric
//
// A column is a vertical slice/span from a wall texture that,
// given the DOOM style restrictions on the view orientation,
// will always have constant z depth.
// Thus a special case loop for very fast rendering can
// be used. It has also been used with Wolfenstein 3D.
//
// Templated version of a column mapping function.
// The data type of the destination pixels and a color-remapping functor
// are passed as template parameters.
//
template<typename PIXEL_T, typename COLORFUNC>
static forceinline void R_DrawColumnGeneric(drawcolumn_t& drawcolumn)
{
#ifdef RANGECHECK 
	if (drawcolumn.x >= screen->width || drawcolumn.yl < 0 || drawcolumn.yh >= screen->height)
	{
		Printf (PRINT_HIGH, "R_DrawColumn: %i to %i at %i\n", drawcolumn.yl, drawcolumn.yh, drawcolumn.x);
		return;
	}
#endif

	int count = drawcolumn.yh - drawcolumn.yl + 1;
	if (count <= 0)
		return;

	const palindex_t* source = drawcolumn.source;
	PIXEL_T* dest = (PIXEL_T*)drawcolumn.dest;
	int pitch = drawcolumn.pitch / sizeof(PIXEL_T);

	const fixed_t fracstep = drawcolumn.iscale; 
	fixed_t frac = drawcolumn.texturefrac;

	const int texheight = drawcolumn.textureheight;
	const int mask = (texheight >> FRACBITS) - 1;

	COLORFUNC colorfunc(drawcolumn);

	// [SL] Properly tile textures whose heights are not a power-of-2,
	// avoiding a tutti-frutti effect.  From Eternity Engine.
	if (texheight & (texheight - 1))
	{
		// texture height is NOT a power-of-2
		// just do a simple blit to the dest buffer (I'm lazy)

		if (frac < 0)
			while ((frac += texheight) < 0);
		else
			while (frac >= texheight)
				frac -= texheight;

		do {
			colorfunc(source[frac >> FRACBITS], dest);
			dest += pitch;
			if ((frac += fracstep) >= texheight)
				frac -= texheight;
		} while (--count);
	}
	else
	{
		// texture height is a power-of-2
		do {
			colorfunc(source[(frac >> FRACBITS) & mask], dest);
			dest += pitch; frac += fracstep;
		} while (--count);
	}
}


//
// R_DrawMaskedColumnGeneric
//
// Templated version of a column mapping function.
// The data type of the destination pixels and a color-remapping functor
// are passed as template parameters. Masking is performed to allow the
// image to have transparent regions.
//
template<typename PIXEL_T, typename COLORFUNC>
static forceinline void R_DrawMaskedColumnGeneric(drawcolumn_t& drawcolumn)
{
#ifdef RANGECHECK 
	if (drawcolumn.x >= screen->width || drawcolumn.yl < 0 || drawcolumn.yh >= screen->height) {
		Printf (PRINT_HIGH, "R_DrawMaskedColumn: %i to %i at %i\n", drawcolumn.yl, drawcolumn.yh, drawcolumn.x);
		return;
	}
#endif

	int count = drawcolumn.yh - drawcolumn.yl + 1;
	if (count <= 0)
		return;

	const byte* mask = drawcolumn.mask;
	const palindex_t* source = drawcolumn.source;
	PIXEL_T* dest = (PIXEL_T*)drawcolumn.dest;
	int pitch = drawcolumn.pitch / sizeof(PIXEL_T);

	const fixed_t fracstep = drawcolumn.iscale; 
	fixed_t frac = drawcolumn.texturefrac;

	COLORFUNC colorfunc(drawcolumn);

	do {
		PIXEL_T tempdest;
		colorfunc(source[frac >> FRACBITS], &tempdest);

		// [SL] negating an unsigned number is a quick way to expand 0x01 to 0xFF
		PIXEL_T sourcemask = -mask[frac >> FRACBITS];
		PIXEL_T destmask = ~sourcemask;
		// [SL] perform masking without branching
		*dest = (*dest & destmask) | (tempdest & sourcemask);

		dest += pitch; frac += fracstep;
	} while (--count);
}



//
// R_FillSpanGeneric
//
// Templated version of a function to fill a span with a solid color.
// The data type of the destination pixels and a color-remapping functor
// are passed as template parameters.
//
template<typename PIXEL_T, typename COLORFUNC>
static forceinline void R_FillSpanGeneric(drawspan_t& drawspan)
{
#ifdef RANGECHECK
	if (drawspan.x2 < drawspan.x1 || drawspan.x1 < 0 || drawspan.x2 >= viewwidth ||
		drawspan.y >= viewheight || drawspan.y < 0)
	{
		Printf(PRINT_HIGH, "R_FillSpan: %i to %i at %i", drawspan.x1, drawspan.x2, drawspan.y);
		return;
	}
#endif

	int count = drawspan.x2 - drawspan.x1 + 1;
	if (count <= 0)
		return;

	PIXEL_T* dest = (PIXEL_T*)drawspan.dest;
	int color = drawspan.color;
	int colsize = drawspan.colsize;

	COLORFUNC colorfunc(drawspan);

	do {
		colorfunc(color, dest);
		dest += colsize;
	} while (--count);
}


//
// R_DrawLevelSpanGeneric
//
// Templated version of a function to fill a horizontal span with a texture map.
// The data type of the destination pixels and a color-remapping functor
// are passed as template parameters.
//
template<typename PIXEL_T, typename COLORFUNC>
static forceinline void R_DrawLevelSpanGeneric(drawspan_t& drawspan)
{
#ifdef RANGECHECK
	if (drawspan.x2 < drawspan.x1 || drawspan.x1 < 0 || drawspan.x2 >= viewwidth ||
		drawspan.y >= viewheight || drawspan.y < 0)
	{
		Printf(PRINT_HIGH, "R_DrawLevelSpan: %i to %i at %i", drawspan.x1, drawspan.x2, drawspan.y);
		return;
	}
#endif

	int count = drawspan.x2 - drawspan.x1 + 1;
	if (count <= 0)
		return;

	const palindex_t* source = drawspan.source;
	PIXEL_T* dest = (PIXEL_T*)drawspan.dest;
	int colsize = drawspan.colsize;
	
	dsfixed_t ufrac = dspan.xfrac;
	dsfixed_t vfrac = dspan.yfrac;
	const dsfixed_t ustep = dspan.xstep;
	const dsfixed_t vstep = dspan.ystep;

	COLORFUNC colorfunc(drawspan);

	// [SL] Note that the texture orientation differs from typical Doom span
	// drawers since flats are stored in column major format now. The roles
	// of ufrac and vfrac have been reversed to accomodate this.
	const int umask = ((1 << drawspan.texturewidthbits) - 1) << drawspan.textureheightbits;
	const int vmask = (1 << drawspan.textureheightbits) - 1;
	const int ushift = FRACBITS - drawspan.textureheightbits; 
	const int vshift = FRACBITS;
 
	do {
		// Current texture index in u,v.
		const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 

		// Lookup pixel from flat texture tile,
		//  re-index using light/colormap.
		colorfunc(source[spot], dest);
		dest += colsize;

		// Next step in u,v.
		ufrac += ustep;
		vfrac += vstep;
	} while (--count);
}


//
// R_DrawSlopedSpanGeneric
//
// Texture maps a sloped surface using affine texturemapping for each row of
// the span.  Not as pretty as a perfect texturemapping but should be much
// faster.
//
// Based on R_DrawSlope_8_64 from Eternity Engine, written by SoM/Quasar
//
// The data type of the destination pixels and a color-remapping functor
// are passed as template parameters.
//
template<typename PIXEL_T, typename COLORFUNC>
static forceinline void R_DrawSlopedSpanGeneric(drawspan_t& drawspan)
{
#ifdef RANGECHECK
	if (drawspan.x2 < drawspan.x1 || drawspan.x1 < 0 || drawspan.x2 >= viewwidth ||
		drawspan.y >= viewheight || drawspan.y < 0)
	{
		Printf(PRINT_HIGH, "R_DrawSlopedSpan: %i to %i at %i", drawspan.x1, drawspan.x2, drawspan.y);
		return;
	}
#endif

	int count = drawspan.x2 - drawspan.x1 + 1;
	if (count <= 0)
		return;
	
	const palindex_t* source = drawspan.source;
	PIXEL_T* dest = (PIXEL_T*)drawspan.dest;
	int colsize = drawspan.colsize;


	// [SL] Note that the texture orientation differs from typical Doom span
	// drawers since flats are stored in column major format now. The roles
	// of xfrac and yfrac have been reversed to accomodate this.
	const int umask = ((1 << drawspan.texturewidthbits) - 1) << drawspan.textureheightbits;
	const int vmask = (1 << drawspan.textureheightbits) - 1;
	const int ushift = FRACBITS - drawspan.textureheightbits; 
	const int vshift = FRACBITS;
 
	float iu = drawspan.iu, iv = drawspan.iv;
	const float ius = drawspan.iustep, ivs = drawspan.ivstep;
	float id = drawspan.id, ids = drawspan.idstep;
	
	int ltindex = 0;

	shaderef_t colormap;
	COLORFUNC colorfunc(drawspan);

	while (count >= SPANJUMP)
	{
		const float mulstart = 65536.0f / id;
		id += ids * SPANJUMP;
		const float mulend = 65536.0f / id;

		const float ustart = iu * mulstart;
		const float vstart = iv * mulstart;

		fixed_t ufrac = (fixed_t)ustart;
		fixed_t vfrac = (fixed_t)vstart;

		iu += ius * SPANJUMP;
		iv += ivs * SPANJUMP;

		const float uend = iu * mulend;
		const float vend = iv * mulend;

		fixed_t ustep = (fixed_t)((uend - ustart) * INTERPSTEP);
		fixed_t vstep = (fixed_t)((vend - vstart) * INTERPSTEP);

		int incount = SPANJUMP;
		while (incount--)
		{
			colormap = drawspan.slopelighting[ltindex++];

			const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 
			colorfunc(source[spot], dest);
			dest += colsize;
			ufrac += ustep;
			vfrac += vstep;
		}

		count -= SPANJUMP;
	}

	if (count > 0)
	{
		const float mulstart = 65536.0f / id;
		id += ids * count;
		const float mulend = 65536.0f / id;

		const float ustart = iu * mulstart;
		const float vstart = iv * mulstart;

		fixed_t ufrac = (fixed_t)ustart;
		fixed_t vfrac = (fixed_t)vstart;

		iu += ius * count;
		iv += ivs * count;

		const float uend = iu * mulend;
		const float vend = iv * mulend;

		fixed_t ustep = (fixed_t)((uend - ustart) / count);
		fixed_t vstep = (fixed_t)((vend - vstart) / count);

		int incount = count;
		while (incount--)
		{
			colormap = drawspan.slopelighting[ltindex++];

			const int spot = ((ufrac >> ushift) & umask) | ((vfrac >> vshift) & vmask); 
			colorfunc(source[spot], dest);
			dest += colsize;
			ufrac += ustep;
			vfrac += vstep;
		}
	}
}


/************************************/
/*									*/
/* Palettized drawers (C versions)	*/
/*									*/
/************************************/

// ----------------------------------------------------------------------------
//
// 8bpp color remapping functors
//
// These functors provide a variety of ways to manipulate a source pixel
// color (given by 8bpp palette index) and write the result to the destination
// buffer.
//
// The functors are instantiated with a shaderef_t* parameter (typically
// dcol.colormap or ds_colormap) that will be used to shade the pixel.
//
// ----------------------------------------------------------------------------

class PaletteFunc
{
public:
	PaletteFunc(const drawcolumn_t& drawcolumn) { }
	PaletteFunc(const drawspan_t& drawspan) { }

	forceinline void operator()(byte c, palindex_t* dest) const
	{
		*dest = c;
	}
};

class PaletteColormapFunc
{
public:
	PaletteColormapFunc(const drawcolumn_t& drawcolumn) :
			colormap(drawcolumn.colormap) { }
	PaletteColormapFunc(const drawspan_t& drawspan) :
			colormap(drawspan.colormap) { }

	forceinline void operator()(byte c, palindex_t* dest) const
	{
		*dest = colormap.index(c);
	}

private:
	const shaderef_t& colormap;
};

class PaletteFuzzyFunc
{
public:
	PaletteFuzzyFunc(const drawcolumn_t& drawcolum) :
			colormap(&GetDefaultPalette()->maps, 6) { }

	forceinline void operator()(byte c, palindex_t* dest) const
	{
		*dest = colormap.index(dest[fuzzoffset[fuzzpos]]);
		fuzzpos = (fuzzpos + 1) & (FUZZTABLE - 1);
	}

private:
	shaderef_t colormap;
};

class PaletteTranslucentColormapFunc
{
public:
	PaletteTranslucentColormapFunc(const drawcolumn_t& drawcolumn) :
			colormap(drawcolumn.colormap)
	{
		calculate_alpha(drawcolumn.translevel);
	}

	PaletteTranslucentColormapFunc(const drawspan_t& drawspan) :
			colormap(drawspan.colormap)
	{
		calculate_alpha(drawspan.translevel);
	}

	forceinline void operator()(byte c, palindex_t* dest) const
	{
		const palindex_t fg = colormap.index(c);
		const palindex_t bg = *dest;
				
		*dest = rt_blend2<palindex_t>(bg, bga, fg, fga);
	}

private:
	void calculate_alpha(fixed_t translevel)
	{
		fga = (translevel & ~0x03FF) >> 8;
		bga = 255 - fga;
	}

	const shaderef_t& colormap;
	int fga, bga;
};

class PaletteTranslatedColormapFunc
{
public:
	PaletteTranslatedColormapFunc(const drawcolumn_t& drawcolumn) : 
			colormap(drawcolumn.colormap), translation(drawcolumn.translation) { }

	forceinline void operator()(byte c, palindex_t* dest) const
	{
		*dest = colormap.index(translation.tlate(c));
	}

private:
	const shaderef_t& colormap;
	const translationref_t& translation;
};

class PaletteTranslatedTranslucentColormapFunc
{
public:
	PaletteTranslatedTranslucentColormapFunc(const drawcolumn_t& drawcolumn) :
			tlatefunc(drawcolumn), translation(drawcolumn.translation) { }

	forceinline void operator()(byte c, palindex_t* dest) const
	{
		tlatefunc(translation.tlate(c), dest);
	}

private:
	PaletteTranslucentColormapFunc tlatefunc;
	const translationref_t& translation;
};

class PaletteSlopeColormapFunc
{
public:
	PaletteSlopeColormapFunc(const drawspan_t& drawspan) :
			colormap(drawspan.slopelighting) { }

	forceinline void operator()(byte c, palindex_t* dest)
	{
		*dest = colormap->index(c);
		colormap++;
	}

private:
	const shaderef_t* colormap;
};


// ----------------------------------------------------------------------------
//
// 8bpp color column drawing wrappers
//
// ----------------------------------------------------------------------------

//
// R_FillColumnP
//
// Fills a column in the 8bpp palettized screen buffer with a solid color,
// determined by dcol.color. Performs no shading.
//
void R_FillColumnP(drawcolumn_t& drawcolumn)
{
	R_FillColumnGeneric<palindex_t, PaletteFunc>(drawcolumn);
}

//
// R_FillMaskedColumnP
//
// Fills a column in the 8bpp palettized screen buffer with a solid color,
// determined by dcol.color. Performs no shading.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_FillMaskedColumnP(drawcolumn_t& drawcolumn)
{
	R_FillMaskedColumnGeneric<palindex_t, PaletteFunc>(drawcolumn);
}

//
// R_DrawColumnP
//
// Renders a column to the 8bpp palettized screen buffer from the source buffer
// dcol.source and scaled by dcol.iscale. Shading is performed using dcol.colormap.
//
void R_DrawColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<palindex_t, PaletteColormapFunc>(drawcolumn);
}

//
// R_DrawMaskedColumnP
//
// Renders a column to the 8bpp palettized screen buffer from the source buffer
// dc_source and scaled by dc_iscale. Shading is performed using dc_colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawMaskedColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<palindex_t, PaletteColormapFunc>(drawcolumn);
}

//
// R_DrawFuzzColumnP
//
// Alters a column in the 8bpp palettized screen buffer using Doom's partial
// invisibility effect, which shades the column and rearranges the ordering
// the pixels to create distortion. Shading is performed using colormap 6.
//
void R_DrawFuzzColumnP(drawcolumn_t& drawcolumn)
{
	// adjust the borders (prevent buffer over/under-reads)
	dcol.yl = MIN(1, dcol.yl);
	dcol.yh = MAX(realviewheight - 2, dcol.yh);
	dcol.dest = R_CalculateDestination(dcol);

	R_FillColumnGeneric<palindex_t, PaletteFuzzyFunc>(drawcolumn);
}

//
// R_DrawFuzzMaskedColumnP
//
// Alters a column in the 8bpp palettized screen buffer using Doom's partial
// invisibility effect, which shades the column and rearranges the ordering
// the pixels to create distortion. Shading is performed using colormap 6.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawFuzzMaskedColumnP(drawcolumn_t& drawcolumn)
{
	// adjust the borders (prevent buffer over/under-reads)
	dcol.yl = MIN(1, dcol.yl);
	dcol.yh = MAX(realviewheight - 2, dcol.yh);
	dcol.dest = R_CalculateDestination(dcol);

	R_FillMaskedColumnGeneric<palindex_t, PaletteFuzzyFunc>(drawcolumn);
}

//
// R_DrawTranslucentColumnP
//
// Renders a translucent column to the 8bpp palettized screen buffer from the
// source buffer dcol.source and scaled by dcol.iscale. The amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
//
void R_DrawTranslucentColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<palindex_t, PaletteTranslucentColormapFunc>(drawcolumn);
}

//
// R_DrawTranslucentMaskedColumnP
//
// Renders a translucent column to the 8bpp palettized screen buffer from the
// source buffer dcol.source and scaled by dcol.iscale. The amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawTranslucentMaskedColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<palindex_t, PaletteTranslucentColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedColumnP
//
// Renders a column to the 8bpp palettized screen buffer with color-remapping
// from the source buffer dcol.source and scaled by dcol.iscale. The translation
// table is supplied by dcol.translation. Shading is performed using dcol.colormap.
//
void R_DrawTranslatedColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<palindex_t, PaletteTranslatedColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedMaskedColumnP
//
// Renders a column to the 8bpp palettized screen buffer with color-remapping
// from the source buffer dcol.source and scaled by dcol.iscale. The translation
// table is supplied by dcol.translation. Shading is performed using dcol.colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawTranslatedMaskedColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<palindex_t, PaletteTranslatedColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedTranslucentColumnP
//
// Renders a translucent column to the 8bpp palettized screen buffer with
// color-remapping from the source buffer dcol.source and scaled by dcol.iscale. 
// The translation table is supplied by dcol.translation and the amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
//
void R_DrawTranslatedTranslucentColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<palindex_t, PaletteTranslatedTranslucentColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedTranslucentMaskedColumnP
//
// Renders a translucent column to the 8bpp palettized screen buffer with
// color-remapping from the source buffer dcol.source and scaled by dcol.iscale. 
// The translation table is supplied by dcol.translation and the amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawTranslatedTranslucentMaskedColumnP(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<palindex_t, PaletteTranslatedTranslucentColormapFunc>(drawcolumn);
}

// ----------------------------------------------------------------------------
//
// 8bpp color span drawing wrappers
//
// ----------------------------------------------------------------------------

//
// R_FillSpanP
//
// Fills a span in the 8bpp palettized screen buffer with a solid color,
// determined by ds_color. Performs no shading.
//
void R_FillSpanP(drawspan_t& drawspan)
{
	R_FillSpanGeneric<palindex_t, PaletteFunc>(drawspan);
}

//
// R_FillTranslucentSpanP
//
// Fills a span in the 8bpp palettized screen buffer with a solid color,
// determined by ds_color using translucency. Shading is performed 
// using ds_colormap.
//
void R_FillTranslucentSpanP(drawspan_t& drawspan)
{
	R_FillSpanGeneric<palindex_t, PaletteTranslucentColormapFunc>(drawspan);
}

//
// R_DrawSpanP
//
// Renders a span for a level plane to the 8bpp palettized screen buffer from
// the source buffer ds_source. Shading is performed using ds_colormap.
//
void R_DrawSpanP(drawspan_t& drawspan)
{
	R_DrawLevelSpanGeneric<palindex_t, PaletteColormapFunc>(drawspan);
}

//
// R_DrawSlopeSpanP
//
// Renders a span for a sloped plane to the 8bpp palettized screen buffer from
// the source buffer ds_source. Shading is performed using ds_colormap.
//
void R_DrawSlopeSpanP(drawspan_t& drawspan)
{
	R_DrawSlopedSpanGeneric<palindex_t, PaletteSlopeColormapFunc>(drawspan);
}


/****************************************/
/*										*/
/* [RH] ARGB8888 drawers (C versions)	*/
/*										*/
/****************************************/

// ----------------------------------------------------------------------------
//
// 32bpp color remapping functors
//
// These functors provide a variety of ways to manipulate a source pixel
// color (given by 8bpp palette index) and write the result to the destination
// buffer.
//
// The functors are instantiated with a shaderef_t* parameter (typically
// dcol.colormap or ds_colormap) that will be used to shade the pixel.
//
// ----------------------------------------------------------------------------

class DirectFunc
{
public:
	DirectFunc(const drawcolumn_t& drawcolumn) { }
	DirectFunc(const drawspan_t& drawspan) { }

	forceinline void operator()(byte c, argb_t* dest) const
	{
		*dest = basecolormap.shade(c);
	}
};

class DirectColormapFunc
{
public:
	DirectColormapFunc(const drawcolumn_t& drawcolumn) :
			colormap(drawcolumn.colormap) { }
	DirectColormapFunc(const drawspan_t& drawspan) :
			colormap(drawspan.colormap) { }

	forceinline void operator()(byte c, argb_t* dest) const
	{
		*dest = colormap.shade(c);
	}

private:
	const shaderef_t& colormap;
};

class DirectFuzzyFunc
{
public:
	DirectFuzzyFunc(const drawcolumn_t& drawcolumn) { }

	forceinline void operator()(byte c, argb_t* dest) const
	{
		argb_t work = dest[fuzzoffset[fuzzpos] >> 2];
		*dest = work - ((work >> 2) & 0x3f3f3f);
		fuzzpos = (fuzzpos + 1) & (FUZZTABLE - 1);
	}
};

class DirectTranslucentColormapFunc
{
public:
	DirectTranslucentColormapFunc(const drawcolumn_t& drawcolumn) :
			colormap(drawcolumn.colormap)
	{
		calculate_alpha(drawcolumn.translevel);
	}

	DirectTranslucentColormapFunc(const drawspan_t& drawspan) :
			colormap(drawspan.colormap)
	{
		calculate_alpha(drawspan.translevel);
	}

	forceinline void operator()(byte c, argb_t* dest) const
	{
		argb_t fg = colormap.shade(c);
		argb_t bg = *dest;
		*dest = alphablend2a(bg, bga, fg, fga);	
	}

private:
	void calculate_alpha(fixed_t translevel)
	{
		fga = (translevel & ~0x03FF) >> 8;
		bga = 255 - fga;
	}

	const shaderef_t& colormap;
	int fga, bga;
};

class DirectTranslatedColormapFunc
{
public:
	DirectTranslatedColormapFunc(const drawcolumn_t& drawcolumn) :
			colormap(drawcolumn.colormap), translation(drawcolumn.translation) { }

	forceinline void operator()(byte c, argb_t* dest) const
	{
		*dest = colormap.tlate(translation, c);
	}

private:
	const shaderef_t& colormap;
	const translationref_t& translation;
};

class DirectTranslatedTranslucentColormapFunc
{
public:
	DirectTranslatedTranslucentColormapFunc(const drawcolumn_t& drawcolumn) :
			tlatefunc(drawcolumn), translation(drawcolumn.translation) { }

	forceinline void operator()(byte c, argb_t* dest) const
	{
		tlatefunc(translation.tlate(c), dest);
	}

private:
	DirectTranslucentColormapFunc tlatefunc;
	const translationref_t& translation;
};

class DirectSlopeColormapFunc
{
public:
	DirectSlopeColormapFunc(const drawspan_t& drawspan) :
			colormap(drawspan.slopelighting) { }

	forceinline void operator()(byte c, argb_t* dest)
	{
		*dest = colormap->shade(c);
		colormap++;
	}

private:
	const shaderef_t* colormap;
};


// ----------------------------------------------------------------------------
//
// 32bpp color drawing wrappers
//
// ----------------------------------------------------------------------------

//
// R_FillColumnD
//
// Fills a column in the 32bpp ARGB8888 screen buffer with a solid color,
// determined by dcol.color. Performs no shading.
//
void R_FillColumnD(drawcolumn_t& drawcolumn)
{
	R_FillColumnGeneric<argb_t, DirectFunc>(drawcolumn);
}

//
// R_FillMaskedColumnD
//
// Fills a column in the 32bpp ARGB8888 screen buffer with a solid color,
// determined by dcol.color. Performs no shading.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_FillMaskedColumnD(drawcolumn_t& drawcolumn)
{
	R_FillMaskedColumnGeneric<argb_t, DirectFunc>(drawcolumn);
}

//
// R_DrawColumnD
//
// Renders a column to the 32bpp ARGB8888 screen buffer from the source buffer
// dcol.source and scaled by dcol.iscale. Shading is performed using dcol.colormap.
//
void R_DrawColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<argb_t, DirectColormapFunc>(drawcolumn);
}

//
// R_DrawMaskedColumnD
//
// Renders a column to the 32bpp ARGB8888 screen buffer from the source buffer
// dc_source and scaled by dc_iscale. Shading is performed using dc_colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawMaskedColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<argb_t, DirectColormapFunc>(drawcolumn);
}

//
// R_DrawFuzzColumnD
//
// Alters a column in the 32bpp ARGB8888 screen buffer using Doom's partial
// invisibility effect, which shades the column and rearranges the ordering
// the pixels to create distortion. Shading is performed using colormap 6.
//
void R_DrawFuzzColumnD(drawcolumn_t& drawcolumn)
{
	// adjust the borders (prevent buffer over/under-reads)
	dcol.yl = MIN(1, dcol.yl);
	dcol.yh = MAX(realviewheight - 2, dcol.yh);
	dcol.dest = R_CalculateDestination(dcol);

	R_FillColumnGeneric<argb_t, DirectFuzzyFunc>(drawcolumn);
}

//
// R_DrawFuzzMaskedColumnD
//
// Alters a column in the 32bpp ARGB8888 screen buffer using Doom's partial
// invisibility effect, which shades the column and rearranges the ordering
// the pixels to create distortion. Shading is performed using colormap 6.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawFuzzMaskedColumnD(drawcolumn_t& drawcolumn)
{
	// adjust the borders (prevent buffer over/under-reads)
	dcol.yl = MIN(1, dcol.yl);
	dcol.yh = MAX(realviewheight - 2, dcol.yh);
	dcol.dest = R_CalculateDestination(dcol);

	R_FillMaskedColumnGeneric<argb_t, DirectFuzzyFunc>(drawcolumn);
}

//
// R_DrawTranslucentColumnD
//
// Renders a translucent column to the 32bpp ARGB8888 screen buffer from the
// source buffer dcol.source and scaled by dcol.iscale. The amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
//
void R_DrawTranslucentColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<argb_t, DirectTranslucentColormapFunc>(drawcolumn);
}

//
// R_DrawTranslucentMaskedColumnD
//
// Renders a translucent column to the 32bpp ARGB8888 screen buffer from the
// source buffer dcol.source and scaled by dcol.iscale. The amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawTranslucentMaskedColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<argb_t, DirectTranslucentColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedColumnD
//
// Renders a column to the 32bpp ARGB8888 screen buffer with color-remapping
// from the source buffer dcol.source and scaled by dcol.iscale. The translation
// table is supplied by dcol.translation. Shading is performed using dcol.colormap.
//
void R_DrawTranslatedColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<argb_t, DirectTranslatedColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedMaskedColumnD
//
// Renders a column to the 32bpp ARGB8888 screen buffer with color-remapping
// from the source buffer dcol.source and scaled by dcol.iscale. The translation
// table is supplied by dcol.translation. Shading is performed using dcol.colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawTranslatedMaskedColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<argb_t, DirectTranslatedColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedTranslucentColumnD
//
// Renders a translucent column to the 32bpp ARGB8888 screen buffer with
// color-remapping from the source buffer dcol.source and scaled by dcol.iscale. 
// The translation table is supplied by dcol.translation and the amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
//
void R_DrawTranslatedTranslucentColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawColumnGeneric<argb_t, DirectTranslatedTranslucentColormapFunc>(drawcolumn);
}

//
// R_DrawTranslatedTranslucentMaskedColumnD
//
// Renders a translucent column to the 32bpp ARGB8888 screen buffer with
// color-remapping from the source buffer dcol.source and scaled by dcol.iscale. 
// The translation table is supplied by dcol.translation and the amount of
// translucency is controlled by dcol.translevel. Shading is performed using
// dcol.colormap.
// Pixels identified with a 0 in dcol.mask are not drawn.
//
void R_DrawTranslatedTranslucentMaskedColumnD(drawcolumn_t& drawcolumn)
{
	R_DrawMaskedColumnGeneric<argb_t, DirectTranslatedTranslucentColormapFunc>(drawcolumn);
}

// ----------------------------------------------------------------------------
//
// 32bpp color span drawing wrappers
//
// ----------------------------------------------------------------------------

//
// R_FillSpanD
//
// Fills a span in the 32bpp ARGB8888 screen buffer with a solid color,
// determined by ds_color. Performs no shading.
//
void R_FillSpanD(drawspan_t& drawspan)
{
	R_FillSpanGeneric<argb_t, DirectFunc>(drawspan);
}

//
// R_FillTranslucentSpanD
//
// Fills a span in the 32bpp ARGB8888 screen buffer with a solid color,
// determined by ds_color using translucency. Shading is performed 
// using ds_colormap.
//
void R_FillTranslucentSpanD(drawspan_t& drawspan)
{
	R_FillSpanGeneric<argb_t, DirectTranslucentColormapFunc>(drawspan);
}

//
// R_DrawSpanD
//
// Renders a span for a level plane to the 32bpp ARGB8888 screen buffer from
// the source buffer ds_source. Shading is performed using ds_colormap.
//
void R_DrawSpanD_c(drawspan_t& drawspan)
{
	R_DrawLevelSpanGeneric<argb_t, DirectColormapFunc>(drawspan);
}

//
// R_DrawSlopeSpanD
//
// Renders a span for a sloped plane to the 32bpp ARGB8888 screen buffer from
// the source buffer ds_source. Shading is performed using ds_colormap.
//
void R_DrawSlopeSpanD_c(drawspan_t& drawspan)
{
	R_DrawSlopedSpanGeneric<argb_t, DirectSlopeColormapFunc>(drawspan);
}


/****************************************************/

//
// R_DrawColumnRange
//
// Draws a range of columns to the screen.
// Columns are written to the screen buffer in 64x64 tiles for better cache
// usage.
//
void R_DrawColumnRange(int start, int stop, int* top, int* bottom, byte** cols, 
						const shaderef_t* colormap_table, void (*colblast)())
{
	if (start > stop)
		return;

	const int BLOCKBITS = 6;
	const int BLOCKSIZE = (1 << BLOCKBITS);
	const int BLOCKMASK = (BLOCKSIZE - 1);

	// [SL] Render the range of columns in 64x64 pixel blocks, aligned to a grid
	// on the screen. This is to make better use of spatial locality in the cache.
	for (int bx = start; bx <= stop; bx = (bx & ~BLOCKMASK) + BLOCKSIZE)
	{
		int blockstartx = bx;
		int blockstopx = MIN((bx & ~BLOCKMASK) + BLOCKSIZE - 1, stop);

		int miny = R_ColumnRangeMinimumHeight(blockstartx, blockstopx, top);
		int maxy = R_ColumnRangeMaximumHeight(blockstartx, blockstopx, bottom);

		for (int by = miny; by <= maxy; by = (by & ~BLOCKMASK) + BLOCKSIZE)
		{
			int blockstarty = by;
			int blockstopy = (by & ~BLOCKMASK) + BLOCKSIZE - 1;

			for (int x = blockstartx; x <= blockstopx; x++)
			{
				dcol.x = x;
				dcol.yl = MAX(top[x], blockstarty);
				dcol.yh = MIN(bottom[x], blockstopy);
				dcol.source = cols[x];
				dcol.dest = R_CalculateDestination(dcol);
				dcol.colormap = colormap_table[x];

				colblast();
			}
		}
	}
}


//
// R_InitBuffer 
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void R_InitBuffer(int width, int height) 
{ 
	int 		i;
	byte		*buffer;
	int			pitch;
	int			xshift;

	// Handle resize,
	//	e.g. smaller view windows
	//	with border and/or status bar.
	viewwindowx = (screen->width-(width<<detailxshift))>>1;

	// [RH] Adjust column offset according to bytes per pixel
	//		and detail mode
	xshift = (screen->is8bit()) ? 0 : 2;
	xshift += detailxshift;

	// Column offset. For windows
	for (i = 0; i < width; i++)
		columnofs[i] = (viewwindowx + i) << xshift;

	// Same with base row offset.
	if ((width<<detailxshift) == screen->width)
		viewwindowy = 0;
	else
		viewwindowy = (ST_Y-(height<<detailyshift)) >> 1;

	screen->Lock ();
	buffer = screen->buffer;
	pitch = screen->pitch;
	screen->Unlock ();

	// Precalculate all row offsets.
	for (i=0 ; i<height ; i++)
		ylookup[i] = buffer + ((i<<detailyshift)+viewwindowy)*pitch;
}


void R_DrawBorder (int x1, int y1, int x2, int y2)
{
	int lump;

	lump = W_CheckNumForName (gameinfo.borderFlat, ns_flats);
	if (lump >= 0)
	{
		screen->FlatFill (x1 & ~63, y1, x2, y2,
			(byte *)W_CacheLumpNum (lump, PU_CACHE));
	}
	else
	{
		screen->Clear (x1, y1, x2, y2, 0);
	}
}


//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void V_MarkRect (int x, int y, int width, int height);

void R_DrawViewBorder (void)
{
	int x, y;
	int offset, size;
	gameborder_t *border;

	if (realviewwidth == screen->width) {
		return;
	}

	border = gameinfo.border;
	offset = border->offset;
	size = border->size;

	R_DrawBorder (0, 0, screen->width, viewwindowy);
	R_DrawBorder (0, viewwindowy, viewwindowx, realviewheight + viewwindowy);
	R_DrawBorder (viewwindowx + realviewwidth, viewwindowy, screen->width, realviewheight + viewwindowy);
	R_DrawBorder (0, viewwindowy + realviewheight, screen->width, ST_Y);

	for (x = viewwindowx; x < viewwindowx + realviewwidth; x += size)
	{
		screen->DrawPatch (W_CachePatch (border->t),
			x, viewwindowy - offset);
		screen->DrawPatch (W_CachePatch (border->b),
			x, viewwindowy + realviewheight);
	}
	for (y = viewwindowy; y < viewwindowy + realviewheight; y += size)
	{
		screen->DrawPatch (W_CachePatch (border->l),
			viewwindowx - offset, y);
		screen->DrawPatch (W_CachePatch (border->r),
			viewwindowx + realviewwidth, y);
	}
	// Draw beveled edge.
	screen->DrawPatch (W_CachePatch (border->tl),
		viewwindowx-offset, viewwindowy-offset);
	
	screen->DrawPatch (W_CachePatch (border->tr),
		viewwindowx+realviewwidth, viewwindowy-offset);
	
	screen->DrawPatch (W_CachePatch (border->bl),
		viewwindowx-offset, viewwindowy+realviewheight);
	
	screen->DrawPatch (W_CachePatch (border->br),
		viewwindowx+realviewwidth, viewwindowy+realviewheight);

	V_MarkRect (0, 0, screen->width, ST_Y);
}

// [RH] Double pixels in the view window horizontally
//		and/or vertically (or not at all).
void R_DetailDouble (void)
{
	switch ((detailxshift << 1) | detailyshift)
	{
		case 1:		// y-double
		{
			int rowsize = realviewwidth << ((screen->is8bit()) ? 0 : 2);
			int pitch = screen->pitch;
			int y;
			byte *line;

			line = screen->buffer + viewwindowy*pitch + viewwindowx;
			for (y = 0; y < viewheight; y++, line += pitch<<1)
			{
				memcpy (line+pitch, line, rowsize);
			}
		}
		break;

		case 2:		// x-double
		{
			int rowsize = realviewwidth >> 2;
			int pitch = screen->pitch >> (2-detailyshift);
			int y,x;
			unsigned *line,a,b;

			line = (unsigned *)(screen->buffer + viewwindowy*screen->pitch + viewwindowx);
			for (y = 0; y < viewheight; y++, line += pitch)
			{
				for (x = 0; x < rowsize; x += 2)
				{
					a = line[x+0];
					b = line[x+1];
					a &= 0x00ff00ff;
					b &= 0x00ff00ff;
					line[x+0] = a | (a << 8);
					line[x+1] = b | (b << 8);
				}
			}
		}
		break;

		case 3:		// x- and y-double
		{
			int rowsize = realviewwidth >> 2;
			int pitch = screen->pitch >> (2-detailyshift);
			int realpitch = screen->pitch >> 2;
			int y,x;
			unsigned *line,a,b;

			line = (unsigned *)(screen->buffer + viewwindowy*screen->pitch + viewwindowx);
			for (y = 0; y < viewheight; y++, line += pitch)
			{
				for (x = 0; x < rowsize; x += 2)
				{
					a = line[x+0];
					b = line[x+1];
					a &= 0x00ff00ff;
					b &= 0x00ff00ff;
					line[x+0] = a | (a << 8);
					line[x+0+realpitch] = a | (a << 8);
					line[x+1] = b | (b << 8);
					line[x+1+realpitch] = b | (b << 8);
				}
			}
		}
		break;
	}
}

enum r_optimize_kind {
	OPTIMIZE_NONE,
	OPTIMIZE_SSE2,
	OPTIMIZE_MMX,
	OPTIMIZE_ALTIVEC
};

static r_optimize_kind optimize_kind = OPTIMIZE_NONE;
static std::vector<r_optimize_kind> optimizations_available;

static const char *get_optimization_name(r_optimize_kind kind)
{
	switch (kind)
	{
		case OPTIMIZE_SSE2:    return "sse2";
		case OPTIMIZE_MMX:     return "mmx";
		case OPTIMIZE_ALTIVEC: return "altivec";
		case OPTIMIZE_NONE:
		default:
			return "none";
	}
}

static std::string get_optimization_name_list(const bool includeNone)
{
	std::string list;
	std::vector<r_optimize_kind>::iterator it = optimizations_available.begin();
	if (!includeNone) ++it;
	for (; it != optimizations_available.end(); ++it)
	{
		list.append(get_optimization_name(*it));
		if (it+1 != optimizations_available.end())
			list.append(", ");
	}
	return list;
}

static void print_optimizations()
{
	Printf(PRINT_HIGH, "r_optimize detected \"%s\"\n", get_optimization_name_list(false).c_str());
}

static bool detect_optimizations()
{
	if (optimizations_available.size() != 0)
		return false;

	optimizations_available.clear();

	// Start with default non-optimized:
	optimizations_available.push_back(OPTIMIZE_NONE);

	// Detect CPU features in ascending order of preference:
#ifdef __MMX__
	if (SDL_HasMMX())
	{
		optimizations_available.push_back(OPTIMIZE_MMX);
	}
#endif

#ifdef __SSE2__
	if (SDL_HasSSE2())
	{
		optimizations_available.push_back(OPTIMIZE_SSE2);
	}
#endif

#ifdef __ALTIVEC__
	if (SDL_HasAltiVec())
	{
		optimizations_available.push_back(OPTIMIZE_ALTIVEC);
	}
#endif

	return true;
}

CVAR_FUNC_IMPL (r_optimize)
{
	// NOTE(jsd): Stupid hack to prevent stack overflow when trying to set the value from within this callback.
	static bool resetting = false;
	if (resetting)
	{
		resetting = false;
		return;
	}

	const char *val = var.cstring();
	//Printf(PRINT_HIGH, "r_optimize called with \"%s\"\n", val);

	// Only print the detected list the first time:
	if (detect_optimizations())
		print_optimizations();

	// Set the optimization based on availability:
	r_optimize_kind trykind = optimize_kind;
	if (stricmp(val, "none") == 0)
		trykind = OPTIMIZE_NONE;
	else if (stricmp(val, "sse2") == 0)
		trykind = OPTIMIZE_SSE2;
	else if (stricmp(val, "mmx") == 0)
		trykind = OPTIMIZE_MMX;
	else if (stricmp(val, "altivec") == 0)
		trykind = OPTIMIZE_ALTIVEC;
	else if (stricmp(val, "detect") == 0)
		// Default to the most preferred:
		trykind = optimizations_available.back();
	else
	{
		Printf(PRINT_HIGH, "Invalid value for r_optimize. Try one of \"%s, detect\"\n", get_optimization_name_list(true).c_str());

		// Restore the original setting:
		resetting = true;
		var.Set(get_optimization_name(optimize_kind));
		R_InitDrawers();
		R_InitColumnDrawers();
		return;
	}

	// If we found the CPU feature, use it:
	std::vector<r_optimize_kind>::iterator it = std::find(optimizations_available.begin(), optimizations_available.end(), trykind);
	if (it != optimizations_available.end())
	{
		optimize_kind = trykind;
		R_InitDrawers();
		R_InitColumnDrawers();
	}

	// Update the cvar string:
	const char *resetname = get_optimization_name(optimize_kind);
	Printf(PRINT_HIGH, "r_optimize set to \"%s\" based on availability\n", resetname);
	resetting = true;
	var.Set(resetname);
}

// Sets up the r_*D function pointers based on CPU optimization selected
void R_InitDrawers ()
{
	if (optimize_kind == OPTIMIZE_SSE2)
	{
#ifdef __SSE2__
		R_DrawSpanD				= R_DrawSpanD_SSE2;
		R_DrawSlopeSpanD		= R_DrawSlopeSpanD_SSE2;
		r_dimpatchD             = r_dimpatchD_SSE2;
#else
		// No SSE2 support compiled in.
		optimize_kind = OPTIMIZE_NONE;
		goto setNone;
#endif
	}
	else if (optimize_kind == OPTIMIZE_MMX)
	{
#ifdef __MMX__
		R_DrawSpanD				= R_DrawSpanD_c;		// TODO
		R_DrawSlopeSpanD		= R_DrawSlopeSpanD_c;	// TODO
		r_dimpatchD             = r_dimpatchD_MMX;
#else
		// No MMX support compiled in.
		optimize_kind = OPTIMIZE_NONE;
		goto setNone;
#endif
	}
	else if (optimize_kind == OPTIMIZE_ALTIVEC)
	{
#ifdef __ALTIVEC__
		R_DrawSpanD				= R_DrawSpanD_c;		// TODO
		R_DrawSlopeSpanD		= R_DrawSlopeSpanD_c;	// TODO
		r_dimpatchD             = r_dimpatchD_ALTIVEC;
#else
		// No ALTIVEC support compiled in.
		optimize_kind = OPTIMIZE_NONE;
		goto setNone;
#endif
	}
	else
	{
		// No CPU vectorization available.
setNone:
		R_DrawSpanD				= R_DrawSpanD_c;
		R_DrawSlopeSpanD		= R_DrawSlopeSpanD_c;
		r_dimpatchD             = r_dimpatchD_c;
	}

	// Check that all pointers are definitely assigned!
	assert(R_DrawSpanD != NULL);
	assert(R_DrawSlopeSpanD != NULL);
	assert(r_dimpatchD != NULL);
}

// [RH] Initialize the column drawer pointers
void R_InitColumnDrawers ()
{
	if (!screen)
		return;

	if (screen->is8bit())
	{
		R_FillColumn					= R_FillColumnP;
		R_FillMaskedColumn				= R_FillMaskedColumnP;
		R_DrawColumn					= R_DrawColumnP;
		R_DrawMaskedColumn				= R_DrawMaskedColumnP;
		R_DrawFuzzColumn				= R_DrawFuzzColumnP;
		R_DrawFuzzMaskedColumn			= R_DrawFuzzMaskedColumnP;
		R_DrawTranslucentColumn			= R_DrawTranslucentColumnP;
		R_DrawTranslucentMaskedColumn	= R_DrawTranslucentMaskedColumnP;
		R_DrawTranslatedColumn			= R_DrawTranslatedColumnP;
		R_DrawTranslatedMaskedColumn	= R_DrawTranslatedMaskedColumnP;
		R_DrawSlopeSpan					= R_DrawSlopeSpanP;
		R_DrawSpan						= R_DrawSpanP;
		R_FillSpan						= R_FillSpanP;
		R_FillTranslucentSpan			= R_FillTranslucentSpanP;
		R_DrawTranslatedTranslucentColumn = R_DrawTranslatedTranslucentColumnP;
		R_DrawTranslatedTranslucentMaskedColumn = R_DrawTranslatedTranslucentMaskedColumnP;
	}
	else
	{
		// 32bpp rendering functions:
		R_FillColumn					= R_FillColumnD;
		R_FillMaskedColumn				= R_FillMaskedColumnD;
		R_DrawColumn					= R_DrawColumnD;
		R_DrawMaskedColumn				= R_DrawMaskedColumnD;
		R_DrawFuzzColumn				= R_DrawFuzzColumnD;
		R_DrawFuzzMaskedColumn			= R_DrawFuzzMaskedColumnD;
		R_DrawTranslucentColumn			= R_DrawTranslucentColumnD;
		R_DrawTranslucentMaskedColumn	= R_DrawTranslucentMaskedColumnD;
		R_DrawTranslatedColumn			= R_DrawTranslatedColumnD;
		R_DrawTranslatedMaskedColumn	= R_DrawTranslatedMaskedColumnD;
		R_DrawSlopeSpan					= R_DrawSlopeSpanD;
		R_DrawSpan						= R_DrawSpanD;
		R_FillSpan						= R_FillSpanD;
		R_FillTranslucentSpan			= R_FillTranslucentSpanD;
		R_DrawTranslatedTranslucentColumn = R_DrawTranslatedTranslucentColumnD;
		R_DrawTranslatedTranslucentMaskedColumn = R_DrawTranslatedTranslucentMaskedColumnD;
	}
}

void r_dimpatchD_c(const DCanvas *const cvs, argb_t color, int alpha, int x1, int y1, int w, int h)
{
	int dpitch = cvs->pitch / sizeof(argb_t);
	argb_t* line = (argb_t *)cvs->buffer + y1 * dpitch;

	for (int y = y1; y < y1 + h; y++)
	{
		for (int x = x1; x < x1 + w; x++)
			line[x] = alphablend1a(line[x], color, alpha);

		line += dpitch;
	}
}

VERSION_CONTROL (r_draw_cpp, "$Id$")

