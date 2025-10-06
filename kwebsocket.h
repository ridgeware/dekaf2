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

#include "kiostreamsocket.h"
#include "kexception.h"
#include "khttp_request.h"
#include "khttp_response.h"
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

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// represents one single websocket connection
class DEKAF2_PUBLIC KWebSocket
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// a websocket (RFC6455) frame header
	class DEKAF2_PUBLIC FrameHeader
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

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

		virtual ~FrameHeader();

		bool        Read            (KInStream& Stream);
		/// write the frame header to a stream
		bool        Write           (KOutStream& Stream);
		/// decode a websocket frame header step by step
		/// @return false until full header has been decoded
		bool        Decode          (uint8_t byte);
		/// @return a string with the serialized header
		KString     Serialize       ()             const;
		/// @return the frame type (Text, Binary, Ping, Pong, Continuation, Close)
		FrameType   Type            ()             const { return m_Opcode;      }
		/// @return the Finished flag
		bool        Finished        ()             const { return m_bIsFin;      }
		/// @return the payload size decoded from the header
		uint64_t    AnnouncedSize   ()             const { return m_iPayloadLen; }
		/// has this frame been sent with xoring?
		bool        IsMaskedRx      ()             const { return m_bMask;       }
		/// returns the size of the preamble expected in front of the payload - this is helpful for
		/// protocol nesting. The value must be a multiple of 4.
		virtual
		std::size_t GetPreambleSize ()             const;
		/// gets the pointer to a buffer of GetPreambleSize() size to fill in the preamble at reading
		/// and read the preamble at writing
		virtual
		char*       GetPreambleBuf  ()             const;


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

		using iterator       = KString::iterator;
		using const_iterator = KString::const_iterator;

		Frame() = default;
		/// construct with payload, text or binary
		Frame(KString sPayload, bool bIsBinary)
		{
			SetPayload(std::move(sPayload), bIsBinary);
		}
		/// construct from Stream, decodes one or multiple frames with payload, may send pong frames
		Frame(KStream& Stream)
		{
			Read(Stream, Stream, false);
		}

		/// construct from InStream / OutStream, decodes one or multiple frames with payload, may send pong frames
		Frame(KInStream& InStream, KOutStream& OutStream)
		{
			Read(InStream, OutStream, false);
		}

		/// decodes one or multiple frames with payload from input stream, may send pong frames -
		/// masking is required from client to server, and forbidden from server to client
		bool           Read       (KInStream& InStream, KOutStream& OutStream, bool bMaskTx);
		/// writes one full frame with payload to output stream -
		/// masking is required from client to server, and forbidden from server to client
		bool           Write      (KOutStream& OutStream, bool bMaskTx);
		/// set binary or text payload from input stream, and write it, possibly in multiple frames
		bool           Write      (KOutStream& OutStream, bool bMaskTx, KInStream& Payload, bool bIsBinary, std::size_t len = npos);
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
		/// returns payload
		const KString& GetPayload () const           { return m_sPayload;           }
		/// returns true if frame has no payload
		bool           empty      () const           { return GetPayload().empty(); }
		/// returns size of the payload
		std::size_t    size       () const           { return GetPayload().size();  }

		iterator       begin      ()                 { return GetPayloadRef().begin(); }
		iterator       end        ()                 { return GetPayloadRef().end();   }
		const_iterator begin      () const           { return GetPayload().begin();    }
		const_iterator end        () const           { return GetPayload().end();      }

	//----------
	protected:
	//----------

		/// set header flags for this frame
		void           SetFlags   (bool bIsBinary, bool bIsContinuation, bool bIsLast);
		/// returns payload
		KString&       GetPayloadRef ()              { return m_sPayload;           }

	//----------
	private:
	//----------

		void           XOR        (KStringRef& sBuffer);
		void           XOR        (char* pBuf, std::size_t iSize);
		void           UnMask     (KStringRef& sBuffer);

		KString m_sPayload;

	}; // Frame


	/// construct with stream socket (will be owned by the new instance) and a callback function to handle new incoming frames
	KWebSocket(std::unique_ptr<KIOStreamSocket>& Stream, std::function<void(KWebSocket&)> WebSocketHandler);

	/// set the finish callback for this instance
	void               SetFinishCallback            (std::function<void()> Finish) { m_Finish = std::move(Finish); }
	/// call the finish callback to end this instance
	void               Finish                       ();
	/// called upon reception of a new frame by a connection controller, will call the registered handler function
	void               CallHandler                  (Frame Frame);
	/// returns a reference to the current frame
	Frame&             GetFrame                     ()                             { return m_Frame;               }
	/// returns a reference to the stream socket for this instance
	KIOStreamSocket&   GetStream                    ()                             { return *m_Stream.get();       }
	/// move the stream socket out of the KWebSocket class
	std::unique_ptr<KIOStreamSocket> MoveStream     ()                             { return std::move(m_Stream);   }

	/// generate a client's sec key
	static KString     GenerateClientSecKey         ();
	/// returns true if sec key looks valid (note, we only check for size and suffix '==', we do not decode the full base64)
	static bool        ClientSecKeyLooksValid       (KStringView sSecKey, bool bThrowIfInvalid);
	/// generate the server response on a client's sec key
	static KString     GenerateServerSecKeyResponse (KString sSecKey, bool bThrowIfInvalid);
	/// check if a client requests a websocket upgrade of the HTTP/1.1 connection
	static bool        CheckForUpgradeRequest       (const KInHTTPRequest& Request, bool bThrowIfInvalid);
	/// check if the server response on a client upgrade request is valid
	static bool        CheckForUpgradeResponse      (KStringView sClientSecKey, KStringView sProtocols, const KOutHTTPResponse& Response, bool bThrowIfInvalid);

//----------
private:
//----------

	std::unique_ptr<KIOStreamSocket> m_Stream;
	std::function<void(KWebSocket&)> m_Handler;
	std::function<void()>            m_Finish;
	Frame                            m_Frame;

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
