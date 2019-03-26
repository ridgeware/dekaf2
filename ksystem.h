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

namespace dekaf2
{

/// Get environment variable. Return @p szDefault if not found.
KStringViewZ kGetEnv (KStringViewZ szEnvVar, KStringViewZ szDefault = "");

/// Set environment variable.
bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ sValue);

/// Unset environment variable.
bool kUnsetEnv (KStringViewZ szEnvVar);

/// Set operating system current working directory.
bool kSetCWD (KStringViewZ sPath);

/// Return operating system current working directory as a string.
KString kGetCWD ();

/// alias to kGetCWD(): return operating system current working directory as a string.
inline KString kcwd () { return kGetCWD(); }

/// Return user's home directory
KString kGetHome();

/// Return the system's tmp directory
KString kGetTemp();

/// Return current operating system user as a string.
KString kGetWhoAmI ();

/// alias to kGetWhoami(): return current operating system user as a string.
inline KString kwhoami () { return kGetWhoAmI(); }

/// Return operating system hostname as a string.
KStringViewZ kGetHostname ();

/// return process ID
inline pid_t kGetPid()
{
	return getpid();
}

/// return thread ID
uint64_t kGetTid();

/// Execute the given command, redirect stdout and stderr into a temp file and then return in the given sOutput string.  Return code matches the exit code command that was run: 0 is normally an indication of success.
int kSystem (KStringView sCommand, KString& sOutput);

/// Execute the given command, redirect stdout and stderr into /dev/null.  Return code matches the exit code command that was run: 0 is normally an indication of success.
int kSystem (KStringView sCommand);

/// Resolve the given hostname into either an IPv4 IP address or an IPv6 address.  If hostname fails to resolve, return empty string.
KString kResolveHost (KStringViewZ sHostname, bool bIPv4, bool bIPv6);

/// Resolve the given hostname into an IPv4 IP address, e.g. "50.1.2.3".  If hostname fails to resolve, return empty string.
inline KString kResolveHostIPV4 (KStringViewZ sHostname)
{
	return kResolveHost (sHostname, true, false);
}

/// Resolve the given hostname into an IPv6 IP address, e.g. "fe80::be27:ebff:fad4:49e7".  If hostname fails to resolve, return empty string.
inline KString kResolveHostIPV6 (KStringViewZ sHostname)
{
	return kResolveHost (sHostname, false, true);
}

/// Sleep for the amount of nanoseconds
void kNanoSleep(uint64_t iNanoSeconds);

/// Sleep for the amount of microseconds
void kMicroSleep(uint64_t iMicroSeconds);

/// Sleep for the amount of milliseconds
void kMilliSleep(uint64_t iMilliSeconds);

/// Sleep for the amount of seconds
void kSleep(uint64_t iSeconds);

/// Returns 32 bit unsigned random number in range [iMin - iMax].
uint32_t kRandom(uint32_t iMin = 0, uint32_t iMax = UINT32_MAX);

/// Block program from running for random amount of time within the given min and max.
void kSleepRandomMilliseconds (uint32_t iMinMilliseconds, uint32_t iMaxMilliseconds);

/// Block program from running for random amount of time within the given min and max.
inline void kSleepRandomSeconds (uint32_t iMinSeconds, uint32_t iMaxSeconds)
{
	kSleepRandomMilliseconds (iMinSeconds * 1000, iMaxSeconds * 1000);
}

} // end of namespace dekaf2

