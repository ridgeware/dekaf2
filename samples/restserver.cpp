
#include "restserver.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kopenid.h>
#include <dekaf2/kjson.h>

using OPT = KRESTRoute::Options::OPT;

constexpr
KRESTRoutes::MemberFunctionTable<RestServer> RestServerRoutes[]
{
	// ---------   ------  ------------------------------------------------------  -----------------------------
	// REQ.METH    SSO?    PATH PATTERN                                            API TO CALL[, FLAGS]
	// ---------   ------  ------------------------------------------------------  -----------------------------
	{ "OPTIONS",   false,  "/*",                                                   &RestServer::ApiOptions          },
	{ "GET",       false,  "/Version",                                             &RestServer::ApiVersion          },
	{ "WEBSOCKET", false,  "/Chat",                                                &RestServer::ApiChat             },
	// ---------   ------  ------------------------------------------------------  -----------------------------
};

using namespace dekaf2;

constexpr KStringView g_Help[] = {
	"",
	"restserver -- dekaf2 restserver sample",
	"",
	"usage: restserver [<options>]",
	"",
	"where <options> are:",
	"   -version               :: show software version and exit",
	"   -help                  :: this help message",
	"   -[d[d[d]]]             :: 3 optional levels of stdout debugging",
	"   -cgi <file>            :: force CGI mode, file contains request + headers + post data",
	"   -lambda                :: force AWS Lambda mode",
	"   -http <port>           :: force HTTP server mode, requires port number",
	"   -socket <socket>       :: force HTTP server mode on unix domain socket file",
	"   -tls                   :: use TLS encryption",
	"   -ssolevel <0..2>       :: set SSO authentication level, default = 0 (off), real check = 2",
	"   -cert <file>           :: TLS certificate filepath",
	"   -key <file>            :: TLS private key filepath",
	"   -n <max>               :: max parallel connections (default 5, only for HTTP mode)",
	"   -baseroute </path>     :: route prefix, e.g. '/restserver'",
	"   -sim <url>             :: simulate request to a restserver method (will use GET unless -X is used)",
	"   -X, --request <method> :: use with -sim: change request method of simulated request",
	"   -D, --data [@]<data>   :: use with -sim: add literal request body, or with @ take contents of file",
	""
};

//-----------------------------------------------------------------------------
void RestServer::SetupInputFile (KOptions::ArgList& ArgList)
//-----------------------------------------------------------------------------
{
	if (!ArgList.empty())
	{
		m_ServerOptions.Simulate.sFilename = ArgList.pop();

		if (!kFileExists(m_ServerOptions.Simulate.sFilename))
		{
			throw KOptions::WrongParameterError(kFormat("input file not existing: {}", m_ServerOptions.Simulate.sFilename));
		}
	}

} // SetupInputFile

//-----------------------------------------------------------------------------
RestServer::RestServer ()
//-----------------------------------------------------------------------------
{
	KInit().SetName(s_sProjectName).SetMultiThreading().SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

	m_CLI.RegisterHelp(g_Help);

	m_CLI.Option("cgi").MinArgs(0)([&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::CGI;
		SetupInputFile(sArgs);
	});

	m_CLI.Option("fcgi").MinArgs(0)([&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::FCGI;
		SetupInputFile(sArgs);
	});

	m_CLI.Option("lambda").MinArgs(0)([&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::LAMBDA;
		SetupInputFile(sArgs);
	});

	m_CLI.Option("http,port", "port number")
	.Type(KOptions::Unsigned).Range(1, 65535)
	([&](KStringViewZ sPort)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.iPort = sPort.UInt16();

		m_ServerOptions.Type = KREST::HTTP;
		KLog::getInstance().SetMode(KLog::SERVER);
	});

	m_CLI.Option("tls")([&]()
	{
		m_ServerOptions.bUseTLS = true;
	});

	m_CLI.Option("cert", "need certificate filepath")
	.Type(KOptions::File)
	([&](KStringViewZ Arg)
	{
		m_ServerOptions.sCert = Arg;
	});

	m_CLI.Option("key", "need key filepath")
	.Type(KOptions::File)
	([&](KStringViewZ Arg)
	{
		m_ServerOptions.sKey = Arg;
	});

	m_CLI.Option("socket", "unix socket file")
	([&](KStringViewZ sSocketFile)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		if (sSocketFile.StartsWith("unix://"))
		{
			sSocketFile.remove_prefix(7);
		}

		m_ServerOptions.sSocketFile = sSocketFile;
		m_ServerOptions.Type = KREST::UNIX;
		KLog::getInstance().SetMode(KLog::SERVER);
	});

	m_CLI.Option("n", "need number of max connections")
	.Type(KOptions::Unsigned).Range(1, 65535)
	([&](KStringViewZ sArg)
	{
		m_ServerOptions.iMaxConnections = sArg.UInt16();
	});

	m_CLI.Option("baseroute", "need base route prefix")
	([&](KStringViewZ sArg)
	{
		if (sArg.front() != '/')
		{
			throw KOptions::WrongParameterError("base route prefix has to start with a /");
		}

		m_ServerOptions.sBaseRoute = sArg;
	});

	m_CLI.Option("ssolevel", "SSO level")
	.Type(KOptions::Unsigned).Range(1, 3)
	([&](KStringViewZ sSSOLevel)
	 {
		m_ServerOptions.iSSOLevel = sSSOLevel.UInt16();
	});

	m_CLI.Option("sim", "url")([&](KStringViewZ sArg)
	{
		m_ServerOptions.Simulate.API = sArg;
		m_ServerOptions.Type = KREST::SIMULATE_HTTP;
	});

	m_CLI.Option("X,request", "request_method")([&](KStringViewZ sMethod)
	{
		m_ServerOptions.Simulate.Method = sMethod.ToUpperASCII();

		if (!m_ServerOptions.Simulate.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KOptions::WrongParameterError(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}
	});

	m_CLI.Option("D,data", "request_body")([&](KStringViewZ sArg)
	{
		if (sArg.StartsWith("@"))
		{
			sArg.TrimLeft('@');

			if (!kReadAll (sArg, m_ServerOptions.Simulate.sBody))
			{
				throw KOptions::WrongParameterError(kFormat("invalid filename: {}", sArg));
			}
		}
		else
		{
			m_ServerOptions.Simulate.sBody = sArg;
		}
	});

	m_CLI.Option("v,version")([&]()
	{
		KRESTRoutes Routes;
		KRESTServer::Options Options;
		KRESTServer HTTP(Routes, Options);
		ApiVersion (HTTP);

		for (auto& it : HTTP.json.tx.items())
		{
			KString sName  = it.key();
			if (it.value().is_string())
			{
				KString sValue = it.value();
				KOut.FormatLine (":: {:<22} : {}", sName, sValue);
			}
			else if (it.value().is_number() || it.value().is_boolean())
			{
				int iValue = it.value();
				KOut.FormatLine (":: {:<22} : {}", sName, iValue);
			}
		}

		m_ServerOptions.bTerminate = true;
	});

} // ctor

//-----------------------------------------------------------------------------
void RestServer::ApiOptions (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// required headers for callers making preflight requests
	HTTP.Response.Headers.Add (KHTTPHeader::ACCESS_CONTROL_ALLOW_HEADERS, "Origin, X-Requested-With, Content-Type, Authorization");
	HTTP.Response.Headers.Add (KHTTPHeader::ACCESS_CONTROL_ALLOW_METHODS, "PUT, GET, POST, OPTIONS, DELETE");
	// allow null response with CORS header

} // ApiOptions

//-----------------------------------------------------------------------------
void RestServer::ApiVersion(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	KJSON json
	{
		{ "Project", "RestServer" },
		{ "Version", "0.0.1" },
	};

	HTTP.json.tx = std::move(json);

} // ApiVersion

//-----------------------------------------------------------------------------
void RestServer::ApiChat (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// this API is called once the websocket has been established, to setup
	// a receive callback and to note the connection handle for transmissions

	HTTP.SetWebSocketHandler([](KWebSocket& Server)
	{
		// the websocket handler that is called with every received frame
		
	});

} // Chat

//-----------------------------------------------------------------------------
int RestServer::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		auto iRetVal = m_CLI.Parse(argc, argv, KOut);

		if (iRetVal	|| m_ServerOptions.bTerminate)
		{
			// either error or completed
			return iRetVal;
		}
	}

	// --- try to auto detect execution by web server ---

	if (m_ServerOptions.Type == KREST::UNDEFINED)
	{
		// by default, derive request type off EXE name:
		KStringView sName = m_CLI.GetProgramName();

		if (sName.EndsWith(".cgi"))
		{
			m_ServerOptions.Type = KREST::CGI;
		}
		else if (sName.EndsWith(".fcgi"))
		{
			m_ServerOptions.Type = KREST::FCGI;
		}
		else if (sName.Contains("lambda"))
		{
			m_ServerOptions.Type = KREST::LAMBDA;
		}
		else
		{
			m_CLI.Help(KOut);
			return 1;
		}
	}

	// ------- create SSO provider ? ---------

	if (!s_SSOProvider.empty())
	{
		switch (m_ServerOptions.iSSOLevel)
		{
			case 0:
				m_ServerOptions.AuthLevel = ServerOptions::ALLOW_ALL;
				break;

			case 1:
				m_ServerOptions.AuthLevel = ServerOptions::ALLOW_ALL_WITH_AUTH_HEADER;
				break;

			default:
			{
				KOpenIDProvider SSOProvider(s_SSOProvider, s_SSOScope);

				if (!SSOProvider.IsValid())
				{
					KErr.FormatLine(">> Error: cannot configure SSO provider {}: {}",
									s_SSOProvider, SSOProvider.Error());
					return 1;
				}

				m_ServerOptions.AuthLevel = ServerOptions::VERIFY_AUTH_HEADER;
				m_ServerOptions.Authenticators.push_back(std::move(SSOProvider));
				m_ServerOptions.sAuthScope = s_SSOScope;
				break;
			}
		}
	}

	// ------- here starts the API routing -------

	if (m_ServerOptions.sBaseRoute.empty())
	{
		// the base route is optional - if it is there it will be removed
		m_ServerOptions.sBaseRoute = "/restserver";
	}

	// bypass CORS
	m_ServerOptions.AddHeader(KHTTPHeader::ACCESS_CONTROL_ALLOW_ORIGIN, "*");

	// add a HTTP header that counts API execution time
	m_ServerOptions.TimerHeader = "x-milliseconds";
	// use x-klog as the header name for HTTP header logging
	m_ServerOptions.KLogHeader  = "x-klog";

	// setup record file name
	m_ServerOptions.sRecordFile = s_sRecordFile;
	// and check if we shall switch recording on
	m_ServerOptions.bRecordRequest = kFileExists (s_sRecordFlag);

	// create a REST router
	KRESTRoutes Routes;
	// add our REST routes to it
	Routes.AddMemberFunctionTable(*this, RestServerRoutes);
	// create the REST service itself
	KREST REST;

	// and call the executor
	if (!REST.Execute(m_ServerOptions, Routes))
	{
		return 1;
	}
	// ------- API routing until here ------

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return RestServer().Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", RestServer::s_sProjectName, ex.what());
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", RestServer::s_sProjectName, ex.what());
	}
	return 1;

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ RestServer::s_sProjectName;
constexpr KStringViewZ RestServer::s_sProjectVersion;
constexpr KStringView  RestServer::s_SSOProvider;
constexpr KStringView  RestServer::s_SSOScope;
constexpr KStringViewZ RestServer::s_sRecordFlag;
constexpr KStringViewZ RestServer::s_sRecordFile;
#endif
