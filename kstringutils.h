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

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KASCII - dekaf's char* helper functions, only safe for ASCII c-strings
class KASCII
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	/// test if strings are an exact match (case sensitive)
	static inline bool strmatch(const char* S1,const char* S2)   { return (strcmp(S1,S2) == 0);                 }

	/// test if strings are an exact match (case INsensitive)
	static inline bool strmatchI(const char* S1,const char* S2)  { return (strcasecmp(S1,S2) == 0);             }

	/// test to see if string S1 starts with S2 (case sensitive)
	static inline bool strmatchN(const char* S1,const char* S2)  { return (strncmp(S1,S2,strlen(S2)) == 0);     }

	/// test to see if string S1 starts with S2 (case INsensitive)
	static inline bool strmatchNI(const char* S1,const char* S2) { return (strncasecmp(S1,S2,strlen(S2)) == 0); }

	/// trims whitespace from left of given string by moving the char* pointer (no change to original string buffer)
	static char* ktrimleft (const char* str);

	/// modifies the given string by writing zeros into whitespace at the end, returns pointer to string for convenience
	static char* ktrimright (char* str);

	/// find a needle in a haystack with haystack delimited by commas or whatever delimeter is given.
	static bool kstrin (const char* sNeedle, const char* sHayStack, int chDelim=',')
	{
		return (kStrIn (sNeedle, sHayStack, chDelim));
	}

	/// make A-Z to lower case by modifying the original string.  skips over any characters that are not A-Z.
	static char* ktolower (char* str);

	/// make a-z to upper case by modifying the original string.  skips over any characters that are not a-z.
	static char* ktoupper (char* str);

}; // KASCII

/// kstrncpy() ensures that the new string is null terminated
///  whereas the original strncpy() does not assure this (see
///  man page entry below).  Note however that the strlen() of
///  target will be AT MOST iMaxAllocTarget-1, since room for
///  the NIL terminator must be allowed.
char* kstrncpy (char* szTarget, const char* szSource, size_t iMaxAllocTarget);

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
template<class String>
String& kTrimLeft(String& string, KStringView svTrim)
//-----------------------------------------------------------------------------
{
	auto iDelete = string.find_first_not_of(svTrim);
	if (iDelete)
	{
		string.erase(static_cast<typename String::size_type>(0), iDelete);
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
		string.erase(static_cast<typename String::size_type>(0), iDelete);
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
template<class String>
String& kTrimRight(String& string, KStringView svTrim)
//-----------------------------------------------------------------------------
{
	auto iDelete = string.find_last_not_of(svTrim);
	if (iDelete != String::npos)
	{
		string.erase(iDelete + 1);
	}
	return string;
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
		string.erase(string.size() - iDelete);
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
template<class String>
String& kTrim(String& string, KStringView svTrim)
//-----------------------------------------------------------------------------
{
	kTrimRight(string, svTrim);
	return kTrimLeft(string, svTrim);
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
template<class String>
String& kCollapse(String& string, KStringView svCollapse, typename String::value_type chTo)
//-----------------------------------------------------------------------------
{
	auto it = string.begin();
	auto ins = it;
	auto ie = string.end();
	bool bLastWasFound { false };

	for (;it != ie; ++it)
	{
		if (DEKAF2_UNLIKELY(svCollapse.Contains(*it)))
		{
			bLastWasFound = true;
		}
		else
		{
			if (DEKAF2_UNLIKELY(bLastWasFound))
			{
				*ins++ = chTo;
				bLastWasFound = false;
			}

			*ins++ = std::move(*it);
		}
	}

	if (bLastWasFound)
	{
		*ins++ = chTo;
	}

	string.erase(ins, ie);

	return string;
}

//-----------------------------------------------------------------------------
template<class String>
String& kCollapseAndTrim(String& string, KStringView svCollapse, typename String::value_type chTo)
//-----------------------------------------------------------------------------
{
	auto it = string.begin();
	auto ins = it;
	auto ie = string.end();
	bool bLastWasFound { false };

	for (;it != ie; ++it)
	{
		if (DEKAF2_UNLIKELY(svCollapse.Contains(*it)))
		{
			if (ins != string.begin())
			{
				// we already had non-trim chars
				bLastWasFound = true;
			}
		}
		else
		{
			if (DEKAF2_UNLIKELY(bLastWasFound))
			{
				*ins++ = chTo;
				bLastWasFound = false;
			}

			*ins++ = std::move(*it);
		}
	}

	string.erase(ins, ie);

	return string;
}

//-----------------------------------------------------------------------------
KString kFormTimestamp (time_t tTime = 0, const char* pszFormat = "%Y-%m-%d %H:%M:%S");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KString kTranslateSeconds(int64_t iNumSeconds, bool bLongForm = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
template <class Arithmetic, class String = KString>
String kFormNumber(Arithmetic i, typename String::value_type separator = ',', typename String::size_type every = 3)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_arithmetic<Arithmetic>::value, "arithmetic type required");
	String result;

	DEKAF2_TRY
	{
		result = std::to_string(i);
	}
	DEKAF2_CATCH (...)
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
KString kFormString(KStringView sInp, typename KString::value_type separator = ',', typename KString::size_type every = 3);
//-----------------------------------------------------------------------------

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
/// Converts a hex digit into the corresponding integer value. Returns -1 if not
/// a valid digit
uint16_t kFromHexChar(char ch);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
template<class Integer>
Integer kToInt(const char* data, size_t size, bool bIsHex = false) noexcept
//-----------------------------------------------------------------------------
{
	Integer iVal{0};
	bool bNeg{false};

	while (size && std::isspace(*data))
	{
		++data;
		--size;
	}

	if (size && *data == '-')
	{
		bNeg = true;
		++data;
		--size;
	}

	if (!bIsHex)
	{
		for (; size > 0; --size)
		{
			auto ch = *data++;

			if (!std::isdigit(ch))
			{
				break;
			}

			iVal *= 10;
			iVal += ch - '0';
		}
	}
	else
	{
		for (; size > 0; --size)
		{
			auto iCh = kFromHexChar(*data++);
			if (iCh > 15)
			{
				break;
			}

			iVal *= 16;
			iVal += iCh;
		}

	}

	if (bNeg)
	{
		iVal *= -1;
	}

	return iVal;
}

} // end of namespace dekaf2
