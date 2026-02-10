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
#include "kjson.h"
#include "kstring.h"
#include "kstringview.h"
#include "kstream.h"
#include "kduration.h"
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>

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

		enum Status : uint16_t
		{
			NormalClosure       = 1000,
			GoingAway           = 1001,
			ProtocolError       = 1002,
			UnsupportedData     = 1003,
			Reserved            = 1004,
			NoStatusReceived    = 1005,
			Abnormal            = 1006,
			InvalidPayloadData  = 1007,
			PolicyViolation     = 1008,
			MessageTooBig       = 1009,
			InternalServerError = 1010,
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
		KStringView GetPreamble     ()             const { return KStringView { GetPreambleBuf(), GetPreambleSize() }; }
		/// returns a status code that is only set with a Close frame, so after the conversation ends
		uint16_t    GetStatusCode   ()             const { return m_iStatusCode; }
		/// reset frame header to initial state
		void        clear           ();

		/// @return the given frame type as string (Text, Binary, Ping, Pong, Continuation, Close)
		static KStringView FrameTypeToString (FrameType Type);
		/// @return the status code, if well known, as string
		static KStringView StatusCodeToString(uint16_t iStatusCode);

	//----------
	protected:
	//----------

		/// set the opcode
		void      SetOpcode         (FrameType Opcode)   { m_Opcode = Opcode;        }
		/// set header flags
		void      SetFlags          (bool bIsBinary, bool bIsContinuation, bool bIsLast);
		/// set payload length
		void      SetPayloadLen     (std::size_t iLen)   { m_iPayloadLen = iLen;     }
		/// set the masking key
		void      SetMaskingKey     (uint32_t iMask)     { m_iMaskingKey = iMask;    }
		void      SetHasMask        (bool bYesNo = true) { m_bMask = bYesNo;         }
		void      SetStatusCode     (uint16_t iStatus)   { m_iStatusCode = iStatus;  }
		/// get the masking key
		uint32_t  GetMaskingKey     ()             const { return m_iMaskingKey;     }
		void      XOR               (char* pBuf, std::size_t iSize) const;
		void      XOR               (KStringRef& sBuffer) const { XOR(&sBuffer[0], sBuffer.size()); }

		/// set flag that we have an encoder/decoder
		void      SetHaveEncoder    (bool bYesNo = true)         { m_bHaveEncoder = bYesNo; };
		/// do we have an encoder?
		bool      GetHaveEncoder    ()              const { return m_bHaveEncoder;   };
		/// shall this frame be encoded / decoded ? (yes if we have an encoder and the opcode is valid and not Close)
		bool      GetEncodeFrame    ()              const;


	//----------
	private:
	//----------

		uint64_t  m_iPayloadLen     { 0 };
		uint32_t  m_iMaskingKey     { 0 };
		uint16_t  m_iStatusCode     { 0 }; // will be set with a Close frame
		FrameType m_Opcode          { Continuation };
		uint8_t   m_iFramePos       { 0 };
		uint8_t   m_iExtension      { 0 };
		bool      m_bIsFin          { false };
		bool      m_bMask           { false };
		bool      m_bHaveEncoder    { false };

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
		/// decodes one or multiple frames with payload from input stream, may send pong frames -
		/// masking is required from client to server, and forbidden from server to client
		bool           Read       (KStream& Stream, bool bMaskTx) { return Read(Stream, Stream, bMaskTx); }
		/// writes one full frame with payload to output stream -
		/// masking is required from client to server, and forbidden from server to client
		bool           Write      (KOutStream& OutStream, bool bMaskTx);
		/// set binary or text payload from input stream, and write it, possibly in multiple frames -
		/// masking is required from client to server, and forbidden from server to client
		bool           Write      (KOutStream& OutStream, bool bMaskTx, KInStream& Payload, bool bIsBinary, std::size_t len = npos);
		/// creates mask key and masks payload - call only as a websocket client
		void           Mask       ();
		/// if the frame was announced as masked, unmasks it and clears the mask flag
		void           UnMask     ()                 { UnMask(GetPayloadRef());              }
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
	/// @param iStatusCode a value between 1000 and 1011, or own range
	/// @param sReason a string with a reason for the close - not needed for codes 1000-1011
		void           Close      (uint16_t iStatusCode = 1000, KString sReason = KString{});
		/// returns payload
		const KString& GetPayload () const           { return m_sPayload;              }
		/// returns true if frame has no payload
		bool           empty      () const           { return GetPayload().empty();    }
		/// returns size of the payload
		std::size_t    size       () const           { return GetPayload().size();     }
		/// reset frame to initial state
		void           clear      ();

		iterator       begin      ()                 { return GetPayloadRef().begin(); }
		iterator       end        ()                 { return GetPayloadRef().end();   }
		const_iterator begin      () const           { return GetPayload().begin();    }
		const_iterator end        () const           { return GetPayload().end();      }

	//----------
	protected:
	//----------

		/// returns payload
		KString&       GetPayloadRef ()              { return m_sPayload;           }

		/// transparent encoding interface, appends to sEncoded
		virtual bool   Encode   (KStringView sInput,   KString& sEncoded);
		/// transparent decoding interface, appends to sDecoded
		virtual bool   Decode   (KStringView sEncoded, KString& sDecoded);
		/// try to decode content if the previous read was without decoder (called by children that have setup another decoder now e.g.)
		bool           TryDecode();

	//----------
	private:
	//----------

		bool           DecodeAndSplitPreamble(KStringView sBuffer);
		bool           ReadPreambleFromStream(KInStream& InStream);

		void           UnMask     (KStringRef& sBuffer);

		KString m_sPayload;

	}; // Frame


	/// construct with stream socket (will be owned by the new instance) and a callback function to handle new incoming frames
	KWebSocket(std::unique_ptr<KIOStreamSocket>& Stream,
	           std::function<void(KWebSocket&)> WebSocketHandler,
	           bool bMaskTx);

	KWebSocket(KWebSocket&& other);

	/// set read timeout, probably in the minutes to hours range (defaults to 60 minutes)
	void SetReadTimeout(KDuration ReadTimeout)   { m_ReadTimeout  = ReadTimeout;  }

	/// set write timeout. probably in the seconds range (defaults to 30 seconds)
	void SetWriteTimeout(KDuration WriteTimeout) { m_WriteTimeout = WriteTimeout; }

	/// read one full data frame from the web socket, store in internal frame buffer
	/// @returns false if timeout
	bool Read();

	/// read one full data frame from the web socket, store in string
	/// @returns false if timeout
	bool Read(KString& sFrame);

	/// read one full data frame from the web socket, store in json
	/// @returns false if timeout
	bool Read(KJSON& sFrame);

	/// write one full data frame to web socket
	/// @returns false if unsuccessful
	bool Write(KWebSocket::Frame Frame);

	/// write one full data frame from string to web socket
	/// @param sFrame the data to write
	/// @param bIsBinary set to false if this is UTF8 text, else to true
	/// @returns false if unsuccessful
	bool Write(KString sFrame, bool bIsBinary = false);

	/// write one full data frame from json to web socket
	/// @param jFrame the json data to write
	/// @returns false if unsuccessful
	bool Write(const KJSON& jFrame);

	/// write a ping with or without content to the opposite endpoint (to keep a connection open)
	/// @param sMessage an arbitrary message to send with the ping (it will be returned with the
	/// response pong)
	/// @returns false if unsuccessful
	bool Ping(KString sMessage = KString{});

	/// send a Close frame to finish the connection
	/// @param iStatusCode a value between 1000 and 1011, or own range
	/// @param sReason a string with a reason for the close - not needed for codes 1000-1011
	/// @returns false if unsuccessful
	bool Close(uint16_t iStatusCode = 1000, KString sReason = KString{});

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

	bool ReadInt(std::function<bool(const KString&)> Func);

	std::unique_ptr<KIOStreamSocket> m_Stream;
	std::function<void(KWebSocket&)> m_Handler;
	std::function<void()>            m_Finish;
	Frame                            m_Frame;

	std::mutex                       m_StreamMutex;
	KDuration                        m_ReadTimeout  { chrono::minutes(60) };
	KDuration                        m_WriteTimeout { chrono::seconds(30) };
	bool                             m_bMaskTx { false };

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
