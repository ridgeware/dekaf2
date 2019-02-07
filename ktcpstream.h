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

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "bits/kasiostream.h"
#include "kstring.h"
#include "kstream.h" // TODO remove
#include "kstreambuf.h"
#include "kurl.h"

namespace dekaf2
{


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream TCP implementation with timeout.
class KTCPIOStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::iostream;

	enum { DEFAULT_TIMEOUT = 15 };

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construcs an unconnected stream
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KTCPIOStream(int iSecondsTimeout = DEFAULT_TIMEOUT);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected stream as a client.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KTCPIOStream(const KTCPEndPoint& Endpoint, int iSecondsTimeout = DEFAULT_TIMEOUT);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructs and closes a stream
	~KTCPIOStream();
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
	bool Connect(const KTCPEndPoint& Endpoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// std::iostream interface to open a stream. Delegates to connect()
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	bool open(const KTCPEndPoint& Endpoint)
	//-----------------------------------------------------------------------------
	{
		return Connect(Endpoint);
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
		return m_Stream.Socket.is_open();
	}

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
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.ec.value() == 0;
	}

	//-----------------------------------------------------------------------------
	KString Error() const
	//-----------------------------------------------------------------------------
	{
		if (!Good())
		{
			return m_Stream.ec.message();
		}
		else
		{
			return {};
		}
	}

//----------
private:
//----------

	using tcpstream = boost::asio::basic_stream_socket<boost::asio::ip::tcp>;

	KAsioStream<tcpstream> m_Stream;

	KBufferedStreamBuf m_TCPStreamBuf{&TCPStreamReader, &TCPStreamWriter, &m_Stream, &m_Stream};

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	static std::streamsize TCPStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	static std::streamsize TCPStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

};

/// TCP stream based on std::iostream
using KTCPStream = KReaderWriter<KTCPIOStream>;

//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(const KTCPEndPoint& EndPoint);
//-----------------------------------------------------------------------------

} // namespace dekaf2

