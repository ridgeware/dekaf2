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
#include "kconnection.h"
#include "khttp_header.h"
#include "khttp_method.h"
#include "kuseragent.h"


namespace dekaf2 {

namespace detail {
namespace http {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCharSet
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static constexpr KStringView ANY_ISO8859         = "ISO-8859"; /*-1...*/
	static constexpr KStringView DEFAULT_CHARSET     = "WINDOWS-1252";

}; // end of namespace KCharSet

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

} // end of namespace http
} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTP
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using KMethod    = detail::http::KMethod;
	using KHeader    = detail::http::KHeader;
	using KUserAgent = detail::http::KUserAgent;
	using KMIME      = detail::http::KMIME;
	using KCharSet   = detail::http::KCharSet;

	enum class State
	{
		CONNECTED,
		RESOURCE_SET,
		HEADER_SET,
		REQUEST_SENT,
		HEADER_PARSED,
		CLOSED
	};

	//-----------------------------------------------------------------------------
	KHTTP(KConnection& stream, const KURL& url = KURL{}, KMethod method = KMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTP& Resource(const KURL& url, KMethod method = KMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTP& RequestHeader(KStringView svName, KStringView svValue);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Request(KStringView svPostData = KStringView{}, KStringView svMime = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Stream into outstream
	size_t Read(KOutStream& stream, size_t len = KString::npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Append to sBuffer
	size_t Read(KString& sBuffer, size_t len = KString::npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Read one line into sBuffer, including EOL
	bool ReadLine(KString& sBuffer);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_t size() const
	//-----------------------------------------------------------------------------
	{
		return m_iRemainingContentSize;
	}

	//-----------------------------------------------------------------------------
	State GetState() const
	//-----------------------------------------------------------------------------
	{
		return m_State;
	}

	//-----------------------------------------------------------------------------
	const KHeader& GetResponseHeader() const
	//-----------------------------------------------------------------------------
	{
		return m_ResponseHeader;
	}

	//-----------------------------------------------------------------------------
	KHeader& GetResponseHeader()
	//-----------------------------------------------------------------------------
	{
		return m_ResponseHeader;
	}

//------
protected:
//------

	//-----------------------------------------------------------------------------
	bool GetNextChunkSize();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void CheckForChunkEnd();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool ReadHeader();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_t Post(KStringView sv);
	//-----------------------------------------------------------------------------

//------
private:
//------

	KConnection& m_Stream;
	KMethod  m_Method;
	KHeader  m_ResponseHeader;
	size_t   m_iRemainingContentSize{0};
	State    m_State{State::CLOSED};
	bool     m_bTEChunked;

}; // KHTTP


} // end of namespace dekaf2
