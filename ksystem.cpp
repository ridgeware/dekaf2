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

#include <thread>
#include <sys/wait.h>
#include <signal.h>
#include "bits/kfilesystem.h"
#include "ksystem.h"
#include "kstring.h"
#include "klog.h"
#include "ksplit.h"

#include <sys/types.h>    // for getpwuid()
#include <pwd.h>          // for getpwuid()
#ifndef DEKAF2_IS_OSX
#include <sys/syscall.h>
#endif

namespace dekaf2
{

//-----------------------------------------------------------------------------
KString kGetCWD ()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	return fs::current_path().string();
#else
	enum { MAX_PATH = 1024 };
	KString str(MAX_PATH, '\0');
	if (::getcwd(&str[0], str.size()-1))
	{
		size_t iPathLen = ::strlen(str.c_str());
		str.erase(iPathLen);
	}
	else
	{
		kWarning("cannot get current working directory: {}", ::strerror(errno));
		str.erase();
	}
	return str;
#endif

} // kGetCWD

//-----------------------------------------------------------------------------
KString kGetWhoAmI ()
//-----------------------------------------------------------------------------
{
	KString sWhoami;

#ifdef WIN32
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// WINDOWS:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	enum { MAX = 100 };
	char szWhoami[MAX + 1] = { 0 };
	DWORD nSize = MAX;
	if (GetUserName (szWhoami, &nSize))
	{
		sWhoami = szWhoami;
	}
#else
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// on UNIX, we do *not* cache the kGetWhoami() result, to allow
	// identity changes through setuid():
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	int iUID = getuid();
	struct passwd* pPassStruct = getpwuid (iUID);
	if (pPassStruct)
	{
		sWhoami = pPassStruct->pw_name;
	}
#endif

	kDebugLog (2, "kGetWhoAmI(): {}", sWhoami);

	return (sWhoami);

} // kGetWhoami

//-----------------------------------------------------------------------------
KString kGetHostname (KString& sHostname)
//-----------------------------------------------------------------------------
{
	KString sHostName;

	enum {MAX = 50};
	char szHostname[MAX+1];

#ifdef WIN32
	*szHostname = 0;

	DWORD nSize = MAX;
	GetComputerName (
		szHostname,         // name buffer
		&nSize              // address of size of name buffer
	);

	sHostname = szHostname;

	if (sHostname.empty())
	{
		sHostname = "hostname-error";
	}
#else
	if (gethostname (szHostname, sizeof (szHostname)) != 0)
	{
		kDebugLog (1, "CMD ERROR: hostname");
		sHostname = "hostname-error";
	}
	else
	{
		sHostname = szHostname;
	}
#endif

	kDebugLog (2, "kGetHostname(): %s", sHostname);

	return (sHostname);

} // kGetHostname

namespace detail {

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

	for (int fd = 0; fd <= 255; ++fd)
	{
		close(fd);
	}

	int iStdIn = open("/dev/null", O_RDONLY);
	if (iStdIn != 0)
	{
		kWarning("opening stdin failed, got fd {}", iStdIn);
	}

	int iStdOut = open("/dev/null", O_WRONLY);
	if (iStdOut != 1)
	{
		kWarning("opening stdout failed, got fd {}", iStdOut);
	}

	int iStdErr = dup(iStdOut);
	if (iStdErr != 2)
	{
		kWarning("opening stderr failed, got fd {}", iStdErr);
	}

} // kDaemonize

} // end of namespace detail

uint64_t kGetTid()
{
#ifdef DEKAF2_IS_OSX
	uint64_t TID;
	pthread_threadid_np(nullptr, &TID);
	return TID;
#else
	return syscall(SYS_gettid);
#endif

} // kGetTid


//-----------------------------------------------------------------------------
bool KChildProcess::Start(KStringView sCommand, KStringViewZ sChangeDirectory, bool bDaemonized)
//-----------------------------------------------------------------------------
{
	if (m_child)
	{
		return SetError("child already started!");
	}

	// Build command args. Do this before forking for the first time, as otherwise
	// ASAN would complain about memory leaks
	std::vector<KString> sArgs;
	kSplit(sArgs, sCommand, " \t", "", 0, true, true);

	if (sArgs.empty())
	{
		return SetError("no command to execute");
	}

	// convert into char* array
	std::vector<const char*> cArgs;
	cArgs.reserve(sArgs.size() + 1);
	for (const auto& it : sArgs)
	{
		cArgs.push_back(it.c_str());
	}
	cArgs.push_back(nullptr);

	pid_t pid;

	if ((pid = fork()))
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

		if (!sChangeDirectory.empty())
		{
			if (chdir(sChangeDirectory.c_str()))
			{
				SetError(kFormat("chdir to {} failed: {}", sChangeDirectory, std::strerror(errno)));
				exit(1);
			}
		}

		// umask never fails, and it returns the previous umask
		umask(S_IWGRP | S_IWOTH);

		for (int fd = 0; fd <= 255; ++fd)
		{
			close(fd);
		}

		int iStdIn = open("/dev/null", O_RDONLY);
		if (iStdIn != 0)
		{
			kDebugLog(1, "KChildProcess: opening stdin failed, got fd {}", iStdIn);
		}

		int iStdOut = open("/dev/null", O_WRONLY);
		if (iStdOut != 1)
		{
			kDebugLog(1, "KChildProcess: opening stdout failed, got fd {}", iStdOut);
		}

		int iStdErr = dup(iStdOut);
		if (iStdErr != 2)
		{
			kDebugLog(1, "KChildProcess: opening stderr failed, got fd {}", iStdErr);
		}

		m_bIsDaemonized = true;

	} // of bDaemonized

	::execvp(cArgs[0], const_cast<char* const*>(cArgs.data()));

	kDebugLog(1, "execvp(): {}", strerror(errno));

	exit(1);

} // Start

//-----------------------------------------------------------------------------
bool KChildProcess::Detach()
//-----------------------------------------------------------------------------
{
	if (m_child)
	{
		if (m_bIsDaemonized)
		{
			m_child = 0;
			m_bIsDaemonized = false;
			m_sError.clear();
			return true;
		}
		else
		{
			return SetError("child is not a daemon");
		}
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

	auto tStart = std::chrono::steady_clock::now();
	pid_t success;
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

		int status;
		success = ::waitpid(m_child, &status, WNOHANG);

		if (success < 0)
		{
			return SetError(kFormat("waitpid(): {}", strerror(errno)));
		}

		if (success)
		{
			m_child = 0;
			m_bIsDaemonized = false;
		}

	} while (!success
			 && (Timeout.count() == 0
				 || (std::chrono::steady_clock::now() - tStart < Timeout)));

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

