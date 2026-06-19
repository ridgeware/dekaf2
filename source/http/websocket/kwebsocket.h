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
/// HTTP web socket code

#include <dekaf2/net/util/kiostreamsocket.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/http/protocol/khttp_request.h>
#include <dekaf2/http/protocol/khttp_response.h>
#include <dekaf2/containers/associative/kassociative.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/time/duration/ktimer.h>
#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/threading/execution/kthreadpool.h>
#include <vector>
#include <deque>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <utility>
#include <functional>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup http_websocket
/// @{

class KWebSocketServer;

class DEKAF2_PUBLIC KWebSocketError : public KException
{
	using KException::KException;
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// permessage-deflate (RFC 7692) codec for one websocket connection.
///
/// Holds the persistent raw-DEFLATE (zlib) compression and decompression streams for one
/// connection. Compress()/Decompress() operate on a whole message (the DEFLATE stream spans
/// all frames of a message). With context takeover the LZ77 window is retained across messages
/// for a better ratio (at ~256KB+40KB resident per connection); with no_context_takeover the
/// compressor is reset after each message. The decompressor always keeps its context, which is
/// correct regardless of the peer's choice (a reset peer simply emits no cross-message back refs).
///
/// zlib is hidden behind a pimpl so <zlib.h> does not leak into this header.
///
/// You normally do not construct this directly: the negotiation helpers below build/parse the
/// Sec-WebSocket-Extensions header, and the codec is enabled on a connection for you. On the
/// server set KRESTServer::Options::WebSocketDeflate (a ServerConfig); on the client call
/// KWebSocketClient::SetPerMessageDeflate(). Messages are then transparently compressed on write
/// and decompressed on read.
///
/// @code
/// // server: accept permessage-deflate, reset compressors per message to cap memory at scale
/// Options.WebSocketDeflate.bEnable                  = true;
/// Options.WebSocketDeflate.bServerNoContextTakeover = true;
/// Options.WebSocketDeflate.bClientNoContextTakeover = true;
///
/// // client: offer permessage-deflate on the next Connect()
/// Client.SetPerMessageDeflate(true);
/// @endcode
/// @see KRESTServer, KWebSocketClient
class DEKAF2_PUBLIC KWebSocketPMCE
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the four RFC 7692 negotiated parameters plus an enabled flag
	struct Parameters
	{
		bool    bClientNoContextTakeover { false };  ///< the client resets its compressor after each message
		bool    bServerNoContextTakeover { false };  ///< the server resets its compressor after each message
		uint8_t iClientMaxWindowBits     { 15    };  ///< the client's compression window (8..15)
		uint8_t iServerMaxWindowBits     { 15    };  ///< the server's compression window (8..15)
		bool    bEnabled                 { false };  ///< true if permessage-deflate was negotiated
	};

	/// construct the codec for a negotiated parameter set
	/// @param Params the negotiated parameters
	/// @param bServer true on the server side (we then compress with the server window and reset
	/// our compressor per bServerNoContextTakeover; false on the client side, vice versa)
	KWebSocketPMCE(Parameters Params, bool bServer);
	~KWebSocketPMCE();

	KWebSocketPMCE(const KWebSocketPMCE&)            = delete;
	KWebSocketPMCE& operator=(const KWebSocketPMCE&) = delete;

	/// compress one full message - the returned bytes are the raw-deflate output with the trailing
	/// empty block (0x00 0x00 0xFF 0xFF) removed, ready to put into a frame with RSV1 set
	/// @returns false on a zlib error
	bool Compress  (KStringView sInput, KString& sOutput);
	/// decompress one full message (the reassembled payload of a frame that had RSV1 set)
	/// @returns false on a zlib error
	bool Decompress(KStringView sInput, KString& sOutput);

	/// the parameters this codec was created with
	const Parameters& GetParameters() const { return m_Params; }

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// server side policy for accepting permessage-deflate offers
	struct ServerConfig
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		bool    bEnable                  { true  };  ///< accept permessage-deflate if the client offers it
		bool    bServerNoContextTakeover { false };  ///< force: reset the server compressor after each message (saves memory at scale)
		bool    bClientNoContextTakeover { false };  ///< force: ask the client to reset its compressor after each message
		uint8_t iServerMaxWindowBits     { 15    };  ///< cap the server compressor window (8..15)

	}; // ServerConfig

	/// negotiate permessage-deflate on the server from a client's Sec-WebSocket-Extensions offer
	/// @param sClientOffer the client's Sec-WebSocket-Extensions header value (may list several extensions)
	/// @param Config the server's policy
	/// @param sResponse receives the Sec-WebSocket-Extensions value to send back (empty if declined)
	/// @return the negotiated parameters, with bEnabled true if permessage-deflate was accepted
	static Parameters NegotiateServer  (KStringView sClientOffer, const ServerConfig& Config, KString& sResponse);
	/// build a client's Sec-WebSocket-Extensions offer value for permessage-deflate
	static KString    BuildClientOffer (bool bClientNoContextTakeover = false, bool bServerNoContextTakeover = false);
	/// parse the server's accepted Sec-WebSocket-Extensions response into negotiated parameters
	/// @return parameters with bEnabled true if the server accepted permessage-deflate
	static Parameters ParseServerResponse(KStringView sServerResponse);

//----------
private:
//----------

	struct Impl;

	Parameters              m_Params;
	bool                    m_bServer;
	std::unique_ptr<Impl>   m_pImpl;

}; // KWebSocketPMCE

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
		/// @return the RSV1 reserved bit (used by permessage-deflate to mark a compressed message)
		bool        GetRSV1         ()             const { return (m_iExtension & 0x04) != 0; }
		/// set or clear the RSV1 reserved bit (permessage-deflate sets it on the first frame of a compressed message)
		void        SetRSV1         (bool bYesNo = true) { if (bYesNo) { m_iExtension |= 0x04; } else { m_iExtension &= ~0x04; } }
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

		/// set the opcode, AND the fin flag
		/// @param Opcode the opcode to set for the frame
		/// @param bIsFin is this the last frame, or will others follow with continuation - defaults to true (= last)
		void      SetOpcodeAndFin   (FrameType Opcode, bool bIsFin = true);
		/// set the opcode to Text, Binary, or Continuation depending on the boolean parms
		/// @param bIsBinary is this a Binary or Text frame
		/// @param bIsContinuation is this a continuation frame to a previous frame
		/// @param bIsFin is this the last frame, or will others follow with continuation - defaults to true (= last)
		void      SetOpcodeAndFin   (bool bIsBinary, bool bIsContinuation, bool bIsFin);
		/// @param bIsFin is this the last frame, or will others follow with continuation
		void      SetFin            (bool bIsFin = true) { m_bIsFin = bIsFin;        }
		/// set payload length
		/// @param iLen the payload len
		void      SetPayloadLen     (std::size_t iLen)   { m_iPayloadLen = iLen;     }
		/// set the masking key
		/// @param iMask the masing key
		void      SetMaskingKey     (uint32_t iMask)     { m_iMaskingKey = iMask;    }
		void      SetHasMask        (bool bYesNo = true) { m_bMask = bYesNo;         }
		/// set the status code
		void      SetStatusCode     (uint16_t iStatus)   { m_iStatusCode = iStatus;  }
		/// get the masking key
		uint32_t  GetMaskingKey     ()             const { return m_iMaskingKey;     }
		/// XOR a buffer
		/// @param pBuf the address of the buffer
		/// @param iSize the size of the buffer
		void      XOR               (void* pBuffer, std::size_t iSize) const;
		/// XOR a string
		/// @param sBuffer the string to XOR
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
		/// construct with frame type, payload, and fin flag
		Frame(FrameType Opcode, KString sPayload, bool bIsFin = true);
		/// construct with payload, text or binary
		Frame(KString sPayload, bool bIsBinary)
		: Frame(bIsBinary ? FrameType::Binary : FrameType::Text, std::move(sPayload))
		{
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
		/// set text payload
		void           Text       (KString sText  );
		/// set binary payload
		void           Binary     (KString sBuffer);
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
		/// set the payload (also updates the announced payload length)
		void           SetPayload (KString sPayload);
		/// @return true if the first data frame of this message had the RSV1 bit set
		/// (i.e. it is a permessage-deflate compressed message)
		bool           IsCompressed() const          { return m_bMessageCompressed;    }
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
		bool    m_bMessageCompressed { false };  // first data frame of the message had RSV1 set

	}; // Frame


	/// construct with stream socket (will be owned by the new instance) and a callback function to handle new incoming frames
	KWebSocket(std::unique_ptr<KIOStreamSocket>& Stream,
	           std::function<void(KWebSocket&)> WebSocketHandler,
	           bool bMaskTx);

	KWebSocket(KWebSocket&& other);

	~KWebSocket();

	/// set read timeout, probably in the minutes to hours range (defaults to 60 minutes)
	void SetReadTimeout(KDuration ReadTimeout)   { m_ReadTimeout  = ReadTimeout;  }

	/// set write timeout. probably in the seconds range (defaults to 30 seconds)
	void SetWriteTimeout(KDuration WriteTimeout) { m_WriteTimeout = WriteTimeout; }

	/// enable permessage-deflate (RFC 7692) compression/decompression on this connection,
	/// using the negotiated parameters - text and binary messages are then transparently
	/// compressed on Write() and decompressed on Read()
	/// @param Params the negotiated permessage-deflate parameters
	/// @param bServer true on the server side, false on the client side
	void EnablePerMessageDeflate(KWebSocketPMCE::Parameters Params, bool bServer);
	/// @return true if permessage-deflate is active on this connection
	bool HasPerMessageDeflate() const { return m_PMCE != nullptr; }

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

	/// force automatic pings being sent to the counterpart to keep the connection alive
	/// @param PingInterval the time interval at which to send pings, defaults to five minutes, 0 switches AutoPing off
	/// @returns true if automatic pings could be setup, false otherwise
	bool AutoPing(KDuration PingInterval = chrono::minutes(5));

	/// set the finish callback for this instance
	void               SetFinishCallback            (std::function<void()> Finish) { m_Finish = std::move(Finish); }
	/// call the finish callback to end this instance
	void               Finish                       ();
	/// called upon reception of a new frame by a connection controller, will call the registered handler function
	void               CallHandler                  (Frame Frame);
	/// call the registered message handler with the frame currently held by this instance
	void               CallHandler                  ()                             { if (m_Handler) m_Handler(*this); }
	/// set a callback that is called once after the connection has been registered with a KWebSocketServer
	void               SetConnectHandler            (std::function<void(KWebSocket&)> Connect) { m_ConnectHandler = std::move(Connect); }
	/// set a callback that is called once when the connection is removed from a KWebSocketServer (argument is the handle)
	void               SetCloseHandler              (std::function<void(std::size_t)> Close)   { m_CloseHandler   = std::move(Close);   }
	/// call the connect handler (called by KWebSocketServer after registration)
	void               CallConnectHandler           ()                             { if (m_ConnectHandler) m_ConnectHandler(*this); }
	/// call the close handler (called by KWebSocketServer on removal)
	void               CallCloseHandler             (std::size_t iHandle)          { if (m_CloseHandler) m_CloseHandler(iHandle); }
	/// set the owning server and the handle for this connection (called by KWebSocketServer::AddWebSocket)
	void               SetServerContext             (KWebSocketServer* pServer, std::size_t iHandle) { m_pServer = pServer; m_iHandle = iHandle; }
	/// returns the handle of this connection within its KWebSocketServer (0 if not added to one)
	DEKAF2_NODISCARD
	std::size_t        GetHandle                    ()                       const { return m_iHandle;             }
	/// returns the owning KWebSocketServer (nullptr if not added to one) - use it to send to other connections
	DEKAF2_NODISCARD
	KWebSocketServer*  GetServer                    ()                       const { return m_pServer;             }
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

	Frame                            m_Frame;
	std::unique_ptr<KIOStreamSocket> m_Stream;
	std::function<void(KWebSocket&)> m_Handler;
	std::function<void()>            m_Finish;
	std::function<void(KWebSocket&)> m_ConnectHandler;
	std::function<void(std::size_t)> m_CloseHandler;
	std::unique_ptr<KWebSocketPMCE>  m_PMCE;
	KWebSocketServer*                m_pServer      { nullptr };
	std::size_t                      m_iHandle      { 0 };
	std::mutex                       m_StreamMutex;
	KDuration                        m_ReadTimeout  { chrono::minutes(60) };
	KDuration                        m_WriteTimeout { chrono::seconds(30) };
	KDuration                        m_PingInterval;
	KTimer::ID_t                     m_TimerID      { KTimer::InvalidID };
	bool                             m_bMaskTx      { false };

}; // KWebSocket

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Maintains multiple websocket connections and dispatches their incoming events.
///
/// A single I/O (reactor) thread polls all connection sockets for incoming data.
/// When a socket becomes readable it is disarmed (so it is not dispatched twice)
/// and the work - reading one full message and calling the message handler - is
/// either run inline in the I/O thread (Options::iWorkerThreads == 0, single
/// thread mode) or handed to a bounded worker thread pool. After the handler
/// returns the socket is re-armed. This serializes events per connection while
/// allowing parallelism across connections.
///
/// To send a message to a specific client from any thread, use Send() with the
/// Handle returned by AddWebSocket() (the application typically learns the handle
/// in the connect handler via KWebSocket::GetHandle() and keeps its own
/// identity -> handle mapping).
///
/// When used together with the REST framework the connection is created and added
/// automatically on a websocket upgrade - you only set the handlers from the route
/// handler. See KRESTServer::SetWebSocketHandler for a complete server-side example.
/// @see KRESTServer::SetWebSocketHandler
class DEKAF2_PUBLIC KWebSocketServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Handle = std::size_t;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// configuration for the websocket server
	struct Options
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// number of worker threads handling events - 0 (default) runs the handlers
		/// inline in the single I/O thread (a slow handler then blocks all connections)
		std::size_t               iWorkerThreads { 0 };
		/// growth policy for the worker thread pool
		KThreadPool::GrowthPolicy Growth         { KThreadPool::PrestartSome };
		/// shrink policy for the worker thread pool
		KThreadPool::ShrinkPolicy Shrink         { KThreadPool::ShrinkSome   };
		/// per connection read (idle) timeout - a connection that is silent for this
		/// long is considered dead and removed
		KDuration                 IdleTimeout    { chrono::minutes(60) };
		/// per connection write timeout - a single frame write that cannot complete within
		/// this time fails and the connection is dropped (bounds how long a slow consumer
		/// can occupy a writer thread)
		KDuration                 WriteTimeout   { chrono::seconds(30) };
		/// maximum number of bytes that may be queued for sending on one connection before it
		/// is dropped as a slow consumer (0 = unlimited). Only relevant with iWorkerThreads > 0,
		/// where Send()/Broadcast() queue and a worker flushes asynchronously
		std::size_t               iMaxQueueBytes { 16 * 1024 * 1024 };
		/// maximum number of connections being flushed (written to) concurrently (0 = unlimited).
		/// The outbound flushes run on the same worker pool as the read handlers but under their
		/// own work-class tag with this concurrency cap, so a swarm of slow consumers cannot tie up
		/// every worker and starve the readers. Only relevant with iWorkerThreads > 0.
		std::size_t               iMaxConcurrentWrites { 0 };

	}; // Options

	KWebSocketServer();
	explicit KWebSocketServer(Options Options);
	~KWebSocketServer();

	KWebSocketServer(const KWebSocketServer&)            = delete;
	KWebSocketServer& operator=(const KWebSocketServer&) = delete;

	/// add an upgraded websocket connection to the server, start watching it for incoming
	/// messages, and return a stable handle to address it (0 on failure)
	Handle      AddWebSocket(KWebSocket WebSocket);

	/// send a text or binary message to a specific connection - thread safe, may be called
	/// from any thread. With iWorkerThreads > 0 the message is queued and written asynchronously
	/// by a worker (the call does not block on the socket); in single thread mode it is written
	/// synchronously. Messages to one connection keep their order in both cases.
	/// @returns false if the handle is unknown, the connection was dropped as a slow consumer
	/// (queue over iMaxQueueBytes), or - in single thread mode - the write failed
	bool        Send      (Handle iHandle, KString sMessage, bool bIsBinary = false);
	/// send a json message (as UTF8 text) to a specific connection (see Send() for threading)
	bool        Send      (Handle iHandle, const KJSON& jMessage);
	/// send a message to all connected clients (queued asynchronously per connection in worker mode)
	/// @returns the number of connections the message was accepted for (queued or written)
	std::size_t Broadcast (KString sMessage, bool bIsBinary = false);
	/// send a ping to a specific connection
	bool        Ping      (Handle iHandle, KString sMessage = KString{});
	/// close a specific connection (sends a close frame and removes it)
	bool        Close     (Handle iHandle, uint16_t iStatusCode = 1000, KString sReason = KString{});
	/// @returns the number of currently connected clients
	DEKAF2_NODISCARD
	std::size_t size      () const;

//----------
private:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// one managed websocket connection - the websocket, its cached socket fd, and the
	/// outbound send queue (drained by an offloaded writer in worker thread mode)
	struct Connection
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		Connection(KWebSocket WebSocket, int iFd)
		: WebSocket(std::move(WebSocket)), iFd(iFd) {}

		KWebSocket                           WebSocket;
		int                                  iFd;
		std::mutex                           OutMutex;             // guards the send queue and its flags
		std::deque<std::pair<KString, bool>> OutQueue;             // pending (message, bIsBinary) frames, FIFO
		std::size_t                          iQueuedBytes { 0 };   // total bytes currently queued
		bool                                 bFlushing    { false };// a flush task is scheduled/running for this connection
		std::atomic<bool>                    bDead        { false };// set when the connection is being torn down

	}; // Connection

	using ConnectionPtr = std::shared_ptr<Connection>;

	// work-class tag for outbound flushes on the shared worker pool (reads use the default tag);
	// configured with iMaxConcurrentWrites so writes cannot starve reads
	static constexpr KThreadPool::Tag s_WriteTag { 1 };

	/// look up a connection by handle (returns nullptr if unknown)
	ConnectionPtr GetConnection    (Handle iHandle) const;
	/// called by the reactor when a connection's socket has incoming data
	void          OnReadable       (Handle iHandle);
	/// read one message and call the handler, then re-arm the socket (runs inline or in a worker)
	void          ServiceConnection(ConnectionPtr pConnection, Handle iHandle);
	/// send synchronously (single thread mode) or queue and schedule the offloaded writer (worker mode)
	bool          QueueOrWrite     (const ConnectionPtr& pConnection, KString sMessage, bool bIsBinary);
	/// drain a connection's send queue on a worker thread (only one flusher per connection at a time)
	void          FlushConnection  (ConnectionPtr pConnection);
	/// remove a connection, stop watching its socket, and call its close handler
	void          RemoveConnection (Handle iHandle);

	Options                       m_Options;
	KPoll                         m_Poll;
	KThreadPool                   m_ThreadPool;
	KThreadSafe<
	    KUnorderedMap<Handle, ConnectionPtr>
	>                             m_Connections;
	std::atomic<Handle>           m_iLastID      { 0 };

}; // KWebSocketServer


/// @}

DEKAF2_NAMESPACE_END
