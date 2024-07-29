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

/// @file ktcpstream.h
/// provides an implementation of std::iostreams for TCP

#include "kdefinitions.h"
#include "kiostreamsocket.h"
#include "bits/kasiostream.h"
#include "kstring.h"
#include "kstreambuf.h"
#include "kurl.h"
#include "kstreamoptions.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream TCP implementation with timeout.
class DEKAF2_PUBLIC KTCPStream : public KIOStreamSocket
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KIOStreamSocket;

//----------
public:
//----------

	using asio_stream_type   = boost::asio::basic_stream_socket<boost::asio::ip::tcp>;
#if (DEKAF2_CLASSIC_ASIO)
	using asio_socket_type  = boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::stream_socket_service<boost::asio::ip::tcp>>;
#else
	using asio_socket_type  = boost::asio::basic_socket<boost::asio::ip::tcp>;
#endif

	//-----------------------------------------------------------------------------
	/// Construcs an unconnected stream
	/// @param Timeout
	/// Timeout for any I/O. Defaults to 15 seconds.
	KTCPStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected client stream
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options 
	/// set options like IPv4 or IPv6, and the timeout
	KTCPStream(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructs and closes a stream
	~KTCPStream() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like IPv4 or IPv6, and the timeout
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
		return m_Stream.Socket.is_open();
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	virtual bool IsDisconnected() override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.IsDisconnected();
	}

	//-----------------------------------------------------------------------------
	/// Gets the ASIO stream socket of the stream, e.g. to move it to another place ..
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
	virtual bool Good() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.ec.value() == 0;
	}

//----------
private:
//----------

	/// Set I/O timeout
	virtual bool Timeout(KDuration Timeout) override final;

	KAsioStream<asio_stream_type> m_Stream;

	KBufferedStreamBuf m_TCPStreamBuf { &TCPStreamReader, &TCPStreamWriter, this, this };

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	DEKAF2_PRIVATE
	static std::streamsize TCPStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	DEKAF2_PRIVATE
	static std::streamsize TCPStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

};


//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTCPStream> CreateKTCPStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout());
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KTCPStream> CreateKTCPStream(const KTCPEndPoint& EndPoint, KStreamOptions Options);
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END
