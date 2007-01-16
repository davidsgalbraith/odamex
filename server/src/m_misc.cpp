// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
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
//	Main loop menu stuff.
//	Default Config File.
//	PCX Screenshots.
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "doomtype.h"
#include "version.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "m_alloc.h"

#include "doomdef.h"

#include "z_zone.h"

#include "m_swap.h"
#include "m_argv.h"

#include "w_wad.h"

#include "c_cvars.h"
#include "c_dispatch.h"

#include "i_system.h"
#include "v_video.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

#include "m_misc.h"

#include "cmdlib.h"

#include "g_game.h"
#include "sv_master.h"


//
// M_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

// Used to identify the version of the game that saved
// a config file to compensate for new features that get
// put into newer configfiles.
static CVAR (configver, CONFIGVERSIONSTR, CVAR_ARCHIVE)

bool M_WriteFile (char const *name, void *source, int length)
{
	int handle;
	int count;

	handle = open ( name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);

	if (handle == -1)
		return false;

	count = write (handle, source, length);
	close (handle);

	if (count < length)
		return false;

	return true;
}


//
// M_ReadFile
//
int
M_ReadFile
( char const*	name,
  byte**	buffer )
{
	int handle, count, length;
	struct stat fileinfo;
	byte *buf;

	handle = open (name, O_RDONLY | O_BINARY, 0666);
	if (handle == -1)
		I_Error ("Couldn't read file %s", name);
	if (fstat (handle,&fileinfo) == -1)
		I_Error ("Couldn't read file %s", name);
	length = fileinfo.st_size;
	buf = (byte *)Z_Malloc (length, PU_STATIC, NULL);
	count = read (handle, buf, length);
	close (handle);

	if (count < length)
		I_Error ("Couldn't read file %s", name);

	*buffer = buf;
	return length;
}

// [RH] Get configfile path.
// This file contains commands to set all
// archived cvars, bind commands to keys,
// and set other general game information.
std::string GetConfigPath (void)
{
	const char *p = Args.CheckValue ("-config");

	if(p)
		return p;

	return I_GetUserFileName ("odasrv.cfg");
}

//
// M_SaveDefaults
//

// [RH] Don't write a config file if M_LoadDefaults hasn't been called.
static BOOL DefaultsLoaded;

void STACK_ARGS M_SaveDefaults (void)
{
	FILE *f;

	if (!DefaultsLoaded)
		return;

	std::string configfile = GetConfigPath();

	// Make sure the user hasn't changed configver
	configver.Set(CONFIGVERSIONSTR);

	if ( (f = fopen (configfile.c_str(), "w")) )
	{
		fprintf (f, "// Generated by Odamex " DOTVERSIONSTR " - don't hurt anything\n");

		// Archive all cvars marked as CVAR_ARCHIVE
		cvar_t::C_ArchiveCVars (f);

		// Archive all active key bindings
		//C_ArchiveBindings (f);

		// Archive all aliases
		DConsoleAlias::C_ArchiveAliases (f);

		// Archive master list
		SV_ArchiveMasters (f);

		fclose (f);
	}
}


//
// M_LoadDefaults
//
extern int cvar_defflags;
EXTERN_CVAR (dimamount)

void M_LoadDefaults (void)
{
	// Set default key bindings. These will be overridden
	// by the bindings in the config file if it exists.
	//AddCommandString (DefBindings);

	std::string cmd = "exec \"";
	cmd += GetConfigPath();
	cmd += "\"";

	cvar_defflags = CVAR_ARCHIVE;
	AddCommandString (cmd.c_str());
	cvar_defflags = 0;

	DefaultsLoaded = true;
	atterm (M_SaveDefaults);
}


//
// SCREEN SHOTS
//


typedef struct
{
	char				manufacturer;
	char				version;
	char				encoding;
	char				bits_per_pixel;

	unsigned short		xmin;
	unsigned short		ymin;
	unsigned short		xmax;
	unsigned short		ymax;
	
	unsigned short		hres;
	unsigned short		vres;

	unsigned char		palette[48];
	
	char				reserved;
	char				color_planes;
	unsigned short		bytes_per_line;
	unsigned short		palette_type;
	
	char				filler[58];
	unsigned char		data;			// unbounded
} pcx_t;

