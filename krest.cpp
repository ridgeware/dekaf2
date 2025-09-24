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

#include "kcompatibility.h"
#include "krest.h"
#include "kcgistream.h"
#include "klambdastream.h"
#include "kfilesystem.h"
#include "kstringutils.h"
#include "kscopeguard.h"
#include <utility>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void KREST::RESTServer::Session (std::unique_ptr<KIOStreamSocket>& Stream)
//-----------------------------------------------------------------------------
{
	KRESTServer RESTServer(m_Routes, m_Options);

	// keep a pointer of the stream socket for REST routes that may want to
	// interact directly with the socket
	RESTServer.SetStreamSocket(*Stream);

	// keep a local copy of the socket for KSocketWatch
	auto nativeSocket = Stream->GetNativeSocket();

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [this, nativeSocket, &RESTServer]() noexcept
	{
		// get out of here if the tcp server is shutting down - otherwise
		// UBSAN complains about accessing the derived class, which is no
		// more existing
		if (!KTCPServer::IsShuttingDown())
		{
			if (m_Options.bPollForDisconnect)
			{
				// if the connection is already marked as disconnected
				// it has already been removed from the watch
				if (!RESTServer.IsDisconnected())
				{
					// use the local copy of the socket fd for removal,
					// as the Stream.GetNativeSocket() may already be -1
					m_SocketWatch.Remove(nativeSocket);
				}
			}

			RESTServer.Disconnect();
		}
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	if (m_Options.bPollForDisconnect)
	{
		// add the current connection to the KPoll watcher
		KPoll::Parameters Params;
		Params.iParameter = kGetTid();
		Params.bOnce      = true; // trigger only once - when the connection is gone it is gone
		Params.Callback   = [this, &RESTServer](int fd, uint16_t events, std::size_t iParam)
		{
			if (!KTCPServer::IsShuttingDown())
			{
				RESTServer.SetDisconnected();

				if (m_Options.DisconnectCallback)
				{
					m_Options.DisconnectCallback(iParam);
				}
			}
		};

		m_SocketWatch.Add(nativeSocket, std::move(Params));
	}

	RESTServer.Accept
	(
		*Stream,
		Stream->GetEndPointAddress().Serialize(),
		IsTLS() ? url::KProtocol::HTTPS : url::KProtocol::HTTP,
		GetPort()
	);

	RESTServer.Execute();

	if (RESTServer.SwitchToWebSocket())
	{
		// the client requests a switch to the websocket protocol
		if (RESTServer.GetWebSocketHandler())
		{
			if (!RESTServer.KeepWebSocketInRunningThread())
			{
				// now move the connection to the websocket event handler, and return this
				// thread into the pool
				auto handle = m_WebSocketServer.AddWebSocket(KWebSocket(Stream, RESTServer.GetWebSocketHandler()));

				if (handle > 0)
				{
					kDebug(2, "successfully upgraded to websocket protocol for '{}'", RESTServer.Route->sRoute);
				}
			}
			else
			{
				// create a websocket instance
				KWebSocket WebSocket(Stream, RESTServer.GetWebSocketHandler());
				// and run it right here through its own handler
				RESTServer.GetWebSocketHandler()(WebSocket);
			}
		}
		else
		{
			kDebug(1, "route handler for '{}' did not set a websocket handler - connection upgrade aborted",
				   RESTServer.Route->sRoute);
		}
	}

} // Session

//-----------------------------------------------------------------------------
KREST::~KREST()
//-----------------------------------------------------------------------------
{
	if (m_SocketWatch)
	{
		m_SocketWatch->Stop();
	}

	if (m_Server)
	{
		m_Server->Stop();
	}

} // dtor

//-----------------------------------------------------------------------------
bool KREST::RealExecute(const Options& Options, const KRESTRoutes& Routes, KStream& Stream, KStringView sRemoteIP, url::KProtocol Proto, uint16_t iPort)
//-----------------------------------------------------------------------------
{
	kDebug (2, "...");

	KRESTServer RESTServer(Routes, Options);
	
	RESTServer.Accept(Stream, sRemoteIP, std::move(Proto), iPort);
	bool bRet = RESTServer.Execute();

	if (RESTServer.SwitchToWebSocket())
	{
		kDebug(1, "cannot upgrade to websocket protocol on stdio");
	}

	RESTServer.Disconnect();

	return bRet;

} // Execute

//-----------------------------------------------------------------------------
int KREST::GetResult()
//-----------------------------------------------------------------------------
{
	if (m_Server)
	{
		return m_Server->GetResult();
	}
	else
	{
		return 0;
	}

} // GetResult

//-----------------------------------------------------------------------------
bool KREST::ExecuteRequest(const Options& Options, const KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
{
	kDebug (2, "...");

	switch (Options.Type)
	{
		case UNDEFINED:
			return SetError("undefined REST server mode");

		case HTTP:
			{
				KLog::getInstance().SetMode(KLog::SERVER);

				bool bUseTLS = !Options.sCert.empty() || !Options.sKey.empty() || Options.bCreateEphemeralCert;

				if (bUseTLS && !Options.bCreateEphemeralCert)
				{
					if (Options.sCert.empty())
					{
						return SetError("TLS mode requested, but no certificate");
					}

					if (Options.bPEMsAreFilenames && !kFileExists(Options.sCert))
					{
						return SetError(kFormat("TLS certificate does not exist: {}", Options.sCert));
					}

					if (!Options.sKey.empty())
					{
						if (Options.bPEMsAreFilenames && !kFileExists(Options.sKey))
						{
							return SetError(kFormat("TLS private key file does not exist: {}", Options.sKey));
						}
					}
				}

				uint16_t iPort = Options.iPort;

				if (iPort == 0)
				{
					iPort = bUseTLS ? 443 : 80;
				}

				if (!RESTServer::IsPortAvailable(iPort))
				{
					return SetError(kFormat("port {} is in use - abort", iPort));
				}

				kDebug(1, "starting standalone {} server on port {}...", bUseTLS ? "HTTPS" : "HTTP", iPort);
				Options.Out = KRESTServer::HTTP;

				m_SocketWatch     = std::make_unique<KSocketWatch>(chrono::milliseconds(250));
				m_WebSocketServer = std::make_unique<KWebSocketServer>();
				m_Server          = std::make_unique<RESTServer>(Options,
																 Routes,
																 *m_SocketWatch,
																 *m_WebSocketServer,
																 iPort,
																 bUseTLS,
																 Options.iMaxConnections,
																 Options.bStoreEphemeralCert);

				if (bUseTLS)
				{
					if (Options.bPEMsAreFilenames)
					{
						if (!m_Server->LoadTLSCertificates(Options.sCert, Options.sKey, Options.sTLSPassword))
						{
							if (!Options.bCreateEphemeralCert)
							{
								return SetError("could not load TLS certificate");
							}
						}
					}
					else
					{
						if (!m_Server->SetTLSCertificates(Options.sCert, Options.sKey, Options.sTLSPassword))
						{
							return SetError("could not set TLS certificate");
						}
					}

					if (!Options.sDHPrimes.empty())
					{
						if (Options.bPEMsAreFilenames)
						{
							if (!m_Server->LoadDHPrimes(Options.sDHPrimes))
							{
								return SetError("could not load DH primes");
							}
						}
						else
						{
							if (!m_Server->SetDHPrimes(Options.sDHPrimes))
							{
								return SetError("could not set DH primes");
							}
						}
					}
					m_Server->SetAllowedCipherSuites(Options.sAllowedCipherSuites);
				}

				m_Server->RegisterShutdownWithSignals(Options.RegisterSignalsForShutdown);
				m_Server->RegisterShutdownCallback(m_ShutdownCallback);

				if (!m_Server->Start(chrono::seconds(Options.iTimeout), Options.bBlocking))
				{
					return SetError(m_Server->CopyLastError()); // already logged
				}

				return true;
			}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case UNIX:
			{
				KLog::getInstance().SetMode(KLog::SERVER);
				kDebug(1, "starting standalone HTTP server on socket file {}...", Options.sSocketFile);
				Options.Out       = KRESTServer::HTTP;
				m_SocketWatch     = std::make_unique<KSocketWatch>(chrono::milliseconds(250));
				m_WebSocketServer = std::make_unique<KWebSocketServer>();
				m_Server          = std::make_unique<RESTServer>(Options,
																 Routes,
																 *m_SocketWatch,
																 *m_WebSocketServer,
																 Options.sSocketFile,
																 Options.iMaxConnections);

				m_Server->RegisterShutdownWithSignals(Options.RegisterSignalsForShutdown);
				m_Server->RegisterShutdownCallback(m_ShutdownCallback);

				if (!m_Server->Start(chrono::seconds(Options.iTimeout), Options.bBlocking))
				{
					return SetError(m_Server->CopyLastError()); // already logged
				}

				return true;
			}
#endif

		case CGI:
			{
				KLog::getInstance().SetMode(KLog::SERVER);
				kDebug (3, "normal CGI request...");

				KCGIInStream CGI(KIn);
				if (!Options.KLogHeader.empty())
				{
					CGI.AddCGIVar(KCGIInStream::ConvertHTTPHeaderNameToCGIVar(Options.KLogHeader), Options.KLogHeader);
				}

				KStream Stream(CGI, KOut);
				Options.Out = KRESTServer::HTTP;
				Options.iMaxKeepaliveRounds = 1; // no keepalive in CGI mode..

				RealExecute(Options,
							Routes,
							Stream,
							kGetEnv(KCGIInStream::REMOTE_ADDR),
							url::KProtocol(kGetEnv(KCGIInStream::SERVER_PORT)), // TODO check for this conversion
							kGetEnv(KCGIInStream::SERVER_PROTOCOL).UInt16());

				return true; // we return true because the request was served
			}

		case FCGI:
			KLog::getInstance().SetMode(KLog::SERVER);
			return SetError("FCGI mode not yet supported");

		case LAMBDA:
			{
				KLog::getInstance().SetMode(KLog::SERVER);
				kDebug(3, "normal LAMBA request...");
				KLambdaInStream Lambda(KIn);
				KStream Stream(Lambda, KOut);

				Options.Out = KRESTServer::LAMBDA;
				Options.iMaxKeepaliveRounds = 1; // no keepalive in Lambda mode..

				RealExecute(Options,
							Routes,
							Stream,
							"0.0.0.0",
							url::KProtocol::HTTP,
							0); // TODO get remote IP, proto, port from env var

				return true; // we return true because the request was served
			}

		case CLI:
			{
				KLog::getInstance().SetMode(KLog::CLI); // CLI mode is the default anyway..
				kDebug (2, "normal CLI request...");
				KStream Stream(KIn, KOut);

				Options.Out = KRESTServer::CLI;

				RealExecute(Options,
							Routes,
							Stream,
							kFirstNonEmpty(kGetEnv(KCGIInStream::REMOTE_ADDR), "127.0.0.1"),
							url::KProtocol::HTTP,
							0);

				return true; // we return true because the request was served
			}

		case SIMULATE_HTTP:
			// nothing to do here..
			return SetError ("please use Simulate() for SIMULATE_HTTP REST request type");
	}

	return true;

} // ExecuteRequest

//-----------------------------------------------------------------------------
bool KREST::ExecuteFromFile(const Options& Options, const KRESTRoutes& Routes, KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	kDebug (2, "...");

	switch (Options.Type)
	{
		case CGI:
			{
				kDebug(2, "simulated CGI request with input file: {}", Options.Simulate.sFilename);
				KInFile File(Options.Simulate.sFilename);
				KCGIInStream CGI(File);
				if (!Options.KLogHeader.empty())
				{
					CGI.AddCGIVar(KCGIInStream::ConvertHTTPHeaderNameToCGIVar(Options.KLogHeader), Options.KLogHeader);
				}
				KStream Stream(CGI, OutStream);
				Options.Out = KRESTServer::HTTP;
				RealExecute(Options,
							Routes,
							Stream,
							"127.0.0.1",
							url::KProtocol::HTTP,
							0);
				return true; // we return true because the request was served
			}

		case LAMBDA:
			{
				kDebug(2, "simulated Lambda request with input file: {}", Options.Simulate.sFilename);
				KInFile File(Options.Simulate.sFilename);
				KLambdaInStream Lambda(File);
				KStream Stream(Lambda, OutStream);
				Options.Out = KRESTServer::LAMBDA;
				RealExecute(Options,
							Routes,
							Stream,
							"127.0.0.1",
							url::KProtocol::HTTP,
							0);
				return true; // we return true because the request was served
			}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case UNIX:
#endif
		case HTTP:
		case CLI:
			// nothing to do here..
			return SetError ("please use Execute() for HTTP or UNIX REST request types");

		case SIMULATE_HTTP:
			// nothing to do here..
			return SetError ("please use Simulate() for SIMULATE_HTTP request types");

		case UNDEFINED:
			return SetError ("undefined REST server mode");

		case FCGI:
			return SetError ("FCGI mode not yet supported");
	}

	return true;

} // ExecuteFromFile

//-----------------------------------------------------------------------------
bool KREST::Simulate(const Options& Options, const KRESTRoutes& Routes, const KResource& API, KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	kDebug (2, "...");

	if (API.empty())
	{
		return SetError(kFormat("invalid request - no API selected"));
	}

	kDebug(2, "simulated CGI request: {}", API.Serialize());

	KHTTPMethod Method = Options.Simulate.Method;

	if (Method.empty())
	{
		Method = Options.Simulate.sBody.empty() ? KHTTPMethod::GET : KHTTPMethod::POST;
	}

	KString sHeaders;
	for (const auto& Header : Options.Simulate.AdditionalRequestHeaders)
	{
		sHeaders += kFormat("{}: {}\r\n", Header.first, Header.second);
	}

	KString sRequest;

	if (!Options.Simulate.sBody.empty())
	{
		sRequest = kFormat("{} {} HTTP/1.0\r\n"
						   "Host: localhost\r\n"
						   "User-Agent: {}\r\n"
						   "Connection: close\r\n"
						   "Content-Length: {}\r\n"
						   "{}"
						   "\r\n",
						   Method.Serialize(),
						   API.Serialize(),
						   CLI_SIM_AGENT,
						   Options.Simulate.sBody.size(),
						   sHeaders);

		sRequest += Options.Simulate.sBody;
	}
	else
	{
		sRequest = kFormat("{} {} HTTP/1.0\r\n"
						   "Host: localhost\r\n"
						   "User-Agent: {}\r\n"
						   "Connection: close\r\n"
						   "{}"
						   "\r\n",
						   Method.Serialize(),
						   API.Serialize(),
						   CLI_SIM_AGENT,
						   sHeaders);

	}

	KInStringStream String(sRequest);
	KStream Stream(String, OutStream);
	Options.Out = KRESTServer::HTTP;
	return RealExecute(Options,
					   Routes,
					   Stream,
					   "127.0.0.1",
					   url::KProtocol::HTTP,
					   0);

} // Simulate

//-----------------------------------------------------------------------------
bool KREST::Execute(const Options& Options, const KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	switch (Options.Type)
	{
		default:
			if (DEKAF2_LIKELY(Options.Simulate.sFilename.empty()))
			{
				// execute real request
				return ExecuteRequest(Options, Routes);
			}
			else
			{
				// execute request from input file
				return ExecuteFromFile(Options, Routes);
			}

		case KREST::SIMULATE_HTTP:
			// simulate request from command line
			return Simulate(Options, Routes, Options.Simulate.API);

		case KREST::UNDEFINED:
			return SetError("no KREST server type defined");

	} // switch (xOptions.Type)

} // Execute

//-----------------------------------------------------------------------------
bool KREST::Good() const
//-----------------------------------------------------------------------------
{
	return !HasError();

} // Good

//-----------------------------------------------------------------------------
KThreadPool::Diagnostics KREST::GetDiagnostics() const
//-----------------------------------------------------------------------------
{
	return m_Server ? m_Server->GetDiagnostics() : KThreadPool::Diagnostics{};

} // GetDiagnostics

//-----------------------------------------------------------------------------
void KREST::RegisterShutdownCallback(KThreadPool::ShutdownCallback callback)
//-----------------------------------------------------------------------------
{
	m_ShutdownCallback = std::move(callback);

	if (m_Server)
	{
	   m_Server->RegisterShutdownCallback(m_ShutdownCallback);
	}

} // RegisterShutdownCallback

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ KREST::CLI_SIM_AGENT;
#endif

DEKAF2_NAMESPACE_END
