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

#include <dekaf2/http/protocol/khttp_response.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN


//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::Parse(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	// A server may send any number of 1xx interim responses ahead of the final
	// response (RFC 9110 15.2), e.g. "103 Early Hints" before every response
	// from an origin behind Cloudflare. They are skipped here. 101 is not
	// interim, it is the final response to an upgrade request.
	static constexpr uint16_t iMaxInterimResponses = 16;

	// make sure we detect an empty header
	Stream.SetReaderRightTrim("\r\n");

	for (uint16_t iInterimResponse = 0; ; ++iInterimResponse)
	{
		KString sLine;

		if (!Stream.ReadLine(sLine))
		{
			if (iInterimResponse == 0)
			{
				// this is simply a read timeout, probably on a keep-alive
				// connection. Unset the error string and return false;
				return SetError("");
			}

			return SetError("read error after interim response");
		}

		if (sLine.empty())
		{
			return SetError("empty status line");
		}

		// analyze protocol and status
		// HTTP/1.1 200 Message with arbitrary words

		auto Words = sLine.Split(" ");

		if (Words.size() < 2)
		{
			// garbage, bail out
			return SetError("cannot read HTTP response status");
		}

		SetHTTPVersion(Words[0]);
		iStatusCode  = Words[1].UInt16();

		if (Words.size() > 2)
		{
			// this actually copies the reminder of the sLine
			// into m_sMessage. It looks dangerous but is absolutely
			// clean, as data() returns a pointer into sLine, which
			// itself is 0-terminated
			sStatusString.assign(Words[2].data());
		}
		else
		{
			sStatusString.clear();
		}

		if (GetHTTPVersion() == KHTTPVersion::none)
		{
			return SetError(kFormat("invalid HTTP version: {}", Words[0]));
		}

		// KHTTPHeaders::Parse() drops the headers of a preceding interim response
		if (!KHTTPHeaders::Parse(Stream))
		{
			return false;
		}

		if (iStatusCode / 100 != 1 || iStatusCode == KHTTPError::H1xx_SWITCHING_PROTOCOLS)
		{
			return true;
		}

		if (iInterimResponse >= iMaxInterimResponses)
		{
			return SetError(kFormat("more than {} interim responses", iMaxInterimResponses));
		}

		kDebug(2, "skipping interim response: {} {}", iStatusCode, sStatusString);
	}

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::Serialize(KOutStream& Stream) const
//-----------------------------------------------------------------------------
{
	if (GetHTTPVersion().empty())
	{
		return SetError("missing http version");
	}
	
	// the status text goes verbatim into the status line - a CR or LF in it would end
	// the line early and let the remainder pass as headers (response splitting). Such
	// a text (it may have been built from request data) is replaced by the canonical one
	KStringView sStatus = sStatusString;

	if (DEKAF2_UNLIKELY(sStatus.find('\n') != KStringView::npos || sStatus.find('\r') != KStringView::npos))
	{
		kDebug(1, "status text contains CR or LF - replaced by the default text for status {}", iStatusCode);
		sStatus = KHTTPError::GetStatusString(iStatusCode);
	}

	if (!Stream.FormatLine("{} {} {}", GetHTTPVersion(), iStatusCode, sStatus))
	{
		return SetError("Cannot write headers");
	}

	return KHTTPHeaders::Serialize(Stream);

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPResponseHeaders::HasChunking() const
//-----------------------------------------------------------------------------
{
	return Headers.Get(KHTTPHeader::TRANSFER_ENCODING) == "chunked";

} // HasChunking

//-----------------------------------------------------------------------------
void KHTTPResponseHeaders::clear()
//-----------------------------------------------------------------------------
{
	KHTTPHeaders::clear();
	sStatusString.clear();
	iStatusCode = 0;

} // clear

//-----------------------------------------------------------------------------
void KHTTPResponseHeaders::SetStatus(uint16_t iCode, KStringView sMessage)
//-----------------------------------------------------------------------------
{
	iStatusCode = KHTTPError::ConvertToRealStatusCode(iCode);

	if (sMessage.empty())
	{
		sStatusString = KHTTPError::GetStatusString(iCode);
	}
	else
	{
		sStatusString = sMessage;
	}

} // SetStatus

//-----------------------------------------------------------------------------
bool KOutHTTPResponse::Serialize()
//-----------------------------------------------------------------------------
{
	if ((GetHTTPVersion() & (KHTTPVersion::http2 | KHTTPVersion::http3)) == 0)
	{
		// set up the chunked writer
		return KOutHTTPFilter::Parse(*this) && KHTTPResponseHeaders::Serialize(UnfilteredStream());
	}
	else
	{
		kDebug(1, "not a valid output path with HTTP/2");
		return false;
	}

} // Serialize

//-----------------------------------------------------------------------------
bool KInHTTPResponse::Parse()
//-----------------------------------------------------------------------------
{
	if ((GetHTTPVersion() & (KHTTPVersion::http2 | KHTTPVersion::http3)) == 0)
	{
		if (!KHTTPResponseHeaders::Parse(UnfilteredStream()))
		{
			return false;
		}
	}

	// analyze the headers for the filter chain
	return  KInHTTPFilter::Parse(*this, iStatusCode, GetHTTPVersion());

} // Parse

//-----------------------------------------------------------------------------
bool KInHTTPResponse::Fail() const
//-----------------------------------------------------------------------------
{
	if (KInHTTPFilter::Fail())
	{
		// check if we have to set an appropriate error code, maybe
		// we had a timeout after already receiving a bad status code
		if (KHTTPResponseHeaders::GetStatusCode() == 0 || KHTTPResponseHeaders::Good())
		{
			// set a read error - we cast the const away..
			const_cast<KInHTTPResponse*>(this)->KHTTPResponseHeaders::SetStatus(KHTTPError::H5xx_READTIMEOUT, "NETWORK READ ERROR");
		}
	}

	return !KHTTPResponseHeaders::Good();

} // Fail

DEKAF2_NAMESPACE_END
