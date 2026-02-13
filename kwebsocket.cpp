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
#include "khttperror.h"
#include "dekaf2.h"

DEKAF2_NAMESPACE_BEGIN
namespace {

// this suffix is defined in RFC 6455
static constexpr KStringViewZ s_sWebsocket_sec_key_suffix = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

} // end of anonymous namespace

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
		if (!m_bHaveEncoder && (m_Opcode & (FrameType::Text | FrameType::Binary)))
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
	return false;

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
	if (!m_bHaveEncoder && (m_Opcode & (FrameType::Text | FrameType::Binary)))
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
	sReason[1] = iStatusCode & 256;
	SetPayload(std::move(sReason));

} // Close

//-----------------------------------------------------------------------------
void KWebSocket::Frame::Mask()
//-----------------------------------------------------------------------------
{
	SetHasMask();
	SetMaskingKey(kRandom());
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

	if (Type() & (FrameType::Text | FrameType::Binary))
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

	for (;bIsLast;)
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
		FrameHeader::Read(InStream);

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

	return false;

} // Read

//-----------------------------------------------------------------------------
bool KWebSocket::FrameHeader::GetEncodeFrame() const
//-----------------------------------------------------------------------------
{
	// returns true if we have an active encoder and the frame type is NOT Close
	// (but we encode it positively to skip bad opcodes)
	return GetHaveEncoder() &&
		(Type() & ( FrameType::Text         |
		            FrameType::Binary       |
				    FrameType::Continuation |
				    FrameType::Ping         |
				    FrameType::Pong ));

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

	if (!GetHaveEncoder() && (Type() & (FrameType::Text | FrameType::Binary)))
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
: m_Frame        (std::move(other.m_Frame  ))
, m_Stream       (std::move(other.m_Stream ))
, m_Handler      (std::move(other.m_Handler))
, m_Finish       (std::move(other.m_Finish ))
, m_ReadTimeout  (other.m_ReadTimeout )
, m_WriteTimeout (other.m_WriteTimeout)
, m_PingInterval (other.m_PingInterval)
, m_bMaskTx      (other.m_bMaskTx     )
{
} // move ctor

//-----------------------------------------------------------------------------
KWebSocket::~KWebSocket()
//-----------------------------------------------------------------------------
{
	AutoPing(KDuration::zero());

} // dtor

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

	m_Frame = std::move(Frame);

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
				if (!m_Frame.Write(*m_Stream, m_bMaskTx))
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
bool KWebSocket::Write(KString sFrame, bool bIsBinary)
//-----------------------------------------------------------------------------
{
	return Write(KWebSocket::Frame(std::move(sFrame), bIsBinary));

} // KWebSocket::Write

//-----------------------------------------------------------------------------
bool KWebSocket::Write(const KJSON& jFrame)
//-----------------------------------------------------------------------------
{
	// json is UTF8 when dumped, therefore set text mode
	return Write(KWebSocket::Frame(Frame::FrameType::Text, jFrame.dump()));

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

	m_TimerID = Timer.CallEvery(m_PingInterval, [this](KUnixTime tNow)
	{
		Ping();
	});

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
{
/*
	// start the executor thread
	m_Executor = std::make_unique<std::thread>([]()
	{
		// TODO implement KPoll interface
	});
*/
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

	if (!handle)
	{
		// overflow..
		handle = ++m_iLastID;
	}

	WebSocket.SetFinishCallback([handle, this]()
	{
		auto Connections = m_Connections.unique();

		auto it = Connections->find(handle);

		if (it == Connections->end())
		{
			kDebug(2, "connection already removed");
			return;
		}

		Connections->erase(it);

	});

	auto p = m_Connections.unique()->insert({handle, std::move(WebSocket)});

	if (!p.second)
	{
		throw KWebSocketError("cannot add websocket");
	}

	return handle;

} // AddConnection

DEKAF2_NAMESPACE_END
