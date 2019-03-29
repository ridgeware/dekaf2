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

#include "kbasepipe.h"

#ifdef DEKAF2_HAS_PIPES

#include "../ksplit.h"
#include "../ksystem.h"
#include "../ksignals.h"
#include "../kchildprocess.h"
#include "../klog.h"
#include <signal.h>

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool KBasePipe::Open(KString sCommand, bool bAsShellCommand, int mode)
//-----------------------------------------------------------------------------
{
	Close(mode); // ensure a previous pipe is closed

	if (m_pid)
	{
		kWarning("cannot open pipe '{}': {}", sCommand, "old one still running");
		return false;
	}

	m_iExitCode = 0;

	sCommand.TrimLeft();

	if (sCommand.empty())
	{
		m_iExitCode = EINVAL;
		return false;
	}

	if (mode & PipeRead)
	{
		if (pipe(m_readPdes) < 0)
		{
			kWarning("cannot open input pipe '{}': {}", sCommand, strerror(errno));
			return false;
		}
	}

	if (mode & PipeWrite)
	{
		if (pipe(m_writePdes) < 0)
		{
			kWarning("cannot open output pipe '{}': {}", sCommand, strerror(errno));
			return false;
		}
	}

	// we need to do the object allocations in the parent
	// process as otherwise leak detectors would claim the
	// child has lost allocated memory (as the child would
	// never run the destructor)

	std::vector<const char*> argV;

	if (bAsShellCommand)
	{
		argV.push_back("/bin/sh");
		argV.push_back("-c");
		argV.push_back(sCommand.c_str());
	}
	else
	{
		kSplitArgsInPlace(argV, sCommand);
	}

	// terminate with nullptr
	argV.push_back(nullptr);

	// create a child
	switch (m_pid = fork())
	{
		case -1: /* error */
		{
			// could not create the child
			if (mode & PipeRead)
			{
				::close(m_readPdes[0]);
				::close(m_readPdes[1]);
			}
			if (mode & PipeWrite)
			{
				::close(m_writePdes[0]);
				::close(m_writePdes[1]);
			}
			m_pid = 0;
			break;
		}

		case 0: /* child */
		{
			detail::kCloseOwnFilesForExec(false, m_readPdes, 4);

			if (mode & PipeWrite)
			{
				// Bind to Child's stdin
				::close(m_writePdes[1]);
				if (m_writePdes[0] != fileno(stdin))
				{
					::dup2(m_writePdes[0], fileno(stdin));
					::close(m_writePdes[0]);
				}
			}

			if (mode & PipeRead)
			{
				// Bind Child's stdout
				::close(m_readPdes[0]);
				if (m_readPdes[1] != fileno(stdout))
				{
					::dup2(m_readPdes[1], fileno(stdout));
					::close(m_readPdes[1]);
				}
			}

			// execute the command
			execvp(argV[0], const_cast<char* const*>(argV.data()));

			_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
		}

	}

	// only parent gets here

	if (mode & PipeRead)
	{
		::close(m_readPdes[1]); // close write end of read pipe (for child use)
	}

	if (mode & PipeWrite)
	{
		::close(m_writePdes[0]); // close read end of write pipe (for child use)
	}

	return true;

} // Open

//-----------------------------------------------------------------------------
int KBasePipe::Close(int mode)
//-----------------------------------------------------------------------------
{
	if (m_pid > 0)
	{
		if (mode & PipeRead)
		{
			// Close read on stdout pipe
			::close(m_readPdes[0]);
		}

		if (mode & PipeWrite)
		{
			// Send EOF by closing write end of pipe
			::close(m_writePdes[1]);
		}

		// Child has been cut off from parent, let it terminate
		Wait(60000);

		// Did the child terminate properly?
		if (IsRunning())
		{
			// no
			kill(m_pid, SIGKILL);
			m_iExitCode = -1;
		}

		m_pid = 0;
		m_writePdes[0] = -1;
		m_writePdes[1] = -1;
		m_readPdes[0] = -1;
		m_readPdes[1] = -1;
	}

	return m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
void KBasePipe::wait()
//-----------------------------------------------------------------------------
{
	if (m_pid)
	{
		int iStatus { 0 };

		pid_t iPid = waitpid(m_pid, &iStatus, WNOHANG);

		if (iPid == -1)
		{
			m_iExitCode = errno;
			kDebug(1, "cannot close pipe: {}", strerror(errno));
			m_pid = 0;
		}
		else if (iPid == m_pid)
		{
			if (WIFEXITED(iStatus))
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
			else
			{
				m_iExitCode = iStatus;
				kDebug(1, "exited with invalid status {}", iStatus);
			}

			m_pid = 0;
		}
		// if iPid == 0 the process is still running
	}

} // wait

//-----------------------------------------------------------------------------
bool KBasePipe::IsRunning()
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return false;
	}

	wait();

	return (m_pid > 0);

} // IsRunning

//-----------------------------------------------------------------------------
bool KBasePipe::Wait(int msecs)
//-----------------------------------------------------------------------------
{
	if (msecs >= 0)
	{
		int counter = 0;
		while (IsRunning())
		{
			kMilliSleep(1);
			++counter;

			if (counter == msecs)
            {
				return false;
            }
		}
		return true;
	}
	return false;

} // Wait

} // end namespace dekaf2

#endif
