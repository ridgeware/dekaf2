
#include "__LowerProjectName__.h"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/crypto/auth/kopenid.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/net/address/knetworkinterface.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/kservice.h>

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
void __ProjectName__::SetServerType (KREST::ServerType Type)
//-----------------------------------------------------------------------------
{
	if (m_ServerOptions.Type != KREST::UNDEFINED)
	{
		throw KOptions::Error("Request type set twice");
	}

	m_ServerOptions.Type = Type;

} // SetServerType

//-----------------------------------------------------------------------------
void __ProjectName__::SetupInputFile (KREST::ServerType Type, const KOptions::Values& Args)
//-----------------------------------------------------------------------------
{
	if (Args.Exists())
	{
		SetServerType(Type);

		if (!Args.empty())
		{
			// the first argument is the input file, further arguments remain as excess commands
			m_ServerOptions.Simulate.sFilename = Args.String();
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
void __ProjectName__::ShowVersion ()
//-----------------------------------------------------------------------------
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

} // ShowVersion

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
		KOptions Options(false, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options
			.SetBriefDescription       ("dekaf2 __ProjectType__ template")
			// .SetHelpSeparator          ("::")            // the column separator between option and help text
			// .SetLinefeedBetweenOptions (false)           // add linefeed between separate options or commands?
			// .SetWrappedHelpIndent      (1)               // the indent for continuation help text
			.SetSpacingPerSection      (true)               // whether commands and options get the same or separate column layout
			.SetAdditionalHelp         (KService::GetHelp()); // the options to run as a service of systemd, launchd or Windows

		bool bShowVersion = Options("version : show software version and exit", false);

		// ---- insert project specific options here ----

		Options.Section("general REST server options:");

		uint16_t                  iPort            = Options("http <port>                : force HTTP server mode, requires port number", 0).Range(1, 65535);
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		KStringViewZ              sSocketFile      = Options("socket <socket>            : force HTTP server mode on unix domain socket file like /tmp/__LowerProjectName__.sock or unix:///tmp/__LowerProjectName__.sock", "");
#endif
		m_ServerOptions.sBindAddress               = Options("bind <address>             : bind the HTTP server to this address only, e.g. 127.0.0.1, default all interfaces", "");
		m_ServerOptions.iMaxConnections            = Options("n <max>                    : max parallel connections (default 50, only for HTTP mode)", m_ServerOptions.iMaxConnections).Range(1, 65535);
		m_ServerOptions.iMaxKeepaliveRounds        = Options("keepalive <maxrounds>      : max keepalive rounds for HTTP mode (default 50, 0 == off)", m_ServerOptions.iMaxKeepaliveRounds).Range(0, 65535);
		m_ServerOptions.Timeout                    = chrono::seconds(Options("timeout <seconds> : HTTP server timeout (default 5, max 3600 (one hour))", 5).Range(1, 60*60).UInt16());
		m_ServerOptions.FirstRequestTimeout        = chrono::seconds(Options("firsttimeout <seconds> : how long a new connection may wait for its first request (default 0 == the server timeout)", 0).Range(0, 60*60).UInt16());
		m_ServerOptions.KeepAliveTimeout           = chrono::seconds(Options("keepalivetimeout <seconds> : how long an idle keep-alive connection may wait for its next request (default 0 == the server timeout)", 0).Range(0, 60*60).UInt16());
		KStringViewZ              sMaxBody         = Options("maxbody <size>             : max request body size, supports k/M/G suffixes (default 256M, 0 == unlimited)", "256M");
		KStringViewZ              sCompressors     = Options("compressors <methods>      : permitted http compression algorithms, default = br,deflate,gzip", "");
		m_ServerOptions.sBaseRoute                 = Options("baseroute </path>          : route prefix, e.g. '/__LowerProjectName__'", "");
		std::vector<KStringViewZ> RestLog          = Options("restlog <file>\n"
		                                                     "        [<type>\n"
		                                                     "        [<format>]] : "
		                                                     "write rest server log to <file> - default off. "
		                                                     "<type> is one of JSON, COMMON, COMBINED, EXTENDED, PARSED, default EXTENDED. "
		                                                     "For PARSED, define <format> string in apache format (e.g. \"%h %l %u %t \\\"%r\\\" %>s %b \\\"%{Referer}i\\\"\")",
		                                                     std::vector<KStringViewZ>{});
		KStringViewZ              sRestLogPeriod   = Options("restlogperiod <period>     : start a new rest log file every day, week or month at midnight UTC, and rename the past ones after their period - default off", "");
		uint16_t                  iRestLogKeep     = Options("restlogkeep <count>        : the number of past rest log files to keep with -restlogperiod (default 0 == all)", 0);
		m_ServerOptions.KLogHeader                 = Options("headerlog <name>           : header name with which a client requests the debug log of its request - choose an unguessable name and use it only in trusted networks, default off", "");

		Options.Section("rate and connection limits:");

		double                    dRateLimit       = Options("ratelimit <req/s>          : per-IP request rate limit, e.g. 10 or 0.5 (default 0 == off)", 0.0);
		uint16_t                  iRateBurst       = Options("rateburst <count>          : rate limit burst size (default 10)", 10).Range(1, 65535);
		uint16_t                  iConnLimit       = Options("connlimit <max>            : per-IP max concurrent connections (default 0 == off)", 0).Range(0, 65535);
		KStringViewZ              sTrustedProxies  = Options("proxies <ip[/prefix],...>  : comma separated list of trusted proxies - the client IP for the rate limit and the logs is then taken from their forwarding headers", "");

		Options.Section("TLS options:");

		m_ServerOptions.sCert                      = Options("cert <file>                : TLS certificate filepath (.pem), defaults to a self-signed ephemeral certificate", "").Type(KOptions::File);
		m_ServerOptions.sKey                       = Options("key <file>                 : TLS private key filepath (.pem), defaults to an ephemeral key", "").Type(KOptions::File);
		m_ServerOptions.sTLSPassword               = Options("tlspass <pass>             : TLS certificate password, if any", "");
		m_ServerOptions.sDHPrimes                  = Options("dh <file>                  : TLS DH prime filepath (.pem) (generate with 'openssl dhparam -out dh2048.pem 2048')", "").Type(KOptions::File);
		m_ServerOptions.sAllowedCipherSuites       = Options("ciphers <suites>           : colon delimited list of permitted cipher suites for TLS, defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", m_ServerOptions.sAllowedCipherSuites);
		m_ServerOptions.bCreateEphemeralCert       =!Options("notls                      : do NOT switch to TLS mode if cert and key were not provided", false);
		m_ServerOptions.bStoreEphemeralCert        = Options("persist                    : persist a self-signed certificate on disk and reuse it at the next start", false);
		std::vector<KStringViewZ> Domains          = Options("domain <name>              : additional domain or IP for the self-signed TLS certificate (repeatable)", std::vector<KStringViewZ>{}).MaxArgs(1);
		bool                      bNoLocalNets     = Options("nolocalnets                : do not include local network addresses in the self-signed TLS certificate", false);

		Options.Section("automatic TLS certificates:");

		std::vector<KStringViewZ> ACMEDomains      = Options("acme <domain>              : obtain and renew the TLS certificate for <domain> via ACME / Let's Encrypt (repeatable) - the CA connects to port 443 of the domain, which must reach this server", std::vector<KStringViewZ>{}).MaxArgs(1);
		m_ServerOptions.sACMEContact               = Options("acmecontact <mailto>       : ACME account contact, e.g. mailto:admin@example.com", "");
		m_ServerOptions.sACMEDirectoryURL          = Options("acmedir <url>              : ACME directory URL, default Let's Encrypt production", "");
		m_ServerOptions.sACMEStorage               = Options("acmestore <directory>      : storage for the ACME account key and certificate, default the TLS config directory", "");
		m_ServerOptions.bACMEVerifyTLS             =!Options("acmenoverify               : do not verify the ACME directory's CA (test servers like Pebble)", false);

		if (!s_SSOProvider.empty())
		{
			Options.Section("SSO options:");

			m_ServerOptions.iSSOLevel              = Options("ssolevel <0..2>            : set SSO authentication level, default = 0 (off), header check = 1, real check = 2", 0).Range(0, 2);
		}

		Options.Section("options for simulation mode (local testing):");

		KStringViewZ              sSimulate    = Options("sim <url>             : simulate request to a __LowerProjectName__ method (will use GET unless -X is used)", "");
		KStringViewZ              sMethod      = Options("X,request <method>    : use with -sim: change request method of simulated request", "");
		std::vector<KStringViewZ> Headers      = Options("H,header <header>     : use with -sim: add \"Header: value\" additional headers to simulated request", std::vector<KStringViewZ>{}).MaxArgs(1);
		KStringViewZ              sData        = Options("D,data [@]<data>      : use with -sim: add literal request body, or with @ take contents of file", "");

		Options.Section("cgi cli usage:");

		SetupInputFile(KREST::CGI,    Options("cgi [<file>]              : force CGI mode, where <file> contains request + headers + post data", std::vector<KStringViewZ>{}).Type(KOptions::File));
#ifdef DEKAF2_WITH_FCGI
		SetupInputFile(KREST::FCGI,   Options("fcgi [<file>]             : force FCGI mode", std::vector<KStringViewZ>{}).Type(KOptions::File));
#endif

		Options.Section("aws-lambda usage:");

		SetupInputFile(KREST::LAMBDA, Options("lambda [<lambda-arg> ...] : force AWS Lambda mode", std::vector<KStringViewZ>{}).Type(KOptions::File));

		// check that all options were consumed - this also prints the help if it was requested
		if (!Options.Check())
		{
			return 1;
		}

		if (bShowVersion)
		{
			ShowVersion();
			return 0;
		}

		if (iPort)
		{
			SetServerType(KREST::HTTP);
			m_ServerOptions.iPort = iPort;
			KLog::getInstance().SetMode(KLog::SERVER);
		}

#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (!sSocketFile.empty())
		{
			SetServerType(KREST::UNIX);

			if (sSocketFile.starts_with("unix://"))
			{
				sSocketFile.remove_prefix(7);
			}

			m_ServerOptions.sSocketFile = sSocketFile;
			KLog::getInstance().SetMode(KLog::SERVER);
		}
#endif

		m_ServerOptions.iMaxRequestBodySize = kFromBinarySize(sMaxBody);

		if (dRateLimit > 0)
		{
			m_ServerOptions.SetRateLimit(dRateLimit, iRateBurst);
		}

		if (iConnLimit > 0)
		{
			m_ServerOptions.SetConnectionLimit(iConnLimit);
		}

		if (!sTrustedProxies.empty())
		{
			m_ServerOptions.AddTrustedProxies(sTrustedProxies);
		}

		m_ServerOptions.SANDomains.assign(Domains.begin(), Domains.end());

		if (bNoLocalNets)
		{
			m_ServerOptions.SANPolicy = m_ServerOptions.SANDomains.empty()
			                          ? SelfSignedCertSANPolicy::LocalhostOnly
			                          : SelfSignedCertSANPolicy::Manual;
		}

		m_ServerOptions.ACMEDomains.assign(ACMEDomains.begin(), ACMEDomains.end());

		if (!m_ServerOptions.ACMEDomains.empty() && !m_ServerOptions.bCreateEphemeralCert && m_ServerOptions.sCert.empty())
		{
			throw KException("acme needs a TLS server - do not combine it with notls");
		}

		if (!sCompressors.empty())
		{
			KHTTPCompression::SetPermittedCompressors(sCompressors);
		}

		if (!m_ServerOptions.sBaseRoute.empty() && m_ServerOptions.sBaseRoute.front() != '/')
		{
			throw KException("base route prefix has to start with a /");
		}

		if (!RestLog.empty())
		{
			KHTTPLog::LOG_FORMAT Format = KHTTPLog::LOG_FORMAT::EXTENDED;

			if (RestLog.size() > 1)
			{
				switch (RestLog[1].ToUpperASCII().Hash())
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
						throw KException(kFormat("unknown log format '{}' - must be one of JSON,COMMON,COMBINED,EXTENDED,PARSED", RestLog[1]));
				}
			}

			KStringView sFormat;

			if (Format == KHTTPLog::LOG_FORMAT::PARSED)
			{
				if (RestLog.size() > 2)
				{
					sFormat = RestLog[2];
				}
				else
				{
					throw KException("missing format definition for PARSED format");
				}
			}

			bool bOpened { false };

			if (sRestLogPeriod.empty())
			{
				bOpened = m_ServerOptions.Logger.Open(Format, RestLog[0], sFormat);
			}
			else
			{
				KDatedLogFile::Options Dated;

				switch (sRestLogPeriod.ToLowerASCII().Hash())
				{
					case "day"_hash:
						Dated.Period = KDatedLogFile::Day;
						break;
					case "week"_hash:
						Dated.Period = KDatedLogFile::Week;
						break;
					case "month"_hash:
						Dated.Period = KDatedLogFile::Month;
						break;
					default:
						throw KException(kFormat("unknown log period '{}' - must be one of day, week, month", sRestLogPeriod));
				}

				if (iRestLogKeep)
				{
					Dated.iRotations = iRestLogKeep;
				}

				bOpened = m_ServerOptions.Logger.Open(Format, RestLog[0], sFormat, Dated);
			}

			if (!bOpened)
			{
				throw KException("cannot open log stream");
			}
		}
		else if (!sRestLogPeriod.empty())
		{
			throw KException("restlogperiod needs a restlog");
		}

		if (!sSimulate.empty())
		{
			SetServerType(KREST::SIMULATE_HTTP);
			m_ServerOptions.Simulate.API = sSimulate;
		}

		if (!sMethod.empty())
		{
			m_ServerOptions.Simulate.Method = sMethod.ToUpperASCII();

			if (!m_ServerOptions.Simulate.Method.Serialize().In(KHTTPMethod::REQUEST_METHODS))
			{
				throw KException(kFormat("invalid request method '{}', legal ones are: {}", sMethod, KHTTPMethod::REQUEST_METHODS));
			}
		}

		for (auto sHeader : Headers)
		{
			auto Pair = kSplitToPair(sHeader, ":");
			m_ServerOptions.Simulate.AdditionalRequestHeaders.push_back(Pair.first, Pair.second);
		}

		if (sData.starts_with('@'))
		{
			sData.remove_prefix(1);

			if (!kReadAll (sData, m_ServerOptions.Simulate.sBody))
			{
				throw KException(kFormat("invalid filename: {}", sData));
			}
		}
		else
		{
			m_ServerOptions.Simulate.sBody = sData;
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

	// create a REST router
	KRESTRoutes Routes;
	// add our REST routes to it
	Routes.AddMemberFunctionTable(*this, __ProjectName__Routes);

	if (m_ServerOptions.Type == KREST::HTTP)
	{
		bool bTLS        = m_ServerOptions.bCreateEphemeralCert || !m_ServerOptions.sCert.empty();
		bool bSelfSigned = m_ServerOptions.bCreateEphemeralCert &&  m_ServerOptions.sCert.empty() && m_ServerOptions.ACMEDomains.empty();

		if (bSelfSigned)
		{
			// offer the self-signed certificate for the import into the trust store of a client
			Routes.AddRoute("/.well-known/cert.pem").Get([](KRESTServer& HTTP)
			{
				KString sCert;

				if (!kReadTextFile(KRSACert::GetDefaultCertFilename(), sCert) || sCert.empty())
				{
					throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "certificate not available - start the server with -persist" };
				}

				HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, "application/x-pem-file");
				HTTP.SetRawOutput(std::move(sCert));

			}).Parse(KRESTRoute::NOREAD);
		}

		KStringView sScheme = bTLS ? "https" : "http";

		if (!m_ServerOptions.sBindAddress.empty())
		{
			kPrintLine(":: listening at {}://{}:{}", sScheme, m_ServerOptions.sBindAddress, m_ServerOptions.iPort);
		}
		else
		{
			for (auto& Network : KNetworkInterface::GetRoutableNetworks())
			{
				if (Network.Is6())
				{
					kPrintLine(":: reachable at {}://[{}]:{}", sScheme, Network.ToString(false), m_ServerOptions.iPort);
				}
				else
				{
					kPrintLine(":: reachable at {}://{}:{}", sScheme, Network.ToString(false), m_ServerOptions.iPort);
				}
			}
		}

		if (bSelfSigned)
		{
			kPrintLine(":: self-signed certificate at /.well-known/cert.pem");
		}

		// stdout is buffered when it goes into a file or the log of a service manager
		KOut.Flush();
	}
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
	// KService::Run() handles -install, -uninstall, -start, -stop and -status, and runs
	// Main() under systemd, launchd or the Windows service manager, or interactively
	return KService::Run("__LowerProjectName__", argc, argv,
		[](int ac, char** av)
		{
			try
			{
				return __ProjectName__().Main (ac, av);
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
		},
		/*bSuppressHelp*/true, // the service options are part of our own help
		__ProjectName__::s_sProjectName,
		"dekaf2 __ProjectType__ template");

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
constexpr KStringView  __ProjectName__::s_SSOProvider;
constexpr KStringView  __ProjectName__::s_SSOScope;
#endif
