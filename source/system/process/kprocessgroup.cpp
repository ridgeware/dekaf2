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

#include <dekaf2/system/process/kprocessgroup.h>

#ifndef DEKAF2_IS_WINDOWS

#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/logging/klog.h>
#include <atomic>
#include <unistd.h>
#include <signal.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KProcessGroup::UseOwn(Mode Group)
//-----------------------------------------------------------------------------
{
	switch (Group)
	{
		case Auto:
			// only with a controlling terminal there are Ctrl-C and a hangup, and a
			// child in another process group cannot read from the terminal
			return !kHasControllingTerminal();

		case Own:
			return true;

		case Shared:
			return false;
	}

	return false;

} // UseOwn

//-----------------------------------------------------------------------------
void KProcessGroup::EnterOwn()
//-----------------------------------------------------------------------------
{
	// fails only if the parent was faster and made us the leader already
	::setpgid(0, 0);

} // EnterOwn

//-----------------------------------------------------------------------------
void KProcessGroup::AdoptChild(pid_t pid)
//-----------------------------------------------------------------------------
{
	// fails if the child was faster, or has called exec() already - which it
	// does only after its own setpgid()
	::setpgid(pid, pid);

	if (kHasControllingTerminal())
	{
		KSignals::AddForwardedProcessGroup(pid);

		if (!KSignals::HasHandlerThread())
		{
			// once per process - every further child would only repeat it
			static std::atomic<bool> s_bWarned { false };

			if (!s_bWarned.exchange(true))
			{
				kWarning("child {} runs in an own process group, but without the signal handler thread "
				         "of KInit nothing forwards Ctrl-C, Ctrl-\\ and a hangup of the terminal to it - "
				         "when this process ends on Ctrl-C, the child keeps running. Use KInit(true), "
				         "or KProcessGroup::Shared", pid);
			}
		}
	}

} // AdoptChild

//-----------------------------------------------------------------------------
void KProcessGroup::ReleaseChild(pid_t pid)
//-----------------------------------------------------------------------------
{
	KSignals::RemoveForwardedProcessGroup(pid);

} // ReleaseChild

//-----------------------------------------------------------------------------
int KProcessGroup::SignalChild(pid_t pid, bool bOwnProcessGroup, int iSignal)
//-----------------------------------------------------------------------------
{
	// the negative ID addresses the whole process group, whose ID is the one
	// of its leader
	return ::kill(bOwnProcessGroup ? -pid : pid, iSignal);

} // SignalChild

DEKAF2_NAMESPACE_END

#endif // !DEKAF2_IS_WINDOWS
