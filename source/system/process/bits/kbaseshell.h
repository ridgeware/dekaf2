/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/
#pragma once

/// @file kbaseshell.h
/// basic shell I/O class

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/init/kcompatibility.h> // pid_t on Windows
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/core/strings/kstring.h>
#include <cstdio>

#ifdef DEKAF2_IS_WINDOWS

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KBaseShell
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Checks if child on other side of pipe is still running
	bool IsRunning();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get process ID of the running child, > 0 if running, else not running
	pid_t GetProcessID() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Default Constructor
	KBaseShell() = default;
	//-----------------------------------------------------------------------------

	KBaseShell(const KBaseShell&) = delete;
	KBaseShell& operator=(const KBaseShell&) = delete;

	//-----------------------------------------------------------------------------
	/// Destructor
	~KBaseShell();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes the pipe, waits up to Timeout for the child to end and returns its
	/// exit code. If the child did not end within Timeout, it is terminated together
	/// with all processes it started, and the exit code is -1
	int Close(KDuration Timeout = KDuration::max());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Terminates the child together with all processes it started, but leaves the
	/// pipe open: the output written so far can still be read, and Close() returns
	/// the exit code -1. The termination completes asynchronously.
	/// @return false if the child could not be terminated
	bool Terminate();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get error code, 0 indicates no errors
	int GetErrno()
	//-----------------------------------------------------------------------------
	{
		return m_iExitCode;
	}

//--------
protected:
//--------

	//-----------------------------------------------------------------------------
	/// Executes given command with a pipe to its stdin (bWrite) or from its stdout,
	/// saving the FILE* of the pipe in m_pipe
	/// @param sCommand the command to execute
	/// @param bWrite true to write to the stdin of the command, false to read its stdout
	/// @param bUseShell true to execute the command with the command interpreter
	/// (%COMSPEC%, cmd.exe), false to execute it directly
	/// @param Environment pairs of names and values that will be added to the
	/// environment of the child, an empty value removes a variable
	bool IntOpen(KString sCommand, bool bWrite, bool bUseShell = true,
	             const std::vector<std::pair<KString, KString>>& Environment = {});
	//-----------------------------------------------------------------------------

	FILE* m_pipe      { nullptr };
	void* m_hProcess  { nullptr }; // HANDLE of the child process
	void* m_hJob      { nullptr }; // HANDLE of the job with the child and the processes it starts
	int   m_iExitCode { 0 };


}; // KBaseShell

DEKAF2_NAMESPACE_END

#endif // DEKAF2_IS_WINDOWS
