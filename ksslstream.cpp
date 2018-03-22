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
KSSLInOutStreamDevice::KSSLInOutStreamDevice(ssl::stream<ip::tcp::socket>& Stream, bool bUseSSL, const int& iTimeoutMilliseconds)
//-----------------------------------------------------------------------------
    : m_Stream(Stream)
    , m_iTimeoutMilliseconds(iTimeoutMilliseconds)
    , m_bUseSSL(bUseSSL)
    , m_bNeedHandshake(bUseSSL)
{
}

//-----------------------------------------------------------------------------
KSSLInOutStreamDevice::POLLSTATE KSSLInOutStreamDevice::timeout(bool bForReading)
//-----------------------------------------------------------------------------
{
	if (m_bFailed)
	{
		return POLL_FAILURE;
	}

#ifdef DEKAF2_IS_UNIX

	pollfd what;

	short event = (bForReading) ? POLLIN : POLLOUT;
	what.fd     = m_Stream.lowest_layer().native_handle();
	what.events = event;

	int err = ::poll(&what, 1, m_iTimeoutMilliseconds);

	if (err < 0)
	{
		kWarning("poll returned {}", strerror(errno));
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

	kDebug(1, "have SSL timeout");

	m_bFailed = true;

	return POLL_FAILURE;

#endif
	// TODO add a Windows implementation for the timeout
}

//-----------------------------------------------------------------------------
void KSSLInOutStreamDevice::handshake(ssl::stream_base::handshake_type role)
//-----------------------------------------------------------------------------
{
	if (m_bFailed)
	{
		return;
	}

	if (m_bNeedHandshake)
	{
		if (timeout(false) == POLL_SUCCESS)
		{
			m_bNeedHandshake = false;
			m_Stream.handshake(role);
		}
	}
}

//-----------------------------------------------------------------------------
size_t KSSLInOutStreamDevice::read_with_timeout(char* s, size_t n)
//-----------------------------------------------------------------------------
{
	if (m_bFailed)
	{
		return 0;
	}

	size_t iRead { 0 };

	do
	{
		POLLSTATE ps = timeout(true);

		if (ps == POLL_FAILURE)
		{
			break;
		}

		boost::system::error_code ec;
		iRead += m_Stream.read_some(boost::asio::buffer(s + iRead, n - iRead), ec);

		if (ps == POLL_LAST || ec != 0)
		{
			break;
		}

	} while (iRead < n);

	return iRead;
}

//-----------------------------------------------------------------------------
std::streamsize KSSLInOutStreamDevice::read(char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	if (m_bFailed)
	{
		// it is weird, but boost::iostreams expect -1 to be returned
		// for a failed read(), but an exception for a failed write()
		return -1;
	}

	try {

		handshake(ssl::stream_base::server); // SSL servers read first

		auto ps = timeout(true);

		if (ps == POLL_FAILURE)
		{
			return -1;
		}

		if (ps == POLL_LAST)
		{
			return -1;
		}

		if (m_bUseSSL)
		{
			return static_cast<std::streamsize>(
				m_Stream.read_some(boost::asio::buffer(s, n))
//				read_with_timeout(s, static_cast<std::size_t>(n))
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

	m_bFailed = true;

	return -1;
}

//-----------------------------------------------------------------------------
std::streamsize KSSLInOutStreamDevice::write(const char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	if (m_bFailed)
	{
		return 0;
	}

	try {
		
		handshake(ssl::stream_base::client); // SSL clients write first

		if (timeout(false) == POLL_FAILURE)
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

	catch (const std::exception& e)
	{
		kException(e);
	}

	catch (...)
	{
		kUnknownException();
	}

	m_bFailed = true;

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
		kDebug(2, "value too big: {}", iSeconds);
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
		else
		{
			options |= ssl::context::sslv23;
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
KTCPInOutStreamDevice::KTCPInOutStreamDevice(tcpstream& Stream, const int& iTimeoutMilliseconds)
//-----------------------------------------------------------------------------
: m_Stream(Stream)
, m_iTimeoutMilliseconds(iTimeoutMilliseconds)
{
}

//-----------------------------------------------------------------------------
KTCPInOutStreamDevice::POLLSTATE KTCPInOutStreamDevice::timeout(bool bForReading)
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
		kWarning("poll returned {}", strerror(errno));
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

	kDebug(1, "have TCP timeout");

	return POLL_FAILURE;

#endif
	// TODO add a Windows implementation for the timeout
}


//-----------------------------------------------------------------------------
std::streamsize KTCPInOutStreamDevice::read(char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	try {

		if (timeout(true) == POLL_FAILURE)
		{
			return -1;
		}
		
		return static_cast<std::streamsize>(
			m_Stream.read_some(boost::asio::buffer(s, n))
		);

	}

	catch (const std::exception& e)
	{
		kException(e);
	}

	catch (...)
	{
		kUnknownException();
	}

	return -1;

} // read

//-----------------------------------------------------------------------------
std::streamsize KTCPInOutStreamDevice::write(const char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	try {

		if (timeout(false) == POLL_FAILURE)
		{
			return 0;
		}

		return static_cast<std::streamsize>(
			boost::asio::write(
				m_Stream, boost::asio::buffer(s, static_cast<std::size_t>(n))
			)
		);

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

} // write

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream()
//-----------------------------------------------------------------------------
: base_type(m_Socket, m_iTimeoutMilliseconds)
, m_Socket(m_IO_Service)
{
	Timeout(DEFAULT_TIMEOUT);
}

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(const char* sServer, const char* sPort, int iSecondsTimeout)
//-----------------------------------------------------------------------------
: base_type(m_Socket, m_iTimeoutMilliseconds)
, m_Socket(m_IO_Service)
{
	Timeout(iSecondsTimeout);
	connect(sServer, sPort);
}

//-----------------------------------------------------------------------------
KTCPIOStream::KTCPIOStream(const KString& sServer, const KString& sPort, int iSecondsTimeout)
//-----------------------------------------------------------------------------
: base_type(m_Socket, m_iTimeoutMilliseconds)
, m_Socket(m_IO_Service)
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
	m_iTimeoutMilliseconds = iSeconds * 1000;
	return true;
}

//-----------------------------------------------------------------------------
bool KTCPIOStream::connect(const char* sServer, const char* sPort)
//-----------------------------------------------------------------------------
{
	try {

		ip::tcp::resolver Resolver(m_IO_Service);

		ip::tcp::resolver::query query(sServer, sPort);
		auto hosts = Resolver.resolve(query);

		if (m_Expect <= 0)
		{
			m_Expect = -1;
		}

		m_ConnectedHost = boost::asio::connect(m_Socket.lowest_layer(), hosts);

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





} // end of namespace KSSL_detail

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
	return std::make_unique<KSSLStream>(EndPoint.Domain.get(), EndPoint.Port.get(), bVerifyCerts, bAllowSSLv2v3);
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

} // end of namespace dekaf2

