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

#include <cinttypes>
#include <algorithm>
#include <functional>
#include <cwchar>
#include "kstring.h"
#include "ktemplate.h"

namespace dekaf2
{

//------------------------------------------------------------------------------
// SFINAE enabled for narrow cpp strings only..
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
typename String::size_type Replace(String& string,
                                   const typename String::value_type* pszSearch,
                                   typename String::size_type iSearchLen,
                                   const typename String::value_type* pszReplaceWith,
                                   typename String::size_type iReplaceWithLen,
                                   bool bReplaceAll = true)
//------------------------------------------------------------------------------
{
	if (!iSearchLen || string.size() < iSearchLen || !pszSearch || !pszReplaceWith)
	{
		return 0;
	}

	typedef typename String::size_type size_type;
	typedef typename String::value_type value_type;

	size_type iNumReplacement = 0;
	size_type needleSize = iSearchLen;
	auto haystack = string.data();
	size_type haystackSize = string.size();

	auto pszFound = static_cast<const value_type*>(memmem(haystack, haystackSize, pszSearch, needleSize));

	if (pszFound)
	{

		if (iReplaceWithLen <= iSearchLen)
		{
			// execute an in-place substitution (C++17 actually has a non-const string.data())
			value_type* pszTarget = const_cast<value_type*>(haystack);

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);
				if (pszTarget < haystack)
				{
					memmove(pszTarget, haystack, untouchedSize);
				}
				pszTarget += untouchedSize;

				if (iReplaceWithLen)
				{
					memmove(pszTarget, pszReplaceWith, iReplaceWithLen);
					pszTarget += iReplaceWithLen;
				}

				haystack = pszFound + needleSize;
				haystackSize -= (needleSize + untouchedSize);

				pszFound = static_cast<const value_type*>(memmem(haystack, haystackSize, pszSearch, needleSize));

				++iNumReplacement;

				if (!bReplaceAll)
				{
					break;
				}
			}

			if (haystackSize)
			{
				memmove(pszTarget, haystack, haystackSize);
				pszTarget += haystackSize;
			}

			auto iResultSize = static_cast<size_type>(pszTarget - string.data());
			string.resize(iResultSize);

		}
		else
		{
			// execute a copy substitution
			String sResult;
			sResult.reserve(string.size());

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);
				sResult.append(haystack, untouchedSize);
				sResult.append(pszReplaceWith, iReplaceWithLen);

				haystack = pszFound + needleSize;
				haystackSize -= (needleSize + untouchedSize);

				pszFound = static_cast<const value_type*>(memmem(haystack, haystackSize, pszSearch, needleSize));

				++iNumReplacement;

				if (!bReplaceAll)
				{
					break;
				}
			}

			sResult.append(haystack, haystackSize);
			string.swap(sResult);
		}
	}

	return iNumReplacement;
}


//-----------------------------------------------------------------------------
template<class String, class Compare>
String& TrimLeft(String& string, Compare cmp)
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
String& TrimLeft(String& string)
//-----------------------------------------------------------------------------
{
	return TrimLeft(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
template<class String, class Compare>
String& TrimRight(String& string, Compare cmp)
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
String& TrimRight(String& string)
//-----------------------------------------------------------------------------
{
	return TrimRight(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
template<class String, class Compare>
String& Trim(String& string, Compare cmp)
//-----------------------------------------------------------------------------
{
	TrimRight(string, cmp);
	return TrimLeft(string, cmp);
}

//-----------------------------------------------------------------------------
template<class String>
String& Trim(String& string)
//-----------------------------------------------------------------------------
{
	return Trim(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
const char* KFormTimestamp (char* szBuffer, size_t iMaxBuf, time_t tTime, const char* pszFormat);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
const char* KFormTimestamp (time_t tTime = 0, const char* pszFormat = "%Y-%m-%d %H:%M:%S");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
const char* KTranslateSeconds(char* szBuffer, size_t iMaxBuf, int64_t iNumSeconds, bool bLongForm = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
const char* KTranslateSeconds(int64_t iNumSeconds, bool bLongForm = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
template <class Arithmetic, class String = KString>
String KFormNumber(Arithmetic i, typename String::value_type separator = ',', typename String::size_type every = 3)
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
template<class Container>
size_t KCountChar(const Container& container, const typename Container::value_type ch) noexcept
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
size_t KCountChar(const char* string, const char ch) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
template<class Char>
inline bool KIsDigit(Char ch) noexcept
//-----------------------------------------------------------------------------
{
	return (ch <= '9' && ch >= '0');
}

//-----------------------------------------------------------------------------
template<class Container>
bool KIsDecimal(const Container& container) noexcept
//-----------------------------------------------------------------------------
{
	size_t len = container.size();
	if (!len)
	{
		return false;
	}

	for (auto it = container.data(); len--;)
	{
		if (!KIsDigit(*it++))
		{
			if (it != container.data()+1 || (*(it-1) != '-' && *(it-1) != '+'))
			{
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
bool KIsDecimal(const char* buf, size_t size) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool KIsDecimal(const char* string) noexcept;
//-----------------------------------------------------------------------------

// exception free conversions

//-----------------------------------------------------------------------------
inline long double KToLongDouble(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtold(s, nullptr);
}

//-----------------------------------------------------------------------------
inline double KToDouble(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::atof(s);
}

//-----------------------------------------------------------------------------
inline float KToFloat(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	return static_cast<float>(KToDouble(s));
}

//-----------------------------------------------------------------------------
inline short int KToShort(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return static_cast<short int>(std::atoi(s));
}

//-----------------------------------------------------------------------------
inline unsigned short int KToUShort(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return static_cast<unsigned short int>(std::strtoul(s, nullptr, 10));
}

//-----------------------------------------------------------------------------
inline int KToInt(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::atoi(s);
}

//-----------------------------------------------------------------------------
inline unsigned int KToUInt(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return static_cast<unsigned int>(std::strtoul(s, nullptr, 10));
}

//-----------------------------------------------------------------------------
inline long KToLong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtol(s, nullptr, 10);
}

//-----------------------------------------------------------------------------
inline unsigned long KToULong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtoul(s, nullptr, 10);
}

//-----------------------------------------------------------------------------
inline long long KToLongLong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtoll(s, nullptr, 10);
}

//-----------------------------------------------------------------------------
inline unsigned long long KToULongLong(const char* s) noexcept
//-----------------------------------------------------------------------------
{
	if (!s || !*s) return 0;
	else return std::strtoull(s, nullptr, 10);
}

template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline float KToFloat(const String& s) noexcept { return KToFloat(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline double KToDouble(const String& s) noexcept { return KToDouble(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline long double KToLongDouble(const String& s) noexcept { return KToLongDouble(s.data); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline int KToShort(const String& s) noexcept { return KToShort(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline int KToInt(const String& s) noexcept { return KToInt(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline long KToLong(const String& s) noexcept { return KToLong(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline long long KToLongLong(const String& s) noexcept { return KToLongLong(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline unsigned int KToUShort(const String& s) noexcept { return KToUShort(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline unsigned int KToUInt(const String& s) noexcept { return KToUInt(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline unsigned long KToULong(const String& s) noexcept { return KToULong(s.data()); }
template<class String, typename = std::enable_if_t<dekaf2::is_narrow_cpp_str<String>::value> >
inline unsigned long long KToULongLong(const String& s) noexcept { return KToULongLong(s.data()); }

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
