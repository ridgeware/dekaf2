
#include "__LowerProjectName__.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kopenid.h>
#include <dekaf2/kjson.h>

constexpr
KRESTRoutes::MemberFunctionTable<__ProjectName__> __ProjectName__Routes[]
{
	// --------- ------ ------------------------------------------------------  -----------------------------
	// REQ.METH  SSO?   PATH PATTERN                                            API TO CALL[, FLAGS]
	// --------- ------ ------------------------------------------------------  -----------------------------
	{ "OPTIONS", false, "/*",                                                   &__ProjectName__::ApiOptions          },
	{ "GET",     false, "/Version",                                             &__ProjectName__::ApiVersion          },
	// --------- ------ ------------------------------------------------------  -----------------------------
};

using namespace dekaf2;

constexpr KStringView g_Help[] = {
	"",
	"__LowerProjectName__ -- dekaf2 __ProjectType__ template",
	"",
	"usage: __LowerProjectName__ [<options>]",
	"",
	"where <options> are:",
	"   -version               :: show software version and exit",
	"   -help                  :: this help message",
	"   -[d[d[d]]]             :: 3 optional levels of stdout debugging",
	"   -cgi                   :: force CGI mode",
	"   -lambda                :: force AWS Lambda mode",
	"   -http <port>           :: force HTTP server mode, requires port number",
	"   -socket <socket>       :: force HTTP server mode on unix domain socket file",
	"   -tls                   :: use TLS encryption",
	"   -ssolevel <0..2>       :: set SSO authentication level, default = 0 (off), real check = 2",
	"   -cert <file>           :: TLS certificate filepath",
	"   -key <file>            :: TLS private key filepath",
	"   -n <max>               :: max parallel connections (default 5, only for HTTP mode)",
	"   -baseroute </path>     :: route prefix, e.g. '/__LowerProjectName__'",
	"   -sim <url>             :: simulate request to a __LowerProjectName__ method (will use GET unless -X is used)",
	"   -X, --request <method> :: use with -sim: change request method of simulated request",
	"   -D, --data [@]<data>   :: use with -sim: add literal request body, or with @ take contents of file",
	"",
	"cgi cli usage:",
	"   __LowerProjectName__ -cgi <file> :: where <file> contains request + headers + post data",
	"",
	"aws-lambda usage:",
	"   (note: all environment is ignored)",
	"   __LowerProjectName__ [<options>] <lambda-arg> [<lambda-arg> ...]",
	""
};

//-----------------------------------------------------------------------------
void __ProjectName__::SetupInputFile (KOptions::ArgList& ArgList)
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
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	KInit().SetName(s_sProjectName).SetMultiThreading().SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

	m_CLI.RegisterHelp(g_Help);

	m_CLI.RegisterOption("cgi", 0, "", [&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::CGI;
		SetupInputFile(sArgs);
	});

	m_CLI.RegisterOption("fcgi", 0, "", [&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::FCGI;
		SetupInputFile(sArgs);
	});

	m_CLI.RegisterOption("lambda", 0, "", [&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::LAMBDA;
		SetupInputFile(sArgs);
	});

	m_CLI.RegisterOption("http,port", "port number", [&](KStringViewZ sPort)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.iPort = sPort.UInt16();

		if (m_ServerOptions.iPort == 0)
		{
			throw KOptions::WrongParameterError("port number has to be between 1 and 65535");
		}

		m_ServerOptions.Type = KREST::HTTP;
		KLog::getInstance().SetMode(KLog::SERVER);
	});

	m_CLI.RegisterOption("tls", [&]()
	{
		m_ServerOptions.bUseTLS = true;
	});

	m_CLI.RegisterOption("cert", "need certificate filepath", [&](KStringViewZ Arg)
	{
		m_ServerOptions.sCert = Arg;

		if (!kFileExists(m_ServerOptions.sCert))
		{
			throw KOptions::WrongParameterError(kFormat("certificate file '{}' not found", m_ServerOptions.sCert));
		}
	});

	m_CLI.RegisterOption("key", "need key filepath", [&](KStringViewZ Arg)
	{
		m_ServerOptions.sKey = Arg;

		if (!kFileExists(m_ServerOptions.sKey))
		{
			throw KOptions::WrongParameterError(kFormat("key file '{}' not found", m_ServerOptions.sKey));
		}
	});

	m_CLI.RegisterOption("socket", "unix socket file", [&](KStringViewZ sSocketFile)
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

	m_CLI.RegisterOption("n", "need number of max connections", [&](KStringViewZ sArg)
	{
		m_ServerOptions.iMaxConnections = sArg.UInt16();

		if (!m_ServerOptions.iMaxConnections)
		{
			throw KOptions::WrongParameterError("number of max connections has to be > 1");
		}
	});

	m_CLI.RegisterOption("baseroute", "need base route prefix", [&](KStringViewZ sArg)
	{
		if (sArg.front() != '/')
		{
			throw KOptions::WrongParameterError("base route prefix has to start with a /");
		}

		m_ServerOptions.sBaseRoute = sArg;
	});

	m_CLI.RegisterOption("ssolevel", "SSO level", [&](KStringViewZ sSSOLevel)
	{
		m_ServerOptions.iSSOLevel = sSSOLevel.UInt16();
	});

	m_CLI.RegisterOption("sim", "url", [&](KStringViewZ sArg)
	{
		m_ServerOptions.Simulate.API = sArg;
		m_ServerOptions.Type = KREST::SIMULATE_HTTP;
	});

	m_CLI.RegisterOption("X,request", "request_method", [&](KStringViewZ sMethod)
	{
		m_ServerOptions.Simulate.Method = sMethod.ToUpperASCII();

		if (!m_ServerOptions.Simulate.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KOptions::WrongParameterError(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}
	});

	m_CLI.RegisterOption("D,data", "request_body", [&](KStringViewZ sArg)
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

	m_CLI.RegisterOption("version,rev,revision", [&]()
	{
		KRESTServer HTTP;
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
void __ProjectName__::ApiOptions (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// required headers for callers making preflight requests
	HTTP.Response.Headers.Add (KHTTPHeaders::ACCESS_CONTROL_ALLOW_HEADERS, "Origin, X-Requested-With, Content-Type, Authorization");
	HTTP.Response.Headers.Add (KHTTPHeaders::ACCESS_CONTROL_ALLOW_METHODS, "PUT, GET, POST, OPTIONS, DELETE");
	// allow null response with CORS header

} // ApiOptions

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
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
		m_ServerOptions.sBaseRoute = "/__LowerProjectName__";
	}

	// bypass CORS
	m_ServerOptions.AddHeader(KHTTPHeaders::ACCESS_CONTROL_ALLOW_ORIGIN, "*");

	// add a HTTP header that counts API execution time
	m_ServerOptions.sTimerHeader = "x-milliseconds";
	// use x-klog as the header name for HTTP header logging
	m_ServerOptions.sKLogHeader  = "x-klog";

	// setup record file name
	m_ServerOptions.sRecordFile = s_sRecordFile;
	// and check if we shall switch recording on
	m_ServerOptions.bRecordRequest = kFileExists (s_sRecordFlag);

	// create a REST router
	KRESTRoutes Routes;
	// add our REST routes to it
	Routes.AddMemberFunctionTable(*this, __ProjectName__Routes);
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
		return __ProjectName__().Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
	}
	return 1;

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
constexpr KStringView  __ProjectName__::s_SSOProvider;
constexpr KStringView  __ProjectName__::s_SSOScope;
constexpr KStringViewZ __ProjectName__::s_sRecordFlag;
constexpr KStringViewZ __ProjectName__::s_sRecordFile;
#endif
