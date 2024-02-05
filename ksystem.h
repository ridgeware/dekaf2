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

#pragma once

/// @file ksystem.h
/// general system utilities for dekaf2

#include "kstring.h"
#include "kstringview.h"
#include "bits/kstringviewz.h"
#include <locale>
#ifndef DEKAF2_IS_WINDOWS
	#include <sys/resource.h>
	#include <sys/utsname.h>   // for uname()
	#include <unistd.h>
#else
	using HANDLE = void*;
#endif
#if DEKAF2_HAS_INCLUDE(<pthread.h>)
	#include <pthread.h>
#else
	using pthread_t = void*;
#endif
#include <thread>

DEKAF2_NAMESPACE_BEGIN

/// Get environment variable.
/// @param szEnvVar the environment variable's name
/// @param szDefault the default value to use if the environment variable is not found (default is the empty string)
/// @return the value of the environment variable, or @p szDefault if not found.
DEKAF2_PUBLIC
KStringViewZ kGetEnv (KStringViewZ szEnvVar, KStringViewZ szDefault = "");

/// Set single environment variable. If value is empty, the variable will be removed.
/// @param szEnvVar the environment variable's name
/// @param szValue the environment variable's value
/// @return true on success
DEKAF2_PUBLIC
bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ szValue);

/// Set multiple environment variables. If value is empty, the variable will be removed.
/// @param Environment a vector of a pair of KString name and values
/// @return true on success
DEKAF2_PUBLIC
bool kSetEnv (const std::vector<std::pair<KString, KString>>& Environment);

/// Unset environment variable.
/// @param szEnvVar the environment variable's name
/// @return true on success
DEKAF2_PUBLIC
bool kUnsetEnv (KStringViewZ szEnvVar);

/// Set operating system current working directory.
/// @param sPath the directory to be set
/// @return true on success
DEKAF2_PUBLIC
bool kSetCWD (KStringViewZ sPath);

/// Return operating system current working directory as a string.
DEKAF2_PUBLIC
KString kGetCWD ();

/// alias to kGetCWD(): return operating system current working directory as a string.
DEKAF2_PUBLIC
inline KString kcwd () { return kGetCWD(); }

/// Return user's home directory
DEKAF2_PUBLIC
KString kGetHome();

/// Return the system's tmp directory
DEKAF2_PUBLIC
KString kGetTemp();

/// Return current operating system user as a string.
DEKAF2_PUBLIC
KString kGetWhoAmI ();

/// alias to kGetWhoami(): return current operating system user as a string.
DEKAF2_PUBLIC
inline KString kwhoami () { return kGetWhoAmI(); }

/// Return operating system hostname (or /etc/khostname if it exists) as a string.
/// @param bAllowKHostname check for /etc/khostname? default is true.
/// @return the hostname
KStringViewZ kGetHostname (bool bAllowKHostname=true);

/// return process ID
DEKAF2_PUBLIC
inline pid_t kGetPid()
{
	return getpid();
}

/// return thread ID
DEKAF2_PUBLIC
uint64_t kGetTid();

/// return own UID
DEKAF2_PUBLIC
uint32_t kGetUid();

/// return own GID
DEKAF2_PUBLIC
uint32_t kGetGid();

/// Execute the given command, redirect stdout and stderr into the given @p sOutput string.
/// @param sCommand the command to execute
/// @param sOutput gets filled with the output of the command
/// @return exit code of the command that was run: 0 is normally an indication of success.
DEKAF2_PUBLIC
int kSystem (KStringView sCommand, KStringRef& sOutput);

/// Execute the given command, redirect stdout and stderr into /dev/null.
/// @param sCommand the command to execute
/// @return exit code of the command that was run: 0 is normally an indication of success.
DEKAF2_PUBLIC
int kSystem (KStringView sCommand);

/// Resolve the given hostname into either an IPv4 IP address or an IPv6 address. If both versions are checked, IPv4 takes precedence if both are found.
/// @param sHostname the hostname to resolve
/// @param bIPv4 try IPv4 resolver
/// @param bIPv6 try IPv6 resolver
/// @return resolved IP address (as a string), or if hostname fails to resolve, an empty string.
DEKAF2_PUBLIC
KString kResolveHost (KStringViewZ sHostname, bool bIPv4, bool bIPv6);

/// Resolve the given hostname into an IPv4 IP address, e.g. "50.1.2.3".
/// @param sHostname the hostname to resolve
/// @return resolved IP address (as a string), or if hostname fails to resolve, an empty string.
DEKAF2_PUBLIC
inline KString kResolveHostIPV4 (KStringViewZ sHostname)
{
	return kResolveHost (sHostname, true, false);
}

/// Resolve the given hostname into an IPv6 IP address, e.g. "fe80::be27:ebff:fad4:49e7".
/// @param sHostname the hostname to resolve
/// @return resolved IP address (as a string), or if hostname fails to resolve, an empty string.
DEKAF2_PUBLIC
inline KString kResolveHostIPV6 (KStringViewZ sHostname)
{
	return kResolveHost (sHostname, false, true);
}

/// Return true if the string represents a valid IPV4 address
DEKAF2_PUBLIC
bool kIsValidIPv4 (KStringViewZ sIPAddr);

/// Return true if the string represents a valid IPV6 address
DEKAF2_PUBLIC
bool kIsValidIPv6 (KStringViewZ sIPAddr);

/// Return the first host name that maps to the specified IP address
DEKAF2_PUBLIC
KString kHostLookup (KStringViewZ sIPAddr);

/// Sleep for the amount of nanoseconds
DEKAF2_PUBLIC
void kNanoSleep(uint64_t iNanoSeconds);

/// Sleep for the amount of microseconds
DEKAF2_PUBLIC
void kMicroSleep(uint64_t iMicroSeconds);

/// Sleep for the amount of milliseconds
DEKAF2_PUBLIC
void kMilliSleep(uint64_t iMilliSeconds);

/// Sleep for the amount of seconds
DEKAF2_PUBLIC
void kSleep(uint64_t iSeconds);

/// Returns 32 bit unsigned random number in range [iMin - iMax].
DEKAF2_PUBLIC
uint32_t kRandom(uint32_t iMin = 0, uint32_t iMax = UINT32_MAX);

/// Block program from running for random amount of time within the given min and max.
DEKAF2_PUBLIC
void kSleepRandomMilliseconds (uint32_t iMinMilliseconds, uint32_t iMaxMilliseconds);

/// Block program from running for random amount of time within the given min and max.
DEKAF2_PUBLIC
inline void kSleepRandomSeconds (uint32_t iMinSeconds, uint32_t iMaxSeconds)
{
	kSleepRandomMilliseconds (iMinSeconds * 1000, iMaxSeconds * 1000);
}

/// Returns the page size used on this CPU/MMU
DEKAF2_PUBLIC
std::size_t kGetPageSize();

/// Returns the physical memory installed on this machine
DEKAF2_PUBLIC
std::size_t kGetPhysicalMemory();

/// Returns count of physical, available CPUs
DEKAF2_PUBLIC
uint16_t kGetCPUCount();

/// return the ID of the CPU currently used in a multi core system
DEKAF2_PUBLIC
uint16_t kGetCPU();

/// set the CPU IDs on which the PROCESS shall run
/// @param CPUs a vector of CPU IDs to run on
/// @param forPid the process ID that should be attached, or 0 for the current process
/// @return true on success, false on failure
DEKAF2_PUBLIC
bool kSetProcessCPU(const std::vector<uint16_t>& CPUs, pid_t forPid = 0);

/// get the CPU IDs on which the THREAD shall run
/// @param forThread the pthread_t that is requested
/// @return a vector of CPU IDs to run on
DEKAF2_PUBLIC
std::vector<uint16_t> kGetThreadCPU(const pthread_t& forThread);

/// get the CPU IDs on which the THREAD shall run
/// @param forThread the std::thread that is requested
/// @return a vector of CPU IDs to run on
DEKAF2_PUBLIC
std::vector<uint16_t> kGetThreadCPU(std::thread& forThread);

/// get the CPU IDs on which this THREAD shall run
/// @return a vector of CPU IDs to run on
DEKAF2_PUBLIC
std::vector<uint16_t> kGetThreadCPU();

/// set the CPU IDs on which the THREAD shall run
/// @param CPUs a vector of CPU IDs to run on
/// @param forTid the thread ID that should be attached
/// @return true on success, false on failure
DEKAF2_PUBLIC
bool kSetThreadCPU(const std::vector<uint16_t>& CPUs, const pthread_t& forThread);

/// set the CPU IDs on which the THREAD shall run
/// @param CPUs a vector of CPU IDs to run on
/// @param forTid the thread ID that should be attached
/// @return true on success, false on failure
DEKAF2_PUBLIC
bool kSetThreadCPU(const std::vector<uint16_t>& CPUs, std::thread& forThread);

/// set the CPU IDs on which this THREAD shall run
/// @param CPUs a vector of CPU IDs to run on
/// @return true on success, false on failure
DEKAF2_PUBLIC
bool kSetThreadCPU(const std::vector<uint16_t>& CPUs);

namespace detail {
DEKAF2_PRIVATE
std::size_t TicksFromRusage(int who);
}

#ifdef DEKAF2_IS_WINDOWS
#ifndef RUSAGE_SELF
  #define RUSAGE_SELF     1
#endif
#ifndef RUSAGE_CHILDREN
  #define RUSAGE_CHILDREN 2
#endif
#ifndef RUSAGE_THREAD
  #define RUSAGE_THREAD   3
#endif
#endif

/// Returns process time for the calling thread in microseconds. May fall back to process time for the calling process, e.g. on MacOS
DEKAF2_PUBLIC
inline std::size_t kGetMicroTicksPerThread()
{
#ifndef RUSAGE_THREAD
	return detail::TicksFromRusage(RUSAGE_SELF);
#else
	return detail::TicksFromRusage(RUSAGE_THREAD);
#endif
}

/// Returns process time for the calling process in microseconds.
DEKAF2_PUBLIC
inline std::size_t kGetMicroTicksPerProcess()
{
	return detail::TicksFromRusage(RUSAGE_SELF);
}

/// Returns process time for the children of the calling process in microseconds. Does not work on Windows
DEKAF2_PUBLIC
inline std::size_t kGetMicroTicksPerChildProcesses()
{
	return detail::TicksFromRusage(RUSAGE_CHILDREN);
}

/// Set the global locale for all threads, set to environment preset if sLocale == empty. You should
/// in general leave the global locale untouched - the exception is a server application that does
/// not reply to the local user, but over the internet. In which case you may probably want to set
/// the global locale to en_US.UTF-8 (or any other UTF-8 locale if you prefer), and use _local_
/// locales for serving different regions.
/// @par sLocale the locale string to construct the std::locale from. Use a unicode aware locale.
/// @return true if locale could be set, false otherwise
DEKAF2_PUBLIC
bool kSetGlobalLocale(KStringViewZ sLocale = KStringViewZ{});

/// @return the currently set global locale
DEKAF2_PUBLIC
std::locale kGetGlobalLocale();

/// This is how you should in general work with locales - leaving the global locale untouched,
/// and operating with user specific local locales.
/// @param sLocale a string to lookup the locale. Normally in the form "en_US.UTF-8"
/// @param bThrow throw KException if locale is not available, defaults to false, in which
/// case the returned locale will be the default locale
/// @return a locale that matches the description in sLocale or the default locale
DEKAF2_PUBLIC
std::locale kGetLocale(KStringViewZ sLocale = KStringViewZ{}, bool bThrow = false);

/// @return the decimal point in the given locale (defaults to the global locale)
DEKAF2_PUBLIC
char kGetDecimalPoint(const std::locale& locale = std::locale());

/// @return the thousands separator in the given locale (defaults to the global locale)
DEKAF2_PUBLIC
char kGetThousandsSeparator(const std::locale& locale = std::locale());

/// @return the full path name of the running exe
DEKAF2_PUBLIC
const KString& kGetOwnPathname();

#ifndef DEKAF2_IS_WINDOWS
// pretty useless on non-Windows, but declare for compatibility
using HANDLE = intptr_t;
#endif

/// @return a windows file handle for a file descriptor
HANDLE kGetHandleFromFileDescriptor(int fd);

/// @return a file descriptor for a windows file handle (must be closed after use!)
int kGetFileDescriptorFromHandle(HANDLE handle);

/// return the file name for a given file descriptor
/// @param fd file descriptor (int value)
/// @return file name for a given file descriptor, or empty string in case of error
DEKAF2_PUBLIC
KString kGetFileNameFromFileDescriptor(int fd);

/// return the file name for a given file handle (although it works for non-Windows, use it in Windows environments only)
/// @param handle file handle
/// @return file name for a given file handle, or empty string in case of error
DEKAF2_PUBLIC
KString kGetFileNameFromFileHandle(HANDLE handle);

/// holds size of a terminal window
struct DEKAF2_PUBLIC KTTYSize
{
	uint16_t lines   { 0 };
	uint16_t columns { 0 };
};

/// return column and line counts of the terminal with file descriptor @p fd
/// @param fd the file descriptor of a terminal window, default = stdout
/// @param iDefaultColumns default column count if not available, default = 80
/// @param iDefaultLines default line count if not available, default = 25
/// @return the size of the terminal with file descriptor  @p fd in a struct KTTYSize
DEKAF2_PUBLIC
KTTYSize kGetTerminalSize(int fd = 0, uint16_t iDefaultColumns = 80, uint16_t iDefaultLines = 25);

/// returns true if the argument points inside the preinitialized data segment of the program
/// (this is important to make a difference e.g. between objects that persist, and those that
/// can go away)
DEKAF2_PUBLIC
bool kIsInsideDataSegment(const void* addr);

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to fetch the system info on construction
class KUNameBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KUNameBase() noexcept;
	KUNameBase(const KUNameBase& other) noexcept;

#ifdef DEKAF2_IS_WINDOWS
	struct utsname
	{
		const char* sysname;
		char nodename[256];
		char release[1];
		char version[256];
		const char* machine;
	};
#endif

	utsname m_UTSName;

}; // KUNameBase

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// access uname system info
class DEKAF2_PUBLIC KUName : private detail::KUNameBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KUName() noexcept;
	KUName(const KUName& other) noexcept;

	/// name of OS
	KStringViewZ sysname;
	/// name of this network node
	KStringViewZ nodename;
	/// release level
	KStringViewZ release;
	/// version level
	KStringViewZ version;
	/// hardware type
	KStringViewZ machine;

}; // KUName

DEKAF2_NAMESPACE_END
