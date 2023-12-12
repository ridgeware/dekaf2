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
#include "kstring.h"
#include "kctype.h"
#include "kbase64.h"
#include "ktime.h"
#include "kstringutils.h"

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


static_assert(std::is_nothrow_move_constructible<KHTTPHeader>::value,
			  "KHTTPHeader is intended to be nothrow move constructible, but is not!");

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

	while (Stream.ReadLine(sLine, MAX_LINELENGTH + 1))
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
	auto sValue = Headers.Get(KHTTPHeader::CONNECTION).ToLowerASCII();

	if (sHTTPVersion == "HTTP/1.0" || sHTTPVersion == "HTTP/0.9")
	{
		// close is default with HTTP < 1.1 - but we allow the client to override
		return sValue == "keep-alive" || sValue == "keepalive";
	}
	else
	{
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

DEKAF2_NAMESPACE_END
