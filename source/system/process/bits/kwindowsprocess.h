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

/// @file kwindowsprocess.h
/// starts and controls a child process on Windows

#include <dekaf2/core/init/kdefinitions.h>

#ifdef DEKAF2_IS_WINDOWS

#include <dekaf2/core/init/kcompatibility.h> // pid_t on Windows
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/duration/kduration.h>
#include <cstdint>
#include <utility>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Starts and controls a child process on Windows, for KBaseProcess and KChildProcess.
/// The child runs in a job, which holds it and all processes it starts: Terminate()
/// ends all of them.
class DEKAF2_PUBLIC KWindowsProcess
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KWindowsProcess() = default;
	KWindowsProcess(const KWindowsProcess&) = delete;
	KWindowsProcess& operator=(const KWindowsProcess&) = delete;
	KWindowsProcess(KWindowsProcess&& other) noexcept;
	KWindowsProcess& operator=(KWindowsProcess&& other) noexcept;

	//-----------------------------------------------------------------------------
	/// Closes the handles of the child, but does not terminate it
	~KWindowsProcess();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Starts a child. A child started before must have been released.
	/// @param sCommandLine the command line, see CommandLine() and ShellCommandLine()
	/// @param hStdIn handle for the stdin of the child, which this method closes - nullptr
	/// for the stdin of this process, or for a detached child the NUL device
	/// @param hStdOut handle for the stdout of the child, as hStdIn
	/// @param hStdErr handle for the stderr of the child, as hStdIn
	/// @param Environment pairs of names and values that will be added to the environment
	/// of the child, an empty value removes a variable
	/// @param sWorkingDirectory the working directory of the child, empty for the one of
	/// this process
	/// @param bDetached true to start the child without a console and outside of a job,
	/// so that it outlives this process
	/// @return 0, or the Win32 error code
	uint32_t Start(KStringView  sCommandLine,
	               void*        hStdIn,
	               void*        hStdOut,
	               void*        hStdErr,
	               const std::vector<std::pair<KString, KString>>& Environment = {},
	               KStringViewZ sWorkingDirectory = KStringViewZ{},
	               bool         bDetached = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true from Start() until Wait() finds the child ended, or Release()
	bool IsStarted() const
	//-----------------------------------------------------------------------------
	{
		return m_hProcess != nullptr;
	}

	//-----------------------------------------------------------------------------
	/// Waits up to Timeout for the end of the child. Once it has ended, its exit code
	/// is read and its process handle is closed - the job stays for Terminate(), which
	/// still reaches the processes the child started, until Release().
	/// @return true if the child has ended, or was not started
	bool Wait(KDuration Timeout = KDuration::max());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns the exit code of the child once Wait() found it ended, else 0
	int GetExitCode() const
	//-----------------------------------------------------------------------------
	{
		return m_iExitCode;
	}

	//-----------------------------------------------------------------------------
	/// Returns the process ID of the child, 0 if it was not started or has ended
	pid_t GetProcessID() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Terminates the child together with all processes it started, with the exit
	/// code -1. The termination completes asynchronously.
	/// @return false if the child could not be terminated
	bool Terminate();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes the handles of the child and its job, without terminating them
	void Release();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns the command line for a program and its arguments, quoted so that the
	/// program gets them unchanged as its argv
	/// @param Args the program (first element, searched in the PATH) and its arguments
	static KString CommandLine(const std::vector<KString>& Args);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns the command line that executes sCommand with the command interpreter
	/// (%COMSPEC%, else cmd.exe from the system directory)
	static KString ShellCommandLine(KStringView sCommand);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns the text for a Win32 error code
	static KString ErrorText(uint32_t iError);
	//-----------------------------------------------------------------------------

//------
private:
//------

	void* m_hProcess  { nullptr }; // HANDLE of the child process
	void* m_hJob      { nullptr }; // HANDLE of the job with the child and the processes it starts
	int   m_iExitCode { 0 };

}; // KWindowsProcess

DEKAF2_NAMESPACE_END

#endif // DEKAF2_IS_WINDOWS
