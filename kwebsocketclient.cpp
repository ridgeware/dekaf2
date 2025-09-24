/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

#include "kwebsocketclient.h"
#include "khttperror.h"
#include "kstringutils.h"
#include "kwebsocket.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KWebSocketClient::KWebSocketClient(KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
// we can only use HTTP/1.1 for now
: KWebClient(Options.Unset(KStreamOptions::RequestHTTP2 | KStreamOptions::RequestHTTP3))
{
	// per default, allow proxy configuration through environment
	KHTTPClient::AutoConfigureProxy(true);
	KWebClient::AllowQueryToWWWFormConversion(false);
	KWebClient::RequestCompression(false);
	KWebClient::AllowCompression(false);

} // ctor

//-----------------------------------------------------------------------------
KWebSocketClient::KWebSocketClient(KURL URL, KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
: KWebSocketClient(Options)
{
	SetURL(std::move(URL), Options);

} // ctor

//-----------------------------------------------------------------------------
KWebSocketClient& KWebSocketClient::SetURL(KURL URL, KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
{
	m_URL = std::move(URL);

	// do not clear the query part - we will use it if it is set
	m_URL.Fragment.clear();

	KHTTPClient::SetStreamOptions(Options);

	return *this;

} // SetURL

//-----------------------------------------------------------------------------
void KWebSocketClient::clear()
//-----------------------------------------------------------------------------
{
	m_sPath.clear();
	m_Query.clear();
	base::clear();

} // clear

//-----------------------------------------------------------------------------
KWebSocketClient& KWebSocketClient::Path(KString sPath)
//-----------------------------------------------------------------------------
{
	m_sPath = std::move(sPath);
	return *this;

} // Path

//-----------------------------------------------------------------------------
KWebSocketClient& KWebSocketClient::SetQuery(url::KQuery Query)
//-----------------------------------------------------------------------------
{
	m_Query = std::move(Query);
	return *this;

} // SetQuery

//-----------------------------------------------------------------------------
KWebSocketClient& KWebSocketClient::SetBinary (bool bYesNo)
//-----------------------------------------------------------------------------
{
	m_bIsBinary = bYesNo;
	return *this;
}

//-----------------------------------------------------------------------------
KWebSocketClient& KWebSocketClient::AddHeader (KHTTPHeader Header, KStringView sValue)
//-----------------------------------------------------------------------------
{
	base::AddHeader(std::move(Header), sValue);
	return *this;

} // AddHeader


/* RFC 6455:

   The handshake from the client looks as follows:

        GET /chat HTTP/1.1
        Host: server.example.com
        Upgrade: websocket
        Connection: Upgrade
        Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
        Origin: http://example.com
        Sec-WebSocket-Protocol: chat, superchat
        Sec-WebSocket-Version: 13

   The handshake from the server looks as follows:

        HTTP/1.1 101 Switching Protocols
        Upgrade: websocket
        Connection: Upgrade
        Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
        Sec-WebSocket-Protocol: chat

 */
//-----------------------------------------------------------------------------
bool KWebSocketClient::Connect (KStringView sWebSocketProtocols)
//-----------------------------------------------------------------------------
{
	KString sError;
	KOutStringStream OutStream(sError);

	bool bResult { false };
	auto sClientSecKey = KWebSocket::GenerateClientSecKey();

	AddHeader(KHTTPHeader::UPGRADE               , "websocket"   );
	AddHeader(KHTTPHeader::CONNECTION            , "Upgrade"     );
	AddHeader(KHTTPHeader::SEC_WEBSOCKET_KEY     , sClientSecKey );
	AddHeader(KHTTPHeader::SEC_WEBSOCKET_VERSION , "13"          );

	if (!sWebSocketProtocols.empty())
	{
		AddHeader(KHTTPHeader::SEC_WEBSOCKET_PROTOCOL, sWebSocketProtocols );
	}

	if (m_URL.Protocol != url::KProtocol::UNIX)
	{
		KURL URL { m_URL };

		if (!m_sPath.empty())
		{
			if (m_sPath.front() != '/')
			{
				if (URL.Path.get().back() != '/')
				{
					URL.Path.get() += '/';
				}
			}

			URL.Path.get() += m_sPath;
		}

		if (!m_Query.empty())
		{
			URL.Query += m_Query;
		}
		
		bResult = KWebClient::HttpRequest(OutStream, URL, KHTTPMethod::GET);
	}
	else
	{
		// for unix socket connections we technically need two URLs,
		// one with the socket path (which is a file system path) and
		// one with the request path (which is a URL path)
		KURL RequestURL;
		RequestURL.Domain.get() = "localhost";
		RequestURL.Path.get() = '/';
		RequestURL.Path.get() += m_sPath;
		RequestURL.Query += m_Query;

		bResult = KWebClient::HttpRequest2Host(OutStream, m_URL, RequestURL, KHTTPMethod::GET);
	}

	if (!bResult)
	{
		if (!HasError())
		{
			if (sError.empty())
			{
				SetError("request failed");
			}
			else
			{
				SetError(sError);
			}
		}

		return false;
	}

	if (!KWebSocket::CheckForUpgradeResponse(sClientSecKey, sWebSocketProtocols, Response, WouldThrowOnError()))
	{
		return SetError("bad server response on upgrade request");
	}

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KWebSocketClient::Write(KInStream& stream, std::size_t len)
//-----------------------------------------------------------------------------
{
	class KWebSocket::Frame OutFrame;

	return OutFrame.Write(Request.UnfilteredStream(), true, stream, len);

} // Write

//-----------------------------------------------------------------------------
bool KWebSocketClient::Write(KString sBuffer)
//-----------------------------------------------------------------------------
{
	if (!sBuffer.empty() && Good())
	{
		class KWebSocket::Frame OutFrame(std::move(sBuffer), m_bIsBinary);

		if (!OutFrame.Write(Request.UnfilteredStream(), true))
		{
			return false;
		}
	}

	return true;

} // Write

//-----------------------------------------------------------------------------
std::istream::int_type KWebSocketClient::Read()
//-----------------------------------------------------------------------------
{
	if (!Good()                ||
	    !GetNextFrameIfEmpty() ||
	    m_sRXBuffer.empty())
	{
		return std::istream::traits_type::eof();
	}

	auto ch = m_sRXBuffer.front();
	m_sRXBuffer.remove_prefix(1);
	return ch;

} // Read

//-----------------------------------------------------------------------------
std::size_t KWebSocketClient::Read(KOutStream& stream, std::size_t len)
//-----------------------------------------------------------------------------
{
	std::size_t iRead { 0 };

	if (len > 0 && Good())
	{
		if (!GetNextFrameIfEmpty())
		{
			return 0;
		}

		auto iFragment = std::min(m_sRXBuffer.size(), len);

		if (!stream.Write(m_sRXBuffer.data(), iFragment))
		{
			return 0;
		}

		m_sRXBuffer.remove_prefix(iFragment);

		iRead += iFragment;
		len   -= iFragment;
	}

	return iRead;

} // Read

//-----------------------------------------------------------------------------
bool KWebSocketClient::GetNextFrameIfEmpty()
//-----------------------------------------------------------------------------
{
	if (m_sRXBuffer.empty())
	{
		if (!Good())
		{
			return false;
		}

		for(;;)
		{
			if (!m_RXFrame.Read(Response.UnfilteredStream(), Request.UnfilteredStream(), false))
			{
				return false;
			}

			// simply skip empty input frames
			if (!m_RXFrame.empty())
			{
				break;
			}
		}

		m_sRXBuffer = m_RXFrame.Payload();
	}

	return true;

} // GetNextFrameIfEmpty

//-----------------------------------------------------------------------------
std::size_t KWebSocketClient::Read(KStringRef& sBuffer, std::size_t len)
//-----------------------------------------------------------------------------
{
	std::size_t iRead { 0 };

	if (len > 0)
	{
		if (!GetNextFrameIfEmpty())
		{
			return 0;
		}

		auto iRead = std::min(m_sRXBuffer.size(), len);

		sBuffer.append(m_sRXBuffer.data(), iRead);

		m_sRXBuffer.remove_prefix(iRead);
	}

	return iRead;

} // Read

//-----------------------------------------------------------------------------
bool KWebSocketClient::ReadLine(KStringRef& sBuffer, std::size_t maxlen)
//-----------------------------------------------------------------------------
{
	// TODO
	return false;

} // ReadLine

DEKAF2_NAMESPACE_END
