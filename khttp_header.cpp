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

#include "khttp_header.h"
#include "kctype.h"
#include "kbase64.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPHeader::KHTTPHeader(KStringView sHeader)
//-----------------------------------------------------------------------------
: m_header(Parse(sHeader))
{
	if (m_header == NONE && !sHeader.empty())
	{
		m_sHeader = std::make_unique<KString>(sHeader);
	}
}

//-----------------------------------------------------------------------------
KHTTPHeader& KHTTPHeader::operator=(const KHTTPHeader& other)
//-----------------------------------------------------------------------------
{
	m_header = other.m_header;
	if (other.m_sHeader)
	{
		m_sHeader = std::make_unique<KString>(*other.m_sHeader);
	}
	else
	{
		m_sHeader.reset();
	}
	return *this;
}

//-----------------------------------------------------------------------------
KHTTPHeader::Header KHTTPHeader::Parse(KStringView sHeader)
//-----------------------------------------------------------------------------
{
	switch (kCalcCaseHash(sHeader))
	{
		case "accept"_hash:
			return ACCEPT;
		case "accept-charset"_hash:
			return ACCEPT_CHARSET;
		case "accept-datetime"_hash:
			return ACCEPT_DATETIME;
		case "accept-encoding"_hash:
			return ACCEPT_ENCODING;
		case "accept-language"_hash:
			return ACCEPT_LANGUAGE;
		case "accept-patch"_hash:
			return ACCEPT_PATCH;
		case "accept-ranges"_hash:
			return ACCEPT_RANGES;
		case "access-control-allow-credentials"_hash:
			return ACCESS_CONTROL_ALLOW_CREDENTIALS;
		case "access-control-allow-headers"_hash:
			return ACCESS_CONTROL_ALLOW_HEADERS;
		case "access-control-allow-methods"_hash:
			return ACCESS_CONTROL_ALLOW_METHODS;
		case "access-control-allow-origin"_hash:
			return ACCESS_CONTROL_ALLOW_ORIGIN;
		case "access-control-expose-headers"_hash:
			return ACCESS_CONTROL_EXPOSE_HEADERS;
		case "access-control-max-age"_hash:
			return ACCESS_CONTROL_MAX_AGE;
		case "access-control-request-headers"_hash:
			return ACCESS_CONTROL_REQUEST_HEADERS;
		case "access-control-request-method"_hash:
			return ACCESS_CONTROL_REQUEST_METHOD;
		case "age"_hash:
			return AGE;
		case "allow"_hash:
			return ALLOW;
		case "alt-svc"_hash:
			return ALT_SVC;
		case "authorization"_hash:
			return AUTHORIZATION;
		case "cache-control"_hash:
			return CACHE_CONTROL;
		case "connection"_hash:
			return CONNECTION;
		case "content-disposition"_hash:
			return CONTENT_DISPOSITION;
		case "content-encoding"_hash:
			return CONTENT_ENCODING;
		case "content-language"_hash:
			return CONTENT_LANGUAGE;
		case "content-length"_hash:
			return CONTENT_LENGTH;
		case "content-location"_hash:
			return CONTENT_LOCATION;
		case "content-md5"_hash:
			return CONTENT_MD5;
		case "content-range"_hash:
			return CONTENT_RANGE;
		case "content-type"_hash:
			return CONTENT_TYPE;
		case "cookie"_hash:
			return COOKIE;
		case "date"_hash:
			return DATE;
		case "etag"_hash:
			return ETAG;
		case "expect"_hash:
			return EXPECT;
		case "expires"_hash:
			return EXPIRES;
		case "forwarded"_hash:
			return FORWARDED;
		case "from"_hash:
			return FROM;
		case "host"_hash:
			return HOST;
		case "hostoverride"_hash:
			return HOST_OVERRIDE;
		case "if-match"_hash:
			return IF_MATCH;
		case "if-modified-since"_hash:
			return IF_MODIFIED_SINCE;
		case "if-none-match"_hash:
			return IF_NONE_MATCH;
		case "if-range"_hash:
			return IF_RANGE;
		case "if-unmodified-since"_hash:
			return IF_UNMODIFIED_SINCE;
		case "keep-alive"_hash:
			return KEEP_ALIVE;
		case "last-modified"_hash:
			return LAST_MODIFIED;
		case "link"_hash:
			return LINK;
		case "location"_hash:
			return LOCATION;
		case "max-forwards"_hash:
			return MAX_FORWARDS;
		case "origin"_hash:
			return ORIGIN;
		case "p3p"_hash:
			return P3P;
		case "pragma"_hash:
			return PRAGMA;
		case "proxy-authenticate"_hash:
			return PROXY_AUTHENTICATE;
		case "proxy-authorization"_hash:
			return PROXY_AUTHORIZATION;
		case "proxy-connection"_hash:
			return PROXY_CONNECTION;
		case "public-key-pins"_hash:
			return PUBLIC_KEY_PINS;
		case "range"_hash:
			return RANGE;
		case "referer"_hash:
			return REFERER;
		case "retry-after"_hash:
			return RETRY_AFTER;
		case "server"_hash:
			return SERVER;
		case "set-cookie"_hash:
			return SET_COOKIE;
		case "strict-transport-security"_hash:
			return STRICT_TRANSPORT_SECURITY;
		case "trailer"_hash:
			return TRAILER;
		case "transfer-encoding"_hash:
			return TRANSFER_ENCODING;
		case "upgrade"_hash:
			return UPGRADE;
		case "user-agent"_hash:
			return USER_AGENT;
		case "vary"_hash:
			return VARY;
		case "via"_hash:
			return VIA;
		case "warning"_hash:
			return WARNING;
		case "www-authenticate"_hash:
			return WWW_AUTHENTICATE;
		case "x-forwarded-for"_hash:
			return X_FORWARDED_FOR;
		case "x-forwarded-host"_hash:
			return X_FORWARDED_HOST;
		case "x-forwarded-proto"_hash:
			return X_FORWARDED_PROTO;
		case "x-forwarded-server"_hash:
			return X_FORWARDED_SERVER;
		case "x-frame-options"_hash:
			return X_FRAME_OPTIONS;
		case "x-powered-by"_hash:
			return X_POWERED_BY;
	}

	// not found..
	return NONE;

} // Parse

//-----------------------------------------------------------------------------
KStringView KHTTPHeader::Serialize() const
//-----------------------------------------------------------------------------
{
	switch (m_header)
	{
		case NONE:
			return (m_sHeader) ? (*m_sHeader).ToView() : "";
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
	kDebug(0, "missing switch case");
	return "";

} // Serialize

//-----------------------------------------------------------------------------
KString KHTTPHeader::DateToString(time_t tTime)
//-----------------------------------------------------------------------------
{
	return kFormHTTPTimestamp(tTime);

} // DateToString

//-----------------------------------------------------------------------------
/// read a date from a string with HTTP date formatting
time_t KHTTPHeader::StringToDate(KStringView sTime)
//-----------------------------------------------------------------------------
{
	return 0; // TBD
	
} // StringToDate

static_assert(std::is_nothrow_move_constructible<KHTTPHeader>::value,
			  "KHTTPHeader is intended to be nothrow move constructible, but is not!");

//-----------------------------------------------------------------------------
bool operator==(const KHTTPHeader& left, const KHTTPHeader& right)
//-----------------------------------------------------------------------------
{
	if (left.m_header != right.m_header)
	{
		return false;
	}

	if (left.m_header != KHTTPHeader::NONE)
	{
		return true;
	}

	return (left.m_sHeader && right.m_sHeader &&
			kCaseEqual(*left.m_sHeader, *right.m_sHeader));
}

//-----------------------------------------------------------------------------
bool KHTTPHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	// Continuation lines are no more allowed in HTTP headers, so we don't
	// care for them. Any broken header (like missing key, no colon) will
	// get dropped.
	// We also do not care for line endings and will cannonify them on
	// serialization.
	// For sake of SMTP we do now support continuation lines, but we
	// canonify the separator to a single space when composing multi line
	// headers into one single line header.

	if (!Headers.empty())
	{
		clear();
	}

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	KString sLine;
	KHeaderMap::iterator last = Headers.end();

	while (Stream.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			// end of header
			return true;
		}
		else if (sLine.size() > MAX_LINELENGTH)
		{
			return SetError(kFormat("HTTP header line too long: {} bytes", sLine.size()));
		}

		if (!KASCII::kIsAlpha(sLine.front()))
		{
			if (KASCII::kIsSpace(sLine.front()) && last != Headers.end())
			{
				// continuation line, append trimmed line to last header, insert a space
				kTrim(sLine);
				if (!sLine.empty())
				{
					if (last->second.size() + sLine.size() > MAX_LINELENGTH)
					{
						return SetError(kFormat("HTTP continuation header line too long: {} bytes",
												last->second.size() + sLine.size()));
					}
					kDebug(2, "continuation header: {}", sLine);
					last->second += ' ';
					last->second += sLine;
				}
				continue;
			}
			else
			{
				// garbage, drop
				continue;
			}
		}

		auto pos = sLine.find(':');

		if (pos == npos)
		{
			kDebug(2, "dropping invalid header: {}", sLine);
			continue;
		}

		// store
		KStringView sKey(sLine.ToView(0, pos));
		KStringView sValue(sLine.ToView(pos + 1, npos));
		kTrimRight(sKey);
		kTrim(sValue);
		kDebug(2, "{}: {}", sKey, sValue);
		last = Headers.Add(sKey, sValue);
	}

	return SetError("HTTP header did not end with empty line");

} // Parse


//-----------------------------------------------------------------------------
bool KHTTPHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	for (const auto& iter : Headers)
	{
		kDebug (2, "{}: {}", iter.first.Serialize(), iter.second);
		if (!Stream.Write(iter.first.Serialize())
			|| !Stream.Write(": ")
			|| !Stream.WriteLine(iter.second))
		{
			return SetError("Cannot write headers");
		}
	}

	if (!Stream.WriteLine() // blank line indicates end of headers
		|| !Stream.Flush())
	{
		return SetError("Cannot write headers");
	}

	return true;

} // Serialize

//-----------------------------------------------------------------------------
std::streamsize KHTTPHeaders::ContentLength() const
//-----------------------------------------------------------------------------
{
	std::streamsize iSize { -1 };

	KStringView sSize = Headers.Get(KHTTPHeader::CONTENT_LENGTH);

	if (!sSize.empty())
	{
		iSize = sSize.UInt64();
	}

	return iSize;

} // ContentLength

//-----------------------------------------------------------------------------
bool KHTTPHeaders::HasContent() const
//-----------------------------------------------------------------------------
{
	auto iSize = ContentLength();

	if (iSize < 0)
	{
		if (Headers.Get(KHTTPHeader::TRANSFER_ENCODING).ToLowerASCII() == "chunked")
		{
			return true;
		}

		auto& sConnection = Headers.Get(KHTTPHeader::CONNECTION);

		if (DEKAF2_LIKELY(sHTTPVersion != "HTTP/1.0"))
		{
			return sConnection == "close";
		}
		else
		{
			return sConnection.empty() // "close" is the default with HTTP/1.0!
				|| sConnection == "close";
		}
	}
	else
	{
		return iSize > 0;
	}

} // HasContent

//-----------------------------------------------------------------------------
void KHTTPHeaders::clear()
//-----------------------------------------------------------------------------
{
	Headers.clear();
	m_sCharset.clear();
	m_sContentType.clear();
	m_sError.clear();

} // clear

//-----------------------------------------------------------------------------
void KHTTPHeaders::SplitContentType() const
//-----------------------------------------------------------------------------
{
	KStringView sHeader = Headers.Get(KHTTPHeader::CONTENT_TYPE);
	if (!sHeader.empty())
	{
		kTrimLeft(sHeader);
		auto pos = sHeader.find(';');
		if (pos == KStringView::npos)
		{
			kTrimRight(sHeader);
			m_sContentType = sHeader;
		}
		else
		{
			KStringView sCtype = sHeader.substr(0, pos);
			kTrimRight(sCtype);
			m_sContentType = sCtype;

			sHeader.remove_prefix(pos + 1);
			pos = sHeader.find("charset=");
			if (pos != KStringView::npos)
			{
				sHeader.remove_prefix(pos + 8);
				kTrimLeft(sHeader);
				kTrimRight(sHeader);
				m_sCharset = sHeader;
				// charsets come in upper and lower case variations
				m_sCharset.MakeLower();
			}
		}
	}

} // SplitContentType

//-----------------------------------------------------------------------------
const KString& KHTTPHeaders::ContentType() const
//-----------------------------------------------------------------------------
{
	if (m_sContentType.empty())
	{
		SplitContentType();
	}
	return m_sContentType;

} // ContentType

//-----------------------------------------------------------------------------
const KString& KHTTPHeaders::Charset() const
//-----------------------------------------------------------------------------
{
	if (m_sCharset.empty())
	{
		SplitContentType();
	}
	return m_sCharset;

} // Charset

//-----------------------------------------------------------------------------
bool KHTTPHeaders::HasKeepAlive() const
//-----------------------------------------------------------------------------
{
	if (sHTTPVersion == "HTTP/1.0" || sHTTPVersion == "HTTP/0.9")
	{
		return false;
	}
	else
	{
		auto sValue = Headers.Get(KHTTPHeader::CONNECTION).ToLowerASCII();
		// keepalive is default with HTTP/1.1
		return sValue.empty() || sValue == "keep-alive" || sValue == "keepalive";
	}

} // HasKeepAlive

//-----------------------------------------------------------------------------
bool KHTTPHeaders::SetError(KString sError) const
//-----------------------------------------------------------------------------
{
	kDebug(1, sError);
	m_sError = std::move(sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
KHTTPHeaders::BasicAuthParms KHTTPHeaders::GetBasicAuthParms() const
//-----------------------------------------------------------------------------
{
	BasicAuthParms Parms;

	const auto it = Headers.find(KHTTPHeader::AUTHORIZATION);
	if (it != Headers.end())
	{
		if (it->second.starts_with("Basic "))
		{
			auto sDecoded = KBase64::Decode(it->second.Mid(6));
			auto iPos = sDecoded.find(':');
			if (iPos != KString::npos)
			{
				Parms.sUsername = sDecoded.Left(iPos);
				Parms.sPassword = sDecoded.Mid(iPos+1);
			}
		}
	}

	return Parms;

} // GetBasicAuthParms

} // end of namespace dekaf2
