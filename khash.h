/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#pragma once

#include "bits/khash.h"
#include "kstringview.h"
#include "kreader.h"

/// @file khash.h
/// FNV hash class

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generates a FNV hash value, also over consecutive pieces of data
class DEKAF2_PUBLIC KHash
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KHash() = default;

	/// ctor with a char
	KHash(KStringView::value_type chInput)
	{
		Update(chInput);
	}

	/// ctor with a string
	KHash(KStringView sInput)
	{
		Update(sInput);
	}

	/// ctor with a stream
	KHash(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// appends a char to the hash
	bool Update(KStringView::value_type chInput);

	/// appends a string to the hash
	bool Update(KStringView sInput);

	/// appends a stream to the hash
	bool Update(KInStream& InputStream);

	/// appends a char to the hash
	KHash& operator+=(KStringView::value_type chInput)
	{
		Update(chInput);
		return *this;
	}

	/// appends a string to the hash
	KHash& operator+=(KStringView sInput)
	{
		Update(sInput);
		return *this;
	}

	/// appends a stream to the hash
	KHash& operator+=(KInStream& InputStream)
	{
		Update(InputStream);
		return *this;
	}

	/// returns the hash as integer (we want the hash value to be 0 if no input was added)
	std::size_t Hash() const
	{
		return m_bUpdated ? m_iHash : 0;
	}

	/// returns the hash as integer (we want the hash value to be 0 if no input was added)
	operator std::size_t() const
	{
		return Hash();
	}

	/// appends a string to the hash
	void operator()(KStringView::value_type chInput)
	{
		Update(chInput);
	}

	/// appends a string to the hash
	void operator()(KStringView sInput)
	{
		Update(sInput);
	}

	/// appends a stream to the hash
	void operator()(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// returns the hash as integer
	std::size_t operator()() const
	{
		return Hash();
	}

	/// clears the hash and prepares for new computation
	void clear()
	{
		*this = KHash();
	}

//------
protected:
//------

	std::size_t m_iHash { kHashBasis };
	bool m_bUpdated { false };

}; // KHash

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generates a ASCII case insensitive FNV hash value, also over consecutive pieces of data
class DEKAF2_PUBLIC KCaseHash
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KCaseHash() = default;

	/// ctor with a char
	KCaseHash(KStringView::value_type chInput)
	{
		Update(chInput);
	}

	/// ctor with a string
	KCaseHash(KStringView sInput)
	{
		Update(sInput);
	}

	/// ctor with a stream
	KCaseHash(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// appends a char to the hash
	bool Update(KStringView::value_type chInput);

	/// appends a string to the hash
	bool Update(KStringView sInput);

	/// appends a stream to the hash
	bool Update(KInStream& InputStream);

	/// appends a char to the hash
	KCaseHash& operator+=(KStringView::value_type chInput)
	{
		Update(chInput);
		return *this;
	}

	/// appends a string to the hash
	KCaseHash& operator+=(KStringView sInput)
	{
		Update(sInput);
		return *this;
	}

	/// appends a stream to the hash
	KCaseHash& operator+=(KInStream& InputStream)
	{
		Update(InputStream);
		return *this;
	}

	/// returns the hash as integer (we want the hash value to be 0 if no input was added)
	std::size_t Hash() const
	{
		return m_bUpdated ? m_iHash : 0;
	}

	/// returns the hash as integer (we want the hash value to be 0 if no input was added)
	operator std::size_t() const
	{
		return Hash();
	}

	/// appends a string to the hash
	void operator()(KStringView::value_type chInput)
	{
		Update(chInput);
	}

	/// appends a string to the hash
	void operator()(KStringView sInput)
	{
		Update(sInput);
	}

	/// appends a stream to the hash
	void operator()(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// returns the hash as integer
	std::size_t operator()() const
	{
		return Hash();
	}

	/// clears the hash and prepares for new computation
	void clear()
	{
		*this = KCaseHash();
	}

//------
protected:
//------

	std::size_t m_iHash { kHashBasis };
	bool m_bUpdated { false };

}; // KCaseHash

} // of namespace dekaf2
