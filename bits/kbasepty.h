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

/// @file kbasepty.h
/// base class for pseudo-terminal (PTY) I/O

#include "../kcompatibility.h"
#include "../kstring.h"

#ifdef DEKAF2_HAS_PIPES

#include <sys/wait.h>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KBasePTY
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum LoginMode
	{
		NoLogin,       ///< spawn a shell without login
		Login          ///< spawn login, expects credentials via stream
	};

	//-----------------------------------------------------------------------------
	/// Checks if child on other side of PTY is still running
	bool IsRunning();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Waits up to the number of given milliseconds for the child to terminate.
	/// Will return early if child terminates
	bool Wait(int msecs);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get error code, 0 indicates no errors
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

	//-----------------------------------------------------------------------------
	/// Terminate the running process. Initially with signal SIGINT, after msecs
	/// waiting time with SIGKILL
	bool Kill(int msecs);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the PTY window size. Sends SIGWINCH to the child process.
	bool SetWindowSize(uint16_t iRows, uint16_t iCols);
	//-----------------------------------------------------------------------------

//--------
protected:
//--------

	int     m_iMasterFD  { -1 };
	pid_t   m_pid        {  0 };
	int     m_iExitCode  {  0 };
	KString m_sSlaveName;

	//-----------------------------------------------------------------------------
	void wait(bool bNoHang = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Open a PTY and fork a child process.
	/// @param sShell shell path (or login command for Login mode). If empty, uses
	///        $SHELL or /bin/sh for NoLogin, /usr/bin/login for Login.
	/// @param Mode NoLogin or Login
	/// @param Environment additional environment variables for the child
	bool Open(KStringView sShell, LoginMode Mode,
			  const std::vector<std::pair<KString, KString>>& Environment);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Close the PTY and wait for the child to terminate.
	/// @param iWaitMilliseconds waits for amount of milliseconds, then kills child
	///        process. Default is -1, which will wait until child terminates.
	/// @return the exit code received from the child
	int Close(int iWaitMilliseconds = -1);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static void CloseAndResetFileDescriptor(int& iFileDescriptor);
	//-----------------------------------------------------------------------------

}; // KBasePTY

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_PIPES
