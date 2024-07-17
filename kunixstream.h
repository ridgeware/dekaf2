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

#include "bits/kasio.h"
#include "bits/kasiostream.h"
#include "kstringview.h"
#include "kstream.h"
#include "kstreambuf.h"
#include "kurl.h"
#include "kstreamoptions.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream TCP implementation with timeout.
class DEKAF2_PUBLIC KUnixIOStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::iostream;

//----------
public:
//----------

	using asiostream = boost::asio::local::stream_protocol::socket;

	//-----------------------------------------------------------------------------
	/// Construcs an unconnected stream
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KUnixIOStream(KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected stream as a client.
	/// @param sSocketFile
	/// a unix socket endpoint file
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	KUnixIOStream(KStringViewZ sSocketFile, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructs and closes a stream
	~KUnixIOStream() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout
	bool Timeout(KDuration Timeout);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param sSocketFile
	/// a unix socket endpoint file
	bool Connect(KStringViewZ sSocketFile);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// std::iostream interface to open a stream. Delegates to connect()
	/// @param sSocketFile
	/// a unix socket endpoint file
	bool open(KStringViewZ sSocketFile)
	//-----------------------------------------------------------------------------
	{
		return Connect(sSocketFile);
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
	DEKAF2_NODISCARD
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.is_open();
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	DEKAF2_NODISCARD
	bool IsDisconnected()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.IsDisconnected();
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying socket of the stream
	/// @return
	/// The socket of the stream (wrapped into ASIO's basic_socket<> template)
#if (BOOST_VERSION < 106600)
	boost::asio::basic_socket<boost::asio::local::stream_protocol, boost::asio::stream_socket_service<boost::asio::local::stream_protocol> >& GetUnixSocket()
#else
	boost::asio::basic_socket<boost::asio::local::stream_protocol>& GetUnixSocket()
#endif
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.lowest_layer();
	}

	//-----------------------------------------------------------------------------
	/// Gets the ASIO socket of the stream, e.g. to move it to another place ..
	DEKAF2_NODISCARD
	asiostream& GetAsioSocket()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket;
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.ec.value() == 0;
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
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

	KAsioStream<asiostream> m_Stream;

	KStreamBuf m_TCPStreamBuf{&UnixStreamReader, &UnixStreamWriter, &m_Stream, &m_Stream};

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

/// TCP stream based on std::iostream
using KUnixStream = KReaderWriter<KUnixIOStream>;

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
