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

/// @file koutstringstream.h
/// provides an output stream that writes into a KString

#include "kdefinitions.h"
#include "kstreambuf.h"
#include "kstring.h"
#include "kwriter.h"
#include <ostream>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
/// this is the custom KString writer
std::streamsize DEKAF2_PUBLIC KStringWriter(const void* sBuffer, std::streamsize iCount, void* sTargetBuf);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This output stream class stores into a KString
class DEKAF2_PUBLIC KOStringStream : public std::ostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
protected:
//----------

	using base_type = std::ostream;

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

	//-----------------------------------------------------------------------------
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::ostream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KOStringStream(KOStringStream&& other) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KOStringStream(KStringRef& str)
	//-----------------------------------------------------------------------------
	: base_type(&m_KOStreamBuf)
	{
		open(str);
	}

	//-----------------------------------------------------------------------------
	KOStringStream& operator=(KOStringStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KOStringStream& operator=(const KOStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set output string
	void open(KStringRef& str)
	//-----------------------------------------------------------------------------
	{
		m_sBuffer = &str;
	}

	//-----------------------------------------------------------------------------
	/// test if we can write
	DEKAF2_NODISCARD
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_sBuffer != nullptr;
	}

	//-----------------------------------------------------------------------------
	/// gets a const ref of the string
	DEKAF2_NODISCARD
	const KStringRef& str() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets the string
	void str(KString sString);
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	KStringRef* m_sBuffer { nullptr };

	KOutStreamBuf m_KOStreamBuf { &detail::KStringWriter, &m_sBuffer };

}; // KOStringStream

/// String stream that writes copy-free into a KString
using KOutStringStream = KWriter<KOStringStream>;

DEKAF2_NAMESPACE_END
