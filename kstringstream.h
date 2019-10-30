/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

/// @file kstringstream.h
/// provides a stream around a KString

#include <iostream>
#include "bits/kcppcompat.h"
#include "kstreambuf.h"
#include "kstring.h"
#include "koutstringstream.h"
#include "kstream.h"

namespace dekaf2 {

namespace detail {

//-----------------------------------------------------------------------------
/// this is the custom KString reader
std::streamsize KIOStringReader(void* sBuffer, std::streamsize iCount, void* sSourceBuf);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This stream class reads and writes from / to KString
class KIOStringStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type = std::iostream;

	//-----------------------------------------------------------------------------
	KIOStringStream()
	//-----------------------------------------------------------------------------
	: base_type(&m_KStreamBuf)
	{}

	//-----------------------------------------------------------------------------
	KIOStringStream(const KIOStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::istream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KIOStringStream(KIOStringStream&& other) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KIOStringStream(KString& sBuffer)
	//-----------------------------------------------------------------------------
	: base_type(&m_KStreamBuf)
	{
		open(sBuffer);
	}

	//-----------------------------------------------------------------------------
	KIOStringStream& operator=(KIOStringStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KIOStringStream& operator=(const KIOStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// open a string for reading
	void open(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		m_Buffer.sBuffer = &sBuffer;
		m_Buffer.iReadPos = 0;
	}

	//-----------------------------------------------------------------------------
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_Buffer.sBuffer != nullptr;
	}

	//-----------------------------------------------------------------------------
	/// get a const ref of the string
	const KString& str() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set string
	void str(KStringView sView);
	//-----------------------------------------------------------------------------

	struct Buffer
	{
		KString* sBuffer { nullptr };
		size_t iReadPos { 0 };
	};

//----------
protected:
//----------

	Buffer m_Buffer;

	KStreamBuf m_KStreamBuf { &detail::KIOStringReader, &detail::KStringWriter, &m_Buffer, &m_Buffer.sBuffer };

}; // KIOStringStream


/// String stream that reads and writes copy-free from and into a KString
using KStringStream    = KReaderWriter<KIOStringStream>;

} // end of namespace dekaf2
