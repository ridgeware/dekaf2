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

#include "kformat.h"
#include "kstring.h"
#include "bits/kcppcompat.h"
#include <fmt/ostream.h>
#include <fmt/printf.h>
#include "klog.h"

namespace fmt {
	template<>
	struct is_contiguous<dekaf2::KString> : std::true_type {};
}

namespace dekaf2 {

namespace detail {

//-----------------------------------------------------------------------------
/// formats a std::ostream using Python syntax
std::ostream& kfFormat(std::ostream& os, KStringView sFormat, fmt::format_args args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		fmt::vprint(os, sFormat.operator fmt::string_view(), args);
	}
	DEKAF2_CATCH (std::exception& e)
	{
		kWarning("bad format arguments for: \"{}\"", sFormat);
		kTraceDownCaller(4, "klog.h,kformat.cpp,kformat.h", e.what());
	}
	return os;

} // kfFormat

//-----------------------------------------------------------------------------
/// formats a KString using Python syntax
KString kFormat(KStringView sFormat, fmt::format_args args)
//-----------------------------------------------------------------------------
{
	KString sOut;
	DEKAF2_TRY
	{
		fmt::vformat_to(std::back_inserter(sOut), sFormat.operator fmt::string_view(), args);
	}
	DEKAF2_CATCH (std::exception& e)
	{
		kWarning("bad format arguments for: \"{}\"", sFormat);
		kTraceDownCaller(4, "klog.h,kformat.cpp,kformat.h", e.what());
	}
	return sOut;

} // kFormat

//-----------------------------------------------------------------------------
/// formats a std::ostream using POSIX printf syntax
std::ostream& kfPrintf(std::ostream& os, KStringView sFormat, fmt::printf_args args)
//-----------------------------------------------------------------------------
{
	DEKAF2_TRY
	{
		fmt::vfprintf(os, sFormat.operator fmt::string_view(), args);
	}
	DEKAF2_CATCH (std::exception& e)
	{
		kWarning("bad format arguments for: \"{}\"", sFormat);
		kTraceDownCaller(4, "klog.h,kformat.cpp,kformat.h", e.what());
	}
	return os;

} // kfFormat

//-----------------------------------------------------------------------------
/// formats a KString using POSIX printf syntax
KString kPrintf(KStringView sFormat, fmt::printf_args args)
//-----------------------------------------------------------------------------
{
	fmt::memory_buffer buffer;
	DEKAF2_TRY
	{
		fmt::printf(buffer, sFormat.operator fmt::string_view(), args);
	}
	DEKAF2_CATCH (std::exception& e)
	{
		kWarning("bad format arguments for: \"{}\"", sFormat);
		kTraceDownCaller(4, "klog.h,kformat.cpp,kformat.h", e.what());
	}
	return { buffer.data(), buffer.size() };

} // kFormat


} // end of namespace detail

} // end of namespace dekaf2

