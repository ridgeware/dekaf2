
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

#include "ktcpstream.h"
#include "klog.h"
#include "kurl.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
std::streamsize KTCPIOStream::TCPStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioStream<asio_stream_type>*>(stream_);

		stream->Socket.async_read_some(boost::asio::buffer(sBuffer, iCount),
		[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
		{
			stream->ec = ec;
			iRead = bytes_transferred;
		});

		stream->RunTimed();

#ifdef DEKAF2_WITH_KLOG
		if (iRead == 0 || stream->ec.value() != 0 || !stream->Socket.is_open())
		{
			if (stream->ec.value() == boost::asio::error::eof)
			{
				kDebug(2, "input stream got closed by endpoint {}", stream->sEndpoint);
			}
			else
			{
				kDebug(1, "cannot read from {} stream with endpoint {}: {}",
					   "tcp",
					   stream->sEndpoint,
					   stream->ec.message());
			}
		}
#endif
	}

	return iRead;

} // TCPStreamReader

//-----------------------------------------------------------------------------
std::streamsize KTCPIOStream::TCPStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// We need to loop the writer, as write_some() could have an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs.

	std::streamsize iWrote{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioStream<asio_stream_type>*>(stream_);

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
#ifdef DEKAF2_WITH_KLOG
				if (stream->ec.value() == boost::asio::error::eof)
				{
					kDebug(2, "output stream got closed by endpoint {}", stream->sEndpoint);
				}
				else
				{
					kDebug(1, "cannot write to {} stream with endpoint {}: {}",
						   "tcp",
						   stream->sEndpoint,
						   stream->ec.message());
				}
#endif
				break;
			}
		}
	}

	return iWrote;

} // TCPStreamWriter

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(KDuration Timeout)
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(Timeout)
{
}

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(const KTCPEndPoint& Endpoint, KDuration Timeout)
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(Timeout)
{
	Connect(Endpoint);
}

//-----------------------------------------------------------------------------
bool KTCPIOStream::Timeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	m_Stream.Timeout = Timeout;
	return true;
}

//-----------------------------------------------------------------------------
bool KTCPIOStream::Connect(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	auto& sHostname = Endpoint.Domain.get();

	auto hosts = detail::kResolveTCP(sHostname, Endpoint.Port.get(), m_Stream.IOService, m_Stream.ec);

	if (Good())
	{
		boost::asio::async_connect(GetTCPSocket(), hosts,
		                           [&](const boost::system::error_code& ec,
#if (BOOST_VERSION < 106600)
			                           boost::asio::ip::tcp::resolver::iterator endpoint)
#else
			                           const boost::asio::ip::tcp::endpoint& endpoint)
#endif
		{
			m_Stream.sEndpoint.Format("{}:{}",
#if (BOOST_VERSION < 106600)
			endpoint->endpoint().address().to_string(),
			endpoint->endpoint().port());
#else
			endpoint.address().to_string(),
			endpoint.port());
#endif
			m_Stream.ec = ec;
		});

		kDebug(2, "trying to connect to {} {}", "endpoint", Endpoint.Serialize());

		m_Stream.RunTimed();
	}

	if (!Good() || !GetAsioSocket().is_open())
	{
		kDebug(1, "{}: {}", Endpoint.Serialize(), Error());
		return false;
	}

	kDebug(2, "connected to {} {}", "endpoint", m_Stream.sEndpoint);

	return true;

} // connect



//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTCPStream>(Timeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(const KTCPEndPoint& EndPoint, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTCPStream>(EndPoint, Timeout);
}

DEKAF2_NAMESPACE_END
