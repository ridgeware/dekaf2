/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

#include <dekaf2/net/mqtt/bits/kmqttcodec.h>

DEKAF2_NAMESPACE_BEGIN

namespace kmqtt {

//-----------------------------------------------------------------------------
void AppendString(KStringRef& sBuffer, KStringView sValue)
//-----------------------------------------------------------------------------
{
	std::size_t iSize = sValue.size();

	sBuffer += static_cast<char>((iSize >> 8) & 0xFF);
	sBuffer += static_cast<char>( iSize       & 0xFF);
	sBuffer += sValue;

} // AppendString

//-----------------------------------------------------------------------------
void AppendVarInt(KStringRef& sBuffer, std::size_t iValue)
//-----------------------------------------------------------------------------
{
	// seven bits per byte, the high bit says "one more follows" - the protocol
	// allows at most four bytes, which caps a packet at 256 MB
	for (uint16_t iByteCount = 0; iByteCount < 4; ++iByteCount)
	{
		uint8_t iByte = static_cast<uint8_t>(iValue % 128);

		iValue /= 128;

		if (iValue > 0) iByte |= 0x80;

		sBuffer += static_cast<char>(iByte);

		if (iValue == 0) break;
	}

} // AppendVarInt

//-----------------------------------------------------------------------------
bool ReadString(KStringView sBuffer, std::size_t& iPos, KStringView& sValue)
//-----------------------------------------------------------------------------
{
	if (iPos + 2 > sBuffer.size()) return false;

	std::size_t iSize = (static_cast<uint8_t>(sBuffer[iPos]) << 8)
	                  |  static_cast<uint8_t>(sBuffer[iPos + 1]);

	iPos += 2;

	if (iPos + iSize > sBuffer.size()) return false;

	sValue = KStringView(sBuffer.data() + iPos, iSize);
	iPos  += iSize;

	return true;

} // ReadString

//-----------------------------------------------------------------------------
bool WritePacket(KOutStream& Stream, uint8_t iFirstByte, KStringView sBody)
//-----------------------------------------------------------------------------
{
	// a longer body cannot be encoded in the remaining length field
	if (sBody.size() > MaxRemainingLength) return false;

	KString sPacket;

	sPacket += static_cast<char>(iFirstByte);
	AppendVarInt(sPacket, sBody.size());
	sPacket += sBody;

	// Flushed through the streambuf, NOT through Flush(): the latter goes via
	// ostream::flush(), which uses a sentry and therefore does nothing at all
	// while the stream state is unclean — and on a socket that state is dirty
	// most of the time, because every partial read raises eofbit (see
	// ReadPacket). kWrite() has no such scruples and puts the bytes into the
	// buffer regardless, so the packet would sit there, written but never
	// sent, and only a state check would hint at it. pubsync() is the same
	// bypass on the way out.
	Stream.Write(sPacket.data(), sPacket.size());

	std::streambuf* pBuffer = Stream.ostream().rdbuf();

	if (pBuffer != nullptr) pBuffer->pubsync();

	// only a hard error counts: eofbit is noise here (see above), and on a
	// socket stream the state may still carry the verdict of the last read
	return !Stream.ostream().bad();

} // WritePacket

//-----------------------------------------------------------------------------
bool ReadPacket(KInStream& Stream, uint8_t& iFirstByte, KStringRef& sBody, std::size_t iMaxPacketSize)
//-----------------------------------------------------------------------------
{
	sBody.clear();

	char chByte { 0 };

	if (Stream.Read(&chByte, 1) != 1) return false;

	// dekaf2's kRead() raises eofbit whenever it gets fewer bytes than asked
	// for. On a socket that is not an end of file, it is an ordinary partial
	// delivery — but the flag sticks, and from then on every stream state
	// check lies (reads keep working because kRead goes straight to the
	// streambuf). Clearing after a successful read keeps the state honest.
	Stream.istream().clear();

	iFirstByte = static_cast<uint8_t>(chByte);

	// the remaining length arrives as a variable byte integer, and TCP gives
	// us no message boundaries - so it is read one byte at a time until a byte
	// without the continuation bit shows up
	std::size_t iLength     { 0 };
	std::size_t iMultiplier { 1 };
	uint8_t     iByte       { 0x80 };

	for (uint16_t iByteCount = 0; (iByte & 0x80) != 0; ++iByteCount)
	{
		// a fifth length byte is a protocol error
		if (iByteCount == 4) return false;

		if (Stream.Read(&chByte, 1) != 1) return false;

		Stream.istream().clear();

		iByte = static_cast<uint8_t>(chByte);

		iLength     += (iByte & 0x7F) * iMultiplier;
		iMultiplier *= 128;
	}

	// oversized bodies are refused unread, which makes them fatal for the
	// connection by design
	if (iLength > iMaxPacketSize) return false;

	if (iLength > 0)
	{
		sBody.resize(iLength);

		std::size_t iRead { 0 };

		// one Read() is not guaranteed to deliver everything
		while (iRead < iLength)
		{
			std::size_t iChunk = Stream.Read(&sBody[iRead], iLength - iRead);

			// zero bytes is the real end: the peer closed the connection, or
			// the watchdog timeout closed the socket under us. Anything above
			// zero is a partial delivery, which is normal.
			if (iChunk == 0) return false;

			Stream.istream().clear();

			iRead += iChunk;
		}
	}

	return true;

} // ReadPacket

} // end of namespace kmqtt

DEKAF2_NAMESPACE_END
