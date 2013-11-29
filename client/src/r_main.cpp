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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//
//-----------------------------------------------------------------------------

#include "m_alloc.h"
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "doomdef.h"
#include "d_net.h"
#include "doomstat.h"
#include "m_random.h"
#include "m_bbox.h"
#include "p_local.h"
#include "r_local.h"
#include "r_sky.h"
#include "st_stuff.h"
#include "c_cvars.h"
#include "v_video.h"
#include "stats.h"
#include "z_zone.h"
#include "i_video.h"
#include "m_vectors.h"
#include "f_wipe.h"

void R_BeginInterpolation(fixed_t amount);
void R_EndInterpolation();
void R_InterpolateCamera(fixed_t amount);

#define DISTMAP			2

int negonearray[MAXWIDTH];
int viewheightarray[MAXWIDTH];

void R_SpanInitData ();

extern int *walllights;

// [RH] Defined in d_main.cpp
extern dyncolormap_t NormalLight;
extern bool r_fakingunderwater;

EXTERN_CVAR (r_flashhom)
EXTERN_CVAR (r_viewsize)
EXTERN_CVAR (sv_allowwidescreen)

static float	LastFOV = 0.0f;
fixed_t			FocalLengthX;
fixed_t			FocalLengthY;
float			xfoc;		// FIXEDFLOAT(FocalLengthX)
float			yfoc;		// FIXEDFLOAT(FocalLengthY)
fixed_t			fovtan;
float			focratio;
float			ifocratio;
int 			viewangleoffset = 0;

// increment every time a check is made
int 			validcount = 1;

// [RH] colormap currently drawing with
shaderef_t		basecolormap;
int				fixedlightlev = -1;
shaderef_t		fixedcolormap;

shaderef_t		fixed_light_colormap_table[MAXWIDTH];
shaderef_t		fixed_colormap_table[MAXWIDTH];

int 			centerx;
extern "C" {int	centery; }

fixed_t 		centerxfrac;
fixed_t 		centeryfrac;
fixed_t			yaspectmul;

// just for profiling purposes
int 			framecount;
int 			linecount;
int 			loopcount;

fixed_t 		viewx;
fixed_t 		viewy;
fixed_t 		viewz;

angle_t 		viewangle;

int32_t 		viewcos;
int32_t 		viewsin;

AActor			*camera;	// [RH] camera to draw from. doesn't have to be a player

//
// precalculated math tables
//

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
int 			viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t 		*xtoviewangle;

const fixed_t	*finecosine = &finesine[FINEANGLES/4];

int				scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
int				scalelightfixed[MAXLIGHTSCALE];
int				zlight[LIGHTLEVELS][MAXLIGHTZ];

// [RH] used to keep hires modes dark enough
int				lightscalexmul;
int				lightscaleymul;

// bumped light from gun blasts
int 			extralight;

// [RH] ignore extralight and fullbright
BOOL			foggy;

BOOL			setsizeneeded;
int				setblocks, setdetail = -1;

fixed_t			freelookviewheight;

unsigned int	R_OldBlend = ~0;

void (*colfunc)(drawcolumn_t&);
void (*maskedcolfunc)(drawcolumn_t&);
void (*spanfunc)(drawspan_t&);
void (*spanslopefunc)(drawspan_t&);

// [AM] Number of fineangles in a default 90 degree FOV at a 4:3 resolution.
int FieldOfView = 2048;
int CorrectFieldOfView = 2048;

fixed_t			render_lerp_amount;

//
//
// R_PointOnSide
//
// Determines which side of a line the point (x, y) is on.
// Returns side 0 (front) or 1 (back) 
//
int R_PointOnSide(fixed_t x, fixed_t y, fixed_t xl, fixed_t yl, fixed_t xh, fixed_t yh)
{
	return int64_t(xh - xl) * (y - yl) - int64_t(yh - yl) * (x - xl) >= 0;
}

//
//
// R_PointOnSide
//
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
// [SL] 2013-02-06 - Changed to use cross product a la ZDoom
//
int R_PointOnSide(fixed_t x, fixed_t y, const node_t *node)
{
	return R_PointOnSide(x, y, node->x, node->y, node->x + node->dx, node->y + node->dy); 
}

//
//
// R_PointOnSegSide
//
// Same, except takes a lineseg as input instead of a linedef
//
// killough 5/2/98: reformatted
// [SL] 2013-02-06 - Changed to use cross product a la ZDoom
//
int R_PointOnSegSide (fixed_t x, fixed_t y, const seg_t *line)
{
	return R_PointOnSide(x, y, line->v1->x, line->v1->y, line->v2->x, line->v2->y);
}


//
// R_PointOnLine
//
// Determines if a point is sitting on a line.
//
bool R_PointOnLine(fixed_t x, fixed_t y, fixed_t xl, fixed_t yl, fixed_t xh, fixed_t yh)
{
	return int64_t(xh - xl) * (y - yl) - int64_t(yh - yl) * (x - xl) == 0;
}


//
// R_PointToAngle
//
// To get a global angle from cartesian coordinates, the coordinates are
// flipped until they are in the first octant of the coordinate system,
// then the y (<=x) is scaled and divided by x to get a tangent (slope)
// value which is looked up in the tantoangle[] table.
//
// This version is from prboom-plus
//
angle_t R_PointToAngle2(fixed_t viewx, fixed_t viewy, fixed_t x, fixed_t y)
{
	return (y -= viewy, (x -= viewx) || y) ?
		x >= 0 ?
			y >= 0 ?
				(x > y) ? tantoangle[SlopeDiv(y,x)] :						// octant 0
					ANG90-1-tantoangle[SlopeDiv(x,y)] :						// octant 1
				x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :				// octant 8
						ANG270+tantoangle[SlopeDiv(x,y)] :					// octant 7
			y >= 0 ? (x = -x) > y ? ANG180-1-tantoangle[SlopeDiv(y,x)] :	// octant 3
							ANG90 + tantoangle[SlopeDiv(x,y)] :				// octant 2
				(x = -x) > (y = -y) ? ANG180+tantoangle[ SlopeDiv(y,x)] :	// octant 4
								ANG270-1-tantoangle[SlopeDiv(x,y)] :		// octant 5
		0;
}

//
// R_PointToAngle - wrapper around R_PointToAngle2
//
angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
	return R_PointToAngle2(viewx, viewy, x, y);
}

//
// R_PointToDist
//
// Returns the distance from the viewpoint to some other point. In a
// floating point environment, we'd probably be better off using the
// Pythagorean Theorem to determine the result.
//
// killough 5/2/98: simplified
// [RH] Simplified further [sin (t + 90 deg) == cos (t)]
//
fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
	fixed_t dx = abs(x - viewx);
	fixed_t dy = abs(y - viewy);

	if (dy > dx)
	{
		fixed_t t = dx;
		dx = dy;
		dy = t;
	}

	return FixedDiv(dx, finecosine[tantoangle[FixedDiv(dy, dx) >> DBITS] >> ANGLETOFINESHIFT]);
}

//
//
// R_PointToDist2
//
//
fixed_t R_PointToDist2(fixed_t dx, fixed_t dy)
{
	dx = abs (dx);
	dy = abs (dy);

	if (dy > dx)
	{
		fixed_t t = dx;
		dx = dy;
		dy = t;
	}

	return FixedDiv(dx, finecosine[tantoangle[FixedDiv(dy, dx) >> DBITS] >> ANGLETOFINESHIFT]);
}


//
// R_RotatePoint
//
// Rotates a point x, y by viewangle and stores the result in tx, ty
//
void R_RotatePoint(fixed_t x, fixed_t y, fixed_t &tx, fixed_t &ty)
{
	tx = FixedMul<16, 30, 16>(x, viewcos) - FixedMul<16, 30, 16>(y, viewsin);
	ty = FixedMul<16, 30, 16>(x, viewsin) + FixedMul<16, 30, 16>(y, viewcos);
}

//
// R_ClipLine
//
// Clips the endpoints of the line defined by in1 & in2 and stores the
// results in out1 & out2. The clipped left endpoint is lclip percent
// of the way between the left and right endpoints and similarly the
// right endpoint is rclip percent of the way between the left and right
// endpoints
//
void R_ClipLine(const v2fixed_t* in1, const v2fixed_t* in2, 
				int32_t lclip, int32_t rclip,
				v2fixed_t* out1, v2fixed_t* out2)
{
	const fixed_t dx = in2->x - in1->x;
	const fixed_t dy = in2->y - in1->y;
	const fixed_t x = in1->x;
	const fixed_t y = in1->y;
	out1->x = x + FixedMul<30, 16, 16>(lclip, dx);
	out2->x = x + FixedMul<30, 16, 16>(rclip, dx);
	out1->y = y + FixedMul<30, 16, 16>(lclip, dy);
	out2->y = y + FixedMul<30, 16, 16>(rclip, dy);
}

void R_ClipLine(const vertex_t* in1, const vertex_t* in2,
				int32_t lclip, int32_t rclip,
				v2fixed_t* out1, v2fixed_t* out2)
{
	R_ClipLine((const v2fixed_t*)in1, (const v2fixed_t*)in2, lclip, rclip, out1, out2);
}

//
// R_ClipLineToFrustum
//
// Clips the endpoints of a line to the view frustum.
// v1 and v2 are the endpoints of the line
// clipdist is the distance from the viewer to the near plane of the frustum.
// The left endpoint of the line should be clipped lclip percent of the way
// between the left and right endpoints and similarly the right endpoint
// should be clipped rclip percent of the way between the left and right
// endpoints.
//
// Returns false if the line is entirely clipped.
//
bool R_ClipLineToFrustum(const v2fixed_t* v1, const v2fixed_t* v2, fixed_t clipdist, int32_t& lclip, int32_t& rclip)
{
	static const int32_t CLIPUNIT = 1 << 30;
	v2fixed_t p1 = *v1, p2 = *v2;

	lclip = 0;
	rclip = CLIPUNIT; 

	// Clip portions of the line that are behind the view plane
	if (p1.y < clipdist)
	{      
		// reject the line entirely if the whole thing is behind the view plane.
		if (p2.y < clipdist)
			return false;

		// clip the line at the point where p1.y == clipdist
		lclip = FixedDiv<16, 16, 30>(clipdist - p1.y, p2.y - p1.y);
	}

	if (p2.y < clipdist)
	{
		// clip the line at the point where p2.y == clipdist
		rclip = FixedDiv<16, 16, 30>(clipdist - p1.y, p2.y - p1.y);
	}

	int32_t unclipped_amount = rclip - lclip;

	// apply the clipping against the 'y = clipdist' plane to p1 & p2
	R_ClipLine(v1, v2, lclip, rclip, &p1, &p2);

	// [SL] A note on clipping to the screen edges:
	// With a 90-degree FOV, if p1.x < -p1.y, then the left point
	// is off the left side of the screen. Similarly, if p2.x > p2.y,
	// then the right point is off the right side of the screen.
	// We use yc1 and yc2 instead of p1.y and p2.y because they are
	// adjusted to work with the current FOV rather than just 90-degrees.
	fixed_t yc1 = FixedMul(fovtan, p1.y);
	fixed_t yc2 = FixedMul(fovtan, p2.y);

	// is the entire line off the left side or the right side of the screen?
	if ((p1.x < -yc1 && p2.x < -yc2) || (p1.x > yc1 && p2.x > yc2))
		return false;

	// is the left vertex off the left side of the screen?
	if (p1.x < -yc1)
	{
		// clip line at left edge of the screen
		fixed_t den = p2.x - p1.x + yc2 - yc1;
		if (den == 0)
			return false;

		int32_t t = FixedDiv<16, 16, 30>(-yc1 - p1.x, den);
		lclip += FixedMul<30, 30, 30>(t, unclipped_amount);
	}

	// is the right vertex off the right side of the screen?
	if (p2.x > yc2)
	{
		// clip line at right edge of the screen
		fixed_t den = p2.x - p1.x - yc2 + yc1;	
		if (den == 0)
			return false;

		int32_t t = FixedDiv<16, 16, 30>(yc1 - p1.x, den);
		rclip -= FixedMul<30, 30, 30>(CLIPUNIT - t, unclipped_amount);
	}

	if (lclip > rclip)
		return false;

	return true;
}


// bits of precision for translated seg_t vertices in R_ProjectSeg
static const int PREC = 16;


static void R_ClipLine2(const v2fixed_t* in1, const v2fixed_t* in2, 
				int32_t lclip, int32_t rclip,
				v2fixed_t* out1, v2fixed_t* out2)
{
	const fixed_t dx = in2->x - in1->x;
	const fixed_t dy = in2->y - in1->y;
	const fixed_t x = in1->x;
	const fixed_t y = in1->y;
	out1->x = x + FixedMul<30, PREC, PREC>(lclip, dx);
	out2->x = x + FixedMul<30, PREC, PREC>(rclip, dx);
	out1->y = y + FixedMul<30, PREC, PREC>(lclip, dy);
	out2->y = y + FixedMul<30, PREC, PREC>(rclip, dy);
}

static void R_ClipLine2(const vertex_t* in1, const vertex_t* in2,
				int32_t lclip, int32_t rclip,
				v2fixed_t* out1, v2fixed_t* out2)
{
	R_ClipLine2((const v2fixed_t*)in1, (const v2fixed_t*)in2, lclip, rclip, out1, out2);
}

static bool R_ClipLineToFrustum2(const v2fixed_t* v1, const v2fixed_t* v2, fixed_t clipdist, int32_t& lclip, int32_t& rclip)
{
	static const int32_t CLIPUNIT = FRACUNIT30;
	v2fixed_t p1 = *v1, p2 = *v2;

	lclip = 0;
	rclip = CLIPUNIT; 

	// Clip portions of the line that are behind the view plane
	if (p1.y < clipdist)
	{      
		// reject the line entirely if the whole thing is behind the view plane.
		if (p2.y < clipdist)
			return false;

		// clip the line at the point where p1.y == clipdist
		lclip = FixedDiv<PREC, PREC, 30>(clipdist - p1.y, p2.y - p1.y);
	}

	if (p2.y < clipdist)
	{
		// clip the line at the point where p2.y == clipdist
		rclip = FixedDiv<PREC, PREC, 30>(clipdist - p1.y, p2.y - p1.y);
	}

	int32_t unclipped_amount = rclip - lclip;

	// apply the clipping against the 'y = clipdist' plane to p1 & p2
	R_ClipLine2(v1, v2, lclip, rclip, &p1, &p2);

	// [SL] A note on clipping to the screen edges:
	// With a 90-degree FOV, if p1.x < -p1.y, then the left point
	// is off the left side of the screen. Similarly, if p2.x > p2.y,
	// then the right point is off the right side of the screen.
	// We use yc1 and yc2 instead of p1.y and p2.y because they are
	// adjusted to work with the current FOV rather than just 90-degrees.
	fixed_t yc1 = FixedMul<16, PREC, PREC>(fovtan, p1.y);
	fixed_t yc2 = FixedMul<16, PREC, PREC>(fovtan, p2.y);

	// is the entire line off the left side or the right side of the screen?
	if ((p1.x < -yc1 && p2.x < -yc2) || (p1.x > yc1 && p2.x > yc2))
		return false;

	// is the left vertex off the left side of the screen?
	if (p1.x < -yc1)
	{
		// clip line at left edge of the screen
		fixed_t den = p2.x - p1.x + yc2 - yc1;
		if (den == 0)
			return false;

		int32_t t = FixedDiv<PREC, PREC, 30>(-yc1 - p1.x, den);
		lclip += FixedMul<30, 30, 30>(t, unclipped_amount);
	}

	// is the right vertex off the right side of the screen?
	if (p2.x > yc2)
	{
		// clip line at right edge of the screen
		fixed_t den = p2.x - p1.x - yc2 + yc1;	
		if (den == 0)
			return false;

		int32_t t = FixedDiv<PREC, PREC, 30>(yc1 - p1.x, den);
		rclip -= FixedMul<30, 30, 30>(CLIPUNIT - t, unclipped_amount);
	}

	if (lclip > rclip)
		return false;

	return true;
}

static fixed_t R_LineLength(fixed_t px1, fixed_t py1, fixed_t px2, fixed_t py2)
{
	float dx = FIXED2FLOAT(px2 - px1);
	float dy = FIXED2FLOAT(py2 - py1);
	return FLOAT2FIXED(sqrt(dx*dx + dy*dy));
}

//
// R_ProjectSeg
//
// Projects a seg_t to the screen, clipping to the view frustum, and saves
// the projection to the drawseg_t ds. The clipped vertices are saved
// as gx1, gy1 and gx2, gy2 respectively.
//
// Returns false if the entire seg_t is clipped (not in the viewable area).
//
bool R_ProjectSeg(const seg_t* segline, drawseg_t* ds, fixed_t clipdist,
	fixed_t& gx1, fixed_t& gy1, fixed_t& gx2, fixed_t& gy2)
{
	//TODO: don't hard-code this
	clipdist = (1 << PREC) / 4;

	const v2fixed_t pt1 = { segline->v1->x, segline->v1->y };
	const v2fixed_t pt2 = { segline->v2->x, segline->v2->y };

	int32_t lclip, rclip;

	// skip this line if it's not facing the camera
	if (R_PointOnSide(viewx, viewy, pt1.x, pt1.y, pt2.x, pt2.y) != 0)
		return false;

	// translate pt1 & pt2 into camera coordinates and store into t1 & t2
	v2fixed_t t1, t2;
	t1.x =	FixedMul<16, 30, PREC>(pt1.x - viewx, viewcos) -
			FixedMul<16, 30, PREC>(pt1.y - viewy, viewsin);
	t1.y =	FixedMul<16, 30, PREC>(pt1.x - viewx, viewsin) +
			FixedMul<16, 30, PREC>(pt1.y - viewy, viewcos);
	t2.x =	FixedMul<16, 30, PREC>(pt2.x - viewx, viewcos) -
			FixedMul<16, 30, PREC>(pt2.y - viewy, viewsin);
	t2.y =	FixedMul<16, 30, PREC>(pt2.x - viewx, viewsin) +
			FixedMul<16, 30, PREC>(pt2.y - viewy, viewcos);

	// clip the line seg to the viewing window
	if (!R_ClipLineToFrustum2(&t1, &t2, clipdist, lclip, rclip))
		return false;

	// apply the view frustum clipping to t1 & t2
	R_ClipLine2(&t1, &t2, lclip, rclip, &t1, &t2);

	ds->curline = segline;

	// project the line endpoints to determine which screen columns the line occupies
	ds->x1 = R_ProjectPointX(t1.x, t1.y);
	ds->x2 = R_ProjectPointX(t2.x, t2.y) - 1;
	if (!R_CheckProjectionX(ds->x1, ds->x2))
		return false;

	int width = ds->x2 - ds->x1 + 1;

	// invert t1.y (Z) and store it using 2.30 fixed-point format
	ds->invz1 = FixedDiv<0, PREC, 28>(1, t1.y);
	ds->invz2 = FixedDiv<0, PREC, 28>(1, t2.y);
	ds->invzstep = (ds->invz2 - ds->invz1) / width;

	// calculate scale values (FocalLengthY / Z)
	ds->scale1 = FixedMul<16, 28, 16>(FocalLengthY, ds->invz1);
	ds->scale2 = FixedMul<16, 28, 16>(FocalLengthY, ds->invz2);
	ds->scalestep = (ds->scale2 - ds->scale1) / width;

	// clip the line seg endpoints in world-space
	// and store in (w1.x, w1.y) and (w2.x, w2.y)
	v2fixed_t w1, w2;
	R_ClipLine(&pt1, &pt2, lclip, rclip, &w1, &w2);

	// determine which vertex of the linedef should be used for texture alignment
	vertex_t *v1;
	if (segline->linedef->sidenum[0] == segline->sidedef - sides)
		v1 = segline->linedef->v1;
	else
		v1 = segline->linedef->v2;

	// calculate seg length and texture offset
	fixed_t length = R_LineLength(w1.x, w1.y, w2.x, w2.y);
	fixed_t textureoffset = R_LineLength(v1->x, v1->y, w1.x, w1.y) + segline->sidedef->textureoffset;

	// calculate column texture mapping values (U / Z)
	// we can later retrieve U with U = Z * (U / Z)
	ds->uinvz1 = FixedMul<16, 28, 20>(textureoffset, ds->invz1);
	ds->uinvz2 = FixedMul<16, 28, 20>(textureoffset + length, ds->invz2);
	ds->uinvzstep = (ds->uinvz2 - ds->uinvz1) / width;

	gx1 = w1.x;
	gy1 = w1.y; 
	gx2 = w2.x; 
	gy2 = w2.y;

	return true;
}





//
// R_ProjectPointX
//
// Returns the screen column that a point in camera-space projects to.
//
int R_ProjectPointX(fixed_t x, fixed_t y)
{
	if (y > 0)
		return FIXED2INT(centerxfrac + int64_t(FocalLengthX) * int64_t(x) / int64_t(y));
	else
		return centerx;
}

//
// R_ProjectPointY
//
// Returns the screen row that a point in camera-space projects to.
//
int R_ProjectPointY(fixed_t z, fixed_t y)
{
	if (y > 0)
		return FIXED2INT(centeryfrac - int64_t(FocalLengthY) * int64_t(z) / int64_t(y));
	else
		return centery;
}

//
// R_CheckProjectionX
//
// Clamps the columns x1 and x2 to the viewing screen and returns false if
// the entire projection is off the screen.
//
bool R_CheckProjectionX(int &x1, int &x2)
{
	x1 = MAX(x1, 0);
	x2 = MIN(x2, viewwidth - 1);
	return (x1 <= x2);
}

//
// R_CheckProjectionY
//
// Clamps the rows y1 and y2 to the viewing screen and returns false if
// the entire projection is off the screen.
//
bool R_CheckProjectionY(int &y1, int &y2)
{
	y1 = MAX(y1, 0);
	y2 = MIN(y2, viewheight - 1);
	return (y1 <= y2);
}


//
// R_DrawPixel
//
static inline void R_DrawPixel(int x, int y, byte color)
{
	*(ylookup[y] + columnofs[x]) = color;
}


//
// R_DrawLine
//
// Draws a colored line between the two endpoints given in world coordinates.
//
void R_DrawLine(const v3fixed_t* inpt1, const v3fixed_t* inpt2, byte color)
{
	// convert from world-space to camera-space
	v3fixed_t pt1, pt2;
	R_RotatePoint(inpt1->x - viewx, inpt1->y - viewy, pt1.x, pt1.y);
	R_RotatePoint(inpt2->x - viewx, inpt2->y - viewy, pt2.x, pt2.y);
	pt1.z = inpt1->z - viewz;
	pt2.z = inpt2->z - viewz;

	if (pt1.x > pt2.x)
	{
		std::swap(pt1.x, pt2.x);
		std::swap(pt1.y, pt2.y);
		std::swap(pt1.z, pt2.z);
	}

	// convert from camera-space to screen-space
	int lclip, rclip;

	if (!R_ClipLineToFrustum((v2fixed_t*)&pt1, (v2fixed_t*)&pt2, FRACUNIT, lclip, rclip))
		return;

	R_ClipLine((v2fixed_t*)&pt1, (v2fixed_t*)&pt2, lclip, rclip, (v2fixed_t*)&pt1, (v2fixed_t*)&pt2);

	int x1 = clamp(R_ProjectPointX(pt1.x, pt1.y), 0, viewwidth - 1);
	int x2 = clamp(R_ProjectPointX(pt2.x, pt2.y), 0, viewwidth - 1);
	int y1 = clamp(R_ProjectPointY(pt1.z, pt1.y), 0, viewheight - 1);
	int y2 = clamp(R_ProjectPointY(pt2.z, pt2.y), 0, viewheight - 1);

	// draw the line to the framebuffer
	int dx = x2 - x1;
	int dy = y2 - y1;

	int ax = 2 * (dx<0 ? -dx : dx);
	int sx = dx < 0 ? -1 : 1;

	int ay = 2 * (dy < 0 ? -dy : dy);
	int sy = dy < 0 ? -1 : 1;

	int x = x1;
	int y = y1;
	int d;

	if (ax > ay)
	{
		d = ay - ax/2;

		while (1)
		{
			R_DrawPixel(x, y, color);
			if (x == x2)
				return;

			if (d >= 0)
			{
				y += sy;
				d -= ax;
			}
			x += sx;
			d += ay;
		}
	}
	else
	{
		d = ax - ay/2;
		while (1)
		{
			R_DrawPixel(x, y, color);
			if (y == y2)
				return;		

			if (d >= 0)
			{
				x += sx;
				d -= ay;
			}
			y += sy;
			d += ax;
		}
	}
}


//
//
// R_InitTextureMapping
//
//

void R_InitTextureMapping (void)
{
	int i, t, x;

	// Use tangent table to generate viewangletox: viewangletox will give
	// the next greatest x after the view angle.

	const fixed_t hitan = finetangent[FINEANGLES/4+CorrectFieldOfView/2];
	const fixed_t lotan = finetangent[FINEANGLES/4-CorrectFieldOfView/2];
	const int highend = viewwidth + 1;
	fovtan = hitan; 

	// Calc focallength so FieldOfView angles covers viewwidth.
	FocalLengthX = FixedDiv (centerxfrac, hitan);
	FocalLengthY = FixedDiv (FixedMul (centerxfrac, yaspectmul), hitan);
	xfoc = FIXED2FLOAT(FocalLengthX);
	yfoc = FIXED2FLOAT(FocalLengthY);

	focratio = yfoc / xfoc;
	ifocratio = xfoc / yfoc;

	for (i = 0; i < FINEANGLES/2; i++)
	{
		fixed_t tangent = finetangent[i];

		if (tangent > hitan)
			t = -1;
		else if (tangent < lotan)
			t = highend;
		else
		{
			t = (centerxfrac - FixedMul (tangent, FocalLengthX) + FRACUNIT - 1) >> FRACBITS;

			if (t < -1)
				t = -1;
			else if (t > highend)
				t = highend;
		}
		viewangletox[i] = t;
	}

	// Scan viewangletox[] to generate xtoviewangle[]:
	//	xtoviewangle will give the smallest view angle
	//	that maps to x.
	for (x = 0; x <= viewwidth; x++)
	{
		i = 0;
		while (viewangletox[i] > x)
			i++;
		xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
	}

	// Take out the fencepost cases from viewangletox.
	for (i = 0; i < FINEANGLES/2; i++)
	{
		t = FixedMul (finetangent[i], FocalLengthX);
		t = centerx - t;

		if (viewangletox[i] == -1)
			viewangletox[i] = 0;
		else if (viewangletox[i] == highend)
			viewangletox[i]--;
	}
}

//
// R_SetFOV
//
// Changes the field of view based on widescreen mode availibility.
//
void R_SetFOV(float fov, bool force = false)
{
	fov = clamp(fov, 1.0f, 179.0f);

	if (fov == LastFOV && force == false)
		return;
 
	LastFOV = fov;
	FieldOfView = int(fov * FINEANGLES / 360.0f);

	if (V_UseWidescreen() || V_UseLetterBox())
	{
		float am = float(screen->width) / float(screen->height) / (4.0f / 3.0f);
		float radfov = fov * PI / 180.0f;
		float widefov = (2 * atan(am * tan(radfov / 2))) * 180.0f / PI;
		CorrectFieldOfView = int(widefov * FINEANGLES / 360.0f);
	}
	else
 	{
		CorrectFieldOfView = FieldOfView;
 	}

	setsizeneeded = true;
}

//
//
// R_GetFOV
//
// Returns the current field of view in degrees
//
//

float R_GetFOV (void)
{
	return LastFOV;
}

//
//
// R_InitLightTables
//
// Only inits the zlight table, because the scalelight table changes with
// view size.
//
// [RH] This now sets up indices into a colormap rather than pointers into
// the colormap, because the colormap can vary by sector, but the indices
// into it don't.
//
//

void R_InitLightTables (void)
{
	int level;
	int startmap;
	int scale;

	// Calculate the light levels to use
	//	for each level / distance combination.
	for (int i = 0; i < LIGHTLEVELS; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (int j = 0; j < MAXLIGHTZ; j++)
		{
			scale = FixedDiv (160*FRACUNIT, (j+1)<<LIGHTZSHIFT);
			scale >>= LIGHTSCALESHIFT-LIGHTSCALEMULBITS;
			level = startmap - scale/DISTMAP;

			if (level < 0)
				level = 0;
			else if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			zlight[i][j] = level;
		}
	}

	lightscalexmul = ((320<<detailyshift) * (1<<LIGHTSCALEMULBITS)) / screen->width;
}

//
//
// R_SetViewSize
//
// Do not really change anything here, because it might be in the middle
// of a refresh. The change will take effect next refresh.
//
//

void R_SetViewSize (int blocks)
{
	setsizeneeded = true;
	setblocks = blocks;
}

//
//
// CVAR r_detail
//
// Selects a pixel doubling mode
//
//

CVAR_FUNC_IMPL (r_detail)
{
	static BOOL badrecovery = false;

	if (badrecovery)
	{
		badrecovery = false;
		return;
	}

	if (var < 0.0 || var > 3.0)
	{
		Printf (PRINT_HIGH, "Bad detail mode. (Use 0-3)\n");
		badrecovery = true;
		var.Set ((float)((detailyshift << 1)|detailxshift));
		return;
	}

	setdetail = (int)var;
	setsizeneeded = true;
}

//
//
// R_ExecuteSetViewSize
//
//

void R_ExecuteSetViewSize (void)
{
	int i, j;
	int level;
	int startmap;

	setsizeneeded = false;

	if (setdetail >= 0)
	{
		detailxshift = setdetail & 1;
		detailyshift = (setdetail >> 1) & 1;
		setdetail = -1;
	}

	if (setblocks == 11 || setblocks == 12)
	{
		realviewwidth = screen->width;
		freelookviewheight = realviewheight = screen->height;
	}
	else if (setblocks == 10) {
		realviewwidth = screen->width;
		realviewheight = ST_Y;
		freelookviewheight = screen->height;
	}
	else
	{
		realviewwidth = ((setblocks*screen->width)/10) & (~(15>>(screen->is8bit() ? 0 : 2)));
		realviewheight = ((setblocks*ST_Y)/10)&~7;
		freelookviewheight = ((setblocks*screen->height)/10)&~7;
	}

	viewwidth = realviewwidth >> detailxshift;
	viewheight = realviewheight >> detailyshift;
	freelookviewheight >>= detailyshift;

	{
		char temp[16];

		sprintf (temp, "%d x %d", viewwidth, viewheight);
		r_viewsize.ForceSet (temp);
	}

	centery = viewheight/2;
	centerx = viewwidth/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	// calculate the vertical stretching factor to emulate 320x200
	// it's a 5:4 ratio = (320 / 200) / (4 / 3)
	// also take r_detail into account
	yaspectmul = (320.0f / 200.0f) / (4.0f / 3.0f) * ((FRACUNIT << detailxshift) >> detailyshift);

	for (int i = 0; i < screen->width; i++)
	{
		negonearray[i] = -1;
		viewheightarray[i] = (int)viewheight;
	}

	R_InitBuffer (viewwidth, viewheight);
	R_InitTextureMapping ();

	// psprite scales
	// [AM] Using centerxfrac will make our sprite too fat, so we
	//      generate a corrected 4:3 screen width based on our
	//      height, then generate the x-scale based on that.
	int cswidth, crvwidth;
	cswidth = (4 * screen->height) / 3;
	if (setblocks < 10)
		crvwidth = ((setblocks * cswidth) / 10) & (~(15 >> (screen->is8bit() ? 0 : 2)));
	else
		crvwidth = cswidth;
	pspritexscale = (((crvwidth >> detailxshift) / 2) << FRACBITS) / 160;

	pspriteyscale = FixedMul(pspritexscale, yaspectmul);
	pspritexiscale = FixedDiv(FRACUNIT, pspritexscale);

	// [RH] Sky height fix for screens not 200 (or 240) pixels tall
	R_InitSkyMap ();

	// allocate for the array that indicates if a screen column is fully solid
	delete [] solidcol;
	solidcol = new byte[viewwidth];

	R_PlaneInitData ();

	// Calculate the light levels to use for each level / scale combination.
	// [RH] This just stores indices into the colormap rather than pointers to a specific one.
	for (i = 0; i < LIGHTLEVELS; i++)
	{
		startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j = 0; j < MAXLIGHTSCALE; j++)
		{
			level = startmap - (j*(screen->width>>detailxshift))/((viewwidth*DISTMAP));
			if (level < 0)
				level = 0;
			else if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			scalelight[i][j] = level;
		}
	}

	// [RH] Initialize z-light tables here
	R_InitLightTables ();
}

//
//
// CVAR screenblocks
//
// Selects the size of the visible window
//
//

CVAR_FUNC_IMPL (screenblocks)
{
	if (var > 12.0)
		var.Set (12.0);
	else if (var < 3.0)
		var.Set (3.0);
	else
		R_SetViewSize ((int)var);
}

//
//
// R_Init
//
//

void R_Init (void)
{
	R_InitData ();
	R_SetViewSize ((int)screenblocks);
	R_InitPlanes ();
	R_InitLightTables ();
	R_InitTranslationTables ();

	R_InitParticles ();	// [RH] Setup particle engine

	framecount = 0;
}

// R_Shutdown
void STACK_ARGS R_Shutdown (void)
{
    R_FreeTranslationTables();
}

//
//
// R_PointInSubsector
//
//

subsector_t *R_PointInSubsector (fixed_t x, fixed_t y)
{
	node_t *node;
	int side;
	int nodenum;

	// single subsector is a special case
	if (!numnodes)
		return subsectors;

	nodenum = numnodes-1;

	while (! (nodenum & NF_SUBSECTOR) )
	{
		node = &nodes[nodenum];
		side = R_PointOnSide (x, y, node);
		nodenum = node->children[side];
	}

	return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_ViewShear
//
// Sets centeryfrac, centery, and fills the yslope array based on the given
// pitch. This allows the y-shearing freelook approximation.
//
static void R_ViewShear(angle_t pitch)
{
	fixed_t dy = FixedMul(FocalLengthY, finetangent[(ANG90 - pitch) >> ANGLETOFINESHIFT]);

	centeryfrac = (viewheight << (FRACBITS - 1)) + dy;
	centery = centeryfrac >> FRACBITS;

	int e = viewheight, i = 0;
	fixed_t focus = FocalLengthY;
	fixed_t den;

	if (i < centery)
	{
		den = centeryfrac - (i << FRACBITS) - FRACUNIT / 2;

		if (e <= centery)
		{
			do {
				yslope[i] = FixedDiv(focus, den);
				den -= FRACUNIT;
			} while (++i < e);
		}
		else
		{
			do {
				yslope[i] = FixedDiv(focus, den);
				den -= FRACUNIT;
			} while (++i < centery);

			den = (i << FRACBITS) - centeryfrac + FRACUNIT / 2;

			do {
				yslope[i] = FixedDiv(focus, den);
				den += FRACUNIT;
			} while (++i < e);
		}
	}
	else
	{
		den = (i << FRACBITS) - centeryfrac + FRACUNIT / 2;

		do {
			yslope[i] = FixedDiv(focus, den);
			den += FRACUNIT;
		} while (++i < e);
	}
}


//
//
// R_SetupFrame
//
//
void R_SetupFrame (player_t *player)
{
	unsigned int newblend;

	camera = player->camera;	// [RH] Use camera instead of viewplayer

	if (!camera || !camera->subsector)
		return;

	if (player->cheats & CF_CHASECAM)
	{
		// [RH] Use chasecam view
		P_AimCamera (camera);
		viewx = CameraX;
		viewy = CameraY;
		viewz = CameraZ;
		viewangle = viewangleoffset + camera->angle;
	}
	else
	{
		if (render_lerp_amount < FRACUNIT)
		{
			R_InterpolateCamera(render_lerp_amount);
		}
		else
		{
			viewx = camera->x;
			viewy = camera->y;
			viewz = camera->player ? camera->player->viewz : camera->z;
			viewangle = viewangleoffset + camera->angle;
		}
	}

	if (camera->player && camera->player->xviewshift && !paused)
	{
		int intensity = camera->player->xviewshift;
		viewx += ((M_Random() % (intensity<<2))
					-(intensity<<1))<<FRACBITS;
		viewy += ((M_Random()%(intensity<<2))
					-(intensity<<1))<<FRACBITS;
	}

	extralight = camera == player->mo ? player->extralight : 0;

	viewsin = finesine30[(ANG90 - viewangle) >> ANGLETOFINESHIFT];
	viewcos = finecosine30[(ANG90 - viewangle) >> ANGLETOFINESHIFT];

	// killough 3/20/98, 4/4/98: select colormap based on player status
	// [RH] Can also select a blend
	if (camera->subsector->sector->heightsec &&
		!(camera->subsector->sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{
		const sector_t *s = camera->subsector->sector->heightsec;
		newblend = viewz < P_FloorHeight(viewx, viewy, s) ? s->bottommap : 
					viewz > P_CeilingHeight(viewx, viewy, s) ? s->topmap : s->midmap;

		if (!screen->is8bit())
			newblend = R_BlendForColormap (newblend);
		else if (APART(newblend) == 0 && newblend >= numfakecmaps)
			newblend = 0;
	}
	else
	{
		newblend = 0;
	}

	// [RH] Don't override testblend unless entering a sector with a
	//		blend different from the previous sector's. Same goes with
	//		NormalLight's maps pointer.
	if (R_OldBlend != newblend)
	{
		R_OldBlend = newblend;
		if (APART(newblend))
		{
			BaseBlendR = RPART(newblend);
			BaseBlendG = GPART(newblend);
			BaseBlendB = BPART(newblend);
			BaseBlendA = APART(newblend) / 255.0f;
			NormalLight.maps = shaderef_t(&realcolormaps, 0);
		}
		else
		{
			NormalLight.maps = shaderef_t(&realcolormaps, (NUMCOLORMAPS+1)*newblend);
			BaseBlendR = BaseBlendG = BaseBlendB = 0;
			BaseBlendA = 0.0f;
		}
	}

	fixedcolormap = shaderef_t();
	fixedlightlev = 0;
	palette_t *pal = GetDefaultPalette();

	if (camera == player->mo && player->fixedcolormap)
	{
		if (player->fixedcolormap < NUMCOLORMAPS)
		{
			fixedlightlev = player->fixedcolormap;
			fixedcolormap = shaderef_t(&pal->maps, 0);
		}
		else
		{
			fixedcolormap = shaderef_t(&pal->maps, player->fixedcolormap);
		}

		walllights = scalelightfixed;

		memset (scalelightfixed, 0, MAXLIGHTSCALE*sizeof(*scalelightfixed));

		// TODO: only calculate these when fixedcolormap or fixedlightlev change
		for (int x = 0; x < viewwidth; x++)
		{
			fixed_light_colormap_table[x] = basecolormap.with(fixedlightlev);
			fixed_colormap_table[x] = fixedcolormap;
		}
	}

	// [RH] freelook stuff
	fixed_t pitch = camera->prevpitch + FixedMul(render_lerp_amount, camera->pitch - camera->prevpitch);
	R_ViewShear(pitch); 

	// [RH] Hack to make windows into underwater areas possible
	r_fakingunderwater = false;

	framecount++;
	validcount++;
}

//
// R_SetFlatDrawFuncs
//
// Sets the drawing function pointers to functions that floodfill with
// flat colors instead of texture mapping.
//
void R_SetFlatDrawFuncs()
{
	colfunc = R_FillColumn;
	maskedcolfunc = R_FillMaskedColumn;
	spanfunc = R_FillSpan;
	spanslopefunc = R_FillSpan;
}

//
// R_SetBlankDrawFuncs
//
// Sets the drawing function pointers to functions that draw nothing.
// These can be used instead of the flat color functions for lucent midtex.
//
void R_SetBlankDrawFuncs()
{
	colfunc = maskedcolfunc = R_BlankColumn;
	spanfunc = spanslopefunc = R_BlankSpan;
}

//
// R_ResetDrawFuncs
//
void R_ResetDrawFuncs()
{
	if (nodrawers)
	{
		R_SetBlankDrawFuncs();
	}
	else if (r_drawflat)
	{
		R_SetFlatDrawFuncs();
	}
	else
	{
		colfunc = R_DrawColumn;
		maskedcolfunc = R_DrawMaskedColumn;
		spanfunc = R_DrawSpan;
		spanslopefunc = R_DrawSlopeSpan;
	}
}

void R_SetFuzzDrawFuncs()
{
	if (nodrawers)
	{
		R_SetBlankDrawFuncs();
	}
	else if (r_drawflat)
	{
		R_SetFlatDrawFuncs();
	}
	else
	{
		colfunc = R_DrawColumn;
		maskedcolfunc = R_DrawFuzzMaskedColumn;
	}
}

void R_SetLucentDrawFuncs()
{
	if (nodrawers)
	{
		R_SetBlankDrawFuncs();
	}
	else if (r_drawflat)
	{
		R_SetFlatDrawFuncs();
	}
	else
	{
		colfunc = R_DrawColumn;
		maskedcolfunc = R_DrawTranslucentMaskedColumn;
	}
}

void R_SetTranslatedDrawFuncs()
{
	if (nodrawers)
	{
		R_SetBlankDrawFuncs();
	}
	else if (r_drawflat)
	{
		R_SetFlatDrawFuncs();
	}
	else
	{
		colfunc = R_DrawTranslatedColumn;
		maskedcolfunc = R_DrawTranslatedMaskedColumn;
	}
}

void R_SetTranslatedLucentDrawFuncs()
{
	if (nodrawers)
	{
		R_SetBlankDrawFuncs();
	}
	else if (r_drawflat)
	{
		R_SetFlatDrawFuncs();
	}
	else
	{
		colfunc = R_DrawColumn;
		maskedcolfunc = R_DrawTranslatedTranslucentMaskedColumn;
	}
}

//
//
// R_RenderView
//
//

void R_RenderPlayerView (player_t *player)
{
	dcol.pitch = screen->is8bit() ? screen->pitch : screen->pitch / 4;

	R_SetupFrame (player);

	// Clear buffers.
	R_ClearClipSegs ();
	R_ClearDrawSegs ();
	R_ClearOpenings();
	R_ClearPlanes ();
	R_ClearSprites ();

	R_ResetDrawFuncs();

	// [SL] fill the screen with a blinking solid color to make HOM more visible
	if (r_flashhom)
	{
		int color = gametic & 8 ? 0 : 200;

		int x1 = viewwindowx;
		int y1 = viewwindowy;
		int x2 = viewwindowx + viewwidth - 1;
		int y2 = viewwindowy + viewheight - 1; 

		if (screen->is8bit())
			screen->Clear(x1, y1, x2, y2, color);
		else
			screen->Clear(x1, y1, x2, y2, basecolormap.shade(color));
	}

	R_BeginInterpolation(render_lerp_amount);

	// [RH] Setup particles for this frame
	R_FindParticleSubsectors();

    // [Russell] - From zdoom 1.22 source, added camera pointer check
	// Never draw the player unless in chasecam mode
	if (camera && camera->player && !(player->cheats & CF_CHASECAM))
	{
		int flags2_backup = camera->flags2;
		camera->flags2 |= MF2_DONTDRAW;
		R_RenderBSPNode(numnodes - 1);
		camera->flags2 = flags2_backup; 
	}
	else
		R_RenderBSPNode(numnodes - 1);	// The head node is the last node output.

	R_DrawPlanes ();

	R_DrawMasked ();

	// [RH] Apply detail mode doubling
	R_DetailDouble ();

	// NOTE(jsd): Full-screen status color blending:
	extern int BlendA, BlendR, BlendG, BlendB;
	if (BlendA != 0)
	{
		unsigned int blend_rgb = MAKERGB(newgamma[BlendR], newgamma[BlendG], newgamma[BlendB]);
		r_dimpatchD(screen, blend_rgb, BlendA, 0, 0, screen->width, screen->height);
	}

	R_EndInterpolation();
}

//
//
// R_MultiresInit
//
// Called from V_SetResolution()
//
//

void R_MultiresInit (void)
{
	// in r_draw.c
	extern byte **ylookup;
	extern int *columnofs;

	// [Russell] - Possible bug, ylookup is 2 star.
    M_Free(ylookup);
    M_Free(columnofs);
    M_Free(xtoviewangle);

	ylookup = (byte **)M_Malloc (screen->height * sizeof(byte *));
	columnofs = (int *)M_Malloc (screen->width * sizeof(int));

	// These get set in R_ExecuteSetViewSize()
	xtoviewangle = (angle_t *)M_Malloc (sizeof(angle_t) * (screen->width + 1));

	// GhostlyDeath -- Clean up the buffers
	memset(ylookup, 0, screen->height * sizeof(byte*));
	memset(columnofs, 0, screen->width * sizeof(int));

    memset(xtoviewangle, 0, screen->width * sizeof(angle_t) + 1);

	R_InitFuzzTable ();
	R_OldBlend = ~0;
}

VERSION_CONTROL (r_main_cpp, "$Id$")


