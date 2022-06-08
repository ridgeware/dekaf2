/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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

#include "kwebsocket.h"
#include "kexception.h"
#include "ksystem.h"
#include "kencode.h"
#include "kmessagedigest.h"
#include "kcrashexit.h"
#include "klog.h"

namespace dekaf2 {
namespace kwebsocket {

namespace {

// this suffix is defined in RFC 6455
static constexpr KStringViewZ s_sWebsocket_sec_key_suffix = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

} // end of anonymous namespace

//-----------------------------------------------------------------------------
StreamType GetStreamType(KStream& Stream)
//-----------------------------------------------------------------------------
{
	{
		auto S = dynamic_cast<KSSLIOStream*>(&Stream);

		if (S)
		{
			return StreamType::TLS;
		}
	}

	{
		auto S = dynamic_cast<KTCPIOStream*>(&Stream);

		if (S)
		{
			return StreamType::TCP;
		}
	}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	{
		auto S = dynamic_cast<KUnixIOStream*>(&Stream);

		if (S)
		{
			return StreamType::UNIX;
		}
	}
#endif

	return StreamType::NONE;

} // GetStreamType

//-----------------------------------------------------------------------------
KTCPIOStream::asiostream&& GetAsioTCPStream (KStream& Stream)
//-----------------------------------------------------------------------------
{
	auto S = dynamic_cast<KTCPIOStream*>(&Stream);

	if (!S)
	{
		throw KWebSocketError("not a KTCPIOStream");
	}

	return std::move(S->GetAsioSocket());

} // GetAsioTCPStream

//-----------------------------------------------------------------------------
// this one _has_ to be an explicit RVALUE because of older GCCs (< 11)
KSSLIOStream::asiostream&& GetAsioTLSStream (KStream& Stream)
//-----------------------------------------------------------------------------
{
	auto S = dynamic_cast<KSSLIOStream*>(&Stream);

	if (!S)
	{
		throw KWebSocketError("not a KSSLIOStream");
	}

	return std::move(S->GetAsioSocket());

} // GetAsioTLSStream

#ifdef DEKAF2_HAS_UNIX_SOCKETS
//-----------------------------------------------------------------------------
KUnixIOStream::asiostream&& GetAsioUnixStream(KStream& Stream)
//-----------------------------------------------------------------------------
{
	auto S = dynamic_cast<KUnixIOStream*>(&Stream);

	if (!S)
	{
		throw KWebSocketError("not a KUnixIOStream");
	}

	return std::move(S->GetAsioSocket());

} // GetAsioUnixStream
#endif

//-----------------------------------------------------------------------------
bool FrameHeader::Decode(uint8_t byte)
//-----------------------------------------------------------------------------
{
/* from RFC 6455:

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+

 */
	switch (m_iFramePos & 0x7f)
	{
		case 0: // the start
			m_bIsFin     = (byte & 0x80);
			m_iExtension = (byte & 0x70) >> 4;
			m_Opcode     = static_cast<FrameType>((byte & 0x0f));
			break;

		case 1: // mask and first 7 length bits
			m_bMask       = (byte & 0x80);
			m_iPayloadLen = (byte & 0x7f);

			if (m_iPayloadLen < 126)
			{
				if (!m_bMask)
				{
					// we're done
					m_iFramePos = 0;
					return true;
				}
				else
				{
					m_iFramePos = 10; // jump to first mask byte
					return false;
				}
			}
			else
			{
				if (m_iPayloadLen == 127)
				{
					// high bit is our 64 bit flag
					m_iFramePos |= 0x80;
				}
			}
			break;

		case 2: // the first length byte
			m_iPayloadLen = byte;
			break;

		case 3: // the second length byte
			m_iPayloadLen <<= 8;
			m_iPayloadLen  |= byte;

			// check the 64 bit flag
			if ((m_iFramePos & 0x80) == 0)
			{
				if (!m_bMask)
				{
					// we're done
					m_iFramePos = 0;
					return true;
				}
				else
				{
					m_iFramePos = 10;
					return false;
				}
			}
			break;

			m_iPayloadLen <<= 8;
			m_iPayloadLen  &= byte;
			break;

		case 4: // the third   length byte
		case 5: // the fourth  length byte
		case 6: // the fifth   length byte
		case 7: // the sixth   length byte
		case 8: // the seventh length byte
			m_iPayloadLen <<= 8;
			m_iPayloadLen  |= byte;
			break;

		case 9: // the eigth length byte
			m_iPayloadLen <<= 8;
			m_iPayloadLen  |= byte;

			if (!m_bMask)
			{
				// we're done
				m_iFramePos = 0;
				return true;
			}
			break;

		case 10: // the first mask byte
			m_iMaskingKey = byte;
			break;

		case 11: // the second mask byte
		case 12: // the third  mask byte
			m_iMaskingKey <<= 8;
			m_iMaskingKey  |= byte;
			break;

		case 13: // the fourth mask byte
			m_iMaskingKey <<= 8;
			m_iMaskingKey  |= byte;
			// we're done
			m_iFramePos = 0;
			return true;

		default:
			throw KWebSocketError(kFormat("impossible frame pos: {}", m_iFramePos & 0x7f));
	}

	++m_iFramePos;
	return false;

} // Decode

//-----------------------------------------------------------------------------
KString FrameHeader::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sHeader;
	uint8_t byte;

	byte  = m_bIsFin ? 0x80 : 0;
	byte |= m_iExtension << 4;
	byte |= static_cast<uint8_t>(m_Opcode) & 0x0f;
	sHeader += byte;

	byte  = m_bMask ? 0x80 : 0;
	uint8_t iLen;
	if (m_iPayloadLen <= 125)
	{
		iLen = m_iPayloadLen;
	}
	else if (m_iPayloadLen < (1 << 16))
	{
		iLen = 126;
	}
	else
	{
		iLen = 127;
	}
	byte |= iLen & 0x7f;
	sHeader += byte;

	if (iLen == 127)
	{
		sHeader += m_iPayloadLen >> 56;
		sHeader += m_iPayloadLen >> 48;
		sHeader += m_iPayloadLen >> 40;
		sHeader += m_iPayloadLen >> 32;
		sHeader += m_iPayloadLen >> 24;
		sHeader += m_iPayloadLen >> 16;
	}
	if (iLen > 125)
	{
		sHeader += m_iPayloadLen >>  8;
		sHeader += m_iPayloadLen >>  0;
	}

	if (m_bMask)
	{
		sHeader += m_iMaskingKey >> 24;
		sHeader += m_iMaskingKey >> 16;
		sHeader += m_iMaskingKey >>  8;
		sHeader += m_iMaskingKey >>  0;
	}

	return sHeader;

} // Serialize

//-----------------------------------------------------------------------------
void Frame::Ping(KString sMessage)
//-----------------------------------------------------------------------------
{
	Binary(std::move(sMessage));
	m_Opcode = FrameType::Ping;

} // Ping

//-----------------------------------------------------------------------------
void Frame::Pong(KString sMessage)
//-----------------------------------------------------------------------------
{
	Binary(std::move(sMessage));
	m_Opcode = FrameType::Pong;

} // Pong

//-----------------------------------------------------------------------------
void Frame::Close()
//-----------------------------------------------------------------------------
{
	Binary("");
	m_Opcode = FrameType::Close;


} // Close

//-----------------------------------------------------------------------------
void Frame::XOR(KStringRef& sBuffer)
//-----------------------------------------------------------------------------
{
	std::array<char, 4> m
	{
		static_cast<char>(m_iMaskingKey >> 24),
		static_cast<char>(m_iMaskingKey >> 16),
		static_cast<char>(m_iMaskingKey >>  8),
		static_cast<char>(m_iMaskingKey >>  0)
	};

	auto iLen = sBuffer.size();

	for (std::size_t iPos = 0; iPos < iLen; ++iPos)
	{
		sBuffer[iPos] ^= m[iPos % 4];
	}

} // UnMask

//-----------------------------------------------------------------------------
void Frame::Mask()
//-----------------------------------------------------------------------------
{
	m_bMask       = true;
	m_iMaskingKey = kRandom();

	XOR(m_sPayload);

} // Mask

//-----------------------------------------------------------------------------
void Frame::UnMask()
//-----------------------------------------------------------------------------
{
	if (m_bMask)
	{
		XOR(m_sPayload);
		m_bMask = false;
	}

} // UnMask

//-----------------------------------------------------------------------------
void Frame::UnMask(KStringRef& sBuffer)
//-----------------------------------------------------------------------------
{
	if (m_bMask)
	{
		XOR(sBuffer);
		m_bMask = false;
	}

} // UnMask

//-----------------------------------------------------------------------------
void Frame::SetPayload(KString sPayload, bool bIsBinary)
//-----------------------------------------------------------------------------
{
	m_sPayload    = std::move(sPayload);
	m_iPayloadLen = m_sPayload.size();
	m_Opcode      = bIsBinary ? FrameType::Binary : FrameType::Text;
	m_iExtension  = 0;
	m_bIsFin      = true;
	m_bMask       = false;
	m_iMaskingKey = 0;

} // SetPayload

//-----------------------------------------------------------------------------
bool Frame::Read(KStream& Stream, bool bMaskTx)
//-----------------------------------------------------------------------------
{
	// buffer for the payload
	KString sBuffer;

	for(;;)
	{
		// read the header
		for (;!Decode(Stream.Read());) {}
		// read the payload
		auto iRead = Stream.Read(sBuffer, AnnouncedSize());
		// decode the incoming data
		UnMask(sBuffer);

		// check the type
		switch (Type())
		{
			case FrameType::Ping:
			{
				Frame Pong;
				Pong.Pong(sBuffer);
				Pong.Write(Stream, bMaskTx);
			}
			break;

			case FrameType::Pong:
			{
			}
			break;

			case FrameType::Continuation:
				m_sPayload += sBuffer;
				break;

			default:
				m_sPayload = std::move(sBuffer);
				break;
		}

		if (iRead != AnnouncedSize())
		{
			return false;
		}

		if (Finished())
		{
			return true;
		}

		// Stream.Read() appends to sBuffer, therefore let us clear it here
		sBuffer.clear();
	}

	return false;

} // Read

//-----------------------------------------------------------------------------
bool Frame::Write(KOutStream& OutStream, bool bMask)
//-----------------------------------------------------------------------------
{
	if (bMask)
	{
		Mask();
	}

	return OutStream.Write(FrameHeader::Serialize()) &&
			OutStream.Write(Payload());

} // Write

//-----------------------------------------------------------------------------
KString GenerateClientSecKey()
//-----------------------------------------------------------------------------
{
	KString sKey;

	for (uint16_t i = 0; i < 16 / 4; ++i)
	{
		auto iRand = kRandom();
		sKey += iRand;
		iRand >>= 8;
		sKey += iRand;
		iRand >>= 8;
		sKey += iRand;
		iRand >>= 8;
		sKey += iRand;
	}

	return KEncode::Base64(sKey);

} // GenerateClientSecKey

//-----------------------------------------------------------------------------
bool ClientSecKeyLooksValid(KStringView sSecKey, bool bThrowIfInvalid)
//-----------------------------------------------------------------------------
{
	if (sSecKey.size() != 24)
	{
		KString sError = kFormat("key size != 24: {}", sSecKey.size());
		kDebug(1, sError);

		if (bThrowIfInvalid)
		{
			throw KWebSocketError(sError);
		}

		return false;
	}

	if (sSecKey[22] != '=' || sSecKey[23] != '=')
	{
		KString sError = kFormat("key is not base64 of 16 input bytes: {}", sSecKey);
		kDebug(1, sError);

		if (bThrowIfInvalid)
		{
			throw KWebSocketError(sError);
		}

		return false;
	}

	return true;

} // ClientSecKeyLooksValid

//-----------------------------------------------------------------------------
KString GenerateServerSecKeyResponse(KString sSecKey, bool bThrowIfInvalid)
//-----------------------------------------------------------------------------
{
	if (ClientSecKeyLooksValid(sSecKey, bThrowIfInvalid))
	{
		KSHA1 SHA1(sSecKey);

		SHA1 += s_sWebsocket_sec_key_suffix;

		return KEncode::Base64(KDecode::Hex(SHA1.Digest()));
	}
	else
	{
		return KString();
	}

} // GenerateServerSecKeyResponse

//-----------------------------------------------------------------------------
bool CheckForWebSocketUpgrade(const KInHTTPRequest& Request, bool bThrowIfInvalid)
//-----------------------------------------------------------------------------
{
	if (Request.Headers.Get(KHTTPHeader::UPGRADE).ToLowerASCII() != "websocket")
	{
		return false;
	}

	auto Error = [bThrowIfInvalid](KStringViewZ sError) -> bool
	{
		kDebug(1, sError);

		if (bThrowIfInvalid)
		{
			throw KWebSocketError { sError };
		}

		return false;
	};

	if (Request.Method != KHTTPMethod::GET)
	{
		return Error("websockets require GET requests for session initiation");
	}

	if (Request.GetHTTPVersion() != "HTTP/1.1")
	{
		return Error("websockets require HTTP/1.1, not any earlier or later version");
	}

	if (Request.Headers.Get(KHTTPHeader::CONNECTION).ToLowerASCII() != "upgrade")
	{
		return Error("missing header: Connection: Upgrade");
	}

	if (Request.Headers.Get(KHTTPHeader::SEC_WEBSOCKET_VERSION).UInt16() != 13)
	{
		return Error(kFormat("wrong websocket version: {}", Request.Headers.Get(KHTTPHeader::SEC_WEBSOCKET_VERSION)));
	}

	kDebug(2, "client requests websocket protocol upgrade");

	return true;

} // CheckForWebSocketUpgrade

} // end of namespace kwebsocket

//-----------------------------------------------------------------------------
KWebSocket::KWebSocket(KStream& Stream, std::function<void(KWebSocket&)> WebSocketHandler)
//-----------------------------------------------------------------------------
: m_Stream(std::move(Stream))
, m_Handler(std::move(WebSocketHandler))
{
} // ctor

//-----------------------------------------------------------------------------
KWebSocketServer::KWebSocketServer()
//-----------------------------------------------------------------------------
{
	// start the executor thread
	m_Executor = std::make_unique<std::thread>([]()
	{

	});

} // ctor

//-----------------------------------------------------------------------------
KWebSocketServer::~KWebSocketServer()
//-----------------------------------------------------------------------------
{
	if (m_Executor)
	{
		// tell the thread to stop
		m_bStop = true;
		// and block until it is home
		m_Executor->join();
	}

} // dtor

//-----------------------------------------------------------------------------
KWebSocketServer::Handle KWebSocketServer::AddWebSocket(KWebSocket WebSocket)
//-----------------------------------------------------------------------------
{
	Handle handle { ++m_iLastID };
/*
	auto p = m_Connections.unique()->insert({handle, std::move(WebSocket)});

	if (!p.second)
	{
		throw KWebSocketError("cannot add websocket");
	}
*/
	return handle;

} // AddConnection

} // end of namespace dekaf2
