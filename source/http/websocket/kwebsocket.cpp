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

#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/core/errors/kcrashexit.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/ksplit.h>
#include <zlib.h>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN
namespace {

// this suffix is defined in RFC 6455
static constexpr KStringViewZ s_sWebsocket_sec_key_suffix = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// the empty DEFLATE block that terminates a Z_SYNC_FLUSH, per RFC 7692 stripped on send and appended on receive
static constexpr unsigned char s_DeflateTail[4] = { 0x00, 0x00, 0xFF, 0xFF };

// clamp a negotiated max_window_bits to the range zlib's raw deflate accepts (9..15)
uint8_t ClampWindowBits(uint8_t iBits)
{
	if (iBits <  9) { return  9; }
	if (iBits > 15) { return 15; }
	return iBits;
}

} // end of anonymous namespace

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct KWebSocketPMCE::Impl
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	z_stream Deflate {};
	z_stream Inflate {};
	bool     bDeflateInit { false };
	bool     bInflateInit { false };
	bool     bResetDeflate { false };  // reset our compressor after each message (our direction's no_context_takeover)

}; // Impl

//-----------------------------------------------------------------------------
KWebSocketPMCE::KWebSocketPMCE(Parameters Params, bool bServer)
//-----------------------------------------------------------------------------
: m_Params(std::move(Params))
, m_bServer(bServer)
, m_pImpl(std::make_unique<Impl>())
{
	// we compress with our own window; the peer inflates with a window >= ours
	auto iWindowBits = ClampWindowBits(m_bServer ? m_Params.iServerMaxWindowBits : m_Params.iClientMaxWindowBits);

	// reset our compressor per message if our direction negotiated no_context_takeover
	m_pImpl->bResetDeflate = m_bServer ? m_Params.bServerNoContextTakeover : m_Params.bClientNoContextTakeover;

	// negative windowBits selects raw deflate (no zlib header/trailer)
	if (deflateInit2(&m_pImpl->Deflate, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
	                 -static_cast<int>(iWindowBits), 8, Z_DEFAULT_STRATEGY) != Z_OK)
	{
		throw KWebSocketError("cannot initialize permessage-deflate compressor");
	}
	m_pImpl->bDeflateInit = true;

	// always inflate with the maximum window - it accepts any window the peer used
	if (inflateInit2(&m_pImpl->Inflate, -15) != Z_OK)
	{
		throw KWebSocketError("cannot initialize permessage-deflate decompressor");
	}
	m_pImpl->bInflateInit = true;

} // ctor

//-----------------------------------------------------------------------------
KWebSocketPMCE::~KWebSocketPMCE()
//-----------------------------------------------------------------------------
{
	if (m_pImpl)
	{
		if (m_pImpl->bDeflateInit) { deflateEnd(&m_pImpl->Deflate); }
		if (m_pImpl->bInflateInit) { inflateEnd(&m_pImpl->Inflate); }
	}

} // dtor

//-----------------------------------------------------------------------------
bool KWebSocketPMCE::Compress(KStringView sInput, KString& sOutput)
//-----------------------------------------------------------------------------
{
	auto& strm = m_pImpl->Deflate;

	strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(sInput.data()));
	strm.avail_in = static_cast<uInt>(sInput.size());

	std::array<char, 16384> Buffer;

	do
	{
		strm.next_out  = reinterpret_cast<Bytef*>(Buffer.data());
		strm.avail_out = static_cast<uInt>(Buffer.size());

		auto iResult = deflate(&strm, Z_SYNC_FLUSH);

		if (iResult != Z_OK && iResult != Z_BUF_ERROR)
		{
			kDebug(1, "deflate failed: {}", iResult);
			return false;
		}

		sOutput.append(Buffer.data(), Buffer.size() - strm.avail_out);
	}
	while (strm.avail_out == 0);

	// a Z_SYNC_FLUSH ends in the 4 byte empty block (0x00 0x00 0xFF 0xFF) - strip it (RFC 7692).
	// For an empty message zlib emits nothing at all; then the stripped result is empty and the
	// receiver, which re-appends the 4 bytes, would see an incomplete block. RFC 7692 §7.2.3.6
	// handles this by sending a single 0x00 byte (an empty uncompressed block header) so that the
	// receiver's re-appended 0x00 0x00 0xFF 0xFF completes a valid (harmless) empty block.
	if (sOutput.size() >= 4 &&
	    std::memcmp(sOutput.data() + sOutput.size() - 4, s_DeflateTail, 4) == 0)
	{
		sOutput.erase(sOutput.size() - 4);
	}

	if (sOutput.empty())
	{
		sOutput += '\0';
	}

	if (m_pImpl->bResetDeflate)
	{
		deflateReset(&strm);
	}

	return true;

} // Compress

//-----------------------------------------------------------------------------
bool KWebSocketPMCE::Decompress(KStringView sInput, KString& sOutput)
//-----------------------------------------------------------------------------
{
	auto& strm = m_pImpl->Inflate;

	// append the empty block that the sender stripped, then inflate the whole
	KString sCompressed { sInput };
	sCompressed.append(reinterpret_cast<const char*>(s_DeflateTail), 4);

	strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(sCompressed.data()));
	strm.avail_in = static_cast<uInt>(sCompressed.size());

	std::array<char, 16384> Buffer;

	for (;;)
	{
		strm.next_out  = reinterpret_cast<Bytef*>(Buffer.data());
		strm.avail_out = static_cast<uInt>(Buffer.size());

		auto iResult = inflate(&strm, Z_SYNC_FLUSH);

		if (iResult != Z_OK && iResult != Z_BUF_ERROR && iResult != Z_STREAM_END)
		{
			kDebug(1, "inflate failed: {}", iResult);
			return false;
		}

		sOutput.append(Buffer.data(), Buffer.size() - strm.avail_out);

		if (iResult == Z_STREAM_END || (strm.avail_in == 0 && strm.avail_out != 0))
		{
			break;
		}
	}

	// we intentionally never reset the inflate stream - keeping its context is correct
	// regardless of the peer's no_context_takeover setting

	return true;

} // Decompress

//-----------------------------------------------------------------------------
KWebSocketPMCE::Parameters KWebSocketPMCE::NegotiateServer(KStringView sClientOffer, const ServerConfig& Config, KString& sResponse)
//-----------------------------------------------------------------------------
{
	Parameters Params; // bEnabled defaults to false
	sResponse.clear();

	if (!Config.bEnable)
	{
		return Params;
	}

	// the client may offer several extensions, comma separated - find permessage-deflate
	for (auto sExtension : sClientOffer.Split(","))
	{
		auto Parts = sExtension.Split(";");

		if (Parts.empty() || Parts.front() != "permessage-deflate")
		{
			continue;
		}

		// read the offered parameters
		bool    bOfferedClientMaxWindow { false };
		uint8_t iOfferedClientMaxWindow { 15 };
		uint8_t iOfferedServerMaxWindow { 15 };
		bool    bOfferedClientNoContext { false };
		bool    bOfferedServerNoContext { false };

		for (std::size_t iPart = 1; iPart < Parts.size(); ++iPart)
		{
			auto Pair = kSplitToPair(Parts[iPart], "=");

			if      (Pair.first == "client_no_context_takeover")
			{
				bOfferedClientNoContext = true;
			}
			else if (Pair.first == "server_no_context_takeover")
			{
				bOfferedServerNoContext = true;
			}
			else if (Pair.first == "client_max_window_bits")
			{
				bOfferedClientMaxWindow = true;
				if (!Pair.second.empty())
				{
					iOfferedClientMaxWindow = Pair.second.UInt16();
				}
			}
			else if (Pair.first == "server_max_window_bits")
			{
				if (!Pair.second.empty())
				{
					iOfferedServerMaxWindow = Pair.second.UInt16();
				}
			}
		}

		// agree on the result
		Params.bEnabled                 = true;
		Params.bServerNoContextTakeover = Config.bServerNoContextTakeover || bOfferedServerNoContext;
		Params.bClientNoContextTakeover = Config.bClientNoContextTakeover || bOfferedClientNoContext;
		Params.iServerMaxWindowBits     = std::min<uint8_t>(Config.iServerMaxWindowBits, std::min<uint8_t>(iOfferedServerMaxWindow, 15));
		// we may only constrain the client window if the client offered the token
		Params.iClientMaxWindowBits     = bOfferedClientMaxWindow ? std::min<uint8_t>(iOfferedClientMaxWindow, 15) : 15;

		if (Params.iServerMaxWindowBits < 8) { Params.iServerMaxWindowBits = 8; }
		if (Params.iClientMaxWindowBits < 8) { Params.iClientMaxWindowBits = 8; }

		// build the response header value
		sResponse = "permessage-deflate";
		if (Params.bClientNoContextTakeover)              { sResponse += "; client_no_context_takeover"; }
		if (Params.bServerNoContextTakeover)              { sResponse += "; server_no_context_takeover"; }
		if (Params.iServerMaxWindowBits < 15)             { sResponse += kFormat("; server_max_window_bits={}", Params.iServerMaxWindowBits); }
		if (bOfferedClientMaxWindow && Params.iClientMaxWindowBits < 15)
		                                                  { sResponse += kFormat("; client_max_window_bits={}", Params.iClientMaxWindowBits); }

		return Params;
	}

	return Params;

} // NegotiateServer

//-----------------------------------------------------------------------------
KString KWebSocketPMCE::BuildClientOffer(bool bClientNoContextTakeover, bool bServerNoContextTakeover)
//-----------------------------------------------------------------------------
{
	KString sOffer = "permessage-deflate";

	// advertise that we accept a server-chosen client window (lets the server cap our memory)
	sOffer += "; client_max_window_bits";

	if (bClientNoContextTakeover)
	{
		sOffer += "; client_no_context_takeover";
	}
	if (bServerNoContextTakeover)
	{
		sOffer += "; server_no_context_takeover";
	}

	return sOffer;

} // BuildClientOffer

//-----------------------------------------------------------------------------
KWebSocketPMCE::Parameters KWebSocketPMCE::ParseServerResponse(KStringView sServerResponse)
//-----------------------------------------------------------------------------
{
	Parameters Params;

	for (auto sExtension : sServerResponse.Split(","))
	{
		auto Parts = sExtension.Split(";");

		if (Parts.empty() || Parts.front() != "permessage-deflate")
		{
			continue;
		}

		Params.bEnabled = true;

		for (std::size_t iPart = 1; iPart < Parts.size(); ++iPart)
		{
			auto Pair = kSplitToPair(Parts[iPart], "=");

			if      (Pair.first == "client_no_context_takeover")
			{
				Params.bClientNoContextTakeover = true;
			}
			else if (Pair.first == "server_no_context_takeover")
			{
				Params.bServerNoContextTakeover = true;
			}
			else if (Pair.first == "client_max_window_bits" && !Pair.second.empty())
			{
				Params.iClientMaxWindowBits = Pair.second.UInt16();
			}
			else if (Pair.first == "server_max_window_bits" && !Pair.second.empty())
			{
				Params.iServerMaxWindowBits = Pair.second.UInt16();
			}
		}

		return Params;
	}

	return Params;

} // ParseServerResponse

//-----------------------------------------------------------------------------
void KWebSocket::FrameHeader::clear()
//-----------------------------------------------------------------------------
{
	*this = FrameHeader();

} // clear

//-----------------------------------------------------------------------------
void KWebSocket::FrameHeader::SetOpcodeAndFin(FrameType Opcode, bool bIsFin)
//-----------------------------------------------------------------------------
{
	m_Opcode = Opcode;
	m_bIsFin = bIsFin;

} // SetOpcode

//-----------------------------------------------------------------------------
bool KWebSocket::FrameHeader::Decode(uint8_t byte)
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

	auto AdjustLen = [this]()
	{
		// if we have an encoder we first read the whole payload to decode it
		// and later split it in preamble and data, if needed
		if (!m_bHaveEncoder && (m_Opcode == FrameType::Text || m_Opcode == FrameType::Binary))
		{
			auto iPreambleSize = GetPreambleSize();

			if (m_iPayloadLen >= iPreambleSize)
			{
				m_iPayloadLen -= iPreambleSize;
			}
			else
			{
				kDebug(1, "preamble len {} > payload len {}", iPreambleSize, m_iPayloadLen);
				m_iPayloadLen = 0;
			}
		}
	};

	switch (m_iFramePos & 0x7f)
	{
		case 0: // the start
			m_bIsFin     = (byte & 0x80);
			m_iExtension = (byte & 0x70) >> 4;
			m_Opcode     = static_cast<FrameType>((byte & 0x0f));
			break;

		case 1: // mask flag and first 7 length bits
			m_bMask       = (byte & 0x80);
			m_iPayloadLen = (byte & 0x7f);

			if (m_iPayloadLen < 126)
			{
				if (!m_bMask)
				{
					// we're done
					m_iFramePos = 0;
					AdjustLen();
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
					AdjustLen();
					return true;
				}
				else
				{
					m_iFramePos = 10;
					return false;
				}
			}
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
				AdjustLen();
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
			AdjustLen();
			return true;

		default:
			throw KWebSocketError(kFormat("impossible frame pos: {}", m_iFramePos & 0x7f));
	}

	++m_iFramePos;
	return false;

} // Decode

//-----------------------------------------------------------------------------
bool KWebSocket::FrameHeader::Read(KInStream& Stream)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		if (!Stream.Good())
		{
			return false;
		}
		if (Decode(Stream.Read()))
		{
			return true;
		}
	}

} // Read

//-----------------------------------------------------------------------------
std::size_t KWebSocket::FrameHeader::GetPreambleSize() const
//-----------------------------------------------------------------------------
{
	return 0;
}

//-----------------------------------------------------------------------------
char* KWebSocket::FrameHeader::GetPreambleBuf() const
//-----------------------------------------------------------------------------
{
	return nullptr;
}

//-----------------------------------------------------------------------------
KStringView KWebSocket::FrameHeader::FrameTypeToString(FrameType Type)
//-----------------------------------------------------------------------------
{
	switch (Type)
	{
		case FrameType::Continuation: return "Continuation";
		case FrameType::Text:         return "Text";
		case FrameType::Binary:       return "Binary";
		case FrameType::Close:        return "Close";
		case FrameType::Ping:         return "Ping";
		case FrameType::Pong:         return "Pong";
	}

	return "";
}

//-----------------------------------------------------------------------------
KStringView KWebSocket::FrameHeader::StatusCodeToString(uint16_t iStatusCode)
//-----------------------------------------------------------------------------
{
	switch (iStatusCode)
	{
		case 1000: return "Normal Closure";
		case 1001: return "Going Away";
		case 1002: return "Protocol Error";
		case 1003: return "Unsupported Data";
		case 1004: return "Reserved";
		case 1005: return "No Status Received";
		case 1006: return "Abnormal Closure";
		case 1007: return "Invalid Payload Data";
		case 1008: return "Policy Violation";
		case 1009: return "Message Too Big";
		case 1010: return "Mandatory Extension";
		case 1011: return "Internal Server Error";
		case 1015: return "TLS handshake"; // may not appear in a Close frame
	}

	return "unknown status";
}

//-----------------------------------------------------------------------------
KString KWebSocket::FrameHeader::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sHeader;
	uint8_t byte;

	byte  = m_bIsFin ? 0x80 : 0;
	byte |= m_iExtension << 4;
	byte |= static_cast<uint8_t>(m_Opcode) & 0x0f;
	sHeader += byte;

	auto iTotalPayloadLen = m_iPayloadLen;

	// if we have an encoder the complete content is now sitting in the
	// payload buffer even if there was/is a preamble
	if (!m_bHaveEncoder && (m_Opcode == FrameType::Text || m_Opcode == FrameType::Binary))
	{
		iTotalPayloadLen += GetPreambleSize();
	}

	uint8_t iLen;

	if (iTotalPayloadLen <= 125)
	{
		iLen = iTotalPayloadLen;
	}
	else if (iTotalPayloadLen < (1 << 16))
	{
		iLen = 126;
	}
	else
	{
		iLen = 127;
	}

	byte  = m_bMask ? 0x80 : 0;
	byte |= iLen & 0x7f;
	sHeader += byte;

	if (iLen == 127)
	{
		sHeader += iTotalPayloadLen >> 56;
		sHeader += iTotalPayloadLen >> 48;
		sHeader += iTotalPayloadLen >> 40;
		sHeader += iTotalPayloadLen >> 32;
		sHeader += iTotalPayloadLen >> 24;
		sHeader += iTotalPayloadLen >> 16;
	}
	if (iLen > 125)
	{
		sHeader += iTotalPayloadLen >>  8;
		sHeader += iTotalPayloadLen >>  0;
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
bool KWebSocket::FrameHeader::Write(KOutStream& Stream)
//-----------------------------------------------------------------------------
{
	return Stream.Write(Serialize()).Good();

} // Write

//-----------------------------------------------------------------------------
void KWebSocket::FrameHeader::SetOpcodeAndFin(bool bIsBinary, bool bIsContinuation, bool bIsFin)
//-----------------------------------------------------------------------------
{
	SetOpcodeAndFin(bIsContinuation ? FrameType::Continuation : bIsBinary ? FrameType::Binary : FrameType::Text, bIsFin);

} // SetOpcode

//-----------------------------------------------------------------------------
void KWebSocket::FrameHeader::XOR(void* pBuffer, std::size_t iSize) const
//-----------------------------------------------------------------------------
{
	if (iSize > 0)
	{
		kDebug(3, "applying mask {:#08x} on buffer of size {}", m_iMaskingKey, iSize);

		std::array<uint8_t, 4> Mask
		{
			static_cast<uint8_t>(m_iMaskingKey >> 24),
			static_cast<uint8_t>(m_iMaskingKey >> 16),
			static_cast<uint8_t>(m_iMaskingKey >>  8),
			static_cast<uint8_t>(m_iMaskingKey >>  0)
		};

		uint8_t* pBuf = static_cast<uint8_t*>(pBuffer);

		for (; iSize >= 4; iSize -= 4)
		{
			*pBuf++ ^= Mask[0];
			*pBuf++ ^= Mask[1];
			*pBuf++ ^= Mask[2];
			*pBuf++ ^= Mask[3];
		}

		switch (iSize)
		{
			case 3:
				*pBuf++ ^= Mask[0];
				*pBuf++ ^= Mask[1];
				*pBuf++ ^= Mask[2];
				break;

			case 2:
				*pBuf++ ^= Mask[0];
				*pBuf++ ^= Mask[1];
				break;

			case 1:
				*pBuf++ ^= Mask[0];
				break;

			default:
				break;
		}
	}

} // XOR

//-----------------------------------------------------------------------------
KWebSocket::FrameHeader::~FrameHeader()
//-----------------------------------------------------------------------------
{
} // virtual dtor

//-----------------------------------------------------------------------------
void KWebSocket::Frame::clear()
//-----------------------------------------------------------------------------
{
	*this = Frame();

} // clear

//-----------------------------------------------------------------------------
void KWebSocket::Frame::Text(KString sText)
//-----------------------------------------------------------------------------
{
	SetOpcodeAndFin(FrameType::Text);
	SetPayload(std::move(sText));

} // Text

//-----------------------------------------------------------------------------
void KWebSocket::Frame::Binary(KString sBuffer)
//-----------------------------------------------------------------------------
{
	SetOpcodeAndFin(FrameType::Binary);
	SetPayload(std::move(sBuffer));

} // Binary

//-----------------------------------------------------------------------------
void KWebSocket::Frame::Ping(KString sMessage)
//-----------------------------------------------------------------------------
{
	SetOpcodeAndFin(FrameType::Ping);
	SetPayload(std::move(sMessage));

} // Ping

//-----------------------------------------------------------------------------
void KWebSocket::Frame::Pong(KString sMessage)
//-----------------------------------------------------------------------------
{
	SetOpcodeAndFin(FrameType::Pong);
	SetPayload(std::move(sMessage));

} // Pong

//-----------------------------------------------------------------------------
void KWebSocket::Frame::Close(uint16_t iStatusCode, KString sReason)
//-----------------------------------------------------------------------------
{
	SetOpcodeAndFin(FrameType::Close);
	// make room for status code - see RFC 6455 for format
	sReason.insert(0, 2, ' ');
	sReason[0] = iStatusCode / 256;
	sReason[1] = iStatusCode % 256;
	SetPayload(std::move(sReason));

} // Close

//-----------------------------------------------------------------------------
void KWebSocket::Frame::Mask()
//-----------------------------------------------------------------------------
{
	SetHasMask();
	SetMaskingKey(kRandom32());
	XOR(m_sPayload);

} // Mask

//-----------------------------------------------------------------------------
void KWebSocket::Frame::UnMask(KStringRef& sBuffer)
//-----------------------------------------------------------------------------
{
	if (IsMaskedRx())
	{
		XOR(sBuffer);
		SetHasMask(false);
	}

} // UnMask

//-----------------------------------------------------------------------------
void KWebSocket::Frame::SetPayload(KString sPayload)
//-----------------------------------------------------------------------------
{
	m_sPayload = std::move(sPayload);
	SetPayloadLen(m_sPayload.size());

} // SetPayload

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::DecodeAndSplitPreamble(KStringView sBuffer)
//-----------------------------------------------------------------------------
{
	// let payload grow and append to the end - this happens when a
	// continuation frame follows a text or binary frame
	if (!Decode(sBuffer, m_sPayload))
	{
		return false;
	}

	if (Type() == FrameType::Text || Type() == FrameType::Binary)
	{
		// get the preamble if existing
		auto iPreambleSize = GetPreambleSize();

		if (iPreambleSize)
		{
			if (iPreambleSize % 4 != 0)
			{
				kDebug(1, "the size of the preamble ({}) must be a multiple of 4 but is not", iPreambleSize);
				return false;
			}
			else if (iPreambleSize > m_sPayload.size())
			{
				kDebug(1, "preamble not entirely in received frame");
				return false;
			}

			auto pPreambleBuf = GetPreambleBuf();

			if (pPreambleBuf)
			{
				std::memcpy(pPreambleBuf, &m_sPayload[0], iPreambleSize);
				m_sPayload.erase(0, iPreambleSize);
			}
		}
	}

	return true;

} // DecodeAndSplitPreamble

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::TryDecode()
//-----------------------------------------------------------------------------
{
	KString sTemp = GetPreamble();
	sTemp += m_sPayload;

	m_sPayload.clear();

	if (!DecodeAndSplitPreamble(sTemp))
	{
		m_sPayload = sTemp.Mid(GetPreambleSize());
		return false;
	}

	return true;

} // TryDecode

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::ReadPreambleFromStream(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	switch (Type())
	{
		case FrameType::Text:
		case FrameType::Binary:
		{
			// read the preamble, if any
			auto iPreambleSize = GetPreambleSize();

			if (iPreambleSize)
			{
				if (iPreambleSize % 4 != 0)
				{
					kDebug(1, "the size of the preamble ({}) must be a multiple of 4 but is not", iPreambleSize);
					return false;
				}

				auto pPreambleBuf = GetPreambleBuf();

				if (pPreambleBuf)
				{
					auto iRead = InStream.Read(pPreambleBuf, iPreambleSize);

					if (iRead != iPreambleSize)
					{
						return false;
					}

					if (IsMaskedRx())
					{
						XOR(pPreambleBuf, iPreambleSize);
					}
				}
				else
				{
					kDebug(1, "no preamble read buffer, but have a preamble of size {}", iPreambleSize);
					return false;
				}
			}
		}
		break;

		default:
			break;
	}

	return true;

} // ReadPreambleFromStream

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::Write(KOutStream& OutStream, bool bMaskTx, KInStream& Payload, bool bIsBinary, std::size_t len)
//-----------------------------------------------------------------------------
{
	bool bIsLast   = { false };
	bool bIsFirst  = {  true };

	for (;!bIsLast;)
	{
		auto iFragment = std::min(len, KDefaultCopyBufSize);
		auto iRead = Payload.Read(m_sPayload, iFragment);
		SetPayloadLen(iRead);
		len -= iRead;
		bIsLast = len == 0 || Payload.istream().eof();
		SetOpcodeAndFin(bIsBinary, !bIsFirst, bIsLast);

		if (!Write(OutStream, bMaskTx))
		{
			return false;
		}

		m_sPayload.clear();
		bIsFirst = false;
	}

	return true;

} // Write

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::Read(KInStream& InStream, KOutStream& OutStream, bool bMaskTx)
//-----------------------------------------------------------------------------
{
	// buffer for the payload
	KString   sBuffer;
	// make sure the payload is empty
	m_sPayload.clear();

	for(;;)
	{
		// read the header
		if (!FrameHeader::Read(InStream))
		{
			return false;
		}

		// remember whether the first data frame of this message was compressed (RSV1),
		// continuation frames carry RSV1=0 and must not clear it
		if (Type() == FrameType::Text || Type() == FrameType::Binary)
		{
			m_bMessageCompressed = GetRSV1();
		}

		if (!GetHaveEncoder())
		{
			// without encoder, read the preamble directly from the stream
			if (!ReadPreambleFromStream(InStream))
			{
				return false;
			}
		}

		// read the payload
		auto iRead = InStream.Read(sBuffer, AnnouncedSize());

		if (iRead != AnnouncedSize())
		{
			kDebug(1, "cannot read {} bytes of payload, got only {}", AnnouncedSize(), iRead);
			return false;
		}

		kDebug(3, "read {} bytes of {}payload for frame type {}",
		           iRead,
		           IsMaskedRx() ? "masked " : "",
		           Frame::FrameTypeToString(Type()));

		// decode the incoming data, ignore already received parts
		UnMask(sBuffer);

		if (GetEncodeFrame())
		{
			// if this frame had been encoded, decode it into the payload,
			// and finally split into preamble and payload
			if (!DecodeAndSplitPreamble(sBuffer))
			{
				return false;
			}
		}
		else
		{
			// if the frame was not encoded ..
			if (Type() != FrameType::Continuation)
			{
				// either just move the received data into the payload
				m_sPayload = std::move(sBuffer);
			}
			else
			{
				// or append it if this is a continuation
				m_sPayload += sBuffer;
			}
		}

		// check the type and do some postprocessing
		switch (Type())
		{
			case FrameType::Ping:
			{
				Frame Pong;
				Pong.Pong(GetPayload());
				Pong.Write(OutStream, bMaskTx);
			}
			break;

			case FrameType::Pong:
				break;

			case FrameType::Close:
				{
					if (m_sPayload.size() > 1)
					{
						// get first two bytes for an error code
						SetStatusCode(static_cast<uint8_t>(m_sPayload[0]) * 256
						            + static_cast<uint8_t>(m_sPayload[1]));
						m_sPayload.erase(0, 2);
						kDebug(2, "connection closed with status code {} ({}){}{}",
						          GetStatusCode(), StatusCodeToString(GetStatusCode()),
						          m_sPayload.empty() ? "" : ": ",
						          m_sPayload);
					}
					else
					{
						kDebug(2, "connection closed without status code");
						SetStatusCode(Status::NoStatusReceived);
					}

					// reply with close and the same payload
					Frame Close;
					Close.Close(GetStatusCode(), GetPayload());
					Close.Write(OutStream, bMaskTx);
				}
				return false;

			case FrameType::Continuation:
			case FrameType::Text:
			case FrameType::Binary:
				break;
		}

		if (Finished())
		{
			return true;
		}

		sBuffer.clear();
	}

} // Read

//-----------------------------------------------------------------------------
bool KWebSocket::FrameHeader::GetEncodeFrame() const
//-----------------------------------------------------------------------------
{
	// returns true if we have an active encoder and the frame type is NOT Close
	return GetHaveEncoder() && Type() != FrameType::Close;

} // EncodeFrame

//-----------------------------------------------------------------------------
KWebSocket::Frame::Frame(FrameType Opcode, KString sPayload, bool bIsFin)
//-----------------------------------------------------------------------------
{
	SetOpcodeAndFin(Opcode, bIsFin);
	SetPayload(std::move(sPayload));

} // ctor

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::Write(KOutStream& OutStream, bool bMask)
//-----------------------------------------------------------------------------
{
	if (GetEncodeFrame())
	{
		KString sTemp = GetPreamble();

		if (sTemp.empty())
		{
			sTemp = std::move(m_sPayload);
		}
		else
		{
			sTemp += m_sPayload;
		}

		m_sPayload.clear();

		if (!Encode(sTemp, m_sPayload))
		{
			return false;
		}

		// adjust payload size
		SetPayloadLen(GetPayload().size());
	}

	if (bMask)
	{
		Mask();
	}

	if (!FrameHeader::Write(OutStream)) return false;

	if (!GetHaveEncoder() && (Type() == FrameType::Text || Type() == FrameType::Binary))
	{
		auto iPreambleSize = GetPreambleSize();

		if (iPreambleSize)
		{
			if (iPreambleSize % 4 != 0)
			{
				kDebug(1, "the size of the preamble ({}) must be a multiple of 4 but is not", iPreambleSize);
				return false;
			}

			auto pPreambleBuf = GetPreambleBuf();

			if (!pPreambleBuf)
			{
				kDebug(1, "no preamble read buffer, but have a preamble of size {}", iPreambleSize);
				return false;
			}

			if (bMask)
			{
				XOR(pPreambleBuf, iPreambleSize);
			}

			if (!OutStream.Write(pPreambleBuf, iPreambleSize))
			{
				kDebug(1, "cannot write preamble of size {}", iPreambleSize);
				return false;
			}
		}
	}

	kDebug(3, "writing {} bytes for frame type {}", size(), FrameTypeToString(Type()));
	return OutStream.Write(GetPayload()) && OutStream.Flush();

} // Write

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::Encode(KStringView sInput, KString& sEncoded)
//-----------------------------------------------------------------------------
{
	return false;
}

//-----------------------------------------------------------------------------
bool KWebSocket::Frame::Decode(KStringView sEncoded, KString& sDecoded)
//-----------------------------------------------------------------------------
{
	return false;
}

//-----------------------------------------------------------------------------
KString KWebSocket::GenerateClientSecKey()
//-----------------------------------------------------------------------------
{
	return KEncode::Base64(kGetRandom(16));

} // GenerateClientSecKey

//-----------------------------------------------------------------------------
bool KWebSocket::ClientSecKeyLooksValid(KStringView sSecKey, bool bThrowIfInvalid)
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
KString KWebSocket::GenerateServerSecKeyResponse(KString sSecKey, bool bThrowIfInvalid)
//-----------------------------------------------------------------------------
{
	if (ClientSecKeyLooksValid(sSecKey, bThrowIfInvalid))
	{
		KSHA1 SHA1(sSecKey);

		SHA1 += s_sWebsocket_sec_key_suffix;

		return KEncode::Base64(SHA1.Digest(), false);
	}
	else
	{
		return KString();
	}

} // GenerateServerSecKeyResponse

//-----------------------------------------------------------------------------
bool KWebSocket::CheckForUpgradeRequest(const KInHTTPRequest& Request, bool bThrowIfInvalid)
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

	// this implementation specifically requires http/1.1 (RFC 6455) - we do not yet
	// support http/2 (RFC 8441) or http/3 (RFC 9220)
	if (Request.GetHTTPVersion() != KHTTPVersion::http11)
	{
		return Error("this server websocket implementation requires HTTP/1.1, not any earlier or later version");
	}

	if (!kStrIn("upgrade", Request.Headers.Get(KHTTPHeader::CONNECTION).ToLowerASCII().Split()))
	{
		// check if it is a comma separated option
		return Error("missing header: Connection: Upgrade");
	}

	if (Request.Headers.Get(KHTTPHeader::SEC_WEBSOCKET_VERSION).UInt16() != 13)
	{
		return Error(kFormat("wrong websocket version: {}", Request.Headers.Get(KHTTPHeader::SEC_WEBSOCKET_VERSION)));
	}

	kDebug(2, "client requests websocket protocol upgrade");

	return true;

} // CheckForUpgradeRequest

//-----------------------------------------------------------------------------
bool KWebSocket::CheckForUpgradeResponse(KStringView sClientSecKey, KStringView sProtocols, const KOutHTTPResponse& Response, bool bThrowIfInvalid)
//-----------------------------------------------------------------------------
{
	auto SetError = [bThrowIfInvalid](KStringViewZ sError)
	{
		kDebug(1, sError);

		if (bThrowIfInvalid)
		{
			throw KWebSocketError(sError);
		}

		return false;
	};

	auto BadHeader = [&Response, &SetError](KHTTPHeader Header)
	{
		return SetError(kFormat("{} header has bad value: '{}'", Header, Response.Headers.Get(Header)));
	};

	if (Response.GetStatusCode() != KHTTPError::H1xx_SWITCHING_PROTOCOLS)
	{
		return SetError(kFormat("bad status code, expected {}, got {}",
		                        static_cast<uint16_t>(KHTTPError::H1xx_SWITCHING_PROTOCOLS),
		                        Response.GetStatusCode()));
	}

	if (Response.Headers.Get(KHTTPHeader::UPGRADE).ToLowerASCII() != "websocket")
	{
		return BadHeader(KHTTPHeader::UPGRADE);
	}

	if (!kStrIn("upgrade", Response.Headers.Get(KHTTPHeader::CONNECTION).ToLowerASCII().Split()))
	{
		return BadHeader(KHTTPHeader::CONNECTION);
	}

	if (Response.Headers.Get(KHTTPHeader::SEC_WEBSOCKET_ACCEPT) != GenerateServerSecKeyResponse(sClientSecKey, false))
	{
		return BadHeader(KHTTPHeader::SEC_WEBSOCKET_ACCEPT);
	}

	if (!sProtocols.empty())
	{
		auto& sProtocol = Response.Headers.Get(KHTTPHeader::SEC_WEBSOCKET_PROTOCOL);

		if (!sProtocol.In(sProtocols))
		{
			return BadHeader(KHTTPHeader::SEC_WEBSOCKET_PROTOCOL);
		}
	}

	return true;

} // CheckForUpgradeResponse

//-----------------------------------------------------------------------------
KWebSocket::KWebSocket(std::unique_ptr<KIOStreamSocket>& Stream, std::function<void(KWebSocket&)> WebSocketHandler, bool bMaskTx)
//-----------------------------------------------------------------------------
: m_Stream(std::move(Stream))
, m_Handler(std::move(WebSocketHandler))
, m_bMaskTx(bMaskTx)
{
	if (!m_Stream)
	{
		throw KWebSocketError("no stream socket set");
	}

	if (!m_Handler)
	{
		throw KWebSocketError("no websocket handler set");
	}

} // ctor

//-----------------------------------------------------------------------------
KWebSocket::KWebSocket(KWebSocket&& other)
//-----------------------------------------------------------------------------
: m_Frame          (std::move(other.m_Frame         ))
, m_Stream         (std::move(other.m_Stream        ))
, m_Handler        (std::move(other.m_Handler       ))
, m_Finish         (std::move(other.m_Finish        ))
, m_ConnectHandler (std::move(other.m_ConnectHandler))
, m_CloseHandler   (std::move(other.m_CloseHandler  ))
, m_PMCE           (std::move(other.m_PMCE          ))
, m_pServer        (other.m_pServer     )
, m_iHandle        (other.m_iHandle     )
, m_ReadTimeout    (other.m_ReadTimeout )
, m_WriteTimeout   (other.m_WriteTimeout)
, m_PingInterval   (other.m_PingInterval)
, m_TimerID        (other.m_TimerID     )
, m_bMaskTx        (other.m_bMaskTx     )
{
	other.m_TimerID      = KTimer::InvalidID;
	other.m_PingInterval = KDuration::zero();
	// a moved-from instance is no longer shared-managed - and the moved-to
	// instance is a different object, so it starts without a weak self
	other.m_WeakSelf.reset();

} // move ctor

//-----------------------------------------------------------------------------
KWebSocket::~KWebSocket()
//-----------------------------------------------------------------------------
{
	AutoPing(KDuration::zero());

} // dtor

//-----------------------------------------------------------------------------
std::shared_ptr<KWebSocket> KWebSocket::Create(std::unique_ptr<KIOStreamSocket>& Stream,
                                               std::function<void(KWebSocket&)> WebSocketHandler,
                                               bool bMaskTx)
//-----------------------------------------------------------------------------
{
	auto pWebSocket = std::make_shared<KWebSocket>(Stream, std::move(WebSocketHandler), bMaskTx);
	pWebSocket->m_WeakSelf = pWebSocket;
	return pWebSocket;

} // Create

//-----------------------------------------------------------------------------
bool KWebSocket::Read()
//-----------------------------------------------------------------------------
{
	m_Frame.clear();

	if (!m_Stream)
	{
		return false;
	}

	for (;;)
	{
		{
			// return latest after set stream timeout
			if (!m_Stream->IsReadReady(m_ReadTimeout))
			{
				return false;
			}
		}

		{
			std::unique_lock<std::mutex> Lock(m_StreamMutex);

			// return immediately from poll
			if (m_Stream->IsReadReady(KDuration()))
			{
				if (!m_Frame.Read(*m_Stream, *m_Stream, m_bMaskTx))
				{
					return false;
				}

				break;
			}
		}

		// stream changed state while acquiring lock, repeat waiting
	}

	// transparently decompress a permessage-deflate compressed message
	if (m_PMCE && m_Frame.IsCompressed())
	{
		KString sDecompressed;

		if (!m_PMCE->Decompress(m_Frame.GetPayload(), sDecompressed))
		{
			kDebug(1, "permessage-deflate decompression failed");
			return false;
		}

		m_Frame.SetPayload(std::move(sDecompressed));
	}

	return true;

} // KWebSocket::Read

//-----------------------------------------------------------------------------
bool KWebSocket::ReadInt(std::function<bool(const KString&)> Func)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		if (!Read())
		{
			return false;
		}

		switch (m_Frame.Type())
		{
			// this is what we waited for
			case KWebSocket::Frame::FrameType::Text:
			case KWebSocket::Frame::FrameType::Binary:
				return Func(m_Frame.GetPayload());

			// answered by the Frame reader itself, simply return false
			case KWebSocket::Frame::FrameType::Close:
				return false;

			// these are handled by the Frame reader itself, just continue reading
			case KWebSocket::Frame::FrameType::Ping:
			case KWebSocket::Frame::FrameType::Pong:
				kDebug(3, "received {} frame with payload '{}' ({} bytes), will continue reading",
				           KWebSocket::Frame::FrameTypeToString(m_Frame.Type()),
				           m_Frame.GetPayload(),
				           m_Frame.GetPayload().size());
				break;

			case KWebSocket::Frame::FrameType::Continuation:
				break;
		}
	}

	return false;

} // KWebSocket::ReadInt

//-----------------------------------------------------------------------------
bool KWebSocket::Read(KString& sFrame)
//-----------------------------------------------------------------------------
{
	sFrame.clear();

	return ReadInt([&sFrame](const KString& sPayload)
	{
		sFrame = sPayload;
		return true;
	});

} // KWebSocketWorker::Read

//-----------------------------------------------------------------------------
bool KWebSocket::Read(KJSON& jFrame)
//-----------------------------------------------------------------------------
{
	jFrame = KJSON{};

	return ReadInt([&jFrame](const KString& sPayload)
	{
		KString sError;
		kjson::Parse(jFrame, sPayload, sError);

		if (!sError.empty())
		{
			kDebug(1, sError);
			return false;
		}

		return true;
	});

} // KWebSocket::Read

//-----------------------------------------------------------------------------
bool KWebSocket::Write(KWebSocket::Frame Frame)
//-----------------------------------------------------------------------------
{
	if (!m_Stream)
	{
		return false;
	}

	// the frame stays local to this call - concurrent writers each carry their
	// own frame and only serialize on the stream mutex, and none of them races
	// the reading thread's use of m_Frame

	for (;;)
	{
		{
			// return latest after set stream timeout
			if (!m_Stream->IsWriteReady(m_WriteTimeout))
			{
				return false;
			}
		}

		{
			std::unique_lock<std::mutex> Lock(m_StreamMutex);

			// return immediately from poll
			if (m_Stream->IsWriteReady(KDuration()))
			{
				if (!Frame.Write(*m_Stream, m_bMaskTx))
				{
					return false;
				}

				break;
			}
		}

		// stream changed state while acquiring lock, repeat waiting
	}

	return true;

} // KWebSocket::Write

//-----------------------------------------------------------------------------
void KWebSocket::EnablePerMessageDeflate(KWebSocketPMCE::Parameters Params, bool bServer)
//-----------------------------------------------------------------------------
{
	if (Params.bEnabled)
	{
		m_PMCE = std::make_unique<KWebSocketPMCE>(std::move(Params), bServer);
	}

} // EnablePerMessageDeflate

//-----------------------------------------------------------------------------
bool KWebSocket::Write(KString sFrame, bool bIsBinary)
//-----------------------------------------------------------------------------
{
	if (m_PMCE)
	{
		// compression and frame write must happen as one unit: the deflate
		// window is shared across messages, so the receiving inflater needs
		// the compressed messages on the wire in compression order
		std::lock_guard<std::mutex> Lock(m_PMCEMutex);

		// transparently compress the message and mark the frame with RSV1
		KString sCompressed;

		if (!m_PMCE->Compress(sFrame, sCompressed))
		{
			kDebug(1, "permessage-deflate compression failed");
			return false;
		}

		KWebSocket::Frame Frame(std::move(sCompressed), bIsBinary);
		Frame.SetRSV1(true);
		return Write(std::move(Frame));
	}

	return Write(KWebSocket::Frame(std::move(sFrame), bIsBinary));

} // KWebSocket::Write

//-----------------------------------------------------------------------------
bool KWebSocket::Write(const KJSON& jFrame)
//-----------------------------------------------------------------------------
{
	// json is UTF8 when dumped, therefore set text mode
	return Write(jFrame.dump(), false);

} // KWebSocket::Write

//-----------------------------------------------------------------------------
bool KWebSocket::Ping(KString sMessage)
//-----------------------------------------------------------------------------
{
	return Write(KWebSocket::Frame(Frame::FrameType::Ping, std::move(sMessage)));

} // KWebSocket::Ping

//-----------------------------------------------------------------------------
bool KWebSocket::Close(uint16_t iStatusCode, KString sReason)
//-----------------------------------------------------------------------------
{
	KWebSocket::Frame Frame;
	Frame.Close(iStatusCode, sReason);
	return Write(std::move(Frame));

} // KWebSocket::Close

//-----------------------------------------------------------------------------
bool KWebSocket::AutoPing(KDuration PingInterval)
//-----------------------------------------------------------------------------
{
	if (m_PingInterval == PingInterval)
	{
		return true;
	}

	auto& Timer = Dekaf::getInstance().GetTimer();

	if (m_TimerID != KTimer::InvalidID)
	{
		Timer.Cancel(m_TimerID);
		m_TimerID = KTimer::InvalidID;
	}

	m_PingInterval = PingInterval;

	if (m_PingInterval.IsZero())
	{
		return true;
	}

	auto WeakSelf = m_WeakSelf;

	if (!WeakSelf.expired())
	{
		// lifetime safe: the ping keeps the socket alive for its duration,
		// and no-ops once the socket is gone
		m_TimerID = Timer.CallEvery(m_PingInterval, [WeakSelf](KUnixTime)
		{
			auto pSelf = WeakSelf.lock();

			if (pSelf)
			{
				pSelf->Ping();
			}
		});
	}
	else
	{
		// not shared-managed: the destructor's Timer.Cancel() drains a ping
		// in flight, so no ping can run on a destroyed socket
		m_TimerID = Timer.CallEvery(m_PingInterval, [this](KUnixTime)
		{
			Ping();
		});
	}

	return m_TimerID != KTimer::InvalidID;

} // AutoPing

//-----------------------------------------------------------------------------
void KWebSocket::CallHandler(class Frame Frame)
//-----------------------------------------------------------------------------
{
	m_Frame = std::move(Frame);
	m_Handler(*this);

} // CallHandler

//-----------------------------------------------------------------------------
void KWebSocket::Finish()
//-----------------------------------------------------------------------------
{
	if (!m_Finish)
	{
		throw KWebSocketError("no finish callback");
	}

	m_Finish();

} // Finish

//-----------------------------------------------------------------------------
KWebSocketServer::KWebSocketServer()
//-----------------------------------------------------------------------------
: KWebSocketServer(Options{})
{
} // ctor

//-----------------------------------------------------------------------------
KWebSocketServer::KWebSocketServer(Options Options)
//-----------------------------------------------------------------------------
: m_Options(std::move(Options))
{
	if (m_Options.iWorkerThreads > 0)
	{
		kDebug(2, "starting websocket server with {} worker threads", m_Options.iWorkerThreads);
		m_ThreadPool.resize(m_Options.iWorkerThreads, m_Options.Growth, m_Options.Shrink);

		if (m_Options.iMaxConcurrentWrites > 0)
		{
			// cap the number of concurrent outbound flushes so a swarm of slow consumers cannot
			// occupy every worker and starve the readers (reads run on the default tag)
			KThreadPool::TagConfig WriteCfg;
			WriteCfg.iMaxConcurrency = m_Options.iMaxConcurrentWrites;
			m_ThreadPool.ConfigureTag(s_WriteTag, WriteCfg);
		}
	}
	else
	{
		kDebug(2, "starting websocket server in single thread mode");
	}

	// the reactor (m_Poll) auto-starts its watcher thread on the first Add()

} // ctor

//-----------------------------------------------------------------------------
KWebSocketServer::~KWebSocketServer()
//-----------------------------------------------------------------------------
{
	// stop dispatching new events first, then drain in-flight workers,
	// then let the connection map (and its sockets) be destroyed
	m_Poll.Stop();
	m_ThreadPool.stop();

} // dtor

//-----------------------------------------------------------------------------
KWebSocketServer::ConnectionPtr KWebSocketServer::GetConnection(Handle iHandle) const
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.shared();

	auto it = Connections->find(iHandle);

	if (it == Connections->end())
	{
		return nullptr;
	}

	return it->second;

} // GetConnection

//-----------------------------------------------------------------------------
KWebSocketServer::Handle KWebSocketServer::AddWebSocket(KWebSocket WebSocket)
//-----------------------------------------------------------------------------
{
	// dekaf2 tracks fds as int (KPoll, the connection table); narrow the native handle here so
	// the invalid-socket check below works on Windows too (SOCKET is unsigned -> "< 0" is never
	// true; static_cast<int>(INVALID_SOCKET) == -1) and so the fd stays formattable/loggable.
	int iFd = static_cast<int>(WebSocket.GetStream().GetNativeSocket());

	if (iFd < 0)
	{
		kDebug(1, "cannot add websocket: invalid socket");
		return 0;
	}

	Handle iHandle { ++m_iLastID };

	if (!iHandle)
	{
		// handle 0 is reserved for "invalid" - skip it on overflow
		iHandle = ++m_iLastID;
	}

	WebSocket.SetReadTimeout(m_Options.IdleTimeout);
	WebSocket.SetWriteTimeout(m_Options.WriteTimeout);
	WebSocket.SetServerContext(this, iHandle);

	// the finish callback lets the application (or KWebSocket itself) tear the connection down
	WebSocket.SetFinishCallback([iHandle, this]()
	{
		RemoveConnection(iHandle);
	});

	auto pConnection = std::make_shared<Connection>(std::move(WebSocket), iFd);

	{
		auto Connections = m_Connections.unique();

		auto p = Connections->insert({ iHandle, pConnection });

		if (!p.second)
		{
			throw KWebSocketError("cannot add websocket");
		}
	}

	kDebug(2, "added websocket connection with handle {} on fd {}", iHandle, iFd);

	// the connection is now registered - let the application learn its handle
	pConnection->WebSocket.CallConnectHandler();

	// start watching the socket for incoming data: dispatch once, then disarm until
	// ServiceConnection() re-arms it (this serializes per-connection dispatch)
	KPoll::Parameters Params;
	Params.iParameter = iHandle;
	Params.iEvents    = POLLIN;
	Params.bRearm     = true;
	Params.Callback   = [this](int iFd, uint16_t iEvents, std::size_t iHandle)
	{
		OnReadable(iHandle);
	};

	m_Poll.Add(iFd, std::move(Params));

	return iHandle;

} // AddWebSocket

//-----------------------------------------------------------------------------
void KWebSocketServer::OnReadable(Handle iHandle)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		// gone in the meantime
		return;
	}

	if (m_Options.iWorkerThreads == 0)
	{
		// single thread mode: handle inline in the reactor thread
		ServiceConnection(std::move(pConnection), iHandle);
	}
	else
	{
		// hand the work to a worker thread, return the reactor thread to polling
		m_ThreadPool.push(&KWebSocketServer::ServiceConnection, this, std::move(pConnection), iHandle);
	}

} // OnReadable

//-----------------------------------------------------------------------------
void KWebSocketServer::ServiceConnection(ConnectionPtr pConnection, Handle iHandle)
//-----------------------------------------------------------------------------
{
	// read one full message (Read() returns false on close, error, or timeout)
	if (!pConnection->WebSocket.Read())
	{
		RemoveConnection(iHandle);
		return;
	}

	// only deliver data messages to the application handler - Ping/Pong are control frames
	// (Ping is auto-answered by the frame reader). A fragmented data message ends with
	// Type() == Continuation, so we filter on the control types, not on the data types.
	auto eType = pConnection->WebSocket.GetFrame().Type();

	if (eType != KWebSocket::Frame::FrameType::Ping &&
	    eType != KWebSocket::Frame::FrameType::Pong)
	{
		// invoke the application's message handler with the freshly read frame
		pConnection->WebSocket.CallHandler();
	}

	// re-arm the socket so the next message gets dispatched (no-op if it was removed)
	m_Poll.Arm(pConnection->iFd);

} // ServiceConnection

//-----------------------------------------------------------------------------
void KWebSocketServer::RemoveConnection(Handle iHandle)
//-----------------------------------------------------------------------------
{
	ConnectionPtr pConnection;

	{
		auto Connections = m_Connections.unique();

		auto it = Connections->find(iHandle);

		if (it == Connections->end())
		{
			kDebug(2, "connection {} already removed", iHandle);
			return;
		}

		pConnection = std::move(it->second);
		Connections->erase(it);
	}

	kDebug(2, "removing websocket connection with handle {}", iHandle);

	// stop any in-flight or queued flusher from touching this connection
	pConnection->bDead = true;

	// stop watching the socket
	m_Poll.Remove(pConnection->iFd);

	// notify the application
	pConnection->WebSocket.CallCloseHandler(iHandle);

} // RemoveConnection

//-----------------------------------------------------------------------------
bool KWebSocketServer::Send(Handle iHandle, KString sMessage, bool bIsBinary)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		kDebug(2, "cannot send to handle {}: unknown connection", iHandle);
		return false;
	}

	return QueueOrWrite(pConnection, std::move(sMessage), bIsBinary);

} // Send

//-----------------------------------------------------------------------------
bool KWebSocketServer::Send(Handle iHandle, const KJSON& jMessage)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		kDebug(2, "cannot send to handle {}: unknown connection", iHandle);
		return false;
	}

	// json is UTF8 when dumped, therefore send as text
	return QueueOrWrite(pConnection, jMessage.dump(), false);

} // Send

//-----------------------------------------------------------------------------
std::size_t KWebSocketServer::Broadcast(KString sMessage, bool bIsBinary)
//-----------------------------------------------------------------------------
{
	// copy the connection pointers out under the lock, then queue/write without holding it
	std::vector<ConnectionPtr> Targets;

	{
		auto Connections = m_Connections.shared();

		Targets.reserve(Connections->size());

		for (const auto& Connection : *Connections)
		{
			Targets.push_back(Connection.second);
		}
	}

	std::size_t iSent { 0 };

	for (auto& pConnection : Targets)
	{
		// pass a copy of the message to each connection (QueueOrWrite takes it by value)
		if (QueueOrWrite(pConnection, sMessage, bIsBinary))
		{
			++iSent;
		}
	}

	return iSent;

} // Broadcast

//-----------------------------------------------------------------------------
bool KWebSocketServer::QueueOrWrite(const ConnectionPtr& pConnection, KString sMessage, bool bIsBinary)
//-----------------------------------------------------------------------------
{
	if (m_Options.iWorkerThreads == 0)
	{
		// single thread mode: there is no worker to offload to - write synchronously
		return pConnection->WebSocket.Write(std::move(sMessage), bIsBinary);
	}

	bool bSchedule { false };

	{
		std::unique_lock<std::mutex> Lock(pConnection->OutMutex);

		if (pConnection->bDead)
		{
			return false;
		}

		if (m_Options.iMaxQueueBytes && pConnection->iQueuedBytes + sMessage.size() > m_Options.iMaxQueueBytes)
		{
			// slow consumer - it cannot drain its queue fast enough, drop the connection
			kDebug(1, "send queue overflow on handle {} ({} bytes queued) - dropping slow consumer",
			          pConnection->WebSocket.GetHandle(), pConnection->iQueuedBytes);
			pConnection->bDead = true;
			Lock.unlock();
			RemoveConnection(pConnection->WebSocket.GetHandle());
			return false;
		}

		pConnection->iQueuedBytes += sMessage.size();
		pConnection->OutQueue.emplace_back(std::move(sMessage), bIsBinary);

		if (!pConnection->bFlushing)
		{
			// take the flusher role for this connection
			pConnection->bFlushing = true;
			bSchedule              = true;
		}
	}

	if (bSchedule)
	{
		// flushes run under their own work-class tag (capped by iMaxConcurrentWrites) so slow
		// consumers cannot starve the readers, which run on the default tag
		m_ThreadPool.PushTagged(s_WriteTag, [this, pConnection]() { FlushConnection(pConnection); });
	}

	return true;

} // QueueOrWrite

//-----------------------------------------------------------------------------
void KWebSocketServer::FlushConnection(ConnectionPtr pConnection)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		KString sMessage;
		bool    bIsBinary { false };

		{
			std::unique_lock<std::mutex> Lock(pConnection->OutMutex);

			if (pConnection->bDead || pConnection->OutQueue.empty())
			{
				// nothing more to do - release the flusher role (checked under the lock
				// together with the queue state, so a concurrent Send() either sees the
				// queue we are about to leave or re-takes the role)
				pConnection->bFlushing = false;
				return;
			}

			sMessage  = std::move(pConnection->OutQueue.front().first);
			bIsBinary = pConnection->OutQueue.front().second;
			pConnection->OutQueue.pop_front();
			pConnection->iQueuedBytes -= sMessage.size();
		}

		// write outside the queue lock - this may block (TLS-safe), but on a worker thread,
		// never on the caller of Send()
		if (!pConnection->WebSocket.Write(std::move(sMessage), bIsBinary))
		{
			kDebug(1, "write failed on handle {} - dropping connection", pConnection->WebSocket.GetHandle());

			{
				std::unique_lock<std::mutex> Lock(pConnection->OutMutex);
				pConnection->bDead        = true;
				pConnection->bFlushing    = false;
				pConnection->iQueuedBytes = 0;
				pConnection->OutQueue.clear();
			}

			RemoveConnection(pConnection->WebSocket.GetHandle());
			return;
		}
	}

} // FlushConnection

//-----------------------------------------------------------------------------
bool KWebSocketServer::Ping(Handle iHandle, KString sMessage)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		return false;
	}

	return pConnection->WebSocket.Ping(std::move(sMessage));

} // Ping

//-----------------------------------------------------------------------------
bool KWebSocketServer::Close(Handle iHandle, uint16_t iStatusCode, KString sReason)
//-----------------------------------------------------------------------------
{
	auto pConnection = GetConnection(iHandle);

	if (!pConnection)
	{
		return false;
	}

	bool bResult = pConnection->WebSocket.Close(iStatusCode, std::move(sReason));

	RemoveConnection(iHandle);

	return bResult;

} // Close

//-----------------------------------------------------------------------------
std::size_t KWebSocketServer::size() const
//-----------------------------------------------------------------------------
{
	return m_Connections.shared()->size();

} // size

DEKAF2_NAMESPACE_END
