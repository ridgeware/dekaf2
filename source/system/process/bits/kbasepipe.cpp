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

#include <dekaf2/system/process/bits/kbasepipe.h>

#ifdef DEKAF2_HAS_PIPES

#include <dekaf2/core/strings/ksplit.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/system/process/kchildprocess.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/format/kformat.h>
#include <csignal>
#ifdef DEKAF2_IS_WINDOWS
	#include <windows.h>
	#include <io.h>
	#include <fcntl.h>
#endif

DEKAF2_NAMESPACE_BEGIN

#ifdef DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
bool KBasePipe::Open(KString sCommand, KStringViewZ sShell, OpenMode Mode, const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	sCommand.TrimLeft();

	if (sCommand.empty())
	{
		// an empty command is invalid also with a shell
		Close(); // ensure a previous pipe is closed
		m_iExitCode = EINVAL;
		return false;
	}

	if (sShell.empty())
	{
		// the child splits its command line into arguments itself
		return OpenCommandLine(sCommand, Mode, Environment);
	}

	if (sShell != "/bin/sh")
	{
		// Windows has only its command interpreter, a shell cannot be chosen
		kDebug(1, "shell '{}' will be ignored and the command interpreter be used", sShell);
	}

	return OpenCommandLine(KWindowsProcess::ShellCommandLine(sCommand), Mode, Environment);

} // Open

//-----------------------------------------------------------------------------
bool KBasePipe::Open(std::vector<KString> Args, OpenMode Mode, const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	if (Args.empty() || Args.front().empty())
	{
		Close(); // ensure a previous pipe is closed
		m_iExitCode = EINVAL;
		return false;
	}

	return OpenCommandLine(KWindowsProcess::CommandLine(Args), Mode, Environment);

} // Open

//-----------------------------------------------------------------------------
bool KBasePipe::OpenCommandLine(KStringView sCommandLine, OpenMode Mode, const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	Close(); // ensure a previous pipe is closed

	m_Mode      = Mode;
	m_iExitCode = 0;

	kDebug(2, "executing: {}", sCommandLine);

	// our ends of the pipes, and those of the child
	HANDLE hRead       { nullptr };
	HANDLE hWrite      { nullptr };
	HANDLE hChildStdIn { nullptr };
	HANDLE hChildStdOut{ nullptr };

	// the pipes are not inheritable: KWindowsProcess::Start() makes the ends of the
	// child inheritable only while it starts the child
	if ((Mode & PipeRead) && !::CreatePipe(&hRead, &hChildStdOut, nullptr, 0))
	{
		auto iError = ::GetLastError();
		kDebug(1, "cannot open input pipe '{}': {}", sCommandLine, KWindowsProcess::ErrorText(iError));
		m_iExitCode = static_cast<int>(iError);
		return false;
	}

	if ((Mode & PipeWrite) && !::CreatePipe(&hChildStdIn, &hWrite, nullptr, 0))
	{
		auto iError = ::GetLastError();
		kDebug(1, "cannot open output pipe '{}': {}", sCommandLine, KWindowsProcess::ErrorText(iError));

		if (hRead)
		{
			::CloseHandle(hRead);
			::CloseHandle(hChildStdOut);
		}

		m_iExitCode = static_cast<int>(iError);
		return false;
	}

	// as on Unix, the pipes replace stdin and stdout of the child, and the other
	// standard handles are those of this process - Start() closes the ends of the child
	auto iError = m_Process.Start(sCommandLine, hChildStdIn, hChildStdOut, nullptr, Environment);

	if (iError)
	{
		if (hRead)  ::CloseHandle(hRead);
		if (hWrite) ::CloseHandle(hWrite);

		m_iExitCode = (iError == ERROR_FILE_NOT_FOUND || iError == ERROR_PATH_NOT_FOUND)
		            ? DEKAF2_POPEN_COMMAND_NOT_FOUND
		            : static_cast<int>(iError);
		return false;
	}

	// file descriptors for KFDReader and KFDWriter, which read what is available - binary
	// as the pipes on Unix, or in text mode as the pipes of _popen()
	int iTextMode = (Mode & Text) ? _O_TEXT : _O_BINARY;
	int iErrno    = 0;

	if (hRead)
	{
		m_readPdes[0] = ::_open_osfhandle(reinterpret_cast<intptr_t>(hRead), _O_RDONLY | iTextMode);

		if (m_readPdes[0] == -1)
		{
			iErrno = errno;
			::CloseHandle(hRead);
		}
	}

	if (hWrite)
	{
		m_writePdes[1] = ::_open_osfhandle(reinterpret_cast<intptr_t>(hWrite), iTextMode);

		if (m_writePdes[1] == -1)
		{
			iErrno = errno;
			::CloseHandle(hWrite);
		}
	}

	if (iErrno)
	{
		kDebug(1, "cannot get a file descriptor for the pipe: {}", ::strerror(iErrno));
		// terminates the child
		Close(chrono::milliseconds(0));
		m_iExitCode = iErrno;
		return false;
	}

	return true;

} // OpenCommandLine

//-----------------------------------------------------------------------------
bool KBasePipe::Kill(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (!m_Process.IsStarted())
	{
		return true;
	}

	// there is no SIGINT for a single child - terminate it right away, together with
	// all processes it started
	m_Process.Terminate();

	// call Close() which waits for the termination
	Close(Timeout);

	return true;

} // Kill

#else // DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
bool KBasePipe::Open(KString sCommand, KStringViewZ sShell, OpenMode Mode, const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	sCommand.TrimLeft();

	if (sCommand.empty())
	{
		// an empty command is invalid also with a shell
		Close(); // ensure a previous pipe is closed
		m_iExitCode = EINVAL;
		return false;
	}

	std::vector<KString> Args;

	if (sShell.empty())
	{
		// split the command line into arguments (honoring quotes)
		std::vector<const char*> argV;
		kSplitArgsInPlace(argV, sCommand);
		Args.reserve(argV.size());

		for (auto szArg : argV)
		{
			Args.emplace_back(szArg);
		}
	}
	else
	{
		Args.reserve(3);
		Args.emplace_back(sShell);
		Args.emplace_back("-c");
		Args.push_back(std::move(sCommand));
	}

	return Open(std::move(Args), Mode, Environment);

} // Open

//-----------------------------------------------------------------------------
bool KBasePipe::Open(std::vector<KString> Args, OpenMode Mode, const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	Close(); // ensure a previous pipe is closed

	KStringView sCommand = Args.empty() ? KStringView{} : KStringView(Args.front());

	if (m_pid)
	{
		kWarning("cannot open pipe '{}': {}", sCommand, "old one still running");
		return false;
	}

	m_Mode = Mode;

	m_iExitCode = 0;

	if (sCommand.empty())
	{
		m_iExitCode = EINVAL;
		return false;
	}

	if (m_Mode & PipeRead)
	{
		if (::pipe(m_readPdes) < 0)
		{
			kWarning("cannot open input pipe '{}': {}", sCommand, ::strerror(errno));
			return false;
		}
	}

	if (m_Mode & PipeWrite)
	{
		if (::pipe(m_writePdes) < 0)
		{
			kWarning("cannot open output pipe '{}': {}", sCommand, ::strerror(errno));
			return false;
		}
	}

	if (kWouldLog(2))
	{
		KString sJoined;

		for (const auto& sArg : Args)
		{
			if (!sJoined.empty()) sJoined += ' ';
			sJoined += sArg;
		}

		kDebug(2, "executing: {}", sJoined);
	}

	// we need to do the object allocations in the parent
	// process as otherwise leak detectors would claim the
	// child has lost allocated memory (as the child would
	// never run the destructor)

	std::vector<const char*> argV;
	argV.reserve(Args.size() + 1);

	for (const auto& sArg : Args)
	{
		argV.push_back(sArg.c_str());
	}

	// terminate with nullptr
	argV.push_back(nullptr);

	auto bOwnProcessGroup = KProcessGroup::UseOwn(m_ProcessGroup);

	// create a child
	switch (m_pid = ::fork())
	{
		case -1: /* error */
		{
			auto iErrno = errno;

			kWarning("cannot fork '{}': {}", sCommand, ::strerror(iErrno));

			// could not create the child - close both ends of the pipes
			if (m_Mode & PipeRead)
			{
				CloseAndResetFileDescriptor(m_readPdes[0]);
				CloseAndResetFileDescriptor(m_readPdes[1]);
			}

			if (m_Mode & PipeWrite)
			{
				CloseAndResetFileDescriptor(m_writePdes[0]);
				CloseAndResetFileDescriptor(m_writePdes[1]);
			}

			m_pid       = 0;
			m_iExitCode = iErrno;

			return false;
		}

		case 0: /* child */
		{
			if (bOwnProcessGroup)
			{
				KProcessGroup::EnterOwn();
			}

			detail::kCloseOwnFilesForExec(false, m_readPdes, 4);

			// enable SIGPIPE!
			::signal(SIGPIPE, SIG_DFL);
			kResetSignalHandlers();
			kUnblockAllSignals();

			if (m_Mode & PipeWrite)
			{
				// Bind to Child's stdin
				CloseAndResetFileDescriptor(m_writePdes[1]);
				if (m_writePdes[0] != ::fileno(stdin))
				{
					::dup2(m_writePdes[0], ::fileno(stdin));
					CloseAndResetFileDescriptor(m_writePdes[0]);
				}
			}

			if (m_Mode & PipeRead)
			{
				// Bind Child's stdout
				CloseAndResetFileDescriptor(m_readPdes[0]);
				if (m_readPdes[1] != ::fileno(stdout))
				{
					::dup2(m_readPdes[1], ::fileno(stdout));
					CloseAndResetFileDescriptor(m_readPdes[1]);
				}
			}

			// set additional environment variables. execvpe() is a GNU extension
			// and cannot be used on MacOS or Windows.
			// also note that we do not replace the parent's environment, instead
			// we inherit it
			kSetEnv(Environment);

			// execute the command
			::execvp(argV[0], const_cast<char* const*>(argV.data()));

			::_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
		}

	}

	// only parent gets here

	if (bOwnProcessGroup)
	{
		KProcessGroup::AdoptChild(m_pid);
		m_bOwnProcessGroup = true;
	}

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
bool KBasePipe::Kill(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return true;
	}

	// send a SIGINT
	KProcessGroup::SignalChild(m_pid, m_bOwnProcessGroup, SIGINT);

	// call Close() which will send a SIGKILL after waiting
	Close(Timeout);

	return true;

} // Kill

#endif // DEKAF2_IS_WINDOWS

//-----------------------------------------------------------------------------
int KBasePipe::Close(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	// also after IsRunning() found the child ended, or after a failed Open(): the
	// descriptors are still open. The child gets the end of its input, or a broken
	// pipe for its output.
	for (auto& iFileDescriptor : m_readPdes)
	{
		CloseAndResetFileDescriptor(iFileDescriptor);
	}

	// child has been cut off from parent, let it terminate
	WaitOrKill(Timeout);

	return m_iExitCode;

} // Close

DEKAF2_NAMESPACE_END

#endif
