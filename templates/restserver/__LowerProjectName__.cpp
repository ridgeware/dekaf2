
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
	KInit()
		.SetName(s_sProjectName)
		.SetMultiThreading()
		.SetOnlyShowCallerOnJsonError();
}

//-----------------------------------------------------------------------------
void __ProjectName__::SetupOptions (KOptions& Options)
//-----------------------------------------------------------------------------
{
	Options
		.Throw()
		.SetBriefDescription       ("__LowerProjectName__ -- dekaf2 __ProjectType__ template")
		// .SetHelpSeparator          ("::")            // the column separator between option and help text
		// .SetLinefeedBetweenOptions (false)           // add linefeed between separate options or commands?
		// .SetWrappedHelpIndent      (1)               // the indent for continuation help text
		.SetSpacingPerSection      (true);              // whether commands and options get the same or separate column layout

	// ---- insert project specific options here ----

	Options
		.Option("version")
		.Help("show software version and exit")
		.Stop()
	([&]()
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
	});

	Options
		.Option("http <port>", "port number")
		.Type(KOptions::Unsigned)
		.Range(1, 65535)
		.Help("force HTTP server mode, requires port number")
		.Section("general REST server options:")
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

	Options
		.Option("socket <socket>", "unix socket file")
		.Help("force HTTP server mode on unix domain socket file like /tmp/__LowerProjectName__.sock or unix:///tmp/__LowerProjectName__.sock")
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

	Options
		.Option("n", "need number of max connections")
		.Type(KOptions::Unsigned).Range(1, 65535)
		.Help("max parallel connections (default 5, only for HTTP mode)")
	([&](KStringViewZ sArg)
	{
		m_ServerOptions.iMaxConnections = sArg.UInt16();
	});

	Options
		.Option("keepalive <maxrounds>", "need number of max keepalive rounds")
		.Type(KOptions::Unsigned)
		.Help("max keepalive rounds for HTTP mode (default 10, 0 == off)")
	([&](KStringViewZ sArg)
	 {
		m_ServerOptions.iMaxKeepaliveRounds = sArg.UInt16();
	});

	Options
		.Option("timeout <seconds>", "need http server timeout")
		.Type(KOptions::Unsigned)
		.Range(1, 60*60)
		.Help("HTTP server timeout (default 5, max 3600 (one hour))")
	([&](KStringViewZ sArg)
	 {
		m_ServerOptions.iTimeout = sArg.UInt16();
	});

	Options
		.Option("compressors <methods>", "list of permitted HTTP compression algorithms")
		.Help("permitted http compression algorithms, default = br,deflate,gzip")
	([&](KStringViewZ sArg)
	{
		KHTTPCompression::SetPermittedCompressors(sArg);
	});

	Options
		.Option("baseroute </path>", "need base route prefix")
		.Help("route prefix, e.g. '/__LowerProjectName__'")
	([&](KStringViewZ sArg)
	{
		if (sArg.front() != '/')
		{
			throw KOptions::WrongParameterError("base route prefix has to start with a /");
		}

		m_ServerOptions.sBaseRoute = sArg;
	});

	Options
		.Option("restlog <file>\n"
				"        [<type>\n"
				"        [<format>]]",
				"rest log file name")
		.Help("write rest server log to <file> - default off. "
			  "<type> is one of JSON, COMMON, COMBINED, EXTENDED, PARSED, default EXTENDED. "
			  "For PARSED, define <format> string in apache format (e.g. \"%h %l %u %t \\\"%r\\\" %>s %b \\\"%{Referer}i\\\"\")")
	([&](KOptions::ArgList& sArgs)
	{
		auto sFilename = sArgs.pop();
		KHTTPLog::LOG_FORMAT Format = KHTTPLog::LOG_FORMAT::EXTENDED;

		if (!sArgs.empty())
		{
			auto sFormat = sArgs.pop();
			switch (sFormat.ToUpperASCII().Hash())
			{
				case "JSON"_hash:
					Format = KHTTPLog::LOG_FORMAT::JSON;
					break;
				case "COMMON"_hash:
					Format = KHTTPLog::LOG_FORMAT::COMMON;
					break;
				case "COMBINED"_hash:
					Format = KHTTPLog::LOG_FORMAT::COMBINED;
					break;
				case "EXTENDED"_hash:
					Format = KHTTPLog::LOG_FORMAT::EXTENDED;
					break;
				case "PARSED"_hash:
					Format = KHTTPLog::LOG_FORMAT::PARSED;
					break;
				default:
					throw KOptions::WrongParameterError(kFormat("unknown log format '{}' - must be one of JSON,COMMON,COMBINED,EXTENDED,PARSED", sFormat));
			}
		}

		KStringView sFormat;
		if (Format == KHTTPLog::LOG_FORMAT::PARSED)
		{
			if (!sArgs.empty())
			{
				sFormat = sArgs.pop();
			}
			else
			{
				throw KOptions::WrongParameterError("missing format definition for PARSED format");
			}
		}

		if (!m_ServerOptions.Logger.Open(Format, sFilename, sFormat))
		{
			throw KOptions::WrongParameterError("cannot open log stream");
		}
	});

	Options
		.Option("cert <file>", "need certificate filepath")
		.Type(KOptions::File)
		.Help("TLS certificate filepath")
		.Section("TLS options:")
([&](KStringViewZ Arg)
	{
		m_ServerOptions.sCert = Arg;
	});

	Options
		.Option("key <file>", "need key filepath")
		.Type(KOptions::File)
		.Help("TLS private key filepath")
	([&](KStringViewZ Arg)
	{
		m_ServerOptions.sKey = Arg;
	});

	Options
		.Option("tlspass <pass>", "need TLS CERT password")
		.Help("TLS certificate password, if any")
	([&](KStringViewZ sArg)
	 {
		m_ServerOptions.sTLSPassword = sArg;
	});

	Options
		.Option("dh <file>", "need diffie-hellman prime filepath")
		.Type(KOptions::File)
		.Help("TLS DH prime filepath (.pem) (generate with 'openssl dhparam -out dh2048.pem 2048')")
	([&](KStringViewZ sArg)
	{
		m_ServerOptions.sDHPrimes = sArg;
	});

	Options
		.Option("ciphers <suites>", "need cipher suites")
		.Help("colon delimited list of permitted cipher suites for TLS")
	([&](KStringViewZ sCipherSuites)
	{
		m_ServerOptions.sAllowedCipherSuites = sCipherSuites;
	});

	if (!s_SSOProvider.empty())
	{
		Options
			.Option("ssolevel <0..2>", "SSO level")
			.Type(KOptions::Unsigned)
			.Range(1, 2)
			.Help("set SSO authentication level, default = 0 (off), header check = 1, real check = 2")
		([&](KStringViewZ sSSOLevel)
		{
			m_ServerOptions.iSSOLevel = sSSOLevel.UInt16();
		});
	}

	Options
		.Option("sim <url>", "url")
		.Help("simulate request to a __LowerProjectName__ method (will use GET unless -X is used)")
		.Section("options for simulation mode (local testing):")
	([&](KStringViewZ sArg)
	{
		m_ServerOptions.Simulate.API = sArg;
		m_ServerOptions.Type = KREST::SIMULATE_HTTP;
	});

	Options
		.Option("X,request <method>", "request_method")
		.ToUpper()
		.Help("use with -sim: change request method of simulated request")
	([&](KStringViewZ sMethod)
	{
		m_ServerOptions.Simulate.Method = sMethod.ToUpperASCII();

		if (!m_ServerOptions.Simulate.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
		{
			throw KOptions::WrongParameterError(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
		}
	});

	Options
		.Option("H,header <header>", "header definition")
		.Help("use with -sim: add \"Header: value\" additional headers to simulated request")
	([&](KString sHeader)
	{
		auto Pair = kSplitToPair(sHeader, ":");
		m_ServerOptions.Simulate.AdditionalRequestHeaders.push_back(Pair.first, Pair.second);
	});

	Options
		.Option("D,data [@]<data>", "request_body")
		.Help("use with -sim: add literal request body, or with @ take contents of file")
	([&](KStringViewZ sArg)
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

	Options
		.Option("cgi <file>")
		.MinArgs(0)
		.Help("force CGI mode, where <file> contains request + headers + post data")
		.Section("cgi cli usage:")
	([&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::CGI;
		SetupInputFile(sArgs);
	});

#ifdef DEKAF2_WITH_FCGI
	Options
		.Option("fcgi")
		.MinArgs(0)
		.Help("force FCGI mode")
	([&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::FCGI;
		SetupInputFile(sArgs);
	});
#endif

	Options
		.Option("lambda [<lambda-arg> ...]")
		.MinArgs(0)
		.Help("force AWS Lambda mode")
		.Section("aws-lambda usage:")
	([&](KOptions::ArgList& sArgs)
	{
		if (m_ServerOptions.Type != KREST::UNDEFINED)
		{
			throw KOptions::Error("Request type set twice");
		}

		m_ServerOptions.Type = KREST::LAMBDA;
		SetupInputFile(sArgs);
	});

} // ctor

//-----------------------------------------------------------------------------
void __ProjectName__::ApiOptions (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// required headers for callers making preflight requests
	HTTP.Response.Headers.Add (KHTTPHeader::ACCESS_CONTROL_ALLOW_HEADERS, "Origin, X-Requested-With, Content-Type, Authorization");
	HTTP.Response.Headers.Add (KHTTPHeader::ACCESS_CONTROL_ALLOW_METHODS, "PUT, GET, POST, OPTIONS, DELETE");
	// allow null response with CORS header

} // ApiOptions

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		KOptions Options(false);
		SetupOptions(Options);

		auto iRetVal = Options.Parse(argc, argv, KOut);

		if (Options.Terminate() || iRetVal)
		{
			// either error or completed
			return iRetVal;
		}

		// --- try to auto detect execution by web server ---

		if (m_ServerOptions.Type == KREST::UNDEFINED)
		{
			// by default, derive request type off EXE name:
			KStringView sName = Options.GetProgramName();

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
				Options.Help(KOut);
				return 1;
			}
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
