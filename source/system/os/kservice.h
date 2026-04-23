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
///   - macOS   : parent pid == 1 (launchd is pid 1 and spawns services directly)

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/bits/kstringviewz.h>
#include <cstdint>
#include <functional>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup system_os
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Adapter that registers an existing CLI main() with the Windows Service
/// Control Manager (SCM), with systemd on Linux, or with launchd on macOS.
/// The same executable continues to run as a regular console program when
/// launched interactively.
///
/// Usage pattern — a single call, any platform, any launch mode:
/// @code
///     int main(int argc, char** argv)
///     {
///         return KService::Run("mysvc", argc, argv,
///             [](int ac, char** av)
///             {
///                 return MyApp().Main(ac, av);
///             },
///             "My Service",
///             "Does stuff. Optional description.");
///     }
/// @endcode
///
/// Run() internally calls HandleCLI(), so -install / -uninstall / -start /
/// -stop / -status / --help are automatically recognised and dispatched
/// before fnMain runs. Applications that need finer control over that
/// handshake may call HandleCLI() themselves instead (both public).
///
/// Platform behaviour:
///   - Windows: uses SCM; install writes to the service registry; state is
///     reported via SetServiceStatus. An internal --service flag is injected
///     into the registered binPath and filtered back out of argv.
///     IMPORTANT — do NOT install the binary from a user-profile path
///     (e.g. `C:\Users\<name>\...`). The SCM stores the absolute binPath
///     and launches the service under LocalSystem (or the configured
///     account), independent of any interactive session. When an RDP /
///     console user logs off, Windows unloads that user's profile; if the
///     service image or any file it has open lives under the profile, the
///     unload can tear the process down with exit code 1067
///     (ERROR_PROCESS_ABORTED) or fast-fail 0xc0000409. Install from a
///     machine-wide location such as `C:\Program Files\<app>\` and keep
///     runtime state under `C:\ProgramData\<app>\` or the Event Log.
///   - Linux:   uses systemd; install writes a .service unit under
///     /etc/systemd/system (or ~/.config/systemd/user for user-scope),
///     calls `systemctl daemon-reload` + `systemctl enable`. Run() sends
///     `sd_notify` READY=1 / STOPPING=1 when launched by systemd (detected
///     via the INVOCATION_ID environment variable). Interactive launches
///     are transparent passthroughs — no notify, no signal changes.
///   - macOS:   uses launchd; install writes a .plist under
///     /Library/LaunchDaemons (or ~/Library/LaunchAgents for user-scope),
///     calls `launchctl load -w`. Service names without a dot are
///     auto-prefixed to `org.dekaf2.<name>` to follow Apple's reverse-DNS
///     label convention. launchd has no ready-notification protocol, so
///     Run() only marks the process as in-service and calls fnMain.
class DEKAF2_PUBLIC KService
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Signature of the user's real main function.
	using MainFunc = std::function<int(int argc, char** argv)>;

	// ========================================================================
	//   PRIMARY ENTRY POINT — use this from main() as the all-in-one method
	// ========================================================================

	//-----------------------------------------------------------------------------
	/// One-stop entry point for services. Call this from your main() instead
	/// of calling the application logic directly. Run() performs, in order:
	///
	///   1. A call to HandleCLI() on the current argv. If any of
	///      -install / -uninstall / -start / -stop / -status is present,
	///      the operation is executed and Run() returns its exit code
	///      immediately — fnMain is NOT invoked. If -h / -help / --help
	///      is present, Run() prints the service-management help block
	///      and then continues normally (so fnMain's own help handler
	///      runs afterwards; "stacked help").
	///   2. Platform-specific service-runtime setup:
	///        - Windows: engages the SCM service dispatcher when the
	///          process was launched by the Service Control Manager,
	///          reports RUNNING once fnMain starts, and routes
	///          SERVICE_CONTROL_STOP / SHUTDOWN into the registered
	///          shutdown handler (or raises SIGTERM via KSignals if no
	///          handler was set).
	///        - Linux:  sends sd_notify READY=1 / STOPPING=1 around
	///          fnMain when INVOCATION_ID is set by systemd.
	///        - macOS:  marks the process as a launchd service when
	///          getppid() == 1, then calls fnMain transparently.
	///   3. Invokes fnMain with the argv the user saw (on Windows, the
	///      internal --service flag is filtered out before the call).
	///
	/// Interactive launches on any platform simply fall through to fnMain.
	///
	/// @param sServiceName the internal service name (no spaces).
	/// @param argc argc as received by main().
	/// @param argv argv as received by main().
	/// @param fnMain the user's real main function. Must return 0 on success.
	/// @param bSuppressHelp if true, no help text will be output even when
	/// the -help option was seen - useful to delegate help output to an overall
	/// help function. Defaults to false : help will be output for SCM specific options.
	/// @param sDisplayName optional display name used in help / install
	/// (Windows services.msc Display Name, systemd Description= fallback,
	/// launchd Label suffix). Defaults to sServiceName.
	/// @param sDescription optional free-form description used in help /
	/// install (Windows services.msc description, systemd Description=).
	/// @return fnMain's exit code, or HandleCLI's exit code if a
	/// management flag was handled.
	static int Run(KStringView sServiceName,
	               int         argc,
	               char**      argv,
	               MainFunc    fnMain,
	               bool        bSuppressHelp = false,
	               KStringView sDisplayName  = KStringView{},
	               KStringView sDescription  = KStringView{});
	//-----------------------------------------------------------------------------

	// ========================================================================
	//   RUNTIME HELPERS — callable from inside fnMain (or anywhere the
	//   process may be running as a service). They complement Run().
	// ========================================================================

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

	/// Service state reported to the Service Control Manager.
	/// Used by ReportState() and returned by Status().
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

	// ========================================================================
	//   LOW-LEVEL BUILDING BLOCKS — alternative to Run(). Use these only if
	//   you need finer control than Run() provides (e.g. dispatch -install
	//   yourself, embed the CLI handler inside a larger custom parser, or
	//   manage another process's service from the outside). Typical code
	//   does NOT need to touch anything below this line.
	// ========================================================================

	//-----------------------------------------------------------------------------
	/// Inspect argv directly for -install / -uninstall / -start / -stop /
	/// -status flags. If any is found, perform the requested operation and
	/// return true. The caller is expected to exit immediately when true is
	/// returned; the process exit code is set via the returned reference.
	///
	/// Does not depend on any CLI parser and is therefore compatible with
	/// applications that rely on KOptions' ad-hoc help-collection pattern.
	///
	/// Run() calls this internally as its first step; applications that use
	/// Run() never need to call HandleCLI() explicitly.
	///
	/// HELP HANDLING: if -h / -help / --help / -? is present on the command
	/// line (and no service-control flag is), HandleCLI() prints the
	/// service-management help block on stdout and returns false — so that
	/// the caller's own CLI parser (KOptions or otherwise) can still print
	/// its own help text and decide when to exit. This is the standard
	/// "stacked help" pattern used by dekaf2-utests and Catch.
	///
	/// RUNTIME ARGUMENTS: on -install, every argv element that is not the
	/// service control flag itself is captured and persisted into the
	/// generated unit / plist. Example:
	///   mysvc -install -p 1234 -f 2345 -s secret -t localhost:4949
	/// produces a service that, on every launch, is started with exactly
	/// those arguments by the platform service manager:
	///   - Windows: appended to the SCM binPath (before the internal
	///     --service flag)
	///   - Linux:   written verbatim into ExecStart= of the systemd unit
	///   - macOS:   each argv element becomes one entry in the launchd
	///     ProgramArguments array (space-split on install; use "quoted"
	///     values if an argument needs to contain a literal space)
	///
	/// OUTPUT CONVENTIONS (identical on all three platforms):
	///   - success confirmations go to stdout ("service 'x' installed" etc.)
	///   - failures go to stderr so `2>/dev/null` keeps a successful run
	///     completely silent but still surfaces errors
	///   - the non-root user-scope fallback notice (Linux / macOS) goes to
	///     stderr as it is a warning, not an error
	///
	/// Typical usage at the very top of main() when NOT using Run():
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
	/// @param bSuppressHelp if true, no help text will be output even when
	/// the -help option was seen - useful to delegate help output to an overall
	/// help function. Defaults to false : help will be output for SCM specific options.
	/// @param sDisplayName optional display name (defaults to sServiceName).
	/// @param sDescription optional free-form description.
	/// @return true if a service management flag was recognised and handled.
	static bool HandleCLI(int           argc,
	                      char**        argv,
	                      KStringView   sServiceName,
	                      int&          iExitCodeOut,
	                      bool          bSuppressHelp = false,
	                      KStringView   sDisplayName  = KStringView{},
	                      KStringView   sDescription  = KStringView{});
	//-----------------------------------------------------------------------------

	/// How the service is started by the platform service manager after
	/// reboot. Used in InstallOptions::Mode.
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
		/// Windows: avoid user-profile paths (`C:\Users\...`). The SCM
		/// records this path verbatim and launches the service under
		/// LocalSystem regardless of who is logged on; when the interactive
		/// user logs off (including RDP disconnect), Windows unloads their
		/// profile and can terminate the running service image with exit
		/// code 1067 (ERROR_PROCESS_ABORTED) or fast-fail 0xc0000409.
		/// Install the binary from a machine-wide location such as
		/// `C:\Program Files\<app>\` instead. Linux / macOS: any path
		/// readable by the target account is fine.
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
		/// Linux / macOS: if true, install as a user-scoped unit.
		/// Linux  paths: `~/.config/systemd/user/<name>.service`
		/// macOS  paths: `~/Library/LaunchAgents/<label>.plist`
		/// If false (default) and the process runs as root, install
		/// system-wide (Linux: `/etc/systemd/system/`, macOS:
		/// `/Library/LaunchDaemons/`). If false but the process is non-root,
		/// Install() automatically falls back to user scope with a stderr
		/// hint — so `./mysvc -install` without sudo always produces a
		/// functional user-level install. Ignored on Windows.
		bool      bUserScope;

		// user-declared default constructor so this struct can be used as a
		// default-argument value without triggering Clang's lazy-parsing issue
		// with in-class member initialisers
		InstallOptions() : Mode(StartMode::Automatic), bUserScope(false) {}
	};

	//-----------------------------------------------------------------------------
	/// Register the current (or a given) executable as a system service.
	/// Requires administrator / root privileges for a system-wide install;
	/// falls back to a user-scoped unit on Linux / macOS otherwise.
	///
	/// START BEHAVIOUR (identical on all three platforms):
	/// Install() only *registers* the service; it never leaves it running.
	/// With Mode=Automatic the service is marked for auto-start at next
	/// reboot / login, but must be started for the current session via an
	/// explicit Start() call (or `-start` on the CLI). This keeps
	/// Windows / Linux / macOS behaviour uniform — historically macOS
	/// started the job implicitly via `launchctl load -w`, which we now
	/// suppress.
	///
	/// @param sServiceName the internal service name (no spaces).
	/// @param Opts install parameters.
	/// @return true on success.
	static bool Install(KStringView sServiceName, const InstallOptions& Opts = InstallOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Unregister a previously installed service. Requires administrator /
	/// root privileges for a system-wide install. Falls back to user scope
	/// on Linux / macOS when not privileged.
	/// @param sServiceName the internal service name (no spaces).
	static bool Uninstall(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ask the platform service manager to start an installed service.
	static bool Start(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ask the platform service manager to stop a running service.
	static bool Stop(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Query current service state from the platform service manager.
	/// Returns State::Stopped when the service is not installed / not
	/// reachable.
	static State Status(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// True if a service with this name is registered with the platform
	/// service manager.
	static bool IsInstalled(KStringView sServiceName);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns a help section explaining the service options, normally output by Run()/HandleCLI()
	/// automatically when a help option was detected
	static KStringViewZ GetHelp();
	//-----------------------------------------------------------------------------

}; // KService

/// @}

DEKAF2_NAMESPACE_END
