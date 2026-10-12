/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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
 //
 */

#pragma once

/// @file khttp2.h
/// wraps nghttp2 primitives in C++

#include <dekaf2/core/init/kdefinitions.h>

#if DEKAF2_HAS_NGHTTP2

#include <dekaf2/web/url/kurl.h>
#include <dekaf2/http/protocol/khttp_method.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/protocol/khttp_response.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/containers/sequential/kbuffer.h>
#include <dekaf2/io/pipes/kdataprovider.h>
#include <dekaf2/io/pipes/kdataconsumer.h>
#include <cstdint>
#include <unordered_map>

// forward declarations into nghttp2 types
struct nghttp2_session;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup http_protocol
/// @{

namespace khttp2
{

using nghttp2_ssize = ::ptrdiff_t;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Stream describes one request/response stream in a HTTP/2 Session (of which HTTP/2 can have many)
class DEKAF2_PUBLIC Stream
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static constexpr std::size_t BufferSize = 2048;

	using ID = int32_t;

	/// construct around url, method and ResponseHeaders
	/// @param Limits the limits for each received response header block
	Stream(KURL url, KHTTPMethod Method, KHTTPResponseHeaders& ResponseHeaders,
	       const KHTTPHeaders::Limits& Limits = KHTTPHeaders::DefaultResponseLimits);

	/// set the data provider for this Stream's request (if any)
	void          SetDataProvider    (std::unique_ptr<KDataProvider> Data) { m_DataProvider = std::move(Data); }
	/// set the data consumer for this Stream's response (if any)
	void          SetDataConsumer    (std::unique_ptr<KDataConsumer> Data) { m_DataConsumer = std::move(Data); }

	/// returns the URI's scheme component (https, ..)
	KStringView   GetScheme          ();
	/// returns the domain name, plus the port if not the default port of the scheme
	KStringView   GetAuthority       ();
	/// returns the path and query componend, path prefixed with / (and existing if otherwise empty), query
	/// prefixed with ? if existing
	KStringView   GetPath            ();
	/// returns the Method
	KStringView   GetMethod          () const         { return m_Method.Serialize(); }

	/// starts the count of a new response header block
	void          StartResponseHeaders();
	/// adds an incoming response header. id is only used for logging purposes.
	/// Headers of a 1xx interim response are dropped, the pseudo header fields count
	/// toward the limits.
	/// @returns false if a header limit or the limit of interim responses is exceeded -
	/// the response headers then name the reason, and the stream has to be reset
	bool          AddResponseHeader  (ID id, KStringView sName, KStringView sValue);
	/// returns true if the response was rejected for an exceeded limit
	bool          IsRejected         () const         { return m_bRejected;        }
	/// returns the response headers object of this stream
	const KHTTPResponseHeaders& GetResponseHeaders() const { return m_ResponseHeaders; }
	/// called at the end of a response header block - completes the headers,
	/// except after a 1xx interim response, whose final response is still to come
	/// @returns true if the headers are complete now
	bool          EndResponseHeaders (ID id);
	/// called once all headers are received
	void          SetHeadersComplete ()               { m_bHeadersComplete = true; }
	/// returns state of all headers received
	bool          IsHeadersComplete  () const         { return m_bHeadersComplete; }
	/// set the buffer that will be filled by the next receive operation
	void          SetReceiveBuffer   (KBuffer buffer);
	/// get the receive buffer (to lookup fill and remaining bytes)
	const KBuffer& GetReceiveBuffer  () const         { return m_RXBuffer;         }
	/// the stream was closed, set the IsClosed flag, and if set, call the DataConsumer callback
	/// @param iErrorCode the HTTP/2 error code the stream was closed with, 0 (NO_ERROR) by default
	void          Close              (uint32_t iErrorCode = 0);
	/// return the close flag
	bool          IsClosed           () const         { return m_bIsClosed;        }
	/// returns the HTTP/2 error code the stream was closed with
	uint32_t      GetCloseErrorCode  () const         { return m_iCloseErrorCode;  }
	/// the peer sent the last frame of the response (a frame with END_STREAM)
	void          SetEndOfStream     ()               { m_bEndOfStream = true;     }
	/// returns true if the peer sent the last frame of the response - a stream that is
	/// closed without it was reset, and its response is incomplete
	bool          IsEndOfStream      () const         { return m_bEndOfStream;     }
	/// add incoming data to this stream - this is a method normally called by the Session class upon reception
	/// of a data chunk. It is written into the ReceiveBuffer (if existing), and any overflowing data gets stored in
	/// a temporary FIFO buffer (which gets later emptied when a new receive buffer is set)
	void          Receive            (KConstBuffer data);
	/// returns true if there is still data in the RX spill buffer that needs to get requested before
	/// closing the stream
	bool          HasRXBuffered      () const         { return !m_RXSpillBuffer.empty(); }

//----------
private:
//----------

	std::unique_ptr<KDataProvider> m_DataProvider;
	std::unique_ptr<KDataConsumer> m_DataConsumer;
	KHTTPResponseHeaders&          m_ResponseHeaders;
	KString                        m_RXSpillBuffer;
	KBuffer                        m_RXBuffer;
	KURL                           m_URI;
	KString                        m_sAuthority;
	KString                        m_sPath;
	KHTTPMethod                    m_Method;
	KHTTPHeaders::Counter          m_Count;
	uint32_t                       m_iCloseErrorCode    { 0 };
	uint16_t                       m_iInterimResponses  { 0 };
	bool                           m_bRejected          { false };
	bool                           m_bHeadersComplete   { false };
	bool                           m_bIsInterimResponse { false };
	bool                           m_bIsClosed          { false };
	bool                           m_bEndOfStream       { false };

}; // Stream

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Session describes one HTTP/2 session, which is the equivalent of one TCP connection.
/// It can have many Streams.
class DEKAF2_PUBLIC Session : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// construct a HTTP/2 session around a connected TLS stream (after successful ALPN H2 negotiation)
	/// @param Limits the limits for received response headers - the size of the header
	/// block is also announced to the peer, and bounds the number of CONTINUATION frames
	Session(KTLSStream& TLSStream, bool bIsClient,
	        const KHTTPHeaders::Limits& Limits = KHTTPHeaders::DefaultResponseLimits);
	~Session();

	/// Submit a (client) request. Internally, this generates a new Stream object, the handle of which is returned as an StreamID_t.
	/// This method is _not_ blocking and returns immediately. It can be called multiple times with different requests and
	/// DataConsumer objects for the same authority. Once a DataConsumer object is received, its completion callback is
	/// called.
	Stream::ID NewStream(KURL                           url,
	                     KHTTPMethod                    Method,
	                     const KHTTPHeaders&            RequestHeaders,
	                     std::unique_ptr<KDataProvider> SendData,
	                     KHTTPResponseHeaders&          ResponseHeaders,
	                     std::unique_ptr<KDataConsumer> ReceiveData);

	/// after adding streams, run the data pump here until Run() returns false
	bool Run();

	/// set the limits for the response headers of new streams - the announced size and
	/// the CONTINUATION limit of the session stay as set at construction
	void SetResponseHeaderLimits(const KHTTPHeaders::Limits& Limits) { m_Limits = Limits; }

	/// returns the limits for the response headers of new streams
	const KHTTPHeaders::Limits& GetResponseHeaderLimits() const { return m_Limits; }

//----------
protected:
//----------

	Stream::ID NewRequest (Stream Stream,
	                       const KHTTPHeaders& RequestHeaders,
	                       std::unique_ptr<KDataProvider> SendData);

	bool    SendClientConnectionHeader ();
	bool    SessionSend         ();
	bool    SessionReceive      ();
	bool    Received            (const void* data, std::size_t len);
	/// returns true if nghttp2 ended the session, e.g. after it sent a GOAWAY for a
	/// protocol error - no further frames are processed
	bool    IsSessionEnded      () const;
	/// returns the cause of the end of the session, for the error message of a stream
	KString GetSessionEndReason () const;

	bool    AddStream           (Stream::ID StreamID, Stream Stream);
	Stream* GetStream           (Stream::ID StreamID);
	bool    CloseStream         (Stream::ID StreamID, uint32_t iErrorCode);
	bool    DeleteStream        (Stream::ID StreamID);

	/// returns the cause of a failed read from the TLS stream: its error, or the end of the connection
	KString GetTLSReadError() const;
	static KStringView TranslateError(int iError);
	static KStringView TranslateFrameType (uint8_t FrameType);

	KTLSStream&    m_TLSStream;

//----------
private:
//----------

	friend struct SessionCallbacks;

	static Session* ToThis(void* user_data)
	{
		return static_cast<Session*>(user_data);
	}

	static KStringView ToView(const uint8_t* value, size_t len)
	{
		return { reinterpret_cast<const char*>(value), len };
	}

	nghttp2_ssize OnReceive       (KBuffer data, int flags);
	nghttp2_ssize OnSend          (KConstBuffer data, int flags);
	int           OnSendData      (void* frame, const uint8_t* framehd, size_t length, KDataProvider& source);
	int           OnHeader        (const void* frame, KStringView sName, KStringView sValue, uint8_t flags);
	int           OnBeginHeaders  (const void* frame);
	int           OnFrameRecv     (const void* frame);
	int           OnFrameSend     (const void* frame);
	int           OnDataChunkRecv (uint8_t flags, Stream::ID stream_id, KConstBuffer data);
	int           OnStreamClose   (Stream::ID stream_id, uint32_t error_code);

	nghttp2_session* m_Session { nullptr };
	std::unordered_map<Stream::ID, Stream> m_Streams;
	KString          m_sAuthority; // the common authority for all Streams managed by this Session
	KHTTPHeaders::Limits m_Limits; // for the response headers of new Streams
	uint32_t         m_iGoAwayErrorCode { 0 }; // of a GOAWAY that this side sent
	bool             m_bSentGoAway      { false };

}; // Session

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A special type of session in which only one single stream at any time is permitted (multiple streams
/// can be generated consecutively). We use this type of session to adapt HTTP2 to dekaf2's std::iostream
/// KHTTPClient.
class DEKAF2_PUBLIC SingleStreamSession : protected Session
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Session::Session;
	using Session::SetError;
	using Session::GetLastError;
	using Session::CopyLastError;
	using Session::HasError;
	using Session::SetResponseHeaderLimits;

	/// Submit a (client) request. Internally, this generates a new Stream object, the handle of which is returned as an Stream::ID.
	/// This method only returns after having sent request headers and body, and received all response headers.
	Stream::ID SubmitRequest (KURL                           url,
	                          KHTTPMethod                    Method,
	                          const KHTTPHeaders&            RequestHeaders,
	                          std::unique_ptr<KDataProvider> SendData,
	                          KHTTPResponseHeaders&          ResponseHeaders);

	/// read more decoded data from a response stream identified by StreamID and store it into the provided buffer - you
	/// need to call this method when you use the SubmitRequest() method without DataConsumer objects. Only one
	/// single stream can be handled in this mode.
	std::streamsize ReadData (Stream::ID StreamID, void* data, std::size_t len);

//----------
protected:
//----------

	bool    ReadResponseHeaders (Stream::ID StreamID);

}; // SingleStreamSession

} // end of namespace khttp2


/// @}

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGHTTP2
