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

/// @file kprocessgroup.h
/// the process group of started children

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/init/kcompatibility.h> // pid_t on Windows
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup system_process
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The process group of a started child (KChildProcess, KInPipe, KOutPipe, KPipe,
/// KInShell, KOutShell).
///
/// In an own group, Stop(), Kill() and Close() with a timeout reach the child together
/// with all processes it started, like those a shell runs. The terminal however treats
/// the child as a process of its own: it does not get Ctrl-C, and when it reads from the
/// controlling terminal - as ssh, sudo and git do through /dev/tty to ask for a password,
/// also when their input is a pipe - the kernel stops it.
///
/// In the group of this process, the terminal treats the child as part of this process,
/// but Stop(), Kill() and Close() reach only the child itself.
///
/// On Windows the mode has no effect: there a child always runs in a job together with
/// all processes it starts. Stop(), Kill(), Terminate() and Close() with a timeout end
/// all of them, and Ctrl-C of the console reaches all of them.
///
/// The static methods are the steps of the process classes around fork().
class DEKAF2_PUBLIC KProcessGroup
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// the choice of the process group for a child
	enum Mode : uint8_t
	{
		Auto,   ///< an own group if this process has no controlling terminal (a service, cron), else the group of this process
		Own,    ///< an own group - with a controlling terminal, the signal handler thread of KInit forwards Ctrl-C, Ctrl-\ and a hangup to it
		Shared  ///< the group of this process
	};

#ifndef DEKAF2_IS_WINDOWS

	//-----------------------------------------------------------------------------
	/// In the parent before fork(): returns true if the child shall get an own process group
	static bool UseOwn(Mode Group);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// In the child after fork(): makes it the leader of an own process group
	static void EnterOwn();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// In the parent after fork(): makes the child the leader of an own process group,
	/// as the child does itself - whichever comes first, the group exists before the
	/// parent signals it. With a controlling terminal, the signals of the terminal are
	/// forwarded to the group.
	static void AdoptChild(pid_t pid);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// After the child was reaped: ends the forwarding of signals to its process group
	static void ReleaseChild(pid_t pid);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Sends iSignal to the child, or to its whole process group if it has an own one
	/// @return 0 on success, -1 on error (see errno)
	static int SignalChild(pid_t pid, bool bOwnProcessGroup, int iSignal);
	//-----------------------------------------------------------------------------

#endif // !DEKAF2_IS_WINDOWS

}; // KProcessGroup

/// @}

DEKAF2_NAMESPACE_END
