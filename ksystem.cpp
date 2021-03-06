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
#include <thread>
#include <cstdlib>
#include "bits/kcppcompat.h"
#include "bits/kfilesystem.h"
#include "kfilesystem.h"
#include "klog.h"
#include "dekaf2.h"
#include "kinshell.h"

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#ifdef DEKAF2_IS_WINDOWS
	#include <ws2tcpip.h>
	#include <sysinfoapi.h>   // for getTotalSystemMemory()
#else
	#include <unistd.h>       // for sysconf()
	#include <sys/types.h>    // for getpwuid()
	#include <pwd.h>          // for getpwuid()
	#include <arpa/inet.h>
	#ifndef DEKAF2_IS_OSX
		#include <sys/syscall.h>
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
bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ sValue)
//-----------------------------------------------------------------------------
{
#ifdef WIN32
	errno_t err = _putenv_s(szEnvVar.c_str(), sValue.c_str());
	if (err)
	{
		kWarning("cannot set {} = {}", szEnvVar, sValue);
	}
	bool bOK = !err;
#else
	if (sValue.empty())
	{
		return kUnsetEnv(szEnvVar);
	}
	bool bOK = (::setenv(szEnvVar.c_str(), sValue.c_str(), true) == 0);
	if (!bOK)
	{
		kWarning("cannot set {} = {}, {}", szEnvVar, sValue, strerror(errno));
	}
#endif  // WIN32
	return (bOK);

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
	KString sPath = fs::current_path(ec).u8string();
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
	KString sTemp = fs::temp_directory_path().u8string();
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

// It is preferable to use KInShell / popen for the call to system as it
// avoids the creation of a temporary output file. Also, on Unixes, KInShell
// actually is KInPipe, which uses fork and exec, and takes care to delete all
// parent file descriptors in the child to prevent descriptor leaking (which
// could lead to resource exhaustion or socket contention)

#define DEKAF2_USE_KSHELL_FOR_SYSTEM

//-----------------------------------------------------------------------------
int kSystem (KStringView sCommand, KString& sOutput)
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

	sOutput.Replace("\r\n", "\n"); // DOS -> UNIX

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

} // end of namespace dekaf2

