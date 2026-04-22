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
#include <dekaf2/util/cli/koptions.h>
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
#endif

DEKAF2_NAMESPACE_BEGIN

namespace {

#ifdef DEKAF2_IS_WINDOWS

/// Argument injected into the registered service's command line. KService::Run
/// uses it as a fast, unambiguous indicator that the binary should engage the
/// SCM dispatcher. It is filtered from argv before the user's main runs.
constexpr KStringView s_sServiceFlag = "--service";

//-----------------------------------------------------------------------------
/// Shared state referenced by the Win32 SCM C-style callbacks. There can only
/// ever be one active service per process, so static storage is appropriate.
struct ServiceContext
//-----------------------------------------------------------------------------
{
	std::wstring                  wsServiceName;
	SERVICE_STATUS_HANDLE         StatusHandle = nullptr;
	SERVICE_STATUS                Status       = {};
	KService::MainFunc            fnMain;
	std::function<void()>         fnShutdown;
	std::vector<char*>            FilteredArgv;      // trailing nullptr for argv parity
	int                           iExitCode    = 0;
	std::atomic<bool>             bIsService { false };
	DWORD                         dwCheckPoint = 0;

}; // ServiceContext

//-----------------------------------------------------------------------------
ServiceContext& ctx()
//-----------------------------------------------------------------------------
{
	static ServiceContext s_ctx;
	return s_ctx;

} // ctx

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

} // end of anonymous namespace

//-----------------------------------------------------------------------------
int KService::Run(KStringView sServiceName, int argc, char** argv, MainFunc fnMain)
//-----------------------------------------------------------------------------
{
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

#else
	// non-Windows: transparent passthrough, no flag filtering needed since we
	// never inject --service into these platforms
	(void)sServiceName;
	return fnMain(argc, argv);
#endif

} // Run

//-----------------------------------------------------------------------------
bool KService::Install(KStringView sServiceName, const InstallOptions& Opts)
//-----------------------------------------------------------------------------
{
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

#else
	(void)sServiceName;
	(void)Opts;
	kWarning("Windows Services are not available on this platform");
	return false;
#endif

} // Install

//-----------------------------------------------------------------------------
bool KService::Uninstall(KStringView sServiceName)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	SC_HANDLE scm = nullptr;
	SC_HANDLE svc = nullptr;

	if (!OpenServiceByName(sServiceName, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS, scm, svc))
	{
		kWarning("OpenService '{}' failed: {}", sServiceName, WinErrorText(::GetLastError()));
		return false;
	}

	// best effort: stop it first (ignore failure)
	SERVICE_STATUS st = {};
	::ControlService(svc, SERVICE_CONTROL_STOP, &st);

	const BOOL bOk = ::DeleteService(svc);

	if (!bOk)
	{
		kWarning("DeleteService '{}' failed: {}", sServiceName, WinErrorText(::GetLastError()));
	}

	::CloseServiceHandle(svc);
	::CloseServiceHandle(scm);

	return bOk != 0;

#else
	(void)sServiceName;
	kWarning("Windows Services are not available on this platform");
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

#else
	(void)sServiceName;
	kWarning("Windows Services are not available on this platform");
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

#else
	(void)sServiceName;
	kWarning("Windows Services are not available on this platform");
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
#else
	(void)state;
	(void)iWaitHintMs;
#endif

} // ReportState

//-----------------------------------------------------------------------------
void KService::AddOptions(KOptions&   Options,
                          KStringView sServiceName,
                          KStringView sDisplayName,
                          KStringView sDescription)
//-----------------------------------------------------------------------------
{
	// KOptions persists the strings we hand it, so keeping value copies here is
	// fine even though the lambdas capture by value below.
	KString sName        = sServiceName;
	KString sDisplayCopy = sDisplayName.empty() ? sServiceName : sDisplayName;
	KString sDescrCopy   = sDescription;

	Options
	    .Option("install")
	    .Help("install as Windows service (requires administrator)")
	    .Final()
	    ([sName, sDisplayCopy, sDescrCopy]()
	    {
	        InstallOptions Opts;
	        Opts.sDisplayName = sDisplayCopy;
	        Opts.sDescription = sDescrCopy;

	        if (Install(sName, Opts))
	        {
	            kPrintLine(":: service '{}' installed", sName);
	        }
	        else
	        {
	            kPrintLine(":: service '{}' install failed", sName);
	            std::exit(1);
	        }
	    });

	Options
	    .Option("uninstall")
	    .Help("uninstall Windows service (requires administrator)")
	    .Final()
	    ([sName]()
	    {
	        if (Uninstall(sName))
	        {
	            kPrintLine(":: service '{}' uninstalled", sName);
	        }
	        else
	        {
	            kPrintLine(":: service '{}' uninstall failed", sName);
	            std::exit(1);
	        }
	    });

	Options
	    .Option("start")
	    .Help("start Windows service")
	    .Final()
	    ([sName]()
	    {
	        if (!Start(sName))
	        {
	            kPrintLine(":: service '{}' start failed", sName);
	            std::exit(1);
	        }
	        kPrintLine(":: service '{}' started", sName);
	    });

	Options
	    .Option("stop")
	    .Help("stop Windows service")
	    .Final()
	    ([sName]()
	    {
	        if (!Stop(sName))
	        {
	            kPrintLine(":: service '{}' stop failed", sName);
	            std::exit(1);
	        }
	        kPrintLine(":: service '{}' stopped", sName);
	    });

	Options
	    .Option("status")
	    .Help("query Windows service status")
	    .Final()
	    ([sName]()
	    {
	        KStringView sLabel;
	        switch (Status(sName))
	        {
	            case State::Stopped:         sLabel = "stopped";          break;
	            case State::StartPending:    sLabel = "start pending";    break;
	            case State::StopPending:     sLabel = "stop pending";     break;
	            case State::Running:         sLabel = "running";          break;
	            case State::ContinuePending: sLabel = "continue pending"; break;
	            case State::PausePending:    sLabel = "pause pending";    break;
	            case State::Paused:          sLabel = "paused";           break;
	        }
	        kPrintLine(":: service '{}' status: {}", sName, sLabel);
	    });

	// Note: the internal --service flag is NOT registered with KOptions — it is
	// filtered out of argv by KService::Run() before the user's main runs.

} // AddOptions

//-----------------------------------------------------------------------------
bool KService::HandleCLI(int           argc,
                         char**        argv,
                         KStringView   sServiceName,
                         int&          iExitCodeOut,
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

			if (Install(sServiceName, Opts))
			{
				kPrintLine(":: service '{}' installed", sServiceName);
			}
			else
			{
				kPrintLine(":: service '{}' install failed", sServiceName);
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
				kPrintLine(":: service '{}' uninstall failed", sServiceName);
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
				kPrintLine(":: service '{}' start failed", sServiceName);
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
				kPrintLine(":: service '{}' stop failed", sServiceName);
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
