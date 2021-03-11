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

#include "kstringview.h"
#include "kcasestring.h"
#include "kprops.h"
#include <fmt/format.h>
#include <memory>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPHeader
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum Header
	{
		NONE,
		ACCEPT,
		ACCEPT_CHARSET,
		ACCEPT_DATETIME,
		ACCEPT_ENCODING,
		ACCEPT_LANGUAGE,
		ACCEPT_PATCH,
		ACCEPT_RANGES,
		ACCESS_CONTROL_ALLOW_CREDENTIALS,
		ACCESS_CONTROL_ALLOW_HEADERS,
		ACCESS_CONTROL_ALLOW_METHODS,
		ACCESS_CONTROL_ALLOW_ORIGIN,
		ACCESS_CONTROL_EXPOSE_HEADERS,
		ACCESS_CONTROL_MAX_AGE,
		ACCESS_CONTROL_REQUEST_HEADERS,
		ACCESS_CONTROL_REQUEST_METHOD,
		AGE,
		ALLOW,
		ALT_SVC,
		AUTHORIZATION,
		CACHE_CONTROL,
		CONNECTION,
		CONTENT_DISPOSITION,
		CONTENT_ENCODING,
		CONTENT_LANGUAGE,
		CONTENT_LENGTH,
		CONTENT_LOCATION,
		CONTENT_MD5,
		CONTENT_RANGE,
		CONTENT_TYPE,
		COOKIE,
		DATE,
		ETAG,
		EXPECT,
		EXPIRES,
		FORWARDED,
		FROM,
		HOST,
		HOST_OVERRIDE,
		IF_MATCH,
		IF_MODIFIED_SINCE,
		IF_NONE_MATCH,
		IF_RANGE,
		IF_UNMODIFIED_SINCE,
		KEEP_ALIVE,
		LAST_MODIFIED,
		LINK,
		LOCATION,
		MAX_FORWARDS,
		ORIGIN,
		P3P,
		PRAGMA,
		PROXY_AUTHENTICATE,
		PROXY_AUTHORIZATION,
		PROXY_CONNECTION,
		PUBLIC_KEY_PINS,
		RANGE,
		REFERER,
		RETRY_AFTER,
		SERVER,
		SET_COOKIE,
		STRICT_TRANSPORT_SECURITY,
		TRAILER,
		TRANSFER_ENCODING,
		UPGRADE,
		USER_AGENT,
		VARY,
		VIA,
		WARNING,
		WWW_AUTHENTICATE,
		X_FORWARDED_FOR,
		X_FORWARDED_HOST,
		X_FORWARDED_PROTO,
		X_FORWARDED_SERVER,
		X_FRAME_OPTIONS,
		X_POWERED_BY
	};

	//-----------------------------------------------------------------------------
	KHTTPHeader(Header header = NONE)
	//-----------------------------------------------------------------------------
	: m_header(header)
	{}

	//-----------------------------------------------------------------------------
	KHTTPHeader(const KHTTPHeader& other)
	//-----------------------------------------------------------------------------
	: m_header(other.m_header)
	{
		if (other.m_sHeader)
		{
			m_sHeader = std::make_unique<KString>(*other.m_sHeader);
		}
	}

	//-----------------------------------------------------------------------------
	KHTTPHeader& operator=(const KHTTPHeader& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPHeader(KHTTPHeader&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPHeader& operator=(KHTTPHeader&& other) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPHeader(KStringView sHeader);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPHeader(const KString& sHeader)
	//-----------------------------------------------------------------------------
	: KHTTPHeader(sHeader.ToView())
	{}

	//-----------------------------------------------------------------------------
	KHTTPHeader(const char* sHeader)
	//-----------------------------------------------------------------------------
	: KHTTPHeader(KStringView(sHeader))
	{}

	//-----------------------------------------------------------------------------
	KStringView Serialize() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_header == NONE && !m_sHeader;
	}

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		*this = KHTTPHeader();
	}

	//-----------------------------------------------------------------------------
	static Header Parse(KStringView sHeader);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// create a string with correct HTTP date formatting
	static KString DateToString(time_t tTime);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// read a date from a string with  HTTP date formatting
	static time_t StringToDate(KStringView sTime);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::size_t Hash() const
	//-----------------------------------------------------------------------------
	{
		if (m_header == NONE && m_sHeader)
		{
			return kCalcCaseHash((*m_sHeader));
		}
		else
		{
			// make sure we spread across buckets..
			return kHash(static_cast<char>(m_header));
		}
	}

	friend bool operator==(const KHTTPHeader&, const KHTTPHeader&);

//------
private:
//------

	Header m_header;
	std::unique_ptr<KString> m_sHeader;

}; // KHTTPHeader

//-----------------------------------------------------------------------------
bool operator==(const KHTTPHeader& left, const KHTTPHeader& right);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
inline bool operator!=(const KHTTPHeader& left, const KHTTPHeader& right)
//-----------------------------------------------------------------------------
{
	return !operator==(left, right);
}

} // end of namespace dekaf2

namespace std
{
	/// provide a std::hash for KHTTPHeader
	template<> struct hash<dekaf2::KHTTPHeader>
	{
		std::size_t operator()(const dekaf2::KHTTPHeader& header) const noexcept
		{
			return header.Hash();
		}
	};

} // end of namespace std

#include <boost/functional/hash.hpp>

namespace boost
{
	/// provide a boost::hash for KHTTPHeader
#if (BOOST_VERSION < 106400)
	template<> struct hash<dekaf2::KHTTPHeader> : public std::unary_function<dekaf2::KHTTPHeader, std::size_t>
#else
	template<> struct hash<dekaf2::KHTTPHeader>
#endif
	{
		std::size_t operator()(const dekaf2::KHTTPHeader& header) const noexcept
		{
			return header.Hash();
		}
};

} // end of namespace boost

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum { MAX_LINELENGTH = 8 * 1024 };

	using KHeaderMap = KProps<KHTTPHeader, KString, /*Sequential =*/ true, /*Unique =*/ false>; // case insensitive map for header info

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
	const KString& ContentType() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const KString& Charset() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool HasKeepAlive() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns -1 for chunked content, 0 for no content, > 0 = size of content
	std::streamsize ContentLength() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool HasContent() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	struct BasicAuthParms
	{
		KString sUsername;
		KString sPassword;

		operator bool() const { return !sUsername.empty(); }
	};

	//-----------------------------------------------------------------------------
	/// decodes, if existing, a Basic Authentication header and returns the clear text
	/// username and password
	BasicAuthParms GetBasicAuthParms();
	//-----------------------------------------------------------------------------

//------
protected:
//------

	//-----------------------------------------------------------------------------
	bool SetError(KString sError) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SplitContentType() const;
	//-----------------------------------------------------------------------------

//------
public:
//------

	KHeaderMap Headers; // response headers read in

	// we store the http version here as it is a shared property
	// of request and response headers
	KString sHTTPVersion;

//------
private:
//------

	mutable KString m_sCharset;
	mutable KString m_sContentType;
	mutable KString m_sError;

}; // KHTTPHeaders

} // end of namespace dekaf2
