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

#include "kstringview.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCharSet
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static constexpr KStringView ANY_ISO8859         = "ISO-8859"; /*-1...*/
	static constexpr KStringView DEFAULT_CHARSET     = "WINDOWS-1252";

}; // KCharSet

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KMIME
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	constexpr
	KMIME()
	//-----------------------------------------------------------------------------
	    : m_mime()
	{}

	//-----------------------------------------------------------------------------
	constexpr
	KMIME(KStringView sv)
	//-----------------------------------------------------------------------------
	    : m_mime(sv)
	{}

	//-----------------------------------------------------------------------------
	constexpr
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return m_mime;
	}

	static constexpr KStringView NONE                = "";
	static constexpr KStringView JSON_UTF8           = "application/json; charset=UTF-8";
	static constexpr KStringView HTML_UTF8           = "text/html; charset=UTF-8";
	static constexpr KStringView XML_UTF8            = "text/xml; charset=UTF-8";
	static constexpr KStringView SWF                 = "application/x-shockwave-flash";
	static constexpr KStringView WWW_FORM_URLENCODED = "application/x-www-form-urlencoded";
	static constexpr KStringView MULTIPART_FORM_DATA = "multipart/form-data";
	static constexpr KStringView TEXT_PLAIN          = "text/plain";
	static constexpr KStringView APPLICATION_BINARY  = "application/octet-stream";

//------
private:
//------

	KStringView m_mime{TEXT_PLAIN};

}; // KMIME

} // end of namespace dekaf2
