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

/// @file kinstringstream.h
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
std::streamsize KStringReader(void* sBuffer, std::streamsize iCount, void* sSourceBuf);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KInStringStreamBuf : public std::streambuf
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
 //----------

	//-----------------------------------------------------------------------------
	KInStringStreamBuf(KStringView sView = KStringView{})
	//-----------------------------------------------------------------------------
	{
		open(sView);
	}

	//-----------------------------------------------------------------------------
	bool open(KStringView sView);
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	virtual int_type underflow() override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual std::streamsize xsgetn(char_type* s, std::streamsize n) override;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual pos_type seekoff(off_type off,
							 std::ios_base::seekdir dir,
							 std::ios_base::openmode which = std::ios_base::in | std::ios_base::out ) override;
	//-----------------------------------------------------------------------------

}; // KInStringStreamBuf

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This input stream class reads from a KStringView. It is about 10% slower than the non-seekable
/// KIStringStream ..
class KSeekableIStringStream : public std::istream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using base_type = std::istream;

	//-----------------------------------------------------------------------------
	KSeekableIStringStream()
	//-----------------------------------------------------------------------------
	: base_type(&m_KIStreamBuf)
	{}

	//-----------------------------------------------------------------------------
	KSeekableIStringStream(const KSeekableIStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KSeekableIStringStream(KSeekableIStringStream&& other) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KSeekableIStringStream(KStringView sView)
	//-----------------------------------------------------------------------------
	: base_type(&m_KIStreamBuf)
	, m_KIStreamBuf(sView)
	{
	}

	//-----------------------------------------------------------------------------
	KSeekableIStringStream& operator=(KSeekableIStringStream&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KSeekableIStringStream& operator=(const KSeekableIStringStream&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// open a string for reading
	void open(KStringView sView)
	//-----------------------------------------------------------------------------
	{
		m_KIStreamBuf.open(sView);
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
		return {};
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

	KInStringStreamBuf m_KIStreamBuf;

}; // KSeekableIStringStream


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This input stream class reads from a KStringView. It is about 10% faster than the seekable
/// KSeekableIStringStream
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
	// we cannot simply use a defaulted move constructor here, as the ones in the
	// parent classes std::istream and std::basic_ios are protected (they do not
	// move the streambuffer, as they would not know how to do that properly for
	// specialized classes)
	KIStringStream(KIStringStream&& other) noexcept
	//-----------------------------------------------------------------------------
	: KIStringStream(other.m_sView)
	{
	}

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

/// Seekable string stream that reads copy-free from a KStringView / KString
using KSeekableInStringStream  = KReader<KSeekableIStringStream>;

} // end of namespace dekaf2
