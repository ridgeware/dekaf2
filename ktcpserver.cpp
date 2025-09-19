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
// https://github.com/JoachimSchurig/CppCDDB/blob/master/asioserver.cpp
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
#include "bits/kasio.h"
#include <boost/system/system_error.hpp>
#ifndef DEKAF2_IS_MSC
#include <boost/exception/diagnostic_information.hpp>
#endif

#include "ktcpserver.h"
#include "ktlsstream.h"
#include "ktcpstream.h"
#ifdef DEKAF2_HAS_UNIX_SOCKETS
#include "kunixstream.h"
#endif
#include "klog.h"
#include "kfilesystem.h"
#include "dekaf2.h"
#include "ksignals.h"
#include "krsacert.h"

DEKAF2_NAMESPACE_BEGIN

using boost::asio::ip::tcp;
using endpoint_type = boost::asio::ip::tcp::acceptor::endpoint_type;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct AtomicStarted
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	AtomicStarted(std::atomic<int>& iStarted) : m_iStarted(iStarted) { ++m_iStarted; }
	~AtomicStarted() { --m_iStarted; }
	std::atomic<int>& m_iStarted;

}; // AtomicStarted

//-----------------------------------------------------------------------------
/// Converts an endpoint type into a human readable string. Could be used for
/// logging.
static KString to_string(const endpoint_type& endpoint)
//-----------------------------------------------------------------------------
{
	if (endpoint.protocol() == tcp::v6())
	{
		return kFormat("[{}]:{}", endpoint.address().to_string(), endpoint.port());
	}
	else
	{
		return kFormat("{}:{}", endpoint.address().to_string(), endpoint.port());
	}
}

//-----------------------------------------------------------------------------
// deprecated signature
bool KTCPServer::Accepted(KStream& stream, KStringView sRemoteEndPoint)
//-----------------------------------------------------------------------------
{
	return true;
}

//-----------------------------------------------------------------------------
bool KTCPServer::Accepted(KIOStreamSocket& stream)
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
KString KTCPServer::Request(KStringRef& qstr, Parameters& parameters)
//-----------------------------------------------------------------------------
{
	return KString();
}

//-----------------------------------------------------------------------------
// deprecated signature
void KTCPServer::Session(KStream& stream, KStringView sRemoteEndPoint, int iSocketFd)
//-----------------------------------------------------------------------------
{
	if (Accepted(stream, sRemoteEndPoint))
	{
		param_t parameters = CreateParameters();

		stream.Write(Init(*parameters));

		KString line;

		while (!parameters->terminate && !stream.istream().bad() && !m_bQuit)
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
void KTCPServer::Session(KIOStreamSocket& stream)
//-----------------------------------------------------------------------------
{
	Session(stream, stream.GetEndPointAddress().Serialize(), stream.GetNativeSocket());

} // Session
 
//-----------------------------------------------------------------------------
void KTCPServer::RunSession(KIOStreamSocket& stream)
//-----------------------------------------------------------------------------
{
	// make sure we adjust this thread's log level to the global log level,
	// even when running repeatedly over a long time
	KLog::getInstance().SyncLevel();

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (!m_iPort)
	{
		kDebug(3, "handling new unix socket connection from {}",
			   stream.GetEndPointAddress());
	}
	else
#endif
	{
		kDebug(3, "handling new TCP connection from {} on port {}",
			   stream.GetEndPointAddress(),
			   m_iPort);
	}

	DEKAF2_TRY
	{
		// run the actual Session code protected by
		// an exception handler
		Session(stream);
	}
	
	DEKAF2_CATCH(const std::exception& e)
	{
		// This is the lowest stack level of a new thread. If we would
		// not catch the exception here the whole program would abort.
		kException(e);
	}

	DEKAF2_CATCH(const boost::exception& e)
	{
#ifndef DEKAF2_IS_MSC
		kWarning(boost::diagnostic_information(e));
#endif
	}

	DEKAF2_CATCH(...)
	{
		kWarning("unknown exception");
	}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (!m_iPort)
	{
		kDebug(3, "closing unix socket connection with {}",
			   stream.GetEndPointAddress());
	}
	else
#endif
	{
		kDebug(3, "closing TCP connection with {} on port {}",
			   stream.GetEndPointAddress(),
			   m_iPort);
	}

	// the thread pool keeps the object alive until it is
	// overwritten in round-robin, therefore we have to call
	// Disconnect explicitly now to shut down the connection
	stream.Disconnect();

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
bool KTCPServer::CreateSelfSignedCertAndKey()
//-----------------------------------------------------------------------------
{
	if (m_sCert.empty())
	{
		if (!m_bStoreNewCerts)
		{
			// create truly ephemeral certs
			auto sError = KRSACert::CheckOrCreateKeyAndCert
			(
				WouldThrowOnError(),
				m_sKey,
				m_sCert,
				m_sPassword
			);

			if (!sError.empty())
			{
				return SetError(sError);
			}
		}
		else
		{
			// persist cert to disk after creation
			auto sKeyFilename  = KRSACert::GetDefaultPrivateKeyFilename();
			auto sCertFilename = KRSACert::GetDefaultCertFilename();

			auto sError = KRSACert::CheckOrCreateKeyAndCertFile
			(
				WouldThrowOnError(),
				sKeyFilename,
				sCertFilename,
				m_sPassword
			);

			if (!sError.empty())
			{
				return SetError(sError);
			}

			LoadTLSCertificates(sCertFilename, sKeyFilename, m_sPassword);
		}
	}

	return true;

} // CreateSelfSignedCertAndKey

//-----------------------------------------------------------------------------
bool KTCPServer::TCPServer(bool ipv6)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {

	boost::asio::ip::v6_only v6_only(false);

	kDebug(2, "opening {} listener on port {}, asking for {}", IsTLS() ? "TLS" : "TCP", m_iPort, (ipv6) ? "dual stack" : "IPv4 only");

	tcp::endpoint local_endpoint((ipv6) ? tcp::v6() : tcp::v4(), m_iPort);
	auto acceptor = std::make_shared<tcp::acceptor>(m_asio, local_endpoint, true); // true means reuse_addr
	m_TCPAcceptors.push_back(acceptor);

	if (ipv6)
	{
		// check if we listen on both v4 and v6, or only on v6
		acceptor->get_option(v6_only);
		// if acceptor is not open this computer does not support v6
		// (although, the acceptor's constructor would have thrown above already),
		// if v6_only then this computer does not use a dual stack
		if (!acceptor->is_open() || v6_only)
		{
			// test for v6_only flag, although it should be redundant
			if (v6_only)
			{
				kDebug(2, "listener opened for IPv6 only, dual stack not supported");
			}

			if (m_bStartIPv4)
			{
				// check if we can stay in this thread (because the v6 acceptor
				// is not working, and blocking construction is requested)
				if (!acceptor->is_open() && m_bBlock)
				{
					TCPServer(false);
				}
				else
				{
					// else open v4 explicitly in another thread
					m_Servers.push_back(std::make_unique<std::thread>(&KTCPServer::TCPServer, this, false));
					m_bHaveSeparatev4Thread = true;
				}
			}
		}
	}

	m_StartedUp.notify_all();

	// this is redundant, the acceptor's constructor would always throw if
	// it could not open
	if (!acceptor->is_open())
	{
		return SetError(kFormat("IPv{} listener for port {} could not open",
		                        (ipv6) ? '6' : '4',
		                        m_iPort));
	}

	AtomicStarted Started(m_iStarted);

	std::unique_ptr<KTLSContext> TLSContext;

	if (IsTLS())
	{
		// the TLS version of the server

		if (m_sCert.empty())
		{
			if (!CreateSelfSignedCertAndKey())
			{
				return false; // already logged
			}
		}

		TLSContext = std::make_unique<KTLSContext>(true);

		if (!TLSContext->SetTLSCertificates(m_sCert, m_sKey, m_sPassword))
		{
			return SetError(TLSContext->CopyLastError()); // already logged
		}

		if (!TLSContext->SetDHPrimes(m_sDHPrimes))
		{
			return SetError(TLSContext->CopyLastError()); // already logged
		}

		if (!TLSContext->SetAllowedCipherSuites(m_sAllowedCipherSuites))
		{
			return SetError(TLSContext->CopyLastError()); // already logged
		}

		if (m_HTTPVersion & KHTTPVersion::http2)
		{
			TLSContext->SetAllowHTTP2(m_HTTPVersion & KHTTPVersion::http11);
		}
	}

	for (;;)
	{
		endpoint_type remote_endpoint;
		boost::system::error_code ec;
		std::unique_ptr<KIOStreamSocket> stream;

		if (IsTLS())
		{
			auto tlsstream = CreateKTLSServer(*TLSContext.get(), m_Timeout);

			acceptor->accept(tlsstream->GetTCPSocket(), remote_endpoint, ec);

			stream = std::move(tlsstream);
		}
		else
		{
			auto tcpstream = CreateKTCPStream(m_Timeout);

			acceptor->accept(tcpstream->GetTCPSocket(), remote_endpoint, ec);

			stream = std::move(tcpstream);
		}

		if (IsShuttingDown())
		{
			return true;
		}

		if (ec)
		{
			if (ec.value() == boost::asio::error::interrupted || ec.value() == boost::asio::error::try_again)
			{
				kDebug(3, "ignored error: {}", ec.message());
				continue;
			}
			return SetError(kFormat("accept error: {}", ec.message()));
		}

		kDebug(2, "accepting {} connection from {}", IsTLS() ? "TLS" : "TCP", to_string(remote_endpoint));

		stream->SetConnectedEndPointAddress(to_string(remote_endpoint));

#if !DEKAF2_HAS_CPP_14 || defined(DEKAF2_IS_MSC)
		// unfortunately C++11 does not know how to move a variable into a lambda scope
		auto* Stream = stream.release();
		m_ThreadPool.push([ this, Stream, remote_endpoint ]()
		{
			std::unique_ptr<KIOStreamSocket> moved_stream { Stream };
#else
		m_ThreadPool.push([ this, moved_stream = std::move(stream), remote_endpoint ]()
		{
#endif
			RunSession(*moved_stream);
		});

	} // for (;;)

	}
	DEKAF2_CATCH(const std::exception& e)
	{
		KStringViewZ sError = e.what();
		// unfortunately we have to investigate the error text from
		// asio to find out about the reason for the throw
		if (sError.starts_with("open: ") && ipv6 == true && m_TCPAcceptors.empty() && m_bStartIPv4)
		{
			// apparently we tried to open an ipv6 acceptor on a ipv4 only stack, just ignore
			// the error and try to start a pure ipv4 acceptor
			// (the full error string may look like: 'open: Address family not supported by protocol')
			kDebug(1, sError);
			kDebug(1, "it looks as if IPv6 is not supported");
			return TCPServer(false);
		}
		else
		{
			SetError(kFormat("exception: {}", sError));
		}
	}

	kDebug(2, "server is closing");

	return false;

} // TCPServer

#ifdef DEKAF2_HAS_UNIX_SOCKETS
//-----------------------------------------------------------------------------
bool KTCPServer::UnixServer()
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {

	kDebug(2, "opening listener on unix socket at {}", m_sSocketFile);

	// remove an existing socket
	if (!kRemoveSocket(m_sSocketFile))
	{
		return SetError(kFormat("cannot remove existing socket file: {}", m_sSocketFile));
	}

	if (m_sSocketFile.size() > 108)
	{
		kDebug(1, "the socket file name length of {} characters might be too long for some file systems - a typical limit is 108 characters", m_sSocketFile.size());
	}

	boost::asio::local::stream_protocol::endpoint local_endpoint(m_sSocketFile.c_str());
	auto acceptor = std::make_shared<boost::asio::local::stream_protocol::acceptor>(m_asio, local_endpoint, true); // true == reuse addr
	m_UnixAcceptor = acceptor;

	// make socket read/writeable for world
	kChangeMode(m_sSocketFile, 0777);

	m_StartedUp.notify_all();

	// this is redundant, the acceptor's constructor would always throw if
	// it could not open
	if (!acceptor->is_open())
	{
		SetError(kFormat("listener for socket file {} could not open", m_sSocketFile));
	}
	else
	{
		AtomicStarted Started(m_iStarted);

		for (;;)
		{
			auto stream = CreateKUnixStream(m_Timeout);

			boost::system::error_code ec;
			acceptor->accept(stream->GetUnixSocket(), ec);

			if (IsShuttingDown())
			{
				return true;
			}

			if (ec)
			{
				if (ec.value() == boost::asio::error::interrupted || ec.value() == boost::asio::error::try_again)
				{
					kDebug(3, "ignored error: {}", ec.message());
					continue;
				}
				return SetError(kFormat("accept error: {}", ec.message()));
			}

			kDebug(2, "accepting connection from local unix socket");

			stream->SetConnectedEndPointAddress(m_sSocketFile);

#if !DEKAF2_HAS_CPP_14
			// unfortunately C++11 does not know how to move a variable into a lambda scope
			auto* Stream = stream.release();
			m_ThreadPool.push([ this, Stream ]()
			{
				std::unique_ptr<KIOStreamSocket> moved_stream { Stream };
#else
			m_ThreadPool.push([ this, moved_stream = std::move(stream) ]()
			{
#endif
				RunSession(*moved_stream);
			});
		}

		// remove the socket
		kRemoveSocket(m_sSocketFile);
	}

	} 
	DEKAF2_CATCH(const std::exception& e)
	{
		SetError(kFormat("exception: {}", e.what()));
	}

	kDebug(2, "server is closing");

	return false;

} // UnixServer
#endif

//-----------------------------------------------------------------------------
/// Return server diagnostics like idle threads, total requests, uptime
KThreadPool::Diagnostics KTCPServer::GetDiagnostics() const
//-----------------------------------------------------------------------------
{
	return m_ThreadPool.get_diagnostics();

} // GetDiagnostics

//-----------------------------------------------------------------------------
void KTCPServer::RegisterShutdownCallback(KThreadPool::ShutdownCallback callback)
//-----------------------------------------------------------------------------
{
	m_ThreadPool.register_shutdown_callback(std::move(callback));

} // RegisterShutdownCallback

//-----------------------------------------------------------------------------
bool KTCPServer::LoadTLSCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	m_sPassword = sPassword;
	m_sCert.clear();
	m_sKey.clear();
	return kReadAll(sCert, m_sCert) && (sKey.empty() || kReadAll(sKey, m_sKey));

} // LoadTLSCertificates

//-----------------------------------------------------------------------------
bool KTCPServer::SetTLSCertificates(KStringView sCert, KStringView sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	m_sPassword = sPassword;
	m_sCert = sCert;
	m_sKey = sKey;
	return true;

} // SetTLSCertificates

//-----------------------------------------------------------------------------
bool KTCPServer::LoadDHPrimes(KStringViewZ sDHPrimesFile)
//-----------------------------------------------------------------------------
{
	return kReadAll(sDHPrimesFile, m_sDHPrimes);

} // LoadDHPrimes

//-----------------------------------------------------------------------------
bool KTCPServer::SetDHPrimes(KStringViewZ sDHPrimes)
//-----------------------------------------------------------------------------
{
	m_sDHPrimes = sDHPrimes;
	return true;

} // SetDHPrimes

//-----------------------------------------------------------------------------
bool KTCPServer::SetAllowHTTP2(bool bAlsoAllowHTTP1)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP2
	m_HTTPVersion = (bAlsoAllowHTTP1) ? KHTTPVersion::http11 | KHTTPVersion::http2 : KHTTPVersion::http2;
	return true;
#else
	return false;
#endif

} // SetAllowHTTP2

//-----------------------------------------------------------------------------
int KTCPServer::GetResult()
//-----------------------------------------------------------------------------
{
	return m_ResultAsFuture.get();
}

//-----------------------------------------------------------------------------
bool KTCPServer::Start(KDuration Timeout, bool bBlock)
//-----------------------------------------------------------------------------
{
	kDebug(2, "starting server");

	m_Timeout = Timeout;
	m_bBlock  = bBlock;
	m_bQuit   = false;

	std::promise<int> promise;
	m_ResultAsFuture = promise.get_future();

	if (IsRunning())
	{
		promise.set_value(2);
		return SetError(kFormat("Server is already running on port {}", m_iPort));
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
		promise.set_value(0);
		kDebug(2, "stopped blocking server");
		return true;
	}
	else
	{
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (!m_sSocketFile.empty())
		{
			m_Servers.push_back(std::make_unique<std::thread>([this](std::promise<int>&& promise)
			{
				promise.set_value_at_thread_exit(0);
				UnixServer();

			},(std::move(promise))));
		}
		else
#endif
		{
			m_Servers.push_back(std::make_unique<std::thread>([this](std::promise<int>&& promise)
			{
				promise.set_value_at_thread_exit(0);
				TCPServer(m_bStartIPv6);

			},(std::move(promise))));
		}

		// give the thread time to start up before we return in non-blocking code
		std::unique_lock<std::mutex> Lock(m_StartupMutex);
		m_StartedUp.wait_for(Lock, std::chrono::milliseconds(1000));

		return IsRunning();
	}

} // Start

#ifdef DEKAF2_TCPSERVER_CONNECT_TO_STOP

//-----------------------------------------------------------------------------
bool KTCPServer::StopServerThread(ServerType SType)
//-----------------------------------------------------------------------------
{
	boost::asio::ip::address localhost;

	// now connect to localhost on the listen port
	switch (SType)
	{
		case TCPv6:
#ifdef DEKAF2_CLASSIC_ASIO
			localhost.from_string("::1");
#else
			localhost = boost::asio::ip::make_address("::1");
#endif
			break;

		case TCPv4:
#ifdef DEKAF2_CLASSIC_ASIO
			localhost.from_string("127.0.0.1");
#else
			localhost = boost::asio::ip::make_address("127.0.0.1");
#endif
			break;

#ifdef DEKAF2_HAS_UNIX_SOCKETS

		case Unix:
			// connect to socket file
			boost::system::error_code ec;
			boost::asio::local::stream_protocol::socket s(m_asio);
			s.connect(boost::asio::local::stream_protocol::endpoint(m_sSocketFile.c_str()), ec);
			// wait a little to avoid acceptor exception
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			return true;

#endif

	}

	boost::system::error_code ec;
	tcp::endpoint remote_endpoint(localhost, m_iPort);
	boost::asio::ip::tcp::socket socket(m_asio);
	socket.connect(remote_endpoint, ec);

	if (ec)
	{
		kDebug(2, "cannot connect to {}:{}: {}", remote_endpoint.address().to_string(), remote_endpoint.port(), ec.message());
		return false;
	}

	// wait a little to avoid acceptor exception
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	return true;

} // StopServerThread

#endif

//-----------------------------------------------------------------------------
// The process to stop a running server is a bit convoluted. Because the acceptor
// can not be interrupted, we simply fire up a client thread per server to trigger
// the acceptor, and directly afterwards the server thread checks m_bQuit and
// finishes
bool KTCPServer::Stop()
//-----------------------------------------------------------------------------
{
	if (!IsRunning() || IsShuttingDown())
	{
		return true;
	}

	KLog::getInstance().SyncLevel();

	kDebug(2, "closing listeners");

	m_bQuit = true;

	for (auto& Acceptor : m_TCPAcceptors)
	{
		if (Acceptor && Acceptor->is_open())
		{
			boost::system::error_code ec;
			kDebug(2, "cancelling TCP listener");
			Acceptor->cancel(ec);
			if (ec)
			{
				SetError(kFormat("error cancelling listener: {}", ec.message()));
			}
			kDebug(2, "closing TCP listener");
			Acceptor->close(ec);
			if (ec)
			{
				SetError(kFormat("error closing listener: {}", ec.message()));
			}
		}
	}
	m_TCPAcceptors.clear();

#ifdef DEKAF2_TCPSERVER_CONNECT_TO_STOP

	kSleep(chrono::milliseconds(10));

	// give it another shot for older systems
	if (m_bStartIPv6)
	{
		bool bStopped = StopServerThread(TCPv6);
		// it may happen (e.g. in a container without IPv6 support)
		// that we open a dual stack listener, but in fact it only
		// listens on an IPv4 interface. In that case, StopServerThread(TCPv6)
		// would return with false, because it could not connect to the
		// v6 interface. We then _have_ to connect to the v4 interface
		// to shut down the listener.
		if (m_bHaveSeparatev4Thread || !bStopped)
		{
			StopServerThread(TCPv4);
		}
	}
	else if (m_bStartIPv4)
	{
		StopServerThread(TCPv4);
	}

#endif

	m_bHaveSeparatev4Thread	= false;

#ifdef DEKAF2_HAS_UNIX_SOCKETS

	if (m_UnixAcceptor && m_UnixAcceptor->is_open())
	{
		boost::system::error_code ec;
		kDebug(2, "cancelling unix listener");
		m_UnixAcceptor->cancel(ec);
		if (ec)
		{
			SetError(kFormat("error closing listener: {}", ec.message()));
		}
		kDebug(2, "closing unix listener");
		m_UnixAcceptor->close(ec);
		if (ec)
		{
			SetError(kFormat("error closing listener: {}", ec.message()));
		}

#ifdef DEKAF2_TCPSERVER_CONNECT_TO_STOP

		// give it another shot for older systems
		StopServerThread(Unix);

#endif

	}
	m_UnixAcceptor.reset();

#endif

	for (auto& Server : m_Servers)
	{
		Server->join();
	}
	m_Servers.clear();

	kDebug(2, "listeners closed");

	return true;

} // Stop


//-----------------------------------------------------------------------------
bool KTCPServer::RegisterShutdownWithSignals(const std::vector<int>& Signals)
//-----------------------------------------------------------------------------
{
	auto SignalHandlers = Dekaf::getInstance().Signals();

	if (!SignalHandlers)
	{
		return SetError("cannot register shutdown handlers, no signal handler thread started");
	}

	for (auto iSignal : Signals)
	{
		// register with iSignal
		SignalHandlers->SetSignalHandler(iSignal, [&](int signal)
		{
			kDebug(1, "received {}, shutting down", kTranslateSignal(signal))

			auto SignalHandlers = Dekaf::getInstance().Signals();

			if (SignalHandlers)
			{
				// reset signal handler
				SignalHandlers->SetDefaultHandler(signal);
			}

			// stop the tcp server
			this->Stop();

		});

		m_RegisteredSignals.push_back(iSignal);
	}

	return true;

} // RegisterShutdownWithSignal


//-----------------------------------------------------------------------------
KTCPServer::KTCPServer(uint16_t iPort, bool bTLS, uint16_t iMaxConnections, bool bStoreNewCerts)
//-----------------------------------------------------------------------------
	: m_ThreadPool(iMaxConnections)
	, m_iPort(iPort)
	, m_bIsTLS(bTLS)
	, m_bStoreNewCerts(bStoreNewCerts)
{
}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
//-----------------------------------------------------------------------------
KTCPServer::KTCPServer(KStringView sSocketFile, uint16_t iMaxConnections)
//-----------------------------------------------------------------------------
	: m_ThreadPool(iMaxConnections)
	, m_sSocketFile(sSocketFile)
	, m_iPort(0)
{
}
#endif

//-----------------------------------------------------------------------------
KTCPServer::~KTCPServer()
//-----------------------------------------------------------------------------
{
	auto SignalHandlers = Dekaf::getInstance().Signals();

	if (SignalHandlers)
	{
		for (auto iSignal : m_RegisteredSignals)
		{
			// reset signal handler to call exit()
			SignalHandlers->SetDefaultHandler(iSignal);
		}
		m_RegisteredSignals.clear();
	}
	
	Stop();

	kSafeErase(m_sCert);
	kSafeErase(m_sKey);
	kSafeErase(m_sPassword);
	kSafeErase(m_sDHPrimes);
}

//-----------------------------------------------------------------------------
KTCPServer::param_t KTCPServer::CreateParameters()
//-----------------------------------------------------------------------------
{
	return std::make_unique<Parameters>();
}

DEKAF2_NAMESPACE_END

