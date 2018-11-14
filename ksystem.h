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

#include <cstdlib>
#include "kstring.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// Get environment variable. Return @p szDefault if not found.
inline KStringViewZ kGetEnv (KStringViewZ szEnvVar, KStringViewZ szDefault = "")
//-----------------------------------------------------------------------------
{
	KStringViewZ sValue = ::getenv(szEnvVar);
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
inline bool kSetEnv (KStringViewZ szEnvVar, KStringViewZ sValue)
//-----------------------------------------------------------------------------
{
	bool bOK = (::setenv(szEnvVar, sValue, true) == 0);
	if (!bOK)
	{
		kWarning("cannot set {} = {}, {}", szEnvVar, sValue, strerror(errno));
	}
	return (bOK);
}

//-----------------------------------------------------------------------------
/// Unset environment variable.
inline bool kUnsetEnv (KStringViewZ szEnvVar)
//-----------------------------------------------------------------------------
{
	bool bOK = (::unsetenv(szEnvVar) == 0);
	if (!bOK)
	{
		kWarning("cannot unset {}, {}", szEnvVar, strerror(errno));
	}
	return (bOK);
}

/// Set operating system current working directory.
bool kSetCWD (KStringViewZ sPath);

/// Return operating system current working directory as a string.
KString kGetCWD ();

/// alias to kGetCWD(): return operating system current working directory as a string.
inline KString kcwd () { return kGetCWD(); }

/// Return user's home directory
KString kGetHome();

/// Return current operating system user as a string.
KString kGetWhoAmI ();

/// alias to kGetWhoami(): return current operating system user as a string.
inline KString kwhoami () { return kGetWhoAmI(); }

/// Return operating system hostname as a string.
KString kGetHostname ();

/// Alias to kGetHostname(): return operating system hostname as a string.
inline KString khostname () { return kGetHostname(); }

/// return process ID
inline pid_t kGetPid()
{
	return getpid();
}

/// return thread ID
uint64_t kGetTid();

/// Execute the given command, redirect stdout and stderr into a temp file and then return in the given sOutput string.  Return code matches the exit code command that was run: 0 is normally an indication of success.
uint8_t ksystem (const KString& sCommand, KString& sOutput);

/// Execute the given command, redirect stdout and stderr into /dev/null.  Return code matches the exit code command that was run: 0 is normally an indication of success.
uint8_t ksystem (const KString& sCommand);

/// Resolve the given hostname into an IPv4 IP address, e.g. "50.1.2.3".  If hostname fails to resolve, return empty string.
KString kResolveHostIPV4 (const KString& sHostname);

/// Block program from running for random amount of time within the given min and max.
void kSleepRandomSeconds (uint64_t iMinSeconds, uint64_t iMaxSeconds);

/// Block program from running for random amount of time within the given min and max.
void kSleepRandomMilliseconds (uint64_t iMinMilliseconds, uint64_t iMaxMilliseconds);

} // end of namespace dekaf2

