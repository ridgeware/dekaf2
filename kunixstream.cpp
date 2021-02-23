
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

#include "kunixstream.h"

#ifdef DEKAF2_HAS_UNIX_SOCKETS

#include "klog.h"
#include "kurl.h"

namespace dekaf2 {


//-----------------------------------------------------------------------------
std::streamsize KUnixIOStream::UnixStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioStream<unixstream>*>(stream_);

		stream->Socket.async_read_some(boost::asio::buffer(sBuffer, iCount),
		[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
		{
			stream->ec = ec;
			iRead = bytes_transferred;
		});

		stream->RunTimed();

		if (iRead == 0 || stream->ec.value() != 0 || !stream->Socket.is_open())
		{
			if (stream->ec.value() == boost::asio::error::eof)
			{
				kDebug(2, "input stream got closed by endpoint {}", stream->sEndpoint);
			}
			else
			{
				kDebug(1, "cannot read from unix stream with endpoint {}: {}",
					   stream->sEndpoint,
					   stream->ec.message());
			}
		}
	}

	return iRead;

} // UnixStreamReader

//-----------------------------------------------------------------------------
std::streamsize KUnixIOStream::UnixStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// We need to loop the writer, as write_some() could have an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs. In typical implementations however the write goes unbuffered in one
	// call, regardless of the size.

	std::streamsize iWrote{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioStream<unixstream>*>(stream_);

		for (;iWrote < iCount;)
		{
			std::size_t iWrotePart { 0 };

			stream->Socket.async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
			[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
			{
				stream->ec = ec;
				iWrotePart = bytes_transferred;
			});

			stream->RunTimed();

			iWrote += iWrotePart;

			if (iWrotePart == 0 || stream->ec.value() != 0 || !stream->Socket.is_open())
			{
				if (stream->ec.value() == boost::asio::error::eof)
				{
					kDebug(2, "output stream got closed by endpoint {}", stream->sEndpoint);
				}
				else
				{
					kDebug(1, "cannot write to unix stream with endpoint {}: {}",
						   stream->sEndpoint,
						   stream->ec.message());
				}
				break;
			}
		}
	}

	return iWrote;

} // UnixStreamWriter

//-----------------------------------------------------------------------------
KUnixIOStream::KUnixIOStream(int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(iSecondsTimeout)
{
}

//-----------------------------------------------------------------------------
KUnixIOStream::KUnixIOStream(KStringViewZ sSocketFile, int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(iSecondsTimeout)
{
	Connect(sSocketFile);
}

//-----------------------------------------------------------------------------
bool KUnixIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	m_Stream.iSecondsTimeout = iSeconds;
	return true;
}

//-----------------------------------------------------------------------------
bool KUnixIOStream::Connect(KStringViewZ sSocketFile)
//-----------------------------------------------------------------------------
{
	m_Stream.Socket.async_connect(boost::asio::local::stream_protocol::endpoint(sSocketFile.c_str()),
								  [&](const boost::system::error_code& ec)
	{
		m_Stream.sEndpoint = sSocketFile;
		m_Stream.ec = ec;
	});

	kDebug(2, "trying to connect to unix domain socket {}", sSocketFile);

	m_Stream.RunTimed();

	if (!Good() || !m_Stream.Socket.is_open())
	{
		kDebug(1, "{}: {}", sSocketFile, Error());
		return false;
	}

	kDebug(2, "connect to unix domain socket {}", sSocketFile);

	return true;

} // connect



//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream(int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUnixStream>(iSecondsTimeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream(KStringViewZ sSocketFile, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUnixStream>(sSocketFile, iSecondsTimeout);
}

} // of namespace dekaf2

#endif // DEKAF2_HAS_UNIX_SOCKETS

