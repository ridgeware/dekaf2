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

#include <dekaf2/system/process/bits/kbaseshell.h>

#ifdef DEKAF2_IS_WINDOWS

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kutf.h> // UTF-8 -> UTF-16 for the Windows W API
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <system_error>
#include <vector>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

// Serializes the time between creating the inheritable handles for a child and
// closing them again after CreateProcessW(): a child started meanwhile by another
// thread could otherwise inherit them, and keep the pipe of our child open.
std::mutex s_SpawnMutex;

//-----------------------------------------------------------------------------
KString ErrorText(DWORD dwError)
//-----------------------------------------------------------------------------
{
	return std::system_category().message(static_cast<int>(dwError));

} // ErrorText

//-----------------------------------------------------------------------------
bool IsValid(HANDLE hHandle)
//-----------------------------------------------------------------------------
{
	return hHandle != nullptr && hHandle != INVALID_HANDLE_VALUE;

} // IsValid

//-----------------------------------------------------------------------------
void CloseIfValid(HANDLE hHandle)
//-----------------------------------------------------------------------------
{
	if (IsValid(hHandle))
	{
		::CloseHandle(hHandle);
	}

} // CloseIfValid

//-----------------------------------------------------------------------------
/// Returns an inheritable duplicate of a standard handle of this process, or a
/// handle of the NUL device if this process has none (like a GUI application)
HANDLE InheritableStdHandle(DWORD iStdHandle, bool bWrite)
//-----------------------------------------------------------------------------
{
	HANDLE hStd = ::GetStdHandle(iStdHandle);
	HANDLE hDup = nullptr;

	if (IsValid(hStd) &&
		::DuplicateHandle(::GetCurrentProcess(), hStd, ::GetCurrentProcess(), &hDup, 0, TRUE, DUPLICATE_SAME_ACCESS))
	{
		return hDup;
	}

	SECURITY_ATTRIBUTES sa {};
	sa.nLength        = sizeof(sa);
	sa.bInheritHandle = TRUE;

	return ::CreateFileW(L"NUL", bWrite ? GENERIC_WRITE : GENERIC_READ,
	                     FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

} // InheritableStdHandle

//-----------------------------------------------------------------------------
/// Before Windows 8 console handles are pseudo handles with the two lowest bits
/// set. CreateProcessW() rejects them in a handle list, and the child gets
/// them anyway, through its console.
bool IsConsolePseudoHandle(HANDLE hHandle)
//-----------------------------------------------------------------------------
{
	return (reinterpret_cast<std::uintptr_t>(hHandle) & 3) == 3 && ::GetFileType(hHandle) == FILE_TYPE_CHAR;

} // IsConsolePseudoHandle

//-----------------------------------------------------------------------------
/// Returns the command interpreter from %COMSPEC%, else cmd.exe from the system
/// directory - not from the search path, which starts with the current directory
std::wstring CommandInterpreter()
//-----------------------------------------------------------------------------
{
	std::wstring wsComSpec;

	auto iSize = ::GetEnvironmentVariableW(L"COMSPEC", nullptr, 0);

	if (iSize > 1)
	{
		wsComSpec.resize(iSize);
		iSize = ::GetEnvironmentVariableW(L"COMSPEC", &wsComSpec[0], iSize);
		wsComSpec.resize(iSize < wsComSpec.size() ? iSize : 0);
	}

	if (wsComSpec.empty())
	{
		wchar_t szSystemDir[MAX_PATH];

		auto iLen = ::GetSystemDirectoryW(szSystemDir, MAX_PATH);

		if (iLen > 0 && iLen < MAX_PATH)
		{
			wsComSpec.assign(szSystemDir, iLen);
			wsComSpec += L"\\cmd.exe";
		}
		else
		{
			wsComSpec = L"cmd.exe";
		}
	}

	return wsComSpec;

} // CommandInterpreter

//-----------------------------------------------------------------------------
/// Returns the name of an entry of an environment block. The name ends at the
/// first '=' after the first character: the names of the hidden per-drive
/// current directories start with '=' (like "=C:=C:\dir").
std::wstring NameOf(const std::wstring& wsEntry)
//-----------------------------------------------------------------------------
{
	return wsEntry.substr(0, wsEntry.find(L'=', 1));

} // NameOf

//-----------------------------------------------------------------------------
/// Returns the environment block for the child: the environment of this process,
/// with the variables of Environment added or replaced - or removed for an empty value
std::wstring EnvironmentBlock(const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	std::vector<std::wstring> Entries;

	if (auto* pEnvironment = ::GetEnvironmentStringsW())
	{
		for (auto* pEntry = pEnvironment; *pEntry; pEntry += std::wcslen(pEntry) + 1)
		{
			Entries.emplace_back(pEntry);
		}

		::FreeEnvironmentStringsW(pEnvironment);
	}

	for (const auto& Variable : Environment)
	{
		auto wsName = kutf::Convert<std::wstring>(Variable.first);

		// names of environment variables are case insensitive on Windows
		Entries.erase(std::remove_if(Entries.begin(), Entries.end(), [&wsName](const std::wstring& wsEntry)
		{
			return ::_wcsicmp(NameOf(wsEntry).c_str(), wsName.c_str()) == 0;

		}), Entries.end());

		// an empty value removes the variable, as with kSetEnv()
		if (!Variable.second.empty())
		{
			Entries.push_back(wsName + L'=' + kutf::Convert<std::wstring>(Variable.second));
		}
	}

	// sorted by name, case insensitively, like the environment blocks of Windows itself
	std::sort(Entries.begin(), Entries.end(), [](const std::wstring& wsLeft, const std::wstring& wsRight)
	{
		return ::_wcsicmp(NameOf(wsLeft).c_str(), NameOf(wsRight).c_str()) < 0;
	});

	std::wstring wsBlock;

	for (const auto& wsEntry : Entries)
	{
		wsBlock += wsEntry;
		wsBlock += L'\0';
	}

	// the block ends with an empty entry
	wsBlock += L'\0';

	return wsBlock;

} // EnvironmentBlock

} // end of anonymous namespace

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
	if (!m_hProcess)
	{
		return false;
	}

	if (::WaitForSingleObject(m_hProcess, 0) == WAIT_TIMEOUT)
	{
		return true;
	}

	DWORD iExitCode { 0 };

	if (::GetExitCodeProcess(m_hProcess, &iExitCode))
	{
		m_iExitCode = static_cast<int>(iExitCode);
	}

	// the child has ended - as on Unix, its process ID is gone now, and Close()
	// returns the exit code from above
	::CloseHandle(m_hProcess);
	m_hProcess = nullptr;

	return false;

} // IsRunning

//-----------------------------------------------------------------------------
pid_t KBaseShell::GetProcessID() const
//-----------------------------------------------------------------------------
{
	return m_hProcess ? static_cast<pid_t>(::GetProcessId(m_hProcess)) : 0;

} // GetProcessID

//-----------------------------------------------------------------------------
bool KBaseShell::IntOpen (KString sCommand, bool bWrite, bool bUseShell,
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

	std::wstring wsCommandLine;

	if (bUseShell)
	{
		// /d: no AutoRun commands from the registry, /s /c "...": cmd.exe removes
		// exactly the added outer quotes, and executes the command unchanged
		wsCommandLine  = L'"';
		wsCommandLine += CommandInterpreter();
		wsCommandLine += L"\" /d /s /c \"";
		wsCommandLine += kutf::Convert<std::wstring>(sCommand);
		wsCommandLine += L'"';
	}
	else
	{
		wsCommandLine = kutf::Convert<std::wstring>(sCommand);
	}

	std::wstring wsEnvironment;

	if (!Environment.empty())
	{
		wsEnvironment = EnvironmentBlock(Environment);
	}

	kDebug(3, "starting: {}", sCommand);

	HANDLE              hOurs   { nullptr };
	PROCESS_INFORMATION Process {};
	DWORD               iError  { 0 };
	bool                bStarted;

	{
		std::lock_guard<std::mutex> Lock(s_SpawnMutex);

		SECURITY_ATTRIBUTES sa {};
		sa.nLength        = sizeof(sa);
		sa.bInheritHandle = TRUE;

		HANDLE hRead  { nullptr };
		HANDLE hWrite { nullptr };

		if (!::CreatePipe(&hRead, &hWrite, &sa, 0))
		{
			iError = ::GetLastError();
			kDebug(1, "CreatePipe() failed: {}", ErrorText(iError));
			m_iExitCode = static_cast<int>(iError);
			return false;
		}

		// our end of the pipe must not be inherited
		hOurs = bWrite ? hWrite : hRead;
		::SetHandleInformation(hOurs, HANDLE_FLAG_INHERIT, 0);

		// as with popen(), the pipe replaces stdin or stdout of the child, and the
		// other standard handles are those of this process
		HANDLE hChilds = bWrite ? hRead : hWrite;
		HANDLE hStdIn  = bWrite ? hChilds : InheritableStdHandle(STD_INPUT_HANDLE,  false);
		HANDLE hStdOut = bWrite ? InheritableStdHandle(STD_OUTPUT_HANDLE, true) : hChilds;
		HANDLE hStdErr = InheritableStdHandle(STD_ERROR_HANDLE, true);

		// the child inherits only these handles, and not every inheritable handle
		// of this process (files, sockets)
		std::vector<HANDLE> Inherit;

		for (auto hHandle : { hStdIn, hStdOut, hStdErr })
		{
			if (IsValid(hHandle) && !IsConsolePseudoHandle(hHandle) &&
				std::find(Inherit.begin(), Inherit.end(), hHandle) == Inherit.end())
			{
				Inherit.push_back(hHandle);
			}
		}

		SIZE_T iAttributeSize { 0 };
		::InitializeProcThreadAttributeList(nullptr, 1, 0, &iAttributeSize);
		std::vector<char> AttributeBuffer(iAttributeSize);
		auto* pAttributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(AttributeBuffer.data());

		bool bHaveAttributes = !Inherit.empty() &&
		                       ::InitializeProcThreadAttributeList(pAttributes, 1, 0, &iAttributeSize);

		bool bHandleList = bHaveAttributes &&
		                   ::UpdateProcThreadAttribute(pAttributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
		                                               Inherit.data(), Inherit.size() * sizeof(HANDLE),
		                                               nullptr, nullptr);
		if (!bHandleList)
		{
			kDebug(1, "cannot restrict the inherited handles: {}", ErrorText(::GetLastError()));
		}

		STARTUPINFOEXW StartupInfo {};
		StartupInfo.StartupInfo.cb         = bHandleList ? sizeof(STARTUPINFOEXW) : sizeof(STARTUPINFOW);
		StartupInfo.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
		StartupInfo.StartupInfo.hStdInput  = hStdIn;
		StartupInfo.StartupInfo.hStdOutput = hStdOut;
		StartupInfo.StartupInfo.hStdError  = hStdErr;
		StartupInfo.lpAttributeList        = bHandleList ? pAttributes : nullptr;

		// suspended until the child is in its job, else it could start processes
		// that would not be in the job
		DWORD iFlags = CREATE_SUSPENDED;

		if (bHandleList)
		{
			iFlags |= EXTENDED_STARTUPINFO_PRESENT;
		}

		if (!wsEnvironment.empty())
		{
			iFlags |= CREATE_UNICODE_ENVIRONMENT;
		}

		if (::GetConsoleWindow() == nullptr)
		{
			// this process has no console (a GUI application or a service) - do not
			// open a console window for the child
			iFlags |= CREATE_NO_WINDOW;
		}

		bStarted = ::CreateProcessW(nullptr, &wsCommandLine[0], nullptr, nullptr, TRUE, iFlags,
		                            wsEnvironment.empty() ? nullptr : &wsEnvironment[0],
		                            nullptr, &StartupInfo.StartupInfo, &Process) != FALSE;

		if (!bStarted)
		{
			iError = ::GetLastError();
		}

		if (bHaveAttributes)
		{
			::DeleteProcThreadAttributeList(pAttributes);
		}

		// the child has its own copies now
		CloseIfValid(hStdIn);
		CloseIfValid(hStdOut);
		CloseIfValid(hStdErr);
	}

	if (!bStarted)
	{
		::CloseHandle(hOurs);

		kDebug(1, "CreateProcessW() failed for '{}': {}", sCommand, ErrorText(iError));

		m_iExitCode = (iError == ERROR_FILE_NOT_FOUND || iError == ERROR_PATH_NOT_FOUND)
		            ? DEKAF2_POPEN_COMMAND_NOT_FOUND
		            : static_cast<int>(iError);
		return false;
	}

	// a job holds the child and all processes it starts: cmd.exe runs the actual
	// command as its own child, and Close() terminates all of them after a timeout
	HANDLE hJob = ::CreateJobObjectW(nullptr, nullptr);

	if (hJob && !::AssignProcessToJobObject(hJob, Process.hProcess))
	{
		// fails before Windows 8 if this process runs in a job already
		kDebug(2, "cannot assign the child to a job: {}", ErrorText(::GetLastError()));
		::CloseHandle(hJob);
		hJob = nullptr;
	}

	::ResumeThread(Process.hThread);
	::CloseHandle(Process.hThread);

	m_hProcess = Process.hProcess;
	m_hJob     = hJob;

	// a file descriptor for KFDReader and KFDWriter, which read what is available - a
	// FILE* would wait in fread() until its buffer is full. Text mode, like the default
	// of _popen().
	m_iPipe = ::_open_osfhandle(reinterpret_cast<intptr_t>(hOurs), (bWrite ? 0 : _O_RDONLY) | _O_TEXT);

	if (m_iPipe == -1)
	{
		auto iErrno = errno;
		kDebug(1, "cannot get a file descriptor for the pipe: {}", ::strerror(iErrno));
		::CloseHandle(hOurs);
		// terminates the child
		Close(chrono::milliseconds(0));
		m_iExitCode = iErrno;
		return false;
	}

	return true;

} // IntOpen

//-----------------------------------------------------------------------------
int KBaseShell::Close(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (m_iPipe >= 0)
	{
		// the child now gets the end of its input, or a broken pipe for its output
		::_close(m_iPipe);
		m_iPipe = -1;
	}

	if (m_hProcess)
	{
		DWORD iTimeout = INFINITE;

		if (Timeout != KDuration::max())
		{
			auto iMilliseconds = Timeout.milliseconds().count();

			iTimeout = (iMilliseconds <= 0)        ? 0
			         : (iMilliseconds >= INFINITE) ? INFINITE - 1
			         : static_cast<DWORD>(iMilliseconds);
		}

		if (::WaitForSingleObject(m_hProcess, iTimeout) == WAIT_OBJECT_0)
		{
			DWORD iExitCode { 0 };

			if (::GetExitCodeProcess(m_hProcess, &iExitCode))
			{
				m_iExitCode = static_cast<int>(iExitCode);
				kDebug(2, "exited with return value {}", m_iExitCode);
			}
		}
		else
		{
			kDebug(1, "child did not end within {}, terminating it", Timeout);

			Terminate();

			// the termination is asynchronous
			::WaitForSingleObject(m_hProcess, 100);

			m_iExitCode = -1;
		}

		::CloseHandle(m_hProcess);
		m_hProcess = nullptr;
	}

	if (m_hJob)
	{
		::CloseHandle(m_hJob);
		m_hJob = nullptr;
	}

	return m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
bool KBaseShell::Terminate()
//-----------------------------------------------------------------------------
{
	// the exit code -1, as for a child that was killed by a signal on Unix
	const UINT iExitCode = static_cast<UINT>(-1);

	BOOL bTerminated = TRUE;

	if (m_hJob)
	{
		// the job holds the child and all processes it started, also when the
		// child itself has ended already
		bTerminated = ::TerminateJobObject(m_hJob, iExitCode);
	}
	else if (m_hProcess)
	{
		bTerminated = ::TerminateProcess(m_hProcess, iExitCode);
	}

	if (!bTerminated)
	{
		kDebug(1, "cannot terminate the child: {}", ErrorText(::GetLastError()));
	}

	return bTerminated != FALSE;

} // Terminate

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_IS_WINDOWS
