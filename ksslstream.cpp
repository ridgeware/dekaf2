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


#include "ksslstream.h"
#include "klog.h"

namespace dekaf2
{

namespace KSSL_detail
{

//-----------------------------------------------------------------------------
KSSLInOutStreamDevice::KSSLInOutStreamDevice(ssl::stream<ip::tcp::socket>& Stream, bool bUseSSL)
//-----------------------------------------------------------------------------
    : m_Stream(Stream)
    , m_bUseSSL(bUseSSL)
    , m_bNeedHandshake(bUseSSL)
{
}

//-----------------------------------------------------------------------------
void KSSLInOutStreamDevice::handshake(ssl::stream_base::handshake_type role)
//-----------------------------------------------------------------------------
{
	if (m_bNeedHandshake)
	{
		m_bNeedHandshake = false;
		m_Stream.handshake(role);
	}
}

//-----------------------------------------------------------------------------
std::streamsize KSSLInOutStreamDevice::read(char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	handshake(ssl::stream_base::server); // SSL servers read first
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

//-----------------------------------------------------------------------------
std::streamsize KSSLInOutStreamDevice::write(const char* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	handshake(ssl::stream_base::client); // SSL clients write first
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


//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream()
//-----------------------------------------------------------------------------
	: base_type(m_Socket)
    , m_Context(m_IO_Service, ssl::context::tlsv12_client)
    , m_Socket(m_IO_Service, m_Context)
{
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(const char* sServer, const char* sPort, bool bVerifyCerts)
//-----------------------------------------------------------------------------
    : base_type(m_Socket)
    , m_Context(m_IO_Service, ssl::context::tlsv12_client)
    , m_Socket(m_IO_Service, m_Context)
{
	connect(sServer, sPort, bVerifyCerts);
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(const KString& sServer, const KString& sPort, bool bVerifyCerts)
//-----------------------------------------------------------------------------
    : base_type(m_Socket)
    , m_Context(m_IO_Service, ssl::context::tlsv12_client)
    , m_Socket(m_IO_Service, m_Context)
{
	connect(sServer, sPort, bVerifyCerts);
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
bool KSSLIOStream::connect(const char* sServer, const char* sPort, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	try {

		m_Context.set_options(
					ssl::context::default_workarounds
					| ssl::context::no_sslv2
					| ssl::context::no_sslv3
		);

		m_Context.set_default_verify_paths();

		ip::tcp::resolver Resolver(m_IO_Service);

		ip::tcp::resolver::query query(sServer, sPort);
		auto hosts = Resolver.resolve(query);

		if (bVerifyCerts)
		{
			m_Socket.set_verify_mode(ssl::verify_peer);
		}

		m_ConnectedHost = boost::asio::connect(m_Socket.lowest_layer(), hosts);

		return true;

	}

/*
	catch (const std::exception& e)
	{
		// we cannot log the .what() string as boost is built with COW strings..
		KLog().Exception(e, "Server", "KTCPServer");
	}
*/
	catch (...)
	{
		KLog().Exception("Connect", "KSSLIOStream");
	}

	return false;
}

} // end of namespace KSSL_detail

} // end of namespace dekaf2

