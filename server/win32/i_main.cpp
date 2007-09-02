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
//	Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------


#ifdef WIN32

#include <stack>
#include <map>
#include <string>

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#include "errors.h"

#include "doomtype.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "c_console.h"
#include "z_zone.h"
#include "version.h"

using namespace std;

void AddCommandString(std::string cmd);

DArgs Args;

#ifdef WIN32
extern UINT TimerPeriod;
#endif

// functions to be called at shutdown are stored in this stack
typedef void (STACK_ARGS *term_func_t)(void);
std::stack< std::pair<term_func_t, std::string> > TermFuncs;

void addterm (void (STACK_ARGS *func) (), const char *name)
{
	TermFuncs.push(std::pair<term_func_t, std::string>(func, name));
}

static void STACK_ARGS call_terms (void)
{
	while (!TermFuncs.empty())
		TermFuncs.top().first(), TermFuncs.pop();
}

int __cdecl main(int argc, char *argv[])
{
    try
    {
		Args.SetArgs (argc, argv);

#ifdef WIN32
		// Set the timer to be as accurate as possible
		TIMECAPS tc;
		if (timeGetDevCaps (&tc, sizeof(tc) != TIMERR_NOERROR))
			TimerPeriod = 1;	// Assume minimum resolution of 1 ms
		else
			TimerPeriod = tc.wPeriodMin;

		timeBeginPeriod (TimerPeriod);
#endif

		atexit (call_terms);

		Z_Init();

		atterm (I_Quit);
		atterm (DObject::StaticShutdown);

		progdir = I_GetBinaryDir();
		startdir = I_GetCWD();

		C_InitConsole (80*8, 25*8, false);
		D_DoomMain ();
    }
    catch (CDoomError &error)
    {
		fprintf (stderr, "%s\n", error.GetMessage().c_str());
		exit (-1);
    }
    catch (...)
    {
		call_terms ();
		throw;
    }
    return 0;
}

int PrintString (int printlevel, const char *outline)
{
	printf("%s", outline);
	return strlen (outline);
}

#endif
