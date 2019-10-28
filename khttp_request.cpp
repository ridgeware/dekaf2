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
		// connection, or the connection close in a CGI environment.
		// Unset the error string and return false;
		if (Stream.Good() == false)
		{
			kDebug (2, "input stream got closed");
		}
		else
		{
			kDebug (2, "cannot read from input stream");
		}
		return SetError("");
	}

	if (sLine.empty())
	{
		return SetError("empty request line");
	}

	// analyze method and resource
	// GET /some/page?query=search#fragment HTTP/1.1

	auto Words = sLine.Split(" ");

	if (Words.size() != 3)
	{
		// garbage, bail out
		kDebug (2, "{}", sLine);
		return SetError(kFormat("invalid status line of HTTP header: {} words instead of 3", Words.size()));
	}

	Method = Words[0];
	Resource = Words[1];
	sHTTPVersion = Words[2];

	kDebug(2, "{} {} {}", Words[0], Words[1], Words[2]);

	if (!sHTTPVersion.starts_with("HTTP/"))
	{
		return SetError(kFormat("invalid status line of HTTP header: expected 'HTTP/' not '{}'", sHTTPVersion));
	}

	return KHTTPHeaders::Parse(Stream);

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	if (!Resource.empty())
	{
		if (!Endpoint.empty())
		{
			kDebug(2, "{} http://{}{} {}",
					Method.Serialize(),
					Endpoint.Serialize(),
					Resource.Serialize(),
					sHTTPVersion);
			Stream.FormatLine("{} http://{}{} {}",
							  Method.Serialize(),
							  Endpoint.Serialize(),
							  Resource.Serialize(),
							  sHTTPVersion);
		}
		else
		{
			kDebug(2, "{} {} {}",
					Method.Serialize(),
					Resource.Serialize(),
					sHTTPVersion);
			Stream.FormatLine("{} {} {}",
							  Method.Serialize(),
							  Resource.Serialize(),
							  sHTTPVersion);
		}
	}
	else
	{
		if (!Endpoint.empty() && Method == KHTTPMethod::CONNECT)
		{
			kDebug(2, "{} {} {}",
					Method.Serialize(),
					Endpoint.Serialize(),
					sHTTPVersion);
			// this is a CONNECT request
			Stream.FormatLine("{} {} {}",
							  Method.Serialize(),
							  Endpoint.Serialize(),
							  sHTTPVersion);
		}
		else
		{
			// special case, insert a single slash for the resource to
			// satisfy the HTTP protocol
			kDebug(1, "resource is empty, inserting /");
			kDebug(2, "{} / {}",
						Method.Serialize(),
						sHTTPVersion);
			Stream.FormatLine("{} / {}",
							  Method.Serialize(),
							  sHTTPVersion);
		}
	}

	return KHTTPHeaders::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::HasChunking() const
//-----------------------------------------------------------------------------
{
	if (sHTTPVersion == "HTTP/1.0" || sHTTPVersion == "HTTP/0.9")
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
		const auto sHeader = Headers.Get(KHTTPHeaders::forwarded).ToLowerASCII();
		if (!sHeader.empty())
		{
			auto iStart = sHeader.find("for=");
			if (iStart != KString::npos)
			{
				auto iEnd = sHeader.find_first_of(",;", iStart+4);
				// KString is immune against npos in substr()
				sBrowserIP = sHeader.substr(iStart+4, iEnd-(iStart+4));
				sBrowserIP.Trim();

				// The header may be an ipv6 address, which has a
				// different format in the Forwarded: header than
				// in X-Forwarded-For, so we normalize it
				// Forwarded   = "[2001:db8:cafe::17]:4711"
				// X-Forwarded = 2001:db8:cafe::17 (no port)
				if (sBrowserIP.size() > 1 && sBrowserIP.front() == '"')
				{
					if (sBrowserIP[1] == '[')
					{
						sBrowserIP.remove_prefix(2);
						// check for the closing ]
						auto iPos = sBrowserIP.find(']');
						if (iPos != KString::npos)
						{
							// remove all after and including the ]
							sBrowserIP.erase(iPos);
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
				else
				{
					// IPv4 address, remove :port
					auto iPos = sBrowserIP.find(':');
					if (iPos != KString::npos)
					{
						// remove all after and including the :
						sBrowserIP.erase(iPos);
					}
				}
			}
		}
	}

	if (sBrowserIP.empty())
	{
		// check the X-Forwarded-For: header
		const auto& sHeader = Headers.Get(KHTTPHeaders::x_forwarded_for);
		if (!sHeader.empty())
		{
			auto iEnd = sHeader.find(',');
			// KString is immune against npos in substr()
			sBrowserIP = sHeader.substr(0, iEnd);
			sBrowserIP.Trim();
		}

		if (sBrowserIP.empty())
		{
			// check the X-ProxyUser-IP: header
			// (mozilla claims this is used by some google services)
			const auto& sHeader = Headers.Get("x-proxyuser-ip");
			if (!sHeader.empty())
			{
				auto iEnd = sHeader.find(',');
				// KString is immune against npos in substr()
				sBrowserIP = sHeader.substr(0, iEnd);
				sBrowserIP.Trim();
			}
		}

		sBrowserIP.ToLowerASCII();
	}

	return sBrowserIP;

} // GetBrowserIP

//-----------------------------------------------------------------------------
KStringView KHTTPRequestHeaders::SupportedCompression() const
//-----------------------------------------------------------------------------
{
	// for compression we need to switch to chunked transfer, as we do not know
	// the size of the compressed content - however, chunking is only supported
	// since HTTP/1.1
	if (HasChunking())
	{
		// check the client's request headers for accepted compression encodings
		auto& sCompression = Headers.Get(KHTTPHeaders::accept_encoding);

		if (sCompression.find("gzip") != KString::npos)
		{
			return "gzip"_ksv;
		}
		else if (sCompression.find("deflate") != KString::npos)
		{
			return "deflate"_ksv;
		}
	}

	return ""_ksv;

} // SupportsCompression

//-----------------------------------------------------------------------------
void KHTTPRequestHeaders::clear()
//-----------------------------------------------------------------------------
{
	KHTTPHeaders::clear();
	sHTTPVersion.clear();
	Resource.clear();
	Endpoint.clear();

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
	return KHTTPRequestHeaders::Parse(UnfilteredStream()) && KInHTTPFilter::Parse(*this, 0);

} // Parse

} // end of namespace dekaf2
