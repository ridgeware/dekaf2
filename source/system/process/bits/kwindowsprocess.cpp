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

#include <dekaf2/system/process/bits/kwindowsprocess.h>

#ifdef DEKAF2_IS_WINDOWS

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kutf.h> // UTF-8 -> UTF-16 for the Windows W API
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <mutex>
#include <system_error>
#include <vector>
#include <windows.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

// Serializes the time in which the handles for a child are inheritable: a child
// started meanwhile by another thread could otherwise inherit them, and keep the
// pipe of our child open.
std::mutex s_SpawnMutex;

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
/// Returns the milliseconds of Timeout for the Wait functions of Windows
DWORD Milliseconds(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (Timeout == KDuration::max())
	{
		return INFINITE;
	}

	auto iMilliseconds = Timeout.milliseconds().count();

	return (iMilliseconds <= 0)        ? 0
	     : (iMilliseconds >= INFINITE) ? INFINITE - 1
	     : static_cast<DWORD>(iMilliseconds);

} // Milliseconds

//-----------------------------------------------------------------------------
/// Returns an inheritable handle of the NUL device
HANDLE NulDevice(bool bWrite)
//-----------------------------------------------------------------------------
{
	SECURITY_ATTRIBUTES sa {};
	sa.nLength        = sizeof(sa);
	sa.bInheritHandle = TRUE;

	return ::CreateFileW(L"NUL", bWrite ? GENERIC_WRITE : GENERIC_READ,
	                     FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

} // NulDevice

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

	return NulDevice(bWrite);

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

//-----------------------------------------------------------------------------
/// Appends an argument to a command line, quoted the way the C runtime of the child
/// splits its command line into argv: backslashes are literal, unless they precede
/// a quote - there each pair of them stands for one backslash, and an odd one makes
/// the quote a literal one
void AppendArgument(KString& sCommandLine, KStringView sArg)
//-----------------------------------------------------------------------------
{
	if (!sCommandLine.empty())
	{
		sCommandLine += ' ';
	}

	if (!sArg.empty() && sArg.find_first_of(" \t\n\v\"") == KStringView::npos)
	{
		sCommandLine += sArg;
		return;
	}

	sCommandLine += '"';

	std::size_t iBackslashes { 0 };

	for (auto ch : sArg)
	{
		if (ch == '\\')
		{
			++iBackslashes;
			continue;
		}

		if (ch == '"')
		{
			// the backslashes before the quote are doubled, and one more escapes the quote
			sCommandLine.append(iBackslashes * 2 + 1, '\\');
		}
		else
		{
			sCommandLine.append(iBackslashes, '\\');
		}

		iBackslashes = 0;
		sCommandLine += ch;
	}

	// the backslashes before the closing quote are doubled
	sCommandLine.append(iBackslashes * 2, '\\');
	sCommandLine += '"';

} // AppendArgument

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KWindowsProcess::KWindowsProcess(KWindowsProcess&& other) noexcept
//-----------------------------------------------------------------------------
: m_hProcess  (other.m_hProcess)
, m_hJob      (other.m_hJob)
, m_iExitCode (other.m_iExitCode)
{
	other.m_hProcess = nullptr;
	other.m_hJob     = nullptr;

} // move ctor

//-----------------------------------------------------------------------------
KWindowsProcess& KWindowsProcess::operator=(KWindowsProcess&& other) noexcept
//-----------------------------------------------------------------------------
{
	if (this != &other)
	{
		Release();

		m_hProcess  = other.m_hProcess;
		m_hJob      = other.m_hJob;
		m_iExitCode = other.m_iExitCode;

		other.m_hProcess = nullptr;
		other.m_hJob     = nullptr;
	}

	return *this;

} // move assignment

//-----------------------------------------------------------------------------
KWindowsProcess::~KWindowsProcess()
//-----------------------------------------------------------------------------
{
	Release();

} // dtor

//-----------------------------------------------------------------------------
uint32_t KWindowsProcess::Start(KStringView  sCommandLine,
                                void*        hStdIn,
                                void*        hStdOut,
                                void*        hStdErr,
                                const std::vector<std::pair<KString, KString>>& Environment,
                                KStringViewZ sWorkingDirectory,
                                bool         bDetached)
//-----------------------------------------------------------------------------
{
	// the standard handles of the child, which this method closes
	HANDLE hStd[3] { hStdIn, hStdOut, hStdErr };

	m_iExitCode = 0;

	auto wsCommandLine = kutf::Convert<std::wstring>(sCommandLine);

	std::wstring wsEnvironment;

	if (!Environment.empty())
	{
		wsEnvironment = EnvironmentBlock(Environment);
	}

	std::wstring wsWorkingDirectory;

	if (!sWorkingDirectory.empty())
	{
		wsWorkingDirectory = kutf::Convert<std::wstring>(sWorkingDirectory);
	}

	kDebug(3, "starting: {}", sCommandLine);

	PROCESS_INFORMATION Process {};
	DWORD               iError  { 0 };
	bool                bStarted;

	{
		std::lock_guard<std::mutex> Lock(s_SpawnMutex);

		const DWORD StdHandleIDs[3] { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };

		for (int i = 0; i < 3; ++i)
		{
			if (IsValid(hStd[i]))
			{
				// the handle of the caller becomes inheritable only now, while no other
				// child of this process can be started
				::SetHandleInformation(hStd[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
			}
			else
			{
				// a detached child must not share the console handles of this process
				hStd[i] = bDetached ? NulDevice(i != 0) : InheritableStdHandle(StdHandleIDs[i], i != 0);
			}
		}

		// the child inherits only these handles, and not every inheritable handle
		// of this process (files, sockets)
		std::vector<HANDLE> Inherit;

		for (auto hHandle : hStd)
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
		StartupInfo.StartupInfo.hStdInput  = hStd[0];
		StartupInfo.StartupInfo.hStdOutput = hStd[1];
		StartupInfo.StartupInfo.hStdError  = hStd[2];
		StartupInfo.lpAttributeList        = bHandleList ? pAttributes : nullptr;

		DWORD iFlags { 0 };

		if (bDetached)
		{
			// without a console, so that it does not end with the console of this process,
			// and outside the job this process may run in, so that it does not end with it
			iFlags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB;
		}
		else
		{
			// suspended until the child is in its job, else it could start processes
			// that would not be in the job
			iFlags |= CREATE_SUSPENDED;

			if (::GetConsoleWindow() == nullptr)
			{
				// this process has no console (a GUI application or a service) - do not
				// open a console window for the child
				iFlags |= CREATE_NO_WINDOW;
			}
		}

		if (bHandleList)
		{
			iFlags |= EXTENDED_STARTUPINFO_PRESENT;
		}

		if (!wsEnvironment.empty())
		{
			iFlags |= CREATE_UNICODE_ENVIRONMENT;
		}

		auto CreateChild = [&]()
		{
			return ::CreateProcessW(nullptr, &wsCommandLine[0], nullptr, nullptr, TRUE, iFlags,
			                        wsEnvironment.empty()      ? nullptr : &wsEnvironment[0],
			                        wsWorkingDirectory.empty() ? nullptr : wsWorkingDirectory.c_str(),
			                        &StartupInfo.StartupInfo, &Process) != FALSE;
		};

		bStarted = CreateChild();

		if (!bStarted && bDetached && ::GetLastError() == ERROR_ACCESS_DENIED)
		{
			// the job of this process does not permit to leave it
			iFlags &= ~static_cast<DWORD>(CREATE_BREAKAWAY_FROM_JOB);
			bStarted = CreateChild();
		}

		if (!bStarted)
		{
			iError = ::GetLastError();
		}

		if (bHaveAttributes)
		{
			::DeleteProcThreadAttributeList(pAttributes);
		}

		// the child has its own copies now
		for (auto hHandle : hStd)
		{
			CloseIfValid(hHandle);
		}
	}

	if (!bStarted)
	{
		kDebug(1, "CreateProcessW() failed for '{}': {}", sCommandLine, ErrorText(iError));
		return iError;
	}

	HANDLE hJob { nullptr };

	if (!bDetached)
	{
		// a job holds the child and all processes it starts: cmd.exe runs the actual
		// command as its own child, and Terminate() ends all of them
		hJob = ::CreateJobObjectW(nullptr, nullptr);

		if (hJob && !::AssignProcessToJobObject(hJob, Process.hProcess))
		{
			// fails before Windows 8 if this process runs in a job already
			kDebug(2, "cannot assign the child to a job: {}", ErrorText(::GetLastError()));
			::CloseHandle(hJob);
			hJob = nullptr;
		}

		::ResumeThread(Process.hThread);
	}

	::CloseHandle(Process.hThread);

	m_hProcess = Process.hProcess;
	m_hJob     = hJob;

	return 0;

} // Start

//-----------------------------------------------------------------------------
bool KWindowsProcess::Wait(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (!m_hProcess)
	{
		return true;
	}

	if (::WaitForSingleObject(m_hProcess, Milliseconds(Timeout)) != WAIT_OBJECT_0)
	{
		return false;
	}

	DWORD iExitCode { 0 };

	if (::GetExitCodeProcess(m_hProcess, &iExitCode))
	{
		m_iExitCode = static_cast<int>(iExitCode);
	}

	// as on Unix after waitpid(), the process ID of the child is gone now
	::CloseHandle(m_hProcess);
	m_hProcess = nullptr;

	return true;

} // Wait

//-----------------------------------------------------------------------------
pid_t KWindowsProcess::GetProcessID() const
//-----------------------------------------------------------------------------
{
	return m_hProcess ? static_cast<pid_t>(::GetProcessId(m_hProcess)) : 0;

} // GetProcessID

//-----------------------------------------------------------------------------
bool KWindowsProcess::Terminate()
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

//-----------------------------------------------------------------------------
void KWindowsProcess::Release()
//-----------------------------------------------------------------------------
{
	if (m_hProcess)
	{
		::CloseHandle(m_hProcess);
		m_hProcess = nullptr;
	}

	if (m_hJob)
	{
		// the job has no limits: closing its handle does not end its processes
		::CloseHandle(m_hJob);
		m_hJob = nullptr;
	}

} // Release

//-----------------------------------------------------------------------------
KString KWindowsProcess::CommandLine(const std::vector<KString>& Args)
//-----------------------------------------------------------------------------
{
	KString sCommandLine;

	for (const auto& sArg : Args)
	{
		AppendArgument(sCommandLine, sArg);
	}

	return sCommandLine;

} // CommandLine

//-----------------------------------------------------------------------------
KString KWindowsProcess::ShellCommandLine(KStringView sCommand)
//-----------------------------------------------------------------------------
{
	// /d: no AutoRun commands from the registry, /s /c "...": cmd.exe removes
	// exactly the added outer quotes, and executes the command unchanged
	KString sCommandLine;

	sCommandLine += '"';
	sCommandLine += kutf::Convert<KString>(CommandInterpreter());
	sCommandLine += "\" /d /s /c \"";
	sCommandLine += sCommand;
	sCommandLine += '"';

	return sCommandLine;

} // ShellCommandLine

//-----------------------------------------------------------------------------
KString KWindowsProcess::ErrorText(uint32_t iError)
//-----------------------------------------------------------------------------
{
	return std::system_category().message(static_cast<int>(iError));

} // ErrorText

DEKAF2_NAMESPACE_END

#endif // DEKAF2_IS_WINDOWS
