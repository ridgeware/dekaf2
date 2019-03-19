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
#include "ktcpserver.h"
#include "kcgistream.h"
#include "klambdastream.h"
#include "kfilesystem.h"

namespace dekaf2 {

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class RESTServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	template<typename... Args>
	RESTServer(const KREST::Options& Options, const KRESTRoutes& Routes, Args&&... args)
	//-----------------------------------------------------------------------------
		: KTCPServer(std::forward<Args>(args)...)
		, m_Options(Options)
		, m_Routes(Routes)
	{
	}

	//-----------------------------------------------------------------------------
	void Session (KStream& Stream, KStringView sRemoteEndpoint) override final
	//-----------------------------------------------------------------------------
	{
		KRESTServer Request;

		Request.Accept(Stream, sRemoteEndpoint);
		Request.Execute(m_Options, m_Routes);
		Request.Disconnect();

	} // Session


//----------
protected:
//----------

	const KREST::Options& m_Options;
	const KRESTRoutes& m_Routes;

}; // RESTServer

} // end of namespace detail

//-----------------------------------------------------------------------------
bool KREST::RealExecute(const Options& Options, const KRESTRoutes& Routes, KStream& Stream, KStringView sRemoteIP)
//-----------------------------------------------------------------------------
{
	KRESTServer Request;
	
	Request.Accept(Stream, sRemoteIP);
	bool bRet = Request.Execute(Options, Routes);
	Request.Disconnect();

	return bRet;

} // Execute

//-----------------------------------------------------------------------------
bool KREST::Execute(const Options& Options, const KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
{
	switch (Options.Type)
	{
		case UNDEFINED:
			return SetError("undefined REST server mode");

		case HTTP:
			{
				KLog().SetMode(KLog::SERVER);

				bool bUseTLS = !Options.sCert.empty() || !Options.sKey.empty();
				if (bUseTLS)
				{
					if (Options.sCert.empty())
					{
						return SetError("TLS mode requested, but no certificate");
					}
					if (!kFileExists(Options.sCert))
					{
						return SetError(kFormat("TLS certificate does not exist: {}", Options.sCert));
					}
					if (Options.sKey.empty())
					{
						return SetError("TLS mode requested, but no private key");
					}
					if (!kFileExists(Options.sKey))
					{
						return SetError(kFormat("TLS private key file does not exist: {}", Options.sKey));
					}
				}

				if (!detail::RESTServer::IsPortAvailable(Options.iPort))
				{
					return SetError(kFormat("port {} is in use - abort", Options.iPort));
				}

				kDebug(1, "starting standalone {} server on port {}...", bUseTLS ? "HTTPS" : "HTTP", Options.iPort);
				Options.Out = KRESTServer::HTTP;
				detail::RESTServer Server(Options, Routes, Options.iPort, bUseTLS, Options.iMaxConnections);
				if (bUseTLS)
				{
					Server.SetSSLCertificates(Options.sCert, Options.sKey);
				}
				Server.Start(Options.iTimeout, true);
				return true;
			}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
		case UNIX:
			{
				KLog().SetMode(KLog::SERVER);
				kDebug(1, "starting standalone HTTP server on socket file {}...", Options.sSocketFile);
				Options.Out = KRESTServer::HTTP;
				detail::RESTServer Server(Options, Routes, Options.sSocketFile, Options.iMaxConnections);
				Server.Start(Options.iTimeout, true);
				return true;
			}
#endif

		case CGI:
			{
				KLog().SetMode(KLog::SERVER);
				kDebug (3, "normal CGI request...");
				KCGIInStream CGI(KIn);
				KStream Stream(CGI, KOut);
				Options.Out = KRESTServer::HTTP;
				RealExecute(Options, Routes, Stream); // TODO get remote IP from env var
				return true; // we return true because the request was served
			}

		case FCGI:
			KLog().SetMode(KLog::SERVER);
			return SetError("FCGI mode not yet supported");

		case LAMBDA:
			{
				KLog().SetMode(KLog::SERVER);
				kDebug(3, "normal LAMBA request...");
				KLambdaInStream Lambda(KIn);
				KStream Stream(Lambda, KOut);
				Options.Out = KRESTServer::LAMBDA;
				RealExecute(Options, Routes, Stream); // TODO get remote IP from env var
				return true; // we return true because the request was served
			}

		case CLI:
			{
				KLog().SetMode(KLog::CLI); // CLI mode is the default anyway..
				kDebug (3, "normal CLI request...");
				KStream Stream(KIn, KOut);
				Options.Out = KRESTServer::CLI;
				RealExecute(Options, Routes, Stream, "127.0.0.1");
				return true; // we return true because the request was served
			}

		case SIMULATE_HTTP:
			// nothing to do here..
			SetError ("please use Simulate() for SIMULATE_HTTP REST request type");
			return false;
	}

	return true;

} // Execute

//-----------------------------------------------------------------------------
bool KREST::ExecuteFromFile(const Options& Options, const KRESTRoutes& Routes, KStringView sFilename, KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	switch (Options.Type)
	{
		case CGI:
			{
				kDebug(3, "simulated CGI request with input file: {}", sFilename);
				KInFile File(sFilename);
				KCGIInStream CGI(File);
				KStream Stream(CGI, OutStream);
				Options.Out = KRESTServer::HTTP;
				RealExecute(Options, Routes, Stream, "127.0.0.1");
				return true; // we return true because the request was served
			}

		case LAMBDA:
			{
				kDebug(3, "simulated Lambda request with input file: {}", sFilename);
				KInFile File(sFilename);
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
bool KREST::Simulate(const Options& Options, const KRESTRoutes& Routes, KStringView sSimulate, KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	if (sSimulate.front() != '/')
	{
		return SetError(kFormat("sFilename does not start with a / - abort : {}", sSimulate));
	}

	kDebug(3, "simulated CGI request: {}", sSimulate);
	KString sRequest;
	auto iSplitPostBody = sSimulate.find(' ');
	if (iSplitPostBody != KString::npos)
	{
		KStringView sURL = sSimulate.ToView(0, iSplitPostBody);
		KStringView sPostBody = sSimulate.ToView(iSplitPostBody + 1);
		sRequest += kFormat("POST {} HTTP/1.0\r\n"
							"Host: localhost\r\n"
							"User-Agent: cli sim agent\r\n"
							"Connection: close\r\n"
							"Content-Length: {}\r\n"
							"\r\n",
							sURL, sPostBody.size());
		sRequest += sPostBody;
	}
	else
	{
		sRequest = kFormat("GET {} HTTP/1.0\r\n"
						   "Host: localhost\r\n"
						   "User-Agent: cli sim agent\r\n"
						   "Connection: close\r\n"
						   "\r\n",
						   sSimulate);
	}

	KInStringStream String(sRequest);
	KStream Stream(String, OutStream);
	Options.Out = KRESTServer::HTTP;
	return RealExecute(Options, Routes, Stream, "127.0.0.1");

} // Simulate

//-----------------------------------------------------------------------------
bool KREST::Execute(const Options& Options, const KRESTRoutes& Routes, KStringView sFilenameOrSimulation)
//-----------------------------------------------------------------------------
{
	switch (Options.Type)
	{
		default:
			if (DEKAF2_LIKELY(sFilenameOrSimulation.empty()))
			{
				// execute real request
				return Execute(Options, Routes);
			}
			else
			{
				// execute request from input file
				return ExecuteFromFile(Options, Routes, sFilenameOrSimulation);
			}

		case KREST::SIMULATE_HTTP:
			// simulate request from command line
			return Simulate(Options, Routes, sFilenameOrSimulation);

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
	m_sError = sError;
	kWarning(m_sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KREST::Good() const
//-----------------------------------------------------------------------------
{
	return m_sError.empty();

} // Good


} // end of namespace dekaf2
