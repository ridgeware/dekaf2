/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

/// @file ksystem.h
/// general system utilities for dekaf2

#include <cstdlib>
#include <chrono>
#include "kstring.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p szDefault if not found.
inline KStringViewZ kGetEnv (KStringViewZ szEnvVar, KStringViewZ szDefault = "")
//-----------------------------------------------------------------------------
{
	KStringViewZ sValue = ::getenv(szEnvVar);
	if (!sValue.empty())
	{
		return (sValue);
	}
	else
	{
		return (szDefault);
	}

} // kGetEnv

//-----------------------------------------------------------------------------
/// Set environment variable.
inline bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ sValue)
//-----------------------------------------------------------------------------
{
	bool bOK = (::setenv(szEnvVar, sValue, true) == 0);
	if (!bOK)
	{
		kWarning("cannot set {} = {}, {}", szEnvVar, sValue, strerror(errno));
	}
	return (bOK);
}

//-----------------------------------------------------------------------------
/// Unset environment variable.
inline bool kUnsetEnv (KStringViewZ szEnvVar)
//-----------------------------------------------------------------------------
{
	bool bOK = (::unsetenv(szEnvVar) == 0);
	if (!bOK)
	{
		kWarning("cannot unset {}, {}", szEnvVar, strerror(errno));
	}
	return (bOK);
}

/// Return operating system current working directory as a string.
KString kGetCWD ();

/// alias to kGetCWD(): return operating system current working directory as a string.
inline KString kcwd () { return kGetCWD(); }

/// Return current operating system user as a string.
KString kGetWhoAmI ();

/// alias to kGetWhoami(): return current operating system user as a string.
inline KString kwhoami () { return kGetWhoAmI(); }

/// Return operating system hostname as a string.
KString kGetHostname ();

/// Alias to kGetHostname(): return operating system hostname as a string.
inline KString khostname () { return kGetHostname(); }

/// return process ID
inline pid_t kGetPid()
{
	return getpid();
}

/// return thread ID
uint64_t kGetTid();

namespace detail {

/// Send this process into the background and detach from terminals. If bChangeDir
/// is true, the child will chdir to /. The umask of the child is set to 022.
/// WARNING: In a dekaf2 governed executable, this function should NOT be called
/// by anyone other than the Dekaf() class itself. As a user, call Dekaf().Daemonize()
/// as early in the process as possible, best before any file and thread based
/// activity. And best do not use it at all but let instead systemd do the work for you.
void kDaemonize(bool bChangeDir = false);

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Start and control a child process
class KChildProcess
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// default ctor
	KChildProcess() = default;
	KChildProcess(const KChildProcess&) = delete;
	KChildProcess(KChildProcess&&) = default;
	KChildProcess& operator=(const KChildProcess&) = delete;
	KChildProcess& operator=(KChildProcess&&) = default;

	/// ctor with arguments to start child
	KChildProcess(KStringView sCommand,
				  KStringViewZ sChangeDirectory = KStringViewZ{},
				  bool bDaemonized = false)
	{
		Start(sCommand, sChangeDirectory, bDaemonized);
	}

	~KChildProcess();

	/// Start a child with sCommand, change to sChangeDirectory, and detach
	/// from terminal if bDaemonized is true
	bool Start(KStringView sCommand,
			   KStringViewZ sChangeDirectory = KStringViewZ{},
			   bool bDaemonized = false);

	/// Detach child so that it will not be killed when this class is destructed.
	/// Only works for daemonized childs
	bool Detach();

	/// Join a started child, wait max for Timeout
	bool Join(std::chrono::nanoseconds Timeout = std::chrono::nanoseconds(0));

	/// Stop a started child with SIGTERM, wait max for Timeout
	bool Stop(std::chrono::nanoseconds Timeout = std::chrono::nanoseconds(0));

	/// Kill a started child with SIGHUP
	bool Kill();

	/// Check if a child is started
	bool IsStarted() const { return m_child != 0; }

	/// Check if a child is stopped
	bool IsStopped() const { return !IsStarted(); }

	/// Returns the process ID of a started child
	pid_t GetChildPID() const { return m_child; }

	/// Returns error string
	const KString& Error() const { return m_sError; }

//------
protected:
//------

	bool SetError(KStringView sError);

	pid_t m_child { 0 };
	bool m_bIsDaemonized { false };
	KString m_sError;

}; // KChildProcess

} // end of namespace dekaf2

