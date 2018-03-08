/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
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
 */

#pragma once

#include "kstringview.h"
#include "khttp_header.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPResponse : public KHTTPHeader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	bool Parse(KInStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Serialize(KOutStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const uint16_t& Status() const
	//-----------------------------------------------------------------------------
	{
		return m_iStatus;
	}

	//-----------------------------------------------------------------------------
	uint16_t& Status()
	//-----------------------------------------------------------------------------
	{
		return m_iStatus;
	}

	//-----------------------------------------------------------------------------
	const KString& HTTPVersion() const
	//-----------------------------------------------------------------------------
	{
		return m_HTTPVersion;
	}

	//-----------------------------------------------------------------------------
	KString& HTTPVersion()
	//-----------------------------------------------------------------------------
	{
		return m_HTTPVersion;
	}

//------
protected:
//------

//------
private:
//------

	KString m_HTTPVersion;
	uint16_t m_iStatus;

}; // KHTTPResponse

} // end of namespace dekaf2
