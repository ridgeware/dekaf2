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
#include <cstring>
#include <map>
#include "kstring.h"
#include "kstringview.h"
#include "ksystem.h"

namespace dekaf2
{

//----------------------------------------------------------------------
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim = ',');
//----------------------------------------------------------------------

//----------------------------------------------------------------------
inline bool kStrIn (KStringView sNeedle, KStringView sHaystack, char iDelim = ',')
//----------------------------------------------------------------------
{
	return sNeedle.In(sHaystack, iDelim);
}

//----------------------------------------------------------------------
inline bool kStrIn (const char* sNeedle, KStringView sHaystack, char iDelim = ',')
//----------------------------------------------------------------------
{
	return kStrIn(KStringView(sNeedle), sHaystack, iDelim);
}

//----------------------------------------------------------------------
inline bool kStrIn (KStringView sNeedle, const char* sHaystack, char iDelim = ',')
//----------------------------------------------------------------------
{
	return kStrIn(sNeedle, KStringView(sHaystack), iDelim);
}

//----------------------------------------------------------------------
bool kStrIn (KStringView sNeedle, const char* Haystack[]);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
template<class Container>
bool kStrIn (KStringView sNeedle, const Container& Haystack)
//----------------------------------------------------------------------
{
	return std::find(std::begin(Haystack), std::end(Haystack), sNeedle) != std::end(Haystack);
}

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
/// pads string at the left up to iWidth size with chPad
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
/// pads string at the right up to iWidth size with chPad
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
/// removes any character in svTrim from the left of the string
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
/// removes any character for which cmp returns true from the left of the string
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
/// removes white space from the left of the string
template<class String>
String& kTrimLeft(String& string)
//-----------------------------------------------------------------------------
{
	return kTrimLeft(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
/// removes any character in svTrim from the right of the string
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
/// removes any character for which cmp returns true from the right of the string
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
/// removes white space from the right of the string
template<class String>
String& kTrimRight(String& string)
//-----------------------------------------------------------------------------
{
	return kTrimRight(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
/// removes any character in svTrim from the left and right of the string
template<class String>
String& kTrim(String& string, KStringView svTrim)
//-----------------------------------------------------------------------------
{
	kTrimRight(string, svTrim);
	return kTrimLeft(string, svTrim);
}

//-----------------------------------------------------------------------------
/// removes any character for which cmp returns true from the left and right of the string
template<class String, class Compare>
String& kTrim(String& string, Compare cmp)
//-----------------------------------------------------------------------------
{
	kTrimRight(string, cmp);
	return kTrimLeft(string, cmp);
}

//-----------------------------------------------------------------------------
/// removes white space from the left and right of the string
template<class String>
String& kTrim(String& string)
//-----------------------------------------------------------------------------
{
	return kTrim(string, [](typename String::value_type ch){ return std::isspace(ch) != 0; });
}

//-----------------------------------------------------------------------------
/// Collapses consecutive chars in svCollapse to one instance of chTo
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
/// Collapses consecutive chars in svCollapse to one instance of chTo and trims the same chars left and right
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
/// Replace variables in sString found in a map Variables, search for sOpen/sClose for lead-in and lead-out.
/// Returns count of replaces
template<class String, class StringView, class ReplaceMap = std::map<StringView, StringView> >
std::size_t kReplaceVariables (String& sString, StringView sOpen, StringView sClose, bool bQueryEnvironment, const ReplaceMap& Variables = ReplaceMap{})
//-----------------------------------------------------------------------------
{
	std::size_t iNumReplacements { 0 };
	auto iOpen = sOpen.size();

	if (DEKAF2_UNLIKELY(!iOpen || !sClose.size()))
	{
		// we need prefix and suffix
		return 0;
	}

	for (typename String::size_type iPos, iStart = 0; (iPos = sString.find(sOpen, iStart)) != String::npos;)
	{
		StringView sHaystack ( sString.ToView(iPos + iOpen) );
		auto iEnd = sHaystack.find(sClose);
		if (iEnd != StringView::npos)
		{
			StringView sValue = sHaystack.ToView(0, iEnd);
			StringView sReplace;
			auto it = Variables.find(sValue);
			if (it != Variables.end())
			{
				sReplace = it->second;
			}
			else if (bQueryEnvironment)
			{
				// need to convert into string, as kGetEnv wants a zero terminated string
				KString strValue(sValue);
				sReplace = kGetEnv(strValue);
			}
			if (!sReplace.empty())
			{
				// found one - replace it
				sString.replace(iPos, sValue.size() + iOpen + sClose.size(), sReplace);
				// readjust start position
				iStart = iPos + sReplace.size();
				// increase replace counter
				++iNumReplacements;
			}
		}
		else
		{
			// no value found, prepare next loop
			iStart += iOpen;
		}
	}

	return (iNumReplacements);

} // kReplaceVariables

//-----------------------------------------------------------------------------
/// Create a UTC time stamp following strftime patterns. If tTime is 0, current time is
/// used.
KString kFormTimestamp (time_t tTime = 0, const char* pszFormat = "%Y-%m-%d %H:%M:%S");
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Form a string that expresses a duration
KString kTranslateSeconds(int64_t iNumSeconds, bool bLongForm = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Convert value into string and insert separator every n digits
template <class Arithmetic, class String = KString>
String kFormNumber(Arithmetic i, typename String::value_type separator = ',', typename String::size_type every = 3)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_arithmetic<Arithmetic>::value, "arithmetic type required");
	String result;

	DEKAF2_TRY
	{
		// TODO specialize on KString::to_string()..
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
/// Copy sInp and insert separator every n digits
KString kFormString(KStringView sInp, typename KString::value_type separator = ',', typename KString::size_type every = 3);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Count occurence of ch in container
template<class Container>
inline size_t kCountChar(const Container& container, const typename Container::value_type ch) noexcept
//-----------------------------------------------------------------------------
{
	return std::count(container.begin(), container.end(), ch);
}

//-----------------------------------------------------------------------------
/// Count occurence of ch in str (to catch char* as well)
inline size_t kCountChar(KStringView str, const char ch) noexcept
//-----------------------------------------------------------------------------
{
	return std::count(str.begin(), str.end(), ch);
}

//-----------------------------------------------------------------------------
/// Returns true if ch is a digit
template<class Char>
inline bool kIsDigit(Char ch) noexcept
//-----------------------------------------------------------------------------
{
	return std::isdigit(ch);
}

//-----------------------------------------------------------------------------
/// Returns true if str contains an integer, possibly with a leading + or -
bool kIsInteger(KStringView str) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains a syntactically valid email address
bool kIsEmail(KStringView str) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains a syntactically valid URL
bool kIsURL(KStringView str) noexcept;
//-----------------------------------------------------------------------------

// exception free conversions

//-----------------------------------------------------------------------------
/// Converts a hex digit into the corresponding integer value. Returns -1 if not
/// a valid digit
uint16_t kFromHexChar(char ch) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Converts a character array starting at data with length size into an integer.
/// If bIsHex is true input will be interpreted as hex string
template<class Integer>
int64_t kToInt(const char* data, size_t size, bool bIsHex = false) noexcept
//-----------------------------------------------------------------------------
{
	int64_t iVal{0};
	bool    bNeg{false};

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

//-----------------------------------------------------------------------------
template<class First>
First kFirstNonEmpty(First sFirst)
//-----------------------------------------------------------------------------
{
	return sFirst;
}

//-----------------------------------------------------------------------------
/// return the first in a sequence of objects that is not .empty()
template<class First, class...More, typename std::enable_if<sizeof...(More) != 0, int>::type = 0>
First kFirstNonEmpty(First sFirst, More&&...more)
//-----------------------------------------------------------------------------
{
	if (!sFirst.empty())
	{
		return sFirst;
	}
	return kFirstNonEmpty<First>(std::forward<More>(more)...);
}

} // end of namespace dekaf2
