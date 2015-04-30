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
#include "i_system.h"
#include "doomkeys.h"
#include <queue>
#include <cassert>

// ============================================================================
//
// ISDL12KeyboardInputDevice implementation
//
// ============================================================================

//
// ISDL12KeyboardInputDevice::ISDL12KeyboardInputDevice
//
ISDL12KeyboardInputDevice::ISDL12KeyboardInputDevice(int id) :
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
	mShiftTransTable['A'] = 'a';
	mShiftTransTable['B'] = 'b';
	mShiftTransTable['C'] = 'c';
	mShiftTransTable['D'] = 'd';
	mShiftTransTable['E'] = 'e';
	mShiftTransTable['F'] = 'f';
	mShiftTransTable['G'] = 'g';
	mShiftTransTable['H'] = 'h';
	mShiftTransTable['I'] = 'i';
	mShiftTransTable['J'] = 'j';
	mShiftTransTable['K'] = 'k';
	mShiftTransTable['L'] = 'l';
	mShiftTransTable['M'] = 'm';
	mShiftTransTable['N'] = 'n';
	mShiftTransTable['O'] = 'o';
	mShiftTransTable['P'] = 'p';
	mShiftTransTable['Q'] = 'q';
	mShiftTransTable['R'] = 'r';
	mShiftTransTable['S'] = 's';
	mShiftTransTable['T'] = 't';
	mShiftTransTable['U'] = 'u';
	mShiftTransTable['V'] = 'v';
	mShiftTransTable['W'] = 'w';
	mShiftTransTable['X'] = 'x';
	mShiftTransTable['Y'] = 'y';
	mShiftTransTable['Z'] = 'z';
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

	bool quit_event = false;

	while ((num_events = SDL_PeepEvents(sdl_events, max_events, SDL_GETEVENT, SDL_KEYEVENTMASK)))
	{
		for (int i = 0; i < num_events; i++)
		{
			const SDL_Event& sdl_ev = sdl_events[i];
			assert(sdl_ev.type == SDL_KEYDOWN || sdl_ev.type == SDL_KEYUP);

			if (sdl_ev.key.keysym.sym == SDLK_F4 && sdl_ev.key.keysym.mod & (KMOD_LALT | KMOD_RALT))
			{
				// HeX9109: Alt+F4 for cheats! Thanks Spleen
				// [SL] Don't handle it here but make sure we indicate there was an ALT+F4 event.
				quit_event = true;
			}
			else
			{
				// Normal game keyboard event - insert it into our internal queue
				event_t ev;
				ev.type = (sdl_ev.type == SDL_KEYDOWN) ? ev_keydown : ev_keyup;
				ev.data1 = sdl_ev.key.keysym.sym;
				ev.data2 = ev.data3 = 0;

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

					// handle CAPS LOCK and translate 'a'-'z' to 'A'-'Z'
					if (c >= 'a' && c <= 'z' && (sdl_ev.key.keysym.mod & KMOD_CAPS))
						c = mShiftTransTable[c];

					// Handle SHIFT keys
					if (sdl_ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
					{
						KeyTranslationTable::const_iterator shift_key_it = mShiftTransTable.find(c);
						if (shift_key_it != mShiftTransTable.end())
							c = shift_key_it->second;
					}

					ev.data2 = ev.data3 = c;
				}

				mEvents.push(ev);
			}
		}
	}

	// Translate the ALT+F4 key combo event into a SDL_QUIT event and push
	// it back into SDL's event queue so that it can be handled elsewhere.
	if (quit_event)
	{
		SDL_Event sdl_ev;
		sdl_ev.type = SDL_QUIT;
		SDL_PushEvent(&sdl_ev);
	}
}


//
// ISDL12KeyboardInputDevice::getEvent
//
// Removes the first event from the queue and returns it.
// This makes no checks to ensure there actually is an event in the queue and
// if there is not, the behavior is undefined.
//
void ISDL12KeyboardInputDevice::getEvent(event_t* ev)
{
	assert(hasEvent());

	memcpy(ev, &mEvents.front(), sizeof(event_t));

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
ISDL12MouseInputDevice::ISDL12MouseInputDevice(int id) :
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
	reset();
	SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_ENABLE);
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
		{
			const SDL_Event& sdl_ev = sdl_events[i];
			assert(sdl_ev.type == SDL_MOUSEMOTION ||
					sdl_ev.type == SDL_MOUSEBUTTONDOWN || sdl_ev.type == SDL_MOUSEBUTTOUP);

			event_t ev;
			ev.data1 = ev.data2 = ev.data3 = 0;

			if (sdl_ev.type == SDL_MOUSEMOTION)
			{
				ev.type = ev_mouse;
				ev.data2 = sdl_ev.motion.xrel;
				ev.data3 = -sdl_ev.motion.yrel;
			}
			else if (sdl_ev.type == SDL_MOUSEBUTTONDOWN || sdl_ev.type == SDL_MOUSEBUTTONUP)
			{
				ev.type = (sdl_ev.type == SDL_MOUSEBUTTONDOWN) ? ev_keydown : ev_keyup;
				if (sdl_ev.button.button == SDL_BUTTON_LEFT)
					ev.data1 = KEY_MOUSE1;
				else if (sdl_ev.button.button == SDL_BUTTON_RIGHT)
					ev.data1 = KEY_MOUSE2;
				else if (sdl_ev.button.button == SDL_BUTTON_MIDDLE)
					ev.data1 = KEY_MOUSE3;
				else if (sdl_ev.button.button == SDL_BUTTON_X1)
					ev.data1 = KEY_MOUSE4;	// [Xyltol 07/21/2011] - Add support for MOUSE4
				else if (sdl_ev.button.button == SDL_BUTTON_X2)
					ev.data1 = KEY_MOUSE5;	// [Xyltol 07/21/2011] - Add support for MOUSE5
				else if (sdl_ev.button.button == SDL_BUTTON_WHEELUP)
					ev.data1 = KEY_MWHEELUP;
				else if (sdl_ev.button.button == SDL_BUTTON_WHEELDOWN)
					ev.data1 = KEY_MWHEELDOWN;
			}

			mEvents.push(ev);
		}
	}

//	center();
}


//
// ISDL12MouseInputDevice::getEvent
//
// Removes the first event from the queue and returns it.
// This makes no checks to ensure there actually is an event in the queue and
// if there is not, the behavior is undefined.
//
void ISDL12MouseInputDevice::getEvent(event_t* ev)
{
	assert(hasEvent());

	memcpy(ev, &mEvents.front(), sizeof(event_t));

	mEvents.pop();
}



// ============================================================================
//
// ISDL12JoystickInputDevice implementation
//
// ============================================================================

//
// ISDL12JoystickInputDevice::ISDL12JoystickInputDevice
//
ISDL12JoystickInputDevice::ISDL12JoystickInputDevice(int id) :
	mActive(false), mJoystickId(id), mJoystick(NULL),
	mNumHats(0), mHatStates(NULL)
{
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	assert(SDL_WasInit(SDL_INIT_JOYSTICK));
	assert(mJoystickId >= 0 && mJoystickId < SDL_NumJoysticks());

	assert(!SDL_JoystickOpened(mJoystickId));

	mJoystick = SDL_JoystickOpen(mJoystickId);
	assert(mJoystick != NULL);

	if (!SDL_JoystickOpened(mJoystickId))
		return;

	mNumHats = SDL_JoystickNumHats(mJoystick);
	mHatStates = new int[mNumHats];
	for (int i = 0; i < mNumHats; i++)
		mHatStates[i] = SDL_HAT_CENTERED;

	// This turns on automatic event polling for joysticks so that the state
	// of each button and axis doesn't need to be manually queried each tick. -- Hyper_Eye
	SDL_JoystickEventState(SDL_ENABLE);
	
	resume();
}


//
// ISDL12JoystickInputDevice::~ISDL12JoystickInputDevice
//
ISDL12JoystickInputDevice::~ISDL12JoystickInputDevice()
{
	pause();

	SDL_JoystickEventState(SDL_IGNORE);

#ifndef _XBOX // This is to avoid a bug in SDLx
	if (SDL_JoystickOpened(mJoystickId))
		SDL_JoystickClose(mJoystick);
#endif

	assert(!SDL_JoystickOpen(mJoystickId));

	delete [] mHatStates;
}


//
// ISDL12JoystickInputDevice::flushEvents
//
void ISDL12JoystickInputDevice::flushEvents()
{
	gatherEvents();
	while (!mEvents.empty())
		mEvents.pop();
	for (int i = 0; i < mNumHats; i++)
		mHatStates = SDL_HAT_CENTERED;
}


//
// ISDL12JoystickInputDevice::reset
//
void ISDL12JoystickInputDevice::reset()
{
	flushEvents();
}


//
// ISDL12JoystickInputDevice::pause
//
// Sets the internal state to ignore all input for this device.
//
// NOTE: SDL_EventState clears the SDL event queue so only call this after all
// SDL events have been processed in all SDL input device instances.
//
void ISDL12JoystickInputDevice::pause()
{
	mActive = false;
}


//
// ISDL12JoystickInputDevice::resume
//
// Sets the internal state to enable all input for this device.
//
// NOTE: SDL_EventState clears the SDL event queue so only call this after all
// SDL events have been processed in all SDL input device instances.
//
void ISDL12JoystickInputDevice::resume()
{
	mActive = true;
	reset();
}


//
// ISDL12JoystickInputDevice::gatherEvents
//
// Pumps the SDL Event queue and retrieves any joystick events and translates
// them to an event_t instances before putting them into this instance's
// event queue.
//
void ISDL12JoystickInputDevice::gatherEvents()
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

	while ((num_events = SDL_PeepEvents(sdl_events, max_events, SDL_GETEVENT, SDL_JOYEVENTMASK)))
	{
		for (int i = 0; i < num_events; i++)
		{
			const SDL_Event& sdl_ev = sdl_events[i];

			assert(sdl_ev.type == SDL_JOYBUTTONDOWN || sdl_ev.type == SDL_JOYBUTTONUP ||
					sdl_ev.type == SDL_JOYAXISMOTION || sdl_ev.type == SDL_JOYHATMOTION ||
					sdl_ev.type == SDL_JOYBALLMOTION);

			if ((sdl_ev.type == SDL_JOYBUTTONDOWN || sdl_ev.type == SDL_JOYBUTTONUP) &&
				sdl_ev.jbutton.which == mJoystickId)
			{
				event_t button_event;
				button_event.data1 = button_event.data2 = sdl_ev.jbutton.button + KEY_JOY1;
				button_event.data3 = 0;
				button_event.type = (sdl_ev.type == SDL_JOYBUTTONDOWN) ? ev_keydown : ev_keyup;
				mEvents.push(button_event);
			}
			else if (sdl_ev.type == SDL_JOYAXISMOTION && sdl_ev.jaxis.which == mJoystickId)
			{
				event_t motion_event;
				motion_event.type = ev_joystick;
				motion_event.data1 = motion_event.data3 = 0;
				motion_event.data2 = sdl_ev.jaxis.axis;
				if ((sdl_ev.jaxis.value >= JOY_DEADZONE) || (sdl_ev.jaxis.value <= -JOY_DEADZONE))
					motion_event.data3 = sdl_ev.jaxis.value;
				mEvents.push(motion_event);
			}
			else if (sdl_ev.type == SDL_JOYHATMOTION && sdl_ev.jhat.which == mJoystickId)
			{
				// [SL] A single SDL joystick hat event indicates on/off for each of the
				// directional triggers for that hat. We need to create a separate 
				// ev_keydown or ev_keyup event_t instance for each directional trigger
				// indicated in this SDL joystick event.
				assert(sdl_ev.jhat.hat < mNumHats);
				int new_state = sdl_ev.jhat.value;
				int old_state = mHatStates[sdl_ev.jhat.hat];

				static const int flags[4] = { SDL_HAT_UP, SDL_HAT_RIGHT, SDL_HAT_DOWN, SDL_HAT_LEFT };
				for (int i = 0; i < 4; i++)
				{
					event_t hat_event;
					hat_event.data1 = hat_event.data2 = (sdl_ev.jhat.hat * 4) + KEY_HAT1 + i;
					hat_event.data3 = 0;

					// determine if the flag's state has changed (ignore it if it hasn't)
					if (!(old_state & flags[i]) && (new_state & flags[i]))
					{
						hat_event.type = ev_keydown;
						mEvents.push(hat_event);
					}
					else if ((old_state & flags[i]) && !(new_state & flags[i]))
					{
						hat_event.type = ev_keyup;
						mEvents.push(hat_event);
					}
				}

				mHatStates[sdl_ev.jhat.hat] = new_state;
			}
		}
	}
}


//
// ISDL12JoystickInputDevice::getEvent
//
// Removes the first event from the queue and returns it.
// This makes no checks to ensure there actually is an event in the queue and
// if there is not, the behavior is undefined.
//
void ISDL12JoystickInputDevice::getEvent(event_t* ev)
{
	assert(hasEvent());

	memcpy(ev, &mEvents.front(), sizeof(event_t));

	mEvents.pop();
}

// ============================================================================
//
// ISDL12InputSubsystem implementation
//
// ============================================================================

//
// ISDL12InputSubsystem::ISDL12InputSubsystem
//
ISDL12InputSubsystem::ISDL12InputSubsystem() :
	IInputSubsystem(),
	mInputGrabbed(false)
{
	grabInput();
	mRepeatDelay = I_ConvertTimeFromMs(SDL_DEFAULT_REPEAT_DELAY);
	mRepeatInterval = I_ConvertTimeFromMs(SDL_DEFAULT_REPEAT_INTERVAL);
}


//
// ISDL12InputSubsystem::~ISDL12InputSubsystem
//
ISDL12InputSubsystem::~ISDL12InputSubsystem()
{
	if (getKeyboardInputDevice())
		shutdownKeyboard(0);
	if (getMouseInputDevice())
		shutdownMouse(0);
	if (getJoystickInputDevice())
		shutdownJoystick(0);
}


//
// ISDL12InputSubsystem::getKeyboardDevices
//
// SDL only allows for one logical keyboard so just return a dummy device
// description.
//
std::vector<IInputDeviceInfo> ISDL12InputSubsystem::getKeyboardDevices() const
{
	std::vector<IInputDeviceInfo> devices;
	devices.push_back(IInputDeviceInfo());
	IInputDeviceInfo& device_info = devices.back();
	device_info.mId = 0;
	device_info.mDeviceName = "SDL 1.2 keyboard";
	return devices;
}


//
// ISDL12InputSubsystem::initKeyboard
//
void ISDL12InputSubsystem::initKeyboard(int id)
{
	setKeyboardInputDevice(new ISDL12KeyboardInputDevice(id));
	registerInputDevice(getKeyboardInputDevice());
	getKeyboardInputDevice()->resume();
}


//
// ISDL12InputSubsystem::shutdownKeyboard
//
void ISDL12InputSubsystem::shutdownKeyboard(int id)
{
	unregisterInputDevice(getKeyboardInputDevice());
	delete getKeyboardInputDevice();
	setKeyboardInputDevice(NULL);
}


//
// ISDL12InputSubsystem::getMouseDevices
//
// SDL only allows for one logical mouse so just return a dummy device
// description.
//
std::vector<IInputDeviceInfo> ISDL12InputSubsystem::getMouseDevices() const
{
	std::vector<IInputDeviceInfo> devices;
	devices.push_back(IInputDeviceInfo());
	IInputDeviceInfo& device_info = devices.back();
	device_info.mId = 0;
	device_info.mDeviceName = "SDL 1.2 mouse";
	return devices;
}


//
// ISDL12InputSubsystem::initMouse
//
void ISDL12InputSubsystem::initMouse(int id)
{
	setMouseInputDevice(new ISDL12MouseInputDevice(id));
	registerInputDevice(getMouseInputDevice());
	getMouseInputDevice()->resume();
}


//
// ISDL12InputSubsystem::shutdownMouse
//
void ISDL12InputSubsystem::shutdownMouse(int id)
{
	unregisterInputDevice(getMouseInputDevice());
	delete getMouseInputDevice();
	setMouseInputDevice(NULL);
}


//
//
// ISDL12InputSubsystem::getJoystickDevices
//
//
std::vector<IInputDeviceInfo> ISDL12InputSubsystem::getJoystickDevices() const
{
	// TODO: does the SDL Joystick subsystem need to be initialized?
	std::vector<IInputDeviceInfo> devices;
	for (int i = 0; i < SDL_NumJoysticks(); i++)
	{
		devices.push_back(IInputDeviceInfo());
		IInputDeviceInfo& device_info = devices.back();
		device_info.mId = i;
		char name[256];
		sprintf(name, "SDL 1.2 joystick (%s)", SDL_JoystickName(i));
		device_info.mDeviceName = name;
	}

	return devices;
}


// ISDL12InputSubsystem::initJoystick
//
void ISDL12InputSubsystem::initJoystick(int id)
{
	setJoystickInputDevice(new ISDL12JoystickInputDevice(id));
	registerInputDevice(getJoystickInputDevice());
	getJoystickInputDevice()->resume();
}


//
// ISDL12InputSubsystem::shutdownJoystick
//
void ISDL12InputSubsystem::shutdownJoystick(int id)
{
	unregisterInputDevice(getJoystickInputDevice());
	delete getJoystickInputDevice();
	setJoystickInputDevice(NULL);
}


//
// ISDL12InputSubsystem::grabInput
//
void ISDL12InputSubsystem::grabInput()
{
	SDL_WM_GrabInput(SDL_GRAB_ON);
	mInputGrabbed = true;
}


//
// ISDL12InputSubsystem::releaseInput
//
void ISDL12InputSubsystem::releaseInput()
{
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	mInputGrabbed = false;
}

VERSION_CONTROL (i_sdlinput_cpp, "$Id: i_sdlinput.cpp 5315 2015-04-24 05:01:36Z dr_sean $")
