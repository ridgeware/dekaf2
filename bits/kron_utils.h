/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#pragma once

#include "../kdefinitions.h"
#include "../kstringview.h"
#include "../kstring.h"
#include "../kstringutils.h"
#include "../ktime.h"

DEKAF2_NAMESPACE_BEGIN

struct Kron_utils
{
	using string_t     = KString;
	using stringview_t = KStringView;

#ifdef _NDEBUG
	static inline void ThrowAssert(bool bCondition, const char* sText) {}
#else
	static void ThrowAssert(bool bCondition, const char* sText)
	{
		if (!bCondition)
		{
			throw std::runtime_error(sText);
		}
	}
#endif

	static inline std::time_t tm_to_time(std::tm& date)
	{
		// we use timegm() directly as it modifies the input argument, which
		// is needed by the parser to normalize incremented field values
#ifdef DEKAF2_IS_WINDOWS
		return _mkgmtime(&date);
#else
		return timegm(&date);
#endif
	}

	static inline std::tm* time_to_tm(std::time_t const * date, std::tm* const out)
	{
		ThrowAssert(date != nullptr, "date must not be nullptr");
		ThrowAssert(out  != nullptr, "out must not be nullptr");
		KUTCTime Time(KUnixTime::from_time_t(*date));
		*out = Time.to_tm();
		return out;
	}

	static inline std::tm to_tm(stringview_t sTime)
	{
		KUTCTime Time(sTime);
		return Time.to_tm();
	}

	static inline string_t to_string(const std::tm& tm)
	{
		return kFormTimestamp(tm);
	}

	static inline string_t to_upper(string_t text)
	{
		text.MakeUpperASCII();
		return text;
	}

	static std::vector<stringview_t> split(stringview_t text, char const delimiter)
	{
		return text.Split(KStringView(&delimiter, 1));
	}

	static DEKAF2_CONSTEXPR_17 bool contains(stringview_t text, char const ch) noexcept
	{
		return stringview_t::npos != text.find(ch);
	}

	static uint16_t to_cron_int(const stringview_t& text)
	{
		if (!kIsInteger(text, false))
		{
			throw std::runtime_error("input is not a (positive) integer");
		}
		return text.UInt16();
	}

}; // Kron_utils

DEKAF2_NAMESPACE_END
