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

#include <dekaf2/system/process/bits/kbaseprocess.h>

#ifdef DEKAF2_HAS_PIPES

#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/core/logging/klog.h>
#include <csignal>
#include <thread>
#ifdef DEKAF2_IS_WINDOWS
	#include <io.h>
#else
	#include <unistd.h>
#endif

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
bool KBaseProcess::IsRunning()
//-----------------------------------------------------------------------------
{
	if (!m_Process.IsStarted())
	{
		return false;
	}

	if (!m_Process.Wait(chrono::milliseconds(0)))
	{
		return true;
	}

	m_iExitCode = m_Process.GetExitCode();
	kDebug(2, "exited with return value {}", m_iExitCode);

	return false;

} // IsRunning

//-----------------------------------------------------------------------------
bool KBaseProcess::Wait(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (!m_Process.IsStarted())
	{
		return true;
	}

	if (!m_Process.Wait(Timeout))
	{
		return false;
	}

	m_iExitCode = m_Process.GetExitCode();
	kDebug(2, "exited with return value {}", m_iExitCode);

	return true;

} // Wait

//-----------------------------------------------------------------------------
void KBaseProcess::WaitOrKill(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (!Wait(Timeout))
	{
		kDebug(1, "child did not end within {}, terminating it", Timeout);

		m_Process.Terminate();

		// the termination is asynchronous
		m_Process.Wait(chrono::milliseconds(100));

		m_iExitCode = -1;
	}

	// also the job, which still held processes the child started
	m_Process.Release();

} // WaitOrKill

//-----------------------------------------------------------------------------
bool KBaseProcess::Terminate()
//-----------------------------------------------------------------------------
{
	return m_Process.Terminate();

} // Terminate

//-----------------------------------------------------------------------------
void KBaseProcess::CloseAndResetFileDescriptor(int& iFileDescriptor)
//-----------------------------------------------------------------------------
{
	if (iFileDescriptor >= 0)
	{
		if (::_close(iFileDescriptor))
		{
			kDebug(2, "error closing file descriptor {}: {}", iFileDescriptor, ::strerror(errno));
		}
		iFileDescriptor = -1;
	}

} // CloseAndResetFileDescriptor

#else // DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
void KBaseProcess::wait(bool bNoHang)
//-----------------------------------------------------------------------------
{
	if (m_pid > 0)
	{
		int iStatus { 0 };

		pid_t iPid;

		do
		{
			iPid = ::waitpid(m_pid, &iStatus, bNoHang ? WNOHANG : 0);
		}
		while (iPid == -1 && errno == EINTR);

		if (iPid == -1)
		{
			m_iExitCode = -1;
			kDebug(1, "waitpid failed: {}", ::strerror(errno));
			ReleaseProcessGroup();
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

			ReleaseProcessGroup();
			m_pid = 0;
		}
		// if iPid == 0 the process is still running
	}

} // wait

//-----------------------------------------------------------------------------
bool KBaseProcess::IsRunning()
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
bool KBaseProcess::Wait(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return true;
	}

	auto tStart = chrono::steady_clock::now();

	while (IsRunning())
	{
		if (chrono::steady_clock::now() - tStart > Timeout)
		{
			return false;
		}

		std::this_thread::sleep_for(chrono::milliseconds(1));
	}

	return true;

} // Wait

//-----------------------------------------------------------------------------
void KBaseProcess::WaitOrKill(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (Timeout == KDuration::max())
	{
		// wait until child terminates
		wait(false);
	}
	else
	{
		// wait for given timeout, then kill child
		Wait(Timeout);

		// did the child terminate properly?
		if (IsRunning())
		{
			// no
			KProcessGroup::SignalChild(m_pid, m_bOwnProcessGroup, SIGKILL);
			wait(false);
			m_iExitCode = -1;
		}
	}

	ReleaseProcessGroup();
	m_pid = 0;

} // WaitOrKill

//-----------------------------------------------------------------------------
bool KBaseProcess::SendSignal(int iSignal)
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return true;
	}

	if (KProcessGroup::SignalChild(m_pid, m_bOwnProcessGroup, iSignal) == 0 || errno == ESRCH)
	{
		// ESRCH: the child has ended meanwhile
		return true;
	}

	kDebug(1, "cannot send {} to pid {}: {}", kTranslateSignal(iSignal), m_pid, ::strerror(errno));

	return false;

} // SendSignal

//-----------------------------------------------------------------------------
bool KBaseProcess::Terminate()
//-----------------------------------------------------------------------------
{
	// SIGKILL cannot be caught, blocked or ignored
	return SendSignal(SIGKILL);

} // Terminate

//-----------------------------------------------------------------------------
void KBaseProcess::ReleaseProcessGroup()
//-----------------------------------------------------------------------------
{
	if (m_bOwnProcessGroup)
	{
		KProcessGroup::ReleaseChild(m_pid);
		m_bOwnProcessGroup = false;
	}

} // ReleaseProcessGroup

//-----------------------------------------------------------------------------
void KBaseProcess::CloseAndResetFileDescriptor(int& iFileDescriptor)
//-----------------------------------------------------------------------------
{
	if (iFileDescriptor >= 0)
	{
		if (::close(iFileDescriptor))
		{
			kDebug(2, "error closing file descriptor {}: {}", iFileDescriptor, ::strerror(errno));
		}
		iFileDescriptor = -1;
	}

} // CloseAndResetFileDescriptor

#endif // DEKAF2_IS_WINDOWS

DEKAF2_NAMESPACE_END

#endif
