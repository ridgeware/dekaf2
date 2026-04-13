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

#ifdef DEKAF2_HAS_PIPES

#include <sys/wait.h>

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
		return m_pid;
	}

//--------
protected:
//--------

	pid_t m_pid       { 0 };
	int   m_iExitCode { 0 };

	//-----------------------------------------------------------------------------
	void wait(bool bNoHang = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Wait for the child to terminate, or kill it after timeout.
	/// Called by derived Close() implementations after their FD cleanup.
	/// @param Timeout KDuration::max() = wait forever, else wait then SIGKILL
	void WaitOrKill(KDuration Timeout = KDuration::max());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static void CloseAndResetFileDescriptor(int& iFileDescriptor);
	//-----------------------------------------------------------------------------

}; // KBaseProcess

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_PIPES
