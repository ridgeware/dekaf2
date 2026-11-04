
#include "__LowerProjectName__.h"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/url/kmime.h>

using namespace dekaf2;

namespace {

constexpr KStringView s_sStyle = R"(
body { font-family: system-ui, sans-serif; margin: 2em; }
button { margin-right: .5em; }
)";

// the page script: the click counter over the live connection of every page, and
// a function of the window that a browser does not have
constexpr KStringView s_sScript = R"(
const count = document.getElementById('count');
const out   = document.getElementById('out');
const hello = document.getElementById('hello');
kLive.on(m => { if (m.t === 'clicks') count.textContent = m.count; });
document.getElementById('click').onclick = () => kLive.send({ t: 'click' });
if (typeof window.kNative !== 'undefined')
{
	hello.onclick = async () => { const r = await window.kNative.hello({ name: 'window' }); out.textContent = r.text; };
}
else
{
	hello.disabled = true;
	hello.title    = 'only in the window';
}
)";

} // end of anonymous namespace

//-----------------------------------------------------------------------------
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	// the signal handler thread lets SIGINT and SIGTERM end Run()
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
	// defines window.kLive, the live connection of the page
	Page.Head().Add<html::Script>().SetSource("/_kwa/live.js");

	auto Body = Page.Body();

	Body.Add<html::Heading>(1, s_sProjectName);

	auto Clicks = Body.Add<html::Paragraph>();
	Clicks.AddText("clicks of all pages: ");
	Clicks.Add<html::Span>(html::Classes{}, "count").AddText("0");

	auto Click = Body.Add<html::Element>("button", html::Classes{}, "click");
	Click.AddText("click");

	auto Hello = Body.Add<html::Element>("button", html::Classes{}, "hello");
	Hello.AddText("say hello");

	Body.Add<html::Paragraph>(html::Classes{}, "out");

	Body.Add<html::Script>(s_sScript);

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	HTTP.SetRawOutput(Page.Serialize());

} // ApiPage

//-----------------------------------------------------------------------------
void __ProjectName__::BroadcastClicks ()
//-----------------------------------------------------------------------------
{
	m_App->Broadcast({ { "t", "clicks" }, { "count", m_iClicks.load() } });

} // BroadcastClicks

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	KWebApp::Options AppOptions;

	// ---------------- parse CLI ------------------
	{
		// no arguments is a normal start - an application started from the desktop has none
		KOptions Options(false, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options
			.SetBriefDescription       ("dekaf2 __ProjectType__ template")
			// .SetHelpSeparator          ("::")             // the column separator between option and help text
			// .SetLinefeedBetweenOptions (false)            // add linefeed between separate options or commands?
			// .SetWrappedHelpIndent      (1)                // the indent for continuation help text
			.SetSpacingPerSection      (true)                // whether commands and options get the same or separate column layout
			//.SetAdditionalHelp         (g_sAdditionalHelp) // extra help text at end of generated help
		;

		bool         bShowVersion  = Options("version              : show version information", false);

		// ---- insert project specific options here ----

		Options.Section("window options:");

		bool         bHeadless     = Options("headless             : no window, only the network server", false);
		bool         bInspector    = Options("inspector            : enable the web inspector in the window", false);

		Options.Section("network options:");

		uint16_t     iPort         = Options("listen <port>        : also serve browsers on the network, with TLS and a login", 0).Range(1, 65535);
		KStringViewZ sUser         = Options("user <name>          : the account for the network", "");
		KStringViewZ sPasswordFile = Options("password-file <path> : file with the password of the account in its first line", "").Type(KOptions::File);
		KStringViewZ sCert         = Options("cert <file>          : TLS certificate for the network (.pem), default a self-signed one", "").Type(KOptions::File);
		KStringViewZ sKey          = Options("key <file>           : TLS private key for the network (.pem)", "").Type(KOptions::File);

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

		if (bHeadless && !iPort)
		{
			throw KException("headless needs listen - else nobody sees the application");
		}

		AppOptions.sTitle   = s_sProjectName;
		AppOptions.sVersion = s_sProjectVersion;
		// ~/.config/__LowerProjectName__/ keeps the window geometry and the instance lock
		AppOptions.sAppName = "__LowerProjectName__";
		AppOptions.bWindow  = !bHeadless;
		AppOptions.bDebug   = bInspector;
		// a menu entry calls the bound handler of its action
		AppOptions.jMenus   = kjson::Parse(R"([ { "title": "__ProjectName__", "items": [ { "title": "Reset counter", "key": "r", "action": "reset" } ] } ])");

		if (iPort)
		{
			if (sUser.empty() || sPasswordFile.empty())
			{
				throw KException("listen needs user and password-file");
			}

			KString sPassword;

			if (!kReadTextFile(sPasswordFile, sPassword))
			{
				throw KException(kFormat("cannot read {}", sPasswordFile));
			}

			// the first line
			auto iEOL = sPassword.find_first_of("\r\n");

			if (iEOL != KString::npos)
			{
				sPassword.erase(iEOL);
			}

			if (sPassword.Trim().empty())
			{
				throw KException(kFormat("no password in the first line of {}", sPasswordFile));
			}

			// keep only the hash, and let bcrypt take its time on every check
			auto BCrypt = std::make_shared<KBCrypt>();
			auto sHash  = std::make_shared<KString>(BCrypt->GenerateHash(sPassword));
			KString sAccount = sUser;
			kSafeErase(sPassword);

			AppOptions.Authenticate = [BCrypt, sHash, sAccount](KStringView sName, KStringView sPass)
			{
				return sName == sAccount && BCrypt->ValidatePassword(KString(sPass), *sHash);
			};

			AppOptions.Network.iPort      = iPort;
			AppOptions.Network.sCert      = sCert;
			AppOptions.Network.sKey       = sKey;
			AppOptions.bNetwork           = true;
			// closing the window keeps the network server running
			AppOptions.bQuitOnWindowClose = false;
			AppOptions.bHideOnClose       = true;
		}
	}

	// the root is the empty route
	m_Routes.AddRoute("").Get([this](KRESTServer& HTTP) { ApiPage(HTTP); }).Parse(KRESTRoute::NOREAD);

	m_App = std::make_unique<KWebApp>(std::move(AppOptions), m_Routes);

	if (m_App->HasError())
	{
		throw KException(m_App->Error());
	}

	if (m_App->IsOtherInstanceRunning())
	{
		// the window of the first instance came to the front
		return 0;
	}

	// ---- insert the functions of the application here ----

	// in the page of the window: await window.kNative.hello({ name: "..." })
	m_App->Bind("hello", [](const KJSON& jArg) -> KJSON
	{
		return { { "text", kFormat("hello {}, this is {}", jArg["name"].String(), s_sProjectName) } };
	});

	// the action of the menu entry
	m_App->Bind("reset", [this](const KJSON&) -> KJSON
	{
		m_iClicks = 0;
		BroadcastClicks();
		return {};
	});

	// a page sends { t: "click" } over its live connection
	m_App->OnMessage([this](const KWebApp::Client&, const KJSON& jMessage)
	{
		if (jMessage["t"].String() == "click")
		{
			++m_iClicks;
			BroadcastClicks();
		}
	});

	// a new page gets the current count
	m_App->OnConnect([this](const KWebApp::Client& Client)
	{
		m_App->Send(Client, { { "t", "clicks" }, { "count", m_iClicks.load() } });
	});

	if (m_App->GetNetworkPort())
	{
		kPrintLine(":: serving browsers at https://localhost:{}", m_App->GetNetworkPort());
		// stdout is buffered when it goes into a file
		KOut.Flush();
	}

	// blocks in the UI loop until the window closes, Quit() is called, or a shutdown signal arrives
	return m_App->Run();

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
#endif
