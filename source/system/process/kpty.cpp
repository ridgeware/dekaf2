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

#include <dekaf2/system/process/kpty.h>

#ifdef DEKAF2_HAS_PTY

#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/system/os/ksignals.h>
#include <dekaf2/system/process/kchildprocess.h>
#include <dekaf2/core/logging/klog.h>
#ifdef DEKAF2_IS_WINDOWS
	#include <dekaf2/core/strings/kutf.h> // UTF-8 -> UTF-16 for the Windows W API
#endif
#include <csignal>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#ifdef DEKAF2_IS_WINDOWS
	#include <algorithm>
	#include <climits>
	#include <windows.h>
	#include <userenv.h>
	#include <io.h>
	#include <cwchar>
	#include <vector>
#else
	#include <unistd.h>
	#include <poll.h>
	#include <sys/ioctl.h>
	#include <termios.h>
#endif

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//-----------------------------------------------------------------------------
std::streamsize KPTYReader(void* sBuffer, std::streamsize iCount, void* pContext)
//-----------------------------------------------------------------------------
{
	auto* pCtx = static_cast<KPTYReaderContext*>(pContext);

	if (!pCtx || !pCtx->pReadFD)
	{
		return 0;
	}

	int fd = *pCtx->pReadFD;

	if (fd < 0)
	{
		return 0;
	}

#ifdef DEKAF2_IS_WINDOWS

	// an anonymous pipe has neither poll() nor overlapped I/O - check for data with
	// PeekNamedPipe() until the timeout, with growing pauses up to 20 milliseconds
	auto hPipe = reinterpret_cast<HANDLE>(::_get_osfhandle(fd));

	if (hPipe == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	KStopTime Timer;
	DWORD     iPause { 1 };

	for (;;)
	{
		DWORD iAvailable { 0 };

		if (!::PeekNamedPipe(hPipe, nullptr, 0, nullptr, &iAvailable, nullptr))
		{
			// the pseudo console has closed its end of the pipe
			return 0;
		}

		if (iAvailable > 0)
		{
			break;
		}

		if (pCtx->pTimeout && Timer.elapsed() >= *pCtx->pTimeout)
		{
			// timeout
			return 0;
		}

		::Sleep(iPause);
		iPause = std::min<DWORD>(iPause * 2, 20);
	}

	// returns what is available, at most iCount
	auto iRead = ::_read(fd, sBuffer, static_cast<unsigned int>(std::min<std::streamsize>(iCount, INT_MAX)));

	return (iRead < 0) ? -1 : iRead;

#else

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

#endif

} // KPTYReader

} // end of namespace detail

//-----------------------------------------------------------------------------
void KPTYIOStream::open(int iReadFD, int iWriteFD)
//-----------------------------------------------------------------------------
{
	m_iReadFD  = iReadFD;
	m_iWriteFD = iWriteFD;

	if (m_iReadFD >= 0)
	{
		clear();
	}

} // open

//-----------------------------------------------------------------------------
void KPTYIOStream::close()
//-----------------------------------------------------------------------------
{
	if (m_iWriteFD >= 0 && m_iWriteFD != m_iReadFD)
	{
#ifdef DEKAF2_IS_WINDOWS
		::_close(m_iWriteFD);
#else
		::close(m_iWriteFD);
#endif
	}

	if (m_iReadFD >= 0)
	{
#ifdef DEKAF2_IS_WINDOWS
		::_close(m_iReadFD);
#else
		::close(m_iReadFD);
#endif
	}

	m_iReadFD  = -1;
	m_iWriteFD = -1;

} // close

template class KReaderWriter<KPTYIOStream>;

#ifdef DEKAF2_IS_WINDOWS

namespace {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The pseudo console functions of kernel32. They exist since Windows 10 1809, and
/// dekaf2 builds for older versions - therefore they are resolved at runtime.
struct PseudoConsoleAPI
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using CreateFunc = HRESULT (WINAPI*)(COORD, HANDLE, HANDLE, DWORD, void**);
	using ResizeFunc = HRESULT (WINAPI*)(void*, COORD);
	using CloseFunc  = void    (WINAPI*)(void*);

	PseudoConsoleAPI()
	{
		if (HMODULE hKernel32 = ::GetModuleHandleW(L"kernel32.dll"))
		{
			Create = reinterpret_cast<CreateFunc>(::GetProcAddress(hKernel32, "CreatePseudoConsole"));
			Resize = reinterpret_cast<ResizeFunc>(::GetProcAddress(hKernel32, "ResizePseudoConsole"));
			Close  = reinterpret_cast<CloseFunc >(::GetProcAddress(hKernel32, "ClosePseudoConsole"));
		}
	}

	bool IsAvailable() const { return Create && Resize && Close; }

	CreateFunc Create { nullptr };
	ResizeFunc Resize { nullptr };
	CloseFunc  Close  { nullptr };

}; // PseudoConsoleAPI

//-----------------------------------------------------------------------------
const PseudoConsoleAPI& ConPTY()
//-----------------------------------------------------------------------------
{
	static const PseudoConsoleAPI s_API;
	return s_API;

} // ConPTY

//-----------------------------------------------------------------------------
/// returns the console size for the pseudo console, which counts in SHORT
COORD ConsoleSize(uint16_t iRows, uint16_t iCols)
//-----------------------------------------------------------------------------
{
	COORD Size;
	Size.X = static_cast<SHORT>(std::min<uint16_t>(iCols, SHRT_MAX));
	Size.Y = static_cast<SHORT>(std::min<uint16_t>(iRows, SHRT_MAX));
	return Size;

} // ConsoleSize

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The terminal of the login: it reads the input pipe of the pseudo console and
/// writes to its output pipe, as /usr/bin/login reads and writes its terminal
class LoginTerminal
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	LoginTerminal(HANDLE hInput, HANDLE hOutput, const std::atomic<bool>& bCancel)
	//-----------------------------------------------------------------------------
	: m_hInput  (hInput)
	, m_hOutput (hOutput)
	, m_bCancel (bCancel)
	{
	}

	//-----------------------------------------------------------------------------
	/// writes sText
	/// @return false if the reader has closed its end of the pipe
	bool Write(KStringView sText)
	//-----------------------------------------------------------------------------
	{
		while (!sText.empty())
		{
			DWORD iWritten { 0 };

			if (!::WriteFile(m_hOutput, sText.data(), static_cast<DWORD>(sText.size()), &iWritten, nullptr))
			{
				return false;
			}

			sText.remove_prefix(iWritten);
		}

		return true;

	} // Write

	//-----------------------------------------------------------------------------
	/// reads a line, and echoes the typed characters if bEcho
	/// @return false if the login was cancelled, the input has ended, or the user
	/// typed Ctrl-C or Ctrl-D
	bool ReadLine(KString& sLine, bool bEcho)
	//-----------------------------------------------------------------------------
	{
		sLine.clear();

		for (;;)
		{
			char ch;

			if (!ReadChar(ch))
			{
				return false;
			}

			if (m_bSkipLineFeed)
			{
				m_bSkipLineFeed = false;

				if (ch == '\n')
				{
					continue;
				}
			}

			switch (ch)
			{
				case '\r':
					// a terminal sends a carriage return for Enter, maybe followed by a line feed
					m_bSkipLineFeed = true;
					return Write("\r\n");

				case '\n':
					return Write("\r\n");

				case '\b':
				case '\x7f':
					if (!sLine.empty())
					{
						// removes the last UTF-8 character
						while (sLine.size() > 1 && (static_cast<unsigned char>(sLine.back()) & 0xC0) == 0x80)
						{
							sLine.remove_suffix(1);
						}

						sLine.remove_suffix(1);

						if (bEcho && !Write("\b \b"))
						{
							return false;
						}
					}
					break;

				case '\x03': // Ctrl-C
				case '\x04': // Ctrl-D
					return false;

				default:
					// other control characters are ignored, and a name or a password
					// has a limited length
					if (static_cast<unsigned char>(ch) >= 0x20 && sLine.size() < 256)
					{
						sLine += ch;

						if (bEcho && !Write(KStringView(&ch, 1)))
						{
							return false;
						}
					}
					break;
			}
		}

	} // ReadLine

//------
private:
//------

	//-----------------------------------------------------------------------------
	/// reads the next character - an anonymous pipe cannot be read with a timeout,
	/// therefore it is checked for data until the login is cancelled
	bool ReadChar(char& ch)
	//-----------------------------------------------------------------------------
	{
		while (m_iPos >= m_sBuffer.size())
		{
			if (m_bCancel)
			{
				return false;
			}

			DWORD iAvailable { 0 };

			if (!::PeekNamedPipe(m_hInput, nullptr, 0, nullptr, &iAvailable, nullptr))
			{
				// the writer has closed its end of the pipe
				return false;
			}

			if (!iAvailable)
			{
				::Sleep(20);
				continue;
			}

			m_sBuffer.resize(std::min<DWORD>(iAvailable, 256));

			DWORD iRead { 0 };

			if (!::ReadFile(m_hInput, &m_sBuffer[0], static_cast<DWORD>(m_sBuffer.size()), &iRead, nullptr))
			{
				return false;
			}

			m_sBuffer.resize(iRead);
			m_iPos = 0;
		}

		ch = m_sBuffer[m_iPos++];

		return true;

	} // ReadChar

	HANDLE                   m_hInput;
	HANDLE                   m_hOutput;
	const std::atomic<bool>& m_bCancel;
	KString                  m_sBuffer;
	std::size_t              m_iPos          { 0 };
	bool                     m_bSkipLineFeed { false };

}; // LoginTerminal

//-----------------------------------------------------------------------------
/// logs the user on with the password - a plain name is a local account,
/// DOMAIN\user and user@domain are domain accounts
/// @return the token of the user, or nullptr
HANDLE LogOn(KStringView sUser, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	std::wstring wsUser;
	std::wstring wsDomain;

	auto iSeparator = sUser.find('\\');

	if (iSeparator != KStringView::npos)
	{
		wsDomain = kutf::Convert<std::wstring>(sUser.substr(0, iSeparator));
		wsUser   = kutf::Convert<std::wstring>(sUser.substr(iSeparator + 1));
	}
	else
	{
		wsUser = kutf::Convert<std::wstring>(sUser);

		if (sUser.find('@') == KStringView::npos)
		{
			// only the local account database - user@domain needs no domain
			wsDomain = L".";
		}
	}

	auto wsPassword = kutf::Convert<std::wstring>(sPassword);

	HANDLE hToken { nullptr };

	bool bLoggedOn = ::LogonUserW(wsUser.c_str(), wsDomain.empty() ? nullptr : wsDomain.c_str(),
	                              wsPassword.c_str(), LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT,
	                              &hToken) != FALSE;

	auto iError = ::GetLastError();

	::SecureZeroMemory(&wsPassword[0], wsPassword.size() * sizeof(wchar_t));

	if (!bLoggedOn)
	{
		kDebug(1, "logon of '{}' failed: {}", sUser, KWindowsProcess::ErrorText(iError));
		return nullptr;
	}

	return hToken;

} // LogOn

//-----------------------------------------------------------------------------
/// returns the TOKEN_USER of a token, or an empty buffer
std::vector<char> TokenUser(HANDLE hToken)
//-----------------------------------------------------------------------------
{
	DWORD iSize { 0 };
	::GetTokenInformation(hToken, ::TokenUser, nullptr, 0, &iSize);

	std::vector<char> Buffer(iSize);

	if (!iSize || !::GetTokenInformation(hToken, ::TokenUser, Buffer.data(), iSize, &iSize))
	{
		Buffer.clear();
	}

	return Buffer;

} // TokenUser

//-----------------------------------------------------------------------------
/// returns true if hToken is one of the user of this process
bool IsUserOfThisProcess(HANDLE hToken)
//-----------------------------------------------------------------------------
{
	HANDLE hOwnToken { nullptr };

	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hOwnToken))
	{
		return false;
	}

	auto Own   = TokenUser(hOwnToken);
	auto Other = TokenUser(hToken);

	::CloseHandle(hOwnToken);

	return !Own.empty() && !Other.empty()
	    && ::EqualSid(reinterpret_cast<TOKEN_USER*>(Own.data())->User.Sid,
	                  reinterpret_cast<TOKEN_USER*>(Other.data())->User.Sid);

} // IsUserOfThisProcess

//-----------------------------------------------------------------------------
/// enables a privilege of this process
/// @return false if the process does not have the privilege
bool EnablePrivilege(const wchar_t* sPrivilege)
//-----------------------------------------------------------------------------
{
	HANDLE hToken { nullptr };

	if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		return false;
	}

	TOKEN_PRIVILEGES Privileges {};
	Privileges.PrivilegeCount           = 1;
	Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	// AdjustTokenPrivileges() succeeds also for a privilege the process does not
	// have - only the last error tells
	bool bEnabled = ::LookupPrivilegeValueW(nullptr, sPrivilege, &Privileges.Privileges[0].Luid)
	             && ::AdjustTokenPrivileges(hToken, FALSE, &Privileges, 0, nullptr, nullptr)
	             && ::GetLastError() == ERROR_SUCCESS;

	::CloseHandle(hToken);

	return bEnabled;

} // EnablePrivilege

//-----------------------------------------------------------------------------
/// returns the profile directory of the user of hToken, or an empty string
KString ProfileDirectory(HANDLE hToken)
//-----------------------------------------------------------------------------
{
	DWORD iSize { 0 };
	::GetUserProfileDirectoryW(hToken, nullptr, &iSize);

	if (!iSize)
	{
		return {};
	}

	std::wstring wsDirectory(iSize, L'\0');

	if (!::GetUserProfileDirectoryW(hToken, &wsDirectory[0], &iSize))
	{
		return {};
	}

	wsDirectory.resize(std::wcslen(wsDirectory.c_str()));

	return kutf::Convert<KString>(wsDirectory);

} // ProfileDirectory

//-----------------------------------------------------------------------------
/// loads the profile of the user of hToken, as a login does - needs privileges
/// that a service under LocalSystem has
/// @return the handle of the profile, or nullptr
HANDLE LoadProfile(HANDLE hToken, KStringView sUser)
//-----------------------------------------------------------------------------
{
	EnablePrivilege(L"SeBackupPrivilege");
	EnablePrivilege(L"SeRestorePrivilege");

	auto wsUser = kutf::Convert<std::wstring>(sUser);

	PROFILEINFOW Profile {};
	Profile.dwSize     = sizeof(Profile);
	Profile.dwFlags    = PI_NOUI;
	Profile.lpUserName = &wsUser[0];

	if (!::LoadUserProfileW(hToken, &Profile))
	{
		kDebug(1, "cannot load the profile of '{}': {}", sUser, KWindowsProcess::ErrorText(::GetLastError()));
		return nullptr;
	}

	return Profile.hProfile;

} // LoadProfile

//-----------------------------------------------------------------------------
/// pauses for Duration
/// @return false if the login was cancelled meanwhile
bool Pause(KDuration Duration, const std::atomic<bool>& bCancel)
//-----------------------------------------------------------------------------
{
	KStopTime Timer;

	while (Timer.elapsed() < Duration)
	{
		if (bCancel)
		{
			return false;
		}

		::Sleep(50);
	}

	return !bCancel;

} // Pause

} // end of anonymous namespace

//-----------------------------------------------------------------------------
bool KPTY::Open(LoginMode Mode,
				KStringView sShell,
				KDuration Timeout,
				const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	Close(); // ensure a previous PTY is closed

	m_iExitCode = 0;

	if (!ConPTY().IsAvailable())
	{
		kDebug(1, "pseudo consoles need Windows 10 version 1809 or later");
		return false;
	}

	// the input pipe of the pseudo console, and its output pipe
	HANDLE hInputRead   { nullptr };
	HANDLE hInputWrite  { nullptr };
	HANDLE hOutputRead  { nullptr };
	HANDLE hOutputWrite { nullptr };

	if (!::CreatePipe(&hInputRead, &hInputWrite, nullptr, 0))
	{
		kDebug(1, "cannot create the input pipe: {}", KWindowsProcess::ErrorText(::GetLastError()));
		return false;
	}

	if (!::CreatePipe(&hOutputRead, &hOutputWrite, nullptr, 0))
	{
		kDebug(1, "cannot create the output pipe: {}", KWindowsProcess::ErrorText(::GetLastError()));
		::CloseHandle(hInputRead);
		::CloseHandle(hInputWrite);
		return false;
	}

	m_iInputFD  = ::_open_osfhandle(reinterpret_cast<intptr_t>(hInputWrite), _O_BINARY);
	m_iOutputFD = ::_open_osfhandle(reinterpret_cast<intptr_t>(hOutputRead), _O_RDONLY | _O_BINARY);

	if (m_iInputFD < 0 || m_iOutputFD < 0)
	{
		kDebug(1, "cannot get file descriptors for the pseudo console: {}", ::strerror(errno));

		if (m_iInputFD  < 0) ::CloseHandle(hInputWrite);
		if (m_iOutputFD < 0) ::CloseHandle(hOutputRead);

		::CloseHandle(hInputRead);
		::CloseHandle(hOutputWrite);

		Close(chrono::milliseconds(0));
		return false;
	}

	// the shell is a command line on Windows - for an empty name or /bin/sh the
	// command interpreter, as for KInShell
	KString sCommandLine = (sShell.empty() || sShell == "/bin/sh")
	                     ? KWindowsProcess::CommandLine({ KWindowsProcess::CommandInterpreter() })
	                     : KString(sShell);

	if (Mode == Login)
	{
		// as /usr/bin/login on Unix: the login asks for the user and the password on
		// the pipes of the pseudo console, and then starts the shell
		m_bCancelLogin       = false;
		m_bLoginStartedShell = false;
		m_iLoginExitCode     = 0;
		m_bLoginActive       = true;
		m_LoginThread        = std::thread(&KPTY::RunLogin, this, hInputRead, hOutputWrite, std::move(sCommandLine), Environment);
	}
	else
	{
		auto iError = StartShell(hInputRead, hOutputWrite, sCommandLine, Environment);

		if (iError)
		{
			Close(chrono::milliseconds(0));

			m_iExitCode = (iError == ERROR_FILE_NOT_FOUND || iError == ERROR_PATH_NOT_FOUND)
			            ? DEKAF2_POPEN_COMMAND_NOT_FOUND
			            : static_cast<int>(iError);
			return false;
		}
	}

	KPTYIOStream::SetTimeout(Timeout);
	KPTYIOStream::open(m_iOutputFD, m_iInputFD);

	return KPTYStream::good();

} // Open

//-----------------------------------------------------------------------------
uint32_t KPTY::StartShell(void* hInputRead, void* hOutputWrite, KStringView sCommandLine,
                          const std::vector<std::pair<KString, KString>>& Environment,
                          KStringViewZ sWorkingDirectory, void* hUserToken)
//-----------------------------------------------------------------------------
{
	void* hPseudoConsole { nullptr };
	auto  hResult = ConPTY().Create(ConsoleSize(m_iRows, m_iCols), hInputRead, hOutputWrite, 0, &hPseudoConsole);

	// the pseudo console has its own copies of its ends of the pipes - else the
	// output pipe would not end when the pseudo console closes
	::CloseHandle(hInputRead);
	::CloseHandle(hOutputWrite);

	if (FAILED(hResult))
	{
		kDebug(1, "cannot create a pseudo console: {}", KWindowsProcess::ErrorText(static_cast<uint32_t>(hResult)));
		return static_cast<uint32_t>(hResult);
	}

	m_hPseudoConsole = hPseudoConsole;

	kDebug(2, "starting {} in a pseudo console", sCommandLine);

	return m_Process.StartInPseudoConsole(sCommandLine, m_hPseudoConsole, Environment, sWorkingDirectory, hUserToken);

} // StartShell

//-----------------------------------------------------------------------------
void KPTY::RunLogin(void* hInputRead, void* hOutputWrite, KString sCommandLine,
                    std::vector<std::pair<KString, KString>> Environment)
//-----------------------------------------------------------------------------
{
	// the end without a shell: with the exit code of /usr/bin/login after a failed
	// login, or -1 if Close() or Terminate() cancelled it
	auto EndWithoutShell = [&](int iExitCode)
	{
		::CloseHandle(hInputRead);
		::CloseHandle(hOutputWrite);

		std::lock_guard<std::mutex> Lock(m_LoginMutex);

		m_iLoginExitCode = m_bCancelLogin ? -1 : iExitCode;
		m_bLoginActive   = false;
	};

	LoginTerminal Terminal(hInputRead, hOutputWrite, m_bCancelLogin);

	HANDLE  hToken    { nullptr };
	KString sUser;
	int     iAttempts { 0 };

	while (!hToken && iAttempts < 3)
	{
		if (!Terminal.Write(kFormat("{} login: ", kGetHostname())) || !Terminal.ReadLine(sUser, true))
		{
			return EndWithoutShell(1);
		}

		sUser.Trim();

		if (sUser.empty())
		{
			// as with login, an empty name asks again
			continue;
		}

		KString sPassword;

		bool bRead = Terminal.Write("Password: ") && Terminal.ReadLine(sPassword, false);

		if (bRead)
		{
			hToken = LogOn(sUser, sPassword);
		}

		kSafeErase(sPassword);

		if (!bRead)
		{
			return EndWithoutShell(1);
		}

		++iAttempts;

		if (!hToken)
		{
			// a pause against the guessing of passwords, as with login
			if (!Pause(chrono::seconds(3), m_bCancelLogin) || !Terminal.Write("\r\nLogin incorrect\r\n"))
			{
				return EndWithoutShell(1);
			}
		}
	}

	if (!hToken)
	{
		return EndWithoutShell(1);
	}

	bool bOtherUser = !IsUserOfThisProcess(hToken);

	if (bOtherUser && !(EnablePrivilege(L"SeAssignPrimaryTokenPrivilege") && EnablePrivilege(L"SeIncreaseQuotaPrivilege")))
	{
		kDebug(1, "cannot start a shell for '{}' without the privileges of a service under LocalSystem", sUser);
		Terminal.Write("\r\nonly the user of this process can log in - another user needs a service under LocalSystem\r\n");
		::CloseHandle(hToken);
		return EndWithoutShell(1);
	}

	// the shell starts in the profile directory, as with login in the home directory
	auto   sProfileDirectory = ProfileDirectory(hToken);
	HANDLE hProfile          = bOtherUser ? LoadProfile(hToken, sUser) : nullptr;

	auto ReleaseToken = [&]()
	{
		if (hProfile)
		{
			::UnloadUserProfile(hToken, hProfile);
		}

		::CloseHandle(hToken);
	};

	std::lock_guard<std::mutex> Lock(m_LoginMutex);

	if (m_bCancelLogin)
	{
		ReleaseToken();
		::CloseHandle(hInputRead);
		::CloseHandle(hOutputWrite);
		m_iLoginExitCode = -1;
		m_bLoginActive   = false;
		return;
	}

	kDebug(1, "login of '{}'", sUser);

	// for the user of this process, the shell runs with the token of this process
	auto iError = StartShell(hInputRead, hOutputWrite, sCommandLine, Environment,
	                         sProfileDirectory, bOtherUser ? hToken : nullptr);

	if (iError)
	{
		ReleaseToken();

		m_iLoginExitCode = (iError == ERROR_FILE_NOT_FOUND || iError == ERROR_PATH_NOT_FOUND)
		                 ? DEKAF2_POPEN_COMMAND_NOT_FOUND
		                 : static_cast<int>(iError);
	}
	else if (bOtherUser)
	{
		// the profile stays loaded until the shell has ended
		m_hUserToken   = hToken;
		m_hUserProfile = hProfile;
	}
	else
	{
		ReleaseToken();
	}

	m_bLoginStartedShell = (iError == 0);
	m_bLoginActive       = false;

} // RunLogin

//-----------------------------------------------------------------------------
void KPTY::FinishLogin(bool bWait)
//-----------------------------------------------------------------------------
{
	if (!m_LoginThread.joinable() || (!bWait && m_bLoginActive))
	{
		return;
	}

	// after the join, all that the login thread wrote is visible
	m_LoginThread.join();

	if (!m_bLoginStartedShell)
	{
		m_iExitCode = m_iLoginExitCode;
	}

} // FinishLogin

//-----------------------------------------------------------------------------
void KPTY::ReleaseUser()
//-----------------------------------------------------------------------------
{
	if (m_hUserProfile)
	{
		::UnloadUserProfile(m_hUserToken, m_hUserProfile);
		m_hUserProfile = nullptr;
	}

	if (m_hUserToken)
	{
		::CloseHandle(m_hUserToken);
		m_hUserToken = nullptr;
	}

} // ReleaseUser

//-----------------------------------------------------------------------------
bool KPTY::IsRunning()
//-----------------------------------------------------------------------------
{
	if (m_bLoginActive)
	{
		return true;
	}

	FinishLogin(false);

	return KBaseProcess::IsRunning();

} // IsRunning

//-----------------------------------------------------------------------------
bool KPTY::Wait(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	KStopTime Timer;

	while (m_bLoginActive)
	{
		if (Timer.elapsed() >= Timeout)
		{
			return false;
		}

		::Sleep(10);
	}

	FinishLogin(false);

	auto Elapsed = Timer.elapsed();

	return KBaseProcess::Wait(Elapsed < Timeout ? Timeout - Elapsed : KDuration(chrono::milliseconds(0)));

} // Wait

//-----------------------------------------------------------------------------
bool KPTY::Terminate()
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(m_LoginMutex);

		if (m_bLoginActive)
		{
			// the login ends, and starts no shell
			m_bCancelLogin = true;
			return true;
		}
	}

	FinishLogin(false);

	return KBaseProcess::Terminate();

} // Terminate

//-----------------------------------------------------------------------------
pid_t KPTY::GetProcessID()
//-----------------------------------------------------------------------------
{
	if (m_bLoginActive)
	{
		return 0;
	}

	FinishLogin(false);

	return KBaseProcess::GetProcessID();

} // GetProcessID

//-----------------------------------------------------------------------------
int KPTY::Close(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	// invalidate the stream - we close the FDs ourselves
	KPTYStream::Cancel();

	// a running login starts no shell anymore
	m_bCancelLogin = true;

	// before Windows 11 24H2, ClosePseudoConsole() waits until the pseudo console has
	// written its last output - with our end of the output pipe closed first, the
	// pseudo console fails to write, instead of waiting for a reader. A running login
	// fails to read and to write as well.
	CloseAndResetFileDescriptor(m_iOutputFD);
	CloseAndResetFileDescriptor(m_iInputFD);

	FinishLogin(true);

	if (m_hPseudoConsole)
	{
		// like a hangup on Unix: the pseudo console ends the processes attached to it
		ConPTY().Close(m_hPseudoConsole);
		m_hPseudoConsole = nullptr;
	}

	WaitOrKill(Timeout);

	// the profile of a logged in user stays loaded until the shell has ended
	ReleaseUser();

	m_iRows = 25;
	m_iCols = 80;

	return m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
bool KPTY::Kill(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (!IsRunning())
	{
		return true;
	}

	// there is no SIGINT for a single process - terminate the shell right away,
	// together with all processes it started, or end the login
	Terminate();

	// call Close() which waits for the termination
	Close(Timeout);

	return true;

} // Kill

//-----------------------------------------------------------------------------
bool KPTY::SetWindowSize(uint16_t iRows, uint16_t iCols)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_LoginMutex);

	if (!m_hPseudoConsole && !m_bLoginActive)
	{
		return false;
	}

	// a running login creates the pseudo console with this size
	m_iRows = iRows;
	m_iCols = iCols;

	if (!m_hPseudoConsole)
	{
		return true;
	}

	// the pseudo console tells the shell about the size change itself
	auto hResult = ConPTY().Resize(m_hPseudoConsole, ConsoleSize(iRows, iCols));

	if (FAILED(hResult))
	{
		kDebug(1, "cannot set window size: {}", KWindowsProcess::ErrorText(static_cast<uint32_t>(hResult)));
		return false;
	}

	return true;

} // SetWindowSize

#else // DEKAF2_IS_WINDOWS

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
			kResetSignalHandlers();
			kUnblockAllSignals();

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

#endif // DEKAF2_IS_WINDOWS

DEKAF2_NAMESPACE_END

#endif
