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
#endif
#include "ksslstream.h"
#include "klog.h"
#include "kurl.h"

using namespace boost::asio;

namespace dekaf2
{

namespace KSSL_detail
{

//-----------------------------------------------------------------------------
KSSLInOutStreamDevice::KSSLInOutStreamDevice(ssl::stream<ip::tcp::socket>& Stream, bool bUseSSL, const int& iTimeoutMilliseconds) noexcept
//-----------------------------------------------------------------------------
    : m_Stream(Stream)
    , m_iTimeoutMilliseconds(iTimeoutMilliseconds)
    , m_bUseSSL(bUseSSL)
    , m_bNeedHandshake(bUseSSL)
{
}

//-----------------------------------------------------------------------------
bool KSSLInOutStreamDevice::timeout(bool bForReading)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_UNIX

	pollfd what;

	short event = (bForReading) ? POLLIN : POLLOUT;
	what.fd     = m_Stream.lowest_layer().native_handle();
	what.events = event;

	int err = ::poll(&what, 1, m_iTimeoutMilliseconds);

	if (err < 0)
	{
		kWarning("KSSLInOutStreamDevice::timeout poll returned {}", strerror(errno));
	}
	else if (err == 1)
	{
		// we do not test for the bit value as any other bit set than
		// the one we requested indicates an error
		if (what.revents == event)
		{
			return false;
		}
	}
	return true;

#endif
	// TODO add a Windows implementation for the timeout
}

//-----------------------------------------------------------------------------
void KSSLInOutStreamDevice::handshake(ssl::stream_base::handshake_type role)
//-----------------------------------------------------------------------------
{
	if (m_bNeedHandshake)
	{
		if (!timeout(false))
		{
			m_bNeedHandshake = false;
			m_Stream.handshake(role);
		}
	}
}

//-----------------------------------------------------------------------------
std::streamsize KSSLInOutStreamDevice::read(char* s, std::streamsize n) noexcept
//-----------------------------------------------------------------------------
{
	try {

		handshake(ssl::stream_base::server); // SSL servers read first

		if (timeout(true))
		{
			return 0;
		}

		if (m_bUseSSL)
		{
			return static_cast<std::streamsize>(
			    m_Stream.read_some(
			        boost::asio::buffer(s, static_cast<std::size_t>(n))
			    )
			);
		}
		else
		{
			return static_cast<std::streamsize>(
			    m_Stream.next_layer().read_some(
			        boost::asio::buffer(s, static_cast<std::size_t>(n))
			    )
			);
		}

	}

	catch (const std::exception& e)
	{
		kException(e);
	}

	catch (...)
	{
		kUnknownException();
	}

	return 0;
}

//-----------------------------------------------------------------------------
std::streamsize KSSLInOutStreamDevice::write(const char* s, std::streamsize n) noexcept
//-----------------------------------------------------------------------------
{
	try {

		handshake(ssl::stream_base::client); // SSL clients write first

		if (timeout(false))
		{
			return 0;
		}

		if (m_bUseSSL)
		{
			return static_cast<std::streamsize>(
				boost::asio::write(
					m_Stream, boost::asio::buffer(s, static_cast<std::size_t>(n))
				)
			);
		}
		else
		{
			return static_cast<std::streamsize>(
				boost::asio::write(
					m_Stream.next_layer(), boost::asio::buffer(s, static_cast<std::size_t>(n))
				)
			);
		}

	}

/*
	catch (const std::exception& e)
	{
		// we cannot log the .what() string as boost is built with COW strings..
		kException(e);
	}
*/
	catch (...)
	{
		kUnknownException();
	}

	return 0;
}


//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream()
//-----------------------------------------------------------------------------
	: base_type(m_Socket, true, m_iTimeoutMilliseconds)
#if (BOOST_VERSION < 106600)
    , m_Context(m_IO_Service, ssl::context::tlsv12_client)
#else
    , m_Context(ssl::context::tlsv12_client)
#endif
    , m_Socket(m_IO_Service, m_Context)
{
	Timeout(DEFAULT_TIMEOUT);
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(const char* sServer, const char* sPort, bool bVerifyCerts, bool bAllowSSLv2v3, int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(m_Socket, true, m_iTimeoutMilliseconds)
#if (BOOST_VERSION < 106600)
    , m_Context(m_IO_Service, ssl::context::tlsv12_client)
#else
    , m_Context(ssl::context::tlsv12_client)
#endif
    , m_Socket(m_IO_Service, m_Context)
{
	Timeout(iSecondsTimeout);
	connect(sServer, sPort, bVerifyCerts, bAllowSSLv2v3);
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(const KString& sServer, const KString& sPort, bool bVerifyCerts, bool bAllowSSLv2v3, int iSecondsTimeout)
//-----------------------------------------------------------------------------
    : base_type(m_Socket, true, m_iTimeoutMilliseconds)
#if (BOOST_VERSION < 106600)
    , m_Context(m_IO_Service, ssl::context::tlsv12_client)
#else
    , m_Context(ssl::context::tlsv12_client)
#endif
    , m_Socket(m_IO_Service, m_Context)
{
	Timeout(iSecondsTimeout);
	connect(sServer, sPort, bVerifyCerts, bAllowSSLv2v3);
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
	m_Context.use_private_key_file(sPem, ssl::context::pem);
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	if (iSeconds > std::numeric_limits<int>::max() / 1000)
	{
		kDebug(2, "KSSLIOStream::timeout: value too big: {}", iSeconds);
		iSeconds = std::numeric_limits<int>::max() / 1000;
	}
	m_iTimeoutMilliseconds = iSeconds * 1000;
	return true;
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::connect(const char* sServer, const char* sPort, bool bVerifyCerts, bool bAllowSSLv2v3)
//-----------------------------------------------------------------------------
{
	try {

		ssl::context::options options = ssl::context::default_workarounds;
		if (!bAllowSSLv2v3)
		{
			options |= (ssl::context::no_sslv2 | ssl::context::no_sslv3);
		}

		m_Context.set_options(options);

		ip::tcp::resolver Resolver(m_IO_Service);

		ip::tcp::resolver::query query(sServer, sPort);
		auto hosts = Resolver.resolve(query);

		if (bVerifyCerts)
		{
			m_Context.set_default_verify_paths();
			m_Socket.set_verify_mode(ssl::verify_peer);
		}

		m_ConnectedHost = boost::asio::connect(m_Socket.lowest_layer(), hosts);

		return true;

	}

/*
	catch (const std::exception& e)
	{
		// we cannot log the .what() string as boost is built with COW strings..
		kException(e);
	}
*/
	catch (...)
	{
		kUnknownException();
	}

	return false;
}

} // end of namespace KSSL_detail

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream()
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLStream>();
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream(const KTCPEndPoint& EndPoint, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLStream>(EndPoint.Domain.get(), EndPoint.Port.get(), bVerifyCerts);
}



} // end of namespace dekaf2

