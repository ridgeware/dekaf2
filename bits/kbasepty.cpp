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

#include "kbasepty.h"

#ifdef DEKAF2_HAS_PIPES

#include "../ksystem.h"
#include "../ksignals.h"
#include "../kchildprocess.h"
#include "../klog.h"
#include <csignal>
#include <thread>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KBasePTY::Open(KStringView sShell, LoginMode Mode,
					const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	Close(); // ensure a previous PTY is closed

	if (m_pid)
	{
		kWarning("cannot open PTY: old one still running");
		return false;
	}

	m_iExitCode = 0;

	// allocate a pseudo-terminal master
	m_iMasterFD = posix_openpt(O_RDWR | O_NOCTTY);

	if (m_iMasterFD < 0)
	{
		kWarning("cannot allocate PTY master: {}", strerror(errno));
		return false;
	}

	if (grantpt(m_iMasterFD) < 0)
	{
		kWarning("grantpt failed: {}", strerror(errno));
		CloseAndResetFileDescriptor(m_iMasterFD);
		return false;
	}

	if (unlockpt(m_iMasterFD) < 0)
	{
		kWarning("unlockpt failed: {}", strerror(errno));
		CloseAndResetFileDescriptor(m_iMasterFD);
		return false;
	}

	const char* pSlaveName = ptsname(m_iMasterFD);

	if (!pSlaveName)
	{
		kWarning("ptsname failed: {}", strerror(errno));
		CloseAndResetFileDescriptor(m_iMasterFD);
		return false;
	}

	// save the slave name (ptsname may use a static buffer)
	m_sSlaveName = pSlaveName;

	kDebug(2, "PTY master fd {}, slave {}", m_iMasterFD, m_sSlaveName);

	// determine the command to execute
	KString sCommand;

	if (Mode == Login)
	{
		sCommand = sShell.empty() ? "/usr/bin/login" : KString(sShell);
	}
	else
	{
		if (sShell.empty())
		{
			const char* pShell = std::getenv("SHELL");
			sCommand = pShell ? pShell : "/bin/sh";
		}
		else
		{
			sCommand = sShell;
		}
	}

	// create a child
	switch (m_pid = fork())
	{
		case -1: // error
		{
			kWarning("cannot fork for PTY: {}", strerror(errno));
			CloseAndResetFileDescriptor(m_iMasterFD);
			m_pid = 0;
			return false;
		}

		case 0: // child
		{
			// close the master FD in the child
			::close(m_iMasterFD);

			// create a new session and become session leader
			if (setsid() < 0)
			{
				_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
			}

			// open the slave PTY
			int iSlaveFD = ::open(m_sSlaveName.c_str(), O_RDWR);

			if (iSlaveFD < 0)
			{
				_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
			}

#ifdef TIOCSCTTY
			// make the slave the controlling terminal
			ioctl(iSlaveFD, TIOCSCTTY, 0);
#endif

			// redirect stdin/stdout/stderr to the slave PTY
			dup2(iSlaveFD, STDIN_FILENO);
			dup2(iSlaveFD, STDOUT_FILENO);
			dup2(iSlaveFD, STDERR_FILENO);

			if (iSlaveFD > STDERR_FILENO)
			{
				::close(iSlaveFD);
			}

			// close all other file descriptors except stdin/stdout/stderr
			detail::kCloseOwnFilesForExec(false);

			// enable SIGPIPE
			signal(SIGPIPE, SIG_DFL);

			// set additional environment variables
			kSetEnv(Environment);

			// execute the command
			execlp(sCommand.c_str(), sCommand.c_str(), nullptr);

			_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
		}
	}

	// only parent gets here
	return true;

} // Open

//-----------------------------------------------------------------------------
int KBasePTY::Close(int iWaitMilliseconds)
//-----------------------------------------------------------------------------
{
	if (m_pid > 0)
	{
		// first try a non-blocking wait - the child may have already
		// exited (e.g. from an "exit" command sent via the stream)
		wait(true);

		if (m_pid > 0)
		{
			// child still running - close master FD to send SIGHUP
			CloseAndResetFileDescriptor(m_iMasterFD);

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
					wait(false);
					m_iExitCode = -1;
				}
			}

			m_pid = 0;
		}

		// clean up the master FD if not already closed
		CloseAndResetFileDescriptor(m_iMasterFD);
	}

	m_sSlaveName.clear();

	return m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
void KBasePTY::wait(bool bNoHang)
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
			kDebug(1, "cannot close PTY: {}", strerror(errno));
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
bool KBasePTY::IsRunning()
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
bool KBasePTY::Wait(int msecs)
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
bool KBasePTY::Kill(int msecs)
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
bool KBasePTY::SetWindowSize(uint16_t iRows, uint16_t iCols)
//-----------------------------------------------------------------------------
{
	if (m_iMasterFD < 0 || m_sSlaveName.empty())
	{
		return false;
	}

	struct winsize ws;
	ws.ws_row    = iRows;
	ws.ws_col    = iCols;
	ws.ws_xpixel = 0;
	ws.ws_ypixel = 0;

	// TIOCSWINSZ must be applied on the slave side on some platforms
	// (macOS returns ENOTTY on the master), so we try the master first
	// and fall back to opening the slave
	if (ioctl(m_iMasterFD, TIOCSWINSZ, &ws) < 0)
	{
		// try the slave side
		int iSlaveFD = ::open(m_sSlaveName.c_str(), O_RDWR | O_NOCTTY);

		if (iSlaveFD < 0)
		{
			kDebug(1, "cannot open slave PTY for window size: {}", strerror(errno));
			return false;
		}

		int iResult = ioctl(iSlaveFD, TIOCSWINSZ, &ws);
		::close(iSlaveFD);

		if (iResult < 0)
		{
			kDebug(1, "cannot set window size: {}", strerror(errno));
			return false;
		}
	}

	// notify the child about the size change
	if (m_pid > 0)
	{
		kill(m_pid, SIGWINCH);
	}

	return true;

} // SetWindowSize

//-----------------------------------------------------------------------------
void KBasePTY::CloseAndResetFileDescriptor(int& iFileDescriptor)
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
