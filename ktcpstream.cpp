
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

namespace dekaf2 {

//-----------------------------------------------------------------------------
std::streamsize KTCPIOStream::TCPStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioStream<asiostream>*>(stream_);

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
				kDebug(1, "cannot read from tcp stream with endpoint {}: {}",
					   stream->sEndpoint,
					   stream->ec.message());
			}
		}
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
		auto stream = static_cast<KAsioStream<asiostream>*>(stream_);

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
					kDebug(1, "cannot write to tcp stream with endpoint {}: {}",
						   stream->sEndpoint,
						   stream->ec.message());
				}
				break;
			}
		}
	}

	return iWrote;

} // TCPStreamWriter

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(iSecondsTimeout)
{
}

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(const KTCPEndPoint& Endpoint, int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(&m_TCPStreamBuf)
    , m_Stream(iSecondsTimeout)
{
	Connect(Endpoint);
}

//-----------------------------------------------------------------------------
bool KTCPIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	m_Stream.iSecondsTimeout = iSeconds;
	return true;
}

//-----------------------------------------------------------------------------
bool KTCPIOStream::Connect(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	kDebug(2, "resolving domain {}", Endpoint.Domain.get());

	boost::asio::ip::tcp::resolver Resolver(m_Stream.IOService);
	boost::asio::ip::tcp::resolver::query query(Endpoint.Domain.get().c_str(), Endpoint.Port.Serialize().c_str());
	auto hosts = Resolver.resolve(query, m_Stream.ec);

	if (Good())
	{
		if (kWouldLog(2))
		{
#if (BOOST_VERSION < 106600)
			auto it = hosts;
			decltype(it) ie;
#else
			auto it = hosts.begin();
			auto ie = hosts.end();
#endif
			for (; it != ie; ++it)
			{
				kDebug(2, "resolved to: {}", it->endpoint().address().to_string());
			}
		}

		boost::asio::async_connect(m_Stream.Socket.lowest_layer(),
								   hosts,
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

		kDebug(2, "trying to connect to endpoint {}", Endpoint.Serialize());

		m_Stream.RunTimed();
	}

	if (!Good() || !m_Stream.Socket.is_open())
	{
		kDebug(1, "{}: {}", Endpoint.Serialize(), Error());
		return false;
	}

	kDebug(2, "connected to endpoint {}", Endpoint.Serialize());

	return true;

} // connect



//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTCPStream>(iSecondsTimeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(const KTCPEndPoint& EndPoint, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTCPStream>(EndPoint, iSecondsTimeout);
}

} // of namespace dekaf2

