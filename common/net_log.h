// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2014 by The Odamex Team.
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
//  
// Message logging for the network subsystem
//
//-----------------------------------------------------------------------------

#ifndef __NET_LOG_H__
#define __NET_LOG_H__

#include "doomtype.h"
#include "m_ostring.h"

class LogChannel
{
public:
	LogChannel() { }
	LogChannel(const OString& name, const OString& filename, bool enabled = true);

	~LogChannel();

	const OString& getName() const
	{
		return mName;
	}	

	bool isEnabled() const
	{
		return mEnabled;
	}

	void setEnabled(bool enabled)
	{
		mEnabled = enabled;
	}

	void write(const char* str);
	
private:
	OString		mName;
	bool		mEnabled;
	FILE*		mStream;
};


#define __NET_DEBUG__

void Net_LogPrintf2(const OString& channel_name, const char* func_name, const char* str, ...);

#define Net_LogPrintf(channel_name, format, ...) Net_LogPrintf2(channel_name, __FUNCTION__, format, ##__VA_ARGS__)



void Net_Printf(const char *str, ...);
void Net_Error(const char *str, ...);
void Net_Warning(const char *str, ...);

void Net_LogStartup();
void STACK_ARGS Net_LogShutdown();

#endif	// __NET_LOG_H__

