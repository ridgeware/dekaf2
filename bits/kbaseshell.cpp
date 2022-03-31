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

#include "kbaseshell.h"

#ifndef DEKAF2_IS_UNIX

#include "../klog.h"
#include "../ksignals.h"
#include <errno.h>

#ifndef DEKAF2_IS_WINDOWS
	#include <sys/wait.h>
#endif


namespace dekaf2
{

//-----------------------------------------------------------------------------
KBaseShell::~KBaseShell()
//-----------------------------------------------------------------------------
{
	Close();
}

//-----------------------------------------------------------------------------
bool KBaseShell::IsRunning()
//-----------------------------------------------------------------------------
{
	return m_pipe != nullptr;
}

//-----------------------------------------------------------------------------
bool KBaseShell::IntOpen (KString sCommand, bool bWrite,
						  const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	Close(); // ensure a previous pipe is closed

	m_iExitCode = 0;

	sCommand.TrimLeft();

	if (sCommand.empty())
	{
		m_iExitCode = EINVAL;
		return false;
	}

	if (!Environment.empty())
	{
		kDebug(1, "cannot set given environment values in child process when executing command with popen");
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
#if defined(DEKAF2_IS_UNIX) && !defined(DEKAF2_IS_OSX)
	// for a strange reason Apple has the CLOSE_ON_EXEC flag for fopen(), but not for popen()..
	m_pipe = popen(sCommand.c_str(), bWrite ? "we" : "re");
#else
	m_pipe = popen(sCommand.c_str(), bWrite ? "w" : "r");
#endif

	if (!m_pipe)
	{
		kDebug (0, "popen() failed: {} - error: {}", sCommand, strerror(errno));
		m_iExitCode = errno;
		return false;
	}

	return true;

} // IntOpen

//-----------------------------------------------------------------------------
int KBaseShell::Close(int milliseconds /* unused */)
//-----------------------------------------------------------------------------
{
	if (m_pipe)
	{
		// the return value is the exit status of the command as returned by wait4
		int iStatus = pclose(m_pipe);

		if (iStatus == -1)
		{
			m_iExitCode = errno;
			kDebug(1, "cannot close pipe: {}", strerror(errno));
		}
		else if (WIFEXITED(iStatus))
		{
			m_iExitCode = WEXITSTATUS(iStatus);
			kDebug(2, "exited with return value {}", m_iExitCode);
		}
		else if (WIFSIGNALED(iStatus))
		{
			m_iExitCode = 1;
			int iSignal = WTERMSIG(iStatus);
			if (iSignal)
			{
				kDebug(1, "aborted by signal {}", kTranslateSignal(iSignal));
			}
		}

		m_pipe = nullptr;
	}

	return m_iExitCode;

} // Close

} // end of namespace dekaf2

#endif // of !DEKAF2_IS_UNIX
