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
#include "kstring.h"
#include "kconfiguration.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
std::vector<KStringView> KInHTTPRequestLine::Parse(KString sRequestLine)
//-----------------------------------------------------------------------------
{
	clear();

	if (sRequestLine.size() <= MAX_REQUESTLINELENGTH)
	{
		m_sFull = std::move(sRequestLine);

		auto Words = m_sFull.Split(" ");

		if (Words.size() == 3)
		{
			auto sMethod = Words[0];

			if (sMethod.data() == m_sFull.data())
			{
				if (sMethod.size() <= std::numeric_limits<decltype(m_iMethodLen)>::max())
				{
					auto iMethodLen = sMethod.size();

					auto sResource = Words[1];

					auto iGap = sResource.data() - (sMethod.data() + sMethod.size());

					if (iGap == 1)
					{
						if (sResource.size() <= std::numeric_limits<decltype(m_iPathLen)>::max())
						{
							auto iHook = sResource.find('?');

							std::size_t iPathLen;

							if (iHook == KStringView::npos)
							{
								iPathLen = sResource.size();
							}
							else
							{
								iPathLen = iHook;
							}

							auto sVersion = Words[2];

							iGap = sVersion.data() - (sResource.data() + sResource.size());

							if (iGap == 1)
							{
								auto iVersionLen = sVersion.size();

								if (iVersionLen <= std::numeric_limits<decltype(m_iVersionLen)>::max())
								{
									if (sVersion.starts_with("HTTP/"))
									{
										m_iPathLen    = static_cast<decltype(m_iPathLen   )>(iPathLen   );
										m_iMethodLen  = static_cast<decltype(m_iMethodLen )>(iMethodLen );
										m_iVersionLen = static_cast<decltype(m_iVersionLen)>(iVersionLen);

										return Words;
									}
									else
									{
										kDebug(1, "version is not HTTP/");
									}
								}
								else
								{
									kDebug(1, "version too long: {}", sVersion);
								}
							}
							else
							{
								kDebug(1, "more than one space between resource and version");
							}
						}
						else
						{
							kDebug(1, "resource too long: {} bytes", sResource.size());
						}
					}
					else
					{
						kDebug(1, "more than one space between method and resource");
					}
				}
				else
				{
					kDebug(1, "method too long: {} bytes", sMethod.size());
				}
			}
			else
			{
				kDebug(1, "space(s) in front of method");
			}
		}
	}
	else
	{
		kDebug(1, "request line too long: {} bytes", sRequestLine.size());
	}

	return {};

} // Parse

//-----------------------------------------------------------------------------
KStringView KInHTTPRequestLine::GetMethod() const
//-----------------------------------------------------------------------------
{
	return KStringView { m_sFull.data(), m_iMethodLen };

} // GetMethod

//-----------------------------------------------------------------------------
KStringView KInHTTPRequestLine::GetResource() const
//-----------------------------------------------------------------------------
{
	if (m_iMethodLen)
	{
		return KStringView { m_sFull.data() + m_iMethodLen + 1, m_sFull.size() - m_iMethodLen - 1 - m_iVersionLen - 1 };
	}
	else
	{
		return {};
	}

} // GetResource

//-----------------------------------------------------------------------------
KStringView KInHTTPRequestLine::GetPath() const
//-----------------------------------------------------------------------------
{
	if (m_iMethodLen)
	{
		return KStringView { m_sFull.data() + m_iMethodLen + 1, m_iPathLen };
	}
	else
	{
		return {};
	}

} // GetPath

//-----------------------------------------------------------------------------
KStringView KInHTTPRequestLine::GetQuery() const
//-----------------------------------------------------------------------------
{
	if (m_iMethodLen)
	{
		auto iQueryStart = m_iMethodLen + 1 + m_iPathLen + 1;
		return KStringView { m_sFull.data() + iQueryStart, m_sFull.size() - iQueryStart - m_iVersionLen - 1 };
	}
	else
	{
		return {};
	}

} // GetQuery

//-----------------------------------------------------------------------------
KStringView KInHTTPRequestLine::GetVersion() const
//-----------------------------------------------------------------------------
{
	if (m_iMethodLen)
	{
		auto iVersionStart = m_sFull.size() - m_iVersionLen;
		return KStringView { m_sFull.data() + iVersionStart, m_iVersionLen };
	}
	else
	{
		return {};
	}

} // GetVersion

//-----------------------------------------------------------------------------
void KInHTTPRequestLine::clear()
//-----------------------------------------------------------------------------
{
	m_iPathLen    = 0;
	m_iMethodLen  = 0;
	m_iVersionLen = 0;
	m_sFull.clear();

} // clear

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	KString sLine;

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	if (!Stream.ReadLine(sLine, KInHTTPRequestLine::MAX_REQUESTLINELENGTH + 1))
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

	if (sLine.size() > KInHTTPRequestLine::MAX_REQUESTLINELENGTH)
	{
		return SetError(kFormat("HTTP request line too long: {} bytes", sLine.size()));
	}

	// analyze method and resource
	// GET /some/page?query=search#fragment HTTP/1.1

	if (!KASCII::kIsUpper(sLine.front()))
	{
		return SetError(kFormat("request line does not start with a valid character: {}", sLine.Left(20)));
	}

	auto Words = RequestLine.Parse(std::move(sLine));

	if (Words.size() != 3)
	{
		// garbage, bail out
		kDebug (2, RequestLine.Get());
		if (!Words.empty())
		{
			return SetError(kFormat("invalid request line: {} words instead of 3", Words.size()));
		}
		else
		{
			return SetError("invalid request line");
		}
	}

	Method       = Words[0];
	Resource     = Words[1];
	sHTTPVersion = Words[2];

	kDebug (1, "{} {} {}", Words[0], Words[1], Words[2]);

	if (!sHTTPVersion.starts_with("HTTP/"))
	{
		return SetError(kFormat("invalid status line of HTTP header: expected 'HTTP/' not '{}'", sHTTPVersion));
	}

	if (!KHTTPHeaders::Parse(Stream))
	{
		// error is already set
		return false;
	}

	// check for HTTP conformity:
	// need a HOST header that ideally matches us - will ATM only check for non-emptyness
	auto it = Headers.find(KHTTPHeader::HOST);

	if (it == Headers.end())
	{
		return SetError("no Host header");
	}

	if (it->second.empty())
	{
		return SetError("empty host header");
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	if (!Resource.empty())
	{
		if (!Endpoint.empty())
		{
			kDebug(1, "{} http://{}{} {}",
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
			kDebug(1, "{} {} {}",
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
			kDebug(1, "{} {} {}",
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
			kDebug(2, "resource is empty, inserting /");
			kDebug(1, "{} / {}",
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
KString KHTTPRequestHeaders::GetRemoteIP() const
//-----------------------------------------------------------------------------
{
	KString sBrowserIP;

	{
		// check the Forwarded: header
		// (note that we cannot use a ref due to the tolower conversion)
		const auto sHeader = Headers.Get(KHTTPHeader::FORWARDED).ToLowerASCII();

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
		const auto& sHeader = Headers.Get(KHTTPHeader::X_FORWARDED_FOR);
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

} // GetRemoteIP

//-----------------------------------------------------------------------------
uint16_t KHTTPRequestHeaders::GetRemotePort() const
//-----------------------------------------------------------------------------
{
	uint16_t iPort { 0 };

	{
		// check the Forwarded: header
		// (note that we cannot use a ref due to the tolower conversion)
		const auto sHeader = Headers.Get(KHTTPHeader::FORWARDED).ToLowerASCII();

		if (!sHeader.empty())
		{
			auto iStart = sHeader.find("for=");
			if (iStart != KString::npos)
			{
				auto iEnd = sHeader.find_first_of(",;", iStart+4);
				// KString is immune against npos in substr()
				auto sBrowserIP = sHeader.ToView(iStart+4, iEnd-(iStart+4));
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
						// check for the closing ]:
						auto iPos = sBrowserIP.find("]:");
						if (iPos != KString::npos)
						{
							iEnd = sBrowserIP.find('"', iPos);
							auto sBrowserPort = sBrowserIP.ToView(iPos + 2, iEnd-(iStart+2));
							iPort = sBrowserPort.UInt16();
						}
					}
				}
				else
				{
					// IPv4 address, remove :port
					auto iPos = sBrowserIP.find(':');
					if (iPos != KString::npos)
					{
						auto sBrowserPort = sBrowserIP.substr(iPos+1);
						iPort = sBrowserPort.UInt16();
					}
				}
			}
		}
	}

	return iPort;

} // GetRemotePort

//-----------------------------------------------------------------------------
url::KProtocol KHTTPRequestHeaders::GetRemoteProto() const
//-----------------------------------------------------------------------------
{
	url::KProtocol Proto;

	// check the Forwarded: header
	// (note that we cannot use a ref due to the tolower conversion)
	auto sHeader = Headers.Get(KHTTPHeader::FORWARDED).ToLowerASCII();

	if (!sHeader.empty())
	{
		auto iStart = sHeader.find("proto=");
		if (iStart != KString::npos)
		{
			auto iEnd = sHeader.find_first_of(",;", iStart+6);
			// KString is immune against npos in ToView()
			auto sProto = sHeader.ToView(iStart+6, iEnd-(iStart+6));
			sProto.Trim();
			Proto.Parse(sProto, true);
		}
	}

	if (Proto != url::KProtocol::UNDEFINED)
	{
		return Proto;
	}

	sHeader = Headers.Get(KHTTPHeader::X_FORWARDED_PROTO).ToLowerASCII();

	if (!sHeader.empty())
	{
		Proto.Parse(sHeader, true);
	}

	return Proto;

} // GetRemoteProto

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
		auto& sCompression = Headers.Get(KHTTPHeader::ACCEPT_ENCODING);

#ifdef DEKAF2_HAS_LIBZSTD
		if (sCompression.find("zstd") != KString::npos)
		{
			return "zstd"_ksv;
		}
		else
#endif
#ifdef DEKAF2_HAS_LIBLZMA
		if (sCompression.find("xz") != KString::npos)
		{
			return "xz"_ksv;
		}
		else if (sCompression.find("lzma") != KString::npos)
		{
			return "lzma"_ksv;
		}
		else
#endif
		if (sCompression.find("deflate") != KString::npos)
		{
			return "deflate"_ksv;
		}
		else if (sCompression.find("gzip") != KString::npos)
		{
			return "gzip"_ksv;
		}
		else if (sCompression.find("bzip2") != KString::npos)
		{
			return "bzip2"_ksv;
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
	RequestLine.clear();
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

//-----------------------------------------------------------------------------
KStringView KInHTTPRequest::GetCookie (KStringView sCookieName) const
//-----------------------------------------------------------------------------
{
	auto CookieHeader = Headers.find(KHTTPHeader::COOKIE);

	if (CookieHeader != Headers.end())
	{
		for (auto Cookie : CookieHeader->second.Split<std::vector<KStringViewPair>>(";", "="))
		{
			if (Cookie.first == sCookieName)
			{
				return Cookie.second;
			}
		}
	}

	return {};

} // GetCookie

} // end of namespace dekaf2

