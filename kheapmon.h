/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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
*/

#pragma once

/// @file kheapmon.h
/// monitoring and profiling memory usage
/// To enable heap profiling you must start the final executable with the environment set to
/// MALLOC_CONF="prof:true,prof_active:false,prof_prefix:jeprof.out"
/// To start profiling, you then call Heap::Profiling::Start() and can then
/// get a report with Heap::Profiling::Dump()

#include "kdefinitions.h"
#include "kstringview.h"
#include <cstddef>

DEKAF2_NAMESPACE_BEGIN

namespace Heap {

/// returns true if the standard malloc implementation is used - which probably means that
/// no extended heap profiling is available
DEKAF2_NODISCARD
bool IsStandardMalloc();
/// return last error code - translate with strerror() .. (thread local implementation)
DEKAF2_NODISCARD
int LastError();
/// print allocation stats, either as text or as JSON
DEKAF2_NODISCARD
KString GetStats(bool bAsJSON = false);

namespace Profiling {

/// output format for profile dumps
enum ReportFormat
{
	RAW,  ///< the raw profile dump for further processing
	TEXT, ///< analysed dump in text format
	SVG,  ///< analysed dump in vector graphics, good for display in a web browser
	PDF   ///< analysed dump in PDF format
};

/// check if monitoring can be switched on
DEKAF2_NODISCARD
bool    IsAvailable();
/// start monitoring
bool    Start();
/// stop monitoring
bool    Stop();
/// dump profile result to file
/// @param sDumpFile the  path name for the output file
/// @param Format the output format, raw or one of the analyzed formats
/// @param sAdditionalOptions further analysis options to pass on to the profiler, like --alloc_space, default none
/// @return true on success
bool    Dump(KStringViewZ sDumpFile, ReportFormat Format, KStringView sAdditionalOptions = KStringView{});
/// dump profile result to string
/// @param Format the output format, raw or one of the analyzed formats
/// @param sAdditionalOptions further analysis options to pass on to the profiler, like --alloc_space, default none
/// @return a string with the dump output
DEKAF2_NODISCARD
KString Dump(ReportFormat Format, KStringView sAdditionalOptions = KStringView{});
/// clear/reset collected data
bool    Reset();
/// returns true if monitoring was started
DEKAF2_NODISCARD
bool    IsStarted();

} // namespace Profiling
} // namespace Heap

DEKAF2_NAMESPACE_END
