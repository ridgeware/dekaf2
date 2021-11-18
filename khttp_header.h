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

#include "bits/kcppcompat.h"
#include "kstringview.h"
#include "kstring.h"
#include "kcasestring.h"
#include "kprops.h"
#include "kcrashexit.h"
#include "kformat.h"
#include <memory>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTTPHeader
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
		X_POWERED_BY,
		OTHER
	};

	static_assert(static_cast<uint16_t>(OTHER) < 256, "more than 256 Header enum values, modify Hash() calculations");

	//-----------------------------------------------------------------------------
	constexpr
	KHTTPHeader() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
#ifndef DEKAF2_IS_WINDOWS
	DEKAF2_CONSTEXPR_14
#endif
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
	KHTTPHeader(KHTTPHeader&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_20
	KHTTPHeader& operator=(KHTTPHeader other) noexcept
	//-----------------------------------------------------------------------------
	{
		swap(*this, other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_20
	friend void swap(KHTTPHeader& left, KHTTPHeader& right) noexcept
	//-----------------------------------------------------------------------------
	{
		using std::swap;
		swap(left.m_header,  right.m_header);
		swap(left.m_sHeader, right.m_sHeader);
	}

	//-----------------------------------------------------------------------------
	constexpr
	KHTTPHeader(Header header) noexcept
	//-----------------------------------------------------------------------------
	: m_header(header)
	{}

	//-----------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	DEKAF2_CONSTEXPR_14
	KHTTPHeader(T&& sHeader)
	//-----------------------------------------------------------------------------
	: m_header(Parse(sHeader))
	{
		if (m_header == OTHER)
		{
			m_sHeader = std::make_unique<KString>(std::forward<T>(sHeader));
		}
	}

	//-------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<std::is_convertible<const T&, KStringView>::value == true, int>::type = 0>
	DEKAF2_CONSTEXPR_14
	KHTTPHeader& operator=(T&& sv)
	//-------------------------------------------------------------------------
	{
		*this = KHTTPHeader(std::forward<T>(sv));
		return *this;
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	KStringViewZ Serialize() const
	//-----------------------------------------------------------------------------
	{
		switch (m_header)
		{
			case NONE:
				return "";
			case OTHER:
				kAssert(m_sHeader.get(), "m_sHeader must not be nullptr");
				return *m_sHeader ? m_sHeader->ToView() : "";
			case ACCEPT:
				return "Accept";
			case ACCEPT_CHARSET:
				return "Accept-Charset";
			case ACCEPT_DATETIME:
				return "Accept-Datetime";
			case ACCEPT_ENCODING:
				return "Accept-Encoding";
			case ACCEPT_LANGUAGE:
				return "Accept-Language";
			case ACCEPT_PATCH:
				return "Accept-Patch";
			case ACCEPT_RANGES:
				return "Accept-Ranges";
			case ACCESS_CONTROL_ALLOW_CREDENTIALS:
				return "Access-Control-Allow-Credentials";
			case ACCESS_CONTROL_ALLOW_HEADERS:
				return "Access-Control-Allow-Headers";
			case ACCESS_CONTROL_ALLOW_METHODS:
				return "Access-Control-Allow-Methods";
			case ACCESS_CONTROL_ALLOW_ORIGIN:
				return "Access-Control-Allow-Origin";
			case ACCESS_CONTROL_EXPOSE_HEADERS:
				return "Access-Control-Expose-Headers";
			case ACCESS_CONTROL_MAX_AGE:
				return "Access-Control-Max-Age";
			case ACCESS_CONTROL_REQUEST_HEADERS:
				return "Access-Control-Request-Headers";
			case ACCESS_CONTROL_REQUEST_METHOD:
				return "Access-Control-Request-Method";
			case AGE:
				return "Age";
			case ALLOW:
				return "Allow";
			case ALT_SVC:
				return "Alt-Svc";
			case AUTHORIZATION:
				return "Authorization";
			case CACHE_CONTROL:
				return "Cache-Control";
			case CONNECTION:
				return "Connection";
			case CONTENT_DISPOSITION:
				return "Content-Disposition";
			case CONTENT_ENCODING:
				return "Content-Encoding";
			case CONTENT_LANGUAGE:
				return "Content-Language";
			case CONTENT_LENGTH:
				return "Content-Length";
			case CONTENT_LOCATION:
				return "Content-Location";
			case CONTENT_MD5:
				return "Content-MD5";
			case CONTENT_RANGE:
				return "Content-Range";
			case CONTENT_TYPE:
				return "Content-Type";
			case COOKIE:
				return "Cookie";
			case DATE:
				return "Date";
			case ETAG:
				return "ETag";
			case EXPECT:
				return "Expect";
			case EXPIRES:
				return "Expires";
			case FORWARDED:
				return "Forwarded";
			case FROM:
				return "From";
			case HOST:
				return "Host";
			case HOST_OVERRIDE:
				return "HostOverride";
			case IF_MATCH:
				return "If-Match";
			case IF_MODIFIED_SINCE:
				return "If-Modified-Since";
			case IF_NONE_MATCH:
				return "If-None-Match";
			case IF_RANGE:
				return "If-Range";
			case IF_UNMODIFIED_SINCE:
				return "If-Unmodified-Since";
			case KEEP_ALIVE:
				return "Keep-Alive";
			case LAST_MODIFIED:
				return "Last-Modified";
			case LINK:
				return "Link";
			case LOCATION:
				return "Location";
			case MAX_FORWARDS:
				return "Max-Forwards";
			case ORIGIN:
				return "Origin";
			case P3P:
				return "P3P";
			case PRAGMA:
				return "Pragma";
			case PROXY_AUTHENTICATE:
				return "Proxy-Authenticate";
			case PROXY_AUTHORIZATION:
				return "Proxy-Authorization";
			case PROXY_CONNECTION:
				return "Proxy-Connection";
			case PUBLIC_KEY_PINS:
				return "Public-Key-Pins";
			case RANGE:
				return "Range";
			case REFERER:
				return "Referer";
			case RETRY_AFTER:
				return "Retry-After";
			case SERVER:
				return "Server";
			case SET_COOKIE:
				return "Set-Cookie";
			case STRICT_TRANSPORT_SECURITY:
				return "Strict-Transport-Security";
			case TRAILER:
				return "Trailer";
			case TRANSFER_ENCODING:
				return "Transfer-Encoding";
			case UPGRADE:
				return "Upgrade";
			case USER_AGENT:
				return "User-Agent";
			case VARY:
				return "Vary";
			case VIA:
				return "Via";
			case WARNING:
				return "Warning";
			case WWW_AUTHENTICATE:
				return "WWW-Authenticate";
			case X_FORWARDED_FOR:
				return "X-Forwarded-For";
			case X_FORWARDED_HOST:
				return "X-Forwarded-Host";
			case X_FORWARDED_PROTO:
				return "X-Forwarded-Proto";
			case X_FORWARDED_SERVER:
				return "X-Forwarded-Server";
			case X_FRAME_OPTIONS:
				return "X-Frame-Options";
			case X_POWERED_BY:
				return "X-Powered-By";
		}

		// not found..
#if !defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION >= 90000
		// gcc 8.3.1 complains that kDebug is not constexpr..
		kDebug(0, "missing switch case: {}", m_header);
#endif
		return "";
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_header == NONE;
	}

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_header = NONE;
		m_sHeader.reset();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	static Header Parse(KStringView sHeader)
	//-----------------------------------------------------------------------------
	{
		switch (sHeader.CaseHash())
		{
			case ""_casehash:
				return NONE;
			case "Accept"_casehash:
				return ACCEPT;
			case "Accept-Charset"_casehash:
				return ACCEPT_CHARSET;
			case "Accept-Datetime"_casehash:
				return ACCEPT_DATETIME;
			case "Accept-Encoding"_casehash:
				return ACCEPT_ENCODING;
			case "Accept-Language"_casehash:
				return ACCEPT_LANGUAGE;
			case "Accept-Patch"_casehash:
				return ACCEPT_PATCH;
			case "Accept-Ranges"_casehash:
				return ACCEPT_RANGES;
			case "Access-Control-Allow-Credentials"_casehash:
				return ACCESS_CONTROL_ALLOW_CREDENTIALS;
			case "Access-Control-Allow-Headers"_casehash:
				return ACCESS_CONTROL_ALLOW_HEADERS;
			case "Access-Control-Allow-Methods"_casehash:
				return ACCESS_CONTROL_ALLOW_METHODS;
			case "Access-Control-Allow-Origin"_casehash:
				return ACCESS_CONTROL_ALLOW_ORIGIN;
			case "Access-Control-Expose-Headers"_casehash:
				return ACCESS_CONTROL_EXPOSE_HEADERS;
			case "Access-Control-Max-Age"_casehash:
				return ACCESS_CONTROL_MAX_AGE;
			case "Access-Control-Request-Headers"_casehash:
				return ACCESS_CONTROL_REQUEST_HEADERS;
			case "Access-Control-Request-Method"_casehash:
				return ACCESS_CONTROL_REQUEST_METHOD;
			case "Age"_casehash:
				return AGE;
			case "Allow"_casehash:
				return ALLOW;
			case "Alt-Svc"_casehash:
				return ALT_SVC;
			case "Authorization"_casehash:
				return AUTHORIZATION;
			case "Cache-Control"_casehash:
				return CACHE_CONTROL;
			case "Connection"_casehash:
				return CONNECTION;
			case "Content-Disposition"_casehash:
				return CONTENT_DISPOSITION;
			case "Content-Encoding"_casehash:
				return CONTENT_ENCODING;
			case "Content-Language"_casehash:
				return CONTENT_LANGUAGE;
			case "Content-Length"_casehash:
				return CONTENT_LENGTH;
			case "Content-Location"_casehash:
				return CONTENT_LOCATION;
			case "Content-MD5"_casehash:
				return CONTENT_MD5;
			case "Content-Range"_casehash:
				return CONTENT_RANGE;
			case "Content-Type"_casehash:
				return CONTENT_TYPE;
			case "Cookie"_casehash:
				return COOKIE;
			case "Date"_casehash:
				return DATE;
			case "ETag"_casehash:
				return ETAG;
			case "Expect"_casehash:
				return EXPECT;
			case "Expires"_casehash:
				return EXPIRES;
			case "Forwarded"_casehash:
				return FORWARDED;
			case "From"_casehash:
				return FROM;
			case "Host"_casehash:
				return HOST;
			case "HostOverride"_casehash:
				return HOST_OVERRIDE;
			case "If-Match"_casehash:
				return IF_MATCH;
			case "If-Modified-Since"_casehash:
				return IF_MODIFIED_SINCE;
			case "If-None-Match"_casehash:
				return IF_NONE_MATCH;
			case "If-Range"_casehash:
				return IF_RANGE;
			case "If-Unmodified-Since"_casehash:
				return IF_UNMODIFIED_SINCE;
			case "Keep-Alive"_casehash:
				return KEEP_ALIVE;
			case "Last-Modified"_casehash:
				return LAST_MODIFIED;
			case "Link"_casehash:
				return LINK;
			case "Location"_casehash:
				return LOCATION;
			case "Max-Forwards"_casehash:
				return MAX_FORWARDS;
			case "Origin"_casehash:
				return ORIGIN;
			case "P3P"_casehash:
				return P3P;
			case "Pragma"_casehash:
				return PRAGMA;
			case "Proxy-Authenticate"_casehash:
				return PROXY_AUTHENTICATE;
			case "Proxy-Authorization"_casehash:
				return PROXY_AUTHORIZATION;
			case "Proxy-Connection"_casehash:
				return PROXY_CONNECTION;
			case "Public-Key-Pins"_casehash:
				return PUBLIC_KEY_PINS;
			case "Range"_casehash:
				return RANGE;
			case "Referer"_casehash:
				return REFERER;
			case "Retry-After"_casehash:
				return RETRY_AFTER;
			case "Server"_casehash:
				return SERVER;
			case "Set-Cookie"_casehash:
				return SET_COOKIE;
			case "Strict-Transport-Security"_casehash:
				return STRICT_TRANSPORT_SECURITY;
			case "Trailer"_casehash:
				return TRAILER;
			case "Transfer-Encoding"_casehash:
				return TRANSFER_ENCODING;
			case "Upgrade"_casehash:
				return UPGRADE;
			case "User-Agent"_casehash:
				return USER_AGENT;
			case "Vary"_casehash:
				return VARY;
			case "Via"_casehash:
				return VIA;
			case "Warning"_casehash:
				return WARNING;
			case "WWW-Authenticate"_casehash:
				return WWW_AUTHENTICATE;
			case "X-Forwarded-For"_casehash:
				return X_FORWARDED_FOR;
			case "X-Forwarded-Host"_casehash:
				return X_FORWARDED_HOST;
			case "X-Forwarded-Proto"_casehash:
				return X_FORWARDED_PROTO;
			case "X-Forwarded-Server"_casehash:
				return X_FORWARDED_SERVER;
			case "X-Frame-Options"_casehash:
				return X_FRAME_OPTIONS;
			case "X-Powered-By"_casehash:
				return X_POWERED_BY;
		}

		// not found..
		return OTHER;

	} // Parse

	//-----------------------------------------------------------------------------
	/// create a string with correct HTTP date formatting
	static KString DateToString(time_t tTime);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// read a date from a string with  HTTP date formatting
	static time_t StringToDate(KStringView sTime);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	std::size_t Hash() const
	//-----------------------------------------------------------------------------
	{
		if (m_header == OTHER && m_sHeader)
		{
			return kCalcCaseHash((*m_sHeader));
		}
		else
		{
			// make sure we spread across buckets..
			return kHash(static_cast<char>(m_header));
		}
	}

	//-----------------------------------------------------------------------------
#if !defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION >= 90000
	DEKAF2_CONSTEXPR_14
#else
	// gcc 8.3.1 insists that all paths through a constexpr function
	// have to be constexpr..
	DEKAF2_CONSTEXPR_20
#endif
	friend bool operator==(const KHTTPHeader& left, const KHTTPHeader& right)
	//-----------------------------------------------------------------------------
	{
		if (left.m_header != right.m_header)
		{
			return false;
		}

		if (left.m_header != KHTTPHeader::OTHER)
		{
			return true;
		}

		return (left.m_sHeader && right.m_sHeader &&
				kCaseEqual(*left.m_sHeader, *right.m_sHeader));
	}

//------
private:
//------

	Header m_header { NONE };
	std::unique_ptr<KString> m_sHeader;

}; // KHTTPHeader

//-----------------------------------------------------------------------------
#if !defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION >= 90000
DEKAF2_CONSTEXPR_14
#else
// gcc 8.3.1 insists that all paths through a constexpr function
// have to be constexpr..
DEKAF2_CONSTEXPR_20
#endif
DEKAF2_PUBLIC
bool operator!=(const KHTTPHeader& left, const KHTTPHeader& right)
//-----------------------------------------------------------------------------
{
	return !operator==(left, right);
}

} // end of namespace dekaf2

template <>
struct fmt::formatter<dekaf2::KHTTPHeader> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KHTTPHeader& Header, FormatContext& ctx)
	{
		return formatter<string_view>::format(Header.Serialize(), ctx);
	}
};

namespace std
{
	/// provide a std::hash for KHTTPHeader
	template<> struct hash<dekaf2::KHTTPHeader>
	{
		DEKAF2_CONSTEXPR_14
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
		DEKAF2_CONSTEXPR_14
		std::size_t operator()(const dekaf2::KHTTPHeader& header) const noexcept
		{
			return header.Hash();
		}
};

} // end of namespace boost

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static std::size_t constexpr MAX_LINELENGTH = 8 * 1024;

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
	bool HasContent(bool bForRequest) const;
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
	BasicAuthParms GetBasicAuthParms() const;
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
