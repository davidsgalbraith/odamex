// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: r_main.h 1856 2010-09-05 03:14:13Z ladna $
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
//	Texture manager to load and convert textures.	
//
//-----------------------------------------------------------------------------

#ifndef __R_TEXTURE_H__
#define __R_TEXTURE_H__

#include "r_data.h"
#include <map>
#include <string>

typedef unsigned int texhandle_t;

class TextureManager;

// ============================================================================
//
// Texture
//
// ============================================================================
//
// Texture is a straight-forward abstraction of Doom's various graphic formats.
// The image is stored in column-major format as a set of 8-bit palettized
// pixels.
//
// A Texture is allocated and initialized by TextureManager so that all memory
// used by a Texture can freed at the same time when the Zone heap needs to
// free memory.
//
class Texture
{
public:
	Texture();

	typedef enum
	{
		TEX_FLAT,
		TEX_PATCH,
		TEX_SPRITE,
		TEX_WALLTEXTURE
	} TextureSourceType;

	byte* getData() const
	{	return mData;	}

	byte* getColumnData(fixed_t x) const
	{	return mColumnLookup[(x & mWidthMask) >> FRACBITS];	}

	fixed_t getWidth() const
	{	return mWidth;	}

	fixed_t getHeight() const
	{	return mHeight;	}

	int getWidthBits() const
	{	return mWidthBits;	}

	int getHeightBits() const
	{	return mHeightBits;	}

	int getOffsetX() const
	{	return mOffsetX;	}

	int getOffsetY() const
	{	return mOffsetY;	}

	fixed_t getScaleX() const
	{	return mScaleX;	}

	fixed_t getScaleY() const
	{	return mScaleY;	}

	
private:
	friend class TextureManager;

	void init(int width, int height);

	static size_t calculateSize(int width, int height)
	{	return sizeof(Texture) + width * sizeof(mColumnLookup) + width * height * sizeof(byte);	}

	fixed_t				mWidth;
	fixed_t				mHeight;
	
	int					mWidthBits;
	int					mHeightBits;

	fixed_t				mWidthMask;
	fixed_t				mHeightMask;

	int					mOffsetX;
	int					mOffsetY;

	fixed_t				mScaleX;
	fixed_t				mScaleY;

	byte**				mColumnLookup;
	byte*				mData;
};


// ============================================================================
//
// TextureManager
//
// ============================================================================
//
// TextureManager provides a unified interface for loading and accessing the
// various types of graphic formats needed by Doom's renderer and interface.
// Its goal is to simplify loading graphic lumps and allow the different
// format graphic lumps to be used interchangeably, for example, allowing
// flats to be used as wall textures.
//
// A handle to a texture is retrieved by calling getHandle() with the name
// of the texture and a texture source type. The name can be either the name
// of a lump in the WAD (in the case of a flat, sprite, or patch) or it can
// be the name of an entry in one of the TEXTURE* lumps. The texture source
// type parameter is used to indicate where to initially search for the
// texture. For example, if the type is TEX_WALLTEXTURE, the TEXTURE* lumps
// will be searched for a matching name first, followed by flats, patches,
// and sprites.
//
// A pointer to a Texture object can be retrieved by passing a texture
// handle to getTexture(). The allocation and freeing of Texture objects is
// managed internally by TextureManager and as such, users should not store
// Texture pointers but instead store the texture handle and use getTexture
// when a Texture object is needed.
//
class TextureManager
{
public:
	TextureManager();
	~TextureManager();

	void init();
	void clear();
	void precache();

	texhandle_t getHandle(const char* name, Texture::TextureSourceType type);
	const Texture* getTexture(texhandle_t handle);

	static const texhandle_t NOT_PRESENT_TEXTURE_HANDLE	= 0x0;
	static const texhandle_t NOT_FOUND_TEXTURE_HANDLE	= 0xFFFFFFFFul;

private:
	static const unsigned int FLAT_HANDLE_MASK			= 0x10000000ul;
	static const unsigned int PATCH_HANDLE_MASK			= 0x20000000ul;
	static const unsigned int SPRITE_HANDLE_MASK		= 0x40000000ul;
	static const unsigned int WALLTEXTURE_HANDLE_MASK	= 0x80000000ul;

	void generateNotFoundTexture();
	void readPNamesDirectory();
	void addTextureDirectory(const char* lumpname); 
	unsigned int lookupWallTexture(const char* name);
	unsigned int lookupFlat(const char* name);

	texhandle_t getFlatHandle(unsigned int lumpnum);
	void cacheFlat(texhandle_t handle);

	texhandle_t getWallTextureHandle(unsigned int lumpnum);
	void cacheWallTexture(texhandle_t handle);

	texhandle_t addFlat(unsigned int flatnum);
	texhandle_t addPatch(unsigned int patchnum);

	void removeFlat(unsigned int flatnum);

	// maps texture handles to Texture*
	std::map<texhandle_t, Texture*>	mHandleMap;

	// lookup table to translate flatnum to mTextures index
	unsigned int				mFirstFlatLumpNum;
	unsigned int				mLastFlatLumpNum;

	int*						mPNameLookup;
	std::vector<texture_t*>		mTextureDefinitions;

	std::map<std::string, unsigned int>	mTextureNameTranslationMap;
};

extern TextureManager texturemanager;

#endif // __R_TEXTURE_H__