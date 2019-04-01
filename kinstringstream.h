/*
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
/// provides an input stream that reads from KStringViews

#include <istream>
#include "bits/kcppcompat.h"
#include "kstreambuf.h"
#include "kstringview.h"
#include "kreader.h"

namespace dekaf2 {

namespace detail {

//-----------------------------------------------------------------------------
/// this is the custom KString reader
std::streamsize KStringReader(void* sBuffer, std::streamsize iCount, void* sTargetBuf);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This input stream class reads from a KStringView
class KIStringStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type = std::istream;

	//-----------------------------------------------------------------------------
	KIStringStream()
	//-----------------------------------------------------------------------------
	: base_type(&m_KIStreamBuf)
	{}

	//-----------------------------------------------------------------------------
	KIStringStream(const KIStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KIStringStream(KIStringStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KIStringStream(KStringView sView)
	//-----------------------------------------------------------------------------
	: base_type(&m_KIStreamBuf)
	{
		open(sView);
	}

	//-----------------------------------------------------------------------------
	KIStringStream& operator=(KIStringStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KIStringStream& operator=(const KIStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// open a string for reading
	void open(KStringView sView)
	//-----------------------------------------------------------------------------
	{
		m_sView = sView;
	}

	//-----------------------------------------------------------------------------
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return true;
	}

	//-----------------------------------------------------------------------------
	/// get a copy of the string
	KStringView str()
	//-----------------------------------------------------------------------------
	{
		return m_sView;
	}

	//-----------------------------------------------------------------------------
	/// set string
	void str(KStringView newView)
	//-----------------------------------------------------------------------------
	{
		open(newView);
	}

//----------
protected:
//----------

	KStringView m_sView;

	KInStreamBuf m_KIStreamBuf { &detail::KStringReader, &m_sView };

}; // KIStringStream

/// String stream that reads copy-free from a KStringView / KString
using KInStringStream  = KReader<KIStringStream>;

} // end of namespace dekaf2
