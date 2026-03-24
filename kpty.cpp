/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "kpty.h"

#ifdef DEKAF2_HAS_PIPES

#include "ksystem.h"
#include "kchildprocess.h"
#include "klog.h"
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <sys/ioctl.h>
#include <termios.h>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
std::streamsize KPTYReader(void* sBuffer, std::streamsize iCount, void* pContext)
//-----------------------------------------------------------------------------
{
	auto* pCtx = static_cast<KPTYReaderContext*>(pContext);

	if (!pCtx || !pCtx->pMasterFD)
	{
		return 0;
	}

	int fd = *pCtx->pMasterFD;

	if (fd < 0)
	{
		return 0;
	}

	// if a timeout is configured, wait for data with poll()
	if (pCtx->pTimeout)
	{
		auto iMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(*pCtx->pTimeout).count();

		struct pollfd pfd;
		pfd.fd      = fd;
		pfd.events  = POLLIN;
		pfd.revents = 0;

		int iReady;

		do
		{
			iReady = ::poll(&pfd, 1, static_cast<int>(iMilliseconds));
		}
		while (iReady == -1 && errno == EINTR);

		if (iReady <= 0)
		{
			// timeout or error
			return 0;
		}

		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			// error condition on the fd
			if (!(pfd.revents & POLLIN))
			{
				return 0;
			}
		}
	}

	auto iRead = ::read(fd, sBuffer, static_cast<size_t>(iCount));

	if (iRead < 0)
	{
		if (errno == EINTR || errno == EAGAIN)
		{
			return 0;
		}
		return -1;
	}

	return iRead;

} // KPTYReader

} // end of namespace detail

//-----------------------------------------------------------------------------
void KPTYIOStream::open(int iMasterFD)
//-----------------------------------------------------------------------------
{
	m_iMasterFD = iMasterFD;

	if (m_iMasterFD >= 0)
	{
		clear();
	}

} // open

//-----------------------------------------------------------------------------
void KPTYIOStream::close()
//-----------------------------------------------------------------------------
{
	if (m_iMasterFD >= 0)
	{
		::close(m_iMasterFD);
		m_iMasterFD = -1;
	}

} // close

template class KReaderWriter<KPTYIOStream>;

//-----------------------------------------------------------------------------
bool KPTY::Open(LoginMode Mode,
				KStringView sShell,
				KDuration Timeout,
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
	m_iMasterFD = ::posix_openpt(O_RDWR | O_NOCTTY);

	if (m_iMasterFD < 0)
	{
		kWarning("cannot allocate PTY master: {}", ::strerror(errno));
		return false;
	}

	if (::grantpt(m_iMasterFD) < 0)
	{
		kWarning("grantpt failed: {}", ::strerror(errno));
		CloseAndResetFileDescriptor(m_iMasterFD);
		return false;
	}

	if (::unlockpt(m_iMasterFD) < 0)
	{
		kWarning("unlockpt failed: {}", ::strerror(errno));
		CloseAndResetFileDescriptor(m_iMasterFD);
		return false;
	}

	const char* pSecondaryName = ::ptsname(m_iMasterFD);

	if (!pSecondaryName)
	{
		kWarning("ptsname failed: {}", ::strerror(errno));
		CloseAndResetFileDescriptor(m_iMasterFD);
		return false;
	}

	// save the secondary PTY name (ptsname may use a static buffer)
	m_sSecondaryName = pSecondaryName;

	kDebug(2, "PTY master fd {}, secondary {}", m_iMasterFD, m_sSecondaryName);

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
	switch (m_pid = ::fork())
	{
		case -1: // error
		{
			kWarning("cannot fork for PTY: {}", ::strerror(errno));
			CloseAndResetFileDescriptor(m_iMasterFD);
			m_pid = 0;
			return false;
		}

		case 0: // child
		{
			// close the master FD in the child
			::close(m_iMasterFD);

			// create a new session and become session leader
			if (::setsid() < 0)
			{
				::_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
			}

			// open the secondary PTY
			int iSecondaryFD = ::open(m_sSecondaryName.c_str(), O_RDWR);

			if (iSecondaryFD < 0)
			{
				::_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
			}

#ifdef TIOCSCTTY
			// make the secondary PTY the controlling terminal
			::ioctl(iSecondaryFD, TIOCSCTTY, 0);
#endif

			// redirect stdin/stdout/stderr to the secondary PTY
			::dup2(iSecondaryFD, STDIN_FILENO);
			::dup2(iSecondaryFD, STDOUT_FILENO);
			::dup2(iSecondaryFD, STDERR_FILENO);

			if (iSecondaryFD > STDERR_FILENO)
			{
				::close(iSecondaryFD);
			}

			// close all other file descriptors except stdin/stdout/stderr
			detail::kCloseOwnFilesForExec(false);

			// enable SIGPIPE
			::signal(SIGPIPE, SIG_DFL);

			// set additional environment variables
			kSetEnv(Environment);

			// execute the command
			::execlp(sCommand.c_str(), sCommand.c_str(), nullptr);

			::_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
		}
	}

	// only parent gets here

	KPTYIOStream::SetTimeout(Timeout);
	KPTYIOStream::open(m_iMasterFD);

	return KPTYStream::good();

} // Open

//-----------------------------------------------------------------------------
int KPTY::Close(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	// invalidate the stream - we close the FD ourselves
	KPTYStream::Cancel();

	if (m_pid > 0)
	{
		// first try a non-blocking wait - the child may have already
		// exited (e.g. from an "exit" command sent via the stream)
		wait(true);

		if (m_pid > 0)
		{
			// child still running - close master FD to send SIGHUP
			CloseAndResetFileDescriptor(m_iMasterFD);
			WaitOrKill(Timeout);
		}

		// clean up the master FD if not already closed
		CloseAndResetFileDescriptor(m_iMasterFD);
	}

	m_sSecondaryName.clear();

	return m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
bool KPTY::Kill(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return true;
	}

	// send a SIGINT
	::kill(m_pid, SIGINT);

	// call Close() which will send a SIGKILL after waiting
	Close(Timeout);

	return true;

} // Kill

//-----------------------------------------------------------------------------
bool KPTY::SetWindowSize(uint16_t iRows, uint16_t iCols)
//-----------------------------------------------------------------------------
{
	if (m_iMasterFD < 0 || m_sSecondaryName.empty())
	{
		return false;
	}

	struct winsize ws;
	ws.ws_row    = iRows;
	ws.ws_col    = iCols;
	ws.ws_xpixel = 0;
	ws.ws_ypixel = 0;

	// TIOCSWINSZ must be applied on the secondary side on some platforms
	// (macOS returns ENOTTY on the master), so we try the master first
	// and fall back to opening the secondary
	if (::ioctl(m_iMasterFD, TIOCSWINSZ, &ws) < 0)
	{
		// try the secondary side
		int iSecondaryFD = ::open(m_sSecondaryName.c_str(), O_RDWR | O_NOCTTY);

		if (iSecondaryFD < 0)
		{
			kDebug(1, "cannot open secondary PTY for window size: {}", ::strerror(errno));
			return false;
		}

		int iResult = ::ioctl(iSecondaryFD, TIOCSWINSZ, &ws);
		::close(iSecondaryFD);

		if (iResult < 0)
		{
			kDebug(1, "cannot set window size: {}", ::strerror(errno));
			return false;
		}
	}

	// notify the child about the size change
	if (m_pid > 0)
	{
		::kill(m_pid, SIGWINCH);
	}

	return true;

} // SetWindowSize

DEKAF2_NAMESPACE_END

#endif
