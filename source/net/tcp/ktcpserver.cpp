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
#include <dekaf2/net/tcp/bits/kasio.h>
#include <boost/system/system_error.hpp>
#ifndef DEKAF2_IS_MSC
#include <boost/exception/diagnostic_information.hpp>
#endif

#include <dekaf2/net/tcp/ktcpserver.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/net/tls/ktlsstream.h>
#include <dekaf2/net/tcp/ktcpstream.h>
#ifdef DEKAF2_HAS_UNIX_SOCKETS
#include <dekaf2/net/tcp/kunixstream.h>
#endif
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/threading/execution/kthreads.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/net/address/knetworkinterface.h>
#include <dekaf2/net/address/kipaddress.h>

DEKAF2_NAMESPACE_BEGIN

using boost::asio::ip::tcp;
using endpoint_type = boost::asio::ip::tcp::acceptor::endpoint_type;

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
/// Builds the X509v3 SAN string for a self-signed certificate based on the
/// given policy and optional explicit domain list.
/// @param Policy controls which local addresses are included
/// @param SANDomains explicitly provided domain names / IP addresses
/// @returns a formatted SAN string like "DNS:localhost,IP:127.0.0.1,IP:::1,IP:192.168.1.5"
static KString BuildSANString(SelfSignedCertSANPolicy Policy, const std::vector<KString>& SANDomains)
//-----------------------------------------------------------------------------
{
	KString sSANs = "DNS:localhost,IP:127.0.0.1,IP:::1";

	// add explicitly provided domains/IPs
	for (auto& sDomain : SANDomains)
	{
		if (kIsIPv4Address(sDomain) || kIsIPv6Address(sDomain, false))
		{
			sSANs += kFormat(",IP:{}", sDomain);
		}
		else
		{
			sSANs += kFormat(",DNS:{}", sDomain);
		}
	}

	if (Policy == SelfSignedCertSANPolicy::AllLocalNets)
	{
		for (auto& net : KNetworkInterface::GetRoutableNetworks())
		{
			sSANs += kFormat(",IP:{}", net.ToString(false));
		}
	}

	return sSANs;

} // BuildSANString

//-----------------------------------------------------------------------------
// deprecated signature
bool KTCPServer::Accepted(KStream& stream, KStringView sRemoteEndPoint)
//-----------------------------------------------------------------------------
{
	return true;
}

//-----------------------------------------------------------------------------
bool KTCPServer::Accepted(std::unique_ptr<KIOStreamSocket>& stream)
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
void KTCPServer::Session(std::unique_ptr<KIOStreamSocket>& stream)
//-----------------------------------------------------------------------------
{
	// provide a fallback to the old Session signature
	Session(*stream, stream->GetEndPointAddress().Serialize(), stream->GetNativeSocket());

} // Session
 
//-----------------------------------------------------------------------------
void KTCPServer::RunSession(std::unique_ptr<KIOStreamSocket>& stream)
//-----------------------------------------------------------------------------
{
	// make sure we adjust this thread's log level to the global log level,
	// even when running repeatedly over a long time
	KLog::getInstance().SyncLevel();

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (!m_iPort)
	{
		kDebug(3, "handling new unix socket connection from {}",
		          stream->GetEndPointAddress());
	}
	else
#endif // DEKAF2_HAS_UNIX_SOCKETS
	{
		kDebug(3, "handling new TCP connection from {} on port {}",
		          stream->GetEndPointAddress(),
		          m_iPort);
	}

	DEKAF2_TRY
	{
		// run the actual Session code protected by
		// an exception handler
		Session(stream);
	}

	DEKAF2_CATCH(const std::exception& ex)
	{
		// This is the lowest stack level of a new thread. If we would
		// not catch the exception here the whole program would abort.
		kDebug(1, ex.what());
	}

	DEKAF2_CATCH(const boost::exception& ex)
	{
#ifndef DEKAF2_IS_MSC
		kDebug(1, boost::diagnostic_information(ex));
#endif
	}

	DEKAF2_CATCH(...)
	{
		kWarning("unknown exception");
	}

	if (!stream)
	{
		// the stream got moved or reset - just return
		return;
	}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (!m_iPort)
	{
		kDebug(3, "closing unix socket connection with {}",
		          stream->GetEndPointAddress());
	}
	else
#endif // DEKAF2_HAS_UNIX_SOCKETS
	{
		kDebug(3, "closing TCP connection with {} on port {}",
		          stream->GetEndPointAddress(),
		          m_iPort);
	}

	// the thread pool keeps the object alive until it is
	// overwritten in round-robin, therefore we have to call
	// Disconnect explicitly now to shut down the connection
	stream->Disconnect();

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
		// build the SAN string based on configured policy and domains
		auto sSANs = BuildSANString(m_SANPolicy, m_SANDomains);

		kDebug(1, "self-signed cert SANs: {}", sSANs);

		if (!m_bStoreNewCerts)
		{
			// create truly ephemeral certs
			auto sError = KRSACert::CheckOrCreateKeyAndCert
			(
				WouldThrowOnError(),
				m_sKey,
				m_sCert,
				m_sPassword,
				"localhost",
				"US",
				"",
				sSANs,
				chrono::years(1),
				KUnixTime(),
				4096
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
				m_sPassword,
				"localhost",
				"US",
				"",
				sSANs,
				chrono::years(1),
				KUnixTime(),
				4096
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
bool KTCPServer::SetupTLSContext()
//-----------------------------------------------------------------------------
{
	if (!IsTLS())
	{
		return true;
	}

	if (m_sCert.empty())
	{
		if (!CreateSelfSignedCertAndKey())
		{
			return false; // already logged
		}
	}

	m_TLSContext = std::make_unique<KTLSContext>(true);

	if (!m_TLSContext->SetTLSCertificates(m_sCert, m_sKey, m_sPassword))
	{
		return SetError(m_TLSContext->CopyLastError()); // already logged
	}

	if (!m_TLSContext->SetDHPrimes(m_sDHPrimes))
	{
		return SetError(m_TLSContext->CopyLastError()); // already logged
	}

	if (!m_TLSContext->SetAllowedCipherSuites(m_sAllowedCipherSuites))
	{
		return SetError(m_TLSContext->CopyLastError()); // already logged
	}

	if (m_HTTPVersion & KHTTPVersion::http2)
	{
		m_TLSContext->SetAllowHTTP2(m_HTTPVersion & KHTTPVersion::http11);
	}

	return true;

} // SetupTLSContext

//-----------------------------------------------------------------------------
bool KTCPServer::SetupTCPAcceptors()
//-----------------------------------------------------------------------------
{
	bool bTryIPv6 = m_bStartIPv6;
	bool bNeedIPv4 = m_bStartIPv4;

	if (bTryIPv6)
	{
		DEKAF2_TRY
		{
			kDebug(2, "opening {} listener on port {}, asking for dual stack", IsTLS() ? "TLS" : "TCP", m_iPort);

			tcp::endpoint local_endpoint(tcp::v6(), m_iPort);
			auto acceptor = std::make_shared<tcp::acceptor>(m_asio, local_endpoint, true); // true means reuse_addr

			boost::asio::ip::v6_only v6_only(false);
			acceptor->get_option(v6_only);

			if (v6_only)
			{
				kDebug(2, "listener opened for IPv6 only, dual stack not supported");
			}
			else
			{
				// dual stack covers IPv4 as well
				bNeedIPv4 = false;
			}

			m_TCPAcceptors.push_back(std::move(acceptor));
		}
		DEKAF2_CATCH(const std::exception& e)
		{
			KStringViewZ sError = e.what();

			// unfortunately we have to investigate the error text from
			// asio to find out about the reason for the throw
			if (sError.starts_with("open: ") && m_bStartIPv4)
			{
				// apparently we tried to open an ipv6 acceptor on a ipv4 only stack, just ignore
				// the error and try to start a pure ipv4 acceptor
				// (the full error string may look like: 'open: Address family not supported by protocol')
				kDebug(1, sError);
				kDebug(1, "it looks as if IPv6 is not supported");
			}
			else
			{
				return SetError(kFormat("exception: {}", sError));
			}
		}
	}

	if (bNeedIPv4)
	{
		kDebug(2, "opening {} listener on port {}, asking for IPv4 only", IsTLS() ? "TLS" : "TCP", m_iPort);

		tcp::endpoint local_endpoint(tcp::v4(), m_iPort);
		auto acceptor = std::make_shared<tcp::acceptor>(m_asio, local_endpoint, true); // true means reuse_addr
		m_TCPAcceptors.push_back(std::move(acceptor));
	}

	if (m_TCPAcceptors.empty())
	{
		return SetError(kFormat("could not open any listener for port {}", m_iPort));
	}

	// post an async_accept for each acceptor
	for (auto& acceptor : m_TCPAcceptors)
	{
		StartTCPAccept(acceptor);
	}

	return true;

} // SetupTCPAcceptors

//-----------------------------------------------------------------------------
void KTCPServer::StartTCPAccept(std::shared_ptr<tcp::acceptor> acceptor)
//-----------------------------------------------------------------------------
{
	// Ownership model for the pre-allocated stream:
	//
	// C++14+ : the stream is init-captured into the async_accept handler
	//          as a std::unique_ptr. The handler closure itself owns the
	//          stream, so it is destroyed whether or not the handler body
	//          runs (e.g. when io_context::stop() discards pending handlers
	//          during shutdown) — no leak.
	//
	// C++11  : init-capture does not exist, so we smuggle ownership through
	//          a raw pointer and reclaim it into a std::unique_ptr at the
	//          top of the handler body. If io_context::stop() discards a
	//          pending handler, that raw pointer leaks — but stop() only
	//          happens at shutdown which is followed by exit, so the OS
	//          reclaims the memory.
	if (IsTLS())
	{
		auto tlsstream = CreateKTLSServer(*m_TLSContext, m_Timeout);
		auto& socket   = tlsstream->GetTCPSocket();
		std::unique_ptr<KIOStreamSocket> stream_base = std::move(tlsstream);

#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
		auto* pStream  = stream_base.release();
#endif

		acceptor->async_accept(socket,
#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
			[this, acceptor, pStream](boost::system::error_code ec)
#else
			[this, acceptor, stream = std::move(stream_base)](boost::system::error_code ec) mutable
#endif
		{
#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
			std::unique_ptr<KIOStreamSocket> stream(pStream);
#endif

			if (IsShuttingDown())
			{
				return;
			}

			if (ec)
			{
				if (ec == boost::asio::error::operation_aborted)
				{
					return;
				}
				kDebug(1, "accept error: {}", ec.message());
			}
			else
			{
				// get the remote endpoint from the accepted socket
				DEKAF2_TRY
				{
					boost::system::error_code ecp;
					auto ep = static_cast<KTLSStream*>(stream.get())->GetTCPSocket().remote_endpoint(ecp);

					if (!ecp)
					{
						KString sEndpoint = to_string(ep);
						kDebug(2, "accepting {} connection from {}", "TLS", sEndpoint);
						stream->SetConnectedEndPointAddress(sEndpoint);
					}
				}
				DEKAF2_CATCH(...) {}

				stream->SetNoDelay(true);

#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
				auto* Stream = stream.release();
				m_ThreadPool.push([ this, Stream ]() mutable
				{
					std::unique_ptr<KIOStreamSocket> moved_stream { Stream };
					RunSession(moved_stream);
				});
#else
				m_ThreadPool.push([ this, moved_stream = std::move(stream) ]() mutable
				{
					RunSession(moved_stream);
				});
#endif
			}

			// chain the next accept
			StartTCPAccept(acceptor);
		});
	}
	else
	{
		auto tcpstream = CreateKTCPStream(m_Timeout);
		auto& socket   = tcpstream->GetTCPSocket();
		std::unique_ptr<KIOStreamSocket> stream_base = std::move(tcpstream);

#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
		auto* pStream  = stream_base.release();
#endif

		acceptor->async_accept(socket,
#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
			[this, acceptor, pStream](boost::system::error_code ec)
#else
			[this, acceptor, stream = std::move(stream_base)](boost::system::error_code ec) mutable
#endif
		{
#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
			std::unique_ptr<KIOStreamSocket> stream(pStream);
#endif

			if (IsShuttingDown())
			{
				return;
			}

			if (ec)
			{
				if (ec == boost::asio::error::operation_aborted)
				{
					return;
				}
				kDebug(1, "accept error: {}", ec.message());
			}
			else
			{
				// get the remote endpoint from the accepted socket
				DEKAF2_TRY
				{
					boost::system::error_code ecp;
					auto ep = static_cast<KTCPStream*>(stream.get())->GetTCPSocket().remote_endpoint(ecp);

					if (!ecp)
					{
						KString sEndpoint = to_string(ep);
						kDebug(2, "accepting {} connection from {}", "TCP", sEndpoint);
						stream->SetConnectedEndPointAddress(sEndpoint);
					}
				}
				DEKAF2_CATCH(...) {}

				stream->SetNoDelay(true);

#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
				auto* Stream = stream.release();
				m_ThreadPool.push([ this, Stream ]() mutable
				{
					std::unique_ptr<KIOStreamSocket> moved_stream { Stream };
					RunSession(moved_stream);
				});
#else
				m_ThreadPool.push([ this, moved_stream = std::move(stream) ]() mutable
				{
					RunSession(moved_stream);
				});
#endif
			}

			// chain the next accept
			StartTCPAccept(acceptor);
		});
	}

} // StartTCPAccept

#ifdef DEKAF2_HAS_UNIX_SOCKETS
//-----------------------------------------------------------------------------
bool KTCPServer::SetupUnixAcceptor()
//-----------------------------------------------------------------------------
{
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
	m_UnixAcceptor = std::make_shared<boost::asio::local::stream_protocol::acceptor>(m_asio, local_endpoint, true); // true == reuse addr

	// make socket read/writeable for world
	kChangeMode(m_sSocketFile, 0777);

	if (!m_UnixAcceptor->is_open())
	{
		return SetError(kFormat("listener for socket file {} could not open", m_sSocketFile));
	}

	StartUnixAccept();

	return true;

} // SetupUnixAcceptor

//-----------------------------------------------------------------------------
void KTCPServer::StartUnixAccept()
//-----------------------------------------------------------------------------
{
	// Ownership model: see StartTCPAccept.
	auto unixstream = CreateKUnixStream(m_Timeout);
	auto& socket    = unixstream->GetUnixSocket();

#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
	auto* pStream   = unixstream.release();
#endif

	m_UnixAcceptor->async_accept(socket,
#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
		[this, pStream](boost::system::error_code ec)
#else
		[this, unixstream = std::move(unixstream)](boost::system::error_code ec) mutable
#endif
	{
#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
		std::unique_ptr<KUnixStream> unixstream(pStream);
#endif

		if (IsShuttingDown())
		{
			return;
		}

		if (ec)
		{
			if (ec == boost::asio::error::operation_aborted)
			{
				return;
			}
			kDebug(1, "accept error: {}", ec.message());
		}
		else
		{
			kDebug(2, "accepting connection from local unix socket");

			unixstream->SetConnectedEndPointAddress(m_sSocketFile);

			// down convert the type to a KIOStreamSocket*
			std::unique_ptr<KIOStreamSocket> stream = std::move(unixstream);

#if !DEKAF2_HAS_CPP_14 || DEKAF2_CLASSIC_ASIO
			auto* Stream = stream.release();
			m_ThreadPool.push([ this, Stream ]() mutable
			{
				std::unique_ptr<KIOStreamSocket> moved_stream { Stream };
				RunSession(moved_stream);
			});
#else
			m_ThreadPool.push([ this, moved_stream = std::move(stream) ]() mutable
			{
				RunSession(moved_stream);
			});
#endif
		}

		// chain the next accept
		StartUnixAccept();
	});

} // StartUnixAccept
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

	if (!kReadAll(sCert, m_sCert))
	{
		return false;
	}

	if (!sKey.empty() && !kReadAll(sKey, m_sKey))
	{
		// clear cert on partial failure to avoid inconsistent state
		m_sCert.clear();
		return false;
	}

	return true;

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
bool KTCPServer::RunServer()
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		// set up acceptors and post async_accept handlers
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (!m_sSocketFile.empty())
		{
			if (!SetupUnixAcceptor())
			{
				return false;
			}
		}
		else
#endif
		{
			if (!SetupTCPAcceptors())
			{
				return false;
			}
		}

		// mark as started before notifying waiters
		++m_iStarted;
		m_StartedUp.notify_all();

		kDebug(2, "server is running");

		// run the io_context - this blocks until stop() is called
		// or all async operations complete
		m_asio.run();

		kDebug(2, "server is closing");

		--m_iStarted;
	}
	DEKAF2_CATCH(const std::exception& e)
	{
		SetError(kFormat("exception: {}", e.what()));
		return false;
	}

	return true;

} // RunServer

//-----------------------------------------------------------------------------
bool KTCPServer::Start(KDuration Timeout, bool bBlock)
//-----------------------------------------------------------------------------
{
	kDebug(2, "starting server");

	m_Timeout = Timeout;
	m_bQuit   = false;

	// reset the io_context for potential restart after a previous Stop()
#if DEKAF2_CLASSIC_ASIO
	m_asio.reset();
#else
	m_asio.restart();
#endif

	std::promise<int> promise;
	m_ResultAsFuture = promise.get_future();

	if (IsRunning())
	{
		promise.set_value(2);
		return SetError(kFormat("Server is already running on port {}", m_iPort));
	}

	// set up TLS context before creating the IO thread - this includes
	// potentially slow RSA key generation for ephemeral certificates
	if (!SetupTLSContext())
	{
		promise.set_value(1);
		return false;
	}

	if (bBlock)
	{
		RunServer();
		promise.set_value(0);
		kDebug(2, "stopped blocking server");
		return true;
	}
	else
	{
		m_IOThread = std::make_unique<std::thread>(kMakeThread(
			[this, Promise = std::move(promise)]() mutable
			{
				Promise.set_value_at_thread_exit(0);
				RunServer();

			}));

		// give the thread time to start up before we return in non-blocking code
		std::unique_lock<std::mutex> Lock(m_StartupMutex);
		m_StartedUp.wait_for(Lock, chrono::milliseconds(1000));

		return IsRunning();
	}

} // Start

//-----------------------------------------------------------------------------
bool KTCPServer::Stop()
//-----------------------------------------------------------------------------
{
	// always set quit flag first, even before checking IsRunning() -
	// the IO thread may be in the setup phase before ++m_iStarted
	bool bWasShuttingDown = m_bQuit.exchange(true);

	if (bWasShuttingDown)
	{
		// another thread is already running Stop()
		return true;
	}

	if (!IsRunning())
	{
		// server not yet running (or already stopped) - but the IO thread
		// may exist and be in the setup phase, so we must still join it
		if (m_IOThread && m_IOThread->joinable())
		{
			m_IOThread->join();
		}
		m_IOThread.reset();
		m_bQuit = false;
		return true;
	}

	KLog::getInstance().SyncLevel();

	kDebug(2, "closing listeners");

	// cancel and close all TCP acceptors - pending async_accept handlers
	// will be called with operation_aborted
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

#ifdef DEKAF2_HAS_UNIX_SOCKETS

	if (m_UnixAcceptor && m_UnixAcceptor->is_open())
	{
		boost::system::error_code ec;
		kDebug(2, "cancelling unix listener");
		m_UnixAcceptor->cancel(ec);
		if (ec)
		{
			SetError(kFormat("error cancelling listener: {}", ec.message()));
		}
		kDebug(2, "closing unix listener");
		m_UnixAcceptor->close(ec);
		if (ec)
		{
			SetError(kFormat("error closing listener: {}", ec.message()));
		}
	}
	m_UnixAcceptor.reset();

	// remove the unix socket file
	if (!m_sSocketFile.empty())
	{
		kRemoveSocket(m_sSocketFile);
	}

#endif

	// Belt-and-suspenders: explicitly stop the io_context. Cancelling the
	// acceptor(s) above normally drains run() by itself (the chained
	// async_accept handlers short-circuit via IsShuttingDown()), but on
	// Windows/IOCP a pending accept may not be woken quickly enough when
	// the service is asked to stop — the process can then sit in
	// SERVICE_STOP_PENDING for the full SCM grace period. stop() unblocks
	// run() immediately on every backend.
	//
	// On C++14+ builds this is leak-safe: the async_accept handlers own
	// their pre-allocated stream via init-capture, so the stream is freed
	// when the handler closure is destroyed regardless of whether it runs.
	// On the C++11 fallback path the handler captures a raw pointer and
	// would leak one pre-allocated stream per stopped acceptor — but since
	// stop() is only called at shutdown and the process exits right after,
	// the OS reclaims that memory anyway.
	m_asio.stop();

	// join the IO thread - run() will return naturally once all
	// cancelled async_accept handlers have completed without chaining
	if (m_IOThread && m_IOThread->joinable())
	{
		m_IOThread->join();
	}
	m_IOThread.reset();

	// release the TLS context
	m_TLSContext.reset();

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
		// capture any previously registered user handler so we can chain it.
		// This is important when multiple KTCPServer instances run in the same
		// process: each one registers a handler, and without chaining only the
		// last registered server would be stopped by a single SIGINT/SIGTERM.
		auto PreviousHandler = SignalHandlers->GetSignalHandler(iSignal);

		// register with iSignal
		SignalHandlers->SetSignalHandler(iSignal, [this, PreviousHandler](int signal)
		{
			kDebug(1, "received {}, shutting down", kTranslateSignal(signal));

			auto SignalHandlers = Dekaf::getInstance().Signals();

			if (SignalHandlers)
			{
				// reset signal handler so that a subsequent signal really
				// terminates the process via the dekaf2 default handler
				SignalHandlers->SetDefaultHandler(signal);
			}

			// stop the tcp server
			this->Stop();

			// chain: call the previously installed user handler so that all
			// registered servers get stopped by a single signal
			if (PreviousHandler)
			{
				PreviousHandler(signal);
			}

		});

		m_RegisteredSignals.push_back(iSignal);
	}

	return true;

} // RegisterShutdownWithSignal


//-----------------------------------------------------------------------------
KTCPServer::KTCPServer(
	uint16_t iPort,
	bool     bTLS,
	uint16_t iMaxThreads,
	bool     bStoreNewCerts,
	KThreadPool::GrowthPolicy Growth,
	KThreadPool::ShrinkPolicy Shrink
)
//-----------------------------------------------------------------------------
	: m_ThreadPool(iMaxThreads, Growth, Shrink)
	, m_iPort(iPort)
	, m_bIsTLS(bTLS)
	, m_bStoreNewCerts(bStoreNewCerts)
{
}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
//-----------------------------------------------------------------------------
KTCPServer::KTCPServer(
	KStringView sSocketFile,
	uint16_t    iMaxThreads,
	KThreadPool::GrowthPolicy Growth,
	KThreadPool::ShrinkPolicy Shrink
)
//-----------------------------------------------------------------------------
	: m_ThreadPool(iMaxThreads, Growth, Shrink)
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

