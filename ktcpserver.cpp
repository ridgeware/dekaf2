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

#include <thread>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/basic_socket_iostream.hpp>
#include <boost/system/system_error.hpp>

#include "ktcpserver.h"
#include "ksslstream.h"
#include "ktcpstream.h"
#ifdef DEKAF2_HAS_UNIX_SOCKETS
#include "kunixstream.h"
#endif
#include "klog.h"
#include "kfilesystem.h"
#include "dekaf2.h"
#include "ksignals.h"

namespace dekaf2
{

using boost::asio::ip::tcp;
using endpoint_type = boost::asio::ip::tcp::acceptor::endpoint_type;

class AtomicStarted
{
public:
	AtomicStarted(std::atomic_int& iStarted, int iWhat)
	: m_iStarted(iStarted)
	, m_iWhat(iWhat)
	{
		m_iStarted |= m_iWhat;
	}

	~AtomicStarted()
	{
		m_iStarted &= ~m_iWhat;
	}

private:
	std::atomic_int& m_iStarted;
	int m_iWhat;

}; // AtomicStarted

//-----------------------------------------------------------------------------
/// Converts an endpoint type into a human readable string. Could be used for
/// logging.
static KString to_string(const endpoint_type& endpoint)
//-----------------------------------------------------------------------------
{
	KString sEndpoint;
	sEndpoint.Format("{}:{}",
					 endpoint.address().to_string(),
					 endpoint.port());
	return sEndpoint;
}

//-----------------------------------------------------------------------------
bool KTCPServer::Accepted(KStream& stream, KStringView sRemoteEndPoint)
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
KString KTCPServer::Request(KString& qstr, Parameters& parameters)
//-----------------------------------------------------------------------------
{
	return KString();
}

//-----------------------------------------------------------------------------
void KTCPServer::Session(KStream& stream, KStringView sRemoteEndPoint)
//-----------------------------------------------------------------------------
{
	if (Accepted(stream, sRemoteEndPoint))
	{
		param_t parameters = CreateParameters();

		stream.Write(Init(*parameters));

		KString line;

		while (!parameters->terminate && !stream.InStream().bad() && !m_bQuit)
		{

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

} // Session

//-----------------------------------------------------------------------------
void KTCPServer::RunSession(KStream& stream, KString sRemoteEndPoint)
//-----------------------------------------------------------------------------
{
	// make sure we adjust this thread's log level to the global log level,
	// even when running repeatedly over a long time
	KLog::getInstance().SyncLevel();

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (!m_iPort)
	{
		kDebug(3, "accepting new unix socket connection from {}",
			   sRemoteEndPoint);
	}
	else
#endif
	{
		kDebug(3, "accepting new TCP connection from {} on port {}",
			   sRemoteEndPoint,
			   m_iPort);
	}

	DEKAF2_TRY
	{
		// run the actual Session code protected by
		// an exception handler

		Session(stream, sRemoteEndPoint);
	}
	
	DEKAF2_CATCH(std::exception& e)
	{
		// This is the lowest stack level of a new thread. If we would
		// not catch the exception here the whole program would abort.
		kException(e);
	}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (!m_iPort)
	{
		kDebug(3, "closing unix socket connection with {}",
			   sRemoteEndPoint);
	}
	else
#endif
	{
		kDebug(3, "closing TCP connection with {} on port {}",
			   sRemoteEndPoint,
			   m_iPort);
	}

} // RunSession

//-----------------------------------------------------------------------------
// static
bool KTCPServer::IsPortAvailable(uint16_t iPort)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		boost::asio::io_service io_service;
		tcp::endpoint local_endpoint(tcp::v4(), iPort);
		tcp::acceptor acceptor(io_service, local_endpoint, true); // true means reuse_addr
		acceptor.close();
		return true;
	}

	DEKAF2_CATCH(const std::exception& e)
	{
		// This exception is expected to happen if the port is already in use,
		// we simply want to return with false from the check.
		kDebug(1, "port {} is not available: {}", iPort, e.what());
	}

	return false;

} // IsPortAvailable

//-----------------------------------------------------------------------------
void KTCPServer::TCPServer(bool ipv6)
//-----------------------------------------------------------------------------
{
	AtomicStarted Started(m_iStarted, ipv6 ? ServerType::TCPv6 : ServerType::TCPv4);

	DEKAF2_TRY_EXCEPTION
	
	boost::asio::ip::v6_only v6_only(false);

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
					TCPServer(false);
				}
				else
				{
					// else open v4 explicitly in another thread
					m_ipv4_server = std::make_unique<std::thread>(&KTCPServer::TCPServer, this, false);
				}
			}
		}
	}

	// give feadback to the starting thread that we are now ready to receive requests
	m_bIsListening = true;

	if (!acceptor.is_open())
	{
		kWarning("IPv{} listener for port {} could not open",
		         (ipv6) ? '6' : '4',
		         m_iPort);
		return;
	}

	if (IsSSL())
	{

		// the SSL version of the server

		KSSLContext SSLContext(true, false);

		if (!SSLContext.SetSSLCertificates(m_sCert, m_sKey, m_sPassword))
		{
			return; // already logged
		}

		while (acceptor.is_open() && !m_bQuit)
		{
			auto stream = CreateKSSLServer(SSLContext);
			stream->Timeout(m_iTimeout);
			endpoint_type remote_endpoint;
			acceptor.accept(stream->GetTCPSocket(), remote_endpoint);
			if (!m_bQuit)
			{
#if defined(_MSC_VER) || !defined(DEKAF2_HAS_CPP_14)
				// unfortunately MSC and C++11 does not know how to move a variable into a lambda scope
				auto* Stream = stream.release();
				m_ThreadPool->push([ this, Stream, remote_endpoint ]()
				{
					std::unique_ptr<KSSLStream> moved_stream { Stream };
#else
				m_ThreadPool->push([ this, moved_stream = std::move(stream), remote_endpoint ]()
				{
#endif
					RunSession(*moved_stream, to_string(remote_endpoint));
					// the thread pool keeps the object alive until it is
					// overwritten in round robin, therefore we have to call
					// Disconnect explicitly now to shut down the connection
					moved_stream->Disconnect();
				});
			}
		}

	}
	else
	{

		// the TCP version of the server

		while (acceptor.is_open() && !m_bQuit)
		{
			auto stream = CreateKTCPStream();
			stream->Timeout(m_iTimeout);
			endpoint_type remote_endpoint;
			acceptor.accept(stream->GetTCPSocket(), remote_endpoint);
			if (!m_bQuit)
			{
#if defined(_MSC_VER) || !defined(DEKAF2_HAS_CPP_14)
				// unfortunately MSC and C++11 does not know how to move a variable into a lambda scope
				auto* Stream = stream.release();
				m_ThreadPool->push([ this, Stream, remote_endpoint ]()
				{
					std::unique_ptr<KTCPStream> moved_stream { Stream };
#else
				m_ThreadPool->push([ this, moved_stream = std::move(stream), remote_endpoint ]()
				{
#endif
					RunSession(*moved_stream, to_string(remote_endpoint));
					// the thread pool keeps the object alive until it is
					// overwritten in round robin, therefore we have to call
					// Disconnect explicitly now to shut down the connection
					moved_stream->Disconnect();
				});
			}
		}

	}

	if (!acceptor.is_open() && !m_bQuit)
	{
		kWarning("IPv{} listener for port {} has closed",
					   (ipv6) ? '6' : '4',
					   m_iPort);
	}

	DEKAF2_LOG_EXCEPTION

} // TCPServer

#ifdef DEKAF2_HAS_UNIX_SOCKETS
//-----------------------------------------------------------------------------
void KTCPServer::UnixServer()
//-----------------------------------------------------------------------------
{
	AtomicStarted Started(m_iStarted, ServerType::Unix);

	DEKAF2_TRY_EXCEPTION

	kDebug(2, "opening listener on unix socket at {}", m_sSocketFile);

	// remove an existing socket
	kRemoveFile(m_sSocketFile);

	boost::asio::local::stream_protocol::endpoint local_endpoint(m_sSocketFile.c_str());
	boost::asio::local::stream_protocol::acceptor acceptor(m_asio, local_endpoint, true); // true == reuse addr

	// make socket read/writeable for world
	kChangeMode(m_sSocketFile, 0777);

	// give feadback to the starting thread that we are now ready to receive requests
	m_bIsListening = true;

	if (!acceptor.is_open())
	{
		kWarning("listener for socket file {} could not open", m_sSocketFile);
	}
	else
	{
		while (acceptor.is_open() && !m_bQuit)
		{
			auto stream = CreateKUnixStream();
			acceptor.accept(stream->GetUnixSocket());
			if (!m_bQuit)
			{
				stream->Timeout(m_iTimeout);
#if defined(_MSC_VER) || !defined(DEKAF2_HAS_CPP_14)
				// unfortunately C++11 does not know how to move a variable into a lambda scope
				auto* Stream = stream.release();
				m_ThreadPool->push([ this, Stream ]()
				{
					std::unique_ptr<KUnixStream> moved_stream { Stream };
#else
				m_ThreadPool->push([ this, moved_stream = std::move(stream) ]()
				{
#endif
					RunSession(*moved_stream, m_sSocketFile);
					// the thread pool keeps the object alive until it is
					// overwritten in round robin, therefore we have to call
					// Disconnect explicitly now to shut down the connection
					moved_stream->Disconnect();
				});
			}
		}

		if (!acceptor.is_open() && !m_bQuit)
		{
			kWarning("listener for socket file {} has closed", m_sSocketFile);
		}

		// remove the socket
		kRemoveFile(m_sSocketFile);
	}
	DEKAF2_LOG_EXCEPTION

} // UnixServer
#endif

//-----------------------------------------------------------------------------
bool KTCPServer::LoadSSLCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	m_sPassword = sPassword;
	return kReadAll(sCert, m_sCert) && kReadAll(sKey, m_sKey);

} // SetSSLCertificateFiles

//-----------------------------------------------------------------------------
bool KTCPServer::SetSSLCertificates(KStringView sCert, KStringView sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	m_sPassword = sPassword;
	m_sCert = sCert;
	m_sKey = sKey;
	return true; // TODO add validity check

} // SetSSLCertificates

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
		if (m_sCert.empty() || m_sKey.empty())
		{
			kWarning("cannot start SSL server on port {}, have no certificates", m_iPort);
			return false;
		}
	}

	if (m_bBlock)
	{
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (!m_sSocketFile.empty())
		{
			UnixServer();
		}
		else
#endif
		{
			TCPServer(m_bStartIPv6);
		}
	}
	else
	{
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (!m_sSocketFile.empty())
		{
			m_unix_server = std::make_unique<std::thread>(&KTCPServer::UnixServer, this);
		}
		else
#endif
		{
			m_ipv6_server = std::make_unique<std::thread>(&KTCPServer::TCPServer, this, m_bStartIPv6);
		}

		// give the thread time to start up before we return in non-blocking code
		uint16_t iCt { 0 };
		while (!m_bIsListening && iCt++ < 100)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	return IsRunning();

} // Start

//-----------------------------------------------------------------------------
void KTCPServer::StopServerThread(ServerType SType)
//-----------------------------------------------------------------------------
{
	boost::asio::ip::address localhost;

	// now connect to localhost on the listen port
	switch (SType)
	{
		case TCPv6:
			localhost.from_string("::1");
			break;

		case TCPv4:
			localhost.from_string("127.0.0.1");
			break;

#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case Unix:
			// connect to socket file
			boost::asio::local::stream_protocol::socket s(m_asio);
			s.connect(boost::asio::local::stream_protocol::endpoint(m_sSocketFile.c_str()));
			// wait a little to avoid acceptor exception
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			return;
#endif
	}

	boost::system::error_code ec;
	tcp::endpoint remote_endpoint(localhost, m_iPort);
	boost::asio::ip::tcp::socket socket(m_asio);
	socket.connect(remote_endpoint, ec);
	// wait a little to avoid acceptor exception
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

} // StopServerThread

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
			StopServerThread(ServerType::TCPv4);
			m_ipv4_server->join();
			m_ipv4_server.reset();
		}

		if (m_ipv6_server)
		{
			StopServerThread(ServerType::TCPv6);
			m_ipv6_server->join();
			m_ipv6_server.reset();
		}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (m_unix_server)
		{
			StopServerThread(ServerType::Unix);
			m_unix_server->join();
			m_unix_server.reset();
		}
#endif
		// now check m_iStarted - we may have a blocking server
		// that does not use the unique_ptrs
		if (m_iStarted)
		{
			if (m_iStarted & ServerType::TCPv4)
			{
				StopServerThread(ServerType::TCPv4);
			}

			if (m_iStarted & ServerType::TCPv6)
			{
				StopServerThread(ServerType::TCPv6);
			}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
			if (m_iStarted & ServerType::Unix)
			{
				StopServerThread(ServerType::Unix);
			}
#endif
		}

		m_bIsListening = false;
	}

	return true;

} // Stop


//-----------------------------------------------------------------------------
bool KTCPServer::RegisterShutdownWithSignal(int iSignal)
//-----------------------------------------------------------------------------
{
	if (iSignal)
	{
		auto Signals = Dekaf::getInstance().Signals();

		if (Signals)
		{
			// register with iSignal
			Signals->SetSignalHandler(iSignal, [&](int signal)
			{
				// stop the tcp server
				this->Stop();

				auto Signals = Dekaf::getInstance().Signals();

				if (Signals)
				{
					// reset signal handler to call exit()
					Signals->SetSignalHandler(signal, [](int signal)
					{
						std::exit(0);
					});
				}
			});

			return true;
		}
		else
		{
			kDebug(1, "cannot register with {}, no signal handler thread started", kTranslateSignal(iSignal));
		}
	}

	return false;

} // RegisterShutdownWithSignal


//-----------------------------------------------------------------------------
KTCPServer::KTCPServer(uint16_t iPort, bool bSSL, uint16_t iMaxConnections)
//-----------------------------------------------------------------------------
	: m_ThreadPool(std::make_unique<KThreadPool>(iMaxConnections))
	, m_iPort(iPort)
	, m_bIsSSL(bSSL)
{
}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
//-----------------------------------------------------------------------------
KTCPServer::KTCPServer(KStringView sSocketFile, uint16_t iMaxConnections)
//-----------------------------------------------------------------------------
	: m_sSocketFile(sSocketFile)
	, m_ThreadPool(std::make_unique<KThreadPool>(iMaxConnections))
	, m_iPort(0)
	, m_bIsSSL(false)
{
}
#endif

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
KTCPServer::Parameters::~Parameters()
//-----------------------------------------------------------------------------
{
}

} // end of namespace dekaf2

