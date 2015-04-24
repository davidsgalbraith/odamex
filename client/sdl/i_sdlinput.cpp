// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_sdlinput.cpp 5302 2015-04-23 01:53:13Z dr_sean $
//
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
//	
//
//-----------------------------------------------------------------------------

#include "doomtype.h"
#include <SDL.h>
#include "i_input.h"
#include "i_sdlinput.h"

#include "i_video.h"
#include "d_event.h"
#include "doomkeys.h"
#include <queue>
#include <cassert>

// ============================================================================
//
// ISDL12KeyboardInputDevice implementation
//
// ============================================================================

//
// ISDL12KeyboardInputDevice::ISDL12MouseInputDevice
//
ISDL12KeyboardInputDevice::ISDL12KeyboardInputDevice() :
	mActive(false),
	mSDLKeyTransTable(128), mShiftTransTable(128)
{
	// initialize the SDL key translation table
	mSDLKeyTransTable[SDLK_BACKSPACE] = '\b';
	mSDLKeyTransTable[SDLK_TAB] = '\t'; 
	mSDLKeyTransTable[SDLK_RETURN] = '\r'; 
	mSDLKeyTransTable[SDLK_SPACE] = ' ';
	mSDLKeyTransTable[SDLK_EXCLAIM]  = '!';
	mSDLKeyTransTable[SDLK_QUOTEDBL]  = '\"';
	mSDLKeyTransTable[SDLK_HASH]  = '#';
	mSDLKeyTransTable[SDLK_DOLLAR]  = '$';
	mSDLKeyTransTable[SDLK_AMPERSAND]  = '&';
	mSDLKeyTransTable[SDLK_QUOTE] = '\'';
	mSDLKeyTransTable[SDLK_LEFTPAREN] = '(';
	mSDLKeyTransTable[SDLK_RIGHTPAREN] = ')';
	mSDLKeyTransTable[SDLK_ASTERISK] = '*';
	mSDLKeyTransTable[SDLK_PLUS] = '+';
	mSDLKeyTransTable[SDLK_COMMA] = ',';
	mSDLKeyTransTable[SDLK_MINUS] = '-';
	mSDLKeyTransTable[SDLK_PERIOD] = '.';
	mSDLKeyTransTable[SDLK_SLASH] = '/';
	mSDLKeyTransTable[SDLK_0] = '0';
	mSDLKeyTransTable[SDLK_1] = '1';
	mSDLKeyTransTable[SDLK_2] = '2';
	mSDLKeyTransTable[SDLK_3] = '3';
	mSDLKeyTransTable[SDLK_4] = '4';
	mSDLKeyTransTable[SDLK_5] = '5';
	mSDLKeyTransTable[SDLK_6] = '6';
	mSDLKeyTransTable[SDLK_7] = '7';
	mSDLKeyTransTable[SDLK_8] = '8';
	mSDLKeyTransTable[SDLK_9] = '9';
	mSDLKeyTransTable[SDLK_COLON] = ':';
	mSDLKeyTransTable[SDLK_SEMICOLON] = ';';
	mSDLKeyTransTable[SDLK_LESS] = '<';
	mSDLKeyTransTable[SDLK_EQUALS] = '=';
	mSDLKeyTransTable[SDLK_GREATER] = '>';
	mSDLKeyTransTable[SDLK_QUESTION] = '?';
	mSDLKeyTransTable[SDLK_AT] = '@';
	mSDLKeyTransTable[SDLK_LEFTBRACKET] = '[';
	mSDLKeyTransTable[SDLK_BACKSLASH] = '\\';
	mSDLKeyTransTable[SDLK_RIGHTBRACKET] = ']';
	mSDLKeyTransTable[SDLK_CARET] = '^';
	mSDLKeyTransTable[SDLK_UNDERSCORE] = '_';
	mSDLKeyTransTable[SDLK_BACKQUOTE] = '`';
	mSDLKeyTransTable[SDLK_a] = 'a';
	mSDLKeyTransTable[SDLK_b] = 'b';
	mSDLKeyTransTable[SDLK_c] = 'c';
	mSDLKeyTransTable[SDLK_d] = 'd';
	mSDLKeyTransTable[SDLK_e] = 'e';
	mSDLKeyTransTable[SDLK_f] = 'f';
	mSDLKeyTransTable[SDLK_g] = 'g';
	mSDLKeyTransTable[SDLK_h] = 'h';
	mSDLKeyTransTable[SDLK_i] = 'i';
	mSDLKeyTransTable[SDLK_j] = 'j';
	mSDLKeyTransTable[SDLK_k] = 'k';
	mSDLKeyTransTable[SDLK_l] = 'l';
	mSDLKeyTransTable[SDLK_m] = 'm';
	mSDLKeyTransTable[SDLK_n] = 'n';
	mSDLKeyTransTable[SDLK_o] = 'o';
	mSDLKeyTransTable[SDLK_p] = 'p';
	mSDLKeyTransTable[SDLK_q] = 'q';
	mSDLKeyTransTable[SDLK_r] = 'r';
	mSDLKeyTransTable[SDLK_s] = 's';
	mSDLKeyTransTable[SDLK_t] = 't';
	mSDLKeyTransTable[SDLK_u] = 'u';
	mSDLKeyTransTable[SDLK_v] = 'v';
	mSDLKeyTransTable[SDLK_w] = 'w';
	mSDLKeyTransTable[SDLK_x] = 'x';
	mSDLKeyTransTable[SDLK_y] = 'y';
	mSDLKeyTransTable[SDLK_z] = 'z';
	mSDLKeyTransTable[SDLK_KP0] = '0';
	mSDLKeyTransTable[SDLK_KP1] = '1';
	mSDLKeyTransTable[SDLK_KP2] = '2';
	mSDLKeyTransTable[SDLK_KP3] = '3';
	mSDLKeyTransTable[SDLK_KP4] = '4';
	mSDLKeyTransTable[SDLK_KP5] = '5';
	mSDLKeyTransTable[SDLK_KP6] = '6';
	mSDLKeyTransTable[SDLK_KP7] = '7';
	mSDLKeyTransTable[SDLK_KP8] = '8';
	mSDLKeyTransTable[SDLK_KP9] = '9';
	mSDLKeyTransTable[SDLK_KP_PERIOD] = '.';
	mSDLKeyTransTable[SDLK_KP_DIVIDE] = '/';
	mSDLKeyTransTable[SDLK_KP_MULTIPLY] = '*';
	mSDLKeyTransTable[SDLK_KP_MINUS] = '-';
	mSDLKeyTransTable[SDLK_KP_PLUS] = '+';
	mSDLKeyTransTable[SDLK_KP_ENTER] = '\r';
	mSDLKeyTransTable[SDLK_KP_EQUALS] = '=';

	// initialize the shift key translation table
	mShiftTransTable['a'] = 'A';
	mShiftTransTable['b'] = 'B';
	mShiftTransTable['c'] = 'C';
	mShiftTransTable['d'] = 'D';
	mShiftTransTable['e'] = 'E';
	mShiftTransTable['f'] = 'F';
	mShiftTransTable['g'] = 'G';
	mShiftTransTable['h'] = 'H';
	mShiftTransTable['i'] = 'I';
	mShiftTransTable['j'] = 'J';
	mShiftTransTable['k'] = 'K';
	mShiftTransTable['l'] = 'L';
	mShiftTransTable['m'] = 'M';
	mShiftTransTable['n'] = 'N';
	mShiftTransTable['o'] = 'O';
	mShiftTransTable['p'] = 'P';
	mShiftTransTable['q'] = 'Q';
	mShiftTransTable['r'] = 'R';
	mShiftTransTable['s'] = 'S';
	mShiftTransTable['t'] = 'T';
	mShiftTransTable['u'] = 'U';
	mShiftTransTable['v'] = 'V';
	mShiftTransTable['w'] = 'W';
	mShiftTransTable['x'] = 'X';
	mShiftTransTable['y'] = 'Y';
	mShiftTransTable['z'] = 'Z';
	mShiftTransTable['1'] = '!';
	mShiftTransTable['2'] = '@';
	mShiftTransTable['3'] = '#';
	mShiftTransTable['4'] = '$';
	mShiftTransTable['5'] = '%';
	mShiftTransTable['6'] = '^';
	mShiftTransTable['7'] = '&';
	mShiftTransTable['8'] = '*';
	mShiftTransTable['9'] = '(';
	mShiftTransTable['0'] = ')';
	mShiftTransTable['`'] = '~';
	mShiftTransTable['-'] = '_';
	mShiftTransTable['='] = '+';
	mShiftTransTable['['] = '{';
	mShiftTransTable[']'] = '}';
	mShiftTransTable['\\'] = '|';
	mShiftTransTable[';'] = ':';
	mShiftTransTable['\''] = '\"';
	mShiftTransTable[','] = '<';
	mShiftTransTable['.'] = '>';
	mShiftTransTable['/'] = '?';

	// enable keyboard input
	resume();
}


//
// ISDL12KeyboardInputDevice::flushEvents
//
void ISDL12KeyboardInputDevice::flushEvents()
{
	gatherEvents();
	while (!mEvents.empty())
		mEvents.pop();
}


//
// ISDL12KeyboardInputDevice::reset
//
void ISDL12KeyboardInputDevice::reset()
{
	flushEvents();
}


//
// ISDL12KeyboardInputDevice::pause
//
// Sets the internal state to ignore all input for this device.
//
// NOTE: SDL_EventState clears the SDL event queue so only call this after all
// SDL events have been processed in all SDL input device instances.
//
void ISDL12KeyboardInputDevice::pause()
{
	mActive = false;
	SDL_EventState(SDL_KEYDOWN, SDL_IGNORE);
	SDL_EventState(SDL_KEYUP, SDL_IGNORE);
}


//
// ISDL12KeyboardInputDevice::resume
//
// Sets the internal state to enable all input for this device.
//
// NOTE: SDL_EventState clears the SDL event queue so only call this after all
// SDL events have been processed in all SDL input device instances.
//
void ISDL12KeyboardInputDevice::resume()
{
	mActive = true;
	SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
	SDL_EventState(SDL_KEYUP, SDL_ENABLE);
	reset();
}


//
// ISDL12KeyboardInputDevice::gatherEvents
//
// Pumps the SDL Event queue and retrieves any mouse events and puts them into
// this instance's event queue.
//
void ISDL12KeyboardInputDevice::gatherEvents()
{
	if (!mActive)
		return;

	// Force SDL to gather events from input devices. This is called
	// implicitly from SDL_PollEvent but since we're using SDL_PeepEvents to
	// process only mouse events, SDL_PumpEvents is necessary.
	SDL_PumpEvents();

	// Retrieve chunks of up to 1024 events from SDL
	int num_events = 0;
	const int max_events = 1024;
	SDL_Event sdl_events[max_events];

	while ((num_events = SDL_PeepEvents(sdl_events, max_events, SDL_GETEVENT, SDL_KEYEVENTMASK)))
	{
		// insert the SDL_Events into our queue
		for (int i = 0; i < num_events; i++)
			mEvents.push(sdl_events[i]);
	}
}


//
// ISDL12KeyboardInputDevice::getEvent
//
// Removes the first event from the queue and translates it to a Doom event_t.
// This makes no checks to ensure there actually is an event in the queue and
// if there is not, the behavior is undefined.
//
void ISDL12KeyboardInputDevice::getEvent(event_t* ev)
{
	assert(hasEvent());

	// clear the destination struct
	ev->type = ev_keydown;
	ev->data1 = ev->data2 = ev->data3 = 0;

	const SDL_Event& sdl_ev = mEvents.front();

	assert(sdl_ev.type == SDL_KEYDOWN || sdl_ev.type == SDL_KEYUP);

	if (sdl_ev.type == SDL_KEYDOWN)
		ev->type = ev_keydown;
	else if (sdl_ev.type == SDL_KEYUP)
		ev->type = ev_keyup;

	ev->data1 = sdl_ev.key.keysym.sym;

	// Translate the "sym" member of a SDL_keysym structure, which is part of
	// a SDL_KeyboardEvent. Previously, The "unicode" member of the SDL_keysym
	// structure was read when trying to discern the ASCII character that the
	// user pressed. However, since SDL 2.0 no longer has a "unicode" member in
	// the SDL_keysym structure, it's best to find other ways to achieve the
	// same end goal.
	KeyTranslationTable::const_iterator sdl_key_it = mSDLKeyTransTable.find(sdl_ev.key.keysym.sym);	
	if (sdl_key_it != mSDLKeyTransTable.end())
	{
		int c = sdl_key_it->second;

		// Determine if shift/caps is active 
		int shift_state = 0;
		if (sdl_ev.key.keysym.mod & KMOD_LSHIFT)
			shift_state ^= 1;
		if (sdl_ev.key.keysym.mod & KMOD_RSHIFT)
			shift_state ^= 1;
		if (sdl_ev.key.keysym.mod & KMOD_CAPS)
			shift_state ^= 1;

		// Translate the key, mimicing the shift key's behavior
		if (shift_state)
		{
			KeyTranslationTable::const_iterator shift_key_it = mShiftTransTable.find(c);
			if (shift_key_it != mShiftTransTable.end())
				c = shift_key_it->second;
		}

		ev->data2 = ev->data3 = c;
	}

	mEvents.pop();
}





// ============================================================================
//
// ISDL12MouseInputDevice implementation
//
// ============================================================================

//
// ISDL12MouseInputDevice::ISDL12MouseInputDevice
//
ISDL12MouseInputDevice::ISDL12MouseInputDevice() :
	mActive(false)
{
	reset();
}

//
// ISDL12MouseInputDevice::center
//
// Moves the mouse to the center of the screen to prevent absolute position
// methods from causing problems when the mouse is near the screen edges.
//
void ISDL12MouseInputDevice::center()
{
	const int centerx = I_GetVideoWidth() / 2;
	const int centery = I_GetVideoHeight() / 2;

	// warp the mouse to the center of the screen
	SDL_WarpMouse(centerx, centery);

	// SDL_WarpMouse inserts a mouse event to warp the cursor to the center of the screen.
	// Filter out this mouse event by reading all of the mouse events in SDL'S queue and
	// re-insert any mouse events that were not inserted by SDL_WarpMouse.
	SDL_PumpEvents();

	// Retrieve chunks of up to 1024 events from SDL
	const int max_events = 1024;
	int num_events = 0;
	SDL_Event sdl_events[max_events];

	// TODO: if there are more than 1024 events, they won't all be examined...
	num_events = SDL_PeepEvents(sdl_events, max_events, SDL_GETEVENT, SDL_MOUSEMOTION);
	assert(num_events < max_events);
	for (int i = 0; i < num_events; i++)
	{
		SDL_Event* sdl_ev = &sdl_events[i];
		assert(sdl_ev->type == SDL_MOUSEMOTION);
		if (sdl_ev->motion.x != centerx || sdl_ev->motion.y != centery)
		{
			// this event is not the event caused by SDL_WarpMouse so add it back
			// to the event queue
			SDL_PushEvent(sdl_ev);
		}
	}
}


//
// ISDL12MouseInputDevice::flushEvents
//
void ISDL12MouseInputDevice::flushEvents()
{
	gatherEvents();
	while (!mEvents.empty())
		mEvents.pop();
}


//
// ISDL12MouseInputDevice::reset
//
void ISDL12MouseInputDevice::reset()
{
	flushEvents();
	center();
}


//
// ISDL12MouseInputDevice::pause
//
// Sets the internal state to ignore all input for this device.
//
// NOTE: SDL_EventState clears the SDL event queue so only call this after all
// SDL events have been processed in all SDL input device instances.
//
void ISDL12MouseInputDevice::pause()
{
	mActive = false;
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);
	SDL_ShowCursor(true);
}


//
// ISDL12MouseInputDevice::resume
//
// Sets the internal state to enable all input for this device.
//
// NOTE: SDL_EventState clears the SDL event queue so only call this after all
// SDL events have been processed in all SDL input device instances.
//
void ISDL12MouseInputDevice::resume()
{
	mActive = true;
	SDL_ShowCursor(false);
	SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_ENABLE);
	reset();
}


//
// ISDL12MouseInputDevice::gatherEvents
//
// Pumps the SDL Event queue and retrieves any mouse events and puts them into
// this instance's event queue.
//
void ISDL12MouseInputDevice::gatherEvents()
{
	if (!mActive)
		return;

	// Force SDL to gather events from input devices. This is called
	// implicitly from SDL_PollEvent but since we're using SDL_PeepEvents to
	// process only mouse events, SDL_PumpEvents is necessary.
	SDL_PumpEvents();

	// Retrieve chunks of up to 1024 events from SDL
	int num_events = 0;
	const int max_events = 1024;
	SDL_Event sdl_events[max_events];

	while ((num_events = SDL_PeepEvents(sdl_events, max_events, SDL_GETEVENT, SDL_MOUSEEVENTMASK)))
	{
		// insert the SDL_Events into our queue
		for (int i = 0; i < num_events; i++)
			mEvents.push(sdl_events[i]);
	}

	center();
}


//
// ISDL12MouseInputDevice::getEvent
//
// Removes the first event from the queue and translates it to a Doom event_t.
// This makes no checks to ensure there actually is an event in the queue and
// if there is not, the behavior is undefined.
//
void ISDL12MouseInputDevice::getEvent(event_t* ev)
{
	assert(hasEvent());

	// clear the destination struct
	ev->type = ev_keydown;
	ev->data1 = ev->data2 = ev->data3 = 0;

	const SDL_Event& sdl_ev = mEvents.front();

	assert(sdl_ev.type == SDL_MOUSEMOTION || sdl_ev.type == SDL_MOUSEBUTTONDOWN || sv_ev.type == SDL_MOUSEBUTTONUP);

	if (sdl_ev.type == SDL_MOUSEMOTION)
	{
		ev->type = ev_mouse;
		ev->data2 = sdl_ev.motion.xrel;
		ev->data3 = -sdl_ev.motion.yrel;
	}
	else if (sdl_ev.type == SDL_MOUSEBUTTONDOWN)
	{
		ev->type = ev_keydown;

		if (sdl_ev.button.button == SDL_BUTTON_LEFT)
			ev->data1 = KEY_MOUSE1;
		else if (sdl_ev.button.button == SDL_BUTTON_RIGHT)
			ev->data1 = KEY_MOUSE2;
		else if (sdl_ev.button.button == SDL_BUTTON_MIDDLE)
			ev->data1 = KEY_MOUSE3;
		else if (sdl_ev.button.button == SDL_BUTTON_X1)
			ev->data1 = KEY_MOUSE4;	// [Xyltol 07/21/2011] - Add support for MOUSE4
		else if (sdl_ev.button.button == SDL_BUTTON_X2)
			ev->data1 = KEY_MOUSE5;	// [Xyltol 07/21/2011] - Add support for MOUSE5
		else if (sdl_ev.button.button == SDL_BUTTON_WHEELUP)
			ev->data1 = KEY_MWHEELUP;
		else if (sdl_ev.button.button == SDL_BUTTON_WHEELDOWN)
			ev->data1 = KEY_MWHEELDOWN;
	}
	else if (sdl_ev.type == SDL_MOUSEBUTTONUP)
	{
		ev->type = ev_keyup;

		if (sdl_ev.button.button == SDL_BUTTON_LEFT)
			ev->data1 = KEY_MOUSE1;
		else if (sdl_ev.button.button == SDL_BUTTON_RIGHT)
			ev->data1 = KEY_MOUSE2;
		else if (sdl_ev.button.button == SDL_BUTTON_MIDDLE)
			ev->data1 = KEY_MOUSE3;
		else if (sdl_ev.button.button == SDL_BUTTON_X1)
			ev->data1 = KEY_MOUSE4;	// [Xyltol 07/21/2011] - Add support for MOUSE4
		else if (sdl_ev.button.button == SDL_BUTTON_X2)
			ev->data1 = KEY_MOUSE5;	// [Xyltol 07/21/2011] - Add support for MOUSE5
	}

	mEvents.pop();
}


VERSION_CONTROL (i_sdlinput_cpp, "$Id: i_sdlinput.cpp 5315 2015-04-24 05:01:36Z dr_sean $")
