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

#include "krest.h"
#include "kcgistream.h"
#include "klambdastream.h"
#include "kfilesystem.h"
#include <poll.h>

namespace dekaf2 {

//-----------------------------------------------------------------------------
void KREST::RESTServer::Session (KStream& Stream, KStringView sRemoteEndpoint, int iSocketFd)
//-----------------------------------------------------------------------------
{
	KRESTServer Request;

	if (m_Options.bPollForDisconnect)
	{
		// add the current connection to the KPoll watcher
		KPoll::Parameters Params;
		Params.iParameter = kGetTid();
		Params.bOnce      = true; // trigger only once - when the connection is gone it is gone
		Params.Callback   = [this, &Request](int fd, uint16_t events, std::size_t iParam)
		{
			Request.SetDisconnected();

			if (m_Options.DisconnectCallback)
			{
				m_Options.DisconnectCallback(iParam);
			}
		};
		m_SocketWatch.Add(iSocketFd, std::move(Params));
	}

	Request.Accept(Stream, sRemoteEndpoint);
	Request.Execute(m_Options, m_Routes);

	if (m_Options.bPollForDisconnect)
	{
		if (!Request.IsDisconnected())
		{
			m_SocketWatch.Remove(iSocketFd);
		}
	}

	Request.Disconnect();

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
bool KREST::RealExecute(const Options& Options, const KRESTRoutes& Routes, KStream& Stream, KStringView sRemoteIP)
//-----------------------------------------------------------------------------
{
	kDebug (2, "...");

	KRESTServer Request;
	
	Request.Accept(Stream, sRemoteIP);
	bool bRet = Request.Execute(Options, Routes);
	Request.Disconnect();

	return bRet;

} // Execute

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

				bool bUseTLS = !Options.sCert.empty() || !Options.sKey.empty();

				if (bUseTLS)
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

				if (!RESTServer::IsPortAvailable(Options.iPort))
				{
					return SetError(kFormat("port {} is in use - abort", Options.iPort));
				}

				kDebug(1, "starting standalone {} server on port {}...", bUseTLS ? "HTTPS" : "HTTP", Options.iPort);
				Options.Out = KRESTServer::HTTP;

				m_SocketWatch = std::make_unique<KSocketWatch>(250);
				m_Server = std::make_unique<RESTServer>(Options, Routes, *m_SocketWatch, Options.iPort, bUseTLS, Options.iMaxConnections);

				if (bUseTLS)
				{
					if (Options.bPEMsAreFilenames)
					{
						if (!m_Server->LoadSSLCertificates(Options.sCert, Options.sKey))
						{
							return SetError("could not load TLS certificate");
						}
					}
					else
					{
						if (!m_Server->SetSSLCertificates(Options.sCert, Options.sKey))
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
				m_Server->Start(Options.iTimeout, Options.bBlocking);
				return true;
			}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case UNIX:
			{
				KLog::getInstance().SetMode(KLog::SERVER);
				kDebug(1, "starting standalone HTTP server on socket file {}...", Options.sSocketFile);
				Options.Out = KRESTServer::HTTP;
				m_SocketWatch = std::make_unique<KSocketWatch>(250);
				m_Server = std::make_unique<RESTServer>(Options, Routes, *m_SocketWatch, Options.sSocketFile, Options.iMaxConnections);
				m_Server->RegisterShutdownWithSignals(Options.RegisterSignalsForShutdown);
				m_Server->RegisterShutdownCallback(m_ShutdownCallback);
				m_Server->Start(Options.iTimeout, Options.bBlocking);
				return true;
			}
#endif

		case CGI:
			{
				KLog::getInstance().SetMode(KLog::SERVER);
				kDebug (3, "normal CGI request...");
				KCGIInStream CGI(KIn);
				if (!Options.sKLogHeader.empty())
				{
					CGI.AddCGIVar(KCGIInStream::ConvertHTTPHeaderNameToCGIVar(Options.sKLogHeader), Options.sKLogHeader);
				}
				KStream Stream(CGI, KOut);
				Options.Out = KRESTServer::HTTP;
				Options.iMaxKeepaliveRounds = 1; // no keepalive in CGI mode..
				RealExecute(Options, Routes, Stream, kGetEnv(KCGIInStream::REMOTE_ADDR));
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
				RealExecute(Options, Routes, Stream); // TODO get remote IP from env var
				return true; // we return true because the request was served
			}

		case CLI:
			{
				KLog::getInstance().SetMode(KLog::CLI); // CLI mode is the default anyway..
				kDebug (2, "normal CLI request...");
				KStream Stream(KIn, KOut);
				Options.Out = KRESTServer::CLI;
				RealExecute(Options, Routes, Stream, kFirstNonEmpty<KStringView>(kGetEnv(KCGIInStream::REMOTE_ADDR), "127.0.0.1"));
				return true; // we return true because the request was served
			}

		case SIMULATE_HTTP:
			// nothing to do here..
			SetError ("please use Simulate() for SIMULATE_HTTP REST request type");
			return false;
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
				if (!Options.sKLogHeader.empty())
				{
					CGI.AddCGIVar(KCGIInStream::ConvertHTTPHeaderNameToCGIVar(Options.sKLogHeader), Options.sKLogHeader);
				}
				KStream Stream(CGI, OutStream);
				Options.Out = KRESTServer::HTTP;
				RealExecute(Options, Routes, Stream, "127.0.0.1");
				return true; // we return true because the request was served
			}

		case LAMBDA:
			{
				kDebug(2, "simulated Lambda request with input file: {}", Options.Simulate.sFilename);
				KInFile File(Options.Simulate.sFilename);
				KLambdaInStream Lambda(File);
				KStream Stream(Lambda, OutStream);
				Options.Out = KRESTServer::LAMBDA;
				RealExecute(Options, Routes, Stream, "127.0.0.1");
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
						   "User-Agent: cli sim agent\r\n"
						   "Connection: close\r\n"
						   "Content-Length: {}\r\n"
						   "{}"
						   "\r\n",
						   Method.Serialize(),
						   API.Serialize(),
						   Options.Simulate.sBody.size(),
						   sHeaders);

		sRequest += Options.Simulate.sBody;
	}
	else
	{
		sRequest = kFormat("{} {} HTTP/1.0\r\n"
						   "Host: localhost\r\n"
						   "User-Agent: cli sim agent\r\n"
						   "Connection: close\r\n"
						   "{}"
						   "\r\n",
						   Method.Serialize(),
						   API.Serialize(),
						   sHeaders);
	}

	KInStringStream String(sRequest);
	KStream Stream(String, OutStream);
	Options.Out = KRESTServer::HTTP;
	return RealExecute(Options, Routes, Stream, "127.0.0.1");

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
			return false;

	} // switch (xOptions.Type)

} // Execute

//-----------------------------------------------------------------------------
const KString& KREST::Error() const
//-----------------------------------------------------------------------------
{
	return m_sError;

} // Error

//-----------------------------------------------------------------------------
bool KREST::SetError(KStringView sError)
//-----------------------------------------------------------------------------
{
	kDebug (1, sError);
	m_sError = sError;
	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KREST::Good() const
//-----------------------------------------------------------------------------
{
	return m_sError.empty();

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
	   m_Server->RegisterShutdownCallback(callback);
	}

} // RegisterShutdownCallback


} // end of namespace dekaf2
