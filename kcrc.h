/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kcrc.h
/// CRC computations

#include <boost/crc.hpp>
#include "kstream.h"
#include "kstringview.h"
#include "kstring.h"


namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KCRC32
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KCRC32() = default;

	/// ctor with a string
	KCRC32(KStringView sInput)
	{
		Update(sInput);
	}

	/// ctor with a stream
	KCRC32(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// appends a string to the CRC
	bool Update(KStringView sInput)
	{
		m_crc.process_bytes(sInput.data(), sInput.size());
		return true;
	}

	/// appends a stream to the CRC
	bool Update(KInStream& InputStream);

	/// appends a string to the CRC
	KCRC32& operator+=(KStringView sInput)
	{
		Update(sInput);
		return *this;
	}

	/// returns the CRC as integer
	uint32_t CRC() const
	{
		return m_crc.checksum();
	}

	/// appends a string to the CRC
	void operator()(KStringView sInput)
	{
		Update(sInput);
	}

	/// appends a stream to the CRC
	void operator()(KInStream& InputStream)
	{
		Update(InputStream);
	}

	/// returns the CRC as integer
	uint32_t operator()() const
	{
		return CRC();
	}

	/// clears the CRC and prepares for new computation
	void clear()
	{
		m_crc.reset();
	}

//------
protected:
//------

	boost::crc_32_type m_crc;

}; // KCRC32

} // end of namespace dekaf2

