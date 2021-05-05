/*
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
#include <cctype>
#include <map>
#include "kstring.h"
#include "kstringview.h"
#include "ksystem.h"
#include "kctype.h"
#include "bits/ktemplate.h"

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
template<class Container,
	typename std::enable_if<!detail::is_cpp_str<Container>::value, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool kStrIn (KStringView sNeedle, const Container& Haystack)
//----------------------------------------------------------------------
{
	return std::find(std::begin(Haystack), std::end(Haystack), sNeedle) != std::end(Haystack);
}

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
	auto iDelete = static_cast<typename String::size_type>(it - string.begin());
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
	return kTrimLeft(string, [](typename String::value_type ch){ return KASCII::kIsSpace(ch); });
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
	return kTrimRight(string, [](typename String::value_type ch){ return KASCII::kIsSpace(ch); });
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
	return kTrim(string, [](typename String::value_type ch){ return KASCII::kIsSpace(ch); });
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
		if (DEKAF2_UNLIKELY(svCollapse.contains(*it)))
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
		if (DEKAF2_UNLIKELY(svCollapse.contains(*it)))
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
			else
			{
				// nothing to replace, readjust start position
				iStart += (iEnd + sClose.size());
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
/// Check if buffer content is binary content, not text
bool kIsBinary(KStringView sBuffer);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get the English abbreviated weekday, input 0..6, 0 = Sun
KStringViewZ kGetAbbreviatedWeekday(uint16_t iDay);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Get the English abbreviated month, input 0..11, 0 = Jan
KStringViewZ kGetAbbreviatedMonth(uint16_t iMonth);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns day of week for every gregorian date. Sunday = 0.
uint16_t kDayOfWeek(uint16_t iDay, uint16_t iMonth, uint16_t iYear);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a time stamp following strftime patterns. If tTime is 0, current time is
/// used.
/// @param tTime Seconds since epoch. If 0, query current time from the system
/// @param pszFormat format string
/// @param bLocalTime use tTime as local time instead of utc
KString kFormTimestamp (time_t tTime = 0, const char* pszFormat = "%Y-%m-%d %H:%M:%S", bool bAsLocalTime = false);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a HTTP time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
KString kFormHTTPTimestamp (time_t tTime = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Create a SMTP time stamp
/// @param tTime Seconds since epoch. If 0, query current time from the system
KString kFormSMTPTimestamp (time_t tTime = 0);
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
#if defined(DEKAF2_HAS_CPP_17) && (DEKAF2_NO_GCC || DEKAF2_GCC_VERSION >= 80000)
		if constexpr (std::is_same<String, KString>::value)
		{
			result = KString::to_string(i);
		}
		else
		{
			result = std::to_string(i);
		}
#else
		result = std::to_string(i);
#endif
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
/// Returns true if str contains an integer, possibly with a leading + or -
bool kIsInteger(KStringView str, bool bSigned = true) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains an unsigned integer, possibly with a leading +
inline bool kIsUnsigned(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
	return kIsInteger(str, false);
}

//-----------------------------------------------------------------------------
/// Returns true if str contains a float, possibly with a leading + or -
bool kIsFloat(KStringView str, KStringView::value_type chDecimalSeparator = '.') noexcept;
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
Integer kToInt(const char* data, size_t size, bool bIsHex = false) noexcept
//-----------------------------------------------------------------------------
{
	Integer iVal{0};
	bool    bNeg{false};

	while (size && KASCII::kIsSpace(*data))
	{
		++data;
		--size;
	}

	if (size)
	{
		if (*data == '-')
		{
			bNeg = true;
			++data;
			--size;
		}
		else if (*data == '+')
		{
			++data;
			--size;
		}
	}

	if (!bIsHex)
	{
		for (; size > 0; --size)
		{
			auto ch = *data++;

			if (!KASCII::kIsDigit(ch))
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
First kFirstNonZero(First iFirst)
//-----------------------------------------------------------------------------
{
	return iFirst;
}

//-----------------------------------------------------------------------------
/// return the first in a sequence of objects that is not .empty()
template<class First, class...More, typename std::enable_if<sizeof...(More) != 0, int>::type = 0>
First kFirstNonZero(First iFirst, More&&...more)
//-----------------------------------------------------------------------------
{
	if (iFirst)
	{
		return iFirst;
	}
	return kFirstNonZero<First>(std::forward<More>(more)...);
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

//-----------------------------------------------------------------------------
template<class String = KString>
String kUnsignedToString(uint64_t i, uint16_t iBase = 10, bool bZeroPad = false, bool bUppercase = false, bool bIsNeg = false)
//-----------------------------------------------------------------------------
{
	static constexpr KStringView s_sLookupLower { "0123456789abcdefghijklmnopqrstuvwxyz" };
	static constexpr KStringView s_sLookupUpper { "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" };

	String sResult;

	if (iBase >=2 && iBase <= 36)
	{
		auto sLookup = (bUppercase) ? s_sLookupUpper : s_sLookupLower;

		do
		{
			sResult += sLookup[i % iBase];
		}
		while (i /= iBase);

		if (bZeroPad && (sResult.size() & 1) == 1)
		{
			sResult += '0';
		}

		if (bIsNeg)
		{
			sResult += '-';
		}

		// revert the string
		std::reverse(sResult.begin(), sResult.end());
	}

	return sResult;

} // detail::kUnsignedToString

//-----------------------------------------------------------------------------
template<class String = KString>
String kSignedToString(int64_t i, uint16_t iBase = 10, bool bZeroPad = false, bool bUppercase = false)
//-----------------------------------------------------------------------------
{
	bool bIsNeg { false };

	auto ui = static_cast<uint64_t>(i);

	if (i < 0)
	{
		bIsNeg = true;
		ui = (~ui + 1);
	}

	return kUnsignedToString<String>(ui, iBase, bZeroPad, bUppercase, bIsNeg);

} // kSignedToString

} // end of namespace dekaf2
