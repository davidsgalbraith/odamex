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
// Network buffer that permits bitlevel reading and writing
//
//-----------------------------------------------------------------------------

#ifndef __NET_BITSTREAM_H__
#define __NET_BITSTREAM_H__

#include "doomtype.h"
#include "m_ostring.h"

class BitStream
{
public:
	BitStream(uint16_t capacity = BitStream::MAX_CAPACITY);
	BitStream(const BitStream &other);
	~BitStream();

	BitStream& operator=(const BitStream& other);

	void clear();
	uint16_t bytesRead() const;
	uint16_t bytesWritten() const;
	uint16_t bitsRead() const;
	uint16_t bitsWritten() const;

	uint16_t readSize() const;
	uint16_t writeSize() const;

	void writeBlob(const uint8_t* data, uint16_t size);
	void readBlob(uint8_t* data, uint16_t size);

	void writeBits(int val, uint16_t bitcount);
	void writeBit(int val);
	void writeS8(int val);
	void writeU8(int val);
	void writeS16(int val);
	void writeU16(int val);
	void writeS32(int val);
	void writeU32(int val);
	void writeFloat(float val);
	void writeString(const OString& str);
	
	int readBits(uint16_t bitcount);
	bool readBit();
	int8_t readS8();
	uint8_t readU8();
	int16_t readS16();
	uint16_t readU16();
	int32_t readS32();
	uint32_t readU32();
	float readFloat();
	OString readString();

	int peekBits(uint16_t bitcount) const;
	bool peekBit() const;
	int8_t peekS8() const;
	uint8_t peekU8() const;
	int16_t peekS16() const;
	uint16_t peekU16() const;
	int32_t peekS32() const;
	uint32_t peekU32() const;

	const uint8_t* getRawData() const;

private:
	static const uint16_t MAX_CAPACITY = 65535;		// in bits

	bool mCheckReadOverflow(uint16_t size) const;
	bool mCheckWriteOverflow(uint16_t size) const;
	
	uint16_t		mCapacity;
	uint16_t		mWritten;
	uint16_t		mRead;

	mutable bool	mWriteOverflow;
	mutable bool	mReadOverflow;

	uint8_t			*mBuffer;
};

#endif	// __NET_BITSTREAM_H__


