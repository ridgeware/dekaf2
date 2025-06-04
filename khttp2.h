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

#include "kdefinitions.h"

#if DEKAF2_HAS_NGHTTP2

#include "kurl.h"
#include "khttp_method.h"
#include "khttp_header.h"
#include "khttp_response.h"
#include "kstringview.h"
#include "kstring.h"
#include "ktlsstream.h"
#include "kerror.h"
#include "kbuffer.h"
#include "kdataprovider.h"
#include "kdataconsumer.h"
#include <cstdint>
#include <unordered_map>

// forward declarations into nghttp2 types
struct nghttp2_session;

DEKAF2_NAMESPACE_BEGIN

namespace khttp2
{

using nghttp2_ssize = ::ptrdiff_t;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Stream describes one request/response stream in a HTTP/2 Session (of which HTTP/2 can have many)
class Stream
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static constexpr std::size_t BufferSize = 2048;

	using ID = int32_t;

	/// construct around url, method and ResponseHeaders
	Stream(KURL url, KHTTPMethod Method, KHTTPResponseHeaders& ResponseHeaders);

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

	/// adds an incoming response header. id is only used for logging purposes.
	void          AddResponseHeader  (ID id, KStringView sName, KStringView sValue);
	/// called once all headers are received
	void          SetHeadersComplete ()               { m_bHeadersComplete = true; }
	/// returns state of all headers received
	bool          IsHeadersComplete  () const         { return m_bHeadersComplete; }
	/// set the buffer that will be filled by the next receive operation
	void          SetReceiveBuffer   (KBuffer buffer);
	/// get the receive buffer (to lookup fill and remaining bytes)
	const KBuffer& GetReceiveBuffer  () const         { return m_RXBuffer;         }
	/// the stream was closed, set the IsClosed flag, and if set, call the DataConsumer callback
	void          Close              ();
	/// return the close flag
	bool          IsClosed           () const         { return m_bIsClosed;        }
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
	bool                           m_bHeadersComplete { false };
	bool                           m_bIsClosed        { false };

}; // Stream

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Session describes one HTTP/2 session, which is the equivalent of one TCP connection.
/// It can have many Streams.
class Session : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// construct a HTTP/2 session around a connected TLS stream (after successful ALPN H2 negotiation)
	Session(KTLSStream& TLSStream, bool bIsClient);
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

	bool    AddStream           (Stream::ID StreamID, Stream Stream);
	Stream* GetStream           (Stream::ID StreamID);
	bool    CloseStream         (Stream::ID StreamID);
	bool    DeleteStream        (Stream::ID StreamID);

	static KStringView TranslateError(int iError);
	static KStringView TranslateFrameType (uint8_t FrameType);

	KTLSStream&    m_TLSStream;

//----------
private:
//----------

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
	int           OnDataChunkRecv (uint8_t flags, Stream::ID stream_id, KConstBuffer data);
	int           OnStreamClose   (Stream::ID stream_id, uint32_t error_code);

	nghttp2_session* m_Session { nullptr };
	std::unordered_map<Stream::ID, Stream> m_Streams;
	KString          m_sAuthority; // the common authority for all Streams managed by this Session

	// -------------------------------------------------------------------
	// the static C style callback functions which translate from C to C++
	// -------------------------------------------------------------------

	static Session* ToThis(void* user_data) 
	{
		return static_cast<Session*>(user_data);
	}

	static nghttp2_ssize OnReceiveCallback(
		nghttp2_session* session,
		uint8_t* buf, size_t length,
		int flags,
		void* user_data
	);

	static nghttp2_ssize OnSendCallback(
		nghttp2_session* session,
		const uint8_t* data, size_t length,
		int flags,
		void* user_data
	);

	static int OnHeaderCallback(
		nghttp2_session* session,
		const void* frame,
		const uint8_t* name, size_t namelen,
		const uint8_t* value, size_t valuelen,
		uint8_t flags,
		void* user_data
	);

	static int OnBeginHeadersCallback(
		nghttp2_session* session,
		const void* frame,
		void* user_data
	);

	static int OnFrameRecvCallback(
		nghttp2_session* session,
		const void* frame,
		void *user_data
	);

	static int OnDataChunkRecvCallback(
		nghttp2_session* session,
		uint8_t flags, Stream::ID stream_id,
		const uint8_t* data, size_t len,
		void* user_data
	);

	static nghttp2_ssize OnDataSourceReadCallback(
		nghttp2_session *session,
		Stream::ID stream_id,
		uint8_t* buf, size_t length,
		uint32_t* data_flags, void* source,
		void *user_data
	);

	static int OnSendDataCallback(
		nghttp2_session* session,
		void* frame,
		const uint8_t* framehd, size_t length,
		void* source,
		void* user_data
	);

	static int OnStreamCloseCallback(
		nghttp2_session* session,
		Stream::ID stream_id, uint32_t error_code,
		void* user_data
	);

}; // Session

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A special type of session in which only one single stream at any time is permitted (multiple streams
/// can be generated consecutively). We use this type of session to adapt HTTP2 to dekaf2's std::iostream
/// KHTTPClient.
class SingleStreamSession : protected Session
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

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGHTTP2
