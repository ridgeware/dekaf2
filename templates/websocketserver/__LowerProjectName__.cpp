
#include "__LowerProjectName__.h"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/net/address/knetworkinterface.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/url/kmime.h>
//@if service
#include <dekaf2/system/os/kservice.h>
//@endif

using namespace dekaf2;

namespace {

constexpr KStringView s_sStyle = R"(
body { font-family: system-ui, sans-serif; margin: 2em; }
#log { border: 1px solid #ccc; height: 20em; overflow-y: auto; padding: .5em; margin-bottom: 1em; }
#msg { width: 30em; }
)";

// the client of the chat: connect, show what arrives, send what is typed
constexpr KStringView s_sScript = R"(
const log = document.getElementById('log');
const msg = document.getElementById('msg');
const ws  = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/chat');
function show(text) { const line = document.createElement('div'); line.textContent = text; log.appendChild(line); log.scrollTop = log.scrollHeight; }
ws.onmessage = e => show(e.data);
ws.onclose   = () => show('-- connection closed');
function send() { if (msg.value) { ws.send(msg.value); msg.value = ''; } }
msg.addEventListener('keydown', e => { if (e.key === 'Enter') send(); });
)";

} // end of anonymous namespace

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
	kPrintLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
void __ProjectName__::ApiPage (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	html::Page Page(s_sProjectName, "en");

	Page.AddMeta("viewport", "width=device-width, initial-scale=1");
	Page.AddStyle(s_sStyle);

	auto Body = Page.Body();

	Body.Add<html::Heading>(1, kFormat("{} chat", s_sProjectName));
	Body.Add<html::Div>(html::Classes{}, "log");

	auto Input = Body.Add<html::Element>("input", html::Classes{}, "msg");
	Input.SetAttribute("placeholder", "type a message and press enter");
	Input.SetAttribute("autofocus", "");

	auto Send = Body.Add<html::Element>("button");
	Send.SetAttribute("onclick", "send()");
	Send.AddText("send");

	Body.Add<html::Script>(s_sScript);

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	HTTP.SetRawOutput(Page.Serialize());

} // ApiPage

//-----------------------------------------------------------------------------
void __ProjectName__::ApiChat (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// ---- insert the handling of the websocket messages here ----

	// called for every message of a client - relay it to all clients
	HTTP.SetWebSocketHandler([](KWebSocket& WebSocket)
	{
		WebSocket.GetServer()->Broadcast(WebSocket.GetFrame().GetPayload());
	});

	// called once after the connection was registered - its handle reaches it later
	HTTP.SetWebSocketConnectHandler([this](KWebSocket& WebSocket)
	{
		auto iClients = ++m_iClients;
		WebSocket.GetServer()->Send(WebSocket.GetHandle(), kFormat("-- welcome, {} connected", iClients));
	});

	// called once when the connection is gone
	HTTP.SetWebSocketCloseHandler([this](KWebSocketServer::Handle)
	{
		--m_iClients;
	});

} // ApiChat

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
//@if service
			.SetAdditionalHelp         (KService::GetHelp()) // the options to run as a service of systemd, launchd or Windows
//@endif
		;

		bool bShowVersion = Options("version : show software version and exit", false);

		// ---- insert project specific options here ----

		Options.Section("server options:");

		m_ServerOptions.iPort                    = Options("http <port>         : port number (default 8080)", 8080).Range(1, 65535);
		m_ServerOptions.sBindAddress             = Options("bind <address>      : bind the server to this address only, e.g. 127.0.0.1, default all interfaces", "");
		m_ServerOptions.iMaxConnections          = Options("n <max>             : max parallel HTTP connections (default 50) - upgraded websockets do not count", m_ServerOptions.iMaxConnections).Range(1, 65535);
		m_ServerOptions.Timeout                  = chrono::seconds(Options("timeout <seconds> : server timeout (default 5)", 5).Range(1, 60*60).UInt16());
		m_ServerOptions.iWebSocketWorkerThreads  = Options("workers <count>     : worker threads for the websocket messages (default 0 == in the I/O thread)", 0).Range(0, 1024);
		m_ServerOptions.WebSocketDeflate.bEnable = Options("deflate             : accept permessage-deflate compression if the client offers it", false);

		Options.Section("TLS options:");

		m_ServerOptions.sCert                    = Options("cert <file>         : TLS certificate filepath (.pem), defaults to a self-signed ephemeral certificate", "").Type(KOptions::File);
		m_ServerOptions.sKey                     = Options("key <file>          : TLS private key filepath (.pem), defaults to an ephemeral key", "").Type(KOptions::File);
		m_ServerOptions.bCreateEphemeralCert     =!Options("notls               : do NOT switch to TLS mode if cert and key were not provided", false);
		m_ServerOptions.bStoreEphemeralCert      = Options("persist             : persist a self-signed certificate on disk and reuse it at the next start", false);

		// check that all options were consumed - this also prints the help if it was requested
		if (!Options.Check())
		{
			return Options.Terminate() ? 0 : 1;
		}

		if (bShowVersion)
		{
			ShowVersion();
			return 0;
		}
	}

	m_ServerOptions.Type      = KREST::HTTP;
	m_ServerOptions.bBlocking = true;
	KLog::getInstance().SetMode(KLog::SERVER);

	KRESTRoutes Routes;

	// the root is the empty route
	Routes.AddRoute("").Get([this](KRESTServer& HTTP) { ApiPage(HTTP); }).Parse(KRESTRoute::NOREAD);
	// a route for the websocket protocol
	Routes.AddRoute("/chat").Get([this](KRESTServer& HTTP) { ApiChat(HTTP); }).Parse(KRESTRoute::NOREAD).Options(KRESTRoute::Options::WEBSOCKET);

	KStringView sScheme = (m_ServerOptions.bCreateEphemeralCert || !m_ServerOptions.sCert.empty()) ? "https" : "http";

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

	// stdout is buffered when it goes into a file or the log of a service manager
	KOut.Flush();

	KREST REST;

	if (!REST.Execute(m_ServerOptions, Routes))
	{
		throw KException(REST.Error());
	}

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	auto RunMain = [](int ac, char** av)
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
	};

//@if service
	// KService::Run() handles -install, -uninstall, -start, -stop and -status, and runs
	// Main() under systemd, launchd or the Windows service manager, or interactively
	return KService::Run("__LowerProjectName__", argc, argv, RunMain,
		/*bSuppressHelp*/true, // the service options are part of our own help
		__ProjectName__::s_sProjectName,
		"dekaf2 __ProjectType__ template");
//@else
	return RunMain(argc, argv);
//@endif

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
#endif
