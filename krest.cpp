/*
//-----------------------------------------------------------------------------//
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
	RESTServer(KStringView sBaseRoute, const KRESTRoutes& Routes, Args&&... args)
	//-----------------------------------------------------------------------------
		: KTCPServer(std::forward<Args>(args)...)
		, m_Routes(Routes)
		, m_sBaseRoute(sBaseRoute)
	{
	}

	//-----------------------------------------------------------------------------
	void Session (KStream& Stream, KStringView sRemoteEndpoint)
	//-----------------------------------------------------------------------------
	{
		KRESTServer Request;

		Request.Accept(Stream, sRemoteEndpoint);
		Request.Execute(m_Routes, m_sBaseRoute, KRESTServer::HTTP);
		Request.Disconnect();

	} // Session


//----------
protected:
//----------

	const KRESTRoutes& m_Routes;
	KStringView m_sBaseRoute;

}; // RESTServer

} // end of namespace detail

//-----------------------------------------------------------------------------
bool KREST::RealExecute(KStream& Stream, KRESTServer::OutputType Type, KStringView sBaseRoute, const KRESTRoutes& Routes, KStringView sRemoteIP)
//-----------------------------------------------------------------------------
{
	KRESTServer Request;
	
	Request.Accept(Stream, sRemoteIP);
	bool bRet = Request.Execute(Routes, sBaseRoute, Type);
	Request.Disconnect();

	return bRet;

} // Execute

//-----------------------------------------------------------------------------
bool KREST::Execute(const Options& Params, const KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
{
	switch (Params.Type)
	{
		case HTTP:
			{
				bool bUseTLS = !Params.sCert.empty() || !Params.sKey.empty();
				if (bUseTLS)
				{
					if (Params.sCert.empty())
					{
						kWarning("TLS mode requested, but no certificate");
						return false;
					}
					if (!kFileExists(Params.sCert))
					{
						kWarning("TLS certificate does not exist: {}", Params.sCert);
						return false;
					}
					if (Params.sKey.empty())
					{
						kDebug(0, "TLS mode requested, but no private key");
						return false;
					}
					if (!kFileExists(Params.sKey))
					{
						kWarning("TLS private key file does not exist: {}", Params.sKey);
						return false;
					}
				}

				if (!detail::RESTServer::IsPortAvailable(Params.iPort))
				{
					SetError(kFormat("port {} is in use - abort", Params.iPort));
					kWarning("{}", Error());
					return false;
				}

				kDebug(1, "starting standalone {} server on port {}...", bUseTLS ? "HTTPS" : "HTTP", Params.iPort);
				detail::RESTServer Server(Params.sBaseRoute, Routes, Params.iPort, bUseTLS, Params.iMaxConnections);
				if (bUseTLS)
				{
					Server.SetSSLCertificates(Params.sCert, Params.sKey);
				}
				Server.Start(Params.iTimeout, true);
			}
			break;

		case UNIX:
			{
				kDebug(1, "starting standalone HTTP server on socket file {}...", Params.sSocketFile);
				detail::RESTServer Server(Params.sBaseRoute, Routes, Params.sSocketFile, Params.iMaxConnections);
				Server.Start(Params.iTimeout, true);
			}
			break;

		case CGI:
			{
				kDebug (3, "normal CGI request...");
				KCGIInStream CGI(KIn);
				KStream Stream(CGI, KOut);
				return RealExecute(Stream, KRESTServer::HTTP, Params.sBaseRoute, Routes); // TODO get remote from env var
			}
			break;

		case LAMBDA:
			{
				kDebug(3, "normal LAMBA request...");
				KLambdaInStream Lambda(KIn);
				KStream Stream(Lambda, KOut);
				return RealExecute(Stream, KRESTServer::LAMBDA, Params.sBaseRoute, Routes); // TODO get remote from env var
			}
			break;

		case CLI:
			{
				kDebug (3, "normal CLI request...");
				KStream Stream(KIn, KOut);
				return RealExecute(Stream, KRESTServer::CLI, Params.sBaseRoute, Routes, "127.0.0.1");
			}
			break;

		case SIMULATE_HTTP:
			// nothing to do here..
			kDebug (3, "please use ExecuteFromFile() for SIMULATE_HTTP REST request type");
			return false;
	}

	return true;

} // Execute

//-----------------------------------------------------------------------------
bool KREST::ExecuteFromFile(const Options& Params, const KRESTRoutes& Routes, KStringView sFilename, KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	switch (Params.Type)
	{
		case CGI:
			{
				kDebug(3, "simulated CGI request with input file: {}", sFilename);
				KInFile File(sFilename);
				KCGIInStream CGI(File);
				KStream Stream(CGI, OutStream);
				return RealExecute(Stream, KRESTServer::HTTP, Params.sBaseRoute, Routes, "127.0.0.1");
			}
			break;

		case LAMBDA:
			{
				kDebug(3, "simulated Lambda request with input file: {}", sFilename);
				KInFile File(sFilename);
				KLambdaInStream Lambda(File);
				KStream Stream(Lambda, OutStream);
				return RealExecute(Stream, KRESTServer::LAMBDA, Params.sBaseRoute, Routes, "127.0.0.1");
			}
			break;

		case HTTP:
		case UNIX:
		case CLI:
			// nothing to do here..
			kDebug (3, "please use Execute() for HTTP or UNIX REST request types");
			return false;

		case SIMULATE_HTTP:
			// nothing to do here..
			kDebug (3, "please use Simulate() for SIMULATE_HTTP request types");
			return false;
	}

	return true;

} // ExecuteFromFile

//-----------------------------------------------------------------------------
bool KREST::Simulate(const Options& Params, const KRESTRoutes& Routes, KStringView sSimulate, KOutStream& OutStream)
//-----------------------------------------------------------------------------
{
	if (sSimulate.front() != '/')
	{
		kWarning("sFilename does not start with a / - abort : {}", sSimulate);
		return false;
	}

	kDebug(3, "simulated CGI request: {}", sSimulate);
	KString sRequest;
	auto iSplitPostBody = sSimulate.find(' ');
	if (iSplitPostBody != KString::npos)
	{
		KStringView sURL = sSimulate.ToView(0, iSplitPostBody);
		KStringView sPostBody = sSimulate.ToView(iSplitPostBody + 1);
		sRequest += kFormat("POST {} HTTP/1.0\r\n"
							"Host: whatever\r\n"
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
						   "Host: whatever\r\n"
						   "User-Agent: cli sim agent\r\n"
						   "Connection: close\r\n"
						   "\r\n",
						   sSimulate);
	}

	KInStringStream String(sRequest);
	KStream Stream(String, OutStream);
	return RealExecute(Stream, KRESTServer::HTTP, Params.sBaseRoute, Routes, "127.0.0.1");
}

//-----------------------------------------------------------------------------
const KString& KREST::Error() const
//-----------------------------------------------------------------------------
{
	return m_sError;
}

//-----------------------------------------------------------------------------
bool KREST::SetError(KStringView sError)
//-----------------------------------------------------------------------------
{
	m_sError = sError;
	return false;
}

//-----------------------------------------------------------------------------
bool KREST::Good() const
//-----------------------------------------------------------------------------
{
	return m_sError.empty();
}


} // end of namespace dekaf2
