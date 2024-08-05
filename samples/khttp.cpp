#include <dekaf2/koptions.h>
#include <dekaf2/kerror.h>
#include <dekaf2/krest.h>
#include <dekaf2/krestroute.h>

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
		KStringViewZ sRoute           = Options("route </path>         : route to serve from, defaults to \"/*\"", "/*");
		Settings.iPort                = Options("http <port>           : port number to bind to", 0);
		Settings.sSocketFile          = Options("socket <socket>       : unix domain socket file like /tmp/khttp.sock or unix:///tmp/khttp.sock", "");
		Settings.iMaxConnections      = Options("n <max>               : max parallel connections (default 25)", 25);
		Settings.iMaxKeepaliveRounds  = Options("keepalive <maxrounds> : max keepalive rounds (default 10, 0 == off)", 10);
		Settings.iTimeout             = Options("timeout <seconds>     : server timeout (default 5)", 5);
		Settings.sCert                = Options("cert <file>           : TLS certificate filepath (.pem)", "");
		Settings.sKey                 = Options("key <file>            : TLS private key filepath (.pem)", "");
		Settings.sTLSPassword         = Options("tlspass <pass>        : TLS certificate password, if any", "");
		Settings.sAllowedCipherSuites = Options("ciphers <suites>      : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
		Settings.sBaseRoute           = Options("baseroute </path>     : route prefix, e.g. '/khttp', default none", "");
		KStringViewZ sRestLog         = Options("restlog <file>        : write rest server log to <file> - default off", "");

		// do a final check if all required options were set
		if (!Options.Check()) return 1;

		// verify some parms
		if (!Settings.iPort && !Settings.sSocketFile) SetError("must have either http or socket option");
		if ( Settings.iPort &&  Settings.sSocketFile) SetError("http and socket options are mutually exclusive");

		// chose the server type, either TCP or UNIX sockets (we speak HTTP over UNIX as well)
		if (Settings.iPort)       Settings.Type = KREST::HTTP;
		if (Settings.sSocketFile) Settings.Type = KREST::UNIX;

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
		// activate header logging
		Settings.KLogHeader = "x-klog";

		KRESTRoutes Routes;

		if (sWWWDir)
		{
			if (!kDirExists(sWWWDir)) SetError(kFormat("www directory does not exist: {}", sWWWDir));
			// add a web server for static files
			Routes.AddWebServer(sWWWDir, sRoute);
		}

		// create the REST server instance
		KREST Http;

		// and run it
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
