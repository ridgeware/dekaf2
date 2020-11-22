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

/// @file kcrashexit.h
/// show reason for a program crash

#include <csignal>
#include "kstring.h"

namespace dekaf2
{

/// special "signal" numbers to be sent to kCrashExit() to indicate
/// crash reason
enum
{
	CRASHCODE_MEMORY      = -100,   // <-- parameter for kCrashExit() for malloc failures
	CRASHCODE_TODO        = -101,   // <-- feature not implmented yet
	CRASHCODE_DEKAFUSAGE  = -102,   // <-- invalid DEKAF framework usage
	CRASHCODE_CORRUPT     = -103,   // <-- self-detected memory corruption
	CRASHCODE_DBERROR     = -104,   // <-- self-detected fatal database error
	CRASHCODE_DBINTEGRITY = -105    // <-- self-detected database integrity problem
};

extern "C" {

#ifdef DEKAF2_IS_WINDOWS
	struct siginfo_t {};
#endif

/// display signal that led to crash (if any) and force a stackdump
/// @param iSignalNum The caught signal or one of the special signal
/// numbers to indicate library internal failures.
void kCrashExitExt (int iSignalNum, siginfo_t* siginfo = nullptr, void* context = nullptr);

/// display signal that led to crash (if any) and force a stackdump
/// @param iSignalNum The caught signal or one of the special signal
/// numbers to indicate library internal failures.
void kCrashExit (int iSignalNum=0);

}

using KCrashCallback = std::function<void(KStringView sMessage)>;

/// allow the appliation to set some context information in case the app crashes
void kSetCrashContext (KStringView sContext);

/// in addition to the KLOG, all the application to get a callback when it is crashing with the crash message (app will still exit after callback is done)
void kSetCrashCallback (KCrashCallback pFunction);

namespace detail {

void kFailedAssert(const char* sCrashMessage);

}

inline
void kAssert (bool bMustBeTrue, KStringView sCrashMessage)
{
	if (!bMustBeTrue)
	{
		detail::kFailedAssert(sCrashMessage.data());
	}
}

inline
void kAssert (bool bMustBeTrue, const char* sCrashMessage)
{
	if (!bMustBeTrue)
	{
		detail::kFailedAssert(sCrashMessage);
	}
}

} // end of namespace dekaf2

