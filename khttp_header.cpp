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

#define DEKAF2_HTTP_HEADER_VIEW_PIPELINE (DEKAF2_HAS_CPP_20 && __cpp_lib_ranges >= 202110L)

#include "khttp_header.h"
#include "khttperror.h"
#include "kstring.h"
#include "kctype.h"
#include "kbase64.h"
#include "ktime.h"
#include "kstringutils.h"
#if !DEKAF2_HTTP_HEADER_VIEW_PIPELINE
	#include <boost/foreach.hpp>
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KString KHTTPHeader::DateToString(KUnixTime tTime)
//-----------------------------------------------------------------------------
{
	return kFormHTTPTimestamp(tTime);

} // DateToString

//-----------------------------------------------------------------------------
/// read a date from a string with HTTP date formatting
KUnixTime KHTTPHeader::StringToDate(KStringView sTime)
//-----------------------------------------------------------------------------
{
	return kParseHTTPTimestamp(sTime);
	
} // StringToDate

//-----------------------------------------------------------------------------
uint16_t KHTTPHeader::CalcQualityValue(KStringView sContent, KStringView::size_type iStartPos)
//-----------------------------------------------------------------------------
{
	if (iStartPos >= sContent.size())
	{
		return 100;
	}
	
	uint16_t iPercent { 0 };

	bool bInvalid { false };

	auto AddDigit = [&iPercent,&sContent,&bInvalid](KStringView::size_type iPos)
	{
		if (!bInvalid)
		{
			if (iPos < sContent.size())
			{
				auto ch = sContent[iPos];

				if (DEKAF2_UNLIKELY(!KASCII::kIsDigit(ch)))
				{
					bInvalid = true;
					iPercent = 100;
				}
				else
				{
					iPercent *= 10;
					iPercent += (ch - '0');
				}
			}
			else
			{
				iPercent *= 10;
			}
		}
	};

	auto CheckDot = [&iPercent,&sContent,&bInvalid](KStringView::size_type iPos)
	{
		if (!bInvalid)
		{
			if (iPos < sContent.size())
			{
				auto ch = sContent[iPos];

				if (DEKAF2_UNLIKELY(!(ch == '.' || ch == ',')))
				{
					bInvalid = true;
					iPercent = 100;
				}
			}
		}
	};

	AddDigit(iStartPos++);
	CheckDot(iStartPos++);
	AddDigit(iStartPos++);
	AddDigit(iStartPos);

	if (DEKAF2_UNLIKELY(iPercent > 100))
	{
		iPercent = 100;
	}

	return iPercent;

} // CalcQualityValue

//-----------------------------------------------------------------------------
std::vector<KHTTPHeader::Range> KHTTPHeader::GetRanges(KStringView sContent, std::uint64_t iResourceSize, bool bThrow)
//-----------------------------------------------------------------------------
{
	std::vector<Range> Ranges;

	// are there ranges?
	if (!sContent.empty() && iResourceSize)
	{
		auto iResourceEnd = iResourceSize - 1;

		if (sContent.remove_prefix("bytes="))
		{
			for (const auto vRange : sContent.Split(","))
			{
				auto rp = kSplitToPair(vRange, "-");

				uint64_t iStart { 0 };
				uint64_t iEnd   { 0 };

				if (rp.first.empty())
				{
					if (!rp.second.empty())
					{
						iEnd   = std::min(iResourceEnd, rp.second.UInt64());
						iStart = iResourceEnd - iEnd;
						iEnd   = iResourceEnd;
					}
					else
					{
						if (bThrow)
						{
							throw KHTTPError { KHTTPError::H4xx_RANGE_NOT_SATISFIABLE , kFormat("bad ranges: {}", sContent) };
						}
						return {{ 0, 0 }};
					}
				}
				else if (rp.second.empty())
				{
					iStart = rp.first.UInt64();
					iEnd   = iResourceEnd;
				}
				else
				{
					iStart = rp.first.UInt64();
					iEnd   = std::min(iResourceEnd, rp.second.UInt64());
				}

				if (iStart <= iEnd)
				{
					// now check for overlaps - we don't allow them
					for (auto& RG : Ranges)
					{
						if (RG.GetStart() <= iStart && RG.GetEnd() >= iStart)
						{
							kDebug(1, "have range overlap at start {}", iStart);

							if (bThrow)
							{
								throw KHTTPError { KHTTPError::H4xx_RANGE_NOT_SATISFIABLE , kFormat("overlapping ranges: {}", sContent) };
							}

							return {{ 0, 0 }};
						}
						else if (RG.GetStart() <= iEnd && RG.GetEnd() >= iEnd)
						{
							kDebug(1, "have range overlap at end {}", iEnd);

							if (bThrow)
							{
								throw KHTTPError { KHTTPError::H4xx_RANGE_NOT_SATISFIABLE , kFormat("overlapping ranges: {}", sContent) };
							}

							return {{ 0, 0 }};
						}
					}

					Ranges.emplace_back(iStart, iEnd);
				}
				else
				{
					kDebug(1, "start > end: {} {}", iStart, iEnd);

					if (bThrow)
					{
						throw KHTTPError { KHTTPError::H4xx_RANGE_NOT_SATISFIABLE , kFormat("start > end: {}", sContent) };
					}

					return {{ 0, 0 }};
				}
			}
		}
		else
		{
			kDebug(1, "bad range request: {}", sContent);

			if (bThrow)
			{
				throw KHTTPError { KHTTPError::H4xx_RANGE_NOT_SATISFIABLE , kFormat("bad ranges: {}", sContent) };
			}

			return {{ 0, 0 }};
		}
	}

	return Ranges;

} // GetRanges

static_assert(std::is_nothrow_move_constructible<KHTTPHeader>::value,
			  "KHTTPHeader is intended to be nothrow move constructible, but is not!");

//-----------------------------------------------------------------------------
bool KHTTPHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	// Continuation lines are no more allowed in HTTP headers, so we don't
	// care for them. Any broken header (like missing key, no colon) will
	// get dropped.
	// We also do not care for line endings and will canonify them on
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

	while (Stream.ReadLine(sLine, MAX_LINELENGTH + 1))
	{
		if (sLine.empty())
		{
			// end of header
			return true;
		}
		else if (sLine.size() > MAX_LINELENGTH)
		{
			return SetError(kFormat("HTTP {}header line too long: {} bytes", "", sLine.size()));
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
						return SetError(kFormat("HTTP {}header line too long: {} bytes",
												"continuation ",
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
		if (   !Stream.Write(iter.first.Serialize())
			|| !Stream.Write(": ")
			|| !Stream.WriteLine(iter.second))
		{
			return SetError("Cannot write headers");
		}
	}

	if (!Stream.WriteLine() ) // blank line indicates end of headers
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
bool KHTTPHeaders::HasContent(bool bForRequest) const
//-----------------------------------------------------------------------------
{
	auto iSize = ContentLength();

	if (iSize < 0)
	{
		if (Headers.Get(KHTTPHeader::TRANSFER_ENCODING).ToLowerASCII() == "chunked")
		{
			return true;
		}

		if (bForRequest)
		{
			// a request cannot be terminated by a connection close
			return false;
		}

		auto sValue = Headers.Get(KHTTPHeader::CONNECTION).ToLowerASCII();

		auto Values = sValue.Split();

		if (DEKAF2_LIKELY(GetHTTPVersion() != KHTTPVersion::http10))
		{
			return kStrIn("close", Values);
		}
		else
		{
			// "close" is the default with HTTP/1.0!
			return !kStrIn("keep-alive", Values);
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
	m_HTTPVersion.clear();
	m_sCharset.clear();
	m_sContentType.clear();
	ClearError();

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
const KMIME& KHTTPHeaders::ContentType() const
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
	auto sValue = Headers.Get(KHTTPHeader::CONNECTION).ToLowerASCII();

	// the CONNECTION header may have multiple values like "keep-alive, upgrade"
	auto Values = sValue.Split();

	if (GetHTTPVersion() == KHTTPVersion::http10)
	{
		// close is default with HTTP < 1.1 - but we allow the client to override
		return kStrIn("keep-alive", Values);
	}
	else
	{
		// keepalive is default with HTTP/1.1 and HTTP/2
		return !kStrIn("close", Values);
	}

} // HasKeepAlive

//-----------------------------------------------------------------------------
std::vector<KHTTPHeader::Range> KHTTPHeaders::GetRanges(uint64_t iResourceSize) const
//-----------------------------------------------------------------------------
{
	return KHTTPHeader::GetRanges(Headers.Get(KHTTPHeader::RANGE), iResourceSize);

} // GetRanges

//-----------------------------------------------------------------------------
KHTTPHeaders::BasicAuthParms KHTTPHeaders::GetBasicAuthParms() const
//-----------------------------------------------------------------------------
{
	return DecodeBasicAuthFromString(Headers.Get(KHTTPHeader::AUTHORIZATION));

} // GetBasicAuthParms

//-----------------------------------------------------------------------------
KHTTPHeaders::BasicAuthParms KHTTPHeaders::DecodeBasicAuthFromString(KStringView sInput)
//-----------------------------------------------------------------------------
{
	BasicAuthParms Parms;

	if (sInput.starts_with("Basic "))
	{
		auto sDecoded = KBase64::Decode(sInput.Mid(6));
		auto iPos = sDecoded.find(':');
		if (iPos != KString::npos)
		{
			Parms.sUsername = sDecoded.Left(iPos);
			Parms.sPassword = sDecoded.Mid(iPos+1);
		}
	}

	return Parms;

} // DecodeBasicAuthFromString


//-----------------------------------------------------------------------------
KString KHTTPTrustedRemoteEndpoint::GetConcatenatedHeaders(
	const KHTTPHeader&              HeaderName,
	const KHTTPHeaders::KHeaderMap& Headers
) noexcept
//-----------------------------------------------------------------------------
{
	KString sForwarded;

	auto ForwardHeaders = Headers.equal_range(HeaderName);

	// concatenate all found forwarded headers
	for (auto it = ForwardHeaders.first; it != ForwardHeaders.second; ++it)
	{
		KStringView sHeader = it->second;
		// removing the spaces here is not a security check,
		// it only helps to avoid tripping on empty headers.
		// we will check for empty comma sequences below
		sHeader.Trim(detail::kASCIISpacesSet);

		if (!sHeader.empty())
		{
			if (!sForwarded.empty())
			{
				sForwarded += ',';
			}

			sForwarded += sHeader.ToLowerASCII();
		}
	}

	return sForwarded;

} // KHTTPTrustedRemoteEndpoint::GetConcatenatedHeaders

//-----------------------------------------------------------------------------
uint16_t KHTTPTrustedRemoteEndpoint::GetAndRemovePortnumber(KStringView& sAddress) noexcept
//-----------------------------------------------------------------------------
{
	// 192.168.1.1       -> 0
	// 192.168.1.1:12345 -> 12345
	// [1234:5678:90ab:cdef:0123:4567:89ab:cdef]       -> 0
	// [1234:5678:90ab:cdef:0123:4567:89ab:cdef]:12345 -> 12345
	// the [] for IPv6 is required!
	uint16_t iPortnumber { 0 };
	uint16_t iFactor     { 1 };

	for (auto i = sAddress.size(); i-- > 0;)
	{
		auto ch = sAddress[i];

		if (ch == ']' || ch == ':')
		{
			auto iDelete = sAddress.size() - i;

			if (iDelete > 1)
			{
				sAddress.remove_suffix(iDelete);
			}

			return iPortnumber;
		}
		else if (ch == '.')
		{
			// this was the last byte of an IPv4
			return 0;
		}
		else if (!KASCII::kIsDigit(ch))
		{
			return 0;
		}
		else
		{
			if (sAddress.size() - i > 5)
			{
				return 0;
			}
			iPortnumber += (ch - '0') * iFactor;
			iFactor *= 10;
		}
	}

	return 0;

} // KHTTPTrustedRemoteEndpoint::GetAndRemovePortnumber

//-----------------------------------------------------------------------------
bool KHTTPTrustedRemoteEndpoint::IsTrustedProxy(const KIPAddress& IP, uint16_t iAddresses) const noexcept
//-----------------------------------------------------------------------------
{
	if (m_TrustedProxies && !m_TrustedProxies->empty())
	{
		// check if this IP is in our trusted proxy list
		for (const auto& Proxy : *m_TrustedProxies)
		{
			// the trusted proxy IP address may be a network like 10.12.1.0/24
			if (Proxy.Contains(IP))
			{
				return true;
			}
		}

		return false;
	}

	return m_iTrustedProxyCount > iAddresses;

} // KHTTPTrustedRemoteEndpoint::IsTrustedProxy

//-----------------------------------------------------------------------------
bool KHTTPTrustedRemoteEndpoint::Analyze(const KHTTPHeaders::KHeaderMap& Headers) noexcept
//-----------------------------------------------------------------------------
{
	KIPError   ec;
	KIPAddress RemoteIP (m_DirectEndpoint.Domain.Serialize(), ec);

	if (ec)
	{
		kDebug(1, "invalid IP of direct endpoint: {}", m_DirectEndpoint);
		return false;
	}

	// may be 0
	uint16_t iRemotePort = m_DirectEndpoint.Port;
	uint16_t iCount { 0 };

	// check if we trust our direct neighbour, else don't even
	// start to inspect the forwarded headers
	if (IsTrustedProxy(RemoteIP, iCount))
	{
		// first check for Forwarded: headers - they override the x-forwarded- family
		auto sForwarded = GetConcatenatedHeaders(KHTTPHeader::FORWARDED, Headers);

		if (!sForwarded.empty())
		{
			// for=12.34.56.78, for="[2001:db8:cafe::17]:12345", for=23.45.67.89:12345;secret=egah2CGj55fSJFs, for=10.1.2.3
			// there's also proto=, by=, host=
			auto Forwards = sForwarded.Split();

#if DEKAF2_HTTP_HEADER_VIEW_PIPELINE
			for (const auto& sForward : Forwards | std::views::reverse)
#else
			BOOST_REVERSE_FOREACH (const auto& sForward, Forwards)
#endif
			{
				if (sForward.empty())
				{
					// this string is empty.. abort here
					break;
				}
				else if (sForward.size() > 255)
				{
					// this string is way too long..
					break;
				}

				constexpr KFindSetOfChars BySemicolon(";");

				const auto Parts = sForward.Split<KMap<KStringView,KStringView>>(BySemicolon, "=");

				// now search for the for=
				auto sFor = Parts["for"];

				if (sFor.empty())
				{
					// no for= part - abort here
					break;
				}

				if (sFor.remove_prefix('"') && !sFor.remove_suffix('"'))
				{
					// bad double quoting - no trailing quote for lead
					break;
				}

				// we may have a :port here as well, see https://www.rfc-editor.org/rfc/rfc7239#section-6
				auto iPort = GetAndRemovePortnumber(sFor);

				KIPError ec;
				KIPAddress IP(sFor, ec);

				if (ec)
				{
					// invalid IP address
					kDebug(2, ec.what());
					break;
				}

				// we accept this tuple as valid
				++iCount;
				// save last IP as remote IP
				RemoteIP      = std::move(IP);
				// save last port as remote port
				iRemotePort   = iPort;
				// check if we have 'by'
				m_RemoteProxy = KIPAddress(Parts["by"], ec);
				// check if we have 'host'
				m_RemoteHost  = url::KDomain(Parts["host"]);
				// check if we have 'proto'
				m_RemoteProto = url::KProtocol(Parts["proto"]);

				// check if we would trust this last IP as our proxy
				if (!IsTrustedProxy(RemoteIP, iCount))
				{
					break;
				}
			}
		}
		// only inspect x-forwarded-for if there were no forwarded headers,
		// regardless of being trusted or valid
		else
		{
			// now check for x-forwarded-for: headers
			sForwarded = GetConcatenatedHeaders(KHTTPHeader::X_FORWARDED_FOR, Headers);

			if (sForwarded.empty())
			{
				// now check for x-proxyuser-ip: headers
				sForwarded = GetConcatenatedHeaders("x-proxyuser-ip", Headers);
			}

			if (!sForwarded.empty())
			{
				// do not delete the Forwarded-borne values (they contains the
				// direct neighbour's IP and its port)

				// x-forwarded-for: 12.34.56.78, 2001:db8:cafe::17,23.45.67.89,10.1.2.3
				auto Forwards = sForwarded.Split();

#if DEKAF2_HTTP_HEADER_VIEW_PIPELINE
				for (const auto& sForward : Forwards | std::views::reverse)
#else
				BOOST_REVERSE_FOREACH (const auto& sForward, Forwards)
#endif
				{
					if (sForward.empty())
					{
						// this string is empty.. abort here
						break;
					}
					else if (sForward.size() > 255)
					{
						// this string is way too long..
						break;
					}

					KIPError ec;
					KIPAddress IP(sForward, ec);

					if (ec)
					{
						// invalid IP address
						kDebug(2, ec.what());
						break;
					}

					// we accept this tuple as valid
					++iCount;
					// save last IP as remote IP
					RemoteIP    = std::move(IP);
					// delete the remote port, it is no more valid nor passed on by x-forwarded-for
					iRemotePort = 0;

					// shall we trust the x-forwarded-proto/x-forwarded-host headers?
					// we only take them for real if the last forwarder was a trusted proxy
					// (which means we would assume any trusted proxy to remove invalid

					// x-forwarded-proto/x-forwarded-host headers)
					// we concatenate same headers to trigger a failure if more than one header is present
					m_RemoteProto = url::KProtocol(GetConcatenatedHeaders(KHTTPHeader::X_FORWARDED_PROTO, Headers));
					m_RemoteHost  = url::KDomain  (GetConcatenatedHeaders(KHTTPHeader::X_FORWARDED_HOST , Headers));

					// check if we would trust this last IP as our proxy
					if (!IsTrustedProxy(RemoteIP, iCount))
					{
						break;
					}
				}
			}
		}
	}

	// build the remote endpoint from IP and port
	m_RemoteEndpoint = KTCPEndPoint(RemoteIP, iRemotePort);

	return true;

} // KHTTPTrustedRemoteEndpoint::Analyze

//-----------------------------------------------------------------------------
void KHTTPTrustedRemoteEndpoint::clear() noexcept
//-----------------------------------------------------------------------------
{
	m_DirectEndpoint.clear();
	m_RemoteEndpoint.clear();
	m_RemoteProto.clear();
	m_RemoteHost.clear();
	m_RemoteProxy.clear();
	m_TrustedProxies = nullptr;
	m_iTrustedProxyCount = 0;

} // KHTTPTrustedRemoteEndpoint::clear

DEKAF2_NAMESPACE_END
