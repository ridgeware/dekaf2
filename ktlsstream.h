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
#include "ktlscontext.h"
#include "kstring.h"
#include "kstream.h" // TODO remove
#include "kstreambuf.h"
#include "kurl.h"
#include "kconnection.h" // TLSOptions ..

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
	KAsioTLSStream(KTLSContext& Context, int iSecondsTimeout = 15, bool _bManualHandshake = false)
	//-----------------------------------------------------------------------------
	: KAsioStream<StreamType, detail::KAsioTLSTraits<StreamType>>
	                   { Context, iSecondsTimeout }
	, TLSContext       { Context                  }
	, bManualHandshake { _bManualHandshake        }
	{
	} // ctor

	//-----------------------------------------------------------------------------
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
class DEKAF2_PUBLIC KTLSIOStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::iostream;

//----------
public:
//----------

	using asiostream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

	enum { DEFAULT_TIMEOUT = 1 * 15 };

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected client stream
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KTLSIOStream(int iSecondsTimeout = DEFAULT_TIMEOUT);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected stream
	/// @param Context
	/// A KTLSContext which defines role (server/client). Custom certs and crypto suites
	/// will also be defined with the KTLSContext.
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KTLSIOStream(KTLSContext& Context,
				 int iSecondsTimeout = DEFAULT_TIMEOUT);
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
	/// set options like certificate verification, manual TLS handshake, HTTP2 request
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KTLSIOStream(KTLSContext& Context,
				 const KTCPEndPoint& Endpoint,
				 TLSOptions Options,
				 int iSecondsTimeout = DEFAULT_TIMEOUT);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructs and closes a stream
	~KTLSIOStream() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout in seconds.
	bool Timeout(int iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request
	bool Connect(const KTCPEndPoint& Endpoint, TLSOptions Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// std::iostream interface to open a stream. Delegates to connect()
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request
	bool open(const KTCPEndPoint& Endpoint, TLSOptions Options)
	//-----------------------------------------------------------------------------
	{
		return Connect(Endpoint, Options);
	}

	//-----------------------------------------------------------------------------
	/// Disconnect the stream
	bool Disconnect()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Disconnect();
	}

	//-----------------------------------------------------------------------------
	/// Disconnect the stream
	bool close()
	//-----------------------------------------------------------------------------
	{
		return Disconnect();
	}

	//-----------------------------------------------------------------------------
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.next_layer().is_open();
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	bool IsDisconnected()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.IsDisconnected();
	}

	//-----------------------------------------------------------------------------
	/// Upgrade connection from TCP to TCP over TLS. Returns true on success. Can also
	/// be used to force a handshake before any IO is triggered.
	bool StartManualTLSHandshake();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Switch to manual handshake, only possible before any data has been read or
	/// written
	bool SetManualTLSHandshake(bool bYesno = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Allow to switch to HTTP2
	/// @param bAlsoAllowHTTP1 if set to false, only HTTP/2 connections are permitted. Else a fallback on
	/// HTTP/1.1 is permitted. Default is true.
	/// @returns true if protocol request is permitted
	bool SetRequestHTTP2(bool bAlsoAllowHTTP1 = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the Application Layer Protocol Negotiation after the TLS handshake
	KStringView GetALPN();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the underlying TCP socket of the stream
	/// @return
	/// The TCP socket of the stream (wrapped into ASIO's basic_socket<> template)
#if (BOOST_VERSION < 106600)
	boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::stream_socket_service<boost::asio::ip::tcp> >& GetTCPSocket()
#else
	boost::asio::basic_socket<boost::asio::ip::tcp>& GetTCPSocket()
#endif
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.lowest_layer();
	}

	//-----------------------------------------------------------------------------
	/// Gets the ASIO socket of the stream, e.g. to move it to another place ..
	asiostream& GetAsioSocket()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket;
	}

	//-----------------------------------------------------------------------------
	std::streamsize direct_read_some(void* sBuffer, std::streamsize iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a reference to self
	KTLSIOStream& GetKTLSIOStream()
	//-----------------------------------------------------------------------------
	{
		return *this;
	}

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.ec.value() == 0;
	}

	//-----------------------------------------------------------------------------
	KString Error() const
	//-----------------------------------------------------------------------------
	{
		KString sError;

		if (!Good())
		{
			sError = m_Stream.ec.message();
		}

		return sError;
	}

//----------
private:
//----------

	KAsioTLSStream<asiostream> m_Stream;

	KBufferedStreamBuf m_TLSStreamBuf{&TLSStreamReader, &TLSStreamWriter, this, this};

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
	static bool handshake(KAsioTLSStream<asiostream>* stream);
	//-----------------------------------------------------------------------------

}; // KTLSIOStream


/// TLS stream based on std::iostream and asio::ssl
using KTLSStream = KReaderWriter<KTLSIOStream>;

// there is nothing special with a tcp ssl client
using KTLSClient = KTLSStream;

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTLSStream> CreateKTLSServer(KTLSContext& Context);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTLSClient> CreateKTLSClient(int iSecondsTimeout = KTLSIOStream::DEFAULT_TIMEOUT);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTLSClient> CreateKTLSClient(const KTCPEndPoint& EndPoint,
											 TLSOptions Options,
											 int iSecondsTimeout = KTLSIOStream::DEFAULT_TIMEOUT);
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END


