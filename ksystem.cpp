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

#include "ksystem.h"
#include "bits/kcppcompat.h"
#include "bits/kfilesystem.h"
#include "bits/kasio.h"
#include "kfilesystem.h"
#include "klog.h"
#include "dekaf2.h"
#include "kinshell.h"
#include "kutf8.h"             // for Windows API conversions
#include <thread>
#include <cstdlib>
#include <ctime>
#include <array>

#ifdef DEKAF2_HAS_LIBPROC
	#include <libproc.h>                       // for proc_pidpath()
#endif
#ifdef DEKAF2_IS_WINDOWS
	#include <cwchar>
	#include <ws2tcpip.h>
	#include <sysinfoapi.h>    // for getTotalSystemMemory()
	#include <consoleapi2.h>   // for GetConsoleScreenBufferInfo()
	#include <fileapi.h>       // for GetFinalPathNameByHandle()
	#include <io.h>            // for _get_osfhandle()
	#include <libloaderapi.h>  // for GetModuleFileName()
	#include <processthreadsapi.h> // for GetCurrentProcessorNumber()
#else
	#include <unistd.h>        // for sysconf()
	#include <sys/types.h>     // for getpwuid()
	#include <pwd.h>           // for getpwuid()
	#include <arpa/inet.h>
	#include <sys/ioctl.h>     // for ioctl(), TIOCGWINSZ
	#ifdef DEKAF2_IS_MACOS
		// MacOS
		#include <sys/syslimits.h> // for MAX_PATH
		#include <fcntl.h>         // for fcntl()
		#ifdef DEKAF2_X86
			#include <cpuid.h>     // for cpuid()
		#endif
		#include <mach-o/getsect.h> // for kIsInsideDataSegment()
	#else
		// Unix
		#include <sys/syscall.h>
		#include <limits.h>    // for MAX_PATH
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

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p szDefault if not found.
KStringViewZ kGetEnv (KStringViewZ szEnvVar, KStringViewZ szDefault)
//-----------------------------------------------------------------------------
{
	KStringViewZ sValue = ::getenv(szEnvVar.c_str());
	if (!sValue.empty())
	{
		return (sValue);
	}
	else
	{
		return (szDefault);
	}

} // kGetEnv

//-----------------------------------------------------------------------------
/// Set environment variable.
bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ szValue)
//-----------------------------------------------------------------------------
{
#ifdef WIN32
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
#endif  // WIN32
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
#ifdef WIN32
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
		kWarning("chdir failed: {}", ::strerror(errno));
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
		kWarning("cannot get current working directory: {}", ::strerror(errno));
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

	kDebug (2, sWhoami);

	return (sWhoami);

} // kGetWhoami

//-----------------------------------------------------------------------------
KStringViewZ kGetHostname (bool bAllowKHostname/*=true*/)
//-----------------------------------------------------------------------------
{
	enum { MAXNAMELEN = 50 };

	struct Names
	{
		Names()
		{
			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// set (and cache) /etc/khostname:
			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
			if (kFileExists("/etc/khostname"))
			{
				sKHostname = kReadAll("/etc/khostname");
			}

			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// set (and cache) OS hostname:
			// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

			#ifdef DEKAF2_IS_WINDOWS

			char szHostname[MAXNAMELEN+1]; szHostname[0] = 0;
			DWORD nSize = MAXNAMELEN;
			GetComputerName (
				szHostname,       // name buffer
				&nSize              // address of size of name buffer
			);

			sHostname = szHostname;

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
		kReadFile (sTmp, sOutput, true);
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
KString kResolveHost (KStringViewZ sHostname, bool bIPv4, bool bIPv6)
//-----------------------------------------------------------------------------
{
	KString sIPV4;
	KString sIPV6;
	KString sRet;

	boost::asio::io_service IOService;
	boost::asio::ip::tcp::resolver Resolver(IOService);
	boost::asio::ip::tcp::resolver::query query(sHostname.c_str(), "80", boost::asio::ip::tcp::resolver::query::numeric_service);
	boost::system::error_code ec;
	auto hosts = Resolver.resolve(query, ec);

	if (ec)
	{
		kDebug (1, "{} --> FAILED : {}", sHostname, ec.message());
	}
	else
	{
#if (BOOST_VERSION < 106600)
		auto it = hosts;
		decltype(it) ie;
#else
		auto it = hosts.begin();
		auto ie = hosts.end();
#endif
		for (; it != ie; ++it)
		{
			if (it->endpoint().protocol() == boost::asio::ip::tcp::v4())
			{
				sIPV4 = it->endpoint().address().to_string();

				if (bIPv4)
				{
					break;
				}
			}
			else
			{
				sIPV6 = it->endpoint().address().to_string();

				if (bIPv6)
				{
					break;
				}
			}
		}

		if (bIPv4 && !sIPV4.empty())
		{
			// success
			kDebug (2, "{} --> {}", sHostname, sIPV4);
			sRet = std::move(sIPV4);
		}
		else if (bIPv6 && !sIPV6.empty())
		{
			// success
			kDebug (2, "{} --> {}", sHostname, sIPV6);
			sRet = std::move(sIPV6);
		}
		else if (sIPV4.empty() && sIPV6.empty())
		{
			// unknown..
			kDebug(1, "{} --> FAILED", sHostname);
		}
		else if (sIPV4.empty())
		{
			kDebug(1, "{} --> FAILED, only has IPV6 {}", sHostname, sIPV6);
		}
		else if (sIPV6.empty())
		{
			kDebug(1, "{} --> FAILED, only has IPV4 {}", sHostname, sIPV4);
		}
	}

	return sRet;

} // kResolveHostIPV4

//-----------------------------------------------------------------------------
bool kIsValidIPv4 (KStringViewZ sIPAddr)
//-----------------------------------------------------------------------------
{
	struct sockaddr_in sockAddr;
	return inet_pton(AF_INET, sIPAddr.c_str(), &(sockAddr.sin_addr)) != 0;

} // kIsValidIPv4

//-----------------------------------------------------------------------------
bool kIsValidIPv6 (KStringViewZ sIPAddr)
//-----------------------------------------------------------------------------
{
	struct sockaddr_in6 sockAddr;
	return inet_pton(AF_INET6, sIPAddr.c_str(), &(sockAddr.sin6_addr)) != 0;

} // kIsValidIPv6

//-----------------------------------------------------------------------------
KString kHostLookup (KStringViewZ sIPAddr)
//-----------------------------------------------------------------------------
{
	KString sFoundHost;
	boost::asio::ip::tcp::endpoint endpoint;
	if (kIsValidIPv4 (sIPAddr))
	{
		boost::asio::ip::address_v4 v4Addr = boost::asio::ip::address_v4::from_string (sIPAddr.c_str());
		endpoint.address (v4Addr);
	}
	else if (kIsValidIPv6 (sIPAddr))
	{
		boost::asio::ip::address_v6 v6Addr = boost::asio::ip::address_v6::from_string (sIPAddr.c_str());
		endpoint.address (v6Addr);
	}
	else
	{
		kDebug(1, "Invalid address specified: {} --> FAILED", sIPAddr);
		return "";
	}

	boost::asio::io_service IOService;
	boost::asio::ip::tcp::resolver Resolver (IOService);
	boost::system::error_code ec;

	auto hostIter = Resolver.resolve(endpoint, ec);

	if (ec)
	{
		kDebug (1, "{} --> FAILED : {}", sIPAddr, ec.message());
	}
	else
	{
		sFoundHost = hostIter->host_name();
	}

	return sFoundHost;

} // kHostLookup


//-----------------------------------------------------------------------------
uint32_t kRandom(uint32_t iMin, uint32_t iMax)
//-----------------------------------------------------------------------------
{
	return Dekaf::getInstance().GetRandomValue(iMin, iMax);

} // kRandom

//-----------------------------------------------------------------------------
void kNanoSleep (uint64_t iNanoSeconds)
//-----------------------------------------------------------------------------
{
	std::this_thread::sleep_for(std::chrono::nanoseconds(iNanoSeconds));

} // kNanoSleep

//-----------------------------------------------------------------------------
void kMicroSleep (uint64_t iMicroSeconds)
//-----------------------------------------------------------------------------
{
	std::this_thread::sleep_for(std::chrono::microseconds(iMicroSeconds));

} // kMicroSleep

//-----------------------------------------------------------------------------
void kMilliSleep (uint64_t iMilliSeconds)
//-----------------------------------------------------------------------------
{
	std::this_thread::sleep_for(std::chrono::milliseconds(iMilliSeconds));

} // kMilliSleep

//-----------------------------------------------------------------------------
void kSleep (uint64_t iSeconds)
//-----------------------------------------------------------------------------
{
	std::this_thread::sleep_for(std::chrono::seconds(iSeconds));

} // kSleep

//-----------------------------------------------------------------------------
void kSleepRandomMilliseconds (uint32_t iMin, uint32_t iMax)
//-----------------------------------------------------------------------------
{
	uint32_t iSleep = iMin;

	if (iMax > iMin)
	{
		iSleep = kRandom(iMin, iMax);
	}

	kDebug (2, "sleeping {} miliseconds...", iSleep);

	kMilliSleep(iSleep);

} // kSleepRandomMilliseconds

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
		MEMORYSTATUSEX status;
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
		return sysInfo.dwNumberOfProcessors ? sysInfo.dwNumberOfProcessors : 1;
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
	return GetCurrentProcessorNumber();

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
	ULARGE_INTEGER time;
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
	return tv.tv_sec * 1000000 + tv.tv_usec;

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
bool kSetGlobalLocale(KStringViewZ sLocale)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		std::locale::global(std::locale(sLocale.c_str()));

		kDebug(1, "changed global locale to {}", sLocale);
	}
	DEKAF2_CATCH (const std::exception& ex)
	{
		kDebug(1, "failed to change global locale to {}: {}", sLocale, ex.what());

		return false;
	}

	return true;

} // kSetGlobalLocale

//-----------------------------------------------------------------------------
std::locale kGetGlobalLocale()
//-----------------------------------------------------------------------------
{
	return std::locale();

} // kGetGlobalLocale

//-----------------------------------------------------------------------------
char kGetDecimalPoint()
//-----------------------------------------------------------------------------
{
	auto locale = kGetGlobalLocale();

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
char kGetThousandsSeparator()
//-----------------------------------------------------------------------------
{
	auto locale = kGetGlobalLocale();

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

			GetModuleFileNameW(hModule, wsPath.data(), wsPath.size());
			wsPath.resize(wcsnlen(wsPath.data(), wsPath.size()));

			sPath = Unicode::ToUTF8<KString>(wsPath);
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

	KString sFileName = Unicode::ToUTF8<KString>(sBuffer);

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

	struct winsize ts;

	if (ioctl(fd, TIOCGWINSZ, &ts) != -1 && ts.ws_col && ts.ws_row)
	{
		TTY.columns = ts.ws_col;
		TTY.lines   = ts.ws_row;
	}
	else
	{
		KString sOutput;

		if (fd == 0)
		{
			kSystem("stty size", sOutput);
		}
		else
		{
			auto sName = kGetFileNameFromFileDescriptor(fd);

			if (!sName.empty())
			{
				kSystem(kFormat("stty -f '{}' size", sName), sOutput);
			}
		}

		auto sCols = sOutput.Split(" ");

		if (sCols.size() == 2)
		{
			TTY.lines   = sCols[0].UInt16();
			TTY.columns = sCols[1].UInt16();
		}
	}

#else // is windows

	CONSOLE_SCREEN_BUFFER_INFO csbi;

	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
	{
		TTY.columns = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
		TTY.lines   = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
	}

#endif

	if (!TTY.columns || !TTY.lines)
	{
		kDebug(1, "cannot read terminal size, using defaults");

		TTY.columns = iDefaultColumns;
		TTY.lines   = iDefaultLines;
	}

	kDebug(2, "terminal size: {}x{}", TTY.columns, TTY.lines);

	return TTY;

} // kGetTerminalSize

} // end of namespace dekaf2

#if !defined(DEKAF2_IS_MACOS) && defined(DEKAF2_IS_UNIX)
extern char _etext;
extern char _edata;
//extern char _end;
#endif

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool kIsInsideDataSegment(const void* addr)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_IS_WINDOWS

	struct Segment
	{
		void* etext { nullptr };
		void* edata { nullptr };
	};

	static const Segment DataSegment = []() -> Segment
	{
		// setup pointers
		HANDLE Self                         = GetModuleHandle(nullptr);
		IMAGE_NT_HEADERS* NtHeader          = ImageNtHeader(Self);
		IMAGE_SECTION_HEADER* SectionHeader = static_cast<IMAGE_SECTION_HEADER*>(NtHeader + 1);
		const char* SelfImage               = static_cast<const char*>(Self);

		Segment Data;

		// iterate through image segments and find the .data segment
		for (int iSection = 0; iSection < NtHeader->FileHeader.NumberOfSections; ++iSection, ++SectionHeader)
		{
			if (!strncmp(".data", SectionHeader->Name, sizeof(SectionHeader->Name)))
			{
				Data.etext = SelfImage  + SectionHeader->VirtualAddress;
				Data.edata = SelfImage  + SectionHeader->VirtualAddress
				                        + SectionHeader->Misc.VirtualSize;
				break;
			}
			++SectionHeader;
		}

		return Data;
	}();

	return ((DataSegment.edata > addr) && (DataSegment.etext <= addr));

#elif DEKAF2_IS_MACOS

	struct Segment
	{
		void* etext { nullptr };
		void* edata { nullptr };
	};

	static const Segment DataSegment = []() -> Segment
	{
		Segment Data;
		Data.etext = (void *)get_etext();
		Data.edata = (void *)get_edata();

		return Data;
	}();

	return ((DataSegment.edata > addr) && (DataSegment.etext <= addr));

#elif DEKAF2_IS_UNIX

	return ((&_edata > addr) && (&_etext <= addr));

#endif

	kDebug(1, "operating system not supported")
	return false;

} // kIsInsideDataSegment


} // end of namespace dekaf2

