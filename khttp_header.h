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

#include "kcompatibility.h"
#include "kstringview.h"
#include "kstring.h"
#include "kcaseless.h"
#include "kprops.h"
#include "kcrashexit.h"
#include "kformat.h"
#include <memory>

#if (!defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION >= 90000) \
	&& !defined(_MSC_VER) \
	&& defined(DEKAF2_HAS_CPP_14)
		#define DEKAF2_KHTTP_HEADER_CONSTEXPR_14 constexpr
#else
		#define DEKAF2_KHTTP_HEADER_CONSTEXPR_14 inline
#endif

DEKAF2_NAMESPACE_BEGIN

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
		CROSS_ORIGIN_EMBEDDER_POLICY,
		CROSS_ORIGIN_OPENER_POLICY,
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
		SEC_WEBSOCKET_ACCEPT,
		SEC_WEBSOCKET_KEY,
		SEC_WEBSOCKET_EXTENSIONS,
		SEC_WEBSOCKET_VERSION,
		SEC_WEBSOCKET_PROTOCOL,
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
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
	KHTTPHeader() = default;
	//-----------------------------------------------------------------------------

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
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
	KHTTPHeader(Header header) noexcept
	//-----------------------------------------------------------------------------
	: m_header(header)
	{}

	//-----------------------------------------------------------------------------
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
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
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value == true, int>::type = 0>
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
	KHTTPHeader& operator=(T&& sv)
	//-------------------------------------------------------------------------
	{
		*this = KHTTPHeader(std::forward<T>(sv));
		return *this;
	}

	//-----------------------------------------------------------------------------
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
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
			case CROSS_ORIGIN_EMBEDDER_POLICY:
				return "Cross-Origin-Embedder-Policy";
			case CROSS_ORIGIN_OPENER_POLICY:
				return "Cross-Origin-Opener-Policy";
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
			case SEC_WEBSOCKET_ACCEPT:
				return "Sec-WebSocket-Accept";
			case SEC_WEBSOCKET_KEY:
				return "Sec-WebSocket-Key";
			case SEC_WEBSOCKET_EXTENSIONS:
				return "Sec-WebSocket-Extensions";
			case SEC_WEBSOCKET_VERSION:
				return "Sec-WebSocket-Version";
			case SEC_WEBSOCKET_PROTOCOL:
				return "Sec-WebSocket-Protocol";
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
		kDebug(0, "missing switch case: {}", m_header);

		return "";
	}

	//-----------------------------------------------------------------------------
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
	operator KStringView() const
	//-----------------------------------------------------------------------------
	{
		return Serialize();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
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
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
	std::size_t Hash() const
	//-----------------------------------------------------------------------------
	{
		if (m_header == OTHER && m_sHeader)
		{
			return kCalcCaselessHash((*m_sHeader));
		}
		else
		{
			// make sure we spread across buckets..
			return kHash(static_cast<char>(m_header));
		}
	}

	//-----------------------------------------------------------------------------
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
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
				kCaselessEqual(*left.m_sHeader, *right.m_sHeader));
	}

	//-----------------------------------------------------------------------------
	DEKAF2_KHTTP_HEADER_CONSTEXPR_14
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
			case "Cross-Origin-Embedder-Policy"_casehash:
				return CROSS_ORIGIN_EMBEDDER_POLICY;
			case "Cross-Origin-Opener-Policy"_casehash:
				return CROSS_ORIGIN_OPENER_POLICY;
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
			case "Sec-WebSocket-Accept"_casehash:
				return SEC_WEBSOCKET_ACCEPT;
			case "Sec-WebSocket-Key"_casehash:
				return SEC_WEBSOCKET_KEY;
			case "Sec-WebSocket-Extensions"_casehash:
				return SEC_WEBSOCKET_EXTENSIONS;
			case "Sec-WebSocket-Version"_casehash:
				return SEC_WEBSOCKET_VERSION;
			case "Sec-WebSocket-Protocol"_casehash:
				return SEC_WEBSOCKET_PROTOCOL;
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
	static KString DateToString(KUnixTime tTime);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// read a date from a string with  HTTP date formatting
	static KUnixTime StringToDate(KStringView sTime);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return the quality value for a parameter, in percent
	/// @param sContent the input string, including the ";q=" start of a quality value (it will be searched for)
	/// @param bRemoveQuality if true, the quality value and all spaces around it are removed
	/// from the input string. Defaults to false.
	/// @return Quality in percent. Default is 100.
	template<class String = KStringView>
	static uint16_t GetQualityValue(String& sContent, bool bRemoveQuality = false)
	//-----------------------------------------------------------------------------
	{
		auto iQualPos = sContent.find(";q=");

		uint16_t iPercent { 100 };

		if (iQualPos != npos)
		{
			iPercent = CalcQualityValue(sContent, iQualPos+3);

			if (bRemoveQuality)
			{
				while (iQualPos > 0 && sContent[iQualPos-1] == ' ')
				{
					--iQualPos;
				}

				sContent.erase(iQualPos, String::npos);
			}
		}

		return iPercent;
	}

	//-----------------------------------------------------------------------------
	/// return the quality value for a string, iStartPos points to first character of a fixed number, like "0.81"
	/// @param sContent the input string, with or without the ";q=" start of a quality value
	/// @param iStartPos the first position of a fixed number like "0.81" that is returned as the quality percent
	static uint16_t CalcQualityValue(KStringView sContent, KStringView::size_type iStartPos = 0);
	//-----------------------------------------------------------------------------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// value type for a container of string/quality pairs
	template<class String = KStringView>
	struct QualVal
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		QualVal() = default;
		QualVal(String sValue, KStringView sQuality)
		: sValue(std::move(sValue))
		, iQuality(CalcQualityValue(sQuality))
		{
		}
		// the adaptor for kSplit
		QualVal(KStringViewPair Pair)
		: QualVal(Pair.first, Pair.second)
		{
		}

		operator String&()             { return sValue;   }
		operator const String&() const { return sValue;   }
		operator uint16_t()      const { return iQuality; }

		String   sValue;
		uint16_t iQuality;

	}; // QualVal

	//-----------------------------------------------------------------------------
	/// split a string in a (stable) sorted vector of string/quality pairs - better quality comes first
	/// @param sContent the input string, may contain multiple comma separated values and their quality,
	/// like "deflate, gzip;q=1.0, *;q=0.5"
	/// @param bSort if true container is stable sorted by quality descending. Default is true
	/// @return a (sorted) container of string/quality pairs
	template<class String = KStringView, typename T = std::vector<QualVal<String>>>
	static T Split(KStringView sContent, bool bSort = true)
	//-----------------------------------------------------------------------------
	{
		T Container;

		kSplit(Container, sContent, ",", ";q=");

		if (bSort)
		{
			std::stable_sort(Container.begin(), Container.end(),
							 [](const typename T::value_type& left, const typename T::value_type& right)
			{
				return left.iQuality > right.iQuality;
			});
		}

		return Container;
	}

//------
private:
//------

	Header m_header { NONE };
	std::unique_ptr<KString> m_sHeader;

}; // KHTTPHeader

//-----------------------------------------------------------------------------
DEKAF2_KHTTP_HEADER_CONSTEXPR_14
DEKAF2_PUBLIC
bool operator!=(const KHTTPHeader& left, const KHTTPHeader& right)
//-----------------------------------------------------------------------------
{
	return !operator==(left, right);
}

DEKAF2_NAMESPACE_END

namespace fmt
{

template <>
struct formatter<DEKAF2_PREFIX KHTTPHeader::Header> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KHTTPHeader::Header& Header, FormatContext& ctx) const
	{
		return formatter<string_view>::format(DEKAF2_PREFIX KHTTPHeader(Header).Serialize(), ctx);
	}
};

template <>
struct formatter<DEKAF2_PREFIX KHTTPHeader> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KHTTPHeader& Header, FormatContext& ctx) const
	{
		return formatter<string_view>::format(Header.Serialize(), ctx);
	}
};

} // end of namespace fmt

namespace std
{
	/// provide a std::hash for KHTTPHeader
	template<> struct hash<DEKAF2_PREFIX KHTTPHeader>
	{
		DEKAF2_KHTTP_HEADER_CONSTEXPR_14
		std::size_t operator()(const DEKAF2_PREFIX KHTTPHeader& header) const noexcept
		{
			return header.Hash();
		}
	};

} // end of namespace std

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
namespace boost {
#else
DEKAF2_NAMESPACE_BEGIN
#endif
	inline
	std::size_t hash_value(const DEKAF2_PREFIX KHTTPHeader& header)
	{
		return header.Hash();
	}
#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
}
#else
DEKAF2_NAMESPACE_END
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTTPHeaders
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static std::size_t constexpr MAX_LINELENGTH { 8 * 1024 };

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

	//-----------------------------------------------------------------------------
	/// return the HTTP version string
	const KString& GetHTTPVersion() const
	//-----------------------------------------------------------------------------
	{
		return sHTTPVersion;
	}

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

DEKAF2_NAMESPACE_END
