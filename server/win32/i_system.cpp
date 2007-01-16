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
//	System specific interface stuff.
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <string.h>
#include <process.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/timeb.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <DIRECT.H> // SoM: I don't know HOW this has been overlooked until now...

#include "errors.h"
#include <math.h>

#include "doomtype.h"
#include "version.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "m_misc.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "sv_main.h"

UINT TimerPeriod;
float mb_used = 32.0;

QWORD (*I_GetTime) (void);
QWORD (*I_WaitForTic) (QWORD);

ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void)
{
	return &emptycmd;
}

int I_GetHeapSize (void)
{
	return (int)(mb_used*1024*1024);
}

byte *I_ZoneBase (unsigned int *size)
{
	void *zone;

	const char *p = Args.CheckValue ("-heapsize");
	if (p)
		mb_used = (float)atof (p);
	*size = (int)(mb_used*1024*1024);

	while (NULL == (zone = Malloc (*size)) && *size >= 2*1024*1024)
		*size -= 1024*1024;

	return (byte *)zone;
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte *I_AllocLow(int length)
{
	byte *mem;

	mem = (byte *)Malloc (length);
	if (mem) {
		memset (mem,0,length);
	}
	return mem;
}

// denis - use this unless you want your program
// to get confused every 49 days due to DWORD limit
QWORD I_UnwrapTime(DWORD now32)
{
	static QWORD last = 0;
	QWORD now = now32;

	if(now < last%UINT_MAX)
		last += (UINT_MAX-(last%UINT_MAX)) + now;
	else
		last = now;

	return last;
}

// [RH] Returns time in milliseconds
QWORD I_MSTime (void)
{
	static QWORD basetime = 0;
	QWORD now = I_UnwrapTime(timeGetTime());

	if(!basetime)
		basetime = now;

	return now - basetime;
}

//
// I_GetTime
// returns time in 1/35th second tics
//
QWORD I_GetTimePolled (void)
{
	return (I_MSTime()*TICRATE)/1000;
}

QWORD I_WaitForTicPolled (QWORD prevtic)
{
	QWORD time;

	while ((time = I_GetTimePolled()) <= prevtic)I_Yield();

	return time;
}

void I_Yield(void)
{
	Sleep(1);
}

void I_WaitVBL (int count)
{
	// I_WaitVBL is never used to actually synchronize to the
	// vertical blank. Instead, it's used for delay purposes.
	Sleep (1000 * count / 70);
}

//
// I_Init
//
void I_Init (void)
{
	I_GetTime = I_GetTimePolled;
	I_WaitForTic = I_WaitForTicPolled;
}

std::string I_GetCWD()
{
	char tmp[4096] = {0};
	std::string ret = "./";

	const char *cwd = getcwd(tmp, sizeof(tmp));
	if(cwd)
		ret = cwd;

	FixPathSeparator(ret);

	return ret;
}

#ifdef UNIX
std::string I_GetHomeDir(std::string user = "")
{
	const char *envhome = getenv("HOME");
	std::string home = envhome ? envhome : "";
	
	if (!home.length())
	{
#ifdef HAVE_PWD_H
		// try the uid way
		passwd *p = user.length() ? getpwnam(user.c_str()) : getpwuid(getuid());
		if(p && p->pw_dir)
			home = p->pw_dir;
#endif
		
		if (!home.length())
			I_FatalError ("Please set your HOME variable");
	}
	
	if(home[home.length() - 1] != '/')
		home += "/";
	
	return home;
}
#endif

std::string I_GetUserFileName (const char *file)
{
#ifdef UNIX
	std::string path = I_GetHomeDir();

	if(path[path.length() - 1] != '/')
		path += "/";

	path += ".odamex";

	struct stat info;
	if (stat (path.c_str(), &info) == -1)
	{
		if (mkdir (path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == -1)
		{
			I_FatalError ("Failed to create %s directory:\n%s",
						  path.c_str(), strerror (errno));
		}
	}
	else
	{
		if (!S_ISDIR(info.st_mode))
		{
			I_FatalError ("%s must be a directory", path.c_str());
		}
	}

	path += "/";
	path += file;
#endif

#ifdef WIN32
	std::string path = I_GetBinaryDir();

	if(path[path.length() - 1] != '/')
		path += "/";

	path += file;
#endif

	return path;
}

void I_ExpandHomeDir (std::string &path)
{
#ifdef UNIX
	if(!path.length())
		return;
	
	if(path[0] != '~')
		return;
	
	std::string user;
	
	size_t slash_pos = path.find_first_of('/');
	size_t end_pos = path.length();
	
	if(slash_pos == std::string::npos)
		slash_pos = end_pos;
	
	if(path.length() != 1 && slash_pos != 1)
		user = path.substr(1, slash_pos - 1);
	
	if(slash_pos != end_pos)
		slash_pos++;
	
	path = I_GetHomeDir(user) + path.substr(slash_pos, end_pos - slash_pos);
#endif
}

std::string I_GetBinaryDir()
{
	std::string ret;

#ifdef WIN32
	char tmp[MAX_PATH]; // denis - todo - make separate function
	GetModuleFileName (NULL, tmp, sizeof(tmp));
	ret = tmp;
#else
	if(!Args[0])
		return "./";

	char realp[PATH_MAX];
	if(realpath(Args[0], realp))
		ret = realp;
	else
	{
		// search through $PATH
		const char *path = getenv("PATH");
		if(path)
		{
			std::stringstream ss(path);
			std::string segment;

			while(ss)
			{
				std::getline(ss, segment, ':');

				if(!segment.length())
					continue;

				if(segment[segment.length() - 1] != '/')
					segment += "/";
				segment += Args[0];

				if(realpath(segment.c_str(), realp))
				{
					ret = realp;
					break;
				}
			}
		}
	}
#endif

	FixPathSeparator(ret);

	size_t slash = ret.find_last_of('/');
	if(slash == std::string::npos)
		return "";
	else
		return ret.substr(0, slash);
}

void I_FinishClockCalibration ()
{
}

//
// I_Quit
//
static int has_exited;

void STACK_ARGS I_Quit (void)
{
	has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

	timeEndPeriod (TimerPeriod);

	G_ClearSnapshots ();
	SV_SendDisconnectSignal ();

	CloseNetwork ();
}


//
// I_Error
//
extern FILE *Logfile;
BOOL gameisdead;

#define MAX_ERRORTEXT	1024

void STACK_ARGS I_FatalError (const char *error, ...)
{
	static BOOL alreadyThrown = false;
	gameisdead = true;

	if (!alreadyThrown)		// ignore all but the first message -- killough
	{
		char errortext[MAX_ERRORTEXT];
		int index;
		va_list argptr;
		va_start (argptr, error);
		index = vsprintf (errortext, error, argptr);
		sprintf (errortext + index, "\nGetLastError = %d", GetLastError());
		va_end (argptr);

		// Record error to log (if logging)
		if (Logfile)
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);

		throw CFatalError (errortext);
	}

	if (!has_exited)	// If it hasn't exited yet, exit now -- killough
	{
		has_exited = 1;	// Prevent infinitely recursive exits -- killough
		exit(-1);
	}
}

void STACK_ARGS I_Error (const char *error, ...)
{
	va_list argptr;
	char errortext[MAX_ERRORTEXT];

	va_start (argptr, error);
	vsprintf (errortext, error, argptr);
	va_end (argptr);

	throw CRecoverableError (errortext);
}

char DoomStartupTitle[256] = { 0 };

void I_SetTitleString (const char *title)
{
	int i;

	for (i = 0; title[i]; i++)
		DoomStartupTitle[i] = title[i] | 0x80;
}

void I_PrintStr (int xp, const char *cp, int count, BOOL scroll)
{
	// used in the DOS version
}

long I_FindFirst (char *filespec, findstate_t *fileinfo)
{
//	return _findfirst (filespec, fileinfo);
	return 0;
}
int I_FindNext (long handle, findstate_t *fileinfo)
{
//	return _findnext (handle, fileinfo);
	return 0;
}

int I_FindClose (long handle)
{
//	return _findclose (handle);
	return 0;
}

//
// I_ConsoleInput
//
std::string I_ConsoleInput (void)
{
	// denis - todo - implement this properly!!!
    static char     text[1024] = {0};
    static char     buffer[1024] = {0};
    unsigned int    len = strlen(buffer);

	while(kbhit() && len < sizeof(text))
	{
		char ch = (char)getch();

		// input the character
		if(ch == '\b' && len)
		{
			buffer[--len] = 0;
			// john - backspace hack
			fwrite(&ch, 1, 1, stdout);
			ch = ' ';
			fwrite(&ch, 1, 1, stdout);
			ch = '\b';
		}
		else
			buffer[len++] = ch;
		buffer[len] = 0;

		// recalculate length
		len = strlen(buffer);

		// echo character back to user
		fwrite(&ch, 1, 1, stdout);
		fflush(stdout);
	}

	if(len && buffer[len - 1] == '\n' || buffer[len - 1] == '\r')
	{
		// echo newline back to user
		char ch = '\n';
		fwrite(&ch, 1, 1, stdout);
		fflush(stdout);

		strcpy(text, buffer);
		text[len-1] = 0; // rip off the /n and terminate
		buffer[0] = 0;
		len = 0;

		return text;
	}

	return "";
}
