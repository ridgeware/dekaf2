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

#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/core/init/kcompatibility.h>
#include <dekaf2/system/filesystem/bits/kfilesystem.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/system/process/kinshell.h>
#include <dekaf2/core/strings/kutf.h>             // for Windows API conversions
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/core/types/kctype.h>
#include <thread>
#include <cstdlib>
#include <cstring>         // for std::memcpy() - kSetThreadName()
#include <ctime>
#include <array>
#include <algorithm>       // for std::sort(), std::upper_bound() - kIsInsideDataSegment()
#include <vector>          // for kIsInsideDataSegment()

#ifdef DEKAF2_HAS_LIBPROC
	#include <libproc.h>                       // for proc_pidpath()
#endif
#ifdef DEKAF2_IS_WINDOWS
	#include <cwchar>
	#include <ws2tcpip.h>
	#include <sysinfoapi.h>    // for GetTickCount64()
	#include <consoleapi2.h>   // for GetConsoleScreenBufferInfo()
	#include <fileapi.h>       // for GetFinalPathNameByHandle()
	#include <io.h>            // for _get_osfhandle()
	#include <libloaderapi.h>  // for GetModuleFileName()
	#include <processthreadsapi.h> // for GetCurrentProcessorNumber()
	#include <dbghelp.h>       // for kIsInsideDataSegment()
	#include <charconv>        // for std::to_chars()
#else
	#include <unistd.h>        // for sysconf()
	#include <sys/types.h>     // for getpwuid(), sysctl()
	#include <pwd.h>           // for getpwuid()
	#include <arpa/inet.h>
	#include <sys/socket.h>    // for socket(), getsockopt(), SO_SNDBUF/SO_RCVBUF
	#include <sys/ioctl.h>     // for ioctl(), TIOCGWINSZ
	#include <grp.h>           // for getgrgid()
	#ifdef DEKAF2_IS_MACOS
		// MacOS
		#include <sys/sysctl.h>    // for sysctl()
		#include <sys/syslimits.h> // for MAX_PATH
		#include <fcntl.h>         // for fcntl()
		#ifdef DEKAF2_X86
			#include <cpuid.h>     // for cpuid()
		#endif
		#include <mach-o/getsect.h> // for kIsInsideDataSegment()
		#include <mach-o/dyld.h>
		#include <dlfcn.h>
	#else
		// Unix
		#include <sys/sysinfo.h> // for sysinfo()
		#include <sys/syscall.h>
		#include <sys/prctl.h>   // for prctl() - kGetThreadName()
		#include <limits.h>    // for MAX_PATH
		#if DEKAF2_HAS_INCLUDE(<link.h>)
			#include <link.h>  // for dl_iterate_phdr() - kIsInsideDataSegment()
			#define DEKAF2_HAS_DL_ITERATE_PHDR 1
		#endif
		#ifdef DEKAF2_GLIBC_VERSION
			#ifndef _GNU_SOURCE
				#define _GNU_SOURCE
			#endif
			#if DEKAF2_HAS_INCLUDE(<sched.h>)
				#include <sched.h>  // for sched_getcpu()
				#define DEKAF2_HAVE_SCHED_H 1
			#endif
			#if DEKAF2_HAS_INCLUDE(<pthread.h>)
				#include <pthread.h>  // for pthread_setaffinity_np()
				#define DEKAF2_HAVE_PTHREAD_H 1
			#endif
		#endif
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p szDefault if not found.
KStringViewZ kGetEnv (KStringViewZ szEnvVar, KStringViewZ szDefault)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	// getenv_s() returns ERANGE (and does NOT fill the buffer) when the value is
	// larger than the buffer - PATH routinely exceeds a few hundred bytes - so we
	// size first with a null buffer, then read into a buffer of the required size.
	static thread_local KString sBuffer;
	std::size_t iRequired { 0 };

	// query the required size (including the trailing NUL)
	getenv_s(&iRequired, nullptr, 0, szEnvVar.c_str());

	if (iRequired == 0)
	{
		// variable does not exist
		return szDefault;
	}

	sBuffer.resize(iRequired);
	std::size_t iRead { 0 };

	if (getenv_s(&iRead, &sBuffer[0], sBuffer.size(), szEnvVar.c_str()) != 0 || iRead == 0)
	{
		kWarning("cannot get {}", szEnvVar);
		return szDefault;
	}

	// drop the trailing NUL that getenv_s counted into iRead
	sBuffer.resize(iRead - 1);

	if (sBuffer.empty())
	{
		return szDefault;
	}

	return sBuffer;
#else
	KStringViewZ sValue = ::getenv(szEnvVar.c_str());

	if (!sValue.empty())
	{
		return (sValue);
	}
	else
	{
		return (szDefault);
	}
#endif

} // kGetEnv

//-----------------------------------------------------------------------------
/// Set environment variable.
bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ szValue)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	errno_t err = _putenv_s(szEnvVar.c_str(), szValue.c_str());
	if (err)
	{
		kWarning("cannot set {} = {}", szEnvVar, szValue);
	}
	bool bOK = !err;
#else
	if (szValue.empty())
	{
		return kUnsetEnv(szEnvVar);
	}
	bool bOK = (::setenv(szEnvVar.c_str(), szValue.c_str(), true) == 0);
	if (!bOK)
	{
		kWarning("cannot set {} = {}, {}", szEnvVar, szValue, strerror(errno));
	}
#endif  // DEKAF2_IS_WINDOWS
	return (bOK);

} // kSetEnv

//-----------------------------------------------------------------------------
bool kSetEnv (const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	for (const auto& env : Environment)
	{
		if (!kSetEnv(env.first, env.second))
		{
			return false;
		}
	}
	return true;

} // kSetEnv

//-----------------------------------------------------------------------------
/// Unset environment variable.
bool kUnsetEnv (KStringViewZ szEnvVar)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	return kSetEnv(szEnvVar, "");
#else
	bool bOK = (::unsetenv(szEnvVar.c_str()) == 0);
	if (!bOK)
	{
		kWarning("cannot unset {}, {}", szEnvVar, strerror(errno));
	}
	return (bOK);
#endif

} // kUnsetEnv

//-----------------------------------------------------------------------------
bool kSetCWD (KStringViewZ sPath)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;
	fs::current_path(kToFilesystemPath(sPath), ec);
	if (ec)
	{
		kWarning("cannot set current working directory: {}", ec.message());
		return false;
	}
	return true;
#else
	if (::chdir(sPath.c_str()))
	{
		kWarning("chdir failed: {}", strerror(errno));
		return false;
	}
	return true;
#endif

} // kSetCWD

//-----------------------------------------------------------------------------
KString kGetCWD ()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	std::error_code ec;
	KString sPath = fs::current_path(ec).string();
	if (ec)
	{
		kWarning("cannot get current working directory: {}", ec.message());
	}
	return sPath;
#else
	enum { MAX_PATH = 1024 };
	KString sPath(MAX_PATH, '\0');
	if (::getcwd(&sPath[0], sPath.size()-1))
	{
		size_t iPathLen = ::strlen(sPath.c_str());
		sPath.erase(iPathLen);
	}
	else
	{
		kWarning("cannot get current working directory: {}", strerror(errno));
		sPath.erase();
	}
	return sPath;
#endif

} // kGetCWD

//-----------------------------------------------------------------------------
KString kGetHome()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	KString sHome = kGetEnv("USERPROFILE");

	if (sHome.empty())
	{
		sHome = kGetEnv("HOMEDRIVE");

		if (!sHome.empty())
		{
			sHome += kGetEnv("HOMEPATH");
		}
	}
#else
	// HOME var is always the authoritative source for the home directory
	KString sHome = kGetEnv("HOME");

	if (sHome.empty())
	{
		// only if it is empty check getpwuid()
		auto ent = getpwuid(geteuid());

		if (ent)
		{
			sHome = ent->pw_dir;
		}
	}
#endif

	if (sHome.empty())
	{
		kWarning("cannot get home directory");
	}

	return sHome;

} // kGetHome

//-----------------------------------------------------------------------------
KString kGetTemp()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_STD_FILESYSTEM
	KString sTemp = fs::temp_directory_path().string();
#else
	KString sTemp = kGetEnv("TMPDIR");

	if (sTemp.empty())
	{
		sTemp = kGetEnv("TEMP");

		if (sTemp.empty())
		{
			sTemp = kGetEnv("TMP");

			if (sTemp.empty())
			{
#ifdef DEKAF2_IS_WINDOWS
				if (kDirExists("C:\\TEMP"))
				{
					sTemp = "C:\\TEMP";
				}
				else if (kDirExists("C:\\TMP"))
				{
					sTemp = "C:\\TMP";
				}
				else if (kDirExists("\\TEMP"))
				{
					sTemp = "\\TEMP";
				}
				else if (kDirExists("\\TMP"))
				{
					sTemp = "\\TMP";
				}
#else
				if (kDirExists("/tmp"))
				{
					sTemp = "/tmp";
				}
				else if (kDirExists("/var/tmp"))
				{
					sTemp = "/var/tmp";
				}
				else if (kDirExists("/usr/tmp"))
				{
					sTemp = "/usr/tmp";
				}
#endif
			}
		}

	}
#endif

	if (sTemp.empty())
	{
		kWarning("cannot get temp directory, setting to current directory");
		sTemp = kGetCWD();
	}

	if (sTemp.back() == kDirSep)
	{
		// the windows implementation of fs::temp_directory_path appends
		// a backslash to the path - we do not want that
		sTemp.remove_suffix(1);
	}

	return sTemp;

} // kGetHome

//-----------------------------------------------------------------------------
KString kGetWhoAmI ()
//-----------------------------------------------------------------------------
{
	KString sWhoami;

#ifdef DEKAF2_IS_WINDOWS

	enum { MAX = 100 };
	wchar_t szWhoami[MAX + 1] = { 0 };
	DWORD nSize = MAX;
	if (GetUserNameW (szWhoami, &nSize))
	{
		kutf::Convert(szWhoami, sWhoami);
	}

#else

	// on UNIX, we do *not* cache the kGetWhoami() result, to allow
	// identity changes through setuid():
	sWhoami = kGetUsername(kGetUid());

#endif

	kDebug (2, sWhoami);

	return (sWhoami);

} // kGetWhoami

//-----------------------------------------------------------------------------
KString kGetUsername (uint32_t iUID)
//-----------------------------------------------------------------------------
{
	KString sUsername;

#ifndef DEKAF2_IS_WINDOWS
	struct passwd* pPassStruct = getpwuid (iUID);

	if (pPassStruct)
	{
		sUsername = pPassStruct->pw_name;
	}
#endif

	return sUsername;

} // kGetUsername

//-----------------------------------------------------------------------------
KString kGetGroupname (uint32_t iGID)
//-----------------------------------------------------------------------------
{
	KString sGroupname;

#ifndef DEKAF2_IS_WINDOWS
	auto* pGroup = getgrgid(iGID);

	if (pGroup)
	{
		sGroupname = pGroup->gr_name;
	}
#endif

	return sGroupname;

} // kGetGroupname

//-----------------------------------------------------------------------------
KStringViewZ kGetHostname (bool bAllowKHostname/*=true*/)
//-----------------------------------------------------------------------------
{
	enum { MAXNAMELEN = 50 };

	struct Names
	{
		Names()
		{
			// set (and cache) /etc/khostname:
			if (kFileExists("/etc/khostname"))
			{
				sKHostname = kReadAll("/etc/khostname");
			}

			// set (and cache) OS hostname:

			#ifdef DEKAF2_IS_WINDOWS

			wchar_t szHostname[MAXNAMELEN + 1]{}; szHostname[0] = 0;
			DWORD nSize = MAXNAMELEN;
			GetComputerNameW (
				szHostname,       // name buffer
				&nSize              // address of size of name buffer
			);

			kutf::Convert(szHostname, sHostname);

			#else

			if (kFileExists ("/proc/sys/kernel/hostname"))
			{
				sHostname = kReadAll ("/proc/sys/kernel/hostname");
			}

			if (! sHostname)
			{
				char szHostname[MAXNAMELEN+1]; szHostname[0] = 0;
				gethostname (szHostname, MAXNAMELEN);
				sHostname = szHostname;
			}

			#endif

			sHostname.Trim();
			sKHostname.Trim();

			if (sKHostname.empty())
			{
				sKHostname = sHostname;
			}
		}

		KString sHostname;
		KString sKHostname;
	};

	static Names s_Names;

	return (bAllowKHostname) ? s_Names.sKHostname : s_Names.sHostname;

} // kGetHostname

//-----------------------------------------------------------------------------
uint64_t kGetTid()
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_IS_OSX)
	uint64_t TID;
	pthread_threadid_np(nullptr, &TID);
	return TID;
#elif defined(DEKAF2_IS_WINDOWS)
	return GetCurrentThreadId();
#else
	return syscall(SYS_gettid);
#endif

} // kGetTid

//-----------------------------------------------------------------------------
bool kSetThreadName (KStringView sName)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_IS_WINDOWS)

	// SetThreadDescription() exists since Windows 10 1607. dekaf2 targets Vista
	// (_WIN32_WINNT 0x0600, for inet_pton()), and even a Windows 10 target could
	// not express the 1607 line - therefore resolve the function dynamically,
	// and fail silently where it is missing
	using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);

	static SetThreadDescriptionFunc pSetThreadDescription = []() -> SetThreadDescriptionFunc
	{
		HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
		return hKernel32 ? reinterpret_cast<SetThreadDescriptionFunc>(GetProcAddress(hKernel32, "SetThreadDescription"))
		                 : nullptr;
	}();

	if (pSetThreadDescription == nullptr)
	{
		return false;
	}

	auto wsName = kutf::Convert<std::wstring>(sName);

	return SUCCEEDED(pSetThreadDescription(GetCurrentThread(), wsName.c_str()));

#else

	// the kernel's comm field is 16 bytes including the NUL - 15 visible chars.
	// Hard truncation is fine for a debugging label (a clipped UTF-8 tail included)
	std::array<char, 16> szName;
	auto iLen = std::min(sName.size(), szName.size() - 1);
	std::memcpy(szName.data(), sName.data(), iLen);
	szName[iLen] = 0;

	#if defined(DEKAF2_IS_MACOS)
		// MacOS can only name the CURRENT thread - single-argument variant
		return pthread_setname_np(szName.data()) == 0;
	#else
		return pthread_setname_np(pthread_self(), szName.data()) == 0;
	#endif

#endif

} // kSetThreadName

//-----------------------------------------------------------------------------
KString kGetThreadName ()
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_IS_WINDOWS)

	// GetThreadDescription() exists since Windows 10 1607 - resolved dynamically
	// for the same reason as SetThreadDescription() in kSetThreadName()
	using GetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PWSTR*);

	static GetThreadDescriptionFunc pGetThreadDescription = []() -> GetThreadDescriptionFunc
	{
		HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
		return hKernel32 ? reinterpret_cast<GetThreadDescriptionFunc>(GetProcAddress(hKernel32, "GetThreadDescription"))
		                 : nullptr;
	}();

	KString sName;

	if (pGetThreadDescription != nullptr)
	{
		PWSTR pwsName { nullptr };

		if (SUCCEEDED(pGetThreadDescription(GetCurrentThread(), &pwsName)) && pwsName != nullptr)
		{
			sName = kutf::Convert<KString>(pwsName);
			LocalFree(pwsName);
		}
	}

	return sName;

#elif defined(DEKAF2_IS_LINUX)

	// a single cheap syscall (~180ns, the same class as the gettid() in kGetTid()),
	// while glibc's pthread_getname_np() may read /proc/self/task/<tid>/comm
	std::array<char, 16> szName;

	if (prctl(PR_GET_NAME, szName.data()) != 0)
	{
		return KString{};
	}

	KString sName(szName.data());

	// unnamed threads inherit the process name on Linux - report them as unnamed,
	// so that callers can apply their own fallback (like klog's thread id)
	static const KString s_sProcessName = []() -> KString
	{
		KString sComm = kReadAll("/proc/self/comm");
		sComm.TrimRight();
		return sComm;
	}();

	if (sName == s_sProcessName)
	{
		sName.clear();
	}

	return sName;

#else

	// MacOS (and the BSDs): unnamed threads simply report an empty name
	std::array<char, 64> szName;

	if (pthread_getname_np(pthread_self(), szName.data(), szName.size()) != 0)
	{
		szName[0] = 0;
	}

	return KString(szName.data());

#endif

} // kGetThreadName

//-----------------------------------------------------------------------------
uint32_t kGetUid()
//-----------------------------------------------------------------------------
{
#if !defined(DEKAF2_IS_WINDOWS)
	return getuid();
#else
	return 0;
#endif

} // kGetUid

//-----------------------------------------------------------------------------
uint32_t kGetGid()
//-----------------------------------------------------------------------------
{
#if !defined(DEKAF2_IS_WINDOWS)
	return getgid();
#else
	return 0;
#endif

} // kGetGid

// It is preferable to use KInShell / popen for the call to system as it
// avoids the creation of a temporary output file. Also, on Unixes, KInShell
// actually is KInPipe, which uses fork and exec, and takes care to delete all
// parent file descriptors in the child to prevent descriptor leaking (which
// could lead to resource exhaustion or socket contention)

#define DEKAF2_USE_KSHELL_FOR_SYSTEM

//-----------------------------------------------------------------------------
int kSystem (KStringView sCommand, KStringRef& sOutput)
//-----------------------------------------------------------------------------
{
	sOutput.clear();
	sCommand.TrimRight();

	if (sCommand.empty())
	{
		kDebug(2, "error: empty command, returning EINVAL ({})", EINVAL);
		return EINVAL;
	}

#ifdef DEKAF2_USE_KSHELL_FOR_SYSTEM

	// construct and execute a shell (this one works on
	// Windows as well as on Linux)
	
	KString sWrapped;
	sWrapped.Format ("({}) 2>&1", sCommand);
	KInShell Shell(sWrapped);

	// read until EOF
	Shell.ReadRemaining(sOutput);

	kReplace(sOutput, "\r\n", "\n"); // DOS -> UNIX

	// close the shell and report the return value to the caller
	return Shell.Close();

#else // not DEKAF2_USE_KSHELL_FOR_SYSTEM

	// use shell output redirection to write into a temporary file
	// and read it thereafter

	KString sTmp;
	sTmp.Format ("{}{}ksystem{}_{}.out", kGetTemp(), kDirSep, kGetPid(), kGetTid());

	KString sWrapped;
	sWrapped.Format ("({}) > {} 2>&1", sCommand, sTmp);

	kDebug (3, sWrapped);

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	int iStatus = std::system (sWrapped.c_str());

	if (WIFEXITED(iStatus))
	{
		iStatus = WEXITSTATUS(iStatus);
		kDebug(2, "exited with return value {}", iStatus);
	}
	else if (WIFSIGNALED(iStatus))
	{
		int iSignal = WSTOPSIG(iStatus);
		if (iSignal)
		{
			kDebug(1, "aborted by signal {}", kTranslateSignal(iSignal));
		}
		iStatus = 1;
	}

	if (!kFileExists (sTmp))
	{
		kDebug (1, "outfile is missing: {}", sTmp);
	}
	else
	{
		kReadTextFile (sTmp, sOutput, true);
		kDebug (3, "output contained {} bytes", sOutput.size());
		kRemoveFile (sTmp);
	}

	return (iStatus);  // 0 => success

#endif // DEKAF2_USE_KSHELL_FOR_SYSTEM

} // ksystem

//-----------------------------------------------------------------------------
int kSystem (KStringView sCommand)
//-----------------------------------------------------------------------------
{
	sCommand.TrimRight();

	if (sCommand.empty())
	{
		return EINVAL;
	}

#ifdef DEKAF2_USE_KSHELL_FOR_SYSTEM

	// construct and execute a shell (this one works on
	// Windows as well as on Linux)

	KString sWrapped;
#ifdef DEKAF2_IS_WINDOWS
	sWrapped.Format ("({}) > {} 2>&1", sCommand, "NUL");
#else
	sWrapped.Format ("({}) > {} 2>&1", sCommand, "/dev/null");
#endif
	KInShell Shell(sWrapped);

	// close the shell and report the return value to the caller
	return Shell.Close();

#else //DEKAF2_USE_KSHELL_FOR_SYSTEM

	KString sWrapped;
#ifdef DEKAF2_IS_WINDOWS
	sWrapped.Format ("({}) > {} 2>&1", sCommand, "NUL");
#else
	sWrapped.Format ("({}) > {} 2>&1", sCommand, "/dev/null");
#endif

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	int iStatus = std::system (sWrapped.c_str());

	if (WIFEXITED(iStatus))
	{
		iStatus = WEXITSTATUS(iStatus);
		kDebug(2, "exited with return value {}", iStatus);
	}
	else if (WIFSIGNALED(iStatus))
	{
		int iSignal = WSTOPSIG(iStatus);
		if (iSignal)
		{
			kDebug(1, "aborted by signal {}", kTranslateSignal(iSignal));
		}
		iStatus = 1;
	}

	return (iStatus);  // 0 => success

#endif //DEKAF2_USE_KSHELL_FOR_SYSTEM

} // ksystem

//-----------------------------------------------------------------------------
void kSleep (KDuration duration)
//-----------------------------------------------------------------------------
{
	std::this_thread::sleep_for(duration);

} // kSleep

//-----------------------------------------------------------------------------
std::size_t kGetPageSize()
//-----------------------------------------------------------------------------
{

#ifndef DEKAF2_IS_WINDOWS

	static auto iPageSize = sysconf(_SC_PAGE_SIZE);

	return iPageSize;

#else

	static auto iPageSize = []() -> std::size_t
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		return sysInfo.dwPageSize;
	}();

	return iPageSize;

#endif

} // kGetPageSize

//-----------------------------------------------------------------------------
std::size_t kGetPhysicalMemory()
//-----------------------------------------------------------------------------
{

#ifndef DEKAF2_IS_WINDOWS

	static auto iPhysPages = sysconf(_SC_PHYS_PAGES);

	return kGetPageSize() * iPhysPages;

#else

	static auto iPhysMemory = []() -> std::size_t
	{
		MEMORYSTATUSEX status{};
		status.dwLength = sizeof(status);
		GlobalMemoryStatusEx(&status);
		return status.ullTotalPhys;
	}();

	return iPhysMemory;

#endif

} // kGetPhysicalMemory

//-----------------------------------------------------------------------------
uint16_t kGetCPUCount()
//-----------------------------------------------------------------------------
{

#ifndef DEKAF2_IS_WINDOWS

	static uint16_t iCPUCount = []() -> uint16_t
	{
		uint16_t iCount = sysconf(_SC_NPROCESSORS_ONLN);
		return iCount ? iCount : 1;
	}();

#else

	static uint16_t iCPUCount = []() -> uint16_t
	{
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		return static_cast<uint16_t>(sysInfo.dwNumberOfProcessors ? sysInfo.dwNumberOfProcessors : 1);
	}();

#endif

	return iCPUCount;

} // kGetCPUCount

//-----------------------------------------------------------------------------
uint16_t kGetCPU()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_OSX

	#ifdef DEKAF2_X86
	uint32_t iInfo[4];

	// this is expensive, as it calls the cpuid instruction which
	// may take from 150 - 800 cycles to complete
	__cpuid_count(1, 0, iInfo[0], iInfo[1], iInfo[2], iInfo[3]);

	if ((iInfo[3] & (1 << 9)) != 0)
	{
		return (iInfo[1] >> 24);
	}
	#elif DEKAF2_ARM64
	// we currently lack a method to detect the CPU on apple arm
	#endif

#elif DEKAF2_HAVE_SCHED_H

	int iCPU = sched_getcpu();

	if (iCPU >= 0)
	{
		kDebug(2, "sched_getcpu: {}", strerror(errno));
		iCPU = 0;
	}

	return iCPU;

#elif DEKAF2_IS_WINDOWS

	// this is only supported from Windows Vista onward
	return static_cast<uint16_t>(GetCurrentProcessorNumber());

#endif

	kDebug(2, "unsupported operating system");

	return 0;

} // kGetCPU

//-----------------------------------------------------------------------------
std::vector<uint16_t> kGetThreadCPU(const pthread_t& forThread)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_PTHREAD_H

	cpu_set_t cpuset;

	auto err = pthread_getaffinity_np(forThread, sizeof(cpuset), &cpuset);

	if (err != 0)
	{
		kDebug(2, "pthread_getaffinity_np: {}", strerror(err));
	}

	std::vector<uint16_t> CPUs;

	for (uint16_t j = 0; j < CPU_SETSIZE; ++j)
	{
		if (CPU_ISSET(j, &cpuset))
		{
			CPUs.push_back(j);
		}
	}

	return CPUs;

#else

	kDebug(2, "unsupported operating system");

	return std::vector<uint16_t>();

#endif

} // kGetThreadCPU

//-----------------------------------------------------------------------------
std::vector<uint16_t> kGetThreadCPU(std::thread& forThread)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_PTHREAD_H

	return kGetThreadCPU(forThread.native_handle());

#else

	kDebug(2, "unsupported operating system");

	return std::vector<uint16_t>();

#endif

} // kGetThreadCPU

//-----------------------------------------------------------------------------
std::vector<uint16_t> kGetThreadCPU()
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_PTHREAD_H

	return kGetThreadCPU(pthread_self());

#else

	kDebug(2, "unsupported operating system");

	return std::vector<uint16_t>();

#endif

} // kGetThreadCPU

//-----------------------------------------------------------------------------
bool kSetProcessCPU(const std::vector<uint16_t>& CPUs, pid_t forPid)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_SCHED_H

	const auto iCPUCount = kGetCPUCount();
	cpu_set_t set;
	CPU_ZERO(&set);

	for (auto ID : CPUs)
	{
		if (ID < iCPUCount)
		{
			CPU_SET(ID, &set);
		}
		else
		{
			kDebug(2, "ID {} exceeds max CPU ID {}", ID, iCPUCount);
		}
	}

	if (sched_setaffinity(forPid, sizeof(set), &set) == -1)
	{
		kDebug(2, "sched_setaffinity: {}", strerror(errno));
		return false;
	}

	return true;

#endif

	kDebug(2, "unsupported operating system");

	return false;

} // kSetProcessCPU

//-----------------------------------------------------------------------------
bool kSetThreadCPU(const std::vector<uint16_t>& CPUs, const pthread_t& forThread)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_PTHREAD_H

	const auto iCPUCount = kGetCPUCount();
	cpu_set_t set;
	CPU_ZERO(&set);

	for (auto ID : CPUs)
	{
		if (ID < iCPUCount)
		{
			CPU_SET(ID, &set);
		}
		else
		{
			kDebug(2, "ID {} exceeds max CPU ID {}", ID, iCPUCount);
		}
	}

	int err = pthread_setaffinity_np(forThread, sizeof(set), &set);

	if (err != 0)
	{
		kDebug(2, "pthread_setaffinity_np: {}", strerror(err));
		return false;
	}

	return true;

#endif

	kDebug(2, "unsupported operating system");

	return false;

} // kSetThreadCPU

//-----------------------------------------------------------------------------
bool kSetThreadCPU(const std::vector<uint16_t>& CPUs, std::thread& forThread)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_PTHREAD_H

	return kSetThreadCPU(CPUs, forThread.native_handle());

#endif

	kDebug(2, "unsupported operating system");

	return false;

} // kSetThreadCPU

//-----------------------------------------------------------------------------
bool kSetThreadCPU(const std::vector<uint16_t>& CPUs)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAVE_PTHREAD_H

	return kSetThreadCPU(CPUs, pthread_self());

#endif

	kDebug(2, "unsupported operating system");

	return false;

} // kSetThreadCPU

#ifdef DEKAF2_IS_WINDOWS

struct rusage
{
	struct timeval ru_utime;
	struct timeval ru_stime;
};

namespace detail {

//-----------------------------------------------------------------------------
DEKAF2_PRIVATE
void FiletimeToTimeval(struct timeval& tv, const FILETIME& ft)
//-----------------------------------------------------------------------------
{
	ULARGE_INTEGER time{};
	time.LowPart   = ft.dwLowDateTime;
	time.HighPart  = ft.dwHighDateTime;
	time.QuadPart /= 10L;
	tv.tv_sec      = static_cast<decltype(tv.tv_sec )>(time.QuadPart / 1000000L);
	tv.tv_usec     = static_cast<decltype(tv.tv_usec)>(time.QuadPart % 1000000L);

} // FiletimeToTimeval

} // namespace detail

//-----------------------------------------------------------------------------
int getrusage(int who, struct rusage *rusage)
//-----------------------------------------------------------------------------
{
	if (!rusage)
	{
		errno = EFAULT;
		return -1;
	}

	switch (who)
	{
		case RUSAGE_SELF:
		{
			FILETIME starttime, exittime, kerneltime, usertime;

			if (!GetProcessTimes(GetCurrentProcess(), &starttime, &exittime, &kerneltime, &usertime))
			{
				return -1;
			}

			detail::FiletimeToTimeval(rusage->ru_stime, kerneltime);
			detail::FiletimeToTimeval(rusage->ru_utime, usertime  );

			return 0;
		}

		case RUSAGE_THREAD:
		{
			FILETIME starttime, exittime, kerneltime, usertime;

			if (!GetThreadTimes(GetCurrentThread(), &starttime, &exittime, &kerneltime, &usertime))
			{
				return -1;
			}

			detail::FiletimeToTimeval(rusage->ru_stime, kerneltime);
			detail::FiletimeToTimeval(rusage->ru_utime, usertime  );

			return 0;
		}

		default:
			break;
	}

	errno = EINVAL;
	return -1;

} // getrusage for Windows

#endif // DEKAF2_IS_WINDOWS

namespace detail {

//-----------------------------------------------------------------------------
DEKAF2_PRIVATE
std::size_t TimeValToMicroTicks(const struct timeval& tv)
//-----------------------------------------------------------------------------
{
	return static_cast<std::size_t>(tv.tv_sec) * 1000000 + tv.tv_usec;

} // TimeValToMicroTicks

//-----------------------------------------------------------------------------
std::size_t TicksFromRusage(int who)
//-----------------------------------------------------------------------------
{
	struct rusage ru;

	if (getrusage(who, &ru))
	{
		return 0;
	}

	return TimeValToMicroTicks(ru.ru_stime)
		 + TimeValToMicroTicks(ru.ru_utime);

} // TimeValToMicroTicks

} // namespace detail

//-----------------------------------------------------------------------------
bool kSetGlobalLocale(const std::locale& locale)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		std::locale::global(locale);

		kDebug(1, "changed global locale to {}", locale.name());
	}
	DEKAF2_CATCH (const std::exception& ex)
	{
		kDebug(1, "failed to change global locale to {}: {}", locale.name(), ex.what());

		return false;
	}

	return true;

} // kSetGlobalLocale

//-----------------------------------------------------------------------------
bool kSetGlobalLocale(KStringViewZ sLocale)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		return kSetGlobalLocale(std::locale(sLocale.c_str()));
	}
	DEKAF2_CATCH (const std::exception& ex)
	{
		kDebug(1, "failed to change global locale to {}: {}", sLocale, ex.what());
	}

	return false;

} // kSetGlobalLocale

//-----------------------------------------------------------------------------
std::locale kGetGlobalLocale()
//-----------------------------------------------------------------------------
{
	return std::locale();

} // kGetGlobalLocale

//-----------------------------------------------------------------------------
std::locale kGetLocale(KStringViewZ sLocale, bool bThrow)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		return std::locale(sLocale);
	}
	DEKAF2_CATCH (const std::exception& ex)
	{
		kDebug(1, "failed to get locale for {}: {}", sLocale, ex.what());

		if (bThrow)
		{
			throw KException(ex.what());
		}

		return std::locale();
	}

} // kGetLocale

//-----------------------------------------------------------------------------
char kGetDecimalPoint(const std::locale& locale)
//-----------------------------------------------------------------------------
{
	if (std::has_facet<std::numpunct<char>>(locale))
	{
		return std::use_facet<std::numpunct<char>>(locale).decimal_point();
	}
	else
	{
		return '.';
	}

} // kGetDecimalPoint

//-----------------------------------------------------------------------------
char kGetThousandsSeparator(const std::locale& locale)
//-----------------------------------------------------------------------------
{
	if (std::has_facet<std::numpunct<char>>(locale))
	{
		return std::use_facet<std::numpunct<char>>(locale).thousands_sep();
	}
	else
	{
		return ',';
	}

} // kGetThousandsSeparator

//-----------------------------------------------------------------------------
const KString& kGetOwnPathname()
//-----------------------------------------------------------------------------
{
	static KString sPathname = []() -> KString
	{
		KString sPath;

#ifdef DEKAF2_HAS_LIBPROC

		// get the own executable's path and name through the libproc abstraction
		// which has the advantage that it also works on systems without /proc file system
		std::array<char, PROC_PIDPATHINFO_MAXSIZE> aPath;

		if (proc_pidpath(kGetPid(), aPath.data(), aPath.size()) >= 0)
		{
			sPath.assign(aPath.data(), strnlen(aPath.data(), aPath.size()));
		}
		else
		{
			sPath.clear();
		}

#elif DEKAF2_IS_UNIX

		// get the own executable's path and name through the /proc file system
		std::array<char, PATH_MAX> aPath;
		ssize_t len;

		if ((len = readlink("/proc/self/exe", aPath.data(), aPath.size())) > 0)
		{
			sPath.assign(aPath.data(), len);
		}
		else
		{
			sPath.clear();
		}

#elif DEKAF2_IS_WINDOWS

		// get the own executable's path and name through GetModuleFileName()

		HMODULE hModule = GetModuleHandle(nullptr);

		if (hModule != nullptr)
		{
			std::wstring wsPath;
			wsPath.resize(MAX_PATH);

			GetModuleFileNameW(hModule, wsPath.data(), static_cast<DWORD>(wsPath.size()));
			wsPath.resize(wcsnlen(wsPath.data(), wsPath.size()));

			kutf::Convert(wsPath, sPath);
			// the API returns the filename with prefixed "\\?\"
			sPath.remove_prefix("\\\\?\\");
		}
		else
		{
			sPath.clear();
		}

#endif

		return sPath;

	}();

	return sPathname;

} // kGetOwnPathname

//-----------------------------------------------------------------------------
bool kIsInsideContainer()
//-----------------------------------------------------------------------------
{
	static bool bResult = []() -> bool
	{
#ifdef DEKAF2_IS_UNIX
		// check for Docker marker file
		if (kFileExists("/.dockerenv")) return true;
		// check for Podman marker file
		if (kFileExists("/run/.containerenv")) return true;
		// check for container env var (set by podman, systemd-nspawn, lxc)
		if (!kGetEnv("container").empty()) return true;
		// check PID 1 cgroup for container runtime markers (cgroup v1)
		KString sCGroup;
		if (kReadFile("/proc/1/cgroup", sCGroup, false))
		{
			for (auto sMarker : { "/docker/", "/lxc/", "/kubepods/", "/containerd/" })
			{
				if (sCGroup.contains(sMarker)) return true;
			}
		}
#endif
		return false;
	}();

	return bResult;

} // kIsInsideContainer

//-----------------------------------------------------------------------------
const KString& kGetConfigPath(bool bCreateDirectory)
//-----------------------------------------------------------------------------
{
	static KString sPathname = []() -> KString
	{
		auto sHome = kGetHome();

#if DEKAF2_IS_UNIX
		if (kIsInsideContainer() || sHome.empty() || sHome == "/")
		{
			kDebug(2, "container environment detected (HOME is '{}'), using /home/{}", sHome, kBasename(kGetOwnPathname()));
			// set the path to /home/{{program name }}
			return kFormat("{}home{}{}", kDirSep, kDirSep, kBasename(kGetOwnPathname()));
		}
		else
#endif
		{
			// ${HOME}/.config/{{program name}}
			return kFormat("{}{}.config{}{}", sHome, kDirSep, kDirSep, kBasename(kGetOwnPathname()));
		}
	}();

	if (bCreateDirectory)
	{
		if (!kDirExists(sPathname))
		{
			kCreateDir(sPathname, DEKAF2_MODE_CREATE_CONFIG_DIR, true);
		}
	}

	return sPathname;

} // kGetConfigPath

//-----------------------------------------------------------------------------
HANDLE kGetHandleFromFileDescriptor(int fd)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	return reinterpret_cast<HANDLE>(_get_osfhandle(fd));
#else
	return static_cast<HANDLE>(fd);
#endif

} // kGetHandleFromFileDescriptor

//-----------------------------------------------------------------------------
int kGetFileDescriptorFromHandle(HANDLE handle)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS
	return _open_osfhandle(reinterpret_cast<intptr_t>(handle), 0);
#else
	return static_cast<int>(handle);
#endif

} // kGetFileDescriptorFromHandle

//-----------------------------------------------------------------------------
KString kGetFileNameFromFileDescriptor(int fd)
//-----------------------------------------------------------------------------
{

#ifdef DEKAF2_IS_WINDOWS

	return kGetFileNameFromFileHandle(kGetHandleFromFileDescriptor(fd));

#else

	KString sFileName;

#ifdef DEKAF2_IS_OSX

	std::array<char, PATH_MAX + 1> buffer;

	if (fcntl(fd, F_GETPATH, buffer.data()) != -1)
	{
		sFileName = buffer.data();
	}

#elif DEKAF2_IS_UNIX

	std::array<char, PATH_MAX> buffer;

	auto iSize = readlink(kFormat("/proc/self/fd/{}", fd).c_str(), buffer.data(), buffer.size());

	if (iSize < 0)
	{
		kDebug(1, strerror(errno));
	}
	else
	{
		sFileName.assign(buffer.data(), iSize);
	}

#endif

	if (sFileName.empty())
	{
		kDebug(1, "cannot find filename for fd {}: {}", fd, strerror(errno));
	}
	else
	{
		kDebug(2, "fd {} == {}", fd, sFileName);
	}

	return sFileName;

#endif

} // kGetFileNameFromFileDescriptor

//-----------------------------------------------------------------------------
KString kGetFileNameFromFileHandle(HANDLE handle)
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_IS_WINDOWS

	return kGetFileNameFromFileDescriptor(kGetFileDescriptorFromHandle(handle));

#else

	std::wstring sBuffer;
	sBuffer.resize(MAX_PATH);

	auto dwRet = GetFinalPathNameByHandleW(handle, sBuffer.data(), static_cast<DWORD>(sBuffer.size()), VOLUME_NAME_DOS);

	if (dwRet > sBuffer.size())
	{
		sBuffer.resize(dwRet);

		dwRet = GetFinalPathNameByHandleW(handle, sBuffer.data(), static_cast<DWORD>(sBuffer.size()), VOLUME_NAME_DOS);
	}

	sBuffer.resize(dwRet);

	KString sFileName = kutf::Convert<KString>(sBuffer);

	// the API returns the filename with prefixed "\\?\" (which signals to windows
	// file APIs that a path name may have up to 2^15 chars instead of 2^8)
	sFileName.remove_prefix("\\\\?\\");

	if (sFileName.empty())
	{
		kDebug(1, "cannot find filename for handle {}", handle);
	}
	else
	{
		kDebug(2, "handle {} == {}", handle, sFileName);
	}

	return sFileName;

#endif

} // kGetFileNameFromFileHandle

//-----------------------------------------------------------------------------
KTTYSize kGetTerminalSize(int fd, uint16_t iDefaultColumns, uint16_t iDefaultLines)
//-----------------------------------------------------------------------------
{
	KTTYSize TTY;

#ifndef DEKAF2_IS_WINDOWS

	bool bIsStdIO = fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO;

	struct winsize ts;

	if (bIsStdIO)
	{
		// try all three std filenos if one fails
		if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ts) != -1 ||
			 ioctl(STDIN_FILENO , TIOCGWINSZ, &ts) != -1 ||
			 ioctl(STDERR_FILENO, TIOCGWINSZ, &ts) != -1)
			&& ts.ws_col > 0 && ts.ws_row > 0)
		{
			TTY.columns = ts.ws_col;
			TTY.lines   = ts.ws_row;
		}
		else
		{
			KString sOutput;

			kSystem("stty size", sOutput);

			auto sCols = sOutput.Split(' ');

			if (sCols.size() == 2)
			{
				TTY.lines   = sCols[0].UInt16();
				TTY.columns = sCols[1].UInt16();
			}
		}
	}
	else
	{
		if (ioctl(fd, TIOCGWINSZ, &ts) != -1 && ts.ws_col && ts.ws_row)
		{
			TTY.columns = ts.ws_col;
			TTY.lines   = ts.ws_row;
		}
		else
		{
			KString sOutput;

			auto sName = kGetFileNameFromFileDescriptor(fd);

			if (!sName.empty())
			{
#if DEKAF2_IS_MACOS
				kSystem(kFormat("stty -f '{}' size", sName), sOutput);
#else
				kSystem(kFormat("stty -F '{}' size", sName), sOutput);
#endif
			}

			auto sCols = sOutput.Split(' ');

			if (sCols.size() == 2)
			{
				TTY.lines   = sCols[0].UInt16();
				TTY.columns = sCols[1].UInt16();
			}
		}
	}

#else // is windows

	bool bIsStdIO = fd == STD_INPUT_HANDLE || fd == STD_OUTPUT_HANDLE || fd == STD_ERROR_HANDLE;

	CONSOLE_SCREEN_BUFFER_INFO csbi;

	if (bIsStdIO)
	{
		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) ||
			GetConsoleScreenBufferInfo(GetStdHandle(STD_INPUT_HANDLE ), &csbi) ||
			GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE ), &csbi))
		{
			TTY.columns = csbi.srWindow.Right  - csbi.srWindow.Left;
			TTY.lines   = csbi.srWindow.Bottom - csbi.srWindow.Top;
		}
	}
	else
	{
		if (GetConsoleScreenBufferInfo(GetStdHandle(fd), &csbi))
		{
			TTY.columns = csbi.srWindow.Right  - csbi.srWindow.Left;
			TTY.lines   = csbi.srWindow.Bottom - csbi.srWindow.Top;
		}
	}

#endif

	if ((!TTY.columns || !TTY.lines) && bIsStdIO)
	{
		TTY.columns = kGetEnv("COLUMNS").UInt16();
		TTY.lines   = kGetEnv("LINES").UInt16();
	}

	if (!TTY.columns || !TTY.lines)
	{
		kDebug(1, "cannot read terminal size, using defaults");

		TTY.columns = iDefaultColumns;
		TTY.lines   = iDefaultLines;
	}

	kDebug(2, "terminal size: {}x{}", TTY.columns, TTY.lines);

	return TTY;

} // kGetTerminalSize

//-----------------------------------------------------------------------------
KString kWhich(KStringView sCommand)
//-----------------------------------------------------------------------------
{
	if (sCommand.empty())
	{
		return {};
	}

	// validate that sCommand only contains safe characters
	for (auto ch : sCommand)
	{
		if (!KASCII::kIsAlNum(ch) && ch != '-' && ch != '_' && ch != '.')
		{
			kDebug(2, "invalid character '{}' in command name '{}'", ch, sCommand);
			return {};
		}
	}

#ifdef DEKAF2_IS_WINDOWS
	static constexpr KStringViewZ sPathExtDefault = ".com;.exe;.bat;.cmd";
#endif

	auto fIsExecutable = [](KStringViewZ sPath) -> bool
	{
#ifdef DEKAF2_IS_WINDOWS
		return kFileExists(sPath);
#else
		return ::access(sPath.c_str(), X_OK) == 0;
#endif
	};

	auto sPathEnv = kGetEnv("PATH");

	if (sPathEnv.empty())
	{
		return {};
	}

#ifdef DEKAF2_IS_WINDOWS
	static constexpr char sPathSeparator = ';';
#else
	static constexpr char sPathSeparator = ':';
#endif

	KString sFullPath;
#ifdef DEKAF2_IS_WINDOWS
	KString sExtPath;
#endif

	// iterate through all directories in PATH
	for (auto sDir : KStringView(sPathEnv).Split(sPathSeparator))
	{
		if (sDir.empty())
		{
			continue;
		}

		sFullPath = sDir;

		if (sFullPath.back() != kDirSep)
		{
			sFullPath += kDirSep;
		}

		sFullPath += sCommand;

		if (fIsExecutable(sFullPath))
		{
			return sFullPath;
		}

#ifdef DEKAF2_IS_WINDOWS
		// on Windows, try appending PATHEXT extensions
		auto sPathExt = kGetEnv("PATHEXT", sPathExtDefault);

		for (auto sExtPart : KStringView(sPathExt).Split(sPathSeparator))
		{
			sExtPath  = sFullPath;
			sExtPath += sExtPart;

			if (fIsExecutable(sExtPath))
			{
				return sExtPath;
			}
		}
#endif
	}

	return {};

} // kWhich

DEKAF2_NAMESPACE_END

#if !defined(DEKAF2_IS_MACOS) && defined(DEKAF2_IS_UNIX) && !defined(DEKAF2_HAS_DL_ITERATE_PHDR)
extern char _etext;
extern char _edata;
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool kIsInsideDataSegment(const void* addr)
//-----------------------------------------------------------------------------
{
// enable this define to see more debugging output - it is normally undefined
// as the output is hard to make visible on demand (option parsing triggers
// the segment detection, and thus makes debug output visible only after
// this code has run..)
// #define DEKAF2_DEBUG_SEGMENT_DETECTION

#if defined(DEKAF2_IS_UNIX) && !defined(DEKAF2_IS_MACOS) && !defined(DEKAF2_HAS_DL_ITERATE_PHDR)

	// no dl_iterate_phdr() on this system - check the preinitialized data of
	// the main executable through the classic linker symbols (shared libraries
	// are not covered)
	return ((&_edata > addr) && (&_etext <= addr));

#elif defined(DEKAF2_IS_WINDOWS)                 \
   || defined(DEKAF2_HAS_DL_ITERATE_PHDR)        \
   || defined(DEKAF2_IS_MACOS)

	struct Segment
	{
		const char* start;
		const char* end;
	};

	using Segments = std::vector<Segment>;

	// the segment list is collected only once. Objects loaded later through
	// dlopen() are not seen, their constants count as dynamic (which is the
	// safe direction for all users of this predicate). The reverse case is a
	// known weakness: a library that was present at collection time but gets
	// unloaded through dlclose() leaves its stale ranges in this list, and a
	// heap or mmap allocation that later reuses such an address range would
	// falsely count as constant. This is accepted because start-time
	// libraries are effectively never unloaded (dlclose is a no-op for
	// libraries with NODELETE, unique symbols, or TLS, and programs do not
	// unload their link-time dependencies) - should that ever change, the
	// fix is a remove-image hook resp. a rebuild on dlclose, at the price of
	// synchronization around every lookup
	static const Segments DataSegments = []() -> Segments
	{
		Segments Data;

#ifdef DEKAF2_IS_WINDOWS

		// setup pointers
		HANDLE Self                         = GetModuleHandle(nullptr);
		IMAGE_NT_HEADERS* NtHeader          = ImageNtHeader(Self);
		IMAGE_SECTION_HEADER* SectionHeader = reinterpret_cast<IMAGE_SECTION_HEADER*>(NtHeader + 1);
		const char* SelfImage               = static_cast<const char*>(Self);

		// iterate through the image sections of the main module - the .rdata
		// segment contains const data and also seems to contain the .data
		// section on contemporary windows
		for (int iSection = 0;
			 iSection < NtHeader->FileHeader.NumberOfSections;
			 ++iSection, ++SectionHeader)
		{
			if (!strncmp(".rdata",
						 reinterpret_cast<const char*>(SectionHeader->Name),
						 sizeof(SectionHeader->Name)))
			{
				const char* start = SelfImage + SectionHeader->VirtualAddress;
				Data.push_back( { start, start + SectionHeader->Misc.VirtualSize } );
				break;
			}
		}

#elif defined(DEKAF2_IS_MACOS)

	#ifdef DEKAF2_IS_64_BITS

		using mach_header_bits        = mach_header_64;
		using segment_command_bits    = segment_command_64;
		using section_bits            = section_64;

		constexpr uint32_t iSegmentID = LC_SEGMENT_64;
		constexpr uint32_t iMagic     = MH_MAGIC_64;

	#elif DEKAF2_IS_32_BITS

		using mach_header_bits        = mach_header;
		using segment_command_bits    = segment_command;
		using section_bits            = section;

		constexpr uint32_t iSegmentID = LC_SEGMENT;
		constexpr uint32_t iMagic     = MH_MAGIC;

	#else
		#error "MacOS with unknown pointer width"
	#endif

		// iterate over all loaded images: the main executable, all its dylibs,
		// and dekaf2 itself when built as a dylib
		for (uint32_t iImage = 0, iImages = _dyld_image_count(); iImage < iImages; ++iImage)
		{
			const auto* MachHeader = reinterpret_cast<const mach_header_bits*>(_dyld_get_image_header(iImage));

			// check if it is valid
			if (!MachHeader || MachHeader->magic != iMagic)
			{
				continue;
			}

	#ifdef MH_DYLIB_IN_CACHE
			if (MachHeader->flags & MH_DYLIB_IN_CACHE)
			{
				// skip the hundreds of system libraries mapped from the dyld
				// shared cache - constants tested with this predicate do not
				// live there
				continue;
			}
	#endif

			// sections record their linked address - add the image slide to
			// get the loaded address
			auto iSlide = _dyld_get_image_vmaddr_slide(iImage);

			// get the first in a series of load commands
			const auto* LoadCommand = reinterpret_cast<const load_command*>(MachHeader + 1);

			// and iterate across it
			for (auto iCommands = MachHeader->ncmds; iCommands--;)
			{
				// check if this is a segment load command
				if (LoadCommand->cmd == iSegmentID)
				{
					// and if yes, cast to the details
					const auto* SegmentCommand = reinterpret_cast<const segment_command_bits*>(LoadCommand);
					const auto* Section        = reinterpret_cast<const section_bits*>(SegmentCommand + 1);

					// iterate across all sections inside this segment
					for (uint32_t iSection = 0; iSection < SegmentCommand->nsects; ++iSection, ++Section)
					{
						// create easy access to names for segment and section
						KStringView sSegName (Section->segname,  strnlen(Section->segname,  sizeof(Section->segname )));
						KStringView sSectName(Section->sectname, strnlen(Section->sectname, sizeof(Section->sectname)));

						// select only the sections with preinitialized data
						if ((sSegName == "__DATA" && !sSectName.ends_with("_bss")) ||
							(sSegName == "__DATA_CONST") ||
							(sSegName == "__TEXT"        &&
							   // MacOS puts a few things into the text segment that normally
							   // go into data - include it here as part of data
							   (sSectName.ends_with("_const") ||
								sSectName.ends_with("_cstring"))
							 )
							)
						{
							const char* start = reinterpret_cast<const char*>(static_cast<uintptr_t>(Section->addr) + iSlide);
							Data.push_back( { start, start + Section->size } );
						}
					}
				}

				// iterate to next load command
				LoadCommand = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(LoadCommand) + LoadCommand->cmdsize);
			}
		}

#else // DEKAF2_HAS_DL_ITERATE_PHDR

		// collect all preinitialized (file-backed) load segments of the main
		// program and of all shared libraries loaded at program start. No
		// filtering on segment permissions: string constants live in read-only
		// segments with newer toolchains, but inside the executable text
		// segment with older ones, and writable preinitialized data has always
		// been part of this predicate.
		dl_iterate_phdr([](struct dl_phdr_info* Info, size_t, void* pData) -> int
		{
			auto& Data = *static_cast<Segments*>(pData);

			for (ElfW(Half) iSegment = 0; iSegment < Info->dlpi_phnum; ++iSegment)
			{
				const auto& ProgHeader = Info->dlpi_phdr[iSegment];

				if (ProgHeader.p_type == PT_LOAD && ProgHeader.p_filesz > 0)
				{
					const char* start = reinterpret_cast<const char*>(Info->dlpi_addr + ProgHeader.p_vaddr);
					Data.push_back( { start, start + ProgHeader.p_filesz } );
				}
			}

			return 0;
		}, &Data);

#endif

		// sort by start address ..
		std::sort(Data.begin(), Data.end(), [](const Segment& left, const Segment& right)
		{
			return left.start < right.start;
		});

		// .. and merge overlapping and adjacent segments
		Segments Merged;
		Merged.reserve(Data.size());

		for (const auto& segment : Data)
		{
			if (segment.start >= segment.end)
			{
				continue;
			}

			if (!Merged.empty() && segment.start <= Merged.back().end)
			{
				if (segment.end > Merged.back().end)
				{
					Merged.back().end = segment.end;
				}
			}
			else
			{
				Merged.push_back(segment);
			}
		}

#ifdef DEKAF2_DEBUG_SEGMENT_DETECTION
		if (kWouldLog(2))
		{
			kDebug(2, "found {} data sections, merged into {} ranges", Data.size(), Merged.size());
			uint32_t iCount = 0;

			for (const auto& segment : Merged)
			{
				kDebug(2, "{}: from: {:>10} to: {:>10} size: {:>8}",
					   ++iCount,
					   static_cast<const void*>(segment.start),
					   static_cast<const void*>(segment.end),
					   segment.end - segment.start);
			}
		}
#endif

		return Merged;
	}();

	// binary search for the one range that could contain the address
	auto it = std::upper_bound(DataSegments.begin(), DataSegments.end(), static_cast<const char*>(addr),
							   [](const char* addr, const Segment& segment)
	{
		return addr < segment.start;
	});

	return (it != DataSegments.begin() && static_cast<const char*>(addr) < (--it)->end);

#else

	#ifdef DEKAF2_DEBUG_SEGMENT_DETECTION
	kDebug(1, "operating system not supported")
	#endif
	return false;

#endif

} // kIsInsideDataSegment

//-----------------------------------------------------------------------------
detail::KUNameBase::KUNameBase() noexcept
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	m_UTSName.release[0] = 0;

	SYSTEM_INFO sys;
	GetNativeSystemInfo(&sys);
	switch(sys.wProcessorArchitecture) {
#ifdef PROCESSOR_ARCHITECTURE_AMD64
		case PROCESSOR_ARCHITECTURE_AMD64:
			m_UTSName.machine = "x64_64";
			break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM
		case PROCESSOR_ARCHITECTURE_ARM:
			m_UTSName.machine = "ARM32";
			break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM64
		case PROCESSOR_ARCHITECTURE_ARM64:
			m_UTSName.machine = "ARM64";
			break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_IA64
		case PROCESSOR_ARCHITECTURE_IA64:
			m_UTSName.machine = "IA-64";
			break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_INTEL
		case PROCESSOR_ARCHITECTURE_INTEL:
			m_UTSName.machine = "x86";
			break;
#endif
		default:
			m_UTSName.machine = "UNKNOWN";
			break;
	}

	HKEY hkey;
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_QUERY_VALUE, &hkey);
	BYTE version;
	long unsigned int size = sizeof(version);
	RegQueryValueExW(hkey, L"DisplayVersion", NULL, NULL, &version, &size);
	char* pStart = m_UTSName.version;
	char* pEnd   = pStart + sizeof(m_UTSName.version) - 1;
	auto result  = std::to_chars(pStart, pEnd, static_cast<uint8_t>(version), 10);
	if (result.ec == std::errc())
	{
		result.ptr = 0;
	}
	else
	{
		m_UTSName.version[0] = 0;
	}
	RegCloseKey(hkey);

	gethostname(m_UTSName.nodename, 256);

	m_UTSName.sysname = "Windows";

#else

	if (uname(&m_UTSName))
	{
		kDebug(1, "cannot get uname info: {}", strerror(errno));

		m_UTSName.sysname [0] = 0;
		m_UTSName.nodename[0] = 0;
		m_UTSName.release [0] = 0;
		m_UTSName.version [0] = 0;
		m_UTSName.machine [0] = 0;
	}

#endif

} // detail::KUNameBase ctor

//-----------------------------------------------------------------------------
detail::KUNameBase::KUNameBase(const detail::KUNameBase& other) noexcept
//-----------------------------------------------------------------------------
{
	static_assert(std::is_trivially_copyable<utsname>::value, "utsname is not trivially copyable");
	std::memcpy(&m_UTSName, &other.m_UTSName, sizeof(utsname));
}

//-----------------------------------------------------------------------------
KUName::KUName() noexcept
//-----------------------------------------------------------------------------
: detail::KUNameBase()
, sysname  (m_UTSName.sysname )
, nodename (m_UTSName.nodename)
, release  (m_UTSName.release )
, version  (m_UTSName.version )
, machine  (m_UTSName.machine )
{
} // kUName ctor

//-----------------------------------------------------------------------------
KUName::KUName(const KUName& other) noexcept
//-----------------------------------------------------------------------------
: detail::KUNameBase(other)
, sysname  (m_UTSName.sysname )
, nodename (m_UTSName.nodename)
, release  (m_UTSName.release )
, version  (m_UTSName.version )
, machine  (m_UTSName.machine )
{
} // KUName copy ctor

//-----------------------------------------------------------------------------
KDuration kPing(KStringView sHostname, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (kIsIPv6Address(sHostname, true))
	{
		// this is an ip v6 numerical address
		sHostname.remove_suffix(1);
		sHostname.remove_prefix(1);
	}

	// call the system ping utility, it has setuid or 'setcap cap_net_raw+ep' set
	KString sOut;

#ifdef DEKAF2_IS_WINDOWS
	uint32_t iTimeout = static_cast<uint32_t>(std::max(KDuration(chrono::milliseconds(1)).milliseconds().count(), Timeout.milliseconds().count()));
	auto iResult = kSystem(kFormat("ping /n 1 /w {} {}", iTimeout, sHostname), sOut);
#else
	uint32_t iTimeout = static_cast<uint32_t>(std::max(KDuration(chrono::seconds(1)).seconds().count(), Timeout.seconds().count()));
	auto iResult = kSystem(kFormat("ping -c 1 -t {} {}", iTimeout, sHostname), sOut);
#endif

	if (!iResult)
	{
		auto pos = sOut.find(" time=");

		if (pos != npos)
		{
			KStringView sFound = sOut.ToView(pos + sizeof(" time="));

			uint32_t iMilliSeconds { 0 };
			uint32_t iMicroSeconds { 0 };
			bool bHadDot { false };

			for (auto ch : sFound)
			{
				if (ch == '.')
				{
					if (bHadDot) break;
					bHadDot = true;
				}
				else if (KASCII::kIsDigit(ch))
				{
					if (bHadDot)
					{
						iMicroSeconds *= 10;
						iMicroSeconds += ch - '0';
					}
					else
					{
						iMilliSeconds *= 10;
						iMilliSeconds += ch - '0';
					}
				}
				else break;
			}

			KDuration duration(chrono::milliseconds(iMilliSeconds) + chrono::microseconds(iMicroSeconds));
			kDebug(2, "{}: {}", sHostname, duration);
			return duration;
		}
	}

	kDebug(1, "{}: error {}", sHostname, iResult);
	return {};

} // kPing

//-----------------------------------------------------------------------------
bool kStdOutIsTerminal()
//-----------------------------------------------------------------------------
{
#if !DEKAF2_IS_WINDOWS
	return KFileStat(STDOUT_FILENO).Type() == KFileType::CHARACTER;
#else
	// on windows we don't know..
	return false;
#endif

} // kStdOutIsTerminal

//-----------------------------------------------------------------------------
KDuration kGetUptime()
//-----------------------------------------------------------------------------
{
#if DEKAF2_IS_MACOS

	struct timeval tvBoot;
	std::size_t iLen = sizeof(tvBoot);
	std::array<int, 2> mib { CTL_KERN, KERN_BOOTTIME };

	if (::sysctl(mib.data(), 2, &tvBoot, &iLen, nullptr, 0) < 0)
	{
		kDebug(1, ::strerror(errno));
		return KDuration { -1 } ;
	}

	return KUnixTime::now() - KUnixTime(tvBoot);

#elif DEKAF2_IS_UNIX

	struct sysinfo info;

	if (::sysinfo(&info) != 0)
	{
		kDebug(1, ::strerror(errno));
		return KDuration { -1 } ;
	}

	return chrono::seconds(info.uptime);

#elif DEKAF2_IS_WINDOWS

	return chrono::milliseconds(GetTickCount64());

#else

	return KDuration { -1 } ;

#endif

} // kGetUptime

//-----------------------------------------------------------------------------
KLoadAverage KLoadAverage::Create()
//-----------------------------------------------------------------------------
{
	KLoadAverage la;

#if DEKAF2_IS_UNIX

	if (::getloadavg(la.m_LoadAvg.data(), 3) < 0)
	{
		kDebug(1, ::strerror(errno));
		la.m_LoadAvg.fill(-1.0);
	}

#endif

	return la;

} // KLoadAverage::Create

//-----------------------------------------------------------------------------
KLoadAverage kGetLoadAverage()
//-----------------------------------------------------------------------------
{
	return KLoadAverage::Create();

} // kGetLoadAverage

//=============================================================================
// Socket / shared-memory / fd limit helpers
//=============================================================================

namespace {

#if defined(DEKAF2_IS_LINUX)

//-----------------------------------------------------------------------------
/// Read a single integer from /proc/sys/<key> (dots converted to slashes).
static uint64_t ReadProcSysUint(KStringView sKey)
//-----------------------------------------------------------------------------
{
	KString sPath = kFormat("/proc/sys/{}", sKey);
	sPath.Replace('.', '/');
	return kReadAll(sPath).Trim().UInt64();

} // ReadProcSysUint

//-----------------------------------------------------------------------------
/// Read the iField-th value (0-based) of a whitespace-separated triplet
/// "min default max" from /proc/sys/<key> (e.g. net.ipv4.tcp_wmem / tcp_rmem):
/// field 1 is the default, field 2 is the max.
static uint64_t ReadProcSysTripletField(KStringView sKey, uint16_t iField)
//-----------------------------------------------------------------------------
{
	KString sPath = kFormat("/proc/sys/{}", sKey);
	sPath.Replace('.', '/');
	KString sValue = kReadAll(sPath).Trim();

	auto iStart = sValue.find_first_not_of(" \t");

	for (uint16_t i = 0; i < iField; ++i)
	{
		if (iStart == KString::npos) return 0;
		auto iEnd = sValue.find_first_of(" \t", iStart); // end of the current token
		if (iEnd == KString::npos) return 0;             // no further token
		iStart = sValue.find_first_not_of(" \t", iEnd);  // start of the next token
	}

	if (iStart == KString::npos) return 0;
	// UInt64() parses the leading digits of this token (it stops at whitespace)
	return sValue.ToView(iStart).UInt64();

} // ReadProcSysTripletField

//-----------------------------------------------------------------------------
/// Query a socket buffer size via getsockopt() on a throwaway socket. Used as a
/// fallback for the per-socket *default* when net.core.{w,r}mem_default is not
/// visible (e.g. inside a container's own netns on kernels that do not
/// namespace those sysctls). Linux reports twice the usable size for
/// SO_SNDBUF/SO_RCVBUF, so we halve it to match the sysctl semantics.
static uint64_t GetSockOptBufDefault(int iSockType, int iOption)
//-----------------------------------------------------------------------------
{
	int iFD = ::socket(AF_INET, iSockType, 0);
	if (iFD < 0) return 0;

	int       iValue  = 0;
	socklen_t iLen    = sizeof(iValue);
	uint64_t  iResult = 0;

	if (::getsockopt(iFD, SOL_SOCKET, iOption, &iValue, &iLen) == 0 && iValue > 0)
	{
		iResult = static_cast<uint64_t>(iValue) / 2;
	}

	::close(iFD);
	return iResult;

} // GetSockOptBufDefault

#elif defined(DEKAF2_IS_MACOS)

//-----------------------------------------------------------------------------
/// Wrapper around sysctlbyname() that returns uint64_t for any integer type.
/// Safe on little-endian (all modern Macs): a 32-bit result written into the
/// lower bytes of a zero-initialized 64-bit buffer gives the correct value.
///
/// Returns 0 on failure (unknown OID, EPERM, etc.). Callers should treat
/// 0 as "unavailable" and provide a fallback where appropriate — modern
/// macOS has retired several sysctls that older versions exposed (e.g.
/// `net.inet.udp.sendspace` is gone from macOS 14+).
static uint64_t MacSysctl(const char* sName)
//-----------------------------------------------------------------------------
{
	uint64_t val = 0;
	size_t   len = sizeof(val);
	if (::sysctlbyname(sName, &val, &len, nullptr, 0) != 0)
	{
		return 0;
	}
	return val;

} // MacSysctl

#endif

} // namespace

//-----------------------------------------------------------------------------
bool KSocketBufferLimits::Read(bool bForUDP)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_IS_LINUX)

	// net.core.{w,r}mem_max are the SO_SNDBUF/SO_RCVBUF ceiling shared by TCP and
	// UDP. On kernels that do not namespace them (older than ~6.12) they exist
	// only in the init netns and are absent inside a container's own netns; fall
	// back to the max field of the (always per-netns) tcp_{w,r}mem triplets.
	iSendMax = ReadProcSysUint("net.core.wmem_max");
	if (iSendMax == 0) iSendMax = ReadProcSysTripletField("net.ipv4.tcp_wmem", 2);
	iRecvMax = ReadProcSysUint("net.core.rmem_max");
	if (iRecvMax == 0) iRecvMax = ReadProcSysTripletField("net.ipv4.tcp_rmem", 2);

	if (bForUDP)
	{
		// net.core.{w,r}mem_default, with a getsockopt() fallback when absent
		iSendDefault = ReadProcSysUint("net.core.wmem_default");
		if (iSendDefault == 0) iSendDefault = GetSockOptBufDefault(SOCK_DGRAM, SO_SNDBUF);
		iRecvDefault = ReadProcSysUint("net.core.rmem_default");
		if (iRecvDefault == 0) iRecvDefault = GetSockOptBufDefault(SOCK_DGRAM, SO_RCVBUF);
	}
	else // TCP
	{
		// tcp_wmem / tcp_rmem are "min default max" triplets; field 1 is the default
		iSendDefault = ReadProcSysTripletField("net.ipv4.tcp_wmem", 1);
		iRecvDefault = ReadProcSysTripletField("net.ipv4.tcp_rmem", 1);
	}

	return iSendMax > 0;

#elif defined(DEKAF2_IS_MACOS)

	// kern.ipc.maxsockbuf is the shared ceiling for send and recv
	iSendMax = iRecvMax = MacSysctl("kern.ipc.maxsockbuf");

	if (bForUDP)
	{
		// macOS 14+ retired `net.inet.udp.sendspace` — the UDP send buffer is
		// now governed by the same per-socket logic as TCP send space. Fall
		// back to the TCP value so callers always get a usable starting
		// point, then to maxsockbuf / 2 as a last resort.
		iSendDefault = MacSysctl("net.inet.udp.sendspace");
		if (iSendDefault == 0) iSendDefault = MacSysctl("net.inet.tcp.sendspace");
		if (iSendDefault == 0) iSendDefault = iSendMax / 2;

		iRecvDefault = MacSysctl("net.inet.udp.recvspace");
		if (iRecvDefault == 0) iRecvDefault = MacSysctl("net.inet.tcp.recvspace");
		if (iRecvDefault == 0) iRecvDefault = iRecvMax / 2;
	}
	else // TCP
	{
		iSendDefault = MacSysctl("net.inet.tcp.sendspace");
		iRecvDefault = MacSysctl("net.inet.tcp.recvspace");
	}

	return iSendMax > 0;

#else

	return false;

#endif

} // KSocketBufferLimits::Read

//-----------------------------------------------------------------------------
bool KSharedMemoryLimits::Read()
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_IS_LINUX)

	iMaxSegmentBytes = ReadProcSysUint("kernel.shmmax");
	iTotalPages      = ReadProcSysUint("kernel.shmall");
	iMaxSegments     = ReadProcSysUint("kernel.shmmni");

	return iMaxSegmentBytes > 0;

#elif defined(DEKAF2_IS_MACOS)

	iMaxSegmentBytes = MacSysctl("kern.sysv.shmmax");
	iTotalPages      = MacSysctl("kern.sysv.shmall");
	iMaxSegments     = MacSysctl("kern.sysv.shmmni");

	return iMaxSegmentBytes > 0;

#else

	return false;

#endif

} // KSharedMemoryLimits::Read

//-----------------------------------------------------------------------------
bool KFileDescriptorLimits::Read()
//-----------------------------------------------------------------------------
{
	bool bOK = false;

#if defined(DEKAF2_IS_LINUX)

	iKernelMax = ReadProcSysUint("fs.file-max");
	bOK = iKernelMax > 0;

#elif defined(DEKAF2_IS_MACOS)

	iKernelMax = MacSysctl("kern.maxfiles");
	bOK = iKernelMax > 0;

#endif

#ifndef DEKAF2_IS_WINDOWS

	struct rlimit rl;
	if (::getrlimit(RLIMIT_NOFILE, &rl) == 0)
	{
		// Store the kernel's raw rlim_t value, no remapping. The platforms
		// disagree on what RLIM_INFINITY actually IS:
		//   Linux: RLIM_INFINITY == ULLONG_MAX (all bits set, 1.8e19)
		//   macOS: RLIM_INFINITY == LLONG_MAX  (top bit clear, 9.2e18)
		// Either value is already so far above any plausible fd count that
		// "uncapped" is a safe interpretation. We used to coerce both to
		// UINT64_MAX for "uniformity", but that bit pattern made Catch2's
		// CHECK( ... > 0 ) report failure on macOS (Catch2 v3 binary-expr
		// decomposition seems to mis-classify the comparison when one side
		// is exactly UINT64_MAX). Keeping the raw value also gives the
		// caller a hint of which platform conventions are at play.
		iProcessSoft = static_cast<uint64_t>(rl.rlim_cur);
		iProcessHard = static_cast<uint64_t>(rl.rlim_max);
		bOK = true;
	}

#endif

	return bOK;

} // KFileDescriptorLimits::Read

//-----------------------------------------------------------------------------
bool KFileDescriptorLimits::Set(uint64_t iSoftLimit)
//-----------------------------------------------------------------------------
{
#ifndef DEKAF2_IS_WINDOWS

	struct rlimit rl;
	if (::getrlimit(RLIMIT_NOFILE, &rl) != 0)
	{
		kDebug(1, "getrlimit: {}", ::strerror(errno));
		return false;
	}

	// clamp to hard limit (RLIM_INFINITY means no ceiling)
	if (rl.rlim_max != RLIM_INFINITY && iSoftLimit > static_cast<uint64_t>(rl.rlim_max))
	{
		iSoftLimit = static_cast<uint64_t>(rl.rlim_max);
	}

	rl.rlim_cur = static_cast<rlim_t>(iSoftLimit);

	if (::setrlimit(RLIMIT_NOFILE, &rl) != 0)
	{
		kDebug(1, "setrlimit: {}", ::strerror(errno));
		return false;
	}

	iProcessSoft = iSoftLimit;
	return true;

#else

	return false;

#endif

} // KFileDescriptorLimits::Set


DEKAF2_NAMESPACE_END
