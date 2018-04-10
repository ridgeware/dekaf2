/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
*/

#pragma once

/// @file KStringStream.h
/// provides streams that can be constructed from KStrings

#include <istream>
#include <ostream>
#include <iostream>
#include <sys/types.h>

#include "kstreambuf.h"
#include "kstream.h"
#include "kformat.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This output stream class stores into a KString which can be retrieved.
class KOStringStream : public std::ostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
protected:
//----------

	using base_type = std::ostream;

	//-----------------------------------------------------------------------------
	/// this is the custom KString writer
	static std::streamsize KStringWriter(const void* sBuffer, std::streamsize iCount, void* sTargetBuf);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KOStringStream()
	//-----------------------------------------------------------------------------
	: base_type(&m_KOStreamBuf)
	{}

	//-----------------------------------------------------------------------------
	KOStringStream(const KOStringStream&) = delete;
	//-----------------------------------------------------------------------------

#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
	//-----------------------------------------------------------------------------
	KOStringStream(KOStringStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KOStringStream(KString& str)
	//-----------------------------------------------------------------------------
	: base_type(&m_KOStreamBuf)
	{
		m_sBuf = &str;
	}

	//-----------------------------------------------------------------------------
	virtual ~KOStringStream();
	//-----------------------------------------------------------------------------

#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
	//-----------------------------------------------------------------------------
	KOStringStream& operator=(KOStringStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KOStringStream& operator=(const KOStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this "restarts" the buffer, like a call to the constructor
	bool open(KString& str);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a there is data stored in constructed KString
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_sBuf != nullptr;
	}

	//-----------------------------------------------------------------------------
	/// get the constructed KString
	KString& str()
	//-----------------------------------------------------------------------------
	{
		return *m_sBuf;
	}

	//-----------------------------------------------------------------------------
	/// set KString
	bool str(KString& newBuffer)
	//-----------------------------------------------------------------------------
	{
		*m_sBuf = newBuffer;
		return true;
	}

//----------
protected:
//----------

	KString* m_sBuf{nullptr};

	KOutStreamBuf m_KOStreamBuf{&KStringWriter, &m_sBuf};

}; // KOStringStream

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This output stream class stores into a KString which can be retrieved.
class KIStringStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
protected:
//----------

	using base_type = std::istream;

	//-----------------------------------------------------------------------------
	/// this is the custom KString reader
	static std::streamsize KStringReader(void* sBuffer, std::streamsize iCount, void* sTargetBuf);
	//-----------------------------------------------------------------------------

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KIStringStream()
	//-----------------------------------------------------------------------------
	: base_type(&m_KIStreamBuf)
	{}

	//-----------------------------------------------------------------------------
	KIStringStream(const KIStringStream&) = delete;
	//-----------------------------------------------------------------------------

#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
	//-----------------------------------------------------------------------------
	KIStringStream(KIStringStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KIStringStream(KStringView sView)
	//-----------------------------------------------------------------------------
	: base_type(&m_KIStreamBuf)
	{
		open(sView);
	}

	//-----------------------------------------------------------------------------
	virtual ~KIStringStream();
	//-----------------------------------------------------------------------------

#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
	//-----------------------------------------------------------------------------
	KIStringStream& operator=(KIStringStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KIStringStream& operator=(const KIStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this "restarts" the buffer, like a call to the constructor
	bool open(KStringView sView)
	//-----------------------------------------------------------------------------
	{
		m_sView = sView;
		return true;
	}

	//-----------------------------------------------------------------------------
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return !m_sView.empty();
	}

	//-----------------------------------------------------------------------------
	/// get the KStringView
	KStringView str()
	//-----------------------------------------------------------------------------
	{
		return m_sView;
	}

	//-----------------------------------------------------------------------------
	/// set KString
	bool str(KStringView newView)
	//-----------------------------------------------------------------------------
	{
		return open(newView);
	}

//----------
protected:
//----------

	KStringView m_sView;

	KInStreamBuf m_KIStreamBuf{&KStringReader, &m_sView};

}; // KIStringStream


/// String streams based on KString
using KInStringStream   = KReader<KIStringStream>;
using KOutStringStream  = KWriter<KOStringStream>;
// TODO make this a KString
using KStringStream     = KReaderWriter<std::stringstream>;

} // end of namespace dekaf2
