/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kchildprocess.h
/// class and utilities to daemonize a process and to start and control child
/// processes

#include "kcompatibility.h"

#ifndef DEKAF2_IS_WINDOWS

#include "kduration.h"
#include "kstring.h"
#include "kerror.h"

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
/// Send this process into the background and detach from terminals. If bChangeDir
/// is true, the child will chdir to /. The umask of the child is set to 022.
/// WARNING: In a dekaf2 governed executable, this function should NOT be called
/// by anyone other than the Dekaf() class itself. As a user, call Dekaf().Daemonize()
/// as early in the process as possible, best before any file and thread based
/// activity. And best do not use it at all but let instead systemd do the work for you.
DEKAF2_PUBLIC
void kDaemonize(bool bChangeDir = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// On Unix, close all open file descriptors of this process to prepare for
/// an exec of a different process image. If bIncludeStandardIO is true, even
/// stdin, stdout and stderr are closed. We need this function as we have no
/// control over the open flags of std::fstream and of included libraries. This
/// function is to be called in a child after fork() and before exec(). Exempt
/// is an array of file descriptors of iExemptSize that shall not be closed.
DEKAF2_PUBLIC
void kCloseOwnFilesForExec(bool bIncludeStandardIO, int Exempt[] = nullptr, size_t iExemptSize = 0);
//-----------------------------------------------------------------------------

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Start and control a child process
class DEKAF2_PUBLIC KChildProcess : public KErrorBase
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
	KChildProcess(KString sCommand,
				  KStringViewZ sChangeDirectory = KStringViewZ{},
				  bool bDaemonized = false)
	{
		Start(std::move(sCommand), sChangeDirectory, bDaemonized);
	}

	~KChildProcess();

	/// Start a child with sCommand, change to sChangeDirectory, and detach
	/// from terminal if bDaemonized is true
	bool Start(KString sCommand,
			   KStringViewZ sChangeDirectory = KStringViewZ{},
			   bool bDaemonized = false);

	/// Fork the current process image, tell an entry function and pass (or leave default empty) argc, argv values
	bool Fork(int(*func)(int, char**), int argc = 0, char* argv[] = nullptr);

	/// Detach child so that it will not be killed when this class is destructed
	bool Detach();

	/// Join a started child, wait max for Timeout, 0 = forever (default)
	bool Join(KDuration Timeout = chrono::nanoseconds(0));

	/// Stop a started child with SIGTERM, wait max for Timeout, 0 = forever (default)
	bool Stop(KDuration Timeout = chrono::nanoseconds(0));

	/// Kill a started child with SIGHUP
	bool Kill();

	/// Check if a child is started
	bool IsStarted() const { return m_child != 0; }

	/// Check if a child is stopped
	bool IsStopped() const { return !IsStarted(); }

	/// Returns the process ID of a started child
	pid_t GetChildPID() const { return m_child; }

	/// Returns the exit status of a terminated child (or 0)
	int GetExitStatus() const { return m_iExitStatus; }

	/// Returns the exit signal of a terminated child (or 0)
	int GetExitSignal() const { return m_iExitSignal; }

//------
protected:
//------

	void Clear();

	pid_t   m_child         { 0 };
	int     m_iExitStatus   { 0 };
	int     m_iExitSignal   { 0 };
	bool    m_bIsDaemonized { false };

}; // KChildProcess

DEKAF2_NAMESPACE_END

#endif // of !DEKAF2_IS_WINDOWS
