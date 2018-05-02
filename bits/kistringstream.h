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

/// @file kistringstream.h
/// provides an input stream that can be constructed from KStringViews

#include <istream>
#include "kcppcompat.h"
#include "../kstreambuf.h"
#include "../kstringview.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This input stream class reads from a KStringView
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

} // end of namespace dekaf2
