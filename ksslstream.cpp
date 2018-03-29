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
#include <openssl/ssl.h>
#include "ksslstream.h"
#include "klog.h"


namespace dekaf2 {

//-----------------------------------------------------------------------------
KSSLIOStream::POLLSTATE KSSLIOStream::timeout(bool bForReading, Stream_t* stream)
//-----------------------------------------------------------------------------
{
	if (bForReading)
	{
		// first check if there is still unread data in the internal
		// SSL buffers (this is in openssl)
		if (SSL_pending(stream->Socket.native_handle()))
		{
			// yes - return immediately
			return POLL_SUCCESS;
		}
		// now check in the read BIO buffers
		BIO* bio = SSL_get_rbio(stream->Socket.native_handle());
		if (BIO_pending(bio))
		{
			// yes - return immediately
			return POLL_SUCCESS;
		}
	}

#ifdef DEKAF2_IS_UNIX

	pollfd what;

	short event = (bForReading) ? POLLIN : POLLOUT;
	what.fd     = stream->Socket.lowest_layer().native_handle();
	if (what.fd < 0)
	{
		// this means that the stream is not yet established..
		return POLL_FAILURE;
	}
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
	if (what.fd < 0)
	{
		// this means that the stream is not yet established..
		return POLL_FAILURE;
	}
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

	kDebug(1, "have SSL timeout");

	return POLL_FAILURE;

} // timeout

//-----------------------------------------------------------------------------
bool KSSLIOStream::handshake(boost::asio::ssl::stream_base::handshake_type role, Stream_t* stream)
//-----------------------------------------------------------------------------
{
	if (stream->bNeedHandshake)
	{
		if (timeout(false, stream) == POLL_SUCCESS)
		{
			stream->bNeedHandshake = false;
			stream->Socket.handshake(role, stream->ec);
		}
	}
	return stream->ec.value() == 0;

} // handshake

//-----------------------------------------------------------------------------
std::streamsize KSSLIOStream::SSLStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead{0};

	if (stream_)
	{
		Stream_t* stream = static_cast<Stream_t*>(stream_);

		if (handshake(boost::asio::ssl::stream_base::server, stream)) // SSL servers read first
		{
			if (timeout(true, stream) != POLL_FAILURE)
			{
				iRead = stream->Socket.read_some(boost::asio::buffer(sBuffer, iCount), stream->ec);
			}
			else
			{
				iRead = -1;
			}
		}

		if (iRead < 0 || stream->ec.value() != 0)
		{
			kDebug(2, "cannot read from stream: {}", stream->ec.message());
		}
	}

	return iRead;

} // SSLStreamReader

//-----------------------------------------------------------------------------
std::streamsize KSSLIOStream::SSLStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (stream_)
	{
		Stream_t* stream = static_cast<Stream_t*>(stream_);

		if (handshake(boost::asio::ssl::stream_base::client, stream)) // SSL servers read first
		{
			if (timeout(false, stream) == POLL_SUCCESS)
			{
				iWrote = stream->Socket.write_some(boost::asio::buffer(sBuffer, iCount), stream->ec);
			}
		}

		if (iWrote != iCount || stream->ec.value() != 0)
		{
			kDebug(2, "cannot write to stream: {}", stream->ec.message());
		}
	}

	return iWrote;

} // SSLStreamWriter

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream()
//-----------------------------------------------------------------------------
    : base_type(&m_SSLStreamBuf)
    #if (BOOST_VERSION < 106600)
    , m_Context(m_IO_Service, boost::asio::ssl::context::tlsv12_client)
    #else
    , m_Context(boost::asio::ssl::context::tlsv12_client)
    #endif
    , m_Stream(m_IO_Service, m_Context)
{
	Timeout(DEFAULT_TIMEOUT);
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(const KTCPEndPoint& Endpoint, bool bVerifyCerts, bool bAllowSSLv2v3, int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(&m_SSLStreamBuf)
    #if (BOOST_VERSION < 106600)
    , m_Context(m_IO_Service, boost::asio::ssl::context::tlsv12_client)
    #else
    , m_Context(boost::asio::ssl::context::tlsv12_client)
    #endif
    , m_Stream(m_IO_Service, m_Context)
{
	Timeout(iSecondsTimeout);
	connect(Endpoint, bVerifyCerts, bAllowSSLv2v3);
}

//-----------------------------------------------------------------------------
KSSLIOStream::~KSSLIOStream()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void KSSLIOStream::SetSSLCertificate(const char* sCert, const char* sPem)
//-----------------------------------------------------------------------------
{
	m_Context.use_certificate_chain_file(sCert);
	m_Context.use_private_key_file(sPem, boost::asio::ssl::context::pem);
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Timeout(int iSeconds)
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
bool KSSLIOStream::connect(const KTCPEndPoint& Endpoint, bool bVerifyCerts, bool bAllowSSLv2v3)
//-----------------------------------------------------------------------------
{
	m_Stream.bNeedHandshake = true;

	boost::asio::ssl::context::options options = boost::asio::ssl::context::default_workarounds;

	if (!bAllowSSLv2v3)
	{
		options |= (boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3);
	}
	else
	{
		options |= boost::asio::ssl::context::sslv23;
	}

	m_Context.set_options(options, m_Stream.ec);

	if (Good())
	{
		boost::asio::ip::tcp::resolver Resolver(m_IO_Service);

		boost::asio::ip::tcp::resolver::query query(Endpoint.Domain.get().c_str(),
													Endpoint.Port.get().c_str());
		
		auto hosts = Resolver.resolve(query, m_Stream.ec);

		if (Good())
		{
			if (bVerifyCerts)
			{
				m_Context.set_default_verify_paths();
				m_Stream.Socket.set_verify_mode(boost::asio::ssl::verify_peer);
			}

			m_ConnectedHost = boost::asio::connect(m_Stream.Socket.lowest_layer(), hosts, m_Stream.ec);
		}
	}

	if (!Good())
	{
		kDebug(2, "{}", Error());
		return false;
	}

	return true;

} // connect


//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream()
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLStream>();
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream(const KTCPEndPoint& EndPoint, bool bVerifyCerts, bool bAllowSSLv2v3)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLStream>(EndPoint, bVerifyCerts, bAllowSSLv2v3);
}


} // end of namespace dekaf2

