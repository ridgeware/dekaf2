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
//
*/

#pragma once

/// @file kservice.h
/// Cross-platform wrapper that allows any long-running CLI (servers, pollers,
/// background workers) to be registered and run as a Windows Service or as a
/// systemd unit on Linux.
///
/// The same executable continues to run as a regular interactive program when
/// launched from a shell; the SCM / systemd integration machinery activates
/// only when the process was launched by the platform's service manager.
///
/// Service manager detection:
///   - Windows : launched by SCM (no console window, or --service flag present)
///   - Linux   : INVOCATION_ID environment variable set by systemd
///   - macOS   : no service-manager integration (transparent passthrough)

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/bits/kstringviewz.h>
#include <cstdint>
#include <functional>

DEKAF2_NAMESPACE_BEGIN

class KOptions;

/// @addtogroup system_os
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Adapter that registers an existing CLI main() with the Windows Service
/// Control Manager (SCM) or with systemd on Linux. The same executable
/// continues to run as a regular console program when launched interactively.
///
/// Usage pattern:
///
/// @code
///     int main(int argc, char** argv)
///     {
///         return KService::Run("mysvc", argc, argv, [](int ac, char** av)
///         {
///             return MyApp().Main(ac, av);
///         });
///     }
/// @endcode
///
/// Optionally, handle the install / uninstall / start / stop management flags
/// with either HandleCLI() (framework-free) or AddOptions() (KOptions-based).
///
/// Platform behaviour:
///   - Windows: uses SCM; install writes to the service registry; state is
///     reported via SetServiceStatus. An internal --service flag is injected
///     into the registered binPath and filtered back out of argv.
///   - Linux:   uses systemd; install writes a .service unit under
///     /etc/systemd/system (or ~/.config/systemd/user for user-scope),
///     calls `systemctl daemon-reload` + `systemctl enable`. Run() sends
///     `sd_notify` READY=1 / STOPPING=1 when launched by systemd (detected
///     via the INVOCATION_ID environment variable). Interactive launches
///     are transparent passthroughs — no notify, no signal changes.
///   - macOS:   service-manager integration is a no-op; Run() simply calls
///     the user's main and all management methods return false.
class DEKAF2_PUBLIC KService
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Signature of the user's real main function.
	using MainFunc = std::function<int(int argc, char** argv)>;

	/// Service state reported to the Service Control Manager.
	enum class State
	{
		Stopped,
		StartPending,
		StopPending,
		Running,
		ContinuePending,
		PausePending,
		Paused
	};

	/// How the service is started by Windows after reboot.
	enum class StartMode
	{
		Automatic,  ///< started at boot
		Manual,     ///< started on demand
		Disabled    ///< not started
	};

	/// Options for KService::Install(). Use a struct to keep the call site
	/// readable without a dozen positional arguments.
	struct InstallOptions
	{
		/// display name shown in services.msc / systemd Description= (falls
		/// back to sServiceName if empty)
		KString   sDisplayName;
		/// free-form description. On Windows appears in services.msc. On
		/// Linux preferred over sDisplayName for the systemd Description=
		/// field; falls back to sDisplayName / sServiceName.
		KString   sDescription;
		/// full binary path. If empty, Install() uses the current executable.
		KString   sBinaryPath;
		/// extra arguments appended to the binary path when registered.
		/// Windows: KService::Run() automatically recognises the "--service"
		/// flag that is injected into this argument string.
		/// Linux: written verbatim into ExecStart= of the systemd unit file.
		KString   sArguments;
		/// Windows: account to run the service under, e.g.
		/// "NT AUTHORITY\\NetworkService". Empty => LocalSystem.
		/// Linux: user name (and optional "user:group") for the systemd
		/// User= / Group= fields. Empty => run as root (system-scope) or the
		/// invoking user (user-scope).
		KString   sRunAsUser;
		/// Windows: password for sRunAsUser. Ignored for built-in accounts.
		/// Linux: not used (no equivalent).
		KString   sRunAsPassword;
		/// service startup mode. On Linux: Automatic => `systemctl enable`,
		/// Manual / Disabled => no enable (service can still be started
		/// manually via `systemctl start`).
		StartMode Mode;
		/// Linux only: if true, install as a user-scoped systemd unit under
		/// `~/.config/systemd/user/<name>.service` and use `systemctl --user`.
		/// If false (default) and the process runs as root, install
		/// system-wide under `/etc/systemd/system/<name>.service`. If false
		/// but the process is non-root, Install() automatically falls back
		/// to user scope with a warning (so `./mysvc -install` without sudo
		/// produces a functional user-level install). Ignored on Windows
		/// and macOS.
		bool      bUserScope;

		// user-declared default constructor so this struct can be used as a
		// default-argument value without triggering Clang's lazy-parsing issue
		// with in-class member initialisers
		InstallOptions() : Mode(StartMode::Automatic), bUserScope(false) {}
	};

	//-----------------------------------------------------------------------------
	/// Entry point wrapper. Call this from your main() instead of calling the
	/// application logic directly. If the process was launched by SCM, Run()
	/// engages the service dispatcher, reports RUNNING once fnMain starts, and
	/// routes SERVICE_CONTROL_STOP / SHUTDOWN into the registered shutdown
	/// handler (or SIGTERM via KSignals if no handler was set).
	/// If the process was launched interactively, fnMain is called directly
	/// with the full argv (stdin / stdout available as normal).
	/// @param sServiceName the internal service name used by SCM (no spaces).
	/// @param argc argc as received by main().
	/// @param argv argv as received by main().
	/// @param fnMain the user's real main function. Must return 0 on success.
	/// @return fnMain's exit code.
	static int Run(KStringView sServiceName, int argc, char** argv, MainFunc fnMain);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Register the current (or a given) executable as a Windows Service.
	/// Requires administrator privileges. No-op on non-Windows (returns false).
	/// @param sServiceName the internal service name (no spaces).
	/// @param Opts install parameters.
	/// @return true on success.
	static bool Install(KStringView sServiceName, const InstallOptions& Opts = InstallOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Unregister a previously installed service. Requires administrator
	/// privileges. No-op on non-Windows (returns false).
	/// @param sServiceName the internal service name (no spaces).
	static bool Uninstall(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ask SCM to start an installed service. No-op on non-Windows.
	static bool Start(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ask SCM to stop a running service. No-op on non-Windows.
	static bool Stop(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Query current service state from SCM. Returns State::Stopped on
	/// non-Windows or when the service is not installed / not reachable.
	static State Status(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// True if a service with this name is registered with SCM.
	/// Always false on non-Windows.
	static bool IsInstalled(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// True if the current process was launched by the Service Control
	/// Manager (i.e. fnMain is running inside the service dispatcher thread).
	/// Always false on non-Windows.
	static bool IsRunningAsService();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Register a handler to be invoked when SCM requests a stop / shutdown.
	/// If no handler is registered, KService raises SIGTERM through KSignals
	/// so applications that already hook signal shutdown keep working
	/// unchanged.
	/// The handler runs in the SCM control-thread context (not the main
	/// thread). It should return quickly; perform the actual shutdown work
	/// asynchronously (e.g. by setting an atomic stop flag or calling a
	/// server's Stop() method).
	static void SetShutdownHandler(std::function<void()> fnShutdown);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Manually report a state transition to SCM, e.g. during a long startup
	/// sequence. Not needed in the common case: KService automatically reports
	/// StartPending before calling fnMain, Running once fnMain is entered, and
	/// Stopped after fnMain returns.
	/// @param state the new state.
	/// @param iWaitHintMs SCM progress hint in milliseconds. Only meaningful
	/// for StartPending / StopPending / ContinuePending / PausePending.
	static void ReportState(State state, uint32_t iWaitHintMs = 0);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Inject uniform service-management options into an existing KOptions
	/// parser. Adds -install, -uninstall, -start, -stop, -status. Handlers
	/// call Install() / Uninstall() / Start() / Stop() / Status() and
	/// terminate the CLI with a success exit code after the operation
	/// completes.
	///
	/// NOTE: KOptions' automatic help generation is only able to discover
	/// ad-hoc options if NO pre-registered options exist at parse time.
	/// Applications that rely on the ad-hoc help-collection pattern (passing
	/// option help through the call operator like Opt("name: help text", ""))
	/// should prefer HandleCLI() below, which does not touch KOptions.
	///
	/// Safe to call on all platforms; on non-Windows the handlers emit a
	/// diagnostic that service management is a Windows-only feature.
	/// @param Options the KOptions instance of the CLI.
	/// @param sServiceName internal service name (no spaces).
	/// @param sDisplayName optional display name (defaults to sServiceName).
	/// @param sDescription optional free-form description.
	static void AddOptions(KOptions&    Options,
	                       KStringView  sServiceName,
	                       KStringView  sDisplayName = KStringView{},
	                       KStringView  sDescription = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Inspect argv directly for -install / -uninstall / -start / -stop /
	/// -status flags. If any is found, perform the requested operation and
	/// return true. The caller is expected to exit immediately when true is
	/// returned; the process exit code is set via the returned reference.
	///
	/// Unlike AddOptions() this does not depend on any CLI parser and is
	/// therefore compatible with applications that rely on KOptions' ad-hoc
	/// help-collection pattern.
	///
	/// Typical usage at the very top of main():
	/// @code
	///     int iExit = 0;
	///     if (KService::HandleCLI(argc, argv, "mysvc", iExit)) return iExit;
	/// @endcode
	///
	/// Recognised flags (case-sensitive, single- or double-dash accepted):
	///   -install     : install the running binary as a service
	///   -uninstall   : uninstall the service
	///   -start       : start the service
	///   -stop        : stop the service
	///   -status      : print the current service state
	///
	/// @param argc argc as received by main().
	/// @param argv argv as received by main().
	/// @param sServiceName internal service name (no spaces).
	/// @param iExitCodeOut set to 0 on success, non-zero on failure. Only
	/// meaningful when the function returns true.
	/// @param sDisplayName optional display name (defaults to sServiceName).
	/// @param sDescription optional free-form description.
	/// @return true if a service management flag was recognised and handled.
	static bool HandleCLI(int           argc,
	                      char**        argv,
	                      KStringView   sServiceName,
	                      int&          iExitCodeOut,
	                      KStringView   sDisplayName = KStringView{},
	                      KStringView   sDescription = KStringView{});
	//-----------------------------------------------------------------------------

}; // KService

/// @}

DEKAF2_NAMESPACE_END
