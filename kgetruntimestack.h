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

/// @file kgetruntimestack.h
/// provides a stack tracer

#include "kstring.h"
#include "kjson.h"

namespace dekaf2
{

/// strips a fully qualified function name down to a minimum
KString kNormalizeFunctionName(KStringView sFunctionName);

/// get a full runtime stack trace (uses gdb if possible). The output
/// of this function is as detailed as possible, and intended for crash
/// situations.
KString kGetRuntimeStack (int iSkipStackLines = 2);

/// kGetRuntimeStack() as a JSON array.
KJSON kGetRuntimeStackJSON (int iSkipStackLines = 3);

/// get a stack trace (uses gdb if possible). The output of this one is
/// shorter than the one of kGetRuntimeStack, and intended for logging
/// purposes during the runtime of the application.
/// @param iSkipStackLines Number of top stack lines to drop. Defaults to 0.
KString kGetBacktrace (int iSkipStackLines = 2);

/// Struct to keep all details about one stack frame
struct KStackFrame
{
	KStackFrame(KString _sFunction = {}, KString _sFile = {}, KString _sLineNumber = {})
	: sFunction(std::move(_sFunction))
	, sFile(std::move(_sFile))
	, sLineNumber(std::move(_sLineNumber))
	{
	}

	KString sFunction;
	KString sFile;
	KString sLineNumber;

}; // KStackFrame

/// get function name, filename and line number of the first stackframe that is not listed
/// in sSkipFiles
KStackFrame kFilterTrace (int iSkipStackLines, KStringView sSkipFiles);

}
