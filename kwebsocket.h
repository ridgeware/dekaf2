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

#pragma once

/// @file kwebsocket.h
/// HTTP web socket helper code

#include "kstream.h"
#include "ktcpstream.h"
#include "ktlsstream.h"
#include "kunixstream.h"
#include "kexception.h"
#include "khttp_request.h"
#include "kassociative.h"
#include "kthreadsafe.h"
#include <vector>
#include <atomic>
#include <thread>

DEKAF2_NAMESPACE_BEGIN

class DEKAF2_PUBLIC KWebSocketError : public KException
{
	using KException::KException;
};

namespace kwebsocket {

enum StreamType
{
	NONE,
	TCP,
	TLS
#ifdef DEKAF2_HAS_UNIX_SOCKETS
	, UNIX
#endif
};

DEKAF2_PUBLIC StreamType                      GetStreamType    (KStream& Stream);
DEKAF2_PUBLIC KTCPStream::asio_stream_type&&  GetAsioTCPStream (KStream& Stream);
DEKAF2_PUBLIC KTLSStream::asio_stream_type&&  GetAsioTLSStream (KStream& Stream);
#ifdef DEKAF2_HAS_UNIX_SOCKETS
DEKAF2_PUBLIC KUnixStream::asio_stream_type&& GetAsioUnixStream(KStream& Stream);
#endif

/// a websocket frame type
enum FrameType : uint8_t
{
	Continuation =  0,
	Text         =  1,
	Binary       =  2,
	Close        =  8,
	Ping         =  9,
	Pong         = 10,
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a websocket (RFC6455) frame header
class DEKAF2_PUBLIC FrameHeader
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// decode a websocket frame header step by step
	/// @return false until full header has been decoded
	bool        Decode(uint8_t byte);
	/// @return a string with the serialized header
	KString     Serialize()           const;
	/// @return the frame type (Text, Binary, Ping, Pong, Continuation, Close)
	FrameType   Type()                const { return m_Opcode;      }
	/// @return the Finished flag
	bool        Finished()            const { return m_bIsFin;      }
	/// @return the payload size decoded from the header
	uint64_t    AnnouncedSize()       const { return m_iPayloadLen; }

//----------
protected:
//----------

	uint64_t  m_iPayloadLen { 0 };
	uint32_t  m_iMaskingKey { 0 };
	FrameType m_Opcode      { Continuation };
	uint8_t   m_iExtension  { 0 };
	bool      m_bIsFin      { false };
	bool      m_bMask       { false };

//----------
private:
//----------

	uint8_t   m_iFramePos   { 0 };

}; // FrameHeader

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a websocket (RFC6455) frame, header and payload
class DEKAF2_PUBLIC Frame : public FrameHeader
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Frame() = default;
	/// construct with payload, text or binary
	Frame(KString sPayload, bool bIsBinary)
	{
		SetPayload(std::move(sPayload), bIsBinary);
	}
	/// construct from stream, decodes one or multiple frames with payload, may send pong frames
	Frame(KStream& Stream)
	{
		Read(Stream, false);
	}

	/// decodes one or multiple frames with payload from input stream, may send pong frames
	bool           Read       (KStream& Stream, bool bMaskTx);
	/// writes one full frame with payload to output stream
	bool           Write      (KOutStream& OutStream, bool bMaskTx);
	/// creates mask key and masks payload - call only as a websocket client
	void           Mask       ();
	/// if the frame was announced as masked, unmasks it and clears the mask flag
	void           UnMask     ();
	/// set binary or text payload
	void           SetPayload (KString sPayload, bool bIsBinary);
	/// set text payload
	void           Text       (KString sText  )  { SetPayload(std::move(sText),  false); }
	/// set binary payload
	void           Binary     (KString sBuffer)  { SetPayload(std::move(sBuffer), true); }
	/// set binary payload
	void           Binary     (const void* Buffer, std::size_t iSize) { Binary(KString(static_cast<const char*>(Buffer), iSize)); }
	/// create a ping frame
	void           Ping       (KString sMessage);
	/// create a pong frame
	void           Pong       (KString sMessage);
	/// create a close frame
	void           Close      ();
	/// read payload
	const KString& Payload    () const           { return m_sPayload; }

//----------
private:
//----------

	void           XOR        (KStringRef& sBuffer);
	void           UnMask     (KStringRef& sBuffer);

	KString m_sPayload;

}; // Frame

/// generate a client's sec key
KString DEKAF2_PUBLIC GenerateClientSecKey();
/// returns true if sec key looks valid (note, we only check for size and suffix '==', we do not decode the full base64)
bool DEKAF2_PUBLIC ClientSecKeyLooksValid(KStringView sSecKey, bool bThrowIfInvalid);
/// generate the server response on a client's sec key
KString DEKAF2_PUBLIC GenerateServerSecKeyResponse(KString sSecKey, bool bThrowIfInvalid);
/// check if a client requests a websocket upgrade of the HTTP/1.1 connection
bool DEKAF2_PUBLIC CheckForWebSocketUpgrade(const KInHTTPRequest& Request, bool bThrowIfInvalid);

} // end of namespace kwebsocket

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// represents one single websocket connection
class DEKAF2_PUBLIC KWebSocket
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KWebSocket(KStream& Stream, std::function<void(KWebSocket&)> WebSocketHandler);

	void CallHandler(kwebsocket::Frame Frame);

//----------
private:
//----------

	KStream m_Stream;
	std::function<void(KWebSocket&)> m_Handler;

}; // KWebSocket

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Maintain multiple websocket connections and their event handlers
class DEKAF2_PUBLIC KWebSocketServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Handle = std::size_t;

	KWebSocketServer();
	~KWebSocketServer();

	Handle AddWebSocket(KWebSocket WebSocket);

//----------
private:
//----------

	KThreadSafe<
	    KUnorderedMap<std::size_t, KWebSocket>
	>                             m_Connections;
	std::unique_ptr<std::thread>  m_Executor;
	std::atomic<std::size_t>      m_iLastID      { 0 };
	std::atomic<bool>             m_bStop    { false };

}; // KWebSocketServer

DEKAF2_NAMESPACE_END
