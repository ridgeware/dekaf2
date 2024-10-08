/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

#pragma once

#include "krestserver.h"
#include "ktcpserver.h"
#include "kpoll.h"
#include "kwebsocket.h"
#include "kerror.h"
#include <csignal>

/// @file krest.h
/// HTTP REST service implementation

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KREST : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KREST() = default;
	~KREST();

	static constexpr KStringViewZ CLI_SIM_AGENT {"cli sim agent"};

	enum ServerType { UNDEFINED, HTTP, CGI, FCGI, LAMBDA, CLI, SIMULATE_HTTP
#ifdef DEKAF2_HAS_UNIX_SOCKETS
	                  , UNIX
#endif
	                };

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Options for simulation mode
	struct SimulationParms
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// filename to read HTTP request from
		KString sFilename;
		/// API to call
		KResource API;
		/// HTTP method to use
		KHTTPMethod Method;
		/// Post body if not read from file
		KString sBody;
		/// Additional request headers
		KHTTPRequestHeaders::KHeaderMap AdditionalRequestHeaders;

	}; // SimulationParms

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// define options for the rest service
	struct Options : public KRESTServer::Options
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// Server type (UNDEFINED, HTTP, CGI, FCGI, LAMBDA, CLI, SIMULATE_HTTP, UNIX)
		ServerType Type { UNDEFINED };
		/// listen port (default none)
		uint16_t iPort { 0 };
		/// max simultaneous connections (default 20)
		uint16_t iMaxConnections { 20 };
		/// timeout in seconds (default 5)
		uint16_t iTimeout { 5 };
		/// signals that will shutdown the server (default SIGINT, SIGTERM)
		std::vector<int> RegisterSignalsForShutdown { SIGINT, SIGTERM };
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		/// unix socket file to listen
		KString sSocketFile;
#endif
		/// start as blocking server in same thread or unblocking in separate thread
		bool bBlocking { true };
		/// are the PEM cert and key and dh prime filenames (default) or buffers with the actual PEMs?
		bool bPEMsAreFilenames { true };
		/// the PEM certificate
		KString sCert;
		/// the PEM private key
		KString sKey;
		/// the password (if any) needed to decode sCert and sKey
		KString sTLSPassword;
		/// the primes used for non-elliptic-curve Diffie-Hellman perfect forward secrecy key exchange (.pem format)
		/// (leave empty to use elliptic curve DHE only)
		KString sDHPrimes;
		/// allowed cipher suites, colon separated - you are allowed to mix TLS 1.2 and 1.3
		/// suites, they will be applied as matching (refer to the OpenSSL documentation).
		/// defaults to "PFS", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305
		KString sAllowedCipherSuites { "PFS" };
		/// do we want to poll connections for disconnects?
		bool bPollForDisconnect { false };
		/// additional callback function to call in case of disconnect, argument is the thread ID
		std::function<void(std::size_t)> DisconnectCallback;
		/// Parameters controling the simulation mode
		SimulationParms Simulate;

	}; // Options

	/// handle one REST request, or start REST server in HTTP and UNIX modes
	bool ExecuteRequest(const Options& Options, const KRESTRoutes& Routes);
	/// handle one REST request, read input from file, output to OutStream
	bool ExecuteFromFile(const Options& Options, const KRESTRoutes& Routes, KOutStream& OutStream = KOut);
	/// simulate one REST request in HTTP/CGI mode, call API, output to OutStream
	bool Simulate(const Options& Options, const KRESTRoutes& Routes, const KResource& API, KOutStream& OutStream = KOut);
	/// call either of the three other execution methods depending on Options.Type and sFilenameOrSimulation
	bool Execute(const Options& Options, const KRESTRoutes& Routes);
	/// returns true if no error
	DEKAF2_NODISCARD
	bool Good() const;
	/// get diagnostics when running with a TCP server
	DEKAF2_NODISCARD
	KThreadPool::Diagnostics GetDiagnostics() const;
	/// Shall we log the shutdown in TCP server mode?
	/// @param callback callback function called at each shutdown thread with some diagnostics
	void RegisterShutdownCallback(KThreadPool::ShutdownCallback callback);
	/// will wait until server termination,
	/// @returns the result of the server operation (0 for no error)
	int GetResult();
	/// will wait until server termination,
	/// @returns the result of the server operation (0 for no error)
	int Wait() { return GetResult(); }

//----------
protected:
//----------

	bool RealExecute(const Options& Options, const KRESTRoutes& Routes, KStream& Stream, KStringView sRemoteIP, url::KProtocol Proto, uint16_t iPort);

//----------
private:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class DEKAF2_PRIVATE RESTServer : public KTCPServer
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		//-----------------------------------------------------------------------------
		template<typename... Args>
		RESTServer(const KREST::Options& Options,
				   const KRESTRoutes& Routes,
				   KSocketWatch& SocketWatch,
				   KWebSocketServer& WebSocketServer,
				   Args&&... args)
		//-----------------------------------------------------------------------------
			: KTCPServer        (std::forward<Args>(args)...)
			, m_Options         (Options)
			, m_Routes          (Routes)
			, m_SocketWatch     (SocketWatch)
			, m_WebSocketServer (WebSocketServer)
		{
		}

		//-----------------------------------------------------------------------------
		void Session (KStream& Stream, KStringView sRemoteEndpoint, int iSocketFd) override final;
		//-----------------------------------------------------------------------------

	//----------
	protected:
	//----------

		const KREST::Options& m_Options;
		const KRESTRoutes&    m_Routes;
		KSocketWatch&         m_SocketWatch;
		KWebSocketServer&     m_WebSocketServer;

	}; // RESTServer

	std::unique_ptr<RESTServer>   m_Server;
	KThreadPool::ShutdownCallback m_ShutdownCallback;
	std::unique_ptr<KSocketWatch> m_SocketWatch;
	std::unique_ptr<KWebSocketServer> m_WebSocketServer;

}; // KREST

DEKAF2_NAMESPACE_END

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KREST::ServerType> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KREST::ServerType& ServerType, FormatContext& ctx) const
	{
		string_view sType;
		switch (ServerType)
		{
			case dekaf2::KREST::UNDEFINED:     sType = "undefined";     break;
			case dekaf2::KREST::HTTP:          sType = "http";          break;
			case dekaf2::KREST::CGI:           sType = "cgi";           break;
			case dekaf2::KREST::FCGI:          sType = "fcgi";          break;
			case dekaf2::KREST::LAMBDA:        sType = "lambda";        break;
			case dekaf2::KREST::CLI:           sType = "cli";           break;
			case dekaf2::KREST::SIMULATE_HTTP: sType = "simulate http"; break;
			case dekaf2::KREST::UNIX:          sType = "socket";        break;
		}
		return formatter<string_view>::format(sType, ctx);
	}
};

} // end of namespace DEKAF2_FORMAT_NAMESPACE
