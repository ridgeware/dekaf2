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

/// @file KOStringStream.h
/// provides kstrings that can be constructed from strings (multiple kinds) passed in

#include <ostream>
#include <iostream>
#include <sys/types.h>

#include "kstreambuf.h"
#include "kwriter.h"
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
	KOStringStream() {}
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
	//-----------------------------------------------------------------------------
	{
		m_sBuf = &str;
	}

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
	/// get the constructed kstring
	KString& str()
	//-----------------------------------------------------------------------------
	{
		return *m_sBuf;
	}

	//-----------------------------------------------------------------------------
	/// set kstring
	bool str(KString& newBuffer)
	//-----------------------------------------------------------------------------
	{
		*m_sBuf = newBuffer;
		return (newBuffer == *m_sBuf);
	}

//----------
protected:
//----------

	KString* m_sBuf{nullptr};

	KOutStreamBuf m_KOStreamBuf{&KStringWriter, &m_sBuf};
};

/// KString writer based on KOStringStream>
using OKStringStream = KWriter<KOStringStream>;

} // end of namespace dekaf2
