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

#include <boost/asio.hpp>
#include "kstringview.h"
#include "kstream.h"
#include "kstreambuf.h"
#include "kurl.h"

namespace dekaf2
{


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream TCP implementation with timeout.
class KUnixIOStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::iostream;

	enum { DEFAULT_TIMEOUT = 1 * 30 };

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construcs an unconnected stream
	KUnixIOStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected stream as a client.
	/// @param sSocketFile
	/// a unix socket endpoint file
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 60.
	KUnixIOStream(KStringViewZ sSocketFile, int iSecondsTimeout = DEFAULT_TIMEOUT);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructs and closes a stream
	~KUnixIOStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout in seconds.
	bool Timeout(int iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param sSocketFile
	/// a unix socket endpoint file
	bool connect(KStringViewZ sSocketFile);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// std::iostream interface to open a stream. Delegates to connect()
	/// @param sSocketFile
	/// a unix socket endpoint file
	bool open(KStringViewZ sSocketFile)
	//-----------------------------------------------------------------------------
	{
		return connect(sSocketFile);
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
	boost::asio::basic_socket<boost::asio::local::stream_protocol, boost::asio::stream_socket_service<boost::asio::local::stream_protocol> >& GetUnixSocket()
#else
	boost::asio::basic_socket<boost::asio::local::stream_protocol>& GetUnixSocket()
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
			return KString{};
		}
	}

//----------
private:
//----------

    boost::asio::io_service m_IO_Service;
	using unixstream = boost::asio::local::stream_protocol::socket;

	struct Stream_t
	{
		Stream_t(boost::asio::io_service& ioservice)
		: Socket(ioservice)
		{}
		
		unixstream Socket;
		boost::system::error_code ec;
		int iTimeoutMilliseconds { 30 * 1000 };
	};

	Stream_t m_Stream;

	KStreamBuf m_TCPStreamBuf{&UnixStreamReader, &UnixStreamWriter, &m_Stream, &m_Stream};

	enum POLLSTATE
	{
		POLL_FAILURE = 0,
		POLL_SUCCESS = 1,
		POLL_LAST    = 2
	};

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	static std::streamsize UnixStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	static std::streamsize UnixStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static POLLSTATE timeout(bool bForReading, Stream_t* stream);
	//-----------------------------------------------------------------------------

};

/// TCP stream based on std::iostream
using KUnixStream = KReaderWriter<KUnixIOStream>;

//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream(KStringViewZ sSocketFile);
//-----------------------------------------------------------------------------

} // namespace dekaf2

