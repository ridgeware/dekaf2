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

#include "khttp_request.h"

namespace dekaf2 {


//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	KString sLine;

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	if (!Stream.ReadLine(sLine))
	{
		// this is simply a read timeout, probably on a keep-alive
		// connection. Unset the error string and return false;
		return SetError("");
	}

	if (sLine.empty())
	{
		return SetError("empty request line");
	}

	// analyze method and resource
	// GET /some/page?query=search#fragment HTTP/1.1

	std::vector<KStringView> Words;
	Words.reserve(3);
	kSplit(Words, sLine, " ");

	if (Words.size() != 3)
	{
		// garbage, bail out
		return SetError("invalid HTTP header");
	}

	Method = Words[0];
	Resource = Words[1];
	HTTPVersion = Words[2];

	if (!HTTPVersion.StartsWith("HTTP/"))
	{
		return SetError("missing HTTP version in header");
	}

	return KHTTPHeaders::Parse(Stream);

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	Stream.FormatLine("{} {} {}",
					  Method.Serialize(),
					  Resource.Serialize(),
					  HTTPVersion);

	return KHTTPHeaders::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::HasChunking() const
//-----------------------------------------------------------------------------
{
	if (HTTPVersion == "HTTP/1.0" || HTTPVersion == "HTTP/0.9")
	{
		return false;
	}
	else
	{
		return true;
	}

} // HasChunking

//-----------------------------------------------------------------------------
KString KHTTPRequestHeaders::GetBrowserIP() const
//-----------------------------------------------------------------------------
{
	KString sBrowserIP;

	{
		// check the Forwarded: header
		auto sHeader = Headers.Get(KHTTPHeaders::forwarded).ToLower();
		if (!sHeader.empty())
		{
			auto iStart = sHeader.find("for=");
			if (iStart != KString::npos)
			{
				auto iEnd = sHeader.find_first_of(",;", iStart+4);
				// KString is immune against npos in substr()
				sBrowserIP = sHeader.substr(iStart+4, iEnd-(iStart+4));
				sBrowserIP.Trim();
			}
		}
	}

	if (sBrowserIP.empty())
	{
		// check the X-Forwarded-For: header
		auto& sHeader = Headers.Get(KHTTPHeaders::x_forwarded_for);
		if (!sHeader.empty())
		{
			auto iEnd = sHeader.find(',');
			// KString is immune against npos in substr()
			sBrowserIP = sHeader.substr(0, iEnd);
			sBrowserIP.Trim();
		}
	}

	if (sBrowserIP.empty())
	{
		// check the X-ProxyUser-IP: header
		// (mozilla claims this is used by some google services)
		auto& sHeader = Headers.Get("x-proxyuser-ip");
		if (!sHeader.empty())
		{
			auto iEnd = sHeader.find(',');
			// KString is immune against npos in substr()
			sBrowserIP = sHeader.substr(0, iEnd);
			sBrowserIP.Trim();
		}
	}

	// The header may be an ipv6 address, which has a
	// different format in the Forwarded: header than
	// in X-Forwarded-For, so we normalize it (also if
	// coming from other headers)
	// Forwarded   = "[2001:db8:cafe::17]:4711"
	// X-Forwarded = 2001:db8:cafe::17:4711
	if (sBrowserIP.size() > 1 && sBrowserIP.front() == '"')
	{
		if (sBrowserIP[1] == '[')
		{
			sBrowserIP.remove_prefix(2);
			// check for the closing ]
			auto iPos = sBrowserIP.find(']');
			if (iPos != KString::npos)
			{
				sBrowserIP.erase(iPos, 1);
			}
		}
		else
		{
			sBrowserIP.remove_prefix(1);
		}
		if (!sBrowserIP.empty() && sBrowserIP.back() == '"')
		{
			sBrowserIP.remove_suffix(1);
		}
	}

	// remove an optional port from the IP
	auto iColon = sBrowserIP.rfind(':');
	if (iColon != KString::npos)
	{
		// check if the colon is part of an IPv6 address,
		// or if it is host:port
		if (!iColon || sBrowserIP[iColon - 1] != ':')
		{
			sBrowserIP.erase(iColon);
		}
	}

	return sBrowserIP;

} // GetBrowserIP

//-----------------------------------------------------------------------------
void KHTTPRequestHeaders::clear()
//-----------------------------------------------------------------------------
{
	KHTTPHeaders::clear();
	HTTPVersion.clear();
	Resource.clear();

} // clear

//-----------------------------------------------------------------------------
bool KOutHTTPRequest::Serialize()
//-----------------------------------------------------------------------------
{
	// set up the chunked writer
	return KOutHTTPFilter::Parse(*this) && KHTTPRequestHeaders::Serialize(UnfilteredStream());

} // Serialize

//-----------------------------------------------------------------------------
bool KInHTTPRequest::Parse()
//-----------------------------------------------------------------------------
{
	// set up the chunked reader
	return KHTTPRequestHeaders::Parse(UnfilteredStream()) && KInHTTPFilter::Parse(*this);

} // Parse

} // end of namespace dekaf2
