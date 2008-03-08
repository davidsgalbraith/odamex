// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
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
//		Main loop menu stuff.
//		Default Config File.
//		PCX Screenshots.
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include <SDL.h>

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
#include "c_bind.h"

#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "hu_stuff.h"

// State.
#include "doomstat.h"

// Data.
#include "dstrings.h"

#include "m_misc.h"

#include "cmdlib.h"

#include "g_game.h"

//
// M_WriteFile
//
#ifndef O_BINARY
#define O_BINARY 0
#endif

// Used to identify the version of the game that saved
// a config file to compensate for new features that get
// put into newer configfiles.
static CVAR (configver, CONFIGVERSIONSTR, CVAR_ARCHIVE | CVAR_NOENABLEDISABLE)

//
// M_WriteFile
//
BOOL M_WriteFile(char const *name, void *source, QWORD length)
{
    FILE *handle;
    int	count;
	
    handle = fopen(name, "wb");

    if (handle == NULL)
	{
		Printf(PRINT_HIGH, "Could not open file %s for writing\n", name);
		return false;
	}

    count = fwrite(source, 1, length, handle);
    fclose(handle);
	
	if (count != length)
	{
		Printf(PRINT_HIGH, "Failed while writing to file %s\n", name);
		return false;
	}
		
    return true;
}


//
// M_ReadFile
//
QWORD M_ReadFile(char const *name, BYTE **buffer)
{
    FILE *handle;
    QWORD count, length;
    BYTE *buf;
	
    handle = fopen(name, "rb");
    
	if (handle == NULL)
	{
		Printf(PRINT_HIGH, "Could not open file %s for reading\n", name);
		return false;
	}

    // find the size of the file by seeking to the end and
    // reading the current position

    fseek(handle, 0, SEEK_END);
    length = ftell(handle);
    fseek(handle, 0, SEEK_SET);
    
    buf = (BYTE *)Z_Malloc (length, PU_STATIC, NULL);
    count = fread(buf, 1, length, handle);
    fclose (handle);
	
    if (count != length)
	{
		Printf(PRINT_HIGH, "Failed while reading from file %s\n", name);
		return false;
	}
		
    *buffer = buf;
    return length;
}


// [Russell] Simple function to check whether the given string is an iwad name
//           it also removes the extension for short-hand comparison
BOOL M_IsIWAD(std::string filename)
{
    // Valid IWAD file names
    static const char *doomwadnames[] =
    {
        "doom2f.wad",
        "doom2.wad",
        "plutonia.wad",
        "tnt.wad",
        "doomu.wad", // Hack from original Linux version. Not necessary, but I threw it in anyway.
        "doom.wad",
        "doom1.wad",
        "freedoom.wad",
        "freedm.wad"
    };
    
    std::vector<std::string> iwad_names(doomwadnames, 
                                        doomwadnames + sizeof(doomwadnames)/sizeof(*doomwadnames));
    
    if (!filename.length())
        return false;
        
    // lowercase our comparison string
    std::transform(filename.begin(), filename.end(), filename.begin(), tolower);
    
    // find our match if there is one
    for (QWORD i = 0; i < iwad_names.size(); i++)
    {
        std::string no_ext;
        
        // create an extensionless version, for short-hand comparison
        QWORD first_dot = iwad_names[i].find_last_of('.', 
                                                     iwad_names[i].length());
        
        if (first_dot != std::string::npos)
            no_ext = iwad_names[i].substr(0, first_dot);    
        
        if ((iwad_names[i] == filename) || (no_ext == filename))
            return true;
    }
    
    return false;
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

	return I_GetUserFileName ("odamex.cfg");
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
		C_ArchiveBindings (f);

		// Archive all aliases
		DConsoleAlias::C_ArchiveAliases (f);

		fclose (f);
	}
}


//
// M_LoadDefaults
//
extern int cvar_defflags;

void M_LoadDefaults (void)
{
	extern char DefBindings[];

	// Set default key bindings. These will be overridden
	// by the bindings in the config file if it exists.
	AddCommandString (DefBindings);

	std::string cmd = "exec \"";
	cmd += GetConfigPath();
	cmd += "\"";

	cvar_defflags = CVAR_ARCHIVE;
	AddCommandString (cmd.c_str());
	cvar_defflags = 0;

	if (configver < 118.0f)
	{
		AddCommandString ("alias idclip noclip");
		AddCommandString ("alias idspispopd noclip");
		AddCommandString ("alias iddqd god");
		AddCommandString ("alias idclev map");
		AddCommandString ("alias changemap map");
		AddCommandString ("alias changemus idmus");
	}

	DefaultsLoaded = true;
}

// Find a free filename that isn't taken
static BOOL FindFreeName (char *lbmname, const char *extension)
{
	int i;

	for (i=0 ; i<=9999 ; i++)
	{
		sprintf (lbmname, "DOOM%04d.%s", i, extension);
		if (!FileExists (lbmname))
			break;		// file doesn't exist
	}
	if (i==10000)
		return false;
	else
		return true;
}

extern DWORD IndexedPalette[256];

/*
    Dump a screenshot as a bitmap (BMP) file
*/
void M_ScreenShot (const char *filename)
{
	byte *linear;
	char  autoname[32];
	char *lbmname;

	// find a file name to save it to
	if (!filename || !strlen(filename))
	{
		lbmname = autoname;
		if (!FindFreeName (lbmname, "bmp\0bmp" + (screen->is8bit() << 2)))
		{
			Printf (PRINT_HIGH, "M_ScreenShot: Delete some screenshots\n");
			return;
		}
		filename = autoname;
	}

	if (screen->is8bit())
	{
		// munge planar buffer to linear
		linear = new byte[screen->width * screen->height];
		I_ReadScreen (linear);

        // [Russell] - Spit out a bitmap file

        // check endianess
        #if SDL_BYTEORDER == SDL_BIG_ENDIAN
            Uint32 rmask = 0xff000000;
            Uint32 gmask = 0x00ff0000;
            Uint32 bmask = 0x0000ff00;
            Uint32 amask = 0x000000ff;
        #else
            Uint32 rmask = 0x000000ff;
            Uint32 gmask = 0x0000ff00;
            Uint32 bmask = 0x00ff0000;
            Uint32 amask = 0xff000000;
        #endif

		SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(linear, screen->width, screen->height, 8, screen->width * 1, rmask,gmask,bmask,amask);

		SDL_Colour colors[256];

		// stolen from the WritePCXfile function, write palette data into SDL_Colour
		DWORD *pal;

		pal = IndexedPalette;

		for (int i = 0; i < 256; i+=1, pal++)
            {
                colors[i].r = RPART(*pal);
                colors[i].g = GPART(*pal);
                colors[i].b = BPART(*pal);
                colors[i].unused = 0;
            }

        // set the palette
        SDL_SetColors(surface, colors, 0, 256);

		// save the bmp file
		if (SDL_SaveBMP(surface, filename) == -1)
        {
            Printf (PRINT_HIGH, "SDL_SaveBMP Error: %s\n", SDL_GetError());

            SDL_FreeSurface(surface);
            delete[] linear;
            return;
        }
/*		WritePCXfile (filename, linear,
					  screen->width, screen->height,
					  IndexedPalette);*/

        SDL_FreeSurface(surface);
		delete[] linear;
	}
	else
	{
		// save the tga file
		//I_WriteTGAfile (filename, &screen);
	}
	Printf (PRINT_HIGH, "screen shot taken: %s\n", filename);
}

BEGIN_COMMAND (screenshot)
{
	if (argc == 1)
		G_ScreenShot (NULL);
	else
		G_ScreenShot (argv[1]);
}
END_COMMAND (screenshot)

VERSION_CONTROL (m_misc_cpp, "$Id$")



