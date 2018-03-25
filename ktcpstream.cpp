
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


#include <limits>
#ifdef DEKAF2_IS_UNIX
#include <poll.h>
#elif DEKAF2_IS_WINDOWS
#include "Mswsock.h" // WSAPoll
#endif
#include "ktcpstream.h"
#include "klog.h"
#include "kurl.h"


namespace dekaf2 {


//-----------------------------------------------------------------------------
KTCPIOStream::POLLSTATE KTCPIOStream::timeout(bool bForReading, Stream_t* stream)
//-----------------------------------------------------------------------------
{

#ifdef DEKAF2_IS_UNIX

	pollfd what;

	short event = (bForReading) ? POLLIN : POLLOUT;
	what.fd     = stream->Socket.lowest_layer().native_handle();
	what.events = event;

	// now check if there is data available coming in from the
	// underlying OS socket (which indicates that there might
	// be openssl data ready soon)
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
	// underlying OS socket (which indicates that there might
	// be openssl data ready soon)
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
std::streamsize KTCPIOStream::TCPStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead{0};

	if (stream_)
	{
		Stream_t* stream = static_cast<Stream_t*>(stream_);

		boost::system::error_code ec;

		if (timeout(true, stream) != POLL_FAILURE)
		{
			iRead = stream->Socket.read_some(boost::asio::buffer(sBuffer, iCount), ec);
		}
		else
		{
			iRead = -1;
		}

		if (iRead < 0 || ec != 0)
		{
			// do some logging
			kDebug(2, "cannot read from stream: - requested {}, got {} bytes",
				   iCount,
				   iRead);
		}
	}

	return iRead;

} // SSLStreamReader

//-----------------------------------------------------------------------------
std::streamsize KTCPIOStream::TCPStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (stream_)
	{
		Stream_t* stream = static_cast<Stream_t*>(stream_);

		boost::system::error_code ec;

		if (timeout(false, stream) == POLL_SUCCESS)
		{
			iWrote = stream->Socket.write_some(boost::asio::buffer(sBuffer, iCount), ec);
		}

		if (iWrote != iCount || ec != 0)
		{
			// do some logging
			kDebug(2, "cannot write to stream");
		}
	}

	return iWrote;

} // SSLStreamWriter

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream()
//-----------------------------------------------------------------------------
: base_type(&m_TCPStreamBuf)
, m_Stream(m_IO_Service)
{
	Timeout(DEFAULT_TIMEOUT);
}

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(const char* sServer, const char* sPort, int iSecondsTimeout)
//-----------------------------------------------------------------------------
: base_type(&m_TCPStreamBuf)
, m_Stream(m_IO_Service)
{
	Timeout(iSecondsTimeout);
	connect(sServer, sPort);
}

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(const KString& sServer, const KString& sPort, int iSecondsTimeout)
//-----------------------------------------------------------------------------
: base_type(&m_TCPStreamBuf)
, m_Stream(m_IO_Service)
{
	Timeout(iSecondsTimeout);
	connect(sServer, sPort);
}

//-----------------------------------------------------------------------------
KTCPIOStream::~KTCPIOStream()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KTCPIOStream::Timeout(int iSeconds)
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
bool KTCPIOStream::connect(const char* sServer, const char* sPort)
//-----------------------------------------------------------------------------
{
	try {

		boost::asio::ip::tcp::resolver Resolver(m_IO_Service);

		boost::asio::ip::tcp::resolver::query query(sServer, sPort);
		auto hosts = Resolver.resolve(query);

		m_ConnectedHost = boost::asio::connect(m_Stream.Socket.lowest_layer(), hosts);

		return true;

	}

	catch (const std::exception& e)
	{
		kException(e);
	}

	catch (...)
	{
		kUnknownException();
	}

	return false;
}



//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream()
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTCPStream>();
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(const KTCPEndPoint& EndPoint)
//-----------------------------------------------------------------------------
{
	std::string sDomain = EndPoint.Domain.get().ToStdString();
	std::string sPort   = EndPoint.Port.get().ToStdString();

	return std::make_unique<KTCPStream>(sDomain, sPort);
}

} // of namespace dekaf2

