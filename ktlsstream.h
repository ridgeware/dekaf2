/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

/// @file ktlsstream.h
/// provides an implementation of std::iostreams supporting TLS

#include "bits/kasiostream.h"
#include "kiostreamsocket.h"
#include "ktlscontext.h"
#include "kstring.h"
#include "kstreambuf.h"
#include "kurl.h"

DEKAF2_NAMESPACE_BEGIN

namespace detail {

template<typename StreamType>
struct KAsioTLSTraits
{
	static bool SocketIsOpen(StreamType& Socket)
		{ return Socket.next_layer().is_open(); }
	static void SocketShutdown(StreamType& Socket, boost::system::error_code& ec)
		{ Socket.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec); }
	static void SocketClose(StreamType& Socket, boost::system::error_code& ec)
		{ Socket.next_layer().close(ec); }
	static void SocketPeek(StreamType& Socket, boost::system::error_code& ec)
		{ uint16_t buffer; Socket.next_layer().receive(boost::asio::buffer(&buffer, 1), Socket.next_layer().message_peek, ec); }
};

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename StreamType>
struct KAsioTLSStream : public KAsioStream<StreamType, detail::KAsioTLSTraits<StreamType>>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----------------------------------------------------------------------------
	KAsioTLSStream(KTLSContext& Context, KDuration Timeout, bool _bManualHandshake = false)
	//-----------------------------------------------------------------------------
	: KAsioStream<StreamType, detail::KAsioTLSTraits<StreamType>>
	                   { Context, Timeout  }
	, TLSContext       { Context           }
	, bManualHandshake { _bManualHandshake }
	{
	} // ctor

	//-----------------------------------------------------------------------------
	/// Gets the KTLSContext used in construction
	const KTLSContext& GetContext() const
	//-----------------------------------------------------------------------------
	{
		return TLSContext;
	}

	KTLSContext& TLSContext;
	bool         bNeedHandshake   { true  };
	bool         bManualHandshake { false };

}; // KAsioTLSStream


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream TLS implementation with timeout.
class DEKAF2_PUBLIC KTLSStream : public KIOStreamSocket
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KIOStreamSocket;

//----------
public:
//----------

	using asio_stream_type = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
#if (DEKAF2_CLASSIC_ASIO)
	using asio_socket_type = boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::stream_socket_service<boost::asio::ip::tcp>>;
#else
	using asio_socket_type = boost::asio::basic_socket<boost::asio::ip::tcp>;
#endif

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected client stream
	KTLSStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected stream
	/// @param Context
	/// A KTLSContext which defines role (server/client). Custom certs and crypto suites
	/// will also be defined with the KTLSContext.
	/// @param iSecondsTimeout
	/// Timeout for any I/O. Defaults to 15 seconds.
	KTLSStream(KTLSContext& Context, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected client stream
	/// @param Context
	/// A KTLSContext which defines role (server/client). Custom certs and crypto suites
	/// will also be defined with the KTLSContext.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	KTLSStream(KTLSContext& Context, const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected client stream
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	KTLSStream(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the endpoint address when in server mode
	virtual void SetConnectedEndPointAddress(const KTCPEndPoint& Endpoint) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	virtual bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{}) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect the stream
	virtual bool Disconnect() override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Disconnect();
	}

	//-----------------------------------------------------------------------------
	virtual bool is_open() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.next_layer().is_open();
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	virtual bool IsDisconnected() override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.IsDisconnected();
	}

	//-----------------------------------------------------------------------------
	/// Upgrade connection from TCP to TCP over TLS. Returns true on success. Can also
	/// be used to force a handshake before any IO is triggered.
	virtual bool StartManualTLSHandshake() override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Switch to manual handshake, only possible before any data has been read or
	/// written
	virtual bool SetManualTLSHandshake(bool bYesno = true) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Allow to switch to HTTP2
	/// @param bAlsoAllowHTTP1 if set to false, only HTTP/2 connections are permitted. Else a fallback on
	/// HTTP/1.1 is permitted. Default is true.
	/// @returns true if protocol request is permitted
	bool SetRequestHTTP2(bool bAlsoAllowHTTP1 = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the ASIO socket of the stream, e.g. to move it to another place ..
	asio_stream_type& GetAsioSocket()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket;
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying TCP socket of the stream
	/// @return
	/// The TCP socket of the stream (wrapped into ASIO's basic_socket<> template)
	asio_socket_type& GetTCPSocket()
	//-----------------------------------------------------------------------------
	{
		return GetAsioSocket().lowest_layer();
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying OS level native socket of the stream
	virtual native_socket_type GetNativeSocket() override final
	//-----------------------------------------------------------------------------
	{
		return GetTCPSocket().native_handle();
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying openssl handle of the stream
	virtual native_tls_handle_type GetNativeTLSHandle() override final
	//-----------------------------------------------------------------------------
	{
		return GetAsioSocket().native_handle();
	}

	//-----------------------------------------------------------------------------
	/// Gets the KTLSContext used in construction
	const KTLSContext& GetContext() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.GetContext();
	}

	//-----------------------------------------------------------------------------
	std::streamsize direct_read_some(void* sBuffer, std::streamsize iCount) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// is this a stream with TLS?
	virtual bool IsTLS() const override final { return true; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a reference to self
	KTLSStream& GetKTLSStream()
	//-----------------------------------------------------------------------------
	{
		return *this;
	}

	//-----------------------------------------------------------------------------
	virtual bool Good() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.ec.value() == 0;
	}

	//-----------------------------------------------------------------------------
	/// If a HTTP/2 handshake failed in a specific way it may be due to an OpenSSL communication problem.
	/// In that case, it is advised to retry the connection with HTTP/1.1
	bool ShouldRetryWithHTTP1() const
	//-----------------------------------------------------------------------------
	{
		return m_bRetryWithHTTP1;
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout.
	virtual bool Timeout(KDuration Timeout) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	DEKAF2_PRIVATE
	static std::streamsize TLSStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	DEKAF2_PRIVATE
	static std::streamsize TLSStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	bool Handshake();
	//-----------------------------------------------------------------------------

	KAsioTLSStream<asio_stream_type> m_Stream;
	KBufferedStreamBuf m_TLSStreamBuf { &TLSStreamReader, &TLSStreamWriter, this, this };
	KStreamOptions m_StreamOptions;
	bool m_bRetryWithHTTP1 { false };

}; // KTLSStream


// there is nothing special with a tcp ssl client
using KTLSClient = KTLSStream;

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTLSStream> CreateKTLSServer(KTLSContext& Context,
											 KDuration Timeout = KStreamOptions::GetDefaultTimeout());
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTLSClient> CreateKTLSClient();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTLSClient> CreateKTLSClient(const KTCPEndPoint& EndPoint, KStreamOptions Options);
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END


