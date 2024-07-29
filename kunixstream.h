/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kunixstream.h
/// provides an implementation of std::iostreams for Unix stream sockets

#include "kdefinitions.h"

#ifdef DEKAF2_HAS_UNIX_SOCKETS

#include "kiostreamsocket.h"
#include "bits/kasiostream.h"
#include "kstringview.h"
#include "kstreambuf.h"
#include "kurl.h"
#include "kstreamoptions.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream TCP implementation with timeout.
class DEKAF2_PUBLIC KUnixStream : public KIOStreamSocket
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KIOStreamSocket;

//----------
public:
//----------

	using asio_stream_type = boost::asio::local::stream_protocol::socket;
#if (DEKAF2_CLASSIC_ASIO)
	using asio_socket_type = boost::asio::basic_socket<boost::asio::local::stream_protocol, boost::asio::stream_socket_service<boost::asio::local::stream_protocol>>;
#else
	using asio_socket_type = boost::asio::basic_socket<boost::asio::local::stream_protocol>;
#endif

	//-----------------------------------------------------------------------------
	/// Construcs an unconnected stream
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KUnixStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected stream as a client.
	/// @param sSocketFile
	/// a unix socket endpoint file
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KUnixStream(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param sSocketFile
	/// a unix socket endpoint file
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
	DEKAF2_NODISCARD
	virtual bool is_open() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.is_open();
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	DEKAF2_NODISCARD
	virtual bool IsDisconnected() override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.IsDisconnected();
	}

	//-----------------------------------------------------------------------------
	/// Gets the ASIO socket of the stream, e.g. to move it to another place ..
	DEKAF2_NODISCARD
	asio_stream_type& GetAsioSocket()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket;
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying socket of the stream
	/// @return
	/// The socket of the stream (wrapped into ASIO's basic_socket<> template)
	asio_socket_type& GetUnixSocket()
	//-----------------------------------------------------------------------------
	{
		return GetAsioSocket().lowest_layer();
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying OS level native socket of the stream
	virtual native_socket_type GetNativeSocket() override final
	//-----------------------------------------------------------------------------
	{
		return GetUnixSocket().native_handle();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	virtual bool Good() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Stream.ec.value() == 0;
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout
	virtual bool Timeout(KDuration Timeout) override final;
	//-----------------------------------------------------------------------------

	KAsioStream<asio_stream_type> m_Stream;

	KStreamBuf m_TCPStreamBuf { &UnixStreamReader, &UnixStreamWriter, this, this };

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	DEKAF2_PRIVATE
	static std::streamsize UnixStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	DEKAF2_PRIVATE
	static std::streamsize UnixStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

};

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KUnixStream> CreateKUnixStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout());
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KUnixStream> CreateKUnixStream(KStringViewZ sSocketFile, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_UNIX_SOCKETS
