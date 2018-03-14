/*
//-----------------------------------------------------------------------------//
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

// large portions of this code are taken from
// https://github.com/JoachimSchurig/CppCDDB/blob/master/KTCPServer.cpp
// which is under a BSD style open source license

//  Copyright © 2016 Joachim Schurig. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/basic_socket_iostream.hpp>
#include <boost/system/system_error.hpp>

#include "ktcpserver.h"
#include "ksslstream.h"
#include "klog.h"

namespace dekaf2
{

using asio::ip::tcp;

//-----------------------------------------------------------------------------
bool KTCPServer::Accepted(KStream& stream, const endpoint_type& remote_endpoint)
//-----------------------------------------------------------------------------
{
	return true;
}

//-----------------------------------------------------------------------------
KString KTCPServer::Init(Parameters& parameters)
//-----------------------------------------------------------------------------
{
	return KString();
}

//-----------------------------------------------------------------------------
KString KTCPServer::Request(const KString& qstr, Parameters& parameters)
//-----------------------------------------------------------------------------
{
	return KString();
}

//-----------------------------------------------------------------------------
void KTCPServer::Session(KStream& stream, const endpoint_type& remote_endpoint)
//-----------------------------------------------------------------------------
{
	if (Accepted(stream, remote_endpoint))
	{
		param_t parameters = CreateParameters();

		ExpiresFromNow(stream, m_iTimeout);
		stream << Init(*parameters);

		KString line;

		while (!parameters->terminate && !stream.InStream().bad() && !m_bQuit)
		{

			ExpiresFromNow(stream, m_iTimeout);

			if (!stream.ReadLine(line))
			{
				return;
			}

			KString sOut(Request(line, *parameters));
			if (!sOut.empty())
			{
				stream.Write(sOut).Flush();
			}
		}
	}

}

//-----------------------------------------------------------------------------
void KTCPServer::RunSession(std::unique_ptr<KStream> stream, const endpoint_type& remote_endpoint)
//-----------------------------------------------------------------------------
{
	++m_iOpenConnections;

	kDebug(3, "accepting new connection from {} on port {}",
	             to_string(remote_endpoint),
	             m_iPort);

	try
	{
		if (IsSSL())
		{
			// set connection timeout once, other than for the
			// non-SSL connections where we have to repeatedly
			// call ExpiresFromNow()
			KSSLStream& s = static_cast<KSSLStream&>(*stream);
			s.Timeout(m_iTimeout);
		}

		// run the actual Session code protected by
		// an exception handler
		Session(*stream, remote_endpoint);
	}

	catch (std::exception& e)
	{
		kException(e);
	}

	catch (...)
	{
		kUnknownException();
	}

	kDebug(3, "closing connection with {} on port {}",
	             to_string(remote_endpoint),
	             m_iPort);

	--m_iOpenConnections;

}

//-----------------------------------------------------------------------------
void KTCPServer::ExpiresFromNow(KStream& stream, long iSeconds)
//-----------------------------------------------------------------------------
{
	if (!IsSSL())
	{
		KTCPStream& s = static_cast<KTCPStream&>(stream);
		s.expires_from_now(boost::posix_time::seconds(iSeconds));
	}
}

//-----------------------------------------------------------------------------
// static
bool KTCPServer::IsPortAvailable(uint16_t iPort)
//-----------------------------------------------------------------------------
{
	try
	{
		asio::io_service io_service;
		tcp::endpoint local_endpoint(tcp::v4(), iPort);
		tcp::acceptor acceptor(io_service, local_endpoint, true); // true means reuse_addr
		acceptor.close();
		return true;
	}

	catch (const std::exception& e)
	{
		kException(e);
	}

	return false;

} // IsPortAvailable

//-----------------------------------------------------------------------------
void KTCPServer::Server(bool ipv6)
//-----------------------------------------------------------------------------
{
	try
	{
		asio::ip::v6_only v6_only(false);

		tcp::endpoint local_endpoint((ipv6) ? tcp::v6() : tcp::v4(), m_iPort);
		tcp::acceptor acceptor(m_asio, local_endpoint, true); // true means reuse_addr

		if (ipv6)
		{
			// check if we listen on both v4 and v6, or only on v6
			acceptor.get_option(v6_only);
			// if acceptor is not open this computer does not support v6
			// if v6_only then this computer does not use a dual stack
			if (!acceptor.is_open() || v6_only)
			{
				if (m_bStartIPv4)
				{
					// check if we can stay in this thread (because the v6 acceptor
					// is not working, and blocking construction is requested)
					if (!acceptor.is_open() && m_bBlock)
					{
						Server(false);
					}
					else
					{
						// else open v4 explicitly in another thread
						m_ipv4_server = std::make_unique<std::thread>(&KTCPServer::Server, this, false);
					}
				}
			}
		}

		if (!acceptor.is_open())
		{
			kWarning("IPv{} listener for port {} could not open",
			               (ipv6) ? '6' : '4',
			               m_iPort);
		}
		else
		{
			while (acceptor.is_open() && !m_bQuit)
			{
				if (IsSSL())
				{
					auto ustream = CreateKSSLStream();
					ustream->SetSSLCertificate(m_sCert.c_str(), m_sPem.c_str());
					endpoint_type remote_endpoint;
					acceptor.accept(ustream->GetTCPSocket(), remote_endpoint);
					if (!m_bQuit)
					{
						std::thread(&KTCPServer::RunSession, this, std::move(ustream), std::ref(remote_endpoint)).detach();
					}
				}
				else
				{
					auto ustream = CreateKTCPStream();
					endpoint_type remote_endpoint;
					acceptor.accept(*(ustream->rdbuf()), remote_endpoint);
					if (!m_bQuit)
					{
						std::thread(&KTCPServer::RunSession, this, std::move(ustream), std::ref(remote_endpoint)).detach();
					}
				}

				while (m_iOpenConnections > m_iMaxConnections)
				{
					// this may actually trigger a few threads too late,
					// but we do not care too much about
					usleep(100);
				}

			}

			if (!acceptor.is_open() && !m_bQuit)
			{
				kWarning("IPv{} listener for port {} has closed",
				               (ipv6) ? '6' : '4',
				               m_iPort);
			}
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
}

//-----------------------------------------------------------------------------
bool KTCPServer::SetSSLCertificate(KStringView sCert, KStringView sPem)
//-----------------------------------------------------------------------------
{
	m_sCert = sCert;
	m_sPem = sPem;
	return true; // TODO add validity check
}

//-----------------------------------------------------------------------------
bool KTCPServer::Start(uint16_t iTimeoutInSeconds, bool bBlock)
//-----------------------------------------------------------------------------
{
	if (IsRunning())
	{
		kWarning("Server is already running on port {}", m_iPort);
		return false;
	}
	m_iTimeout = iTimeoutInSeconds;
	m_bBlock = bBlock;
	m_bQuit = false;

	if (IsSSL())
	{
		if (m_sCert.empty() || m_sPem.empty())
		{
			kWarning("cannot start SSL server on port {}, have no certificates", m_iPort);
			return false;
		}
	}

	if (m_bBlock)
	{
		Server(m_bStartIPv6);
	}
	else
	{
		m_ipv6_server = std::make_unique<std::thread>(&KTCPServer::Server, this, m_bStartIPv6);
	}

	sleep(1);

	return IsRunning();
}

//-----------------------------------------------------------------------------
void KTCPServer::StopServerThread(bool ipv6)
//-----------------------------------------------------------------------------
{
	// now connect to localhost on the listen port
	boost::asio::ip::address localhost;
	if (ipv6)
	{
		localhost.from_string("::1");
	}
	else
	{
		localhost.from_string("127.0.0.1");
	}
	tcp::endpoint remote_endpoint(localhost, m_iPort);

	boost::asio::io_service io_service;
	boost::asio::ip::tcp::socket socket(io_service);

	socket.connect(remote_endpoint);
}

//-----------------------------------------------------------------------------
// The process to stop a running server is a bit convoluted. Because the acceptor
// can not be interrupted, we simply fire up a client thread per server to trigger
// the acceptor, and directly afterwards the server thread checks m_bQuit and
// finishes
bool KTCPServer::Stop()
//-----------------------------------------------------------------------------
{
	if (IsRunning())
	{
		m_bQuit = true;

		if (m_ipv4_server)
		{
			std::thread(&KTCPServer::StopServerThread, this, false).detach();
		}

		if (m_ipv6_server)
		{
			std::thread(&KTCPServer::StopServerThread, this, true).detach();
		}

		// now wait for completion
		if (m_ipv4_server)
		{
			m_ipv4_server->join();
			m_ipv4_server.reset();
		}

		if (m_ipv6_server)
		{
			m_ipv6_server->join();
			m_ipv6_server.reset();
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
KTCPServer::~KTCPServer()
//-----------------------------------------------------------------------------
{
	Stop();
}

//-----------------------------------------------------------------------------
KTCPServer::param_t KTCPServer::CreateParameters()
//-----------------------------------------------------------------------------
{
	return std::make_unique<Parameters>();
}

//-----------------------------------------------------------------------------
KString KTCPServer::to_string(const endpoint_type& endpoint)
//-----------------------------------------------------------------------------
{
	KString sEndpoint;
	sEndpoint.Format("{}:{}",
	                 endpoint.address().to_string(),
	                 endpoint.port());
	return sEndpoint;
}

//-----------------------------------------------------------------------------
KTCPServer::Parameters::~Parameters()
//-----------------------------------------------------------------------------
{
}

} // end of namespace dekaf2

