/*
//-----------------------------------------------------------------------------//
//
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

/// @file kstringutils.h
/// standalone string utility functions

#include <cinttypes>
#include <algorithm>
#include <functional>
#include "bits/kcppcompat.h"
#include "bits/ktemplate.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
template<class String>
String& kPadLeft(String& string, size_t iWidth, typename String::value_type chPad = ' ')
//-----------------------------------------------------------------------------
{
	if (string.size() < iWidth)
	{
		string.insert(0, iWidth - string.size(), chPad);
	}
	return string;
}

//-----------------------------------------------------------------------------
template<class String>
String& kPadRight(String& string, size_t iWidth, typename String::value_type chPad = ' ')
//-----------------------------------------------------------------------------
{
	if (string.size() < iWidth)
	{
		string.append(iWidth - string.size(), chPad);
	}
	return string;
}

//-----------------------------------------------------------------------------
template<class String, class Compare>
String& kTrimLeft(String& string, Compare cmp)
//-----------------------------------------------------------------------------
{
	auto it = std::find_if_not(string.begin(), string.end(), cmp);
	typename String::size_type iDelete = static_cast<typename String::size_type>(it - string.begin());
	if (iDelete)
	{
		string.erase(0, iDelete);
	}
	return string;
}

//-----------------------------------------------------------------------------
template<class String>
String& kTrimLeft(String& string)
//-----------------------------------------------------------------------------
{
	return kTrimLeft(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
template<class String, class Compare>
String& kTrimRight(String& string, Compare cmp)
//-----------------------------------------------------------------------------
{
	auto it = std::find_if_not(string.rbegin(), string.rend(), cmp);
	auto iDelete = static_cast<typename String::size_type>(it - string.rbegin());
	if (iDelete)
	{
		string.resize(string.size() - iDelete);
	}
	return string;
}

//-----------------------------------------------------------------------------
template<class String>
String& kTrimRight(String& string)
//-----------------------------------------------------------------------------
{
	return kTrimRight(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
template<class String, class Compare>
String& kTrim(String& string, Compare cmp)
//-----------------------------------------------------------------------------
{
	kTrimRight(string, cmp);
	return kTrimLeft(string, cmp);
}

//-----------------------------------------------------------------------------
template<class String>
String& kTrim(String& string)
//-----------------------------------------------------------------------------
{
	return kTrim(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
std::string kFormTimestamp (time_t tTime = 0, const char* pszFormat = "%Y-%m-%d %H:%M:%S");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
std::string kTranslateSeconds(int64_t iNumSeconds, bool bLongForm = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
template <class Arithmetic, class String = KString>
String kFormNumber(Arithmetic i, typename String::value_type separator = ',', typename String::size_type every = 3)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_arithmetic<Arithmetic>::value, "arithmetic type required");
	String result;

	try
	{
		result = std::to_string(i);
	}
	catch (...)
	{
		// that conversion failed..
		result.clear();
	}

	if (every > 0)
	{
		// now insert the separator every N digits
		auto last = every;
		if (i < 0)
		{
			// do not count the leading '-' as an insertion point
			++last;
		}

		auto pos = result.length();
		while (pos > last)
		{
			result.insert(pos-every, 1, separator);
			pos -= every;
		}
	}
	return result;
}

//-----------------------------------------------------------------------------
size_t kCountChar(KStringView str, const char ch) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
template<class Container>
size_t kCountChar(const Container& container, const typename Container::value_type ch) noexcept
//-----------------------------------------------------------------------------
{
	size_t ret{0};
	size_t len = container.size();

	for (auto it = container.data(); len--;)
	{
		if (*it++ == ch)
		{
			++ret;
		}
	}

	return ret;
}

//-----------------------------------------------------------------------------
template<class Char>
inline bool kIsDigit(Char ch) noexcept
//-----------------------------------------------------------------------------
{
	return std::isdigit(ch);
}

//-----------------------------------------------------------------------------
bool kIsDecimal(KStringView str) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool kIsEmail(KStringView str) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool kIsURL(KStringView str) noexcept;
//-----------------------------------------------------------------------------

// exception free conversions

//-----------------------------------------------------------------------------
inline long double kToLongDouble(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtold(s, nullptr);
}

//-----------------------------------------------------------------------------
inline double kToDouble(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::atof(s);
}

//-----------------------------------------------------------------------------
inline float kToFloat(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	return static_cast<float>(kToDouble(s));
}

//-----------------------------------------------------------------------------
inline short int kToShort(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return static_cast<short int>(std::atoi(s));
}

//-----------------------------------------------------------------------------
inline unsigned short int kToUShort(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return static_cast<unsigned short int>(std::strtoul(s, nullptr, 10));
}

//-----------------------------------------------------------------------------
inline int kToInt(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::atoi(s);
}

//-----------------------------------------------------------------------------
inline unsigned int kToUInt(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return static_cast<unsigned int>(std::strtoul(s, nullptr, 10));
}

//-----------------------------------------------------------------------------
inline long kToLong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtol(s, nullptr, 10);
}

//-----------------------------------------------------------------------------
inline unsigned long kToULong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtoul(s, nullptr, 10);
}

//-----------------------------------------------------------------------------
inline long long kToLongLong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtoll(s, nullptr, 10);
}

//-----------------------------------------------------------------------------
inline unsigned long long kToULongLong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtoull(s, nullptr, 10);
}

template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline float kToFloat(const String& s) noexcept { return kToFloat(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline double kToDouble(const String& s) noexcept { return kToDouble(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline long double kToLongDouble(const String& s) noexcept { return kToLongDouble(s.data); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline int kToShort(const String& s) noexcept { return kToShort(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline int kToInt(const String& s) noexcept { return kToInt(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline long kToLong(const String& s) noexcept { return kToLong(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline long long kToLongLong(const String& s) noexcept { return kToLongLong(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline unsigned int kToUShort(const String& s) noexcept { return kToUShort(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline unsigned int kToUInt(const String& s) noexcept { return kToUInt(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline unsigned long kToULong(const String& s) noexcept { return kToULong(s.data()); }
template<class String, typename = std::enable_if_t<detail::is_narrow_cpp_str<String>::value> >
inline unsigned long long kToULongLong(const String& s) noexcept { return kToULongLong(s.data()); }

namespace // hide kx2c in an anonymous namespace
{

//-----------------------------------------------------------------------------
template<class Ch>
inline Ch kx2c (Ch* pszGoop)
//-----------------------------------------------------------------------------
{
	Ch digit;

	digit = (pszGoop[0] >= 'A' ? ((pszGoop[0] & 0xdf) - 'A')+10 : (pszGoop[0] - '0'));
	digit *= 16;
	digit += (pszGoop[1] >= 'A' ? ((pszGoop[1] & 0xdf) - 'A')+10 : (pszGoop[1] - '0'));

	return digit;

} // kx2c

} // anonymous until here

//-----------------------------------------------------------------------------
template<class String>
void kUrlDecode (String& sDecode)
//-----------------------------------------------------------------------------
{
	auto insert  = &sDecode[0];
	auto current = insert;
	auto end     = current + sDecode.size();
	while (current != end)
	{
		if (*current == '+')
		{
			*insert++ = ' ';
			++current;
		}
		else if (*current == '%'
			&& end - current > 2
			&& std::isxdigit(*(current + 1))
			&& std::isxdigit(*(current + 2)))
		{
			*insert++ = kx2c(current + 1);
			current += 3;
		}
		else
		{
			*insert++ = *current++;
		}
	}

	if (insert < end)
	{
		size_t nsz = insert - &sDecode[0];
		sDecode.erase(nsz);
	}

} // kUrlDecode

} // end of namespace dekaf2
