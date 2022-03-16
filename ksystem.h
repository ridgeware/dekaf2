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

namespace dekaf2
{

/// Get environment variable. Return @p szDefault if not found.
DEKAF2_PUBLIC
KStringViewZ kGetEnv (KStringViewZ szEnvVar, KStringViewZ szDefault = "");

/// Set environment variable.
DEKAF2_PUBLIC
bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ sValue);

/// Unset environment variable.
DEKAF2_PUBLIC
bool kUnsetEnv (KStringViewZ szEnvVar);

/// Set operating system current working directory.
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

/// Execute the given command, redirect stdout and stderr into a temp file and then return in the given sOutput string.  Return code matches the exit code command that was run: 0 is normally an indication of success.
DEKAF2_PUBLIC
int kSystem (KStringView sCommand, KStringRef& sOutput);

/// Execute the given command, redirect stdout and stderr into /dev/null.  Return code matches the exit code command that was run: 0 is normally an indication of success.
DEKAF2_PUBLIC
int kSystem (KStringView sCommand);

/// Resolve the given hostname into either an IPv4 IP address or an IPv6 address.  If hostname fails to resolve, return empty string.
DEKAF2_PUBLIC
KString kResolveHost (KStringViewZ sHostname, bool bIPv4, bool bIPv6);

/// Resolve the given hostname into an IPv4 IP address, e.g. "50.1.2.3".  If hostname fails to resolve, return empty string.
DEKAF2_PUBLIC
inline KString kResolveHostIPV4 (KStringViewZ sHostname)
{
	return kResolveHost (sHostname, true, false);
}

/// Resolve the given hostname into an IPv6 IP address, e.g. "fe80::be27:ebff:fad4:49e7".  If hostname fails to resolve, return empty string.
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
std::size_t kGetCPUCount();

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

/// Set the global locale for all threads, set to environment preset if sLocale == empty
/// @par sLocale the locale string to construct the std::locale from. Use a unicode aware locale.
/// @return true if locale could be set, false otherwise
DEKAF2_PUBLIC
bool kSetGlobalLocale(KStringViewZ sLocale = KStringViewZ{});

/// @return the currently set global locale
DEKAF2_PUBLIC
std::locale kGetGlobalLocale();

/// @return the decimal point in the currently set global locale
DEKAF2_PUBLIC
char kGetDecimalPoint();

/// @return the thousands separator in the currently set global locale
DEKAF2_PUBLIC
char kGetThousandsSeparator();

} // end of namespace dekaf2

