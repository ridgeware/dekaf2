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

/// @file khttp3.h
/// wraps nghttp3 and openssl C primitives in C++

#include "kdefinitions.h"

#if DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_OPENSSL_QUIC

#include "kurl.h"
#include "khttp_method.h"
#include "khttp_header.h"
#include "khttp_request.h"
#include "khttp_response.h"
#include "kstringview.h"
#include "kstring.h"
#include "kquicstream.h"
#include "kerror.h"
#include "kdataprovider.h"
#include "kdataconsumer.h"
#include "bits/kunique_deleter.h"
#include <cstdint>
#include <unordered_map>
#include <iostream>

// forward declarations into nghttp3 types
struct nghttp3_conn;
struct nghttp3_rcbuf;
struct nghttp3_vec;

DEKAF2_NAMESPACE_BEGIN

namespace khttp3
{

using nghttp3_ssize = ::ptrdiff_t;

class Session;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Stream describes one request/response stream in a HTTP/3 Session (of which HTTP/3 can have many)
class Stream : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class Session;
	friend class SingleStreamSession;

//----------
public:
//----------

	/// the stream ID type
	using ID = int64_t;

	/// the stream type
	enum Type : uint8_t
	{
		Control,
		QPackEncode,
		QPackDecode,
		Request,
		Incoming
	};

	enum WaitFor : uint8_t
	{
		Nothing = 0,
		Reads   = 1 << 0,
		Writes  = 1 << 1
	};

	/// construct around url, method, and Headers
	Stream(
		Session&                   session,
		KURL                       url,
		KHTTPMethod                Method,
		const KHTTPRequestHeaders& RequestHeaders,
		KHTTPResponseHeaders&      ResponseHeaders
	);
	/// constuct a naked stream of type type, used for control and header streams
	Stream(Session& session, Type type);
	/// accept a stream that already exists, used for incoming streams
	Stream(Session& session, ::SSL* QuicStream, Type type);

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
	int           AddResponseHeader  (ID id, KStringView sName, KStringView sValue);
	/// called once all headers are received
	void          SetHeadersComplete ()               { m_bHeadersComplete = true; }
	/// returns state of all headers received
	bool          IsHeadersComplete  () const         { return m_bHeadersComplete; }

	/// returns the Quic stream ID
	ID            GetStreamID        () const         { return m_StreamID;         }

	/// returns a pointer to the request headers object, may be null
	const KHTTPRequestHeaders*  GetRequestHeaders () const { return m_RequestHeaders;     }
	/// returns a pointer to the response headers object, may be null
	const KHTTPResponseHeaders* GetResponseHeaders() const { return m_ResponseHeaders;    }
	/// returns a pointer to the data provider object, may be null
	const KDataProvider*        GetDataProvider   () const { return m_DataProvider.get(); }
	/// returns a pointer to the data consumer object, may be null
	const KDataConsumer*        GetDataConsumer   () const { return m_DataConsumer.get(); }

//----------
protected:
//----------

	/// receive data pump for this stream
	/// @param bOnce if set to true returns after one read attempt on the stream was successful. If false (default), loops
	/// until stream is exhausted for the moment.
	/// @return < 0 on error, 0 on no error but (temporarily) empty input or >0 count of read bytes
	nghttp3_ssize ReceiveFromQuic    (bool bOnce = false);
	/// send data for this stream coming from nhttp3
	bool          SendToQuic         (nghttp3_vec* vecs, std::size_t num_vecs, bool bFin);
	/// add incoming data to this stream - this is a method normally called by the Session class upon reception
	/// of a data chunk. It is written into the ReceiveBuffer (if existing), and any overflowing data gets stored in
	/// a temporary FIFO buffer (which gets later emptied when a new receive buffer is set)
	int           AddData            (KStringView sData);
	/// reset this stream - no idea when this is called actually
	int           Reset              (int64_t iAppErrorCode);
	/// read from data provider (on request by the nghttp3 layer)
	int           ReadFromDataProvider(KStringView& Buffer, uint32_t* iPFlags);
	/// nghttp3 tells us that the other side acked until iTotalReceived data in this stream - check if we can drop
	/// a buffer that we created for non-persisting data providers
	int           AckedStreamData    (std::size_t iTotalReceived);
	/// returns the Stream::Type
	Type          GetType            () const        { return m_Type;             }
	/// mark this stream as closed
	void          Close              ();
	/// check if this stream is closed
	bool          IsClosed           () const        { return m_bDoneReceivedFin || m_bIsClosed;     }
	/// check if this stream can be removed
	bool          CanDelete          () const        { return IsClosed() && m_RXSpillBuffer.empty(); }
	/// set the buffer that will be filled by the next receive operation
	void          SetReceiveBuffer   (KBuffer buffer);
	/// get the receive buffer (to lookup fill and remaining bytes)
	const KBuffer& GetReceiveBuffer  () const        { return m_RXBuffer;         }
	/// returns the SSL* for this stream
	::SSL*        GetQuicStream      ()              { return m_QuicStream.get(); }

	/// returns if the last data pumps for this stream aborted because of missing read or write data, or both
	WaitFor       GetWaitFor         () const        { return m_WaitFor;          }
	/// resets the WaitFor state to nothing
	void          ClearWaitFor       ()              { m_WaitFor = Nothing;       }
	/// adds @param what to the WaitFor state, @returns the new state
	WaitFor       AddWaitFor         (WaitFor what);
	/// deletes @param what from the WaitFor state, @returns the new state
	WaitFor       DelWaitFor         (WaitFor what);

//----------
private:
//----------

	Session&                       m_Session;
	KUniquePtr<::SSL, ::SSL_free>  m_QuicStream;
	ID                             m_StreamID;
	std::unique_ptr<KDataProvider> m_DataProvider;
	std::unique_ptr<KDataConsumer> m_DataConsumer;
	const KHTTPRequestHeaders*     m_RequestHeaders   { nullptr };
	KHTTPResponseHeaders*          m_ResponseHeaders  { nullptr };
	KURL                           m_URI;
	KString                        m_sAuthority;
	KString                        m_sPath;
	KString                        m_RXSpillBuffer;
	KBuffer                        m_RXBuffer;
	KHTTPMethod                    m_Method;
	Type                           m_Type;
	bool                           m_bHeadersComplete {   false };
	bool                           m_bDoneReceivedFin {   false };
	bool                           m_bIsClosed        {   false };
	bool                           m_bWasBlocked      {   false };
	WaitFor                        m_WaitFor          { Nothing };
	KReservedBuffer<4096>          m_BufferFromQuic;

}; // Stream

DEKAF2_ENUM_IS_FLAG(Stream::WaitFor)

inline Stream::WaitFor Stream::AddWaitFor(WaitFor what) { m_WaitFor |=  what; return m_WaitFor; }
inline Stream::WaitFor Stream::DelWaitFor(WaitFor what) { m_WaitFor &= ~what; return m_WaitFor; }

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Session describes one HTTP/3 session, which is the equivalent of one Quic connection.
/// It can have many Streams.
class Session : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class Stream;

//----------
public:
//----------

	/// construct a HTTP/3 session around a connected Quic stream (after successful ALPN H3 negotiation)
	              Session(KQuicStream& QuicConnection, bool bIsClient);
	              ~Session();

	/// Submit a (client) request. Internally, this generates a new Stream object, the handle of which is returned as a Stream::ID.
	/// This method is _not_ blocking and returns immediately. It can be called multiple times with different requests and
	/// DataConsumer objects for the same authority. Once a DataConsumer object is received, its completion callback is
	/// called.
	Stream::ID    NewStream (
		KURL                           url,
		KHTTPMethod                    Method,
		const KHTTPHeaders&            RequestHeaders,
		std::unique_ptr<KDataProvider> SendData,
		KHTTPResponseHeaders&          ResponseHeaders,
		std::unique_ptr<KDataConsumer> ReceiveData
	);

	/// After adding streams, run the data pump here to send and receive all data.
	/// @returns false in case of an error, which can be retrieved with GetLastError(), else true
	bool          Run                 ()                 { return Run(true);            }

//----------
protected:
//----------

	/// After adding streams, run the data pump here.
	/// @param bWithResponses if true, the loop will be continued until all outstanding response data
	/// for all streams has been received. The stream objects should best have been constructed with valid
	/// KDataConsumers, as otherwise the data will be stored locally in the stream objects until Stream::SetReceiveBuffer()
	/// and Stream::GetReceiveBuffer() are used. That is a mechanism that is normally reserved for the std::iostream
	/// emulation, in which case the SingleStreamSession() class is the better choice.
	bool          Run                 (bool bWithResponses);

	Stream::ID    NewRequest          (std::unique_ptr<Stream> Stream);
	Stream::ID    NewStream           (std::unique_ptr<Stream> Stream);
	Stream::ID    CreateStream        (Stream::Type type);
	Stream::ID    AcceptStream        (::SSL* QuicStream);
	bool          DeleteStream        (Stream::ID StreamID);

	bool          AddStream           (Stream::ID StreamID, std::unique_ptr<Stream> Stream);
	Stream*       GetStream           (Stream::ID StreamID);

	/// returns true if we still have open request streams
	bool          HaveOpenRequestStreams(bool bWithResponses = true) const;

	/// the event handler
	/// @param bWithResponses if true, tries to read response streams as well, if false will read only the control and qpack streams
	/// @return false on error, else true
	bool          HandleEvents        (bool bWithResponses = true);

	nghttp3_conn* GetNGHTTP3_Session  ()                 { return m_Session;            }
	Session&      GetSession          ()                 { return *this;                }
	KQuicStream&  GetKQuicStream      ()                 { return m_KQuickStream;       }
	::SSL*        GetQuicConnection   ()                 { return m_SSL;                }
	::BIO*        GetBio              ()                 { return ::SSL_get_wbio(GetQuicConnection()); }

	std::size_t   GetConsumedAppData  () const           { return m_consumed_app_data;  }
	void          AddConsumedAppData  (std::size_t size) { m_consumed_app_data += size; }
	void          ClearConsumedAppData()                 { m_consumed_app_data = 0;     }

//----------
private:
//----------

	static KStringView ToView(const uint8_t* value, size_t len)
	{
		return { reinterpret_cast<const char*>(value), len };
	}

	int           OnReceiveHeader     (Stream::ID StreamID, KStringView sName, KStringView sValue, uint8_t iFlags);
	int           OnEndHeaders        (Stream::ID StreamID, int fin);
	int           OnReceiveData       (Stream::ID StreamID, KStringView sData);
	int           OnEndStream         (Stream::ID StreamID);
	int           OnStreamClose       (Stream::ID StreamID, uint64_t iAppErrorCode);
	int           OnStopSending       (Stream::ID StreamID, uint64_t iAppErrorCode);
	int           OnResetStream       (Stream::ID StreamID, uint64_t iAppErrorCode);
	int           OnDeferredConsume   (Stream::ID StreamID, std::size_t iConsumed);
	nghttp3_ssize OnDataSourceRead    (Stream::ID StreamID, KStringView& sBuffer, std::size_t iMaxBuffers, uint32_t* iPFlags);
	int           OnAckedStreamData   (Stream::ID StreamID, std::size_t iTotalReceived);

	KQuicStream&  m_KQuickStream;
	::SSL*        m_SSL               { nullptr };
	nghttp3_conn* m_Session           { nullptr };
	std::size_t   m_consumed_app_data { 0 };
	KString       m_sAuthority; // the common authority for all Streams managed by this Session
	std::unordered_map<Stream::ID, std::unique_ptr<Stream>> m_Streams;

	// -------------------------------------------------------------------
	// the static C style callback functions which translate from C to C++
	// -------------------------------------------------------------------

	static Session* ToThis(void* conn_user_data)
	{
		return static_cast<Session*>(conn_user_data);
	}

	static int on_recv_header(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		int32_t token,
		nghttp3_rcbuf* name, nghttp3_rcbuf* value,
		uint8_t flags,
		void* conn_user_data,
		void* stream_user_data
	);

	static int on_end_headers(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		int fin,
		void* conn_user_data,
		void* stream_user_data
	);

	static int on_recv_data(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		const uint8_t* data, size_t datalen,
		void* conn_user_data,
		void* stream_user_data
	);

	static int on_end_stream(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		void* conn_user_data,
		void* stream_user_data
	);

	static int on_stream_close(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		uint64_t app_error_code,
		void* conn_user_data,
		void* stream_user_data
	);

	static int on_stop_sending(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		uint64_t app_error_code,
		void* conn_user_data,
		void* stream_user_data
	);

	static int on_reset_stream(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		uint64_t app_error_code,
		void* conn_user_data,
		void* stream_user_data
	);

	static int on_deferred_consume(
		nghttp3_conn* h3conn,
		Stream::ID stream_id,
		size_t consumed,
		void* conn_user_data,
		void* stream_user_data
	);

	static nghttp3_ssize on_data_source_read(
		nghttp3_conn* conn,
		Stream::ID stream_id,
		nghttp3_vec* vec, size_t veccnt,
		uint32_t* pflags,
		void* conn_user_data,
		void* stream_user_data
	);
	
	static int on_acked_stream_data(
		nghttp3_conn* conn,
		Stream::ID stream_id,
		uint64_t datalen,
		void* conn_user_data,
		void* stream_user_data
	);

}; // Session

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A special type of session in which only one single stream at any time is permitted (multiple streams
/// can be generated consecutively). We use this type of session to adapt HTTP3 to dekaf2's std::iostream
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

	/// Submit a (client) request. Internally, this generates a new Stream object, the handle of which is returned as a Stream::ID.
	/// This method only returns after having sent request headers and body, and received all response headers.
	Stream::ID SubmitRequest (
		KURL                           url,
		KHTTPMethod                    Method,
		const KHTTPRequestHeaders&     RequestHeaders,
		std::unique_ptr<KDataProvider> SendData,
		KHTTPResponseHeaders&          ResponseHeaders
	);

	/// read more decoded data from a response stream identified by StreamID and store it into the provided buffer - you
	/// need to call this method when you use the SubmitRequest() method without DataConsumer objects. Only one
	/// single stream can be handled in this mode.
	std::streamsize ReadData (Stream::ID StreamID, void* data, std::size_t len);

//----------
protected:
//----------

	bool    ReadResponseHeaders (Stream::ID StreamID);

}; // SingleStreamSession

} // end of namespace khttp3

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGHTTP3
