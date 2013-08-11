// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
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
//	A hash table implementation with iterator.
//
//-----------------------------------------------------------------------------

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <utility>
#include <string>

// ============================================================================
//
// HashTable
//
// Lightweight templated hashtable implementation with iterator.
// The template parameters are:
// 		key type,
//		value type,
//		hash functor with the following signature (optional):
//			size_t operator()(KT) const;
//
// ============================================================================

// Default hash functors for integer & string types
template <typename KT>
struct hashfunc
{ };

template <> struct hashfunc<unsigned char>
{	size_t operator()(unsigned char val) const { return val; }	};

template <> struct hashfunc<signed char>
{	size_t operator()(signed char val) const { return val; }	};

template <> struct hashfunc<unsigned short>
{	size_t operator()(unsigned short val) const { return val; }	};

template <> struct hashfunc<signed short>
{	size_t operator()(signed short val) const { return val; }	};

template <> struct hashfunc<unsigned int>
{	size_t operator()(unsigned int val) const { return val; }	};

template <> struct hashfunc<signed int>
{	size_t operator()(signed int val) const { return val; }		};

template <> struct hashfunc<unsigned long long>
{	size_t operator()(unsigned long long val) const { return val; }	};

template <> struct hashfunc<signed long long>
{	size_t operator()(signed long long val) const { return val; }	};

template <> struct hashfunc<const char*>
{	size_t operator()(const char* cstr) const
	{
		size_t val = 0;
		while (*cstr != 0)
			val = val * 101 + *cstr++;
		return val;	
	}
};

template <> struct hashfunc<const std::string&>
{	size_t operator()(const std::string& str) const
	{
		const char* cstr = str.c_str();
		size_t val = 0;
		while (*cstr != 0)
			val = val * 101 + *cstr++;
		return val;
	}
};

// ----------------------------------------------------------------------------
// HashTable interface & inline implementation
//
// The implementation is fairly straight forward. Hash collisions are resolved
// with chaining, making each bucket a doubly-linked list of nodes with matching
// hash keys. Iteration through the hash table is done quickly via a
// doubly-linked list of all of the nodes.
// ----------------------------------------------------------------------------

template <typename KT, typename VT, typename HF = hashfunc<KT> >
class HashTable
{
private:
	typedef std::pair<KT, VT> HashPair;

	class Node
	{
	public:
		Node() : mChainNext(NULL), mChainPrev(NULL), mItrPrev(NULL), mItrNext(NULL) { }
		
		HashPair			mKeyValuePair;
		Node*				mChainNext;
		Node*				mChainPrev;
		Node*				mItrPrev;
		Node*				mItrNext;
	};
	
public:
	// ------------------------------------------------------------------------
	// HashTable::iterator & const_iterator implementation
	// ------------------------------------------------------------------------

	template <typename IteratorType>
	class generic_iterator : public std::iterator<std::forward_iterator_tag, HashTable>
	{
	public:
		generic_iterator() :
			mNode(NULL), mHashTable(NULL)
		{ }

		generic_iterator(const generic_iterator& other) :
			mNode(other.mNode), mHashTable(other.mHashTable)
		{ }

		generic_iterator& operator= (const generic_iterator& other)
		{
			if (&other != this)
			{
				mNode = other.mNode;
				mHashTable = other.mHashTable;
			}
			return *this;
		}

		bool operator== (const generic_iterator& other) const
		{
			return mNode == other.mNode && mHashTable == other.mHashTable;
		}

		bool operator!= (const generic_iterator& other) const
		{
			return mNode != other.mNode || mHashTable != other.mHashTable;
		}

		std::pair<KT, VT>& operator* ()
		{
			return mNode->mKeyValuePair;
		}

		std::pair<KT, VT>* operator-> ()
		{
			return &(mNode->mKeyValuePair);
		}

		generic_iterator& operator++ ()
		{
			mNode = mNode->mItrNext;
			return *this;
		}

		generic_iterator operator++ (int)
		{
			generic_iterator temp(*this);
			mNode = mNode->mItrNext;
			return temp;
		}

		friend class HashTable<KT, VT, HF>;

	private:
		generic_iterator(typename HashTable<KT, VT, HF>::Node* node, HashTable<KT, VT, HF>* hashtable) :
			mNode(node), mHashTable(hashtable)
		{ }

		typename HashTable<KT, VT, HF>::Node*	mNode;
		HashTable<KT, VT, HF>*					mHashTable;	
	};

	typedef generic_iterator< std::pair<KT, VT> > iterator;
	typedef generic_iterator< const std::pair<KT, VT> > const_iterator;


	// ------------------------------------------------------------------------
	// HashTable functions
	// ------------------------------------------------------------------------

	HashTable(size_t size = 32) :
		mSize(0), mUsed(0), mHashTable(NULL), mHashItrList(NULL)
	{
		resize(size);
	}

	HashTable(const HashTable<KT, VT, HF>& other) :
		mSize(0), mUsed(0), mHashTable(NULL), mHashItrList(NULL)
	{
		resize(other.mSize);
		for (Node* itr = other.mHashItrList; itr != NULL; itr = itr->mItrNext)
		{
			const KT& key = itr->mKeyValuePair.first;
			const VT& value = itr->mKeyValuePair.second;
			insertNode(key, value);
		}
	}

	~HashTable()
	{
		clear();
		delete [] mHashTable;
	}

	HashTable& operator= (const HashTable<KT, VT, HF>& other)
	{
		clear();
		resize(other.mSize);
		for (Node* itr = other.mHashItrList; itr != NULL; itr = itr->mItrNext)
		{
			const KT& key = itr->mKeyValuePair.first;
			const VT& value = itr->mKeyValuePair.second;
			insertNode(key, value);
		}

		return *this;
	}

	bool empty() const
	{
		return mUsed == 0;
	}

	size_t size() const
	{
		return mUsed;
	}

	size_t count(const KT& key) const
	{
		return 1;
	}

	void clear()
	{
		while (mHashItrList != NULL)
		{
			Node* oldnode = mHashItrList;
			mHashItrList = mHashItrList->mItrNext;
			delete oldnode;
		}

		mUsed = 0;
	}

	iterator begin()
	{
		return iterator(mHashItrList, this);
	}

	const_iterator begin() const
	{
		return const_iterator(mHashItrList, this);
	}

	iterator end()
	{
		return iterator(NULL, this);
	}

	const_iterator end() const
	{
		return const_iterator(NULL, this);
	}	

	iterator find(const KT& key)
	{
		const Node* node = findNode(key);
		if (node)
			return iterator(node, this);
	
		return end();
	}

	const_iterator find(const KT& key) const
	{
		const Node* node = findNode(key);
		if (node)
			return const_iterator(node, this);
	
		return end();
	}

	VT& operator[](const KT& key)
	{
		Node* node = findNode(key);
		if (!node)
			node = insertNode(key, VT());	// no match so insert new pair

		return node->mKeyValuePair.second;
	}

	std::pair<iterator, bool> insert(const HashPair& hp)
	{
		const KT& key = hp.first;
		const VT& value = hp.second;
		bool inserted = false;

		Node* node = findNode(key);
		if (!node)
		{
			node = insertNode(key, value);
			inserted = true;
		}

		iterator it = iterator(node, this);
		return std::pair<iterator, bool>(it, inserted);
	}

	template <typename Inputiterator>
	void insert(Inputiterator it1, Inputiterator it2)
	{
		while (it1 != it2)
		{
			const KT& key = it1->first;
			const VT& value = it1->second;
			if (!findNode(key))
				insertNode(key, value);
			++it1;
		}
	}

	void erase(iterator it)
	{
		eraseNode(it.mNode);
	}

	size_t erase(const KT& key)
	{
		Node* node = findNode(key);
		if (!node)
			return 0;

		eraseNode(node);
		return 1;
	}

	void erase(iterator it1, iterator it2)
	{
		while (it1 != it2)
		{
			erase(it1);
			++it1;
		}
	}

private:
	inline size_t hashKey(const KT& key) const
	{
		return mHashFunc(key) % mSize;
	}

	void resize(size_t newsize)
	{
		if (newsize == 0)		// don't allow 0 size tables!
			newsize = 1;

		mSize = newsize;
		mUsed = 0;

		delete [] mHashTable;
		mHashTable = new Node*[mSize];
		for (int i = 0; i < mSize; i++)
			mHashTable[i] = NULL;

		Node* itr = mHashItrList;
		mHashItrList = NULL;

		while (itr)
		{
			const KT& key = itr->mKeyValuePair.first;
			const VT& value = itr->mKeyValuePair.second;
			insertNode(key, value); 	// rehash the node from the old iteration list

			Node* olditr = itr;
			itr = itr->mItrNext;
			delete olditr;		// free the node from the old iteration list
		}
	}

	Node* findNode(const KT& key) const
	{
		Node* node = mHashTable[hashKey(key)];
		while (node)
		{
			if (node->mKeyValuePair.first == key)
				return node;
			node = node->mChainNext;
		}

		// node not found
		return NULL;
	}

	Node* insertNode(const KT& key, const VT& value)
	{
		// double the capacity if we're going to exceed the number of buckets
		if (mUsed + 1 > mSize)
			resize(2 * mSize + 1);

		// create new node
		Node* newnode = new Node;
		newnode->mKeyValuePair.first = key;
		newnode->mKeyValuePair.second = value;

		// insert at head of chain for the bucket
		size_t index = hashKey(key);
		newnode->mChainPrev = NULL;
		newnode->mChainNext = mHashTable[index];
		if (newnode->mChainNext)
			newnode->mChainNext->mChainPrev = newnode;
		mHashTable[index] = newnode;

		// insert at head of iteration list
		if (mHashItrList)
			mHashItrList->mItrPrev = newnode;
		newnode->mItrPrev = NULL;
		newnode->mItrNext = mHashItrList;
		mHashItrList = newnode;

		mUsed++;
		return newnode;
	}

	void eraseNode(Node* node)
	{
		size_t index = hashKey(node->mKeyValuePair.first);

		// remove from this bucket
		if (node->mChainPrev)
			node->mChainPrev->mChainNext = node->mChainNext;
		if (node->mChainNext)
			node->mChainNext->mChainPrev = node->mChainPrev;

		// removing head of linked list
		if (mHashTable[index] == node)
			mHashTable[index] = node->mChainNext;

		// remove from the iteration list
		if (node->mItrPrev)
			node->mItrPrev->mItrNext = node->mItrNext;
		else
			mHashItrList = node->mItrNext;

		if (node->mItrNext)
			node->mItrNext->mItrPrev = node->mItrPrev;
	   
		delete node;
		mUsed--;
	}

	size_t			mSize;
	size_t			mUsed;
	Node**			mHashTable;		// table of linked-lists (chaining hashtable)
	Node*			mHashItrList;	// linked-list for fast iteration
	HF				mHashFunc;		// hash key generation functor
};

#endif	// __HASHTABLE_H__

