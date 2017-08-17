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
#include "klog.h"

namespace dekaf2
{

using asio::ip::tcp;

//-----------------------------------------------------------------------------
bool KTCPServer::Accepted(KTCPStream& stream, const endpoint_type& remote_endpoint)
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
void KTCPServer::Session(KTCPStream& stream, const endpoint_type& remote_endpoint)
//-----------------------------------------------------------------------------
{
	if (Accepted(stream, remote_endpoint))
	{
		param_t parameters = CreateParameters();

		stream.expires_from_now(boost::posix_time::seconds(m_iTimeout));
		stream << Init(*parameters);

		KString line;

		while (!parameters->terminate && !stream.bad() && !m_bQuit)
		{

			stream.expires_from_now(boost::posix_time::seconds(m_iTimeout));

			if (!stream.ReadLine(line))
			{
				return;
			}

			if (line != "\r")
			{
				stream << Request(line, *parameters);
			}

		}
	}

}

//-----------------------------------------------------------------------------
void KTCPServer::RunSession(KTCPStream& stream, const endpoint_type& remote_endpoint)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KTCPServer: accepting new connection from {} on port {}",
	             to_string(remote_endpoint),
	             m_iPort);

	try
	{
		// run the actual Session code protected by
		// an exception handler
		Session(stream, remote_endpoint);
	}

/*
	catch (std::exception& e)
	{
		// we cannot log the .what() string as boost is built with COW strings..
		KLog().Exception(e, "RunSession", "KTCPServer");
	}
*/
	catch (...)
	{
		KLog().Exception("RunSession", "KTCPServer");
	}

	KLog().debug(3, "KTCPServer: closing connection with {} on port {}",
	             to_string(remote_endpoint),
	             m_iPort);

}

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
			KLog().warning("KTCPServer::Server(): IPv{} listener for port {} could not open",
			               (ipv6) ? '6' : '4',
			               m_iPort);
		}
		else
		{
			while (acceptor.is_open() && !m_bQuit)
			{
				KTCPStream stream;
				endpoint_type remote_endpoint;
				acceptor.accept(*(stream.rdbuf()), remote_endpoint);
				std::thread(&KTCPServer::RunSession, this, std::ref(stream), std::ref(remote_endpoint)).detach();
			}

			if (!acceptor.is_open())
			{
				KLog().warning("KTCPServer::Server(): IPv{} listener for port {} has closed",
				               (ipv6) ? '6' : '4',
				               m_iPort);
			}
		}
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
		KLog().Exception("Server", "KTCPServer");
	}
}

//-----------------------------------------------------------------------------
bool KTCPServer::Start(uint16_t iTimeoutInSeconds, bool bBlock)
//-----------------------------------------------------------------------------
{
	if (IsRunning())
	{
		KLog().warning("KTCPServer::start(): Server is already running on port {}", m_iPort);
		return false;
	}
	m_iTimeout = iTimeoutInSeconds;
	m_bBlock = bBlock;
	m_bQuit = false;
	if (m_bBlock)
	{
		Server(m_bStartIPv6);
	}
	else
	{
		m_ipv6_server = std::make_unique<std::thread>(&KTCPServer::Server, this, m_bStartIPv6);
	}
	return IsRunning();
}


//-----------------------------------------------------------------------------
KTCPServer::~KTCPServer()
//-----------------------------------------------------------------------------
{
	// needed to send some signal to stop the running threads.
	// currently they only quit after an accept on the listen sockets.

	m_bQuit = true;

	// now wait for completion
	if (m_ipv4_server)
	{
		m_ipv4_server->join();
	}

	if (m_ipv6_server)
	{
		m_ipv6_server->join();
	}
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

