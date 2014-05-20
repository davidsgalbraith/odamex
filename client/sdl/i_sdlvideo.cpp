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
//	SDL implementation of the IVideo class.
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <cassert>

#include <algorithm>
#include <functional>
#include <string>
#include "doomstat.h"

// [Russell] - Just for windows, display the icon in the system menu and
// alt-tab display
#include "win32inc.h"
#if defined(_WIN32) && !defined(_XBOX)
    #include "SDL_syswm.h"
    #include "resource.h"
#endif // WIN32

#include "i_video.h"
#include "v_video.h"

#include "v_palette.h"
#include "i_sdlvideo.h"
#include "i_system.h"
#include "m_argv.h"
#include "w_wad.h"

#include "res_texture.h"

#ifdef _XBOX
#include "i_xbox.h"
#endif

EXTERN_CVAR (vid_autoadjust)
EXTERN_CVAR (vid_fullscreen)


// ****************************************************************************


// ============================================================================
//
// ISDL12WindowSurface class implementation
//
// Abstraction for SDL 1.2 drawing surfaces, wrapping the SDL_Surface struct.
//
// ============================================================================

//
// ISDL12WindowSurface::ISDL12WindowSurface
//
ISDL12WindowSurface::ISDL12WindowSurface(IWindow* window, int width, int height, int bpp) :
	IWindowSurface(window), mSDLSurface(NULL), mLocks(0)
{
	Uint32 flags = SDL_SWSURFACE;
	SDL_Surface* sdlsurface = SDL_CreateRGBSurface(flags, width, height, bpp, 0, 0, 0, 0);

	initializeFromSDLSurface(sdlsurface);
	mFreeSDLSurface = true;

	memset(mPalette, 255, 256 * sizeof(*mPalette));
}


//
// ISDL12WindowSurface::ISDL12WindowSurface
//
// Constructs the surface using an existing SDL_Surface handle.
//
ISDL12WindowSurface::ISDL12WindowSurface(IWindow* window, SDL_Surface* sdlsurface) :
	IWindowSurface(window), mSDLSurface(NULL), mLocks(0)
{
	initializeFromSDLSurface(sdlsurface);

	// shouldn't free sdlsurface if it was obtained from SDL_SetVideoMode
	mFreeSDLSurface = false;

	memset(mPalette, 255, 256 * sizeof(*mPalette));
}


//
// ISDL12WindowSurface::initializeFromSDLSurface
//
// Private helper function for the constructors.
//
void ISDL12WindowSurface::initializeFromSDLSurface(SDL_Surface* sdlsurface)
{
	if (mSDLSurface)
		SDL_FreeSurface(mSDLSurface);

	mSDLSurface = sdlsurface;

	lock();
	mSurfaceBuffer = (byte*)mSDLSurface->pixels;
	mWidth = mSDLSurface->w;
	mHeight = mSDLSurface->h;
	mBitsPerPixel = mSDLSurface->format->BitsPerPixel;
	mBytesPerPixel = mBitsPerPixel / 8;
	mPitch = mSDLSurface->pitch;
	mPitchInPixels = mPitch / mBytesPerPixel;
	unlock();

	assert(mWidth >= 0 && mWidth <= MAXWIDTH);
	assert(mHeight >= 0 && mHeight <= MAXHEIGHT);
	assert(mBitsPerPixel == 8 || mBitsPerPixel == 32);
}


//
// ISDL12WindowSurface::~ISDL12WindowSurface
//
// Frees the SDL_Surface handle.
//
ISDL12WindowSurface::~ISDL12WindowSurface()
{
	if (mSDLSurface && mFreeSDLSurface)
		SDL_FreeSurface(mSDLSurface);
}


//
// ISDL12WindowSurface::lock
//
// Locks the surface for direct pixel access. This must be called prior to
// accessing mSurfaceBuffer.
//
void ISDL12WindowSurface::lock()
{
	if (++mLocks == 1)
		SDL_LockSurface(mSDLSurface);

	assert(mLocks >= 1 && mLocks < 100);
}


//
// ISDL12WindowSurface::unlock
//
// Unlocks the surface after direct pixel access. This must be called after
// accessing mSurfaceBuffer.
//
void ISDL12WindowSurface::unlock()
{
	if (--mLocks == 0)
		SDL_UnlockSurface(mSDLSurface);

	assert(mLocks >= 0 && mLocks < 100);
}


//
// ISDL12WindowSurface::setPalette
//
// Accepts an array of 256 argb_t values.
//
void ISDL12WindowSurface::setPalette(const argb_t* palette)
{
	memcpy(mPalette, palette, 256 * sizeof(*mPalette));

	if (mSDLSurface->format->BitsPerPixel == 8)
	{
		lock();

		assert(mSDLSurface->format->palette != NULL);
		assert(mSDLSurface->format->palette->ncolors == 256);
		SDL_Color* sdlcolors = mSDLSurface->format->palette->colors;
		for (int c = 0; c < 256; c++)
		{
			sdlcolors[c].r = mPalette[c].r;
			sdlcolors[c].g = mPalette[c].g;
			sdlcolors[c].b = mPalette[c].b;
		}

		unlock();
	}
}


//
// ISDLWindowSurface::getPalette
//
const argb_t* ISDL12WindowSurface::getPalette() const
{
	return mPalette;
}


// ****************************************************************************

// ============================================================================
//
// ISDL12Window class implementation
//
// ============================================================================


//
// ISDL12Window::ISDL12Window
//
// Constructs a new application window using SDL 1.2.
// A ISDL12WindowSurface object is instantiated for frame rendering.
//
ISDL12Window::ISDL12Window(int width, int height, int bpp, bool fullscreen, bool vsync) :
	IWindow(),
	mPrimarySurface(NULL),
	mIsFullScreen(fullscreen), mUseVSync(vsync), mNeedPaletteRefresh(true)
{
	const SDL_version* SDLVersion = SDL_Linked_Version();

	if (SDLVersion->major != SDL_MAJOR_VERSION || SDLVersion->minor != SDL_MINOR_VERSION)
	{
		I_FatalError("SDL version conflict (%d.%d.%d vs %d.%d.%d dll)\n",
			SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
			SDLVersion->major, SDLVersion->minor, SDLVersion->patch);
		return;
	}

	if (SDLVersion->patch != SDL_PATCHLEVEL)
	{
		Printf_Bold("SDL version warning (%d.%d.%d vs %d.%d.%d dll)\n",
			SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
			SDLVersion->major, SDLVersion->minor, SDLVersion->patch);
	}

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
	{
		I_FatalError("Could not initialize SDL video.\n");
		return;
	}

	// Set SDL video centering
	SDL_putenv((char*)"SDL_VIDEO_WINDOW_POS=center");
	SDL_putenv((char*)"SDL_VIDEO_CENTERED=1");

	#if defined _WIN32 && !defined _XBOX
	// From the SDL 1.2.10 release notes:
	//
	// > The "windib" video driver is the default now, to prevent
	// > problems with certain laptops, 64-bit Windows, and Windows
	// > Vista.
	//
	// The hell with that.

	// SoM: the gdi interface is much faster for windowed modes which are more
	// commonly used. Thus, GDI is default.
	//
	// GDI mouse issues fill many users with great sadness. We are going back
	// to directx as defulat for now and the people will rejoice. --Hyper_Eye
	if (Args.CheckParm ("-gdi"))
		SDL_putenv((char*)"SDL_VIDEODRIVER=windib");
	else if (SDL_getenv("SDL_VIDEODRIVER") == NULL || Args.CheckParm ("-directx") > 0)
		SDL_putenv((char*)"SDL_VIDEODRIVER=directx");
	#endif	// _WIN32 && !_XBOX

	buildVideoModeList();

	setMode(width, height, bpp, fullscreen, vsync);
}


//
// ISDL12Window::~ISDL12Window
//
ISDL12Window::~ISDL12Window()
{
	delete mPrimarySurface;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


//
// ISDL12Window::refresh
//
void ISDL12Window::refresh()
{
	SDL_Surface* sdlsurface = SDL_GetVideoSurface();

	if (mNeedPaletteRefresh && sdlsurface->format->BitsPerPixel == 8)
	{
		Uint32 flags = SDL_LOGPAL | SDL_PHYSPAL;
		SDL_Color* sdlcolors = sdlsurface->format->palette->colors;
		SDL_SetPalette(sdlsurface, flags, sdlcolors, 0, 256);
	}

	mNeedPaletteRefresh = false;

	if (mUseVSync)
		SDL_Flip(sdlsurface);
	else
		SDL_UpdateRect(sdlsurface, 0, 0, 0, 0);
}


//
// ISDL12Window::setWindowTitle
//
// Sets the title caption of the window.
//
void ISDL12Window::setWindowTitle(const std::string& str)
{
	SDL_WM_SetCaption(str.c_str(), str.c_str());
}


//
// ISDL12Window::setWindowIcon
//
// Sets the icon for the application window, which will appear in the
// window manager's task list.
//
void ISDL12Window::setWindowIcon()
{
	#if WIN32 && !_XBOX
	// [SL] Use Win32-specific code to make use of multiple-icon sizes
	// stored in the executable resources. SDL 1.2 only allows a fixed
	// 32x32 px icon.
	//
	// [Russell] - Just for windows, display the icon in the system menu and
	// alt-tab display

	HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));

	if (hIcon)
	{
		HWND WindowHandle;

		SDL_SysWMinfo wminfo;
		SDL_VERSION(&wminfo.version)
		SDL_GetWMInfo(&wminfo);

		WindowHandle = wminfo.window;

		SendMessage(WindowHandle, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
		SendMessage(WindowHandle, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	}

	#else

	texhandle_t icon_handle = texturemanager.getHandle("ICON", Texture::TEX_PNG);
	const Texture* icon_texture = texturemanager.getTexture(icon_handle);
	const int icon_width = icon_texture->getWidth(), icon_height = icon_texture->getHeight();

	SDL_Surface* icon_sdlsurface = SDL_CreateRGBSurface(0, icon_width, icon_height, 8, 0, 0, 0, 0);
	Res_TransposeImage((byte*)icon_sdlsurface->pixels, icon_texture->getData(), icon_width, icon_height);

	SDL_SetColorKey(icon_sdlsurface, SDL_SRCCOLORKEY, 0);

	// set the surface palette
	const argb_t* palette_colors = V_GetDefaultPalette()->basecolors;
	SDL_Color* sdlcolors = icon_sdlsurface->format->palette->colors;
	for (int c = 0; c < 256; c++)
	{
		sdlcolors[c].r = palette_colors[c].r;
		sdlcolors[c].g = palette_colors[c].g;
		sdlcolors[c].b = palette_colors[c].b;
	}

	SDL_WM_SetIcon(icon_sdlsurface, NULL);
	SDL_FreeSurface(icon_sdlsurface);

	#endif
}


//
// ISDL12Window::getVideoDriverName
//
// Returns the name of the video driver that SDL is currently
// configured to use.
//
std::string ISDL12Window::getVideoDriverName() const
{
	char driver[128];

	if ((SDL_VideoDriverName(driver, 128)) == NULL)
	{
		const char* pdrv = getenv("SDL_VIDEODRIVER");

		if (pdrv == NULL)
			return "";
		return std::string(pdrv);
	}

	return std::string(driver);
}


//
// ISDL12Window::setPalette
//
// Saves the given palette and updates it during refresh.
//
void ISDL12Window::setPalette(const argb_t* palette)
{
	getPrimarySurface()->setPalette(palette);
	mNeedPaletteRefresh = true;
}


//
// ISDL12Window::setMode
//
// Sets the window size to the specified size and frees the existing primary
// surface before instantiating a new primary surface.
// 
bool ISDL12Window::setMode(int width, int height, int bpp, bool fullscreen, bool vsync)
{
	assert(width <= MAXWIDTH);
	assert(height <= MAXHEIGHT);
	assert(bpp == 8 || bpp == 32);

	delete mPrimarySurface;
	mPrimarySurface = NULL;

	// ensure there is an availible fullscreen mode
	if (mVideoModes.empty())
		fullscreen = false;

	// find the closest fullscreen mode to the desired dimensions
	if (fullscreen && vid_autoadjust)
	{
		IVideoMode closest_mode = getClosestMode(width, height);
		width = closest_mode.getWidth();
		height = closest_mode.getHeight();
	}

	if (width == 0 || height == 0)
		return false;

	// Make sure we don't set the resolution smaller than Doom's original 320x200
	// resolution. Bad things might happen.
	width = clamp(width, 320, MAXWIDTH);
	height = clamp(height, 200, MAXHEIGHT);

	#ifdef _WIN32
	// fullscreen directx requires a 32-bit mode to fix broken palette
	// [Russell] - Use for gdi as well, fixes d2 map02 water
	if (fullscreen)
		bpp = 32;
	#endif

	uint32_t flags = 0;

	if (vsync)
		flags |= SDL_HWSURFACE | SDL_DOUBLEBUF;
	else
		flags |= SDL_SWSURFACE;

	if (fullscreen)
		flags |= SDL_FULLSCREEN;
	else
		flags |= SDL_RESIZABLE;

	if (fullscreen && bpp == 8)
		flags |= SDL_HWPALETTE;

	// TODO: check for multicore
	flags |= SDL_ASYNCBLIT;

	// [SL] SDL_SetVideoMode reinitializes DirectInput if DirectX is being used.
	// This interferes with RawWin32Mouse's input handlers so we need to
	// disable them prior to reinitalizing DirectInput...
	I_PauseMouse();

	#ifdef SDL_GL_SWAP_CONTROL
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, vsync);
	#endif

	SDL_Surface* sdlsurface = SDL_SetVideoMode(width, height, bpp, flags);

	// [SL] ...and re-enable RawWin32Mouse's input handlers after
	// DirectInput is reinitalized.
	I_ResumeMouse();

	if (sdlsurface == NULL)
		return false;

	const SDL_PixelFormat* fmt = sdlsurface->format;
	#ifdef __BIG_ENDIAN__
	if (fmt->Rshift != 8 || fmt->Gshift != 16 || fmt->Bshift != 24)
		Printf(PRINT_HIGH, "ISDL12Window::setMode: unexpected primary surface pixel format! " \
					"r = %i, g = %i, b = %i\n", fmt->Rshift, fmt->Gshift, fmt->Bshift);
	#else
	if (fmt->Rshift != 16 || fmt->Gshift != 8 || fmt->Bshift != 0)
		Printf(PRINT_HIGH, "ISDL12Window::setMode: unexpected primary surface pixel format! " \
					"r = %i, g = %i, b = %i\n", fmt->Rshift, fmt->Gshift, fmt->Bshift);
	#endif

	mWidth = width;
	mHeight = height;
	mBitsPerPixel = bpp;
	mIsFullScreen = fullscreen;
	mUseVSync = vsync;

	// create a new IWindowSurface for the SDL_Surface handle that
	// was returned by SDL_SetVideoMode
	mPrimarySurface = new ISDL12WindowSurface(this, sdlsurface);	

	return true;
}


//
// ISDL12Window::buildVideoModeList
//
// Queries SDL for the supported full screen video modes and populates
// the mVideoModes list.
//
void ISDL12Window::buildVideoModeList()
{
	mVideoModes.clear();

	// Fetch the list of fullscreen modes for this bpp setting:
	SDL_Rect** sdlmodes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_SWSURFACE);

	if (sdlmodes == NULL)
	{
		// no fullscreen modes, but we could still try windowed
		Printf(PRINT_HIGH, "No fullscreen video modes are available.\n");
		return;
	}
	else if (sdlmodes == (SDL_Rect**)-1)
	{
		// SDL 1.2 documentation indicates the following
		// "-1: Any dimension is okay for the given format"
		// Shouldn't happen with SDL_FULLSCREEN flag though

		I_FatalError("SDL_ListModes returned -1. Internal error.\n");
		return;
	}

	// always add the following modes
	mVideoModes.push_back(IVideoMode(320, 200));
	mVideoModes.push_back(IVideoMode(320, 240));
	mVideoModes.push_back(IVideoMode(640, 400));
	mVideoModes.push_back(IVideoMode(640, 480));

	// add the full screen video modes reported by SDL	
	while (*sdlmodes)
	{
		int width = (*sdlmodes)->w, height = (*sdlmodes)->h;
		if (width > 0 && width <= MAXWIDTH && height > 0 && height <= MAXHEIGHT)
			mVideoModes.push_back(IVideoMode(width, height));
		++sdlmodes;
	}

	// reverse sort the modes
	std::sort(mVideoModes.begin(), mVideoModes.end(), std::greater<IVideoMode>());

	// get rid of any duplicates (SDL some times reports duplicates)
	mVideoModes.erase(std::unique(mVideoModes.begin(), mVideoModes.end()), mVideoModes.end());
}

VERSION_CONTROL (i_sdlvideo_cpp, "$Id$")

