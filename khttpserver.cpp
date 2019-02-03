/*
//-----------------------------------------------------------------------------//
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

#include "khttpserver.h"

namespace dekaf2 {


//-----------------------------------------------------------------------------
void KHTTPServer::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	m_sError.clear();
	RemoteEndpoint.clear();
	m_bSetCompression = true;
}

//-----------------------------------------------------------------------------
KHTTPServer::KHTTPServer(KStream& Stream, KStringView sRemoteEndpoint)
//-----------------------------------------------------------------------------
{
	Accept(Stream, sRemoteEndpoint);

} // Ctor

//-----------------------------------------------------------------------------
bool KHTTPServer::Accept(KStream& Stream, KStringView sRemoteEndpoint)
//-----------------------------------------------------------------------------
{
	SetError(KStringView{});

	RemoteEndpoint = sRemoteEndpoint;

	Stream.SetReaderEndOfLine('\n');
	Stream.SetReaderLeftTrim("");
	Stream.SetReaderRightTrim("\r\n");
	Stream.SetWriterEndOfLine("\r\n");

	Response.SetOutputStream(Stream);
	Request.SetInputStream(Stream);

	return true;

} // Accept

//-----------------------------------------------------------------------------
void KHTTPServer::Disconnect()
//-----------------------------------------------------------------------------
{
	Response.close();
	Request.close();
}

//-----------------------------------------------------------------------------
bool KHTTPServer::Parse()
//-----------------------------------------------------------------------------
{
	Response.close();
	
	if (!Request.Parse())
	{
		kDebugLog (1, "KHTTPServer::Parse(): failed to parse incoming headers: {}", Request.Error());
		SetError(Request.Error());
		return false;
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPServer::Serialize()
//-----------------------------------------------------------------------------
{
	if (m_bSetCompression)
	{
		EnableCompressionIfPossible();
	}

	if (!Response.Serialize())
	{
		SetError(Response.Error());
		return false;
	}

	return true;

} // Serialize

//-----------------------------------------------------------------------------
KString KHTTPServer::GetBrowserIP() const
//-----------------------------------------------------------------------------
{
	KString sBrowserIP(Request.GetBrowserIP());

	if (sBrowserIP.empty())
	{
		// check our connection endpoint
		sBrowserIP = RemoteEndpoint;
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
	}

	return sBrowserIP;

} // GetBrowserIP

//-----------------------------------------------------------------------------
bool KHTTPServer::SetError(KStringView sError) const
//-----------------------------------------------------------------------------
{
	kDebug(1, "{}", sError);
	m_sError = sError;
	return false;

} // SetError

//-----------------------------------------------------------------------------
void KHTTPServer::EnableCompressionIfPossible()
//-----------------------------------------------------------------------------
{
	auto sCompression = Request.SupportedCompression();

	if (sCompression == "gzip")
	{
		Response.Headers.Set (KHTTPHeaders::CONTENT_ENCODING, "gzip");
	}
	else if (sCompression == "deflate")
	{
		Response.Headers.Set (KHTTPHeaders::CONTENT_ENCODING, "deflate");
	}
	else
	{
		return;
	}
	
	// for compression we need to switch to chunked transfer, as we do not know
	// the size of the compressed content
	Response.Headers.Set (KHTTPHeaders::TRANSFER_ENCODING, "chunked");
	// remove the content length
	Response.Headers.Remove (KHTTPHeaders::content_length);

} // EnableCompressionIfPossible

} // end of namespace dekaf2
