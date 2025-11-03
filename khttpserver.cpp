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

#include "khttpserver.h"
#include <utility>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void KHTTPServer::clear()
//-----------------------------------------------------------------------------
{
	// this clear is called once per new receive round - we therefore clear
	// everything that is not related to the connection itself
	ClearError();
	Request.clear();
	Response.clear();
	AllowCompression(true);
	// we also clear the authenticated user, as the connection may be proxied
	// and shared by multiple users
	m_sAuthenticatedUser.clear();

} // clear

//-----------------------------------------------------------------------------
KHTTPServer::KHTTPServer(KStream& Stream, KStringView sRemoteEndpoint, url::KProtocol Proto, uint16_t iPort)
//-----------------------------------------------------------------------------
{
	Accept(Stream, sRemoteEndpoint, std::move(Proto), iPort);

} // Ctor

//-----------------------------------------------------------------------------
bool KHTTPServer::Accept(KStream& Stream, KStringView sRemoteEndpoint, url::KProtocol Proto, uint16_t iPort)
//-----------------------------------------------------------------------------
{
	ClearError();

	RemoteEndpoint = sRemoteEndpoint;
	Protocol       = std::move(Proto);
	Port           = iPort;

	Stream.SetReaderEndOfLine('\n');
	Stream.SetReaderLeftTrim("");
	Stream.SetReaderRightTrim("\r\n");
	Stream.SetWriterEndOfLine("\r\n");

	Request.SetInputStream(Stream);
	Response.SetOutputStream(Stream);

	return Stream.istream().good() && Stream.ostream().good();

} // Accept

//-----------------------------------------------------------------------------
void KHTTPServer::Disconnect()
//-----------------------------------------------------------------------------
{
	Request.Reset();
	Response.Reset();
}

//-----------------------------------------------------------------------------
bool KHTTPServer::Parse()
//-----------------------------------------------------------------------------
{
	if (!Request.Parse())
	{
		if (!Request.Error().empty())
		{
			kDebug (1, "failed to parse incoming headers: {}", Request.Error());
		}
		else if (!Request.UnfilteredStream().Good())
		{
			kDebug (2, "input stream got closed");
		}
		SetError(Request.CopyLastError());
		return false;
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPServer::Serialize()
//-----------------------------------------------------------------------------
{
	if (m_bConfigureCompression)
	{
		EnableCompressionIfPossible();
	}

	if (!Response.Serialize())
	{
		SetError(Response.CopyLastError());
		return false;
	}

	return true;

} // Serialize

//-----------------------------------------------------------------------------
KString KHTTPServer::Read()
//-----------------------------------------------------------------------------
{
	KString sBuffer;
	Read(sBuffer);
	return sBuffer;

} // Read

//-----------------------------------------------------------------------------
KString KHTTPServer::GetConnectedClientIP() const
//-----------------------------------------------------------------------------
{
	// check our connection endpoint
	KString sConnectedClientIP = RemoteEndpoint;

	auto iColon = sConnectedClientIP.rfind(':');

	if (iColon != KString::npos)
	{
		// check if the colon is part of an IPv6 address,
		// or if it is host:port
		if (!iColon || sConnectedClientIP[iColon - 1] != ':')
		{
			sConnectedClientIP.erase(iColon);
		}
	}

	return sConnectedClientIP;

} // GetConnectedClientIP

//-----------------------------------------------------------------------------
uint16_t KHTTPServer::GetConnectedClientPort() const
//-----------------------------------------------------------------------------
{
	// check our connection endpoint
	auto iColon = RemoteEndpoint.rfind(':');

	if (iColon != KString::npos)
	{
		// check if the colon is part of an IPv6 address,
		// or if it is host:port
		if (!iColon || RemoteEndpoint[iColon - 1] != ':')
		{
			return RemoteEndpoint.Mid(iColon + 1).UInt16();
		}
	}

	return 0;

} // GetConnectedClientPort

//-----------------------------------------------------------------------------
KString KHTTPServer::GetRemoteIP() const
//-----------------------------------------------------------------------------
{
	auto sBrowserIP = Request.GetRemoteIP();

	if (sBrowserIP.empty())
	{
		sBrowserIP = GetConnectedClientIP();
	}

	return sBrowserIP;

} // GetRemoteIP

//-----------------------------------------------------------------------------
url::KProtocol KHTTPServer::GetRemoteProto() const
//-----------------------------------------------------------------------------
{
	auto Proto = Request.GetRemoteProto();

	if (Proto == url::KProtocol::UNDEFINED)
	{
		// we fall back to the direct connection protocol in any case
		Proto = Protocol;
	}

	return Proto;

} // GetRemoteProto

//-----------------------------------------------------------------------------
uint16_t KHTTPServer::GetRemotePort() const
//-----------------------------------------------------------------------------
{
	auto iPort = Request.GetRemotePort();

	if (iPort == 0)
	{
		if (Request.GetRemoteIP().empty())
		{
			// we only take the connected client port if
			// we do not have any proxying involved
			iPort = GetConnectedClientPort();
		}
	}

	return iPort;

} // GetRemotePort

//-----------------------------------------------------------------------------
const KString& KHTTPServer::GetRequestPath() const
//-----------------------------------------------------------------------------
{
	return Request.Resource.Path.get();
}

//-----------------------------------------------------------------------------
/// get one query parm value as a const string ref
const KString& KHTTPServer::GetQueryParm(KStringView sKey) const
//-----------------------------------------------------------------------------
{
	return GetQueryParms().Get(sKey);
}

//-----------------------------------------------------------------------------
KString KHTTPServer::GetQueryParm(KStringView sKey, KStringView sDefault) const
//-----------------------------------------------------------------------------
{
	KString sValue = GetQueryParm(sKey);

	if (sValue.empty())
	{
		sValue = sDefault;
	}

	return sValue;

} // GetQueryParm

//-----------------------------------------------------------------------------
void KHTTPServer::SetAuthenticatedUser(KString sAuthenticatedUser)
//-----------------------------------------------------------------------------
{
	if (kWouldLog(2))
	{
		if (!m_sAuthenticatedUser.empty())
		{
			kDebug(2, "removing old authenticated user: '{}'", m_sAuthenticatedUser);
		}
		if (!sAuthenticatedUser.empty())
		{
			kDebug(2, "setting new authenticated user:  '{}'", sAuthenticatedUser);
		}
	}
	m_sAuthenticatedUser = std::move(sAuthenticatedUser);

} // SetAuthenticatedUser

//-----------------------------------------------------------------------------
void KHTTPServer::EnableCompressionIfPossible()
//-----------------------------------------------------------------------------
{
	auto sCompression = Request.SupportedCompression();

	if (sCompression.empty())
	{
		return;
	}

	KMIME mime(Response.Headers.Get(KHTTPHeader::CONTENT_TYPE));

	if (!mime.IsCompressible())
	{
		kDebug(2, "MIME type {} is not compressible", mime.Serialize());
		return;
	}

	Response.Headers.Set (KHTTPHeader::CONTENT_ENCODING, sCompression);

	// for compression, we need to switch to chunked transfer, as we do not know
	// the size of the compressed content in advance
	Response.Headers.Set (KHTTPHeader::TRANSFER_ENCODING, "chunked");
	// remove the content length
	Response.Headers.Remove (KHTTPHeader::CONTENT_LENGTH);

} // EnableCompressionIfPossible

DEKAF2_NAMESPACE_END
