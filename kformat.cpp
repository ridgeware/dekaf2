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
#endif

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
/// format no-op for filedesc
bool kWrite(int fd, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWrite(fd, sFormat.data(), sFormat.size()) == sFormat.size();
}

//-----------------------------------------------------------------------------
/// format no-op for filedesc
bool kWriteLine(int fd, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWrite(fd, sFormat) && kWrite(fd, "\n");
}

//-----------------------------------------------------------------------------
/// format no-op for std::FILE*
bool kWrite(std::FILE* fp, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWrite(fp, sFormat.data(), sFormat.size()) == sFormat.size();
}

//-----------------------------------------------------------------------------
/// format no-op for std::FILE*
bool kWriteLine(std::FILE* fp, KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWrite(fp, sFormat) && kWrite(fp, "\n");
}

//-----------------------------------------------------------------------------
std::ostream& kWrite(std::ostream& os, KStringView sOutput) noexcept
//-----------------------------------------------------------------------------
{
	return os.write(sOutput.data(), sOutput.size());
}

//-----------------------------------------------------------------------------
std::ostream& kWriteLine(std::ostream& os, KStringView sOutput) noexcept
//-----------------------------------------------------------------------------
{
	return os.write(sOutput.data(), sOutput.size()).write("\n", 1);
}

//-----------------------------------------------------------------------------
bool kWrite(KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWrite(stdout, sFormat);
}

//-----------------------------------------------------------------------------
bool kWriteLine(KStringView sFormat) noexcept
//-----------------------------------------------------------------------------
{
	return kWriteLine(stdout, sFormat);
}

//-----------------------------------------------------------------------------
bool kFlush() noexcept
//-----------------------------------------------------------------------------
{
	return kFlush(stdout);
}

#if !DEKAF2_FORMAT_INLINE || !DEKAF2_HAS_FORMAT_RUNTIME

namespace kformat_detail {

//-----------------------------------------------------------------------------
/// formats a KString using Python syntax
KString Format(DEKAF2_FORMAT_NAMESPACE::string_view sFormat, const DEKAF2_FORMAT_NAMESPACE::format_args& args) noexcept
//-----------------------------------------------------------------------------
{
	KString sOut;

	DEKAF2_TRY
	{
		sOut = DEKAF2_FORMAT_NAMESPACE::vformat(sFormat, args);
	}
	DEKAF2_CATCH (const std::exception& e)
	{
		kTraceDownCaller(4, "klog.cpp,klog.h,kformat.cpp,kformat.h,kgetruntimestack.cpp,kgetruntimestack.h,ktime.cpp,ktime.h,kdate.cpp,kdate.h",
						 DEKAF2_FORMAT_NAMESPACE::format("bad format arguments for: \"{}\": {}", sFormat, e.what()));
	}

	return sOut;

} // Format

//-----------------------------------------------------------------------------
/// formats a KString using Python syntax, with locale
KString Format(const std::locale& locale, DEKAF2_FORMAT_NAMESPACE::string_view sFormat, const DEKAF2_FORMAT_NAMESPACE::format_args& args) noexcept
//-----------------------------------------------------------------------------
{
	KString sOut;

	DEKAF2_TRY
	{
		sOut = DEKAF2_FORMAT_NAMESPACE::vformat(locale, sFormat, args);
	}
	DEKAF2_CATCH (const std::exception& e)
	{
		kTraceDownCaller(4, "klog.cpp,klog.h,kformat.cpp,kformat.h,kgetruntimestack.cpp,kgetruntimestack.h,ktime.cpp,ktime.h,kdate.cpp,kdate.h",
						 DEKAF2_FORMAT_NAMESPACE::format("bad format arguments for: \"{}\": {}", sFormat, e.what()));
	}

	return sOut;

} // Format

} // end of namespace kformat_detail

#endif

DEKAF2_NAMESPACE_END
