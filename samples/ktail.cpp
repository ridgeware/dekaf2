// ktail.cpp
//
// see ktail.h

#include "ktail.h"

#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/crypto/auth/kbcrypt.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/web/url/kurlencode.h>
#include <dekaf2/http/protocol/khttp_header.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/rest/framework/krestsession.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/time/clock/ktime.h>
#include <fstream>
#include <thread>

namespace {

constexpr KStringView sTailPath  = "/tail";
constexpr KStringView sNotesPath = "/notes";

// the texts of the user interface, in one place - the page script gets them as
// a JSON object, and a translation would replace this table
constexpr KStringView sTexts = R"json({
	"name"          : "Name",
	"size"          : "Size",
	"modified"      : "Modified",
	"pickFile"      : "Select a file on the left to follow it live.",
	"reveal"        : "Show in file manager",
	"notifyOnLines" : "Notify on new lines",
	"note"          : "Note for this file",
	"saveNote"      : "Save note",
	"noteSaved"     : "Note saved",
	"theme"         : "Theme",
	"themeAuto"     : "Auto",
	"themeLight"    : "Light",
	"themeDark"     : "Dark",
	"connected"     : "Connected",
	"disconnected"  : "Disconnected",
	"truncated"     : "File was truncated, starting over",
	"gone"          : "File is gone",
	"newLines"      : "New lines",
	"signOut"       : "Sign out",
	"noteUpdated"   : "Note updated"
})json";

// colors as tokens, the dark set both for the system preference and for the
// explicit choice, so that the page's toggle wins in both directions
constexpr KStringView sStyle = R"css(
:root { color-scheme: light dark;
	--bg: #ffffff; --fg: #1d1d1f; --muted: #6e6e73; --panel: #f5f5f7; --border: #d2d2d7; --accent: #0a66c2;
	--mono: ui-monospace, Menlo, Consolas, monospace; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) {
	--bg: #1c1c1e; --fg: #f5f5f7; --muted: #98989d; --panel: #2c2c2e; --border: #3a3a3c; --accent: #4c9aff; } }
:root[data-theme="dark"] {
	--bg: #1c1c1e; --fg: #f5f5f7; --muted: #98989d; --panel: #2c2c2e; --border: #3a3a3c; --accent: #4c9aff; }
body { margin: 0; height: 100vh; display: flex; flex-direction: column; background: var(--bg); color: var(--fg);
	font: 14px/1.4 -apple-system, "Segoe UI", Helvetica, sans-serif; }
header { display: flex; align-items: center; gap: 1em; padding: .6em 1em; border-bottom: 1px solid var(--border); background: var(--panel); }
header h1 { font-size: 1.1em; margin: 0; }
header .path { flex: 1; color: var(--muted); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
header .path a { color: var(--accent); text-decoration: none; }
button { font: inherit; padding: .3em .8em; border: 1px solid var(--border); border-radius: 6px; background: var(--panel); color: var(--fg); cursor: pointer; }
main { flex: 1; min-height: 0; display: grid; grid-template-columns: minmax(16em, 28%) 1fr; }
nav { overflow: auto; border-right: 1px solid var(--border); }
nav table { border-collapse: collapse; width: 100%; }
nav th { text-align: left; color: var(--muted); font-weight: 500; padding: .4em .8em; border-bottom: 1px solid var(--border); }
nav td { padding: .3em .8em; border-bottom: 1px solid var(--border); white-space: nowrap; }
nav td.num { text-align: right; color: var(--muted); font-variant-numeric: tabular-nums; }
nav a { color: var(--fg); text-decoration: none; }
nav a.dir { color: var(--accent); }
nav tr.selected td { background: var(--panel); }
section { display: flex; flex-direction: column; min-width: 0; }
section .bar { display: flex; align-items: center; gap: 1em; padding: .5em 1em; border-bottom: 1px solid var(--border); }
section .bar h2 { flex: 1; font-size: 1em; margin: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
pre#tail { flex: 1; margin: 0; padding: .8em 1em; overflow: auto; font: 12px/1.45 var(--mono); }
form.note { display: flex; gap: .6em; padding: .6em 1em; border-top: 1px solid var(--border); background: var(--panel); }
form.note textarea { flex: 1; font: inherit; background: var(--bg); color: var(--fg); border: 1px solid var(--border); border-radius: 6px; padding: .4em; resize: vertical; }
footer { padding: .3em 1em; color: var(--muted); font-size: .9em; border-top: 1px solid var(--border); }
.hint { padding: 2em 1em; color: var(--muted); }
.native-only { display: none; }
.native .native-only { display: inline-flex; align-items: center; gap: .3em; }
.browser-only { display: none; margin: 0; }
:root:not(.native) .browser-only { display: inline; }
)css";

// the page script: theme toggle, live tail over the websocket, and the native
// extras that only exist when window.kNative is there
constexpr KStringView sScript = R"js(
(() => {
	const D      = document.currentScript.dataset;
	const T      = JSON.parse(D.texts);
	const root   = document.documentElement;
	const native = typeof window.kNative !== 'undefined';
	if (native) root.classList.add('native');

	// theme: auto, light, dark - remembered per browser
	const themes = ['auto', 'light', 'dark'];
	const labels = { auto: T.themeAuto, light: T.themeLight, dark: T.themeDark };
	const button = document.getElementById('theme');
	let   theme  = localStorage.getItem('ktail.theme') || 'auto';
	const apply  = () => {
		if (theme === 'auto') delete root.dataset.theme; else root.dataset.theme = theme;
		button.textContent = T.theme + ': ' + labels[theme];
	};
	button.addEventListener('click', () => {
		theme = themes[(themes.indexOf(theme) + 1) % themes.length];
		localStorage.setItem('ktail.theme', theme);
		apply();
	});
	apply();

	// the window's menu entries arrive as events
	window.addEventListener('kwa-menu', (ev) => {
		switch (ev.detail) {
			case 'toggleTheme': button.click(); break;
			case 'saveNote':    { const f = document.querySelector('form.note'); if (f) f.requestSubmit(); break; }
			case 'revealFile':  if (D.file && native) window.kNative.reveal({ file: D.file }); break;
		}
	});

	const status = document.getElementById('status');
	if (D.saved === '1') status.textContent = T.noteSaved;
	if (!D.file) return;

	// notes saved in any other view arrive over the application's live connection
	if (window.kLive) kLive.on((m) => {
		if (m.t === 'note' && m.file === D.file) {
			document.getElementById('note').value = m.text;
			status.textContent = T.noteUpdated + (m.by ? ' (' + m.by + ')' : '');
		}
	});

	const pre    = document.getElementById('tail');
	const notify = document.getElementById('notify');
	const reveal = document.getElementById('reveal');
	if (reveal) reveal.addEventListener('click', () => window.kNative.reveal({ file: D.file }));

	let lines = 0, lastNotify = 0;
	const append = (list) => {
		const atEnd = pre.scrollHeight - pre.scrollTop - pre.clientHeight < 40;
		pre.textContent += list.join('\n') + '\n';
		lines += list.length;
		if (lines > 5000) {
			const keep = pre.textContent.split('\n');
			pre.textContent = keep.slice(keep.length - 2500).join('\n');
			lines = 2500;
		}
		if (atEnd) pre.scrollTop = pre.scrollHeight;
	};

	const connect = () => {
		const ws = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host
		                         + D.tailPath + '?file=' + encodeURIComponent(D.file));
		ws.onopen    = () => status.textContent = T.connected;
		ws.onclose   = () => { status.textContent = T.disconnected; setTimeout(connect, 2000); };
		ws.onmessage = (ev) => {
			const m = JSON.parse(ev.data);
			if (m.t === 'lines') {
				append(m.lines);
				if (m.live && native && notify && notify.checked && document.hidden && Date.now() - lastNotify > 5000) {
					lastNotify = Date.now();
					window.kNative.notify({ title: T.newLines + ': ' + D.file, body: m.lines[m.lines.length - 1] });
				}
			}
			else if (m.t === 'truncated') { pre.textContent = ''; lines = 0; status.textContent = T.truncated; }
			else if (m.t === 'gone')      { status.textContent = T.gone; }
		};
	};
	connect();
})();
)js";

//-----------------------------------------------------------------------------
// the texts, parsed once
const KJSON& Texts()
//-----------------------------------------------------------------------------
{
	static const KJSON s_jTexts = kjson::Parse(sTexts);
	return s_jTexts;

} // Texts

//-----------------------------------------------------------------------------
// a text by its key
KStringView Text(KStringView sKey)
//-----------------------------------------------------------------------------
{
	return Texts()[sKey].String();

} // Text

//-----------------------------------------------------------------------------
// a query string for the page: dir and optional file, both relative to the root
KString PageLink(KStringView sDir, KStringView sFile = KStringView{})
//-----------------------------------------------------------------------------
{
	KString sLink = "/?dir=";
	kUrlEncode(sDir, sLink, URIPart::Query);

	if (!sFile.empty())
	{
		sLink += "&file=";
		kUrlEncode(sFile, sLink, URIPart::Query);
	}

	return sLink;

} // PageLink

//-----------------------------------------------------------------------------
// the parent of a relative path, empty for the root
KStringView Parent(KStringView sRelative)
//-----------------------------------------------------------------------------
{
	auto iSlash = sRelative.rfind('/');
	return iSlash == KStringView::npos ? KStringView{} : sRelative.substr(0, iSlash);

} // Parent

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KTail::KTail(Config Config)
//-----------------------------------------------------------------------------
: m_Config(std::move(Config))
{
	m_Config.sRoot = kNormalizePath(m_Config.sRoot.empty() ? kGetCWD() : m_Config.sRoot);

	if (!kDirExists(m_Config.sRoot))
	{
		SetError(kFormat("not a directory: {}", m_Config.sRoot));
		return;
	}

	m_sNotesDir = kFormat("{}/.config/ktail/notes", kGetHome());

	if (!kCreateDir(m_sNotesDir))
	{
		SetError(kFormat("cannot create {}", m_sNotesDir));
		return;
	}

	// the page, the note form, and the live tail - the root is the empty route
	m_Routes.AddRoute("").Get([this](KRESTServer& HTTP) { Page(HTTP); });
	m_Routes.AddRoute(KString(sNotesPath)).Post([this](KRESTServer& HTTP) { SaveNote(HTTP); }).Parse(KRESTRoute::WWWFORM);
	m_Routes.AddRoute(KString(sTailPath)).Get([this](KRESTServer& HTTP) { Tail(HTTP); }).Parse(KRESTRoute::NOREAD).Options(KRESTRoute::Options::WEBSOCKET);

	KWebApp::Options Options;
	Options.sTitle   = m_Config.sTitle;
	Options.sVersion = m_Config.sVersion;
	Options.bWindow  = m_Config.bWindow;
	Options.bDebug   = m_Config.bDebug;
	Options.iWidth   = 1100;
	Options.iHeight  = 700;
	Options.sAppName = "ktail";
	// the page answers these entries through the "kwa-menu" event
	Options.jMenus   = kjson::Parse(R"json([
		{ "title": "File", "items": [
			{ "title": "Save Note",            "key": "s", "action": "saveNote" },
			{ "separator": true },
			{ "title": "Show in File Manager", "key": "r", "action": "revealFile" }
		] },
		{ "title": "View", "items": [
			{ "title": "Toggle Theme",         "key": "t", "action": "toggleTheme" }
		] }
	])json");

	if (!m_Config.sListen.empty())
	{
		// browsers on the network: TLS, a login, and the server outlives the window
		if (m_Config.sUser.empty() || m_Config.sPasswordFile.empty())
		{
			SetError("the network needs -user and -password-file");
			return;
		}

		KString sPassword;
		{
			KInFile File(m_Config.sPasswordFile);

			if (!File.is_open() || !File.ReadLine(sPassword) || sPassword.Trim().empty())
			{
				SetError(kFormat("cannot read a password from {}", m_Config.sPasswordFile));
				return;
			}
		}

		// keep only the hash, and let bcrypt take its time on every check
		auto BCrypt = std::make_shared<KBCrypt>();
		auto sHash  = std::make_shared<KString>(BCrypt->GenerateHash(sPassword));
		auto sUser  = m_Config.sUser;

		Options.Authenticate = [BCrypt, sHash, sUser](KStringView sName, KStringView sPassword)
		{
			return sName == sUser && BCrypt->ValidatePassword(KString(sPassword), *sHash);
		};

		KStringView sListen = m_Config.sListen;
		auto        iColon  = sListen.rfind(':');

		if (iColon != KStringView::npos)
		{
			Options.Network.sBindAddress = KString(sListen.substr(0, iColon));
			sListen.remove_prefix(iColon + 1);
		}

		Options.Network.iPort = sListen.UInt16();

		if (Options.Network.iPort == 0)
		{
			SetError(kFormat("not a port to listen on: {}", m_Config.sListen));
			return;
		}

		Options.Network.sCert     = m_Config.sCert;
		Options.Network.sKey      = m_Config.sKey;
		Options.bNetwork          = true;
		Options.bQuitOnWindowClose = false;
	}

	m_App = std::make_unique<KWebApp>(std::move(Options), m_Routes);

	if (m_App->HasError())
	{
		SetError(m_App->Error());
		return;
	}

	// what only the desktop can do - notifications and dialogs are KWebApp's own
	m_App->Bind("reveal", [this](const KJSON& jArg) { return Reveal(jArg); });

} // ctor

//-----------------------------------------------------------------------------
int KTail::Run()
//-----------------------------------------------------------------------------
{
	return (m_App && !HasError()) ? m_App->Run() : 1;

} // Run

//-----------------------------------------------------------------------------
bool KTail::Resolve(KStringView sRelative, KString& sAbsolute) const
//-----------------------------------------------------------------------------
{
	// relative to the root, and never above it
	if (sRelative.starts_with('/')
		|| sRelative == ".."
		|| sRelative.starts_with("../")
		|| sRelative.ends_with("/..")
		|| sRelative.contains("/../"))
	{
		return false;
	}

	sAbsolute = m_Config.sRoot;

	if (!sRelative.empty())
	{
		sAbsolute += '/';
		sAbsolute += sRelative;
	}

	sAbsolute = kNormalizePath(sAbsolute);

	return sAbsolute == m_Config.sRoot || sAbsolute.starts_with(kFormat("{}/", m_Config.sRoot));

} // Resolve

//-----------------------------------------------------------------------------
KString KTail::GetNotePath(KStringView sAbsolute) const
//-----------------------------------------------------------------------------
{
	return kFormat("{}/{:016x}.txt", m_sNotesDir, sAbsolute.Hash());

} // GetNotePath

//-----------------------------------------------------------------------------
void KTail::Page(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	auto sDir  = HTTP.GetQueryParm("dir");
	auto sFile = HTTP.GetQueryParm("file");

	KString sAbsDir;

	if (!Resolve(sDir, sAbsDir) || !kDirExists(sAbsDir))
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "no such directory" };
	}

	KString sAbsFile;
	bool    bHaveFile = !sFile.empty() && Resolve(sFile, sAbsFile) && kFileExists(sAbsFile);

	html::Page Page(m_Config.sTitle, "en");
	Page.Head().Add<html::Meta>("viewport", "width=device-width, initial-scale=1");
	Page.AddStyle(sStyle);

	// header: title, the path as links, the theme toggle
	auto Header = Page.Add<html::Element>("header");
	Header.Add<html::Element>("h1").AddText(m_Config.sTitle);

	auto Path = Header.Add<html::Element>("span", "path");
	Path.Add<html::Link>(PageLink(""), m_Config.sRoot);

	{
		KString sWalk;

		for (auto sPart : sDir.Split("/"))
		{
			if (!sWalk.empty()) sWalk += '/';
			sWalk += sPart;
			Path.AddText(" / ");
			Path.Add<html::Link>(PageLink(sWalk), sPart);
		}
	}

	Header.Add<html::Button>(KStringView{}, html::Button::BUTTON, html::Classes{}, "theme");

	// a browser has a session to end, the window has not
	auto SignOut = Header.Add<html::Form>("/logout", "browser-only");
	SignOut.SetMethod(html::Form::POST);
	SignOut.Add<html::Button>(Text("signOut"));

	auto Main = Page.Add<html::Element>("main");

	// left: the directory
	{
		auto Nav   = Main.Add<html::Element>("nav");
		auto Table = Nav.Add<html::Element>("table");
		auto Head  = Table.Add<html::Element>("tr");
		Head.Add<html::TableHeader>(Text("name"));
		Head.Add<html::TableHeader>(Text("size"));
		Head.Add<html::TableHeader>(Text("modified"));

		if (!sDir.empty())
		{
			auto Row = Table.Add<html::Element>("tr");
			Row.Add<html::TableData>().Add<html::Link>(PageLink(Parent(sDir)), "..", "dir");
			Row.Add<html::TableData>();
			Row.Add<html::TableData>();
		}

		KDirectory Dir(sAbsDir);
		Dir.Sort(KDirectory::SortBy::NAME);

		// directories first, then files
		for (auto Type : { KFileType::DIRECTORY, KFileType::FILE })
		{
			for (const auto& Entry : Dir)
			{
				if (Entry.Type() != Type || Entry.Filename().starts_with('.'))
				{
					continue;
				}

				KString sRelative = sDir;
				if (!sRelative.empty()) sRelative += '/';
				sRelative += Entry.Filename();

				bool bIsDir    = Type == KFileType::DIRECTORY;
				bool bSelected = bHaveFile && sRelative == sFile;

				auto Row = Table.Add<html::Element>("tr", bSelected ? html::Classes("selected") : html::Classes{});
				Row.Add<html::TableData>().Add<html::Link>(bIsDir ? PageLink(sRelative) : PageLink(sDir, sRelative),
				                                           Entry.Filename(),
				                                           bIsDir ? html::Classes("dir") : html::Classes{});
				Row.Add<html::TableData>(bIsDir ? KStringView{} : KStringView(kFormBytes(Entry.Size())), "num");
				Row.Add<html::TableData>(kFormTimestamp(KLocalTime(Entry.ModificationTime()), "{:%Y-%m-%d %H:%M}"), "num");
			}
		}
	}

	// right: the tail of the selected file, and its note
	{
		auto Section = Main.Add<html::Element>("section");

		if (bHaveFile)
		{
			auto Bar = Section.Add<html::Element>("div", "bar");
			Bar.Add<html::Element>("h2").AddText(sFile);

			auto Label = Bar.Add<html::Element>("label", "native-only");
			Label.Add<html::Input>("notify", "", html::Input::CHECKBOX, html::Classes{}, "notify");
			Label.AddText(Text("notifyOnLines"));

			Bar.Add<html::Button>(Text("reveal"), html::Button::BUTTON, "native-only", "reveal");

			Section.Add<html::Element>("pre", html::Classes{}, "tail");

			KString sNote;
			{
				KInFile Note(GetNotePath(sAbsFile));
				if (Note.is_open()) Note.ReadRemaining(sNote);
			}

			auto Form = Section.Add<html::Form>(sNotesPath, "note");
			Form.SetMethod(html::Form::POST);
			Form.Add<html::Input>("file", sFile, html::Input::HIDDEN);
			Form.Add<html::TextArea>("text", sNote, html::Classes{}, "note").SetAttribute("rows", "2").SetPlaceholder(Text("note"));
			Form.Add<html::Button>(Text("saveNote"));
		}
		else
		{
			Section.Add<html::Element>("p", "hint").AddText(Text("pickFile"));
		}
	}

	Page.Add<html::Element>("footer", html::Classes{}, "status").AddText(kFormat("{} {}", m_Config.sTitle, m_Config.sVersion));

	// the live connection to the application, then the page script with its parameters as data attributes
	Page.Body().Add<html::Script>().SetAttribute("src", "/_kwa/live.js");
	Page.Body().Add<html::Script>(sScript)
		.SetAttribute("data-texts",    sTexts)
		.SetAttribute("data-tail-path", sTailPath)
		.SetAttribute("data-file",     bHaveFile ? sFile : KString{})
		.SetAttribute("data-saved",    HTTP.GetQueryParm("saved"));

	Page.Generate();

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	HTTP.SetRawOutput(Page.Print());

} // Page

//-----------------------------------------------------------------------------
void KTail::SaveNote(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// the form fields arrive as query parms through the WWWFORM parser
	auto sFile = HTTP.GetQueryParm("file");
	auto sText = HTTP.GetQueryParm("text");

	KString sAbsFile;

	if (!Resolve(sFile, sAbsFile) || !kFileExists(sAbsFile))
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "no such file" };
	}

	if (!kWriteFile(GetNotePath(sAbsFile), sText))
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, "cannot write note" };
	}

	// every view of this file gets the new note, the window and the browsers alike
	KJSON jNote;
	jNote["t"]    = "note";
	jNote["file"] = sFile;
	jNote["text"] = sText;
	jNote["by"]   = m_App->IsFromWindow(HTTP) ? KString("window") : KString(KRESTSession(*m_App->GetSession(), HTTP).GetUser());
	m_App->Broadcast(jNote);

	// back to the page
	HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, kFormat("{}&saved=1", PageLink(Parent(sFile), sFile)));
	throw KHTTPError { KHTTPError::H302_MOVED_TEMPORARILY, "" };

} // SaveNote

//-----------------------------------------------------------------------------
void KTail::Tail(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	auto    sFile = HTTP.GetQueryParm("file");
	KString sAbsFile;

	if (!Resolve(sFile, sAbsFile) || !kFileExists(sAbsFile))
	{
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "no such file" };
	}

	// this connection keeps its thread, the loop below owns it until the page leaves
	HTTP.SetWebSocketHandler([this, sAbsFile](KWebSocket& WebSocket)
	{
		TailLoop(WebSocket, sAbsFile);
	});

	HTTP.SetKeepWebSocketInRunningThread();

} // Tail

//-----------------------------------------------------------------------------
void KTail::TailLoop(KWebSocket& WebSocket, KString sPath)
//-----------------------------------------------------------------------------
{
	// the read timeout is the poll interval: a frame from the page ends the wait
	// early, a timeout looks at the file - and at the application, which cannot
	// stop the server while this thread runs
	WebSocket.SetReadTimeout(chrono::milliseconds(250));

	// start with the last 16 KB, from the first complete line
	std::size_t iOffset   = kFileSize(sPath);
	bool        bMidLine  = iOffset > 16 * 1024;
	iOffset               = bMidLine ? iOffset - 16 * 1024 : 0;
	bool        bLive     = false;
	bool        bGone     = false;
	KString     sPartial;

	auto Poll = [&]() -> bool
	{
		KFileStat Stat(sPath);

		if (!Stat.Exists())
		{
			if (!bGone)
			{
				bGone = true;
				return WebSocket.Write(KJSON { { "t", "gone" } });
			}
			return true;
		}

		bGone = false;
		auto iSize = Stat.Size();

		if (iSize < iOffset)
		{
			// rotated or truncated
			iOffset = 0;
			sPartial.clear();

			if (!WebSocket.Write(KJSON { { "t", "truncated" } }))
			{
				return false;
			}
		}

		if (iSize > iOffset)
		{
			std::ifstream File(sPath.c_str(), std::ios::binary);
			File.seekg(static_cast<std::streamoff>(iOffset));

			KString sData;
			sData.resize(iSize - iOffset);
			File.read(&sData[0], static_cast<std::streamsize>(sData.size()));
			sData.resize(static_cast<std::size_t>(File.gcount()));
			iOffset += sData.size();

			if (bMidLine)
			{
				// drop the partial first line of the initial window
				auto iNL = sData.find('\n');
				sData.erase(0, iNL == KString::npos ? sData.size() : iNL + 1);
				bMidLine = false;
			}

			sPartial += sData;

			KJSON jLines = KJSON::array();
			KStringView sRest = sPartial;

			for (auto iNL = sRest.find('\n'); iNL != KStringView::npos; iNL = sRest.find('\n'))
			{
				auto sLine = sRest.substr(0, iNL);
				if (sLine.ends_with('\r')) sLine.remove_suffix(1);
				jLines.push_back(sLine);
				sRest.remove_prefix(iNL + 1);
			}

			sPartial = sRest;

			if (!jLines.empty())
			{
				KJSON jMessage;
				jMessage["t"]     = "lines";
				jMessage["live"]  = bLive;
				jMessage["lines"] = std::move(jLines);

				if (!WebSocket.Write(jMessage))
				{
					return false;
				}
			}
		}

		bLive = true;
		return true;
	};

	if (!Poll())
	{
		return;
	}

	for (;;)
	{
		if (WebSocket.Read())
		{
			// the page has nothing to tell us yet
			continue;
		}

		switch (WebSocket.GetReadState())
		{
			case KWebSocket::ReadState::Timeout:
				if (m_App->IsQuitting() || !Poll())
				{
					return;
				}
				break;

			case KWebSocket::ReadState::Success:
				break;

			case KWebSocket::ReadState::PeerClose:
			case KWebSocket::ReadState::Error:
				return;
		}
	}

} // TailLoop

//-----------------------------------------------------------------------------
KJSON KTail::Reveal(const KJSON& jArg)
//-----------------------------------------------------------------------------
{
	KString sAbsFile;

	if (!Resolve(jArg["file"].String(), sAbsFile) || !kFileExists(sAbsFile))
	{
		return false;
	}

#if DEKAF2_IS_MACOS
	KString sCommand = kFormat("open -R {}", kEscapeForCommands(sAbsFile));
#elif DEKAF2_IS_WINDOWS
	KString sCommand = kFormat("explorer /select,\"{}\"", sAbsFile);
#else
	KString sCommand = kFormat("xdg-open {}", kEscapeForCommands(kDirname(sAbsFile)));
#endif

	// not on the UI thread
	std::thread([sCommand] { kSystem(sCommand); }).detach();

	return true;

} // Reveal

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// the signal handler thread lets SIGINT and SIGTERM end a headless run
	KInit(true);

	try
	{
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);
		Options.SetBriefDescription("browse a directory and follow files live, in a desktop window or a browser");

		KTail::Config Config;
		Config.sRoot         = Options("dir <path>            : the directory to browse, defaults to the current one", "");
		Config.bDebug        = Options("inspector             : enable the web inspector in the window", false);
		Config.sListen       = Options("listen <[addr:]port>  : also serve browsers on the network, with TLS and a login", "");
		Config.bWindow       = !Options("headless              : no window, only the network server", false);
		Config.sUser         = Options("user <name>           : the account for the network", "");
		Config.sPasswordFile = Options("password-file <path>  : file with the account's password in the first line", "");
		Config.sCert         = Options("cert <file>           : TLS certificate for the network (PEM), default: self-signed", "");
		Config.sKey          = Options("key <file>            : TLS private key for the network (PEM)", "");

		if (Options.Terminate())
		{
			return 0;
		}

		KTail App(std::move(Config));

		if (App.HasError())
		{
			KErr.FormatLine("ktail: {}", App.Error());
			return 1;
		}

		return App.Run();
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine("ktail: {}", ex.what());
	}

	return 1;

} // main
