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
#include "kstringutils.h"
#include "kconfiguration.h"
#include "kipaddress.h"
#include "kipnetwork.h"

DEKAF2_NAMESPACE_BEGIN

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
								kDebug(1, "more than one space between {}", "resource and version");
							}
						}
						else
						{
							kDebug(1, "{} too long: {} bytes", "resource", sResource.size());
						}
					}
					else
					{
						kDebug(1, "more than one space between {}", "method and resource");
					}
				}
				else
				{
					kDebug(1, "{} too long: {} bytes", "method", sMethod.size());
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
		return SetError(kFormat("request line does not start with a valid character: {}", kEscapeForLogging(sLine.Left(20))));
	}

	auto Words = RequestLine.Parse(std::move(sLine));

	if (Words.size() != 3)
	{
		// garbage, bail out
		kDebug (2, RequestLine.Get());
		if (!Words.empty())
		{
			return SetError(kFormat("{}: {} words instead of 3", "invalid request line: {}", Words.size(), kEscapeForLogging(sLine.Left(40))));
		}
		else
		{
			return SetError("invalid request line");
		}
	}

	Method       = Words[0];
	Resource     = Words[1];
	SetHTTPVersion(Words[2]);

	kDebug (1, "{} {} {}", Words[0], Words[1], Words[2]);

	if (GetHTTPVersion() == KHTTPVersion::none)
	{
		return SetError(kFormat("invalid HTTP version: {}", Words[2]));
	}

	if (!KHTTPHeaders::Parse(Stream))
	{
		// error is already set
		return false;
	}

	// analyze the forwarded-for family of headers
	KHTTPTrustedRemoteEndpoint::Analyze(Headers);

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
bool KHTTPRequestHeaders::SerializeRequestLine(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	if (!Resource.empty())
	{
		if (!GetRemoteEndpoint().empty())
		{
			kDebug(2, "{} http://{}{} {}",
					Method.Serialize(),
				   GetRemoteEndpoint().Serialize(),
					Resource.Serialize(),
					GetHTTPVersion());
			if (!Stream.FormatLine("{} http://{}{} {}",
								   Method.Serialize(),
								   GetRemoteEndpoint().Serialize(),
								   Resource.Serialize(),
								   GetHTTPVersion()))
			{
				return SetError("Cannot write request line");
			}
		}
		else
		{
			kDebug(2, "{} {} {}",
					Method.Serialize(),
					Resource.Serialize(),
					GetHTTPVersion());
			if (!Stream.FormatLine("{} {} {}",
								   Method.Serialize(),
								   Resource.Serialize(),
								   GetHTTPVersion()))
			{
				return SetError("Cannot write request line");
			}
		}
	}
	else
	{
		if (!GetRemoteEndpoint().empty() && Method == KHTTPMethod::CONNECT)
		{
			kDebug(2, "{} {} {}",
				    Method.Serialize(),
				    GetRemoteEndpoint().Serialize(),
				    GetHTTPVersion());
			// this is a CONNECT request
			if (!Stream.FormatLine("{} {} {}",
								   Method.Serialize(),
								   GetRemoteEndpoint().Serialize(),
								   GetHTTPVersion()))
			{
				return SetError("Cannot write request line");
			}
		}
		else
		{
			// special case, insert a single slash for the resource to
			// satisfy the HTTP protocol
			kDebug(2, "resource is empty, inserting /");
			kDebug(2, "{} / {}",
						Method.Serialize(),
						GetHTTPVersion());
			if (!Stream.FormatLine("{} / {}",
								   Method.Serialize(),
								   GetHTTPVersion()))
			{
				return SetError("Cannot write request line");
			}
		}
	}

	return true;

} // SerializeRequestLine

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	return SerializeRequestLine(Stream) &&
		KHTTPHeaders::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPRequestHeaders::HasChunking() const
//-----------------------------------------------------------------------------
{
	if (GetHTTPVersion() == KHTTPVersion::http10)
	{
		return false;
	}
	else
	{
		return true;
	}

} // HasChunking

//-----------------------------------------------------------------------------
KStringView KHTTPRequestHeaders::SupportedCompression() const
//-----------------------------------------------------------------------------
{
	KHTTPCompression Compression;

	// for compression we need to switch to chunked transfer, as we do not know
	// the size of the compressed content - however, chunking is only supported
	// since HTTP/1.1
	if (HasChunking())
	{
		// check the client's request headers for accepted compression encodings
		Compression = Headers.Get(KHTTPHeader::ACCEPT_ENCODING);
	}

	return Compression.Serialize();

} // SupportedCompression

//-----------------------------------------------------------------------------
void KHTTPRequestHeaders::clear()
//-----------------------------------------------------------------------------
{
	KHTTPHeaders::clear();
	KHTTPTrustedRemoteEndpoint::clear();
	Resource.clear();
	RequestLine.clear();

} // clear

//-----------------------------------------------------------------------------
bool KOutHTTPRequest::Serialize()
//-----------------------------------------------------------------------------
{
	if ((GetHTTPVersion() & (KHTTPVersion::http2 | KHTTPVersion::http3)) == 0)
	{
		// set up the chunked writer
		return KOutHTTPFilter::Parse(*this) && KHTTPRequestHeaders::Serialize(UnfilteredStream());
	}
	else
	{
		kDebug(1, "not a valid output path with HTTP/2");
		return false;
	}

} // Serialize

//-----------------------------------------------------------------------------
bool KInHTTPRequest::Parse()
//-----------------------------------------------------------------------------
{
	if ((GetHTTPVersion() & (KHTTPVersion::http2 | KHTTPVersion::http3)) == 0)
	{
		if (!KHTTPRequestHeaders::Parse(UnfilteredStream()))
		{
			return false;
		}
	}

	// analyze the headers for the filter chain
	return KInHTTPFilter::Parse(*this, 0, GetHTTPVersion());

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

DEKAF2_NAMESPACE_END

