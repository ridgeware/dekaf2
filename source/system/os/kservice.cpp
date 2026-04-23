/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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
// |/|   distribute, sublicense, and/or sell copies of the Software,       |\|
// |/|   and to permit persons to whom the Software is furnished to        |/|
// |\|   do so, subject to the following conditions:                       |\|
// |/|                                                                     |/|
// |\|   The above copyright notice and this permission notice shall       |\|
// |/|   be included in all copies or substantial portions of the          |/|
// |\|   Software.                                                         |\|
// |/|                                                                     |/|
// |\|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |\|
// |/|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |/|
// |\|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |\|
// |/|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |/|
// |\|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
// |/|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |/|
// |\|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |\|
// |/|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |/|
// |\|                                                                     |\|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#include <dekaf2/system/os/kservice.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/system/os/ksystem.h>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#ifdef DEKAF2_IS_WINDOWS
	#include <dekaf2/core/strings/kutf.h>
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
	#include <winsvc.h>
#elif defined(DEKAF2_IS_LINUX)
	#include <dekaf2/core/strings/kstringutils.h>
	#include <dekaf2/system/filesystem/kfilesystem.h>
	#include <sys/socket.h>
	#include <sys/un.h>
	#include <sys/types.h>
	#include <unistd.h>
	#include <cerrno>
	#include <cstring>
#elif defined(DEKAF2_IS_MACOS)
	#include <dekaf2/core/strings/kstringutils.h>
	#include <dekaf2/data/xml/kxml.h>
	#include <dekaf2/system/filesystem/kfilesystem.h>
	#include <unistd.h>
	#include <sys/types.h>
#endif

DEKAF2_NAMESPACE_BEGIN

namespace {

#ifdef DEKAF2_IS_WINDOWS
/// Argument injected into the registered service's command line. KService::Run
/// uses it as a fast, unambiguous indicator that the binary should engage the
/// SCM dispatcher. It is filtered from argv before the user's main runs.
constexpr KStringView s_sServiceFlag = "--service";
#endif

//-----------------------------------------------------------------------------
/// Process-wide state for the service integration, shared across all
/// platforms. Only one active service per process is supported, so static
/// storage is appropriate. Platform-specific fields are gated by #ifdef so
/// the struct stays small on platforms that do not need them.
struct Context
//-----------------------------------------------------------------------------
{
	// ---- common fields, used on every platform --------------------------
	std::atomic<bool>             bIsService { false };  ///< running under a service manager
	std::function<void()>         fnShutdown;            ///< optional shutdown hook

#ifdef DEKAF2_IS_WINDOWS
	// ---- Windows SCM state ---------------------------------------------
	std::wstring                  wsServiceName;
	SERVICE_STATUS_HANDLE         StatusHandle = nullptr;
	SERVICE_STATUS                Status       = {};
	KService::MainFunc            fnMain;
	std::vector<char*>            FilteredArgv;          // trailing nullptr for argv parity
	int                           iExitCode    = 0;
	DWORD                         dwCheckPoint = 0;
#endif

#ifdef DEKAF2_IS_LINUX
	// ---- systemd state --------------------------------------------------
	KString                       sNotifySocket;         ///< cached $NOTIFY_SOCKET
#endif

	// macOS launchd has no additional state beyond the common fields.

}; // Context

//-----------------------------------------------------------------------------
Context& ctx()
//-----------------------------------------------------------------------------
{
	static Context s_ctx;
	return s_ctx;

} // ctx

#ifdef DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
DWORD ToWinState(KService::State state)
//-----------------------------------------------------------------------------
{
	switch (state)
	{
		case KService::State::Stopped:         return SERVICE_STOPPED;
		case KService::State::StartPending:    return SERVICE_START_PENDING;
		case KService::State::StopPending:     return SERVICE_STOP_PENDING;
		case KService::State::Running:         return SERVICE_RUNNING;
		case KService::State::ContinuePending: return SERVICE_CONTINUE_PENDING;
		case KService::State::PausePending:    return SERVICE_PAUSE_PENDING;
		case KService::State::Paused:          return SERVICE_PAUSED;
	}
	return SERVICE_STOPPED;

} // ToWinState

//-----------------------------------------------------------------------------
KService::State FromWinState(DWORD dw)
//-----------------------------------------------------------------------------
{
	switch (dw)
	{
		case SERVICE_STOPPED:          return KService::State::Stopped;
		case SERVICE_START_PENDING:    return KService::State::StartPending;
		case SERVICE_STOP_PENDING:     return KService::State::StopPending;
		case SERVICE_RUNNING:          return KService::State::Running;
		case SERVICE_CONTINUE_PENDING: return KService::State::ContinuePending;
		case SERVICE_PAUSE_PENDING:    return KService::State::PausePending;
		case SERVICE_PAUSED:           return KService::State::Paused;
	}
	return KService::State::Stopped;

} // FromWinState

//-----------------------------------------------------------------------------
void ReportStateImpl(KService::State state, uint32_t iWaitHintMs)
//-----------------------------------------------------------------------------
{
	auto& c = ctx();

	if (!c.StatusHandle)
	{
		return;
	}

	c.Status.dwCurrentState            = ToWinState(state);
	c.Status.dwWaitHint                = iWaitHintMs;
	c.Status.dwWin32ExitCode           = (state == KService::State::Stopped && c.iExitCode != 0)
	                                   ? ERROR_SERVICE_SPECIFIC_ERROR : NO_ERROR;
	c.Status.dwServiceSpecificExitCode = static_cast<DWORD>(c.iExitCode);

	if (state == KService::State::StartPending
	 || state == KService::State::StopPending
	 || state == KService::State::ContinuePending
	 || state == KService::State::PausePending)
	{
		c.Status.dwCheckPoint = ++c.dwCheckPoint;
	}
	else
	{
		c.Status.dwCheckPoint = 0;
		c.dwCheckPoint        = 0;
	}

	c.Status.dwControlsAccepted = (state == KService::State::Running)
	                            ? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN)
	                            : 0;

	::SetServiceStatus(c.StatusHandle, &c.Status);

} // ReportStateImpl

//-----------------------------------------------------------------------------
DWORD WINAPI ControlHandler(DWORD dwControl, DWORD /*dwEventType*/, LPVOID /*lpEventData*/, LPVOID /*lpContext*/)
//-----------------------------------------------------------------------------
{
	switch (dwControl)
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			ReportStateImpl(KService::State::StopPending, 30000);

			if (ctx().fnShutdown)
			{
				try
				{
					ctx().fnShutdown();
				}
				catch (...)
				{
					// never let an exception escape the SCM control thread
				}
			}
			else
			{
				// fall back to the standard signal path so KSignals users
				// transparently receive the stop request
				::raise(SIGTERM);
			}
			return NO_ERROR;

		case SERVICE_CONTROL_INTERROGATE:
			::SetServiceStatus(ctx().StatusHandle, &ctx().Status);
			return NO_ERROR;
	}
	return ERROR_CALL_NOT_IMPLEMENTED;

} // ControlHandler

//-----------------------------------------------------------------------------
void WINAPI ServiceMainW(DWORD /*dwArgc*/, LPWSTR* /*lpszArgv*/)
//-----------------------------------------------------------------------------
{
	auto& c = ctx();

	c.Status.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
	c.Status.dwCurrentState            = SERVICE_START_PENDING;
	c.Status.dwControlsAccepted        = 0;
	c.Status.dwWin32ExitCode           = NO_ERROR;
	c.Status.dwServiceSpecificExitCode = 0;
	c.Status.dwCheckPoint              = 0;
	c.Status.dwWaitHint                = 0;

	c.StatusHandle = ::RegisterServiceCtrlHandlerExW(c.wsServiceName.c_str(), ControlHandler, nullptr);

	if (!c.StatusHandle)
	{
		// cannot communicate with SCM; exit quietly — SCM will mark as failed
		return;
	}

	ReportStateImpl(KService::State::StartPending, 30000);
	ReportStateImpl(KService::State::Running,      0);

	try
	{
		// FilteredArgv has a trailing nullptr; argc excludes it
		const int iFilteredArgc = static_cast<int>(c.FilteredArgv.size()) - 1;
		c.iExitCode             = c.fnMain(iFilteredArgc, c.FilteredArgv.data());
	}
	catch (const std::exception& ex)
	{
		kException(ex);
		c.iExitCode = 1;
	}
	catch (...)
	{
		c.iExitCode = 1;
	}

	ReportStateImpl(KService::State::Stopped, 0);

} // ServiceMainW

//-----------------------------------------------------------------------------
/// Converts a Win32 error code into a human readable string like
/// "The service is not started. (1062)".
KString WinErrorText(DWORD err)
//-----------------------------------------------------------------------------
{
	LPWSTR pBuf = nullptr;
	DWORD  n    = ::FormatMessageW(
	    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	    nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	    reinterpret_cast<LPWSTR>(&pBuf), 0, nullptr);

	if (!n || !pBuf)
	{
		return kFormat("error {}", err);
	}

	std::wstring ws(pBuf, n);
	::LocalFree(pBuf);

	while (!ws.empty() && (ws.back() == L'\r' || ws.back() == L'\n' || ws.back() == L'.' || ws.back() == L' '))
	{
		ws.pop_back();
	}

	return kFormat("{} ({})", kutf::Convert<KString>(ws), err);

} // WinErrorText

//-----------------------------------------------------------------------------
std::vector<char*> FilterServiceFlag(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	std::vector<char*> result;
	result.reserve(static_cast<size_t>(argc) + 1);

	for (int i = 0; i < argc; ++i)
	{
		if (!argv[i] || KStringView(argv[i]) != s_sServiceFlag)
		{
			result.push_back(argv[i]);
		}
	}

	result.push_back(nullptr);
	return result;

} // FilterServiceFlag

//-----------------------------------------------------------------------------
bool HasServiceFlag(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	for (int i = 1; i < argc; ++i)
	{
		if (argv[i] && KStringView(argv[i]) == s_sServiceFlag)
		{
			return true;
		}
	}
	return false;

} // HasServiceFlag

//-----------------------------------------------------------------------------
bool HasInteractiveConsole()
//-----------------------------------------------------------------------------
{
	// services run in session 0 without a console window; interactive sessions
	// launched from cmd.exe, PowerShell or Explorer have one
	return ::GetConsoleWindow() != nullptr;

} // HasInteractiveConsole

//-----------------------------------------------------------------------------
bool OpenServiceByName(KStringView sServiceName, DWORD dwAccess,
                       SC_HANDLE& scmOut, SC_HANDLE& svcOut)
//-----------------------------------------------------------------------------
{
	scmOut = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);

	if (!scmOut)
	{
		kWarning("OpenSCManager failed: {}", WinErrorText(::GetLastError()));
		return false;
	}

	std::wstring wsName = kutf::Convert<std::wstring>(sServiceName);
	svcOut              = ::OpenServiceW(scmOut, wsName.c_str(), dwAccess);

	if (!svcOut)
	{
		::CloseServiceHandle(scmOut);
		scmOut = nullptr;
		return false;
	}

	return true;

} // OpenServiceByName

#endif // DEKAF2_IS_WINDOWS

#if defined(DEKAF2_IS_LINUX) || defined(DEKAF2_IS_MACOS)

//-----------------------------------------------------------------------------
/// Trampoline for std::signal; forwards SIGTERM (or SIGINT) into the user's
/// shutdown handler stored in ctx(). Must be async-signal-safe, so we do
/// nothing more than invoking the std::function if one is registered.
///
/// systemd sends SIGTERM on `systemctl stop`; launchd sends SIGTERM on
/// `launchctl unload` / `launchctl stop`. The trampoline is identical for
/// both platforms, hence shared.
void PosixSignalTrampoline(int /*iSignal*/)
//-----------------------------------------------------------------------------
{
	auto& h = ctx().fnShutdown;

	if (h)
	{
		try
		{
			h();
		}
		catch (...)
		{
			// cannot let exceptions escape a signal handler
		}
	}

} // PosixSignalTrampoline

//-----------------------------------------------------------------------------
/// Double-quote a value so it is safe to pass through /bin/sh. Uses dekaf2's
/// kEscapeForQuotedCommands() to backslash-escape shell metacharacters that
/// survive inside double quotes (", \, `, $, NUL). Used for both
/// systemctl (Linux) and launchctl (macOS) invocations.
KString QuoteForShell(KStringView sValue)
//-----------------------------------------------------------------------------
{
	return kFormat("\"{}\"", kEscapeForQuotedCommands(sValue));

} // QuoteForShell

#endif // DEKAF2_IS_LINUX || DEKAF2_IS_MACOS

#ifdef DEKAF2_IS_LINUX

//-----------------------------------------------------------------------------
/// Send a single notification message to systemd via the NOTIFY_SOCKET, using
/// the sd_notify wire format (key=value lines, NUL-free payload). Silently
/// succeeds as a no-op if NOTIFY_SOCKET was not set. Implemented inline to
/// avoid a dependency on libsystemd.
bool SdNotify(KStringView sMessage)
//-----------------------------------------------------------------------------
{
	const auto& sSocket = ctx().sNotifySocket;

	if (sSocket.empty() || sMessage.empty())
	{
		return false;
	}

	int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);

	if (fd < 0)
	{
		kDebug(1, "sd_notify: socket() failed: {}", std::strerror(errno));
		return false;
	}

	sockaddr_un addr = {};
	addr.sun_family  = AF_UNIX;

	// NOTIFY_SOCKET begins with either '/' (filesystem path) or '@' (abstract
	// Linux socket namespace). For the abstract form, the first byte of
	// sun_path must be a NUL byte followed by the rest of the name.
	const std::size_t iMaxPath = sizeof(addr.sun_path);
	std::size_t       iLen     = sSocket.size();

	if (iLen >= iMaxPath)
	{
		iLen = iMaxPath - 1;
	}

	std::memcpy(addr.sun_path, sSocket.data(), iLen);
	socklen_t iAddrLen = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + iLen);

	if (addr.sun_path[0] == '@')
	{
		addr.sun_path[0] = '\0';  // switch to abstract namespace
	}
	else
	{
		// for the filesystem variant, sun_path must be NUL-terminated and
		// iAddrLen must include that terminator
		if (iLen < iMaxPath)
		{
			addr.sun_path[iLen] = '\0';
			iAddrLen            = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + iLen + 1);
		}
	}

	ssize_t n = ::sendto(fd, sMessage.data(), sMessage.size(), MSG_NOSIGNAL,
	                     reinterpret_cast<sockaddr*>(&addr), iAddrLen);
	::close(fd);

	if (n < 0)
	{
		kDebug(1, "sd_notify: sendto() failed: {}", std::strerror(errno));
		return false;
	}

	return true;

} // SdNotify

//-----------------------------------------------------------------------------
/// Filesystem path of the systemd unit file for sServiceName under the
/// requested scope. Returns an empty string for user scope when $HOME cannot
/// be resolved.
KString UnitFilePath(KStringView sServiceName, bool bUserScope)
//-----------------------------------------------------------------------------
{
	if (bUserScope)
	{
		KString sBase = kGetEnv("XDG_CONFIG_HOME");

		if (sBase.empty())
		{
			sBase = kGetHome();

			if (sBase.empty())
			{
				return {};
			}
			sBase += "/.config";
		}

		return kFormat("{}/systemd/user/{}.service", sBase, sServiceName);
	}

	return kFormat("/etc/systemd/system/{}.service", sServiceName);

} // UnitFilePath

//-----------------------------------------------------------------------------
/// Returns the scope of a previously-installed service by probing both the
/// system and the user unit-file locations. *pUserScope is set to the
/// detected scope (unchanged if nothing was found).
bool DetectInstalledScope(KStringView sServiceName, bool* pUserScope)
//-----------------------------------------------------------------------------
{
	if (kFileExists(UnitFilePath(sServiceName, false)))
	{
		if (pUserScope) *pUserScope = false;
		return true;
	}

	KString sUserPath = UnitFilePath(sServiceName, true);

	if (!sUserPath.empty() && kFileExists(sUserPath))
	{
		if (pUserScope) *pUserScope = true;
		return true;
	}

	return false;

} // DetectInstalledScope

//-----------------------------------------------------------------------------
/// Run `systemctl [--user] <args>` synchronously, return its exit code.
int Systemctl(bool bUserScope, KStringView sArgs)
//-----------------------------------------------------------------------------
{
	KString sCmd = "systemctl";

	if (bUserScope)
	{
		sCmd += " --user";
	}

	sCmd += ' ';
	sCmd += sArgs;

	return kSystem(sCmd);

} // Systemctl

//-----------------------------------------------------------------------------
/// Run `systemctl [--user] <args>` synchronously, capture its stdout into
/// sOutputOut. Trailing whitespace is stripped so callers can compare against
/// "active", "inactive", etc.
int SystemctlCapture(bool bUserScope, KStringView sArgs, KString& sOutputOut)
//-----------------------------------------------------------------------------
{
	KString sCmd = "systemctl";

	if (bUserScope)
	{
		sCmd += " --user";
	}

	sCmd += ' ';
	sCmd += sArgs;

	sOutputOut.clear();
	int iExit = kSystem(sCmd, sOutputOut);
	sOutputOut.TrimRight();
	return iExit;

} // SystemctlCapture

//-----------------------------------------------------------------------------
/// Render the contents of the systemd unit file for the given install
/// parameters.
KString BuildUnitFile(KStringView sServiceName, const KService::InstallOptions& Opts)
//-----------------------------------------------------------------------------
{
	KStringView sDescription = !Opts.sDescription.empty() ? KStringView(Opts.sDescription)
	                         : !Opts.sDisplayName.empty() ? KStringView(Opts.sDisplayName)
	                         :                              sServiceName;

	KString sBinary = Opts.sBinaryPath.empty() ? kGetOwnPathname() : Opts.sBinaryPath;

	KString sExecStart;

	if (sBinary.find(' ') != KString::npos)
	{
		sExecStart = kFormat("\"{}\"", sBinary);
	}
	else
	{
		sExecStart = sBinary;
	}

	if (!Opts.sArguments.empty())
	{
		sExecStart += ' ';
		sExecStart += Opts.sArguments;
	}

	KString s;
	s += "# generated by dekaf2 KService — do not edit by hand\n";
	s += "[Unit]\n";
	s += kFormat("Description={}\n", sDescription);
	s += "After=network.target\n";
	s += "\n";

	s += "[Service]\n";
	// Type=notify so systemd waits for the READY=1 notification that
	// KService::Run() sends once fnMain is entered. NotifyAccess=main is the
	// default but set it explicitly for clarity.
	s += "Type=notify\n";
	s += "NotifyAccess=main\n";
	s += kFormat("ExecStart={}\n", sExecStart);

	if (!Opts.sRunAsUser.empty())
	{
		// accept either "user" or "user:group"
		auto iColon = Opts.sRunAsUser.find(':');

		if (iColon == KString::npos)
		{
			s += kFormat("User={}\n", Opts.sRunAsUser);
		}
		else
		{
			s += kFormat("User={}\n",  Opts.sRunAsUser.substr(0, iColon));
			s += kFormat("Group={}\n", Opts.sRunAsUser.substr(iColon + 1));
		}
	}

	s += "Restart=on-failure\n";
	s += "RestartSec=5\n";
	s += "\n";

	s += "[Install]\n";
	// user-scope units default to default.target; system units to
	// multi-user.target (non-graphical boot), which is what servers want
	s += Opts.bUserScope ? "WantedBy=default.target\n"
	                     : "WantedBy=multi-user.target\n";

	return s;

} // BuildUnitFile

#endif // DEKAF2_IS_LINUX

#ifdef DEKAF2_IS_MACOS

//-----------------------------------------------------------------------------
/// Apply Apple's reverse-DNS label convention. If sServiceName already
/// contains a dot it is assumed to be a fully-qualified label and is passed
/// through unchanged; otherwise it is prefixed with "org.dekaf2." so it is
/// globally unique without the caller needing to care.
KString MakeLabel(KStringView sServiceName)
//-----------------------------------------------------------------------------
{
	if (sServiceName.find('.') != KStringView::npos)
	{
		return KString(sServiceName);
	}
	return kFormat("org.dekaf2.{}", sServiceName);

} // MakeLabel

//-----------------------------------------------------------------------------
/// Filesystem path of the launchd plist for sLabel under the requested scope.
/// Returns an empty string for user scope when $HOME cannot be resolved.
KString PlistPath(KStringView sLabel, bool bUserScope)
//-----------------------------------------------------------------------------
{
	if (bUserScope)
	{
		KString sHome = kGetHome();

		if (sHome.empty())
		{
			return {};
		}
		return kFormat("{}/Library/LaunchAgents/{}.plist", sHome, sLabel);
	}

	return kFormat("/Library/LaunchDaemons/{}.plist", sLabel);

} // PlistPath

//-----------------------------------------------------------------------------
/// Stdout / stderr log paths used in the plist. First = stdout, second = stderr.
/// Empty pair if user-scope but $HOME cannot be resolved.
std::pair<KString, KString> LogPaths(KStringView sLabel, bool bUserScope)
//-----------------------------------------------------------------------------
{
	if (bUserScope)
	{
		KString sHome = kGetHome();

		if (sHome.empty())
		{
			return {};
		}
		return { kFormat("{}/Library/Logs/{}.out", sHome, sLabel),
		         kFormat("{}/Library/Logs/{}.err", sHome, sLabel) };
	}

	return { kFormat("/var/log/{}.out", sLabel),
	         kFormat("/var/log/{}.err", sLabel) };

} // LogPaths

//-----------------------------------------------------------------------------
/// Determine which scope holds the installed plist for sLabel by probing both
/// well-known locations. *pUserScope is set to the detected scope (unchanged
/// if nothing was found).
bool DetectInstalledScope(KStringView sLabel, bool* pUserScope)
//-----------------------------------------------------------------------------
{
	if (kFileExists(PlistPath(sLabel, false)))
	{
		if (pUserScope) *pUserScope = false;
		return true;
	}

	KString sUserPath = PlistPath(sLabel, true);

	if (!sUserPath.empty() && kFileExists(sUserPath))
	{
		if (pUserScope) *pUserScope = true;
		return true;
	}

	return false;

} // DetectInstalledScope

//-----------------------------------------------------------------------------
/// Run `launchctl <args>` synchronously, return its exit code.
int Launchctl(KStringView sArgs)
//-----------------------------------------------------------------------------
{
	return kSystem(kFormat("launchctl {}", sArgs));

} // Launchctl

//-----------------------------------------------------------------------------
/// Run `launchctl <args>` synchronously, capture its stdout. Trailing
/// whitespace is stripped.
int LaunchctlCapture(KStringView sArgs, KString& sOutputOut)
//-----------------------------------------------------------------------------
{
	sOutputOut.clear();
	int iExit = kSystem(kFormat("launchctl {}", sArgs), sOutputOut);
	sOutputOut.TrimRight();
	return iExit;

} // LaunchctlCapture

//-----------------------------------------------------------------------------
/// Append one `<key>K</key><string>V</string>` pair to a plist dict node.
/// KXML takes care of XML escaping for both the key and the value.
void AddStringPair(KXMLNode& Dict, KStringView sKey, KStringView sValue)
//-----------------------------------------------------------------------------
{
	Dict.AddNode("key").SetValue(sKey);
	Dict.AddNode("string").SetValue(sValue);

} // AddStringPair

//-----------------------------------------------------------------------------
/// Render the contents of the launchd plist for the given install parameters.
/// The body, the XML declaration and the DOCTYPE are all produced by KXML
/// (escaping + serialisation handled by the DOM). Only the free-form header
/// comment is inserted manually, since KXML does not expose a comment-node API.
KString BuildPlist(KStringView sLabel, const KService::InstallOptions& Opts, bool bUserScope)
//-----------------------------------------------------------------------------
{
	KString sBinary = Opts.sBinaryPath.empty() ? kGetOwnPathname() : Opts.sBinaryPath;

	KXML Xml;

	auto Plist = KXMLNode(Xml).AddNode("plist");
	Plist.AddAttribute("version", "1.0");

	auto Dict = Plist.AddNode("dict");

	AddStringPair(Dict, "Label", sLabel);

	// ProgramArguments = argv[0..n]. We split Opts.sArguments on runs of
	// whitespace (bCombineDelimiters=true) because launchd treats each
	// array entry as one argv element — no shell re-parsing happens, so we
	// must produce the final argv layout ourselves. Users who need spaces
	// inside a single argument must quote them (bRespectQuotes=true).
	Dict.AddNode("key").SetValue("ProgramArguments");
	auto Args = Dict.AddNode("array");
	Args.AddNode("string").SetValue(sBinary);

	for (auto sArg : KStringView(Opts.sArguments).Split(
	                     " \t",                   // delimiters: spaces + tabs
	                     " \t",                   // trim same set
	                     '\0',                    // no escape char
	                     /*bCombineDelimiters=*/true,
	                     /*bRespectQuotes=*/true))
	{
		if (!sArg.empty())
		{
			Args.AddNode("string").SetValue(sArg);
		}
	}

	// Automatic / Manual / Disabled map to RunAtLoad + KeepAlive combinations.
	// "true" and "false" are empty elements in plist; KXML serialises empty
	// nodes as self-closing tags automatically.
	if (Opts.Mode == KService::StartMode::Automatic)
	{
		Dict.AddNode("key").SetValue("RunAtLoad");
		Dict.AddNode("true");
		// restart on crash (non-zero exit) but not on clean exit
		Dict.AddNode("key").SetValue("KeepAlive");
		auto KeepAlive = Dict.AddNode("dict");
		KeepAlive.AddNode("key").SetValue("SuccessfulExit");
		KeepAlive.AddNode("false");
	}
	else
	{
		// Manual or Disabled => do not launch at load, user invokes explicitly
		Dict.AddNode("key").SetValue("RunAtLoad");
		Dict.AddNode("false");
	}

	// User / group override (system scope only — user agents already run
	// as the logged-in user)
	if (!bUserScope && !Opts.sRunAsUser.empty())
	{
		auto iColon = Opts.sRunAsUser.find(':');

		if (iColon == KString::npos)
		{
			AddStringPair(Dict, "UserName", Opts.sRunAsUser);
		}
		else
		{
			AddStringPair(Dict, "UserName",  Opts.sRunAsUser.substr(0, iColon));
			AddStringPair(Dict, "GroupName", Opts.sRunAsUser.substr(iColon + 1));
		}
	}

	// Log paths — always set so debugging does not require plist surgery
	auto logs = LogPaths(sLabel, bUserScope);

	if (!logs.first.empty())
	{
		AddStringPair(Dict, "StandardOutPath",   logs.first);
		AddStringPair(Dict, "StandardErrorPath", logs.second);
	}

	// prepend the fixed plist boilerplate (XML declaration + DOCTYPE)
	Xml.AddXMLDeclaration("1.0", "UTF-8", "");
	Xml.AddDocumentType("plist",
	                    "-//Apple//DTD PLIST 1.0//EN",
	                    "http://www.apple.com/DTDs/PropertyList-1.0.dtd");

	KString sOut = Xml.Serialize();

	// inject the free-form header comment right before the root <plist> tag;
	// this is a string operation because KXML has no public comment-node API
	constexpr KStringView sRootTag = "<plist ";
	auto iPos = sOut.find(sRootTag);
	if (iPos != KString::npos)
	{
		sOut.insert(iPos, "<!-- generated by dekaf2 KService — do not edit by hand -->\n");
	}

	return sOut;

} // BuildPlist

#endif // DEKAF2_IS_MACOS

} // end of anonymous namespace

//-----------------------------------------------------------------------------
int KService::Run(KStringView sServiceName,
                  int         argc,
                  char**      argv,
                  MainFunc    fnMain,
                  bool        bSuppressHelp,
                  KStringView sDisplayName,
                  KStringView sDescription)
//-----------------------------------------------------------------------------
{
	// Step 1: dispatch any service-management flags before touching the
	// service runtime. HandleCLI() prints the stacked help block on
	// -h / -help / --help / -? and returns false (so we continue); on
	// -install / -uninstall / -start / -stop / -status it performs the
	// action and returns true — meaning fnMain must not be called.
	{
		int iCliExit = 0;

		if (HandleCLI(argc, argv, sServiceName, iCliExit, bSuppressHelp,
		              sDisplayName, sDescription))
		{
			return iCliExit;
		}
	}

#ifdef DEKAF2_IS_WINDOWS

	auto& c = ctx();

	c.wsServiceName = kutf::Convert<std::wstring>(sServiceName);
	c.fnMain        = std::move(fnMain);
	c.FilteredArgv  = FilterServiceFlag(argc, argv);
	c.iExitCode     = 0;

	const bool bExplicitService = HasServiceFlag(argc, argv);
	const bool bLikelyService   = bExplicitService || !HasInteractiveConsole();

	if (bLikelyService)
	{
		c.bIsService.store(true, std::memory_order_release);

		// SCM keeps a pointer to this array until StartServiceCtrlDispatcher
		// returns, so it must outlive the call.
		std::wstring wsName = c.wsServiceName;
		SERVICE_TABLE_ENTRYW dispatchTable[] =
		{
			{ const_cast<LPWSTR>(wsName.c_str()), &ServiceMainW },
			{ nullptr, nullptr }
		};

		if (::StartServiceCtrlDispatcherW(dispatchTable))
		{
			// dispatcher returned after ServiceMainW completed
			return c.iExitCode;
		}

		const DWORD err = ::GetLastError();
		c.bIsService.store(false, std::memory_order_release);

		if (err != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			kWarning("StartServiceCtrlDispatcher failed: {}", WinErrorText(err));
		}
		else if (bExplicitService)
		{
			// user typed --service manually without being launched by SCM;
			// warn but keep running interactively so the CLI remains usable
			kDebug(1, "--service flag present but not launched by SCM — running interactively");
		}
		// fall through to interactive
	}

	// interactive path — run the user's main directly
	const int iFilteredArgc = static_cast<int>(c.FilteredArgv.size()) - 1;
	return c.fnMain(iFilteredArgc, c.FilteredArgv.data());

#elif defined(DEKAF2_IS_LINUX)

	(void)sServiceName;

	// systemd sets INVOCATION_ID for every unit it launches; its presence is
	// the canonical signal that we are running as a service. We deliberately
	// do NOT trigger the notify path merely because NOTIFY_SOCKET is set
	// (that would also fire under non-systemd supervisors that we do not
	// want to bind to).
	const bool bIsService = !kGetEnv("INVOCATION_ID").empty();

	if (!bIsService)
	{
		// interactive launch → transparent passthrough
		return fnMain(argc, argv);
	}

	auto& s = ctx();
	s.bIsService.store(true, std::memory_order_release);
	s.sNotifySocket = kGetEnv("NOTIFY_SOCKET");

	// Tell systemd we are up and running so Type=notify units progress to
	// the active state. Include MAINPID for robustness — systemd already
	// knows our pid but being explicit avoids races with forked children.
	if (!s.sNotifySocket.empty())
	{
		SdNotify(kFormat("READY=1\nMAINPID={}\n", static_cast<long>(::getpid())));
	}

	int iExit = 0;

	try
	{
		iExit = fnMain(argc, argv);
	}
	catch (const std::exception& ex)
	{
		kException(ex);
		iExit = 1;
	}
	catch (...)
	{
		iExit = 1;
	}

	// inform systemd that we are on our way out so it does not flag our
	// exit as unexpected
	if (!s.sNotifySocket.empty())
	{
		SdNotify("STOPPING=1\n");
	}

	return iExit;

#elif defined(DEKAF2_IS_MACOS)

	(void)sServiceName;

	// launchd is pid 1 on macOS and spawns managed services as direct
	// children, so getppid() == 1 is the canonical signal that we are
	// running as a launch daemon / agent. Scripts, ssh sessions, and
	// terminal launches never see this parent.
	const bool bIsService = (::getppid() == 1);

	if (!bIsService)
	{
		return fnMain(argc, argv);
	}

	ctx().bIsService.store(true, std::memory_order_release);

	// launchd has no ready-notification protocol — as soon as the process
	// is alive it is considered started. Just call the user's main.
	return fnMain(argc, argv);

#else
	// other POSIX: transparent passthrough
	(void)sServiceName;
	return fnMain(argc, argv);
#endif

} // Run

//-----------------------------------------------------------------------------
bool KService::Install(KStringView sServiceName, const InstallOptions& Opts)
//-----------------------------------------------------------------------------
{
	// If a service with this name is already registered, remove it first so
	// that the new arguments / paths / options take effect. Without this:
	//   - macOS:  `launchctl load -w` is a no-op for an already-loaded
	//             label, so launchd keeps the original ProgramArguments
	//             array even after the plist on disk was overwritten.
	//   - Linux:  `kWriteFile` + `daemon-reload` picks up the new
	//             ExecStart=, but a currently-running instance keeps the
	//             old arguments until the unit is restarted.
	//   - Windows: CreateServiceW returns ERROR_SERVICE_EXISTS — a second
	//             install of the same name would otherwise fail outright.
	// Uninstall() on a non-existent service prints a warning, so we
	// gate the call on IsInstalled() to keep output clean for the
	// common first-install case.
	if (IsInstalled(sServiceName))
	{
		if (!Uninstall(sServiceName))
		{
			kWarning("cannot replace existing installation of service '{}'", sServiceName);
			return false;
		}
	}

#ifdef DEKAF2_IS_WINDOWS

	SC_HANDLE scm = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);

	if (!scm)
	{
		kWarning("OpenSCManager failed — install requires administrator privileges: {}",
		         WinErrorText(::GetLastError()));
		return false;
	}

	KString sBinary = Opts.sBinaryPath.empty() ? kGetOwnPathname() : Opts.sBinaryPath;

	if (sBinary.empty())
	{
		kWarning("cannot determine own executable path");
		::CloseServiceHandle(scm);
		return false;
	}

	// compose the binPath: "<exe>" [args] --service
	KString sBinPath;

	if (sBinary.find(' ') != KString::npos)
	{
		sBinPath = kFormat("\"{}\"", sBinary);
	}
	else
	{
		sBinPath = sBinary;
	}

	if (!Opts.sArguments.empty())
	{
		sBinPath += ' ';
		sBinPath += Opts.sArguments;
	}

	sBinPath += ' ';
	sBinPath += s_sServiceFlag;

	std::wstring wsName    = kutf::Convert<std::wstring>(sServiceName);
	std::wstring wsDisplay = kutf::Convert<std::wstring>(Opts.sDisplayName.empty()
	                                                     ? KStringView(sServiceName)
	                                                     : KStringView(Opts.sDisplayName));
	std::wstring wsPath    = kutf::Convert<std::wstring>(sBinPath);
	std::wstring wsUser    = Opts.sRunAsUser.empty()     ? std::wstring{}
	                                                     : kutf::Convert<std::wstring>(Opts.sRunAsUser);
	std::wstring wsPass    = Opts.sRunAsPassword.empty() ? std::wstring{}
	                                                     : kutf::Convert<std::wstring>(Opts.sRunAsPassword);

	DWORD dwStart = SERVICE_AUTO_START;

	switch (Opts.Mode)
	{
		case StartMode::Automatic: dwStart = SERVICE_AUTO_START;   break;
		case StartMode::Manual:    dwStart = SERVICE_DEMAND_START; break;
		case StartMode::Disabled:  dwStart = SERVICE_DISABLED;     break;
	}

	SC_HANDLE svc = ::CreateServiceW(
	    scm,
	    wsName.c_str(),
	    wsDisplay.c_str(),
	    SERVICE_ALL_ACCESS,
	    SERVICE_WIN32_OWN_PROCESS,
	    dwStart,
	    SERVICE_ERROR_NORMAL,
	    wsPath.c_str(),
	    nullptr,    // no load-order group
	    nullptr,    // no tag id
	    nullptr,    // no dependencies
	    wsUser.empty() ? nullptr : wsUser.c_str(),
	    wsPass.empty() ? nullptr : wsPass.c_str());

	if (!svc)
	{
		const DWORD err = ::GetLastError();

		if (err == ERROR_SERVICE_EXISTS)
		{
			kWarning("service '{}' is already installed", sServiceName);
		}
		else
		{
			kWarning("CreateService failed: {}", WinErrorText(err));
		}
		::CloseServiceHandle(scm);
		return false;
	}

	if (!Opts.sDescription.empty())
	{
		std::wstring        wsDesc = kutf::Convert<std::wstring>(Opts.sDescription);
		SERVICE_DESCRIPTIONW desc  = { const_cast<LPWSTR>(wsDesc.c_str()) };
		::ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);
	}

	::CloseServiceHandle(svc);
	::CloseServiceHandle(scm);

	return true;

#elif defined(DEKAF2_IS_LINUX)

	// If the caller did not explicitly request user scope but we are not
	// running as root, silently fall back to a user-scope install rather
	// than fail on the write to /etc/systemd/system. We only warn once so
	// interactive sessions get a clear hint about what happened.
	bool bUserScope = Opts.bUserScope;

	if (!bUserScope && ::getuid() != 0)
	{
		// write directly to stderr so the hint is visible even when KLog
		// warnings are disabled — this is a CLI-facing notice, not a bug
		kPrintLine(stderr,
		           ":: not running as root — installing '{}' as user-scope service "
		           "under ~/.config/systemd/user\n"
		           ":: run with sudo for a system-wide install",
		           sServiceName);
		bUserScope = true;
	}

	KString sUnitPath = UnitFilePath(sServiceName, bUserScope);

	if (sUnitPath.empty())
	{
		kWarning("cannot determine unit-file path for user scope — $HOME not set");
		return false;
	}

	// make sure the parent directory exists (user scope usually does not
	// ship with a pre-existing ~/.config/systemd/user)
	auto iSlash = sUnitPath.rfind('/');

	if (iSlash != KString::npos)
	{
		KString sDir = sUnitPath.substr(0, iSlash);

		if (!sDir.empty() && !kCreateDir(sDir))
		{
			kWarning("cannot create directory '{}'", sDir);
			return false;
		}
	}

	// take a mutable copy of the options so BuildUnitFile() sees the effective
	// scope in case we auto-switched above (affects WantedBy=)
	InstallOptions EffectiveOpts = Opts;
	EffectiveOpts.bUserScope     = bUserScope;

	KString sUnit = BuildUnitFile(sServiceName, EffectiveOpts);

	if (!kWriteFile(sUnitPath, sUnit))
	{
		kWarning("cannot write unit file '{}'", sUnitPath);
		return false;
	}

	KString sQuotedName = QuoteForShell(sServiceName);

	// re-read unit files so daemon-reload is visible to subsequent calls
	if (Systemctl(bUserScope, "daemon-reload") != 0)
	{
		kWarning("systemctl daemon-reload failed");
		// continue anyway — the unit file was written
	}

	// Automatic => enable at boot; Manual => registered but not enabled;
	// Disabled => explicitly disabled
	if (EffectiveOpts.Mode == StartMode::Automatic)
	{
		if (Systemctl(bUserScope, kFormat("enable {}", sQuotedName)) != 0)
		{
			kWarning("systemctl enable '{}' failed", sServiceName);
			// unit file is still in place — caller can retry manually
		}
	}
	else if (EffectiveOpts.Mode == StartMode::Disabled)
	{
		Systemctl(bUserScope, kFormat("disable {}", sQuotedName));
	}

	return true;

#elif defined(DEKAF2_IS_MACOS)

	bool bUserScope = Opts.bUserScope;

	// Same policy as Linux: if caller didn't request user-scope but we are
	// not root, fall back to a LaunchAgent install. Direct stderr notice so
	// it's visible even with KLog silenced.
	if (!bUserScope && ::getuid() != 0)
	{
		kPrintLine(stderr,
		           ":: not running as root — installing '{}' as LaunchAgent "
		           "under ~/Library/LaunchAgents\n"
		           ":: run with sudo for a system-wide LaunchDaemon install",
		           sServiceName);
		bUserScope = true;
	}

	KString sLabel    = MakeLabel(sServiceName);
	KString sPlistPath = PlistPath(sLabel, bUserScope);

	if (sPlistPath.empty())
	{
		kWarning("cannot determine plist path — $HOME not set");
		return false;
	}

	// make sure the plist directory exists (fresh macOS accounts don't
	// ship with a ~/Library/LaunchAgents)
	auto iSlash = sPlistPath.rfind('/');

	if (iSlash != KString::npos)
	{
		KString sDir = sPlistPath.substr(0, iSlash);

		if (!sDir.empty() && !kCreateDir(sDir))
		{
			kWarning("cannot create directory '{}'", sDir);
			return false;
		}
	}

	// ensure the log directory exists too; /var/log and ~/Library/Logs
	// usually exist already but a `kCreateDir` is a no-op in that case
	auto logs = LogPaths(sLabel, bUserScope);

	if (!logs.first.empty())
	{
		auto iLogSlash = logs.first.rfind('/');

		if (iLogSlash != KString::npos)
		{
			KString sLogDir = logs.first.substr(0, iLogSlash);

			if (!sLogDir.empty())
			{
				// best-effort; failure here is not fatal (launchd will just
				// log that it cannot open the file)
				kCreateDir(sLogDir);
			}
		}
	}

	KString sPlist = BuildPlist(sLabel, Opts, bUserScope);

	if (!kWriteFile(sPlistPath, sPlist))
	{
		kWarning("cannot write plist file '{}'", sPlistPath);
		return false;
	}

	// load + enable the job — -w persists the "enabled" state across reboots
	if (Launchctl(kFormat("load -w {}", QuoteForShell(sPlistPath))) != 0)
	{
		kWarning("launchctl load '{}' failed — plist remains in place; "
		         "retry manually with: launchctl load -w {}",
		         sLabel, sPlistPath);
		// do not unwind the plist write: the caller may want to inspect it
	}

	return true;

#else
	(void)sServiceName;
	(void)Opts;
	kWarning("service management is not implemented on this platform");
	return false;
#endif

} // Install

//-----------------------------------------------------------------------------
bool KService::Uninstall(KStringView sServiceName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	// Uninstall on Windows is a three-step dance because SCM's stop / delete
	// semantics are both asynchronous and interlocked:
	//
	//   1. ControlService(STOP) kicks the service out of SERVICE_RUNNING
	//      but returns as soon as the service reports SERVICE_STOP_PENDING;
	//      the actual process exit may take seconds.
	//   2. DeleteService only MARKS the service for deletion; the record is
	//      kept alive until every open handle — including the service
	//      process's own SERVICE_STATUS_HANDLE — is closed.
	//   3. A subsequent CreateServiceW() with the same name therefore fails
	//      with ERROR_SERVICE_MARKED_FOR_DELETE if either the running
	//      process has not exited yet, or if SCM is still cleaning up the
	//      deleted record.
	//
	// We therefore (a) wait for the service to reach SERVICE_STOPPED before
	// calling DeleteService, and (b) poll OpenServiceW() until it returns
	// ERROR_SERVICE_DOES_NOT_EXIST, guaranteeing that an immediate Install()
	// with the same name can succeed. Both waits cap at 30 s and degrade
	// gracefully if exceeded.

	constexpr DWORD iWaitStepMs   = 250;
	constexpr DWORD iWaitTotalMs  = 30000;

	SC_HANDLE scm = nullptr;
	SC_HANDLE svc = nullptr;

	if (!OpenServiceByName(sServiceName, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS, scm, svc))
	{
		kWarning("OpenService '{}' failed: {}", sServiceName, WinErrorText(::GetLastError()));
		return false;
	}

	// Step 1: if the service is running, request a stop and wait for it
	// to actually terminate. A service that is already stopped or in
	// SERVICE_STOP_PENDING is still waited for.
	{
		SERVICE_STATUS_PROCESS ssp   = {};
		DWORD                  dwLen = 0;

		if (::QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO,
		                           reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &dwLen))
		{
			if (ssp.dwCurrentState != SERVICE_STOPPED
			 && ssp.dwCurrentState != SERVICE_STOP_PENDING)
			{
				SERVICE_STATUS st = {};
				::ControlService(svc, SERVICE_CONTROL_STOP, &st);
			}

			DWORD iWaited = 0;

			while (ssp.dwCurrentState != SERVICE_STOPPED && iWaited < iWaitTotalMs)
			{
				::Sleep(iWaitStepMs);
				iWaited += iWaitStepMs;

				if (!::QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO,
				                            reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &dwLen))
				{
					break;
				}
			}

			if (ssp.dwCurrentState != SERVICE_STOPPED)
			{
				kWarning("service '{}' did not reach SERVICE_STOPPED within {} s — "
				         "continuing with DeleteService anyway",
				         sServiceName, iWaitTotalMs / 1000);
			}
		}
	}

	// Step 2: mark the service record for deletion. ERROR_SERVICE_MARKED_FOR_DELETE
	// means a previous uninstall attempt is still in flight — we treat that as
	// success and just let the poll loop below wait for it to finish.
	const BOOL  bDeleted = ::DeleteService(svc);
	const DWORD iDelErr  = bDeleted ? 0 : ::GetLastError();

	::CloseServiceHandle(svc);
	::CloseServiceHandle(scm);

	if (!bDeleted && iDelErr != ERROR_SERVICE_MARKED_FOR_DELETE)
	{
		kWarning("DeleteService '{}' failed: {}", sServiceName, WinErrorText(iDelErr));
		return false;
	}

	// Step 3: block until the record is actually gone. Each iteration opens
	// a fresh SCM / service handle so we are not pinning the record open
	// ourselves.
	{
		std::wstring wsName  = kutf::Convert<std::wstring>(sServiceName);
		DWORD        iWaited = 0;

		while (iWaited < iWaitTotalMs)
		{
			SC_HANDLE scmProbe = ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);

			if (!scmProbe)
			{
				// SCM not reachable — nothing useful we can do; assume gone.
				break;
			}

			SC_HANDLE svcProbe = ::OpenServiceW(scmProbe, wsName.c_str(), SERVICE_QUERY_STATUS);

			if (!svcProbe)
			{
				const DWORD err = ::GetLastError();
				::CloseServiceHandle(scmProbe);

				if (err == ERROR_SERVICE_DOES_NOT_EXIST)
				{
					return true;  // fully gone
				}
				// any other error → give up the wait; the delete succeeded
				// from our side so caller can proceed.
				break;
			}

			::CloseServiceHandle(svcProbe);
			::CloseServiceHandle(scmProbe);

			::Sleep(iWaitStepMs);
			iWaited += iWaitStepMs;
		}

		if (iWaited >= iWaitTotalMs)
		{
			kWarning("service '{}' is still marked-for-delete after {} s — "
			         "subsequent reinstall may fail",
			         sServiceName, iWaitTotalMs / 1000);
		}
	}

	return true;

#elif defined(DEKAF2_IS_LINUX)

	bool bUserScope = false;

	if (!DetectInstalledScope(sServiceName, &bUserScope))
	{
		kWarning("service '{}' is not installed", sServiceName);
		return false;
	}

	KString sQuotedName = QuoteForShell(sServiceName);

	// best-effort stop + disable (ignore failures so we can still remove
	// the unit file if the service was already inactive or disabled)
	Systemctl(bUserScope, kFormat("stop {}",    sQuotedName));
	Systemctl(bUserScope, kFormat("disable {}", sQuotedName));

	KString sUnitPath = UnitFilePath(sServiceName, bUserScope);

	if (!sUnitPath.empty() && kFileExists(sUnitPath))
	{
		if (!kRemoveFile(sUnitPath))
		{
			kWarning("cannot remove unit file '{}'", sUnitPath);
			return false;
		}
	}

	Systemctl(bUserScope, "daemon-reload");
	return true;

#elif defined(DEKAF2_IS_MACOS)

	KString sLabel     = MakeLabel(sServiceName);
	bool    bUserScope = false;

	if (!DetectInstalledScope(sLabel, &bUserScope))
	{
		kWarning("service '{}' (label '{}') is not installed", sServiceName, sLabel);
		return false;
	}

	KString sPlistPath = PlistPath(sLabel, bUserScope);

	// unload first — this also stops the job. Best-effort: if unload fails
	// (e.g. job was not loaded) we still proceed to remove the plist file.
	Launchctl(kFormat("unload -w {}", QuoteForShell(sPlistPath)));

	if (!sPlistPath.empty() && kFileExists(sPlistPath))
	{
		if (!kRemoveFile(sPlistPath))
		{
			kWarning("cannot remove plist file '{}'", sPlistPath);
			return false;
		}
	}

	return true;

#else
	(void)sServiceName;
	kWarning("service management is not implemented on this platform");
	return false;
#endif

} // Uninstall

//-----------------------------------------------------------------------------
bool KService::Start(KStringView sServiceName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	SC_HANDLE scm = nullptr;
	SC_HANDLE svc = nullptr;

	if (!OpenServiceByName(sServiceName, SERVICE_START, scm, svc))
	{
		kWarning("OpenService '{}' failed: {}", sServiceName, WinErrorText(::GetLastError()));
		return false;
	}

	const BOOL bOk = ::StartServiceW(svc, 0, nullptr);

	if (!bOk && ::GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
	{
		kWarning("StartService '{}' failed: {}", sServiceName, WinErrorText(::GetLastError()));
	}

	::CloseServiceHandle(svc);
	::CloseServiceHandle(scm);

	return bOk != 0;

#elif defined(DEKAF2_IS_LINUX)

	bool bUserScope = false;

	if (!DetectInstalledScope(sServiceName, &bUserScope))
	{
		kWarning("service '{}' is not installed", sServiceName);
		return false;
	}

	return Systemctl(bUserScope, kFormat("start {}", QuoteForShell(sServiceName))) == 0;

#elif defined(DEKAF2_IS_MACOS)

	KString sLabel = MakeLabel(sServiceName);

	if (!DetectInstalledScope(sLabel, nullptr))
	{
		kWarning("service '{}' (label '{}') is not installed", sServiceName, sLabel);
		return false;
	}

	return Launchctl(kFormat("start {}", QuoteForShell(sLabel))) == 0;

#else
	(void)sServiceName;
	kWarning("service management is not implemented on this platform");
	return false;
#endif

} // Start

//-----------------------------------------------------------------------------
bool KService::Stop(KStringView sServiceName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	SC_HANDLE scm = nullptr;
	SC_HANDLE svc = nullptr;

	if (!OpenServiceByName(sServiceName, SERVICE_STOP, scm, svc))
	{
		kWarning("OpenService '{}' failed: {}", sServiceName, WinErrorText(::GetLastError()));
		return false;
	}

	SERVICE_STATUS  st = {};
	const BOOL      bOk = ::ControlService(svc, SERVICE_CONTROL_STOP, &st);

	if (!bOk)
	{
		const DWORD err = ::GetLastError();

		if (err != ERROR_SERVICE_NOT_ACTIVE)
		{
			kWarning("ControlService(STOP) '{}' failed: {}", sServiceName, WinErrorText(err));
		}
	}

	::CloseServiceHandle(svc);
	::CloseServiceHandle(scm);

	return bOk != 0;

#elif defined(DEKAF2_IS_LINUX)

	bool bUserScope = false;

	if (!DetectInstalledScope(sServiceName, &bUserScope))
	{
		kWarning("service '{}' is not installed", sServiceName);
		return false;
	}

	return Systemctl(bUserScope, kFormat("stop {}", QuoteForShell(sServiceName))) == 0;

#elif defined(DEKAF2_IS_MACOS)

	KString sLabel = MakeLabel(sServiceName);

	if (!DetectInstalledScope(sLabel, nullptr))
	{
		kWarning("service '{}' (label '{}') is not installed", sServiceName, sLabel);
		return false;
	}

	return Launchctl(kFormat("stop {}", QuoteForShell(sLabel))) == 0;

#else
	(void)sServiceName;
	kWarning("service management is not implemented on this platform");
	return false;
#endif

} // Stop

//-----------------------------------------------------------------------------
KService::State KService::Status(KStringView sServiceName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	SC_HANDLE scm = nullptr;
	SC_HANDLE svc = nullptr;

	if (!OpenServiceByName(sServiceName, SERVICE_QUERY_STATUS, scm, svc))
	{
		return State::Stopped;
	}

	SERVICE_STATUS_PROCESS ssp   = {};
	DWORD                  dwLen = 0;
	const BOOL bOk = ::QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO,
	                                        reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &dwLen);

	::CloseServiceHandle(svc);
	::CloseServiceHandle(scm);

	if (!bOk)
	{
		return State::Stopped;
	}

	return FromWinState(ssp.dwCurrentState);

#elif defined(DEKAF2_IS_LINUX)

	bool bUserScope = false;

	if (!DetectInstalledScope(sServiceName, &bUserScope))
	{
		return State::Stopped;
	}

	KString sOutput;
	SystemctlCapture(bUserScope, kFormat("is-active {}", QuoteForShell(sServiceName)), sOutput);

	// `systemctl is-active` maps cleanly to our State enum; unknown values
	// (e.g. "failed") fall through to Stopped
	if (sOutput == "active")       return State::Running;
	if (sOutput == "activating")   return State::StartPending;
	if (sOutput == "deactivating") return State::StopPending;
	if (sOutput == "reloading")    return State::StartPending;

	return State::Stopped;

#elif defined(DEKAF2_IS_MACOS)

	KString sLabel = MakeLabel(sServiceName);

	if (!DetectInstalledScope(sLabel, nullptr))
	{
		return State::Stopped;
	}

	KString sOutput;
	int iExit = LaunchctlCapture(kFormat("list {}", QuoteForShell(sLabel)), sOutput);

	if (iExit != 0)
	{
		// launchctl list returns 113 (ESRCH) when the label is not loaded
		return State::Stopped;
	}

	// `launchctl list <label>` prints a plist dict. A running job contains
	// a line like:  "PID" = 1234;
	// No PID line means the job is loaded but not currently executing.
	if (sOutput.find("\"PID\"") != KString::npos)
	{
		return State::Running;
	}

	return State::Stopped;

#else
	(void)sServiceName;
	return State::Stopped;
#endif

} // Status

//-----------------------------------------------------------------------------
bool KService::IsInstalled(KStringView sServiceName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	SC_HANDLE scm = nullptr;
	SC_HANDLE svc = nullptr;

	if (!OpenServiceByName(sServiceName, SERVICE_QUERY_CONFIG, scm, svc))
	{
		return false;
	}

	::CloseServiceHandle(svc);
	::CloseServiceHandle(scm);

	return true;

#elif defined(DEKAF2_IS_LINUX)

	return DetectInstalledScope(sServiceName, nullptr);

#elif defined(DEKAF2_IS_MACOS)

	return DetectInstalledScope(MakeLabel(sServiceName), nullptr);

#else
	(void)sServiceName;
	return false;
#endif

} // IsInstalled

//-----------------------------------------------------------------------------
bool KService::IsRunningAsService()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	return ctx().bIsService.load(std::memory_order_acquire);
#elif defined(DEKAF2_IS_LINUX)
	return ctx().bIsService.load(std::memory_order_acquire);
#elif defined(DEKAF2_IS_MACOS)
	return ctx().bIsService.load(std::memory_order_acquire);
#else
	return false;
#endif

} // IsRunningAsService

//-----------------------------------------------------------------------------
void KService::SetShutdownHandler(std::function<void()> fnShutdown)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	ctx().fnShutdown = std::move(fnShutdown);
#elif defined(DEKAF2_IS_LINUX)
	// Store the handler and install a SIGTERM / SIGINT trampoline. Note:
	// std::signal is coarse — applications that use KSignals (via KInit(true))
	// block these signals on the main thread, so our trampoline will not fire
	// in that setup. Such applications should route their KSignals handler
	// into their own shutdown logic directly rather than rely on this method.
	ctx().fnShutdown = std::move(fnShutdown);

	if (ctx().fnShutdown)
	{
		std::signal(SIGTERM, &PosixSignalTrampoline);
		std::signal(SIGINT,  &PosixSignalTrampoline);
	}
#elif defined(DEKAF2_IS_MACOS)
	// launchd sends SIGTERM when a service is unloaded / stopped. Same
	// KSignals caveat as on Linux: if the application uses KInit(true) and
	// has KSignals block SIGTERM on all threads, our trampoline will not
	// fire — route shutdown through KSignals in that case.
	ctx().fnShutdown = std::move(fnShutdown);

	if (ctx().fnShutdown)
	{
		std::signal(SIGTERM, &PosixSignalTrampoline);
		std::signal(SIGINT,  &PosixSignalTrampoline);
	}
#else
	(void)fnShutdown;
#endif

} // SetShutdownHandler

//-----------------------------------------------------------------------------
void KService::ReportState(State state, uint32_t iWaitHintMs)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	ReportStateImpl(state, iWaitHintMs);
#elif defined(DEKAF2_IS_LINUX)
	(void)iWaitHintMs;

	if (!ctx().bIsService.load(std::memory_order_acquire))
	{
		return;
	}

	// map our abstract state enum to systemd's notify protocol
	switch (state)
	{
		case State::Running:
			SdNotify(kFormat("READY=1\nMAINPID={}\n", static_cast<long>(::getpid())));
			break;
		case State::StopPending:
		case State::Stopped:
			SdNotify("STOPPING=1\n");
			break;
		case State::StartPending:
			SdNotify("STATUS=starting\n");
			break;
		case State::ContinuePending:
		case State::PausePending:
		case State::Paused:
			// no first-class equivalent in systemd
			break;
	}
#elif defined(DEKAF2_IS_MACOS)
	// launchd has no state-reporting protocol — the process is simply
	// considered running as long as it is alive. Nothing to do here.
	(void)state;
	(void)iWaitHintMs;
#else
	(void)state;
	(void)iWaitHintMs;
#endif

} // ReportState

namespace {

// Service-management help block emitted by HandleCLI() when it sees
// -h / -help / --help / -?. Kept at file scope so it is compiled into
// .rodata rather than rebuilt on every call.
constexpr const char* s_pszCLIHelp =
    "Service management:\n"
    "\n"
    "   -install   : install the running binary as a system service\n"
    "                Any further arguments on the same command line are\n"
    "                baked into the generated unit / plist and replayed\n"
    "                on every service start, e.g.:\n"
    "                   mysvc -install -p 1234 -t localhost:4949\n"
    "   -uninstall : remove a previously installed service\n"
    "   -start     : ask the service manager to start the service\n"
    "   -stop      : ask the service manager to stop the service\n"
    "   -status    : print the current service state\n"
#if defined(DEKAF2_IS_LINUX) || defined(DEKAF2_IS_MACOS)
    "\n"
    "   -install falls back to a user-scoped unit if the process is not\n"
    "    running as root\n"
#endif
    ;

} // anonymous namespace

//-----------------------------------------------------------------------------
KStringViewZ KService::GetHelp()
//-----------------------------------------------------------------------------
{
	return s_pszCLIHelp;
}

//-----------------------------------------------------------------------------
bool KService::HandleCLI(int           argc,
                         char**        argv,
                         KStringView   sServiceName,
                         int&          iExitCodeOut,
                         bool          bSuppressHelp,
                         KStringView   sDisplayName,
                         KStringView   sDescription)
//-----------------------------------------------------------------------------
{
	iExitCodeOut = 0;

	auto stripDashes = [](KStringView sArg) -> KStringView
	{
		if (sArg.size() > 2 && sArg[0] == '-' && sArg[1] == '-')
		{
			return sArg.substr(2);
		}
		if (sArg.size() > 1 && sArg[0] == '-')
		{
			return sArg.substr(1);
		}
		return {};
	};

	// Names of the service-control commands we recognise. Any argv element
	// that is NOT one of these (and comes after the binary name) is treated
	// as a runtime argument for the service itself and persisted into the
	// generated unit / plist on `install`.
	auto IsServiceCommand = [](KStringView sStripped) -> bool
	{
		return sStripped == "install"
		    || sStripped == "uninstall"
		    || sStripped == "start"
		    || sStripped == "stop"
		    || sStripped == "status";
	};

	// Help flags (single-dash and double-dash accepted; "?" is the
	// traditional one-char help glyph on Windows and Catch).
	auto IsHelpFlag = [](KStringView sStripped) -> bool
	{
		return sStripped == "h"
		    || sStripped == "help"
		    || sStripped == "?";
	};

	// First pass: spot help flags. If any is present AND no service-control
	// flag is present, emit the KService help block and fall through
	// (return false) so the caller's own help handler still runs. This
	// produces "stacked help" where KService options appear alongside the
	// application's own synopsis.
	bool bHasHelp           = false;
	bool bHasServiceCommand = false;

	for (int i = 1; i < argc; ++i)
	{
		if (!argv[i])
		{
			continue;
		}

		KStringView sStripped = stripDashes(KStringView(argv[i]));

		if (sStripped.empty())
		{
			continue;
		}

		if (IsServiceCommand(sStripped))
		{
			bHasServiceCommand = true;
		}
		else if (IsHelpFlag(sStripped))
		{
			bHasHelp = true;
		}
	}

	// we treat no arguments (argc == 1) as if --help was passed)
	if (argc == 1 || (bHasHelp && !bHasServiceCommand))
	{
		if (!bSuppressHelp)
		{
			kPrint("\n{}", s_pszCLIHelp);
		}
		return false;
	}

	// Collect all non-service-command argv elements as a space-separated
	// string so Install() can embed them in the unit / plist. The strings
	// are taken verbatim (dashes included) so they reproduce the original
	// interactive command line.
	auto CollectProgramArguments = [&]() -> KString
	{
		KString sArgs;

		for (int j = 1; j < argc; ++j)
		{
			if (!argv[j])
			{
				continue;
			}

			KStringView sRaw      = KStringView(argv[j]);
			KStringView sStripped = stripDashes(sRaw);

			if (!sStripped.empty() && IsServiceCommand(sStripped))
			{
				continue;
			}

			if (!sArgs.empty())
			{
				sArgs += ' ';
			}
			sArgs += sRaw;
		}

		return sArgs;
	};

	for (int i = 1; i < argc; ++i)
	{
		if (!argv[i])
		{
			continue;
		}

		KStringView sArg = stripDashes(KStringView(argv[i]));

		if (sArg.empty())
		{
			continue;
		}

		if (sArg == "install")
		{
			InstallOptions Opts;
			Opts.sDisplayName = sDisplayName.empty() ? KString(sServiceName) : KString(sDisplayName);
			Opts.sDescription = KString(sDescription);
			Opts.sArguments   = CollectProgramArguments();

			if (Install(sServiceName, Opts))
			{
				// confirmations on stdout so success output can be captured
				// or piped; the user-scope fallback warning (emitted from
				// Install() itself) already goes to stderr.
				kPrintLine(":: service '{}' installed", sServiceName);
				if (!Opts.sArguments.empty())
				{
					kPrintLine(":: runtime arguments: {}", Opts.sArguments);
				}
			}
			else
			{
				// failures on stderr so they surface in quiet pipelines
				kPrintLine(stderr, ":: service '{}' install failed", sServiceName);
				iExitCodeOut = 1;
			}
			return true;
		}

		if (sArg == "uninstall")
		{
			if (Uninstall(sServiceName))
			{
				kPrintLine(":: service '{}' uninstalled", sServiceName);
			}
			else
			{
				kPrintLine(stderr, ":: service '{}' uninstall failed", sServiceName);
				iExitCodeOut = 1;
			}
			return true;
		}

		if (sArg == "start")
		{
			if (Start(sServiceName))
			{
				kPrintLine(":: service '{}' started", sServiceName);
			}
			else
			{
				kPrintLine(stderr, ":: service '{}' start failed", sServiceName);
				iExitCodeOut = 1;
			}
			return true;
		}

		if (sArg == "stop")
		{
			if (Stop(sServiceName))
			{
				kPrintLine(":: service '{}' stopped", sServiceName);
			}
			else
			{
				kPrintLine(stderr, ":: service '{}' stop failed", sServiceName);
				iExitCodeOut = 1;
			}
			return true;
		}

		if (sArg == "status")
		{
			KStringView sLabel;
			switch (Status(sServiceName))
			{
				case State::Stopped:         sLabel = "stopped";          break;
				case State::StartPending:    sLabel = "start pending";    break;
				case State::StopPending:     sLabel = "stop pending";     break;
				case State::Running:         sLabel = "running";          break;
				case State::ContinuePending: sLabel = "continue pending"; break;
				case State::PausePending:    sLabel = "pause pending";    break;
				case State::Paused:          sLabel = "paused";           break;
			}
			kPrintLine(":: service '{}' status: {}", sServiceName, sLabel);
			return true;
		}
	}

	return false;

} // HandleCLI

DEKAF2_NAMESPACE_END
