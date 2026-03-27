/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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

#include <dekaf2/koptions.h>
#include <dekaf2/kerror.h>
#include <dekaf2/krest.h>
#include <dekaf2/krestroute.h>
#include <dekaf2/khttperror.h>
#include <dekaf2/kwebserverpermissions.h>
#include <dekaf2/dekaf2.h> // KInit()

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A simple HTTP(s) server
class KHTTP : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	int Main(int argc, char** argv)
	//-----------------------------------------------------------------------------
	{
		// initialize, and because we are a server start a signal handler thread in parallel
		KInit(true);

		// we will throw when setting errors
		SetThrowOnError(true);

		// setup CLI option parsing
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

		// create an instance of the REST server options
		KREST::Options Settings;

		// define cli options and read them into the REST server options
		KStringViewZ sWWWDir          = Options("www <directory>       : base directory for HTTP server (served content)");
		bool bCreateAdHocIndex        = Options("autoindex             : enable directory browsing and auto-generated index listings, default false", false);
		bool bAllowUpload             = Options("upload                : allow upload into directory, default false", false);
		bool bWebDAV                  = Options("webdav                : enable WebDAV Class 1 support (PROPFIND, MKCOL, COPY, MOVE)", false);
		KStringViewZ sDefaultPerms    = Options("permissions <flags>   : default permissions (read|write|erase|browse|all|none), default 'read|browse'", "");
		KStringViewZ sUserParms       = Options("user <user:pass:/path:flags> : set user with permissions per path (flags: read|write|erase|browse|all|none)", "");
		KStringViewZ sUsersFile       = Options("users <pathname>      : set pathname for file with list of lines of user:pass:/path:flags", "");
		KStringViewZ sRoute           = Options("route </path>         : route to serve from, defaults to \"/*\"", "/*");
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		Settings.iPort                = Options("http <port>           : port number to bind to", 0);
		Settings.sSocketFile          = Options("socket <socket>       : unix domain socket file like /tmp/khttp.sock or unix:///tmp/khttp.sock", "");
#else
		Settings.iPort                = Options("http <port>           : port number to bind to");
#endif
		Settings.iMaxConnections      = Options("n <max>               : max parallel connections (default 25)", 25);
		Settings.iMaxKeepaliveRounds  = Options("keepalive <maxrounds> : max keepalive rounds (default 10, 0 == off)", 10);
		Settings.iTimeout             = Options("timeout <seconds>     : server timeout (default 5)", 5);
		double dRateLimit             = Options("ratelimit <req/s>     : per-IP request rate limit (default 0 == off)", 0.0);
		uint16_t iRateBurst           = Options("rateburst <count>     : rate limit burst size (default 10)", 10);
		uint16_t iConnLimit           = Options("connlimit <max>       : per-IP max concurrent connections (default 0 == off)", 0);
		Settings.sCert                = Options("cert <file>           : TLS certificate filepath (.pem), defaults to self-signed ephemeral cert", "");
		Settings.sKey                 = Options("key <file>            : TLS private key filepath (.pem), defaults to ephemeral key", "");
		Settings.bStoreEphemeralCert  = Options("persist               : should a self-signed cert be persisted to disk and reused at next start?", false);
		Settings.bCreateEphemeralCert =!Options("notls                 : do NOT switch to TLS mode if cert and key were not provided", false);
		Settings.sTLSPassword         = Options("tlspass <pass>        : TLS certificate password, if any", "");
		Settings.sAllowedCipherSuites = Options("ciphers <suites>      : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
		Settings.sBaseRoute           = Options("baseroute </path>     : route prefix, e.g. '/khttp', default none", "");
		KStringViewZ sRestLog         = Options("restlog <file>        : write rest server log to <file> - default off", "");
		Settings.KLogHeader           = Options("headerlog <x-klog>    : set header name to request and return trace logs, default off", "");
		KStringViewZ sServer          = Options("server <xyz/1.0>      : set server header contents, default khttp/1.0", "khttp/1.0");
		bool bQuiet                   = Options("quiet                 : force no output if no errors", false);

		// do a final check if all required options were set
		if (!Options.Check()) return 1;

		if (!bQuiet) kPrintLine(":: KHTTP/1.0");

		// verify some parms
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (!Settings.iPort && !Settings.sSocketFile) SetError("must have either http or socket option");
		if (Settings.iPort  &&  Settings.sSocketFile) SetError("http and socket options are mutually exclusive");
#endif

		// chose the server type, either TCP or UNIX sockets (we speak HTTP over UNIX as well)
		if (Settings.iPort)       Settings.Type = KREST::HTTP;
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		if (Settings.sSocketFile) Settings.Type = KREST::UNIX;
#endif

		if (sRestLog) 
		{
			// open a log file if requested
			if (!Settings.Logger.Open(sRestLog)) SetError(kFormat("cannot open log file: {}", sRestLog));
		}

		// the server will block until finished
		Settings.bBlocking = true;
		// we want a microsecond resolution timing header
		Settings.bMicrosecondTimerHeader = true;
		// and its name is x-microseconds
		Settings.TimerHeader = "x-microseconds";

		if (dRateLimit > 0) Settings.SetRateLimit(dRateLimit, iRateBurst);
		if (iConnLimit > 0) Settings.SetConnectionLimit(iConnLimit);

		// set up permissions
		KWebServerPermissions Permissions;

		// set default permissions from CLI flags
		if (sDefaultPerms)
		{
			Permissions.SetDefaultPermissions(KWebServerPermissions::ParsePermissions(sDefaultPerms));
		}
		else
		{
			// derive defaults from convenience flags
			KWebServerPermissions::Permission iPerms = KWebServerPermissions::Read;

			if (bCreateAdHocIndex) iPerms |= KWebServerPermissions::Browse;
			if (bAllowUpload)      iPerms |= KWebServerPermissions::Write | KWebServerPermissions::Erase;

			Permissions.SetDefaultPermissions(iPerms);
		}

		// add users from CLI or file
		if (sUserParms) Permissions.AddUser(sUserParms);
		if (sUsersFile)
		{
			if (!Permissions.LoadUsers(sUsersFile)) SetError(kFormat("cannot load users file: {}", sUsersFile));
		}

		// set up basic authentication if users are configured
		if (Permissions.HasUsers())
		{
			Settings.PreRouteCallback = [&Permissions](KRESTServer& HTTP)
			{
				auto Basic = HTTP.Request.GetBasicAuthParms();

				if (!Permissions.Authenticate(Basic.sUsername, Basic.sPassword))
				{
					HTTP.Response.Headers.Set(KHTTPHeader::WWW_AUTHENTICATE, "Basic realm=\"KHTTP\"");
					throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "not authorized" };
				}

				HTTP.SetAuthenticatedUser(Basic.sUsername);
			};
		}
		else
		{
			if (!bQuiet)
			{
				kPrintLine(">> configured with open access, permissions: {}",
				           KWebServerPermissions::SerializePermissions(Permissions.GetDefaultPermissions()));
				kPrintLine(">> use -user or -users options to restrict access");
			}
		}

		KRESTRoutes Routes;

		if (sWWWDir)
		{
			if (!kDirExists(sWWWDir)) SetError(kFormat("www directory does not exist: {}", sWWWDir));

			if (bWebDAV)
			{
				Routes.AddWebDAV(sWWWDir, sRoute, std::move(Permissions));
				if (!bQuiet) kPrintLine(":: serving WebDAV from: {}", sWWWDir);
			}
			else
			{
				Routes.AddWebServer(sWWWDir, sRoute, std::move(Permissions));
				if (!bQuiet) kPrintLine(":: serving files from: {}", sWWWDir);
			}
		}

		if (!sServer.empty())
		{
			Settings.AddHeader(KHTTPHeader::SERVER, sServer);
			// reverse proxies and LBs often override the server header, so lets add a second one
			Settings.AddHeader("x-server", sServer);
		}

		// create the REST server instance
		KREST Http;

		// and run it
		if (!bQuiet) kPrintLine(":: starting as '{}' on port {}", sServer, Settings.iPort);
		if (!Http.Execute(Settings, Routes)) SetError(Http.CopyLastError());

		return 0;

	} // Main

}; // KHTTP

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return KHTTP().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		kPrintLine(">> {}: {}", "khttp", ex.what());
	}
	return 1;
}
