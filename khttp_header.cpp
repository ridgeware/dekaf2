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

constexpr KStringViewZ KHTTPHeaders::ACCEPT;
constexpr KStringViewZ KHTTPHeaders::ACCEPT_CHARSET;
constexpr KStringViewZ KHTTPHeaders::ACCEPT_DATETIME;
constexpr KStringViewZ KHTTPHeaders::ACCEPT_ENCODING;
constexpr KStringViewZ KHTTPHeaders::ACCEPT_LANGUAGE;
constexpr KStringViewZ KHTTPHeaders::ACCEPT_PATCH;
constexpr KStringViewZ KHTTPHeaders::ACCEPT_RANGES;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_ALLOW_CREDENTIALS;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_ALLOW_HEADERS;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_ALLOW_METHODS;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_ALLOW_ORIGIN;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_EXPOSE_HEADERS;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_MAX_AGE;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_REQUEST_HEADERS;
constexpr KStringViewZ KHTTPHeaders::ACCESS_CONTROL_REQUEST_METHOD;
constexpr KStringViewZ KHTTPHeaders::AGE;
constexpr KStringViewZ KHTTPHeaders::ALLOW;
constexpr KStringViewZ KHTTPHeaders::ALT_SVC;
constexpr KStringViewZ KHTTPHeaders::AUTHORIZATION;
constexpr KStringViewZ KHTTPHeaders::CACHE_CONTROL;
constexpr KStringViewZ KHTTPHeaders::CONNECTION;
constexpr KStringViewZ KHTTPHeaders::CONTENT_DISPOSITION;
constexpr KStringViewZ KHTTPHeaders::CONTENT_ENCODING;
constexpr KStringViewZ KHTTPHeaders::CONTENT_LANGUAGE;
constexpr KStringViewZ KHTTPHeaders::CONTENT_LENGTH;
constexpr KStringViewZ KHTTPHeaders::CONTENT_LOCATION;
constexpr KStringViewZ KHTTPHeaders::CONTENT_MD5;
constexpr KStringViewZ KHTTPHeaders::CONTENT_RANGE;
constexpr KStringViewZ KHTTPHeaders::CONTENT_TYPE;
constexpr KStringViewZ KHTTPHeaders::COOKIE;
constexpr KStringViewZ KHTTPHeaders::DATE;
constexpr KStringViewZ KHTTPHeaders::ETAG;
constexpr KStringViewZ KHTTPHeaders::EXPECT;
constexpr KStringViewZ KHTTPHeaders::EXPIRES;
constexpr KStringViewZ KHTTPHeaders::FORWARDED;
constexpr KStringViewZ KHTTPHeaders::FROM;
constexpr KStringViewZ KHTTPHeaders::HOST;
constexpr KStringViewZ KHTTPHeaders::HOST_OVERRIDE;
constexpr KStringViewZ KHTTPHeaders::IF_MATCH;
constexpr KStringViewZ KHTTPHeaders::IF_MODIFIED_SINCE;
constexpr KStringViewZ KHTTPHeaders::IF_NONE_MATCH;
constexpr KStringViewZ KHTTPHeaders::IF_RANGE;
constexpr KStringViewZ KHTTPHeaders::IF_UNMODIFIED_SINCE;
constexpr KStringViewZ KHTTPHeaders::KEEP_ALIVE;
constexpr KStringViewZ KHTTPHeaders::LAST_MODIFIED;
constexpr KStringViewZ KHTTPHeaders::LINK;
constexpr KStringViewZ KHTTPHeaders::LOCATION;
constexpr KStringViewZ KHTTPHeaders::MAX_FORWARDS;
constexpr KStringViewZ KHTTPHeaders::ORIGIN;
constexpr KStringViewZ KHTTPHeaders::P3P;
constexpr KStringViewZ KHTTPHeaders::PRAGMA;
constexpr KStringViewZ KHTTPHeaders::PROXY_AUTHENTICATE;
constexpr KStringViewZ KHTTPHeaders::PROXY_AUTHORIZATION;
constexpr KStringViewZ KHTTPHeaders::PUBLIC_KEY_PINS;
constexpr KStringViewZ KHTTPHeaders::RANGE;
constexpr KStringViewZ KHTTPHeaders::REFERER;
constexpr KStringViewZ KHTTPHeaders::REQUEST_COOKIE;
constexpr KStringViewZ KHTTPHeaders::RESPONSE_COOKIE;
constexpr KStringViewZ KHTTPHeaders::RETRY_AFTER;
constexpr KStringViewZ KHTTPHeaders::SERVER;
constexpr KStringViewZ KHTTPHeaders::SET_COOKIE;
constexpr KStringViewZ KHTTPHeaders::STRICT_TRANSPORT_SECURITY;
constexpr KStringViewZ KHTTPHeaders::TRAILER;
constexpr KStringViewZ KHTTPHeaders::TRANSFER_ENCODING;
constexpr KStringViewZ KHTTPHeaders::UPGRADE;
constexpr KStringViewZ KHTTPHeaders::USER_AGENT;
constexpr KStringViewZ KHTTPHeaders::VARY;
constexpr KStringViewZ KHTTPHeaders::VIA;
constexpr KStringViewZ KHTTPHeaders::WARNING;
constexpr KStringViewZ KHTTPHeaders::WWW_AUTHENTICATE;
constexpr KStringViewZ KHTTPHeaders::X_FORWARDED_FOR;
constexpr KStringViewZ KHTTPHeaders::X_FORWARDED_HOST;
constexpr KStringViewZ KHTTPHeaders::X_FORWARDED_PROTO;
constexpr KStringViewZ KHTTPHeaders::X_FORWARDED_SERVER;
constexpr KStringViewZ KHTTPHeaders::X_FRAME_OPTIONS;
constexpr KStringViewZ KHTTPHeaders::X_POWERED_BY;

constexpr KStringViewZ KHTTPHeaders::accept;
constexpr KStringViewZ KHTTPHeaders::accept_charset;
constexpr KStringViewZ KHTTPHeaders::accept_datetime;
constexpr KStringViewZ KHTTPHeaders::accept_encoding;
constexpr KStringViewZ KHTTPHeaders::accept_language;
constexpr KStringViewZ KHTTPHeaders::accept_patch;
constexpr KStringViewZ KHTTPHeaders::accept_ranges;
constexpr KStringViewZ KHTTPHeaders::access_control_allow_credentials;
constexpr KStringViewZ KHTTPHeaders::access_control_allow_headers;
constexpr KStringViewZ KHTTPHeaders::access_control_allow_methods;
constexpr KStringViewZ KHTTPHeaders::access_control_allow_origin;
constexpr KStringViewZ KHTTPHeaders::access_control_expose_headers;
constexpr KStringViewZ KHTTPHeaders::access_control_max_age;
constexpr KStringViewZ KHTTPHeaders::access_control_request_headers;
constexpr KStringViewZ KHTTPHeaders::access_control_request_method;
constexpr KStringViewZ KHTTPHeaders::age;
constexpr KStringViewZ KHTTPHeaders::allow;
constexpr KStringViewZ KHTTPHeaders::alt_svc;
constexpr KStringViewZ KHTTPHeaders::authorization;
constexpr KStringViewZ KHTTPHeaders::cache_control;
constexpr KStringViewZ KHTTPHeaders::connection;
constexpr KStringViewZ KHTTPHeaders::content_disposition;
constexpr KStringViewZ KHTTPHeaders::content_encoding;
constexpr KStringViewZ KHTTPHeaders::content_language;
constexpr KStringViewZ KHTTPHeaders::content_length;
constexpr KStringViewZ KHTTPHeaders::content_location;
constexpr KStringViewZ KHTTPHeaders::content_md5;
constexpr KStringViewZ KHTTPHeaders::content_range;
constexpr KStringViewZ KHTTPHeaders::content_type;
constexpr KStringViewZ KHTTPHeaders::cookie;
constexpr KStringViewZ KHTTPHeaders::date;
constexpr KStringViewZ KHTTPHeaders::etag;
constexpr KStringViewZ KHTTPHeaders::expect;
constexpr KStringViewZ KHTTPHeaders::expires;
constexpr KStringViewZ KHTTPHeaders::forwarded;
constexpr KStringViewZ KHTTPHeaders::from;
constexpr KStringViewZ KHTTPHeaders::host;
constexpr KStringViewZ KHTTPHeaders::host_override;
constexpr KStringViewZ KHTTPHeaders::if_match;
constexpr KStringViewZ KHTTPHeaders::if_modified_since;
constexpr KStringViewZ KHTTPHeaders::if_none_match;
constexpr KStringViewZ KHTTPHeaders::if_range;
constexpr KStringViewZ KHTTPHeaders::if_unmodified_since;
constexpr KStringViewZ KHTTPHeaders::keep_alive;
constexpr KStringViewZ KHTTPHeaders::link;
constexpr KStringViewZ KHTTPHeaders::location;
constexpr KStringViewZ KHTTPHeaders::max_forwards;
constexpr KStringViewZ KHTTPHeaders::origin;
constexpr KStringViewZ KHTTPHeaders::p3p;
constexpr KStringViewZ KHTTPHeaders::pragma;
constexpr KStringViewZ KHTTPHeaders::proxy_authenticate;
constexpr KStringViewZ KHTTPHeaders::proxy_authorization;
constexpr KStringViewZ KHTTPHeaders::public_key_pins;
constexpr KStringViewZ KHTTPHeaders::range;
constexpr KStringViewZ KHTTPHeaders::referer;
constexpr KStringViewZ KHTTPHeaders::request_cookie;
constexpr KStringViewZ KHTTPHeaders::response_cookie;
constexpr KStringViewZ KHTTPHeaders::retry_after;
constexpr KStringViewZ KHTTPHeaders::server;
constexpr KStringViewZ KHTTPHeaders::set_cookie;
constexpr KStringViewZ KHTTPHeaders::strict_transport_security;
constexpr KStringViewZ KHTTPHeaders::trailer;
constexpr KStringViewZ KHTTPHeaders::transfer_encoding;
constexpr KStringViewZ KHTTPHeaders::upgrade;
constexpr KStringViewZ KHTTPHeaders::user_agent;
constexpr KStringViewZ KHTTPHeaders::vary;
constexpr KStringViewZ KHTTPHeaders::via;
constexpr KStringViewZ KHTTPHeaders::warning;
constexpr KStringViewZ KHTTPHeaders::www_authenticate;
constexpr KStringViewZ KHTTPHeaders::x_forwarded_for;
constexpr KStringViewZ KHTTPHeaders::x_forwarded_host;
constexpr KStringViewZ KHTTPHeaders::x_forwarded_proto;
constexpr KStringViewZ KHTTPHeaders::x_forwarded_server;
constexpr KStringViewZ KHTTPHeaders::x_frame_options;
constexpr KStringViewZ KHTTPHeaders::x_powered_by;

#endif

template class KProps<KCaseTrimString, KString>;

} // end of namespace dekaf2
