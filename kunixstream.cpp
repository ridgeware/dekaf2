
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


#include <limits>
#ifdef DEKAF2_IS_UNIX
#include <poll.h>
#elif DEKAF2_IS_WINDOWS
#include "Mswsock.h" // WSAPoll
#endif
#include "kunixstream.h"
#include "klog.h"
#include "kurl.h"


namespace dekaf2 {


//-----------------------------------------------------------------------------
KUnixIOStream::POLLSTATE KUnixIOStream::timeout(bool bForReading, Stream_t* stream)
//-----------------------------------------------------------------------------
{

#ifdef DEKAF2_IS_UNIX

	pollfd what;

	short event = (bForReading) ? POLLIN : POLLOUT;
	what.fd     = stream->Socket.lowest_layer().native_handle();
	what.events = event;

	// now check if there is data available coming in from the
	// underlying OS socket
	int err = ::poll(&what, 1, stream->iTimeoutMilliseconds);

	if (err < 0)
	{
		kDebug(1, "poll returned {}", strerror(errno));
	}
	else if (err == 1)
	{
		if ((what.revents & event) == event)
		{
			if ((what.revents & POLLHUP) == POLLHUP)
			{
				return POLL_LAST;
			}
			else
			{
				return POLL_SUCCESS;
			}
		}
	}

#elif DEKAF2_IS_WINDOWS

	WSAPOLLFD what;

	short event = (bForReading) ? POLLIN : POLLOUT;
	what.fd     = stream->Socket.lowest_layer().native_handle();
	what.events = event;

	// now check if there is data available coming in from the
	// underlying OS socket
	int err = WSAPoll(&what, 1, stream->iTimeoutMilliseconds);

	if (err < 0)
	{
		// TODO check if strerror is appropriate on Windows,
		// probably FormatMessage() needs to be used (or simply
		// the naked error number..)
		kDebug(1, "poll returned {}", strerror(WSAGetLastError()));
	}
	else if (err == 1)
	{
		if ((what.revents & event) == event)
		{
			if ((what.revents & POLLHUP) == POLLHUP)
			{
				return POLL_LAST;
			}
			else
			{
				return POLL_SUCCESS;
			}
		}
	}

#endif

	kDebug(1, "have TCP timeout");

	return POLL_FAILURE;

} // timeout

//-----------------------------------------------------------------------------
std::streamsize KUnixIOStream::UnixStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead{0};

	if (stream_)
	{
		Stream_t* stream = static_cast<Stream_t*>(stream_);

		if (timeout(true, stream) != POLL_FAILURE)
		{
			iRead = stream->Socket.read_some(boost::asio::buffer(sBuffer, iCount), stream->ec);
		}
		else
		{
			iRead = -1;
		}

		if (iRead < 0 || stream->ec.value() != 0)
		{
			kDebug(2, "cannot read from stream: {}", stream->ec.message());
		}
	}

	return iRead;

} // SSLStreamReader

//-----------------------------------------------------------------------------
std::streamsize KUnixIOStream::UnixStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (stream_)
	{
		Stream_t* stream = static_cast<Stream_t*>(stream_);

		if (timeout(false, stream) == POLL_SUCCESS)
		{
			iWrote = stream->Socket.write_some(boost::asio::buffer(sBuffer, iCount), stream->ec);
		}

		if (iWrote != iCount || stream->ec.value() != 0)
		{
			kDebug(2, "cannot write to stream: {}", stream->ec.message());
		}
	}

	return iWrote;

} // SSLStreamWriter

//-----------------------------------------------------------------------------
KUnixIOStream::KUnixIOStream()
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(m_IO_Service)
{
	Timeout(DEFAULT_TIMEOUT);
}

//-----------------------------------------------------------------------------
KUnixIOStream::KUnixIOStream(const KString& sSocketFile, int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(m_IO_Service)
{
	Timeout(iSecondsTimeout);
	connect(sSocketFile);
}

//-----------------------------------------------------------------------------
KUnixIOStream::~KUnixIOStream()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KUnixIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	if (iSeconds > std::numeric_limits<int>::max() / 1000)
	{
		kDebug(2, "value too big: {}", iSeconds);
		iSeconds = std::numeric_limits<int>::max() / 1000;
	}
	m_Stream.iTimeoutMilliseconds = iSeconds * 1000;
	return true;
}

//-----------------------------------------------------------------------------
bool KUnixIOStream::connect(const KString& sSocketFile)
//-----------------------------------------------------------------------------
{
	m_Stream.Socket.connect(boost::asio::local::stream_protocol::endpoint(sSocketFile.c_str()), m_Stream.ec);

	if (!Good())
	{
		kDebug(2, "{}", Error());
		return false;
	}

	return true;

} // connect



//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream()
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUnixStream>();
}

//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream(const KString& sSocketFile)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUnixStream>(sSocketFile);
}

} // of namespace dekaf2

