
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

DEKAF2_NAMESPACE_BEGIN


//-----------------------------------------------------------------------------
std::streamsize KUnixStream::UnixStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead{0};

	if (stream_)
	{
		auto& IOStream = *static_cast<KUnixStream*>(stream_);

		IOStream.m_Stream.Socket.async_read_some(boost::asio::buffer(sBuffer, iCount),
		[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
		{
			IOStream.m_Stream.ec = ec;
			iRead = bytes_transferred;
		});

		IOStream.m_Stream.RunTimed();

#ifdef DEKAF2_WITH_KLOG
		if (iRead == 0 || IOStream.m_Stream.ec.value() != 0 || !IOStream.m_Stream.Socket.is_open())
		{
			if (IOStream.m_Stream.ec.value() == boost::asio::error::eof)
			{
				kDebug(2, "input stream got closed by endpoint {}", IOStream.m_Stream.sEndpoint);
			}
			else
			{
				IOStream.SetError(kFormat("cannot read from {} stream: {}",
										  "unix",
										  IOStream.m_Stream.ec.message()));
			}
		}
#endif
	}

	return iRead;

} // UnixStreamReader

//-----------------------------------------------------------------------------
std::streamsize KUnixStream::UnixStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// We need to loop the writer, as write_some() could have an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs. In typical implementations however the write goes unbuffered in one
	// call, regardless of the size.

	std::streamsize iWrote{0};

	if (stream_)
	{
		auto& IOStream = *static_cast<KUnixStream*>(stream_);

		for (;iWrote < iCount;)
		{
			std::size_t iWrotePart { 0 };

			IOStream.m_Stream.Socket.async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
			[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
			{
				IOStream.m_Stream.ec = ec;
				iWrotePart = bytes_transferred;
			});

			IOStream.m_Stream.RunTimed();

			iWrote += iWrotePart;

			if (iWrotePart == 0 || IOStream.m_Stream.ec.value() != 0 || !IOStream.m_Stream.Socket.is_open())
			{
#ifdef DEKAF2_WITH_KLOG
				if (IOStream.m_Stream.ec.value() == boost::asio::error::eof)
				{
					kDebug(2, "output stream got closed by endpoint {}", IOStream.m_Stream.sEndpoint);
				}
				else
				{
					IOStream.SetError(kFormat("cannot write to {} stream: {}",
											  "unix",
											  IOStream.m_Stream.ec.message()));
				}
#endif
				break;
			}
		}
	}

	return iWrote;

} // UnixStreamWriter

//-----------------------------------------------------------------------------
KUnixStream::KUnixStream(KDuration Timeout)
//-----------------------------------------------------------------------------
: base_type(&m_TCPStreamBuf, Timeout)
, m_Stream(Timeout)
{
}

//-----------------------------------------------------------------------------
KUnixStream::KUnixStream(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
: base_type(&m_TCPStreamBuf, Options.GetTimeout())
, m_Stream(Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
bool KUnixStream::Timeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	m_Stream.Timeout = Timeout;
	return true;
}

//-----------------------------------------------------------------------------
bool KUnixStream::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	SetTimeout(Options.GetTimeout());

	SetUnresolvedEndPoint(Endpoint);

	if (!Endpoint.bIsUnixDomain)
	{
		return SetError("not a unix domain socket name");
	}

	auto& sSocketFile = Endpoint.Domain.get();

	GetAsioSocket().async_connect(boost::asio::local::stream_protocol::endpoint(sSocketFile.c_str()),
	[&](const boost::system::error_code& ec)
	{
		m_Stream.sEndpoint  = Endpoint.Serialize(); // for logging only..
		m_Stream.ec         = ec;
		SetEndPointAddress(Endpoint);
	});

	kDebug (3, "trying to connect to {} {}", "unix domain socket", Endpoint);

	m_Stream.RunTimed();

	if (!Good() || !GetAsioSocket().is_open())
	{
		return SetError(m_Stream.ec.message());
	}

	kDebug(2, "connected to {} {}", "unix domain socket", GetEndPointAddress());

	return true;

} // connect



//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUnixStream>(Timeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KUnixStream> CreateKUnixStream(KStringViewZ sSocketFile, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KUnixStream>(sSocketFile, Timeout);
}

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_UNIX_SOCKETS

