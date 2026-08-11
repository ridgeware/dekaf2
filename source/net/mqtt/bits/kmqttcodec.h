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

#pragma once

/// @file kmqttcodec.h
/// MQTT 3.1.1 wire format: control packet types, packet framing, and the
/// primitive encodings

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>

DEKAF2_NAMESPACE_BEGIN

namespace kmqtt {

/// MQTT 3.1.1 control packet types, in the upper nibble of the first byte
enum PacketType : uint8_t
{
	Connect     =  1,
	ConnAck     =  2,
	Publish     =  3,
	Subscribe   =  8,
	SubAck      =  9,
	Unsubscribe = 10,
	UnsubAck    = 11,
	PingReq     = 12,
	PingResp    = 13,
	Disconnect  = 14
};

/// the "remaining length" is encoded in at most four bytes of seven bits each
static constexpr std::size_t MaxRemainingLength = 268435455;

/// a length prefixed string carries a 16 bit length
static constexpr std::size_t MaxStringLength    = 65535;

//-----------------------------------------------------------------------------
/// append a length prefixed UTF-8 string (2 byte big endian length)
/// @param sValue at most MaxStringLength bytes - a longer value produces a
/// malformed packet
DEKAF2_PUBLIC
void AppendString(KStringRef& sBuffer, KStringView sValue);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// append the "remaining length" as a variable byte integer (7 bits per byte,
/// the high bit marks a continuation, at most 4 bytes)
DEKAF2_PUBLIC
void AppendVarInt(KStringRef& sBuffer, std::size_t iValue);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// read a length prefixed UTF-8 string at iPos, advancing iPos
/// @return false when the buffer is too short (a malformed packet)
DEKAF2_PUBLIC
bool ReadString(KStringView sBuffer, std::size_t& iPos, KStringView& sValue);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// write one packet and flush it. The caller guarantees that no other thread
/// touches this stream meanwhile.
/// @return false on a hard stream error or a body above MaxRemainingLength
DEKAF2_PUBLIC
bool WritePacket(KOutStream& Stream, uint8_t iFirstByte, KStringView sBody);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// read one packet, blocking
/// @param iMaxPacketSize bodies above this size are treated as malformed -
/// there is no way to resync on a stream after refusing a body
/// @return false when the connection is gone or the packet is malformed
DEKAF2_PUBLIC
bool ReadPacket(KInStream& Stream, uint8_t& iFirstByte, KStringRef& sBody, std::size_t iMaxPacketSize);
//-----------------------------------------------------------------------------

} // end of namespace kmqtt

DEKAF2_NAMESPACE_END
