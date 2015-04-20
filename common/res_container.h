// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: res_container.h $
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
//
//-----------------------------------------------------------------------------

#ifndef __RES_CONTAINER_H__
#define __RES_CONTAINER_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include "m_ostring.h"

// ============================================================================
//
// ContainerDirectory class interface & implementation
//
// ============================================================================
//
// A generic resource container directory for querying information about
// game resource lumps.
//

class ContainerDirectory
{
private:
	struct EntryInfo
	{
		OString		name;
		size_t		length;
		size_t		offset;
	};

	typedef std::vector<EntryInfo> EntryInfoList;

public:
	typedef EntryInfoList::iterator iterator;
	typedef EntryInfoList::const_iterator const_iterator;

	ContainerDirectory(const size_t initial_size = 4096) :
		mNameLookup(2 * initial_size)
	{
		mEntries.reserve(initial_size);
	}

	iterator begin()
	{
		return mEntries.begin();
	}

	const_iterator begin() const
	{
		return mEntries.begin();
	}

	iterator end()
	{
		return mEntries.end();
	}

	const_iterator end() const
	{
		return mEntries.end();
	}

	const LumpId getLumpId(iterator it) const
	{
		return it - begin();
	}

	const LumpId getLumpId(const_iterator it) const
	{
		return it - begin();
	}

	size_t size() const
	{
		return mEntries.size();
	}

	bool validate(const LumpId lump_id) const
	{
		return lump_id < mEntries.size();
	}

	void addEntryInfo(const OString& name, size_t length, size_t offset = 0)
	{
		mEntries.push_back(EntryInfo());
		EntryInfo* entry = &mEntries.back();
		entry->name = name;
		entry->length = length;
		entry->offset = offset;

		const LumpId lump_id = getLumpId(entry);
		assert(lump_id != INVALID_LUMP_ID);
		mNameLookup.insert(std::make_pair(name, lump_id));
	}

	size_t getLength(const LumpId lump_id) const
	{
		return mEntries[lump_id].length;
	}

	size_t getOffset(const LumpId lump_id) const
	{
		return mEntries[lump_id].offset;
	}

	const OString& getName(const LumpId lump_id) const
	{
		return mEntries[lump_id].name;
	}

	bool between(const LumpId lump_id, const OString& start, const OString& end) const
	{
		LumpId start_lump_id = getLumpId(start);
		LumpId end_lump_id = getLumpId(end);

		if (lump_id == INVALID_LUMP_ID || start_lump_id == INVALID_LUMP_ID || end_lump_id == INVALID_LUMP_ID)
			return false;
		return start_lump_id < lump_id && lump_id < end_lump_id;
	}

	bool between(const OString& name, const OString& start, const OString& end) const
	{
		return between(getLumpId(name), start, end);
	}

	const OString& next(const LumpId lump_id) const
	{
		if (lump_id != INVALID_LUMP_ID && lump_id + 1 < mEntries.size())
			return mEntries[lump_id + 1].name;
		static OString empty_string;
		return empty_string; 
	}

	const OString& next(const OString& name) const
	{
		return next(getLumpId(name));
	}

private:
	static const LumpId INVALID_LUMP_ID = static_cast<LumpId>(-1);

	EntryInfoList		mEntries;

	typedef OHashTable<OString, LumpId> NameLookupTable;
	NameLookupTable		mNameLookup;

	LumpId getLumpId(const OString& name) const
	{
		NameLookupTable::const_iterator it = mNameLookup.find(OStringToUpper(name));
		if (it != mNameLookup.end())
			return getLumpId(&mEntries[it->second]);
		return INVALID_LUMP_ID;
	}

	LumpId getLumpId(const EntryInfo* entry) const
	{
		if (!mEntries.empty() && static_cast<LumpId>(entry - &mEntries.front()) < mEntries.size())
			return static_cast<LumpId>(entry - &mEntries.front());
		return INVALID_LUMP_ID;
	}
};

#endif	// __RES_CONTAINER_H__
