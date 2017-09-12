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

/// @file OKStringStream.h
/// provides kstrings that can be constructed from strings (multiple kinds) passed in

//#include <cinttypes>
//#include <streambuf>
//#include <ostream>
//#include <unistd.h>
#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>


#include "kwriter.h"


namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Unbuffered std::ostream that is constructed around a unix file descriptor.
/// Mainly to allow its usage with pipes, for general file I/O use std::ofstream.
/// This one is really slow on small writes to files, on purpose, because pipes
/// should not be buffered. Therefore do _not_ use it for ordinary file I/O.
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
	//KOStringStream() {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KOStringStream(const KOStringStream&) = delete;
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
	//-----------------------------------------------------------------------------
	KOStringStream(KOStringStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// this class can be started from KString preferably
	KOStringStream(KString& str)
	    : m_sBuf(str)
	//-----------------------------------------------------------------------------
	{}

	//-----------------------------------------------------------------------------
	virtual ~KOStringStream();
	//-----------------------------------------------------------------------------

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
	//-----------------------------------------------------------------------------
	KOStringStream& operator=(KOStringStream&& other);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KOStringStream& operator=(const KOStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this "restarts" the buffer, like a call to the constructor
	bool open(KString& str)
	//-----------------------------------------------------------------------------
	{
		clear();
		addMore(str);
	}

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: construct a formatted KString
	bool addMore(KString& str);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the main purpose of this class: construct a formatted KString
	template<class... Args>
	bool addFormatted(Args&&... args);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// test if a there is data stored in constructed KString
	inline bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_sBuf.get().empty();
	}

	//-----------------------------------------------------------------------------
	/// test if a there is data stored
	inline void clear()
	//-----------------------------------------------------------------------------
	{
		return m_sBuf.get().clear();
	}

	//-----------------------------------------------------------------------------
	/// returns current size of constructed string
	inline unsigned long size() const
	//-----------------------------------------------------------------------------
	{
		return m_sBuf.get().size();
	}

	//-----------------------------------------------------------------------------
	/// get the constructed string
	KString& GetConstructedKString()
	//-----------------------------------------------------------------------------
	{
		return m_sBuf;
	}

//----------
protected:
//----------

	typedef std::reference_wrapper<KString> KStringRef;
	KStringRef m_sBuf;

	KOutStreamBuf m_KOStreamBuf{&KStringWriter, &m_sBuf};

};

/// FOR PIPES AND SPECIAL DEVICES ONLY! File descriptor writer based on KOStringStream>
using OKStringStream = KWriter<KOStringStream>;

} // end of namespace dekaf2
