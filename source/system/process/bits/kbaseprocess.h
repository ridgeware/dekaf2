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
*/

#pragma once

/// @file kbaseprocess.h
/// common base class for child process management (shared by KBasePipe and KPTY)

#include <dekaf2/core/init/kcompatibility.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/system/process/kprocessgroup.h>

#ifdef DEKAF2_HAS_PIPES

#ifdef DEKAF2_IS_WINDOWS
	#include <dekaf2/system/process/bits/kwindowsprocess.h>
#else
	#include <sys/wait.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Common base class for child process management. Provides process lifecycle
/// methods (wait, kill, status) shared by KBasePipe and KPTY.
class DEKAF2_PUBLIC KBaseProcess
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Checks if child process is still running
	bool IsRunning();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Waits up to Timeout for the child to terminate.
	/// Will return early if child terminates
	bool Wait(KDuration Timeout);
	//-----------------------------------------------------------------------------

#ifndef DEKAF2_IS_WINDOWS
	//-----------------------------------------------------------------------------
	/// Sends iSignal to the child - in an own process group to all processes of the group
	/// @return true if the signal was sent, or the child has ended already
	bool SendSignal(int iSignal);
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// Terminates the child without waiting for its end - in an own process group, and
	/// on Windows always, together with all processes it started. The pipes stay open:
	/// the output written so far can still be read, and Close() returns the exit code -1.
	/// @return true if the child was terminated, or has ended already
	bool Terminate();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get exit code, 0 indicates no errors
	int GetExitCode()
	//-----------------------------------------------------------------------------
	{
		return m_iExitCode;
	}

	//-----------------------------------------------------------------------------
	/// Get process ID of the running process, > 0 if running, else not running
	pid_t GetProcessID()
	//-----------------------------------------------------------------------------
	{
#ifdef DEKAF2_IS_WINDOWS
		return m_Process.GetProcessID();
#else
		return m_pid;
#endif
	}

//--------
protected:
//--------

#ifdef DEKAF2_IS_WINDOWS
	KWindowsProcess m_Process;
	int   m_iExitCode        { 0 };
#else
	pid_t m_pid              { 0 };
	int   m_iExitCode        { 0 };
	bool  m_bOwnProcessGroup { false }; // the child leads an own process group

	//-----------------------------------------------------------------------------
	void wait(bool bNoHang = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the child was reaped - signals are not forwarded to its process group anymore
	void ReleaseProcessGroup();
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// Wait for the child to terminate, or kill it after timeout.
	/// Called by derived Close() implementations after their FD cleanup.
	/// @param Timeout KDuration::max() = wait forever, else wait then SIGKILL (on
	/// Windows: terminate the job of the child)
	void WaitOrKill(KDuration Timeout = KDuration::max());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static void CloseAndResetFileDescriptor(int& iFileDescriptor);
	//-----------------------------------------------------------------------------

}; // KBaseProcess

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_PIPES
