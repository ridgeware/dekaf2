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
		return Headers.Get(KHTTPHeaders::transfer_encoding) == "chunked";
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


#if !defined(DEKAF2_NO_GCC) && (DEKAF2_GCC_VERSION < 70000)

// for some strange reason gcc < 7 wants these repeated definitions
// once one puts a constexpr variable into a class. As long as
// they stay outside of a class, the linker does not complain about
// missing references, but once they are inside a class it does.

constexpr KStringView KHeader::ACCEPT;
constexpr KStringView KHeader::ACCEPT_CHARSET;
constexpr KStringView KHeader::ACCEPT_DATETIME;
constexpr KStringView KHeader::ACCEPT_ENCODING;
constexpr KStringView KHeader::ACCEPT_ENCODING;
constexpr KStringView KHeader::ACCEPT_LANGUAGE;
constexpr KStringView KHeader::ACCEPT_PATCH;
constexpr KStringView KHeader::ACCEPT_RANGES;
constexpr KStringView KHeader::ACCESS_CONTROL_ALLOW_CREDENTIALS;
constexpr KStringView KHeader::ACCESS_CONTROL_ALLOW_HEADERS;
constexpr KStringView KHeader::ACCESS_CONTROL_ALLOW_METHODS;
constexpr KStringView KHeader::ACCESS_CONTROL_ALLOW_ORIGIN;
constexpr KStringView KHeader::ACCESS_CONTROL_EXPOSE_HEADERS;
constexpr KStringView KHeader::ACCESS_CONTROL_MAX_AGE;
constexpr KStringView KHeader::ACCESS_CONTROL_REQUEST_HEADERS;
constexpr KStringView KHeader::ACCESS_CONTROL_REQUEST_METHOD;
constexpr KStringView KHeader::AGE;
constexpr KStringView KHeader::ALLOW;
constexpr KStringView KHeader::ALT_SVC;
constexpr KStringView KHeader::AUTHORIZATION;
constexpr KStringView KHeader::CACHE_CONTROL;
constexpr KStringView KHeader::CONNECTION;
constexpr KStringView KHeader::CONTENT_DISPOSITION;
constexpr KStringView KHeader::CONTENT_ENCODING;
constexpr KStringView KHeader::CONTENT_LANGUAGE;
constexpr KStringView KHeader::CONTENT_LENGTH;
constexpr KStringView KHeader::CONTENT_LOCATION;
constexpr KStringView KHeader::CONTENT_MD5;
constexpr KStringView KHeader::CONTENT_RANGE;
constexpr KStringView KHeader::CONTENT_TYPE;
constexpr KStringView KHeader::COOKIE;
constexpr KStringView KHeader::DATE;
constexpr KStringView KHeader::ETAG;
constexpr KStringView KHeader::EXPECT;
constexpr KStringView KHeader::EXPIRES;
constexpr KStringView KHeader::FORWARDED;
constexpr KStringView KHeader::FROM;
constexpr KStringView KHeader::HOST;
constexpr KStringView KHeader::HOST_OVERRIDE;
constexpr KStringView KHeader::IF_MATCH;
constexpr KStringView KHeader::IF_MODIFIED_SINCE;
constexpr KStringView KHeader::IF_NONE_MATCH;
constexpr KStringView KHeader::IF_RANGE;
constexpr KStringView KHeader::IF_UNMODIFIED_SINCE;
constexpr KStringView KHeader::KEEP_ALIVE;
constexpr KStringView KHeader::LAST_MODIFIED;
constexpr KStringView KHeader::LINK;
constexpr KStringView KHeader::LOCATION;
constexpr KStringView KHeader::MAX_FORWARDS;
constexpr KStringView KHeader::ORIGIN;
constexpr KStringView KHeader::P3P;
constexpr KStringView KHeader::PRAGMA;
constexpr KStringView KHeader::PROXY_AUTHENTICATE;
constexpr KStringView KHeader::PROXY_AUTHORIZATION;
constexpr KStringView KHeader::PUBLIC_KEY_PINS;
constexpr KStringView KHeader::RANGE;
constexpr KStringView KHeader::REFERER;
constexpr KStringView KHeader::REQUEST_COOKIE;
constexpr KStringView KHeader::RESPONSE_COOKIE;
constexpr KStringView KHeader::RETRY_AFTER;
constexpr KStringView KHeader::SERVER;
constexpr KStringView KHeader::SET_COOKIE;
constexpr KStringView KHeader::STRICT_TRANSPORT_SECURITY;
constexpr KStringView KHeader::TRAILER;
constexpr KStringView KHeader::TRANSFER_ENCODING;
constexpr KStringView KHeader::UPGRADE;
constexpr KStringView KHeader::USER_AGENT;
constexpr KStringView KHeader::VARY;
constexpr KStringView KHeader::VIA;
constexpr KStringView KHeader::WARNING;
constexpr KStringView KHeader::WWW_AUTHENTICATE;
constexpr KStringView KHeader::X_FORWARDED_FOR;
constexpr KStringView KHeader::X_FORWARDED_HOST;
constexpr KStringView KHeader::X_FORWARDED_PROTO;
constexpr KStringView KHeader::X_FORWARDED_SERVER;
constexpr KStringView KHeader::X_FRAME_OPTIONS;
constexpr KStringView KHeader::X_POWERED_BY;

constexpr KStringView KHeader::accept;
constexpr KStringView KHeader::accept_charset;
constexpr KStringView KHeader::accept_datetime;
constexpr KStringView KHeader::accept_encoding;
constexpr KStringView KHeader::accept_encoding;
constexpr KStringView KHeader::accept_language;
constexpr KStringView KHeader::accept_patch;
constexpr KStringView KHeader::accept_ranges;
constexpr KStringView KHeader::access_control_allow_credentials;
constexpr KStringView KHeader::access_control_allow_headers;
constexpr KStringView KHeader::access_control_allow_methods;
constexpr KStringView KHeader::access_control_allow_origin;
constexpr KStringView KHeader::access_control_expose_headers;
constexpr KStringView KHeader::access_control_max_age;
constexpr KStringView KHeader::access_control_request_headers;
constexpr KStringView KHeader::access_control_request_method;
constexpr KStringView KHeader::age;
constexpr KStringView KHeader::allow;
constexpr KStringView KHeader::alt_svc;
constexpr KStringView KHeader::authorization;
constexpr KStringView KHeader::cache_control;
constexpr KStringView KHeader::connection;
constexpr KStringView KHeader::content_disposition;
constexpr KStringView KHeader::content_encoding;
constexpr KStringView KHeader::content_language;
constexpr KStringView KHeader::content_length;
constexpr KStringView KHeader::content_location;
constexpr KStringView KHeader::content_md5;
constexpr KStringView KHeader::content_range;
constexpr KStringView KHeader::content_type;
constexpr KStringView KHeader::cookie;
constexpr KStringView KHeader::date;
constexpr KStringView KHeader::etag;
constexpr KStringView KHeader::expect;
constexpr KStringView KHeader::expires;
constexpr KStringView KHeader::forwarded;
constexpr KStringView KHeader::from;
constexpr KStringView KHeader::host;
constexpr KStringView KHeader::host_override;
constexpr KStringView KHeader::if_match;
constexpr KStringView KHeader::if_modified_since;
constexpr KStringView KHeader::if_none_match;
constexpr KStringView KHeader::if_range;
constexpr KStringView KHeader::if_unmodified_since;
constexpr KStringView KHeader::keep_alive;
constexpr KStringView KHeader::link;
constexpr KStringView KHeader::location;
constexpr KStringView KHeader::max_forwards;
constexpr KStringView KHeader::origin;
constexpr KStringView KHeader::p3p;
constexpr KStringView KHeader::pragma;
constexpr KStringView KHeader::proxy_authenticate;
constexpr KStringView KHeader::proxy_authorization;
constexpr KStringView KHeader::public_key_pins;
constexpr KStringView KHeader::range;
constexpr KStringView KHeader::referer;
constexpr KStringView KHeader::request_cookie;
constexpr KStringView KHeader::response_cookie;
constexpr KStringView KHeader::retry_after;
constexpr KStringView KHeader::server;
constexpr KStringView KHeader::set_cookie;
constexpr KStringView KHeader::strict_transport_security;
constexpr KStringView KHeader::trailer;
constexpr KStringView KHeader::transfer_encoding;
constexpr KStringView KHeader::upgrade;
constexpr KStringView KHeader::user_agent;
constexpr KStringView KHeader::vary;
constexpr KStringView KHeader::via;
constexpr KStringView KHeader::warning;
constexpr KStringView KHeader::www_authenticate;
constexpr KStringView KHeader::x_forwarded_for;
constexpr KStringView KHeader::x_forwarded_host;
constexpr KStringView KHeader::x_forwarded_proto;
constexpr KStringView KHeader::x_forwarded_server;
constexpr KStringView KHeader::x_frame_options;
constexpr KStringView KHeader::x_powered_by;

#endif

template class KProps<KCaseTrimString, KString>;

} // end of namespace dekaf2
