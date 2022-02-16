/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "kchildprocess.h"

#ifndef DEKAF2_IS_WINDOWS

#include "dekaf2.h"
#include "kstring.h"
#include "klog.h"
#include "ksplit.h"
#include "ktimer.h"
#include "ksignals.h"
#include <thread>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <cstdlib>

namespace dekaf2
{

namespace detail {

//-----------------------------------------------------------------------------
void kCloseOwnFilesForExec(bool bIncludeStandardIO, int Exempt[], size_t iExemptSize)
//-----------------------------------------------------------------------------
{
	const int iLowerBound = bIncludeStandardIO ? 0 : 3;

	// try to read directory /proc/self/fd (does not exist everywhere...)
#ifdef DEKAF2_IS_OSX
	auto* dir = opendir("/dev/fd");
#else
	auto* dir = opendir("/proc/self/fd");
#endif
	if (dir)
	{
		for (;;)
		{
			dirent* entry = readdir(dir);
			if (!entry)
			{
				break;
			}
#ifdef DEKAF2_IS_OSX
			KStringView sFileDescriptor(entry->d_name, entry->d_namlen);
#else
			KStringView sFileDescriptor(entry->d_name);
#endif
			int fd = sFileDescriptor.Int32();
			if (fd >= iLowerBound)
			{
				if (Exempt)
				{
					bool bExempt { false };
					for (size_t i = 0; i < iExemptSize; ++i)
					{
						if (Exempt[i] == fd)
						{
							bExempt = true;
							break;
						}
					}
					if (bExempt)
					{
						continue;
					}
				}
				close(fd);
			}
		}
		closedir(dir);
	}
	else
	{
		// just do a loop
		for (int fd = iLowerBound; fd < 1024; ++fd)
		{
			if (Exempt)
			{
				bool bExempt { false };
				for (size_t i = 0; i < iExemptSize; ++i)
				{
					if (Exempt[i] == fd)
					{
						bExempt = true;
						break;
					}
				}
				if (bExempt)
				{
					continue;
				}
			}
			close(fd);
		}
	}

} // kCloseOwnFilesForExec

//-----------------------------------------------------------------------------
void kDaemonize(bool bChangeDir)
//-----------------------------------------------------------------------------
{
	pid_t pid;

	if ((pid = fork()))
	{
		// parent

		if (pid < 0)
		{
			kWarning("cannot fork: {}", std::strerror(errno));
			exit(1);
		}

		exit(0);
	}

	// child

	pid_t sid = setsid();
	if (sid < 0)
	{
		if (errno != EPERM)
		{
			kWarning("setsid failed: {}", std::strerror(errno));
			exit(1);
		}
	}

	if (!(pid = fork()))
	{
		// parent
		exit(0);
	}

	if (pid < 0)
	{
		kWarning("cannot fork again: {}", std::strerror(errno));
		exit(1);
	}

	if (bChangeDir)
	{
		if (chdir("/"))
		{
			kWarning("chdir to / failed: {}", std::strerror(errno));
			exit(1);
		}
	}

	// umask never fails, and it returns the previous umask
	umask(S_IWGRP | S_IWOTH);

	kCloseOwnFilesForExec(true);

	int iStdIn = open("/dev/null", O_RDONLY);
	if (iStdIn != 0)
	{
		kWarning("opening {} failed, got fd {}", "stdin", iStdIn);
	}

	int iStdOut = open("/dev/null", O_WRONLY);
	if (iStdOut != 1)
	{
		kWarning("opening {} failed, got fd {}", "stdout", iStdOut);
	}

	int iStdErr = dup(iStdOut);
	if (iStdErr != 2)
	{
		kWarning("opening {} failed, got fd {}", "stderr", iStdErr);
	}

} // kDaemonize

} // end of namespace detail

//-----------------------------------------------------------------------------
void KChildProcess::Clear()
//-----------------------------------------------------------------------------
{

	m_child = 0;
	m_iExitStatus = 0;
	m_iExitSignal = 0;
	m_bIsDaemonized = false;
	m_sError.clear();

} // Clear

//-----------------------------------------------------------------------------
bool KChildProcess::Fork(int(*func)(int, char**), int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	if (m_child)
	{
		return SetError("child already started!");
	}

	Clear();

	pid_t pid;

	if (Dekaf::IsStarted())
	{
		// fork through Dekaf, as we may need to stop and restart timer and signal threads
		pid = Dekaf::getInstance().Fork();
	}
	else
	{
		pid = fork();

		if (pid)
		{
			kDebug(2, "new pid: {}", pid);
		}
	}

	if (pid)
	{
		// parent

		if (pid < 0)
		{
			return SetError(kFormat("fork(): {}", strerror(errno)));
		}

		m_child = pid;

		return true;
	}

	// child

	kDebug(2, "child process started");

	// call entry function
	auto iError = func(argc, argv);

	kDebug(2, "exiting child");

	// and exit on return
	_exit (iError);

} // Fork

//-----------------------------------------------------------------------------
bool KChildProcess::Start(KString sCommand, KStringViewZ sChangeDirectory, bool bDaemonized)
//-----------------------------------------------------------------------------
{
	if (m_child)
	{
		return SetError("child already started!");
	}

	Clear();

	// Build command args. Do this before forking for the first time, as otherwise
	// ASAN would complain about memory leaks

	std::vector<const char*> cArgs;

	kSplitArgsInPlace(cArgs, sCommand);

	if (cArgs.empty())
	{
		return SetError("no command to execute");
	}

	// execvp() needs a final nullptr in the array
	cArgs.push_back(nullptr);

	pid_t pid;

	if ((pid = fork()))
	{
		// parent

		if (pid < 0)
		{
			return SetError(kFormat("fork(): {}", strerror(errno)));
		}

		kDebug(2, "new pid: {}", pid);

		m_child = pid;

		return true;
	}

	// child

	if (bDaemonized)
	{

		pid_t sid = setsid();
		if (sid < 0)
		{
			if (errno != EPERM)
			{
				SetError(kFormat("setsid failed: {}", std::strerror(errno)));
				exit(1);
			}
		}

		pid_t pid;

		if ((pid = fork()))
		{
			// parent 2

			if (pid < 0)
			{
				SetError(kFormat("cannot fork again: {}", std::strerror(errno)));
				exit(1);
			}

			return true;
		}

		m_child = pid;

		// umask never fails, and it returns the previous umask
		umask(S_IWGRP | S_IWOTH);

		detail::kCloseOwnFilesForExec(true);

		int iStdIn = open("/dev/null", O_RDONLY);
		if (iStdIn != 0)
		{
			kDebug(1, "opening {} failed, got fd {}", "stdin", iStdIn);
		}

		int iStdOut = open("/dev/null", O_WRONLY);
		if (iStdOut != 1)
		{
			kDebug(1, "opening {} failed, got fd {}", "stdout", iStdOut);
		}

		int iStdErr = dup(iStdOut);
		if (iStdErr != 2)
		{
			kDebug(1, "opening {} failed, got fd {}", "stderr", iStdErr);
		}

		m_bIsDaemonized = true;

	} // of bDaemonized
	else
	{
		detail::kCloseOwnFilesForExec(false);
	}

	if (!sChangeDirectory.empty())
	{
		if (chdir(sChangeDirectory.c_str()))
		{
			SetError(kFormat("chdir to {} failed: {}", sChangeDirectory, std::strerror(errno)));
			exit(1);
		}
	}

	::execvp(cArgs[0], const_cast<char* const*>(cArgs.data()));

	kDebug(1, "execvp(): {}", strerror(errno));

	exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);

} // Start

//-----------------------------------------------------------------------------
bool KChildProcess::Detach()
//-----------------------------------------------------------------------------
{
	if (m_child)
	{
		// we're no more responsible for this child
		Clear();
		return true;
	}
	else
	{
		return SetError("no child started");
	}

} // Detach

//-----------------------------------------------------------------------------
bool KChildProcess::Join(std::chrono::nanoseconds Timeout)
//-----------------------------------------------------------------------------
{
	if (!m_child)
	{
		SetError("no child started");
	}

	pid_t success { 0 };
	int status;

	if (Timeout.count() == 0)
	{
		for (;;)
		{
			// wait forever, so do not loop
			success = ::waitpid(m_child, &status, 0);

			if (!(success < 0 && errno == EINTR))
			{
				break;
			}
		}

		if (success < 0)
		{
			return SetError(kFormat("waitpid(): {}", strerror(errno)));
		}
	}
	else
	{
		KStopTime Timer;
		bool bFirst { true };

		do
		{
			if (bFirst)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(10));
				bFirst = false;
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::microseconds(500));
			}

			for (;;)
			{
				success = ::waitpid(m_child, &status, WNOHANG);

				if (!(success < 0 && errno == EINTR))
				{
					break;
				}
			}

			if (success < 0)
			{
				return SetError(kFormat("waitpid(): {}", strerror(errno)));
			}

		} while (!success
				 && Timeout < Timer.elapsed<std::chrono::nanoseconds>());
	}

	if (success)
	{
		if (WIFEXITED(status))
		{
			m_iExitStatus = WEXITSTATUS(status);
			kDebug(1, "pid {} exited with value {}", success, m_iExitStatus);
		}
		else if (WIFSIGNALED(status))
		{
			m_iExitSignal = WTERMSIG(status);
			kDebug(1, "pid {} terminated by signal {} ({})", success, m_iExitSignal, kTranslateSignal(m_iExitSignal));
		}
		else if (WIFSTOPPED(status))
		{
			kDebug(1, "pid {} stopped by signal {} ({})", success, WSTOPSIG(status), kTranslateSignal(WSTOPSIG(status)));
		}

		m_child = 0;
		m_bIsDaemonized = false;
	}

	return success;

} // Join

//-----------------------------------------------------------------------------
bool KChildProcess::Stop(std::chrono::nanoseconds Timeout)
//-----------------------------------------------------------------------------
{
	if (!m_child)
	{
		SetError("no child started");
	}

	// send a SIGTERM
	if (::kill(m_child, SIGTERM))
	{
		return SetError(kFormat("kill(): {}", strerror(errno)));
	}

	return Join(Timeout);

} // Stop

//-----------------------------------------------------------------------------
bool KChildProcess::Kill()
//-----------------------------------------------------------------------------
{
	if (!m_child)
	{
		SetError("no child started");
	}

	// send a SIGHUP
	if (::kill(m_child, SIGHUP))
	{
		return SetError(kFormat("kill(): {}", strerror(errno)));
	}

	return Join(std::chrono::milliseconds(20));

} // Kill

//-----------------------------------------------------------------------------
bool KChildProcess::SetError(KStringView sError)
//-----------------------------------------------------------------------------
{
	m_sError = sError;
	kDebug(1, m_sError);
	return false;
}

//-----------------------------------------------------------------------------
KChildProcess::~KChildProcess()
//-----------------------------------------------------------------------------
{
	if (m_child)
	{
		Kill();
	}

} // dtor

} // end of namespace dekaf2

#endif // of !DEKAF2_IS_WINDOWS
