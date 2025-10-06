/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

// KTunnel can be started either as an exposed host (that can listen on ports
// reachable by one or more clients) or as a protected host (so called because
// it typically will be protected behind a firewall or network configuration
// (like mobile IP networks) that do not allow incoming connections).
//
// The protected KTunnel connects via HTTPS (upgrading to websockets) to the
// exposed KTunnel, and opens more TLS data streams inside the websocket
// connection as requested by the exposed KTunnel, typically on request of
// incoming TCP connections to the latter, being either raw or TLS or whatever
// encoded - the connection is fully transparent.
//
// The protected KTunnel connects to the final target host, as requested by
// the exposed KTunnel (setup per forwarded connection).
//
// The exposed KTunnel opens one TLS communications port for internal
// communication and general REST services, and one or more TCP forward
// ports for different targets.
//
//         e.g. port 1234         e.g. port 443
//                                              |
// client >>--TCP/TLS-->> KTunnel <<----TLS websocket----<< KTunnel >>--TCP/TLS-->> target(s)
//            data in    (exposed)              |         (protected)   data in
//          any protocol                        |                     any protocol
//                                     firewall |
//                       or e.g. mobile network |
//
// The control and data streams between the tunnel endpoints are multiplexed
// by a simple protocol over a TLS websocket connection, initiated from upstream.
//
// The tunnel connection can transport up to 16 million tunneled streams.

#include "ktunnel.h"
#include <dekaf2/kscopeguard.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
void ExtendedConfig::PrintMessage(KStringView sMessage) const
//-----------------------------------------------------------------------------
{
	if (!bQuiet)
	{
		kWriteLine(sMessage);
	}
	else
	{
		kDebug(1, sMessage);
	}

} // ExtendedConfig::PrintMessage

//-----------------------------------------------------------------------------
void ExposedServer::ControlStream(std::unique_ptr<KIOStreamSocket> Stream)
//-----------------------------------------------------------------------------
{
	m_Tunnel = std::make_unique<KTunnel>(m_Config, std::move(Stream));

	auto EndpointAddress = m_Tunnel->GetEndPointAddress();

	m_Config.Message("[{}]: opened control stream from {}", 0, EndpointAddress);

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [this, &EndpointAddress]() noexcept
	{
		// make sure the write control stream is safely removed
		m_Tunnel.reset();
		m_Config.Message("[{}]: closed control stream from {}", 0, EndpointAddress);
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	m_Tunnel->Run();

} // ControlStream

//-----------------------------------------------------------------------------
void ExposedServer::ForwardStream(KIOStreamSocket& Downstream, const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	// this is the stream between the client and the first ktunnel instance which
	// will be forwarded through the tunnel

	kDebug(1, "incoming connection from {} for target {}", Downstream.GetEndPointAddress(), Endpoint);

	if (Endpoint.empty())
	{
		throw KError("missing target endpoint definition with domain:port");
	}

	if (!m_Tunnel)
	{
		throw KError("no tunnel established");
	}

	// is already set from KTCPServer ?
	Downstream.SetTimeout(m_Config.Timeout);

	// connect through the tunnel
	// will throw or return after connection is closed
	m_Tunnel->Connect(&Downstream, Endpoint);

} // ForwardStream

//-----------------------------------------------------------------------------
ExposedServer::ExposedServer (const Config& config)
//-----------------------------------------------------------------------------
: m_Config(config)
{
	KREST::Options Settings;

	Settings.Type                    = KREST::HTTP;
	Settings.iPort                   = m_Config.iPort;
	Settings.iMaxConnections         = m_Config.iMaxTunneledConnections * 2 + 20;
	Settings.iTimeout                = m_Config.Timeout.seconds().count();
	Settings.KLogHeader              = "";
	Settings.bBlocking               = false;
	Settings.bMicrosecondTimerHeader = true;
	Settings.TimerHeader             = "x-microseconds";
	Settings.sCert                   = m_Config.sCertFile;
	Settings.sKey                    = m_Config.sKeyFile;
	Settings.sTLSPassword            = m_Config.sTLSPassword;
	Settings.sAllowedCipherSuites    = m_Config.sCipherSuites;
	Settings.bCreateEphemeralCert    = true;
	Settings.bStoreEphemeralCert     = m_Config.bPersistCert;

	Settings.AddHeader(KHTTPHeader::SERVER, "ktunnel");
	Settings.AddHeader("x-server", "ktunnel");

	KRESTRoutes Routes;

	Routes.AddRoute("/Version").Get([](KRESTServer& HTTP)
	{
		html::Page Page("Version", "en_US");
		Page.Body() += html::Text("ktunnel v1.0");
		HTTP.SetRawOutput(Page.Serialize());
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE , KMIME::HTML_UTF8);

	}).Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute("/Configure").Get([](KRESTServer& HTTP)
	{
		/// TODO

	}).Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute("/Tunnel").Get([this](KRESTServer& HTTP)
	{
		// set the handler for websockets in the REST server instance
		HTTP.SetWebSocketHandler([this](KWebSocket& WebSocket)
		{
			ControlStream(WebSocket.MoveStream());
		});

		// ask for per-thread execution in the REST server instance
		HTTP.SetKeepWebSocketInRunningThread();

	}).Parse(KRESTRoute::ParserType::NOREAD).Options(KRESTRoute::Options::WEBSOCKET);

	// set a default route
	Routes.SetDefaultRoute([](KRESTServer& HTTP)
	{
		if (HTTP.RequestPath.sRoute == "/favicon.ico" ||
			HTTP.RequestPath.sRoute.starts_with("/apple-touch-icon"))
		{
			HTTP.Response.SetStatus(KHTTPError::H4xx_NOTFOUND);
		}
		else if (HTTP.RequestPath.sRoute == "/robots.txt")
		{
			HTTP.SetRawOutput("User-agent: *\nDisallow: /\n");
			HTTP.Response.SetStatus(KHTTPError::H2xx_OK);
			HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		}
		else
		{
			HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, "/Configure/");
		}
	});

	// create the REST server instance
	KREST Http;

	// and run it
	m_Config.Message("starting TLS REST server on port {}", Settings.iPort);
	if (!Http.Execute(Settings, Routes)) throw KError(Http.Error());

	// start the server to forward raw data
	m_ExposedRawServer = std::make_unique<ExposedRawServer>
	(
		this,
		m_Config.DefaultTarget,
		m_Config.iRawPort,
		false,
		m_Config.iMaxTunneledConnections
	);

	m_Config.Message("listening on port {} for forward data connections", m_Config.iRawPort);

	// listen on TCP
	m_ExposedRawServer->Start(m_Config.Timeout, /* bBlock= */ true);

} // ExposedServer::ExposedServer


//-----------------------------------------------------------------------------
// handles one incoming connection
// - a raw tcp stream to forward via TLS to the protected host
void ExposedRawServer::Session (std::unique_ptr<KIOStreamSocket>& Stream)
//-----------------------------------------------------------------------------
{
	m_ExposedServer.ForwardStream(*Stream, m_Target);
}

//-----------------------------------------------------------------------------
ProtectedHost::ProtectedHost(const ExtendedConfig& Config)
//-----------------------------------------------------------------------------
: m_Config(Config)
{
	KHTTPStreamOptions Options;
	// connect timeout to exposed host, also wait timeout between two tries
	Options.SetTimeout(m_Config.ConnectTimeout);
	// only do HTTP/1.1, we haven't implemented websockets for any later protocol
	// (but as we are not a browser that doesn't mean any disadvantage)
	Options.Unset(KStreamOptions::RequestHTTP2);

	for (;;)
	{
		try
		{
			m_Config.Message("connecting {}..", m_Config.ExposedHost);

			KURL ExposedHost;
			ExposedHost.Protocol = url::KProtocol::HTTPS;
			ExposedHost.Domain   = m_Config.ExposedHost.Domain;
			ExposedHost.Port     = m_Config.ExposedHost.Port;
			ExposedHost.Path     = "/Tunnel";

			KWebSocketClient WebSocket(ExposedHost, Options);

			WebSocket.SetTimeout(m_Config.ConnectTimeout);
			WebSocket.SetBinary();

			if (!WebSocket.Connect())
			{
				throw KError("cannot establish tunnel connection");
			}

			// we are the "client" side of the tunnel
			KTunnel Tunnel(m_Config, std::move(WebSocket.GetStream()), "", *m_Config.Secrets.begin());
/* TODO
			// a client has to actively call Login
			if (!Tunnel.Login())
			{
				m_Config.Message("login failed!");
				return;
			}
*/
			m_Config.Message("control stream opened - now waiting for data streams");

			Tunnel.Run();

			m_Config.Message("lost connection - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}
		catch(const std::exception& ex)
		{
			m_Config.Message("{}", ex.what());
			m_Config.Message("could not connect - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}

		// sleep until next connect try
		kSleep(m_Config.ConnectTimeout);
	}

} // ProtectedHost::ctor

//-----------------------------------------------------------------------------
int Tunnel::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// initialize with signal handler thread
	KInit(true).SetName("KTUNL").KeepCLIMode();

	// we will throw when setting errors
	SetThrowOnError(true);

	// setup CLI option parsing
	KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

	// define cli options
	m_Config.ExposedHost   = Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
	m_Config.iPort         = Options("p,port       : port number to listen at for TLS connections (if exposed host), or connect to (if protected host) - defaults to 443.", 443);
	m_Config.iRawPort      = Options("f,forward    : port number to listen at for raw TCP connections that will be forwarded (if exposed host)", 0);
	KStringView sSecrets   = Options("s,secret     : if exposed host, takes comma separated list of secrets for login by protected hosts, or one single secret if this is the protected host.").String();
	m_Config.DefaultTarget = Options("t,target     : if exposed host, takes the domain:port of a default target, if no other target had been specified in the incoming data connect", "");
	m_Config.iMaxTunneledConnections
	                       = Options("m,maxtunnels : if exposed host, maximum number of tunnels to open, defaults to 10 - if protected host, the setting has no effect.", 10);
	m_Config.sCertFile     = Options("cert <file>  : if exposed host, TLS certificate filepath (.pem) - if option is unused a self-signed cert is created", "");
	m_Config.sKeyFile      = Options("key <file>   : if exposed host, TLS private key filepath (.pem) - if option is unused a new key is created", "");
	m_Config.sTLSPassword  = Options("tlspass <pass> : if exposed host, TLS certificate password, if any", "");
	m_Config.Timeout       = chrono::seconds(Options("to,timeout <seconds> : data connection timeout in seconds (default 30)", 30));
	m_Config.bPersistCert  = Options("persist      : should a self-signed cert be persisted to disk and reused at next start?", false);
	m_Config.sCipherSuites = Options("ciphers <suites> : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
	m_Config.bQuiet        = Options("q,quiet      : do not output status to stdout", false);

	// do a final check if all required options were set
	if (!Options.Check()) return 1;

	if (!m_Config.iPort)
	{
		SetError("need valid port number");
	}

	if (m_Config.ExposedHost.Port.empty())
	{
		m_Config.ExposedHost.Port.get() = m_Config.iPort;
	}

	if (!IsExposed() && (!m_Config.sCertFile.empty() || !m_Config.sKeyFile.empty()))
	{
		SetError("the protected host may not have a cert or key option");
	}

	// split and store list of secrets
	for (auto& sSecret : sSecrets.Split())
	{
		m_Config.Secrets.insert(sSecret);
	}

	if (m_Config.Secrets.empty()) SetError("need at least one (shared) secret");
	if (!m_Config.iMaxTunneledConnections) SetError("maxtunnels should be at least 1");

	if (IsExposed())
	{
		if (m_Config.DefaultTarget.empty()) SetError("exposed host needs a default target");
		if (m_Config.DefaultTarget.Port.get() == 0) SetError("endpoind needs an explicit port number");
		if (!m_Config.iRawPort) SetError("exposed host needs a forward port being configured");
		m_Config.Message("starting as exposed host");

		ExposedServer Exposed(m_Config);
	}
	else
	{
		m_Config.Message("starting as protected host, connecting exposed host at {}", m_Config.ExposedHost);

		ProtectedHost Protected(m_Config);
	}

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return Tunnel().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", "ktunnel", ex.what());
	}
	return 1;
}
