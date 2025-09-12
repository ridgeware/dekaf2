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
#include "../kstring.h"
#include "../kformat.h"
#include "../kcompatibility.h"
#include <csignal>
#include <thread>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KBasePipe::Open(KString sCommand, KStringViewZ sShell, OpenMode Mode, const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	Close(); // ensure a previous pipe is closed

	if (m_pid)
	{
		kWarning("cannot open pipe '{}': {}", sCommand, "old one still running");
		return false;
	}

	m_Mode = Mode;

	m_iExitCode = 0;

	sCommand.TrimLeft();

	if (sCommand.empty())
	{
		m_iExitCode = EINVAL;
		return false;
	}

	if (m_Mode & PipeRead)
	{
		if (pipe(m_readPdes) < 0)
		{
			kWarning("cannot open input pipe '{}': {}", sCommand, strerror(errno));
			return false;
		}
	}

	if (m_Mode & PipeWrite)
	{
		if (pipe(m_writePdes) < 0)
		{
			kWarning("cannot open output pipe '{}': {}", sCommand, strerror(errno));
			return false;
		}
	}

	kDebug(2, "executing: {}", sCommand);

	// we need to do the object allocations in the parent
	// process as otherwise leak detectors would claim the
	// child has lost allocated memory (as the child would
	// never run the destructor)

	std::vector<const char*> argV;

	if (sShell.empty())
	{
		kSplitArgsInPlace(argV, sCommand);
	}
	else
	{
		argV.reserve(4);
		argV.push_back(sShell.c_str());
		argV.push_back("-c");
		argV.push_back(sCommand.c_str());
	}

	// terminate with nullptr
	argV.push_back(nullptr);

	// create a child
	switch (m_pid = fork())
	{
		case -1: /* error */
		{
			kWarning("cannot fork '{}': {}", sCommand, strerror(errno));

			// could not create the child
			if (m_Mode & PipeRead)
			{
				CloseAndResetFileDescriptor(m_readPdes[0]);
				// the other side gets closed anyway in the regular parent code
			}

			if (m_Mode & PipeWrite)
			{
				CloseAndResetFileDescriptor(m_writePdes[1]);
				// the other side gets closed anyway in the regular parent code
			}

			m_pid = 0;
			break;
		}

		case 0: /* child */
		{
			detail::kCloseOwnFilesForExec(false, m_readPdes, 4);

			// enable SIGPIPE!
			signal(SIGPIPE, SIG_DFL);

			if (m_Mode & PipeWrite)
			{
				// Bind to Child's stdin
				CloseAndResetFileDescriptor(m_writePdes[1]);
				if (m_writePdes[0] != fileno(stdin))
				{
					::dup2(m_writePdes[0], fileno(stdin));
					CloseAndResetFileDescriptor(m_writePdes[0]);
				}
			}

			if (m_Mode & PipeRead)
			{
				// Bind Child's stdout
				CloseAndResetFileDescriptor(m_readPdes[0]);
				if (m_readPdes[1] != fileno(stdout))
				{
					::dup2(m_readPdes[1], fileno(stdout));
					CloseAndResetFileDescriptor(m_readPdes[1]);
				}
			}

			// set additional environment variables. execvpe() is a GNU extension
			// and cannot be used on MacOS or Windows.
			// also note that we do not replace the parent's environment, instead
			// we inherit it
			kSetEnv(Environment);

			// execute the command
			execvp(argV[0], const_cast<char* const*>(argV.data()));

			_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
		}

	}

	// only parent gets here

	if (m_Mode & PipeRead)
	{
		// close write end of read pipe (for child use)
		CloseAndResetFileDescriptor(m_readPdes[1]);
	}

	if (m_Mode & PipeWrite)
	{
		// close read end of write pipe (for child use)
		CloseAndResetFileDescriptor(m_writePdes[0]);
	}

	return true;

} // Open

//-----------------------------------------------------------------------------
int KBasePipe::Close(int iWaitMilliseconds)
//-----------------------------------------------------------------------------
{
	if (m_pid > 0)
	{
		if (m_Mode & PipeRead)
		{
			// Close read on stdout pipe
			CloseAndResetFileDescriptor(m_readPdes[0]);
		}

		if (m_Mode & PipeWrite)
		{
			// send EOF by closing write end of pipe
			CloseAndResetFileDescriptor(m_writePdes[1]);
		}

		// child has been cut off from parent, let it terminate
		if (iWaitMilliseconds < 0)
		{
			// wait until child terminates
			wait(false);
		}
		else
		{
			// wait for given timeout, then kill child
			Wait(iWaitMilliseconds);

			// did the child terminate properly?
			if (IsRunning())
			{
				// no
				kill(m_pid, SIGKILL);
				m_iExitCode = -1;
			}
		}

		m_pid = 0;
	}

	return m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
void KBasePipe::wait(bool bNoHang)
//-----------------------------------------------------------------------------
{
	if (m_pid > 0)
	{
		int iStatus { 0 };

		pid_t iPid;

		do
		{
			iPid = waitpid(m_pid, &iStatus, bNoHang ? WNOHANG : 0);
		}
		while (iPid == -1 && errno == EINTR);

		if (iPid == -1)
		{
			m_iExitCode = -1;
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
				m_iExitCode = -1;
				int iSignal = WTERMSIG(iStatus);
				if (iSignal)
				{
					kDebug(1, "aborted by signal {}", kTranslateSignal(iSignal));
				}
			}
			else
			{
				m_iExitCode = -1;
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

	wait(true);

	return (m_pid > 0);

} // IsRunning

//-----------------------------------------------------------------------------
bool KBasePipe::Wait(int msecs)
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return true;
	}

	std::chrono::steady_clock::time_point tStart = std::chrono::steady_clock::now();

	std::chrono::milliseconds timeout(msecs);

	while (IsRunning())
	{
		std::chrono::steady_clock::time_point tNow = std::chrono::steady_clock::now();

		if (tNow - tStart > timeout)
		{
			return false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return true;

} // Wait

//-----------------------------------------------------------------------------
bool KBasePipe::Kill(int msecs)
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return true;
	}

	// send a SIGINT
	kill(m_pid, SIGINT);

	// call Close() which will send a SIGKILL after waiting
	Close(msecs);

	return true;

} // Kill

//-----------------------------------------------------------------------------
void KBasePipe::CloseAndResetFileDescriptor(int& iFileDescriptor)
//-----------------------------------------------------------------------------
{
	if (iFileDescriptor >= 0)
	{
		if (::close(iFileDescriptor))
		{
			kDebug(2, "error closing file descriptor {}: {}", iFileDescriptor, strerror(errno));
		}
		iFileDescriptor = -1;
	}

} // CloseAndResetFileDescriptor

DEKAF2_NAMESPACE_END

#endif
