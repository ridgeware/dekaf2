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
// https://github.com/JoachimSchurig/CppCDDB/blob/master/asioserver.hpp
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

#pragma once

/// @file ktcpserver.h
/// TCP server implementation with SSL/TLS

#include "bits/kasio.h"
#include "kstream.h"
#include "kstring.h"
#include "kthreadpool.h"
#include <cinttypes>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>

#if (BOOST_VERSION < 107000) || !defined(DEKAF2_IS_MACOS)
	#define DEKAF2_TCPSERVER_CONNECT_TO_STOP
#endif

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A TCP server implementation supporting SSL/TLS.
class DEKAF2_PUBLIC KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using self_type = KTCPServer;

	//-----------------------------------------------------------------------------
	/// Construct a server, but do not yet start it.
	/// @param iPort Port to bind to
	/// @param bSSL If true will use SSL/TLS
	KTCPServer(uint16_t iPort, bool bSSL, uint16_t iMaxConnections = 50);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	//-----------------------------------------------------------------------------
	/// Construct a server, but do not yet start it.
	/// @param sSocketFile Unix domain socket to bind to
	KTCPServer(KStringView sSocketFile, uint16_t iMaxConnections = 50);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	virtual ~KTCPServer();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KTCPServer(const self_type&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KTCPServer(self_type&&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self_type& operator=(const self_type&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self_type& operator=(self_type&&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// allow IPv4 connections only
	void v4_Only()
	//-----------------------------------------------------------------------------
	{
		m_bStartIPv4 = true;
		m_bStartIPv6 = false;
	}

	//-----------------------------------------------------------------------------
	/// allow IPv6 connections only
	void v6_Only()
	//-----------------------------------------------------------------------------
	{
		m_bStartIPv4 = false;
		m_bStartIPv6 = true;
	}

	//-----------------------------------------------------------------------------
	/// allow IPv4 and IPv6 connections (the default after construction)
	void v4_And_6()
	//-----------------------------------------------------------------------------
	{
		m_bStartIPv4 = true;
		m_bStartIPv6 = true;
	}

	//-----------------------------------------------------------------------------
	/// Load the SSL/TLS certificates from files (.pem format)
	bool LoadSSLCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the SSL/TLS certificates from strings (.pem format)
	bool SetSSLCertificates(KStringView sCert, KStringView sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Load DH key exchange primes from file (.pem format)
	bool LoadDHPrimes(KStringViewZ sDHPrimesFile);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set DH key exchange primes from string (.pem format)
	bool SetDHPrimes(KStringViewZ sDHPrimes);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the allowed cipher suites (if empty, an OpenSSL default is used)
	void SetAllowedCipherSuites(KString sAllowedCipherSuites)
	//-----------------------------------------------------------------------------
	{
		m_sAllowedCipherSuites = std::move(sAllowedCipherSuites);
	}

	//-----------------------------------------------------------------------------
	/// Start the server
	/// @param iTimeoutInSeconds Timeout for I/O operations in seconds (default 15)
	/// @param bBlock If true will only return when server is destructed. If false
	/// starts a server thread and returns immediately.
	bool Start(uint16_t iTimeoutInSeconds = 15, bool bBlock = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Stop the server
	bool Stop();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Tests if the server is already running
	bool IsRunning() const
	//-----------------------------------------------------------------------------
	{
		return m_iStarted;
	}

	//-----------------------------------------------------------------------------
	/// Checks if the given port can be bound to
	static bool IsPortAvailable(uint16_t iPort);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Register to shut down on iSignal
	bool RegisterShutdownWithSignals(const std::vector<int>& Signals);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Return server diagnostics like idle threads, total requests, uptime
	KThreadPool::Diagnostics GetDiagnostics() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Shall we log the shutdown?
	/// @param callback callback function called at each shutdown thread with some diagnostics
	void RegisterShutdownCallback(KThreadPool::ShutdownCallback callback);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// return the future once it is available (== once the server has terminated)
	int GetResult();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get error string, if any
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

//-------
protected:
//-------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// struct that could be expanded by implementing classes to carry on
	/// control parameters for one open session.
	struct Parameters
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		//-----------------------------------------------------------------------------
		virtual ~Parameters() = default;
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		bool terminate{false};
		//-----------------------------------------------------------------------------
	};

	using param_t = std::unique_ptr<Parameters>;

	//-----------------------------------------------------------------------------
	/// Virtual hook to override with a completely new session management logic
	/// (either calling Accept(), CreateParameters(), Init() and Request() below,
	/// or anything else)
	virtual void Session(KStream& stream, KStringView sRemoteEndPoint, int iSocketFd);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Virtual hook that is called immediately after accepting a new stream.
	/// Default does nothing. Could be used to set stream parameters. If
	/// return value is false connection is terminated.
	virtual bool Accepted(KStream& stream, KStringView sRemoteEndPoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Virtual hook to send a init message to the client, directly after
	/// accepting the incoming connection. Default sends nothing.
	virtual KString Init(Parameters& parameters);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Virtual hook to process one line of client requests
	virtual KString Request(KStringRef& qstr, Parameters& parameters);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Request the stream timeout requested for this instance (set it in own
	/// session handlers for reading and writing on the stream)
	uint16_t GetTimeout() const
	//-----------------------------------------------------------------------------
	{
		return m_iTimeout;
	}

	//-----------------------------------------------------------------------------
	/// If the derived class needs addtional per-thread control parameters,
	/// define a Parameters class to accomodate those, and return an instance
	/// of this class from CreateParameters()
	virtual param_t CreateParameters();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if the server is in SSL/TLS mode.
	bool IsSSL() const
	//-----------------------------------------------------------------------------
	{
		return m_bIsSSL;
	}

	//-----------------------------------------------------------------------------
	/// Get the port we're bound to - returns 0 for unix sockets
	uint16_t GetPort() const
	//-----------------------------------------------------------------------------
	{
		return m_iPort;
	}

	//-----------------------------------------------------------------------------
	/// Returns true when the server is in the shutdown process
	bool IsShuttingDown() const
	//-----------------------------------------------------------------------------
	{
		return m_bQuit;
	}

	//-----------------------------------------------------------------------------
	/// Set error string and log
	bool SetError(KString sError, bool bNoLogging = false);
	//-----------------------------------------------------------------------------

//-------
private:
//-------

	enum ServerType
	{
		TCPv4 = 1 << 0,
		TCPv6 = 1 << 1,
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		Unix  = 1 << 2
#endif
	};

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	bool TCPServer(bool ipv6);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_UNIX_SOCKETS
	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	bool UnixServer();
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	void RunSession(KStream& stream, KString sRemoteEndPoint, int iSocketFd);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_TCPSERVER_CONNECT_TO_STOP
	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	void StopServerThread(ServerType SType);
	//-----------------------------------------------------------------------------
#endif

	boost::asio::io_service                   m_asio;
	std::vector<std::unique_ptr<std::thread>> m_Servers;
	std::vector<std::shared_ptr<boost::asio::ip::tcp::acceptor>>
	                                          m_TCPAcceptors;
#ifdef DEKAF2_HAS_UNIX_SOCKETS
	std::shared_ptr<boost::asio::local::stream_protocol::acceptor>
		                                      m_UnixAcceptor;
#endif
	std::mutex                                m_StartupMutex;
	std::condition_variable                   m_StartedUp;

	std::vector<int>  m_RegisteredSignals;
	KThreadPool       m_ThreadPool;
#ifdef DEKAF2_HAS_UNIX_SOCKETS
	KString           m_sSocketFile;
#endif
	KString           m_sCert;
	KString           m_sKey;
	KString           m_sPassword;
	KString           m_sDHPrimes;
	KString           m_sAllowedCipherSuites;
	KString           m_sError;
	std::future<int>  m_ResultAsFuture;
	std::atomic<int>  m_iStarted              {     0 };
	uint16_t          m_iPort                 {     0 };
	uint16_t          m_iTimeout              {    15 };
	std::atomic<bool> m_bQuit                 { false };
	bool              m_bBlock                {  true };
	bool              m_bStartIPv4            {  true };
	bool              m_bStartIPv6            {  true };
	bool              m_bHaveSeparatev4Thread { false };
	bool              m_bIsSSL                { false };

}; // KTCPServer

DEKAF2_NAMESPACE_END

