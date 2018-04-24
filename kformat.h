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
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/printf.h>
#include "bits/kcppcompat.h"

namespace dekaf2
{

namespace kFormat_internal
{

void report_format_exception(const char* where);
void report_format_exception(std::exception& e, const char* where);

} // end of namespace kFormat_internal

//-----------------------------------------------------------------------------
/// formats a string using Python syntax
template<class... Args>
std::string kFormat(Args&&... args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {
		return fmt::format(std::forward<Args>(args)...);
	} DEKAF2_CATCH (std::exception& e) {
		kFormat_internal::report_format_exception(e, DEKAF2_FUNCTION_NAME);
	}
	return std::string();
}

//-----------------------------------------------------------------------------
/// formats a std::ostream using Python syntax
template<class... Args>
std::ostream& kfFormat(std::ostream& os, Args&&... args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {
		fmt::print(os, std::forward<Args>(args)...);
	} DEKAF2_CATCH (std::exception& e) {
		kFormat_internal::report_format_exception(e, DEKAF2_FUNCTION_NAME);
	}
	return os;
}

//-----------------------------------------------------------------------------
/// formats a string using POSIX printf syntax
template<class... Args>
std::string kPrintf(Args&&... args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {
		return fmt::sprintf(std::forward<Args>(args)...);
	} DEKAF2_CATCH (std::exception& e) {
		kFormat_internal::report_format_exception(e, DEKAF2_FUNCTION_NAME);
	}
	return std::string();
}

//-----------------------------------------------------------------------------
/// formats a file using POSIX printf syntax
template<class... Args>
std::ostream& kfPrintf(std::ostream& os, Args&&... args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY {
		std::string tmp = kPrintf(std::forward<Args>(args)...);
		os.write(tmp.data(), static_cast<std::streamsize>(tmp.size()));
	} DEKAF2_CATCH (std::exception& e) {
		kFormat_internal::report_format_exception(e, DEKAF2_FUNCTION_NAME);
	}
	return os;
}

} // end of namespace dekaf2

