
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
std::streamsize KTCPStream::direct_read_some(void* sBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead { 0 };

	GetAsioSocket().async_read_some(boost::asio::buffer(sBuffer, iCount),
	[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		m_Stream.ec = ec;
		iRead = bytes_transferred;
	});

	m_Stream.RunTimed();

	return iRead;

} // direct_read_some

//-----------------------------------------------------------------------------
std::streamsize KTCPStream::TCPStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead{0};

	if (stream_)
	{
		auto& IOStream = *static_cast<KTCPStream*>(stream_);

		iRead = IOStream.direct_read_some(sBuffer, iCount);

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
										  "tcp",
										  IOStream.m_Stream.ec.message()));
			}
		}
#endif
	}

	return iRead;

} // TCPStreamReader

//-----------------------------------------------------------------------------
std::streamsize KTCPStream::TCPStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// We need to loop the writer, as write_some() could have an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs.

	std::streamsize iWrote{0};

	if (stream_)
	{
		auto& IOStream = *static_cast<KTCPStream*>(stream_);

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
											  "tcp",
											  IOStream.m_Stream.ec.message()));
				}
#endif
				break;
			}
		}
	}

	return iWrote;

} // TCPStreamWriter

//-----------------------------------------------------------------------------
KTCPStream::KTCPStream(KDuration Timeout)
//-----------------------------------------------------------------------------
: base_type(&m_TCPStreamBuf, Timeout)
, m_Stream(Timeout)
{
}

//-----------------------------------------------------------------------------
KTCPStream::KTCPStream(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
: base_type(&m_TCPStreamBuf, Options.GetTimeout())
, m_Stream(Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
bool KTCPStream::Timeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	m_Stream.Timeout = Timeout;
	return true;
}

//-----------------------------------------------------------------------------
bool KTCPStream::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	SetTimeout(Options.GetTimeout());

	SetUnresolvedEndPoint(Endpoint);

	auto& sHostname = Endpoint.Domain.get();

	auto hosts = KIOStreamSocket::ResolveTCP(sHostname, Endpoint.Port.get(), Options.GetFamily(), m_Stream.IOService, m_Stream.ec);

	if (Good())
	{
		boost::asio::async_connect(GetTCPSocket(), hosts,
		                           [&](const boost::system::error_code& ec,
#if (DEKAF2_CLASSIC_ASIO)
			                           resolver_endpoint_tcp_type endpoint)
#else
			                           const resolver_endpoint_tcp_type& endpoint)
#endif
		{
			m_Stream.sEndpoint  = PrintResolvedAddress(endpoint);
			m_Stream.ec         = ec;
			// parse the endpoint back into our basic KTCPEndpoint
			SetEndPointAddress(m_Stream.sEndpoint);
		});

		kDebug (3, "trying to connect to {} {}", "endpoint", Endpoint);

		m_Stream.RunTimed();
	}

	if (!Good() || !GetAsioSocket().is_open())
	{
		return SetError(m_Stream.ec.message());
	}

	kDebug(2, "connected to {} {}", "endpoint", GetEndPointAddress());

	return true;

} // connect

//-----------------------------------------------------------------------------
void KTCPStream::SetConnectedEndPointAddress(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	// update stream
	m_Stream.sEndpoint = Endpoint.Serialize();
	// update base
	SetEndPointAddress(Endpoint);

} // SetConnectedEndPointAddress



//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTCPStream>(Timeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KTCPStream> CreateKTCPStream(const KTCPEndPoint& EndPoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KTCPStream>(EndPoint, Options);
}

DEKAF2_NAMESPACE_END
