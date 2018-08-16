/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

/// @file kformat.h
/// provides basic string formatter functionality

#include <ostream>
#include <fmt/ostream.h>
#include <fmt/printf.h>
#include "bits/kcppcompat.h"
#include "bits/kostringstream.h"
#include "kstring.h"

namespace dekaf2
{

namespace kFormat_internal
{

void report_format_exception(std::exception& e, const char* where);

} // end of namespace kFormat_internal

//-----------------------------------------------------------------------------
/// formats a std::ostream using Python syntax
template<class... Args>
std::ostream& kfFormat(std::ostream& os, Args&&... args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {
		fmt::print(os, std::forward<Args>(args)...);
	} DEKAF2_CATCH (std::exception& e) {
		kFormat_internal::report_format_exception(e, "kFormat");
	}
	return os;
}

//-----------------------------------------------------------------------------
/// formats a KString using Python syntax
template<class... Args>
KString kFormat(Args&&... args)
//-----------------------------------------------------------------------------
{
	KString sOutput;
	KOStringStream oss(sOutput);
	kfFormat(oss, std::forward<Args>(args)...);
	return sOutput;
}

//-----------------------------------------------------------------------------
/// formats a file using POSIX printf syntax
template<class... Args>
std::ostream& kfPrintf(std::ostream& os, Args&&... args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {
		fmt::fprintf(os, std::forward<Args>(args)...);
	} DEKAF2_CATCH (std::exception& e) {
		kFormat_internal::report_format_exception(e, "kfPrintf");
	}
	return os;
}

//-----------------------------------------------------------------------------
/// formats a KString using POSIX printf syntax
template<class... Args>
KString kPrintf(Args&&... args)
//-----------------------------------------------------------------------------
{
	KString sOutput;
	KOStringStream oss(sOutput);
	kfPrintf(oss, std::forward<Args>(args)...);
	return sOutput;
}

} // end of namespace dekaf2

