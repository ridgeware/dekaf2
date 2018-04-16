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

#include "khttp_header.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KHTTPHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	// Continuation lines are no more allowed in HTTP headers, so we don't
	// care for them. Any broken header (like missing key, no colon) will
	// get dropped.
	// We also do not care for line endings and will cannonify them on
	// serialization.

	if (!Headers.empty())
	{
		clear();
	}

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	KString sLine;

	while (Stream.ReadLine(sLine))
	{
		if (sLine.empty())
		{
			// end of header
			return true;
		}

		if (!std::isalpha(sLine.front()))
		{
			// garbage, drop
			continue;
		}

		auto pos = sLine.find(':');

		if (pos == npos)
		{
			// drop invalid header
			continue;
		}

		// store
		KStringView sKey(sLine.ToView(0, pos));
		KStringView sValue(sLine.ToView(pos + 1, npos));
		kTrimRight(sKey);
		kTrim(sValue);

		Headers.emplace(sKey, sValue);

	}

	return false;
}


//-----------------------------------------------------------------------------
bool KHTTPHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	for (const auto& iter : Headers)
	{
		Stream.Write(iter.first);
		Stream.Write(": ");
		Stream.WriteLine(iter.second);
	}

	Stream.WriteLine();

	Stream.Flush();

	return true;

} // Serialize

//-----------------------------------------------------------------------------
std::streamsize KHTTPHeaders::ContentLength() const
//-----------------------------------------------------------------------------
{
	std::streamsize iSize { -1 };

	KStringView sSize = Headers.Get(KHTTPHeaders::content_length);

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
		return Headers.Get(KHTTPHeaders::transfer_encoding) == "chunked"
		    || Headers.Get(KHTTPHeaders::connection) == "close";
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
	KStringView sHeader = Headers.Get(content_type);
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
	if (HTTPVersion == "HTTP/1.0" || HTTPVersion == "HTTP/0.9")
	{
		return false;
	}
	else
	{
		KStringView sValue = Headers.Get(KHTTPHeaders::connection);

		if (!sValue.empty())
		{
			KString slower = sValue;
			slower.MakeLower();
			return slower == "keep-alive" || slower == "keepalive";
		}
		else
		{
			// keepalive is default with HTTP/1.1
			return true;
		}
	}

} // HasKeepAlive

#if !defined(DEKAF2_HAS_CPP_17)

constexpr KStringView KHTTPHeaders::ACCEPT;
constexpr KStringView KHTTPHeaders::ACCEPT_CHARSET;
constexpr KStringView KHTTPHeaders::ACCEPT_DATETIME;
constexpr KStringView KHTTPHeaders::ACCEPT_ENCODING;
constexpr KStringView KHTTPHeaders::ACCEPT_LANGUAGE;
constexpr KStringView KHTTPHeaders::ACCEPT_PATCH;
constexpr KStringView KHTTPHeaders::ACCEPT_RANGES;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_ALLOW_CREDENTIALS;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_ALLOW_HEADERS;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_ALLOW_METHODS;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_ALLOW_ORIGIN;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_EXPOSE_HEADERS;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_MAX_AGE;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_REQUEST_HEADERS;
constexpr KStringView KHTTPHeaders::ACCESS_CONTROL_REQUEST_METHOD;
constexpr KStringView KHTTPHeaders::AGE;
constexpr KStringView KHTTPHeaders::ALLOW;
constexpr KStringView KHTTPHeaders::ALT_SVC;
constexpr KStringView KHTTPHeaders::AUTHORIZATION;
constexpr KStringView KHTTPHeaders::CACHE_CONTROL;
constexpr KStringView KHTTPHeaders::CONNECTION;
constexpr KStringView KHTTPHeaders::CONTENT_DISPOSITION;
constexpr KStringView KHTTPHeaders::CONTENT_ENCODING;
constexpr KStringView KHTTPHeaders::CONTENT_LANGUAGE;
constexpr KStringView KHTTPHeaders::CONTENT_LENGTH;
constexpr KStringView KHTTPHeaders::CONTENT_LOCATION;
constexpr KStringView KHTTPHeaders::CONTENT_MD5;
constexpr KStringView KHTTPHeaders::CONTENT_RANGE;
constexpr KStringView KHTTPHeaders::CONTENT_TYPE;
constexpr KStringView KHTTPHeaders::COOKIE;
constexpr KStringView KHTTPHeaders::DATE;
constexpr KStringView KHTTPHeaders::ETAG;
constexpr KStringView KHTTPHeaders::EXPECT;
constexpr KStringView KHTTPHeaders::EXPIRES;
constexpr KStringView KHTTPHeaders::FORWARDED;
constexpr KStringView KHTTPHeaders::FROM;
constexpr KStringView KHTTPHeaders::HOST;
constexpr KStringView KHTTPHeaders::HOST_OVERRIDE;
constexpr KStringView KHTTPHeaders::IF_MATCH;
constexpr KStringView KHTTPHeaders::IF_MODIFIED_SINCE;
constexpr KStringView KHTTPHeaders::IF_NONE_MATCH;
constexpr KStringView KHTTPHeaders::IF_RANGE;
constexpr KStringView KHTTPHeaders::IF_UNMODIFIED_SINCE;
constexpr KStringView KHTTPHeaders::KEEP_ALIVE;
constexpr KStringView KHTTPHeaders::LAST_MODIFIED;
constexpr KStringView KHTTPHeaders::LINK;
constexpr KStringView KHTTPHeaders::LOCATION;
constexpr KStringView KHTTPHeaders::MAX_FORWARDS;
constexpr KStringView KHTTPHeaders::ORIGIN;
constexpr KStringView KHTTPHeaders::P3P;
constexpr KStringView KHTTPHeaders::PRAGMA;
constexpr KStringView KHTTPHeaders::PROXY_AUTHENTICATE;
constexpr KStringView KHTTPHeaders::PROXY_AUTHORIZATION;
constexpr KStringView KHTTPHeaders::PUBLIC_KEY_PINS;
constexpr KStringView KHTTPHeaders::RANGE;
constexpr KStringView KHTTPHeaders::REFERER;
constexpr KStringView KHTTPHeaders::REQUEST_COOKIE;
constexpr KStringView KHTTPHeaders::RESPONSE_COOKIE;
constexpr KStringView KHTTPHeaders::RETRY_AFTER;
constexpr KStringView KHTTPHeaders::SERVER;
constexpr KStringView KHTTPHeaders::SET_COOKIE;
constexpr KStringView KHTTPHeaders::STRICT_TRANSPORT_SECURITY;
constexpr KStringView KHTTPHeaders::TRAILER;
constexpr KStringView KHTTPHeaders::TRANSFER_ENCODING;
constexpr KStringView KHTTPHeaders::UPGRADE;
constexpr KStringView KHTTPHeaders::USER_AGENT;
constexpr KStringView KHTTPHeaders::VARY;
constexpr KStringView KHTTPHeaders::VIA;
constexpr KStringView KHTTPHeaders::WARNING;
constexpr KStringView KHTTPHeaders::WWW_AUTHENTICATE;
constexpr KStringView KHTTPHeaders::X_FORWARDED_FOR;
constexpr KStringView KHTTPHeaders::X_FORWARDED_HOST;
constexpr KStringView KHTTPHeaders::X_FORWARDED_PROTO;
constexpr KStringView KHTTPHeaders::X_FORWARDED_SERVER;
constexpr KStringView KHTTPHeaders::X_FRAME_OPTIONS;
constexpr KStringView KHTTPHeaders::X_POWERED_BY;

constexpr KStringView KHTTPHeaders::accept;
constexpr KStringView KHTTPHeaders::accept_charset;
constexpr KStringView KHTTPHeaders::accept_datetime;
constexpr KStringView KHTTPHeaders::accept_encoding;
constexpr KStringView KHTTPHeaders::accept_language;
constexpr KStringView KHTTPHeaders::accept_patch;
constexpr KStringView KHTTPHeaders::accept_ranges;
constexpr KStringView KHTTPHeaders::access_control_allow_credentials;
constexpr KStringView KHTTPHeaders::access_control_allow_headers;
constexpr KStringView KHTTPHeaders::access_control_allow_methods;
constexpr KStringView KHTTPHeaders::access_control_allow_origin;
constexpr KStringView KHTTPHeaders::access_control_expose_headers;
constexpr KStringView KHTTPHeaders::access_control_max_age;
constexpr KStringView KHTTPHeaders::access_control_request_headers;
constexpr KStringView KHTTPHeaders::access_control_request_method;
constexpr KStringView KHTTPHeaders::age;
constexpr KStringView KHTTPHeaders::allow;
constexpr KStringView KHTTPHeaders::alt_svc;
constexpr KStringView KHTTPHeaders::authorization;
constexpr KStringView KHTTPHeaders::cache_control;
constexpr KStringView KHTTPHeaders::connection;
constexpr KStringView KHTTPHeaders::content_disposition;
constexpr KStringView KHTTPHeaders::content_encoding;
constexpr KStringView KHTTPHeaders::content_language;
constexpr KStringView KHTTPHeaders::content_length;
constexpr KStringView KHTTPHeaders::content_location;
constexpr KStringView KHTTPHeaders::content_md5;
constexpr KStringView KHTTPHeaders::content_range;
constexpr KStringView KHTTPHeaders::content_type;
constexpr KStringView KHTTPHeaders::cookie;
constexpr KStringView KHTTPHeaders::date;
constexpr KStringView KHTTPHeaders::etag;
constexpr KStringView KHTTPHeaders::expect;
constexpr KStringView KHTTPHeaders::expires;
constexpr KStringView KHTTPHeaders::forwarded;
constexpr KStringView KHTTPHeaders::from;
constexpr KStringView KHTTPHeaders::host;
constexpr KStringView KHTTPHeaders::host_override;
constexpr KStringView KHTTPHeaders::if_match;
constexpr KStringView KHTTPHeaders::if_modified_since;
constexpr KStringView KHTTPHeaders::if_none_match;
constexpr KStringView KHTTPHeaders::if_range;
constexpr KStringView KHTTPHeaders::if_unmodified_since;
constexpr KStringView KHTTPHeaders::keep_alive;
constexpr KStringView KHTTPHeaders::link;
constexpr KStringView KHTTPHeaders::location;
constexpr KStringView KHTTPHeaders::max_forwards;
constexpr KStringView KHTTPHeaders::origin;
constexpr KStringView KHTTPHeaders::p3p;
constexpr KStringView KHTTPHeaders::pragma;
constexpr KStringView KHTTPHeaders::proxy_authenticate;
constexpr KStringView KHTTPHeaders::proxy_authorization;
constexpr KStringView KHTTPHeaders::public_key_pins;
constexpr KStringView KHTTPHeaders::range;
constexpr KStringView KHTTPHeaders::referer;
constexpr KStringView KHTTPHeaders::request_cookie;
constexpr KStringView KHTTPHeaders::response_cookie;
constexpr KStringView KHTTPHeaders::retry_after;
constexpr KStringView KHTTPHeaders::server;
constexpr KStringView KHTTPHeaders::set_cookie;
constexpr KStringView KHTTPHeaders::strict_transport_security;
constexpr KStringView KHTTPHeaders::trailer;
constexpr KStringView KHTTPHeaders::transfer_encoding;
constexpr KStringView KHTTPHeaders::upgrade;
constexpr KStringView KHTTPHeaders::user_agent;
constexpr KStringView KHTTPHeaders::vary;
constexpr KStringView KHTTPHeaders::via;
constexpr KStringView KHTTPHeaders::warning;
constexpr KStringView KHTTPHeaders::www_authenticate;
constexpr KStringView KHTTPHeaders::x_forwarded_for;
constexpr KStringView KHTTPHeaders::x_forwarded_host;
constexpr KStringView KHTTPHeaders::x_forwarded_proto;
constexpr KStringView KHTTPHeaders::x_forwarded_server;
constexpr KStringView KHTTPHeaders::x_frame_options;
constexpr KStringView KHTTPHeaders::x_powered_by;

#endif

template class KProps<KCaseTrimString, KString>;

} // end of namespace dekaf2
