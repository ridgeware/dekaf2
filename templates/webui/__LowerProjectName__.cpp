
#include "__LowerProjectName__.h"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/net/address/knetworkinterface.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/web/ui/kwebui.h>
#include <dekaf2/web/url/kmime.h>
//@if service
#include <dekaf2/system/os/kservice.h>
//@endif

using namespace dekaf2;

namespace {

constexpr KStringView s_sStyle = R"(
body  { font-family: system-ui, sans-serif; margin: 0; }
nav   { background: #333; padding: .7em 2em; }
nav a { color: #fff; margin-right: 1.5em; text-decoration: none; }
main  { margin: 2em; max-width: 40em; }
.card { border: 1px solid #ccc; border-radius: 6px; padding: 1em; margin-bottom: 1em; }
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
KHTMLNode __ProjectName__::BeginPage (html::Page& Page)
//-----------------------------------------------------------------------------
{
	Page.AddMeta("viewport", "width=device-width, initial-scale=1");
	Page.AddStyle(s_sStyle);

	auto Nav = Page.Body().Add<html::Element>("nav");
	Nav.Add<html::Link>("/", s_sProjectName);
	Nav.Add<html::Link>("/settings", "Settings");

	return Page.Body().Add<html::Element>("main");

} // BeginPage

//-----------------------------------------------------------------------------
void __ProjectName__::SendPage (KRESTServer& HTTP, html::Page& Page)
//-----------------------------------------------------------------------------
{
	// gives the bound inputs of the page their names
	Page.Generate();

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	HTTP.SetRawOutput(Page.Serialize());

} // SendPage

//-----------------------------------------------------------------------------
void __ProjectName__::AddSettingsForm (KHTMLNode Parent, Settings& Bound)
//-----------------------------------------------------------------------------
{
	auto Form = Parent.Add<html::Form>("/settings");
	Form.SetMethod(html::Form::POST);

	// ---- insert the inputs of the settings here ----

	auto Group = Form.Add<html::FieldSet>("display");
	Group.Add<html::TextInput>(Bound.sGreeting, "greeting").SetLabelBefore("greeting ").SetSize(30);
	Group.Add<html::Break>();
	Group.Add<html::CheckBox>(Bound.bShowTime, "showtime").SetLabelAfter(" show the time");

	Form.Add<html::Button>("save");

} // AddSettingsForm

//-----------------------------------------------------------------------------
void __ProjectName__::ApiHome (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	Settings Current;
	{
		std::lock_guard<std::mutex> Lock(m_SettingsMutex);
		Current = m_Settings;
	}

	html::Page Page(s_sProjectName, "en");
	auto Main = BeginPage(Page);

	auto Card = Main.Add<html::ui::Card>(Current.sGreeting);
	auto Body = Card.Body();

	Body.Add<html::Paragraph>().AddText(kFormat("{} v{}", s_sProjectName, s_sProjectVersion));

	if (Current.bShowTime)
	{
		Body.Add<html::Paragraph>().AddText(kFormat("the time is {:%Y-%m-%d %H:%M:%S} UTC", KUTCTime::now()));
	}

	SendPage(HTTP, Page);

} // ApiHome

//-----------------------------------------------------------------------------
void __ProjectName__::ApiSettings (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	Settings Bound;
	{
		std::lock_guard<std::mutex> Lock(m_SettingsMutex);
		Bound = m_Settings;
	}

	html::Page Page("Settings", "en");
	auto Main = BeginPage(Page);

	auto Card = Main.Add<html::ui::Card>("Settings");
	AddSettingsForm(Card.Body(), Bound);

	SendPage(HTTP, Page);

} // ApiSettings

//-----------------------------------------------------------------------------
void __ProjectName__::ApiSaveSettings (KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// the browser sends the credentials with every request - refuse a form of another site
	const auto& sOrigin = HTTP.Request.Headers.Get(KHTTPHeader::ORIGIN);

	if (!sOrigin.empty() && !sOrigin.ends_with(kFormat("//{}", HTTP.Request.Headers.Get(KHTTPHeader::HOST))))
	{
		throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, "the form was sent from another site" };
	}

	Settings Bound;
	{
		std::lock_guard<std::mutex> Lock(m_SettingsMutex);
		Bound = m_Settings;
	}

	// the same form as on the settings page, so that the submitted fields find their inputs
	html::Page Page("Settings", "en");
	AddSettingsForm(Page.Body(), Bound);
	Page.Generate();
	Page.Synchronize(HTTP.GetQueryParms());

	{
		std::lock_guard<std::mutex> Lock(m_SettingsMutex);
		m_Settings = std::move(Bound);
	}

	// back to the home page
	HTTP.SetStatus(303);
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, "/");

} // ApiSaveSettings

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	KString sUser;
	KString sPasswordHash;

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

		m_ServerOptions.iPort                = Options("http <port>          : port number (default 8080)", 8080).Range(1, 65535);
		m_ServerOptions.sBindAddress         = Options("bind <address>       : bind the server to this address only, e.g. 127.0.0.1, default all interfaces", "");
		m_ServerOptions.iMaxConnections      = Options("n <max>              : max parallel connections (default 50)", m_ServerOptions.iMaxConnections).Range(1, 65535);
		m_ServerOptions.Timeout              = chrono::seconds(Options("timeout <seconds> : server timeout (default 5)", 5).Range(1, 60*60).UInt16());
		sUser                                = Options("user <name>          : ask for this account with basic authentication, default none", "");
		KStringViewZ sPasswordFile           = Options("password-file <path> : file with the password of the account in its first line", "").Type(KOptions::File);

		Options.Section("TLS options:");

		m_ServerOptions.sCert                = Options("cert <file>          : TLS certificate filepath (.pem), defaults to a self-signed ephemeral certificate", "").Type(KOptions::File);
		m_ServerOptions.sKey                 = Options("key <file>           : TLS private key filepath (.pem), defaults to an ephemeral key", "").Type(KOptions::File);
		m_ServerOptions.bCreateEphemeralCert =!Options("notls                : do NOT switch to TLS mode if cert and key were not provided", false);
		m_ServerOptions.bStoreEphemeralCert  = Options("persist              : persist a self-signed certificate on disk and reuse it at the next start", false);

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

		if (!sUser.empty())
		{
			if (sPasswordFile.empty())
			{
				throw KException("user needs password-file");
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

			// keep only the hash
			sPasswordHash = KBCrypt().GenerateHash(sPassword);
			kSafeErase(sPassword);
		}
	}

	m_ServerOptions.Type      = KREST::HTTP;
	m_ServerOptions.bBlocking = true;
	KLog::getInstance().SetMode(KLog::SERVER);

	if (!sUser.empty())
	{
		// every request needs the account, checked with bcrypt
		m_ServerOptions.PreRouteCallback = [sUser, sPasswordHash](KRESTServer& HTTP)
		{
			auto Basic = HTTP.Request.GetBasicAuthParms();

			if (Basic.sUsername != sUser || !KBCrypt().ValidatePassword(Basic.sPassword, sPasswordHash))
			{
				HTTP.Response.Headers.Set(KHTTPHeader::WWW_AUTHENTICATE, kFormat("Basic realm=\"{}\"", s_sProjectName));
				throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "not authorized" };
			}

			HTTP.SetAuthenticatedUser(Basic.sUsername);
		};
	}

	KRESTRoutes Routes;

	// the root is the empty route
	Routes.AddRoute("").Get([this](KRESTServer& HTTP) { ApiHome(HTTP); }).Parse(KRESTRoute::NOREAD);
	Routes.AddRoute("/settings").Get ([this](KRESTServer& HTTP) { ApiSettings    (HTTP); }).Parse(KRESTRoute::NOREAD);
	// the form fields arrive as query parameters
	Routes.AddRoute("/settings").Post([this](KRESTServer& HTTP) { ApiSaveSettings(HTTP); }).Parse(KRESTRoute::WWWFORM);

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
