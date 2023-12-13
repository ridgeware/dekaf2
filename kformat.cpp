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
#include "kwrite.h"
#include "kstring.h"
#include "klog.h"

#ifndef DEKAF2_HAS_STD_FORMAT
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
template<>
struct DEKAF2_PREFIX format::is_contiguous<DEKAF2_PREFIX KString> : std::true_type {};
#endif
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
/// format no-op for filedesc
bool kPrint(int fd, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWrite(fd, sFormat.data(), sFormat.size()) == sFormat.size();
}

//-----------------------------------------------------------------------------
/// format no-op for filedesc
bool kPrintLine(int fd, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(fd, sFormat) && kPrint(fd, "\n");
}

//-----------------------------------------------------------------------------
/// format no-op for std::FILE*
bool kPrint(std::FILE* fp, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWrite(fp, sFormat.data(), sFormat.size()) == sFormat.size();
}

//-----------------------------------------------------------------------------
/// format no-op for std::FILE*
bool kPrintLine(std::FILE* fp, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(fp, sFormat) && kPrint(fp, "\n");
}

//-----------------------------------------------------------------------------
bool kPrint(KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kPrint(stdout, sFormat);
}

//-----------------------------------------------------------------------------
bool kPrintLine(KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kPrintLine(stdout, sFormat);
}

namespace detail {

//-----------------------------------------------------------------------------
/// formats a KString using Python syntax
KString kFormat(KStringView sFormat, const dekaf2_format::format_args& args) noexcept
//-----------------------------------------------------------------------------
{
	KString sOut;

	DEKAF2_TRY
	{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
		dekaf2_format::vformat_to(std::back_inserter(sOut), sFormat.operator format::string_view(), args);
#elif DEKAF2_KSTRINGVIEW_IS_STD_STRINGVIEW
		sOut = dekaf2_format::vformat(sFormat, args);
#else
		sOut = dekaf2_format::vformat(sFormat.operator dekaf2_format::string_view(), args);
#endif
	}
	DEKAF2_CATCH (const std::exception& e)
	{
		kTraceDownCaller(4, "klog.cpp,klog.h,kformat.cpp,kformat.h,kgetruntimestack.cpp,kgetruntimestack.h,ktime.cpp,ktime.h,kdate.cpp,kdate.h",
						 kFormat("bad format arguments for: \"{}\": {}", sFormat, e.what()));
	}

	return sOut;

} // kFormat

//-----------------------------------------------------------------------------
/// formats a KString using Python syntax, with locale
KString kFormat(const std::locale& locale, KStringView sFormat, const dekaf2_format::format_args& args) noexcept
//-----------------------------------------------------------------------------
{
	KString sOut;

	DEKAF2_TRY
	{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
		format::vformat_to(locale, std::back_inserter(sOut), sFormat.operator format::string_view(), args);
#elif DEKAF2_KSTRINGVIEW_IS_STD_STRINGVIEW
		sOut = dekaf2_format::vformat(locale, sFormat, args);
#else
		sOut = dekaf2_format::vformat(locale, sFormat.operator dekaf2_format::string_view(), args);
#endif
	}
	DEKAF2_CATCH (const std::exception& e)
	{
		kTraceDownCaller(4, "klog.cpp,klog.h,kformat.cpp,kformat.h,kgetruntimestack.cpp,kgetruntimestack.h,ktime.cpp,ktime.h,kdate.cpp,kdate.h",
						 kFormat("bad format arguments for: \"{}\": {}", sFormat, e.what()));
	}

	return sOut;

} // kFormat

} // end of namespace detail

DEKAF2_NAMESPACE_END
