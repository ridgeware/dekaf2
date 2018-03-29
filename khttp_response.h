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
#include "khttpinputfilter.h"
#include "khttpoutputfilter.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPResponseHeaders : public KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	bool Parse(KInStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Serialize(KOutStream& Stream) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetStatus(uint16_t iCode, KStringView sMessage)
	//-----------------------------------------------------------------------------
	{
		StatusCode = iCode;
		StatusString = sMessage;
	}

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return StatusCode <= 299 && StatusCode >= 200;
	}

	//-----------------------------------------------------------------------------
	bool HasChunking() const;
	//-----------------------------------------------------------------------------

//------
public:
//------

	KString StatusString;
	uint16_t StatusCode;

}; // KHTTPResponseHeaders

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KOutHTTPResponse : public KHTTPResponseHeaders, public KHTTPOutputFilter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	bool Serialize(KOutStream& Stream);
	//-----------------------------------------------------------------------------

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KInHTTPResponse : public KHTTPResponseHeaders, public KHTTPInputFilter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	bool Parse(KInStream& Stream);
	//-----------------------------------------------------------------------------

};


} // end of namespace dekaf2
