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

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "kctype.h"
#include "kutf8.h"
#include "ktemplate.h"
#include <cinttypes>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <map>
#include <array>

DEKAF2_NAMESPACE_BEGIN

//------------------------------------------------------------------------------
DEKAF2_PUBLIC
KStringRef::size_type kReplace(KStringRef& string,
							   const KStringView sSearch,
							   const KStringView sReplaceWith,
							   KStringRef::size_type pos = 0,
							   bool bReplaceAll = true);
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
DEKAF2_PUBLIC
KStringRef::size_type kReplace(KStringRef& string,
							   const KStringRef::value_type chSearch,
							   const KStringRef::value_type chReplaceWith,
							   KStringRef::size_type pos = 0,
							   bool bReplaceAll = true);
//------------------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase (UTF8)
DEKAF2_PUBLIC
KString kToUpper(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase (UTF8)
DEKAF2_PUBLIC
KString kToLower(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
DEKAF2_PUBLIC
KString kToUpperLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
DEKAF2_PUBLIC
KString kToLowerLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase assuming ASCII encoding
DEKAF2_PUBLIC
KString kToUpperASCII(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase assuming ASCII encoding
DEKAF2_PUBLIC
KString kToLowerASCII(KStringView sInput);
//----------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// returns leftmost iCount chars of string
template<class String, class StringView = KStringView>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
StringView kLeft(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return StringView(sInput.data(), std::min(iCount, sInput.size()));
}

//-----------------------------------------------------------------------------
/// reduces string to leftmost iCount chars of string
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
String& kMakeLeft(String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	sInput.erase(std::min(iCount, sInput.size()));
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns substring starting at iStart for remaining chars
template<class String, class StringView = KStringView>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
StringView kMid(const String& sInput, std::size_t iStart)
//-----------------------------------------------------------------------------
{
	if (iStart > sInput.size()) iStart = sInput.size();
	return StringView(&sInput[iStart], sInput.size() - iStart);
}

//-----------------------------------------------------------------------------
/// returns substring starting at iStart for remaining chars
template<class String, class StringView = KStringView>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
StringView kMid(const String& sInput, std::size_t iStart, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (iStart > sInput.size()) iStart = sInput.size();
	if (iCount > sInput.size() - iStart) iCount = sInput.size() - iStart;
	return StringView(&sInput[iStart], iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to starting at iStart
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
String& kMakeMid(String& sInput, std::size_t iStart)
//-----------------------------------------------------------------------------
{
	if (iStart > sInput.size()) iStart = sInput.size();
	sInput.erase(0, iStart);
	return sInput;
}

//-----------------------------------------------------------------------------
/// reduces string to starting at iStart with iCount chars
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
String& kMakeMid(String& sInput, std::size_t iStart, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (iStart > sInput.size()) iStart = sInput.size();
	if (iCount > sInput.size() - iStart) iCount = sInput.size() - iStart;
	sInput.erase(iStart + iCount);
	sInput.erase(0, iStart);
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns rightmost iCount chars of string
template<class String, class StringView = KStringView>
DEKAF2_PUBLIC
StringView kRight(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return (iCount > sInput.size())
		? StringView(sInput)
		: StringView(sInput.data() + sInput.size() - iCount, iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to rightmost iCount chars of string
template<class String>
DEKAF2_PUBLIC
String& kMakeRight(String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (iCount < sInput.size()) sInput.erase(0, sInput.size() - iCount);
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns leftmost iCount codepoints of string
template<class String, class StringView = KStringView>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
StringView kLeftUTF8(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return Unicode::LeftUTF8<String, StringView>(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to leftmost iCount codepoints of string
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
String& kMakeLeftUTF8(String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto it = Unicode::LeftUTF8(sInput.begin(), sInput.end(), iCount);
	sInput.erase(it, sInput.end());
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns substring starting at codepoint iStart for iCount codepoints
template<class String, class StringView = KStringView>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
StringView kMidUTF8(const String& sInput, std::size_t iStart, std::size_t iCount = npos)
//-----------------------------------------------------------------------------
{
	return Unicode::MidUTF8<String, StringView>(sInput, iStart, iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to starting at codepoint iStart with iCount codepoints
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
String& kMakeMidUTF8(String& sInput, std::size_t iStart, std::size_t iCount = npos)
//-----------------------------------------------------------------------------
{
	auto it = Unicode::LeftUTF8(sInput.begin(), sInput.end(), iStart);
	sInput.erase(sInput.begin(), it);
	return kMakeLeftUTF8(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// returns rightmost iCount UTF8 codepoints of string
template<class String, class StringView = KStringView>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
StringView kRightUTF8(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return Unicode::RightUTF8<String, StringView>(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to rightmost iCount UTF8 codepoints of string
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
String& kMakeRightUTF8(String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto it = Unicode::RightUTF8(sInput.begin(), sInput.end(), iCount);
	sInput.erase(sInput.begin(), it);
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns KCcodePoint at UTF8 position iCount
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
KCodePoint kAtUTF8(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return Unicode::AtUTF8(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// returns true if string contains UTF8 runs
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
bool kHasUTF8(const String& sInput)
//-----------------------------------------------------------------------------
{
	return Unicode::HasUTF8(sInput);
}

//-----------------------------------------------------------------------------
/// returns the count of unicode codepoints (or, UTF8 sequences)
template<class String>
DEKAF2_CONSTEXPR_14
DEKAF2_PUBLIC
std::size_t kSizeUTF8(const String& sInput)
//-----------------------------------------------------------------------------
{
	return Unicode::CountUTF8(sInput);
}

//----------------------------------------------------------------------
DEKAF2_PUBLIC
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim = ',');
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_PUBLIC
bool kStrIn (KStringView sNeedle, KStringView sHaystack, char iDelim = ',');
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_PUBLIC
inline bool kStrIn (const char* sNeedle, KStringView sHaystack, char iDelim = ',')
//----------------------------------------------------------------------
{
	return kStrIn(KStringView(sNeedle), sHaystack, iDelim);
}

//----------------------------------------------------------------------
DEKAF2_PUBLIC
inline bool kStrIn (KStringView sNeedle, const char* sHaystack, char iDelim = ',')
//----------------------------------------------------------------------
{
	return kStrIn(sNeedle, KStringView(sHaystack), iDelim);
}

//----------------------------------------------------------------------
DEKAF2_PUBLIC
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
template<class String, class StringView,
         typename std::enable_if<detail::is_str<StringView>::value, int>::type = 0>
String& kTrimLeft(String& string, const StringView& svTrim)
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
template<class String, class Compare,
         typename std::enable_if<!detail::is_str<Compare>::value, int>::type = 0>
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
/// removes any white space character from the left of the string
template<class String>
String& kTrimLeft(String& string)
//-----------------------------------------------------------------------------
{
	return kTrimLeft(string, [](typename String::value_type ch)
	{
		return KASCII::kIsSpace(ch);
	});
}

//-----------------------------------------------------------------------------
/// removes any character in svTrim from the right of the string
template<class String, class StringView,
         typename std::enable_if<detail::is_str<StringView>::value, int>::type = 0>
String& kTrimRight(String& string, const StringView& svTrim)
//-----------------------------------------------------------------------------
{
	auto iDelete = string.find_last_not_of(svTrim);
	if (iDelete == String::npos)
	{
		string.clear();
	}
	else
	{
		string.erase(iDelete + 1);
	}
	return string;
}

//-----------------------------------------------------------------------------
/// removes any character for which cmp returns true from the right of the string
template<class String, class Compare,
         typename std::enable_if<!detail::is_str<Compare>::value, int>::type = 0>
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
/// removes any white space character from the right of the string
template<class String>
String& kTrimRight(String& string)
//-----------------------------------------------------------------------------
{
	return kTrimRight(string, [](typename String::value_type ch)
	{
		return KASCII::kIsSpace(ch);
	});
}

//-----------------------------------------------------------------------------
/// removes any white space character from the left and right of the string
template<class String>
String& kTrim(String& string)
//-----------------------------------------------------------------------------
{
	kTrimRight(string);
	return kTrimLeft(string);
}

//-----------------------------------------------------------------------------
/// removes any character in svTrim from the left and right of the string
template<class String, class StringView,
         typename std::enable_if<detail::is_str<StringView>::value, int>::type = 0>
String& kTrim(String& string, const StringView& svTrim)
//-----------------------------------------------------------------------------
{
	kTrimRight(string, svTrim);
	return kTrimLeft(string, svTrim);
}

//-----------------------------------------------------------------------------
/// removes any character for which cmp returns true from the left and right of the string
template<class String, class Compare,
         typename std::enable_if<!detail::is_str<Compare>::value, int>::type = 0>
String& kTrim(String& string, Compare cmp)
//-----------------------------------------------------------------------------
{
	kTrimRight(string, cmp);
	return kTrimLeft(string, cmp);
}

//-----------------------------------------------------------------------------
/// Collapses consecutive characters for which cmp returns true to one instance of chTo
template<class String, class Compare>
String& kCollapse(String& string, typename String::value_type chTo, Compare cmp)
//-----------------------------------------------------------------------------
{
	auto it = string.begin();
	auto ins = it;
	auto ie = string.end();
	bool bLastWasFound { false };

	for (;it != ie; ++it)
	{
		if (cmp(*it))
		{
			bLastWasFound = true;
		}
		else
		{
			if (bLastWasFound)
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
/// Collapses consecutive white space chars to one space char
template<class String>
String& kCollapse(String& string)
//-----------------------------------------------------------------------------
{
	return kCollapse(string, ' ', [](typename String::value_type ch)
	{
		return KASCII::kIsSpace(ch);
	});
}

//-----------------------------------------------------------------------------
/// Collapses consecutive chars in svCollapse to one instance of chTo
template<class String, class StringView>
String& kCollapse(String& string, const StringView& svCollapse, typename String::value_type chTo)
//-----------------------------------------------------------------------------
{
	return kCollapse(string, chTo, [&svCollapse](typename String::value_type ch)
	{
		return svCollapse.find(ch) != StringView::npos;
	});
}

//-----------------------------------------------------------------------------
/// Collapses consecutive characters for which cmp returns true to one instance of chTo and trims those characters left and right
template<class String, class Compare>
String& kCollapseAndTrim(String& string, typename String::value_type chTo, Compare cmp)
//-----------------------------------------------------------------------------
{
	auto it = string.begin();
	auto ins = it;
	auto ie = string.end();
	bool bLastWasFound { false };

	for (;it != ie; ++it)
	{
		if (cmp(*it))
		{
			if (ins != string.begin())
			{
				// we already had non-trim chars
				bLastWasFound = true;
			}
		}
		else
		{
			if (bLastWasFound)
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
template<class String>
String& kCollapseAndTrim(String& string)
//-----------------------------------------------------------------------------
{
	return kCollapseAndTrim(string, ' ', [](typename String::value_type ch)
	{
		return KASCII::kIsSpace(ch);
	});
}

//-----------------------------------------------------------------------------
/// Collapses consecutive chars in svCollapse to one instance of chTo and trims the same chars left and right
template<class String, class StringView>
String& kCollapseAndTrim(String& string, const StringView& svCollapse, typename String::value_type chTo)
//-----------------------------------------------------------------------------
{
	return kCollapseAndTrim(string, chTo, [&svCollapse](typename String::value_type ch)
	{
		return svCollapse.find(ch) != StringView::npos;
	});
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
		StringView sHaystack(sString);
		sHaystack.remove_prefix(iPos + iOpen);
		auto iEnd = sHaystack.find(sClose);
		if (iEnd != StringView::npos)
		{
			StringView sValue = sHaystack.substr(0, iEnd);
			StringView sReplace;
			auto it = Variables.find(sValue);
			if (it != Variables.end())
			{
				sReplace = it->second;
			}
			else if (bQueryEnvironment)
			{
				// need to convert into string, as kGetEnv wants a zero terminated string
				String strValue(sValue);
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
DEKAF2_PUBLIC
bool kIsBinary(KStringView sBuffer);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Convert value into string and insert selectable separator every n digits, with requested precision
/// @param i the arithmetic value to format
/// @param chSeparator the "thousands" separator char, defaults to ','
/// @param iEvery insert the chSeparator every iEvery chars
/// @param iPrecision decimal precision (also for non-floating point values), defaults to 0
/// @param bRoundToNearest true if the output shall be rounded to nearest value, defaults to true
/// @return the formatted number as a String
template <class Arithmetic, class String = KString,
		  typename std::enable_if<!detail::is_duration<Arithmetic>::value, int>::type = 0>
String kFormNumber(Arithmetic i, typename String::value_type chSeparator = ',', uint16_t iEvery = 3, uint16_t iPrecision = 0, bool bRoundToNearest = true)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_arithmetic<Arithmetic>::value, "arithmetic type required");

	String sResult;

	DEKAF2_TRY
	{
#if defined(DEKAF2_HAS_FULL_CPP_17) && !DEKAF2_KSTRING_IS_STD_STRING
		if constexpr (std::is_same<String, KString>::value)
		{
			sResult = KString::to_string(i);
		}
		else
#endif
		{
			sResult = std::to_string(i);
		}
	}
	DEKAF2_CATCH (...)
	{
		// that conversion failed..
		sResult.clear();
	}

	typename String::size_type iDecSepPos;

	if (std::is_floating_point<Arithmetic>::value)
	{
		iDecSepPos       = sResult.rfind('.');
		auto iResultSize = sResult.size();

		if (iDecSepPos == String::npos)
		{
			iDecSepPos = sResult.rfind(',');

			if (iDecSepPos == String::npos)
			{
				// there is no decimal separator - add  a character,
				// the value itself is not important as we replace it below
				sResult += ',';
				iDecSepPos = iResultSize++;
			}
		}

		sResult[iDecSepPos] = (chSeparator != '.') ? '.' : ',';

		auto iHave = iResultSize - iDecSepPos - 1;

		if (iHave > iPrecision)
		{
			auto iCutAt = iDecSepPos + iPrecision + ((iPrecision > 0) ? 1 : 0);

			// check for rounding
			if (bRoundToNearest)
			{
				auto iPos = iDecSepPos + iPrecision + 1;

				for(;iPos < iResultSize;)
				{
					auto ch = sResult[iPos];

					if (ch >= '5')
					{
						// round..
						break;
					}
					else if (ch == '4')
					{
						// check more digits
						if (++iPos == iResultSize)
						{
							bRoundToNearest = false;
							break;
						}
					}
					else
					{
						// do not round..
						bRoundToNearest = false;
						break;
					}
				}

				if (bRoundToNearest)
				{
					for (auto iCurPos = iCutAt - 1; iCurPos >= 0;)
					{
						if (sResult[iCurPos] < '9')
						{
							if (sResult[iCurPos] == '-')
							{
								sResult.insert(iCurPos + 1, 1, '1');
								++iDecSepPos;
								++iCutAt;
							}
							else
							{
								sResult[iCurPos] = sResult[iCurPos] + 1;
							}
							break;
						}
						else
						{
							sResult[iCurPos] = '0';

							if (iCurPos == 0)
							{
								sResult.insert(0, 1, '1');
								++iDecSepPos;
								++iCutAt;
								break;
							}

							if (--iCurPos == iDecSepPos)
							{
								--iCurPos;
							}
						}
					}
				}
			}

			sResult.erase(iCutAt);
		}
		else
		{
			auto iCount = iPrecision;

			while (iCount-- > iHave)
			{
				sResult += '0';
			}
		}
	}
	else
	{
		// not a float/double
		iDecSepPos = sResult.size();

		if (iPrecision > 0)
		{
			sResult += (chSeparator != '.') ? '.' : ',';

			auto iCount = iPrecision;

			while (iCount--)
			{
				sResult += '0';
			}
		}
	}

	if (iEvery > 0)
	{
		// now insert the separator every N digits
		auto iLast = iEvery;

		if (i < 0)
		{
			// do not count the leading '-' as an insertion point
			++iLast;
		}

		auto iPos = iDecSepPos;

		while (iPos > iLast)
		{
			sResult.insert(iPos - iEvery, 1, chSeparator);
			iPos -= iEvery;
			++iDecSepPos;
		}

		if (iEvery < iPrecision)
		{
			iPos = iDecSepPos + 1 + iEvery;

			while (iPos < sResult.size())
			{
				sResult.insert(iPos, 1, chSeparator);
				iPos += iEvery + 1;
			}
		}
	}

	return sResult;

} // kFormNumber

//-----------------------------------------------------------------------------
/// Convert value into string and insert separator every n digits
template <class Duration, class String = KString,
typename std::enable_if<detail::is_duration<Duration>::value, int>::type = 0>
String kFormNumber(Duration duration, typename String::value_type chSeparator = ',', uint16_t iEvery = 3, uint16_t iPrecision = 0)
//-----------------------------------------------------------------------------
{
	return kFormNumber(duration.count(), chSeparator, iEvery, iPrecision);
}

//-----------------------------------------------------------------------------
/// Copy sInp and insert separator every n digits
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
inline size_t kCountChar(KStringView str, const char ch) noexcept
//-----------------------------------------------------------------------------
{
	return std::count(str.begin(), str.end(), ch);
}

//-----------------------------------------------------------------------------
/// Returns true if str contains an integer, possibly with a leading + or -
DEKAF2_PUBLIC
bool kIsInteger(KStringView str, bool bSigned = true) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains an unsigned integer, possibly with a leading +
DEKAF2_PUBLIC
inline bool kIsUnsigned(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
	return kIsInteger(str, false);
}

//-----------------------------------------------------------------------------
/// Returns true if str contains a float, possibly with a leading + or -
DEKAF2_PUBLIC
bool kIsFloat(KStringView str, KStringView::value_type chDecimalSeparator = '.') noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains a syntactically valid email address. This function always returns false if
/// KRegex is not available.
DEKAF2_PUBLIC
bool kIsEmail(KStringView str) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains a syntactically valid URL. This function always returns false if
/// KURL is not available.
DEKAF2_PUBLIC
bool kIsURL(KStringView str) noexcept;
//-----------------------------------------------------------------------------

// exception free conversions

namespace detail {

extern const uint8_t LookupBase36[256];

}

//-----------------------------------------------------------------------------
/// Converts any base36 digit into the corresponding integer value. Returns 0xFF if not
/// a valid digit
inline DEKAF2_PUBLIC
uint8_t kFromBase36(char ch) noexcept
//-----------------------------------------------------------------------------
{
	return detail::LookupBase36[static_cast<unsigned char>(ch)];
}

//-----------------------------------------------------------------------------
/// Converts a hex digit into the corresponding integer value. Returns > 15 if not
/// a valid digit
inline DEKAF2_PUBLIC
uint8_t kFromHexChar(char ch) noexcept
//-----------------------------------------------------------------------------
{
	return kFromBase36(ch);
}

//-----------------------------------------------------------------------------
template<class Integer, class Iterator,
		 typename std::enable_if<std::is_arithmetic<Integer>::value, int>::type = 0>
Integer kToInt(Iterator it, Iterator end, uint16_t iBase = 10) noexcept
//-----------------------------------------------------------------------------
{
	Integer iVal { 0 };

	if (DEKAF2_LIKELY(iBase <= 36))
	{
		// work on numbers expressed by ASCII alnum - accept negative values
		// by a '-' prefix, or positive values by a '+' prefix or none

		for (;it != end && KASCII::kIsSpace(*it); ++it) {}

		if (it != end)
		{
			bool bNeg { false };

			switch (*it)
			{
				case '-':
					bNeg = true;
					DEKAF2_FALLTHROUGH;
				case '+':
					++it;
					break;
			}

			for (;it != end;)
			{
				auto ch = static_cast<char>(*it++);

				auto iBase36 = kFromBase36(ch);

				if (iBase36 >= iBase)
				{
					// just in case this view was converted from a char* array with a max size
					// but which ended early on a null byte (or on a space), then abort without
					// error

					if (ch != 0 && !KASCII::kIsSpace(ch))
					{
						// this is a value overflow (the number was not encoded in this base)
//						kDebug(2, "string value is at least of base {}, not of {}", iBase36 - 1, iBase);
					}
					break;
				}

				iVal *= iBase;
				iVal += iBase36;
			}

			if (bNeg)
			{
				iVal *= -1;
			}
		}
	}
	else if (DEKAF2_UNLIKELY(iBase == 256))
	{
		// this is a pure binary encoding, do not trim anything, do not assume
		// signed values from prefixes

		for (;it != end;)
		{
			iVal *= 256;
			iVal += static_cast<unsigned char>(*it++);
		}
	}
	else
	{
//		kDebug(1, "invalid integer base of {}, must be in range 2..36,256", iBase);
	}

	return iVal;

} // kToInt

//-----------------------------------------------------------------------------
template<class Integer, class String,
         typename std::enable_if<std::is_arithmetic<Integer>::value &&
                                 detail::is_cpp_str<String>::value, int>::type = 0>
Integer kToInt(const String& sNumber, uint16_t iBase = 10) noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<Integer>(sNumber.begin(), sNumber.end(), iBase);
}

//-----------------------------------------------------------------------------
template<class Integer, class String,
		 typename std::enable_if<std::is_arithmetic<Integer>::value &&
								 detail::is_narrow_c_str<String>::value, int>::type = 0>
Integer kToInt(const String sNumber, uint16_t iBase = 10) noexcept
//-----------------------------------------------------------------------------
{
	return DEKAF2_LIKELY(sNumber != nullptr) ? kToInt<Integer>(sNumber, sNumber + std::strlen(sNumber), iBase) : 0;
}

//-----------------------------------------------------------------------------
template<class Integer, class String,
		 typename std::enable_if<std::is_arithmetic<Integer>::value &&
								 detail::is_wide_c_str<String>::value, int>::type = 0>
Integer kToInt(const String sNumber, uint16_t iBase = 10) noexcept
//-----------------------------------------------------------------------------
{
	return DEKAF2_LIKELY(sNumber != nullptr) ? kToInt<Integer>(sNumber, sNumber + std::wcslen(sNumber), iBase) : 0;
}

//-----------------------------------------------------------------------------
template<class First>
First kFirstNonZero(First iFirst)
//-----------------------------------------------------------------------------
{
	return iFirst;
}

//-----------------------------------------------------------------------------
/// return the first in a sequence of objects that is not zero (or false)
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

	if (iBase >= 2 && iBase <= 36)
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

} // kUnsignedToString

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

//-----------------------------------------------------------------------------
template<typename T, class String = KString,
         typename std::enable_if<std::is_unsigned<T>::value && std::is_arithmetic<T>::value, int>::type = 0>
String kIntToString(T i, uint16_t iBase = 10, bool bZeroPad = false, bool bUppercase = false)
//-----------------------------------------------------------------------------
{
	return kUnsignedToString(i, iBase, bZeroPad, bUppercase);
}

//-----------------------------------------------------------------------------
template<typename T, class String = KString,
		 typename std::enable_if<std::is_signed<T>::value && std::is_arithmetic<T>::value, int>::type = 0>
String kIntToString(T i, uint16_t iBase = 10, bool bZeroPad = false, bool bUppercase = false)
//-----------------------------------------------------------------------------
{
	return kSignedToString(i, iBase, bZeroPad, bUppercase);
}

//-----------------------------------------------------------------------------
inline
void kFromString(float& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_KSTRING_IS_STD_STRING
	KString sTmp(sValue);
	Value = std::strtof(sTmp.c_str(), nullptr);
#else
	Value = sValue.Float();
#endif
}

//-----------------------------------------------------------------------------
inline
void kFromString(double& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_KSTRING_IS_STD_STRING
	KString sTmp(sValue);
	Value = std::strtod(sTmp.c_str(), nullptr);
#else
	Value = sValue.Double();
#endif
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<std::is_integral<T>::value == false &&
								 std::is_enum    <T>::value == false &&
                                 std::is_assignable<T, KStringView>::value == true, int>::type = 0>
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value = sValue;
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<detail::has_Parse<T>::value == true, int>::type = 0>
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value.Parse(sValue);
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<std::is_integral<T>::value == true, int>::type = 0>
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value = kToInt<T>(sValue, iBase);
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<std::is_enum<T>::value == true, int>::type = 0>
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value = static_cast<T>(kToInt<int64_t>(sValue, iBase));
}

//-----------------------------------------------------------------------------
template<typename T>
T kFromString(KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	T Value;
	kFromString(Value, sValue, iBase);
	return Value;
}

//-----------------------------------------------------------------------------
/// Escape or hex encode problematic characters, append to sLog
DEKAF2_PUBLIC
void kEscapeForLogging(KStringRef& sLog, KStringView sInput);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Escape or hex encode problematic characters
DEKAF2_PUBLIC
inline KString kEscapeForLogging(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString sLog;
	kEscapeForLogging(sLog, sInput);
	return sLog;

} // kEscapeForLogging

//-----------------------------------------------------------------------------
/// returns true if iterator at_iter points to start of word in iterator start_iter (start_iter is either lower or equal with at_iter)
template<class Iterator,
		 typename std::enable_if<detail::is_cpp_str<Iterator>::value == false, int>::type = 0>
bool kIsAtStartofWordASCII(Iterator start_iter, Iterator at_iter)
//-----------------------------------------------------------------------------
{
	return (start_iter == at_iter || (start_iter < at_iter && KASCII::kIsAlNum(*(at_iter-1)) == false));

} // kIsAtStartofWordASCII

//-----------------------------------------------------------------------------
/// returns true if iterator at_iter points to end of word in iterator end_iter (end_iter is either bigger or equal with at_iter)
template<class Iterator,
		 typename std::enable_if<detail::is_cpp_str<Iterator>::value == false, int>::type = 0>
bool kIsAtEndofWordASCII(Iterator end_iter, Iterator at_iter)
//-----------------------------------------------------------------------------
{
	return (at_iter >= end_iter || KASCII::kIsAlNum(*at_iter) == false);

} // kIsAtEndofWordASCII

//-----------------------------------------------------------------------------
/// returns true if pos iPos points to start of word in string sHaystack
template<class String,
		 typename std::enable_if<detail::is_cpp_str<String>::value == true, int>::type = 0>
bool kIsAtStartofWordASCII(const String& sHaystack, typename String::size_type iPos)
//-----------------------------------------------------------------------------
{
	return iPos != String::npos && kIsAtStartofWordASCII(sHaystack.begin(), sHaystack.begin() + iPos);

} // kIsAtStartofWordASCII

//-----------------------------------------------------------------------------
/// returns true if pos iPos points to end of word in string sHaystack
template<class String,
		 typename std::enable_if<detail::is_cpp_str<String>::value == true, int>::type = 0>
bool kIsAtEndofWordASCII(const String& sHaystack, typename String::size_type iPos)
//-----------------------------------------------------------------------------
{
	return iPos != String::npos && kIsAtEndofWordASCII(sHaystack.end(), sHaystack.begin() + iPos);

} // kIsAtEndofWordASCII

namespace detail {

//-----------------------------------------------------------------------------
template<class String = KString, class StringView = KStringView>
std::pair<typename String::size_type, typename String::size_type>
	CalcSizeLimit(const String& sLimitMe,
				  typename String::size_type iMaxSize,
				  StringView& sEllipsis)
//-----------------------------------------------------------------------------
{
	if (iMaxSize <= sEllipsis.size() + 2)
	{
		// cannot insert ellipsis
		sEllipsis = StringView();
	}
	// remove the excess data in the _middle_ of the string
	auto iSize   = sLimitMe.size();
	auto iRemove = iSize - iMaxSize + sEllipsis.size();
	auto iStart  = iSize / 2 - iRemove / 2;

	return std::make_pair(iStart, iRemove);

} // CalcSizeLimit

//-----------------------------------------------------------------------------
template<class String = KString, class StringView = KStringView>
std::pair<typename String::size_type, typename String::size_type>
	CalcSizeLimitUTF8(const String& sLimitMe,
					  typename String::size_type iMaxSize,
					  StringView& sEllipsis)
//-----------------------------------------------------------------------------
{
	if (iMaxSize <= sEllipsis.size() + 2)
	{
		// cannot insert ellipsis
		sEllipsis = StringView();
	}
	// remove the excess data in the _middle_ of the string
	auto iSize   = sLimitMe.size();
	auto iRemove = iSize - iMaxSize + sEllipsis.size();
	auto iStart  = iSize / 2 - iRemove / 2;

	if (sizeof(typename String::value_type) == 1)
	{
		// make sure we use an unsigned char type!
		using uchar_t = typename std::make_unsigned<typename String::value_type>::type;

		// find start and end such that no UTF8 code run will be interrupted
		auto ch = static_cast<uchar_t>(sLimitMe[iStart]);

		while (iStart && Unicode::IsContinuationByte(ch))
		{
			ch = static_cast<uchar_t>(sLimitMe[--iStart]);
		}

		ch = static_cast<uchar_t>(sLimitMe[iStart + iRemove]);

		while (iStart + iRemove < iSize && Unicode::IsContinuationByte(ch))
		{
			ch = static_cast<uchar_t>(sLimitMe[iStart + ++iRemove]);
		}
	}
	else if (sizeof(typename String::value_type) == 2)
	{
		// make sure we use an unsigned char type!
		using uchar_t = typename std::make_unsigned<typename String::value_type>::type;

		// do not cut surrogate pairs..
		auto ch = static_cast<uchar_t>(sLimitMe[iStart]);

		if (iStart && Unicode::IsTrailSurrogate(ch))
		{
			--iStart;
		}

		ch = static_cast<uchar_t>(sLimitMe[iStart + iRemove]);

		if (iStart + iRemove < iSize && Unicode::IsLeadSurrogate(ch))
		{
			++iRemove;
		}
	}

	return std::make_pair(iStart, iRemove);

} // CalcSizeLimitUTF8

} // namespace detail

//-----------------------------------------------------------------------------
/// Limit the size of a string by removing data in the middle of it, and inserting an ellipsis. This function is
/// not UTF8 aware and should only be used when the content of the string has only ASCII or one-byte
/// codepage data. For UTF8 strings, use kLimitSizeUTF8()
/// @param sLimitMe the string to limit in size, as a reference (input/output)
/// @param iMaxSize the maximum size for the string
/// @param sEllipsis the string to insert in place of the removed mid section, default = "..."
/// @return a reference on the input string
template<class String = KString, class StringView = KStringView>
String& kLimitSizeInPlace(String& sLimitMe,
						  typename String::size_type iMaxSize,
						  StringView sEllipsis = StringView{"..."})
//-----------------------------------------------------------------------------
{
	auto iSize = sLimitMe.size();

	if (iSize > iMaxSize)
	{
		auto size = detail::CalcSizeLimit(sLimitMe, iMaxSize, sEllipsis);

#if defined(DEKAF2_HAS_FULL_CPP_17)
		sLimitMe.replace(size.first, size.second, sEllipsis);
#else
		sLimitMe.replace(size.first, size.second, sEllipsis.data(), sEllipsis.size());
#endif
	}

	return sLimitMe;

} // kLimitSizeInPlace

//-----------------------------------------------------------------------------
/// Limit the size of a string by removing data in the middle of it, and inserting an ellipsis. This function is
/// not UTF8 aware and should only be used when the content of the string has only ASCII or one-byte
/// codepage data. For UTF8 strings, use kLimitSizeUTF8()
/// @param sLimitMe the string to limit in size, as a string view (input)
/// @param iMaxSize the maximum size for the string
/// @param sEllipsis the string to insert in place of the removed mid section, default = "..."
/// @return the resized output string
template<class String = KString, class StringView = KStringView>
String kLimitSize(const StringView& sLimitMe,
				  typename String::size_type iMaxSize,
				  StringView sEllipsis = StringView{"..."})
//-----------------------------------------------------------------------------
{
	auto iSize = sLimitMe.size();

	if (iSize > iMaxSize)
	{
		auto size = detail::CalcSizeLimit(sLimitMe, iMaxSize, sEllipsis);

		KString sLimited(sLimitMe, 0, size.first);

		sLimited += sEllipsis;
		sLimited += sLimitMe.substr(size.first + size.second);

		return sLimited;
	}
	else
	{
		return sLimitMe;
	}

} // kLimitSize

//-----------------------------------------------------------------------------
/// Limit the size of a string by removing data in the middle of it, and inserting an ellipsis. This function is
/// UTF8 and UTF16 aware and will not damage multi-byte UTF8 or UTF16 codepoints.
/// @param sLimitMe the string to limit in size, as a reference (input/output)
/// @param iMaxSize the maximum size for the string (as returned by size(), not in UTF8 codepoint units)
/// @param sEllipsis the string to insert in place of the removed mid section, default = "…" (the Unicode ellipsis character)
/// @return a reference on the input string
template<class String = KString, class StringView = KStringView>
String& kLimitSizeUTF8InPlace(String& sLimitMe,
					   typename String::size_type iMaxSize,
					   StringView sEllipsis = StringView{"…"})
//-----------------------------------------------------------------------------
{
	auto iSize = sLimitMe.size();

	if (iSize > iMaxSize)
	{
		auto size = detail::CalcSizeLimitUTF8(sLimitMe, iMaxSize, sEllipsis);

#if defined(DEKAF2_HAS_FULL_CPP_17)
		sLimitMe.replace(size.first, size.second, sEllipsis);
#else
		sLimitMe.replace(size.first, size.second, sEllipsis.data(), sEllipsis.size());
#endif
	}

	return sLimitMe;

} // kLimitSizeUTF8InPlace

//-----------------------------------------------------------------------------
/// Limit the size of a string by removing data in the middle of it, and inserting an ellipsis. This function is
/// UTF8 and UTF16 aware and will not damage multi-byte UTF8 or UTF16 codepoints.
/// @param sLimitMe the string to limit in size, as a string view (input)
/// @param iMaxSize the maximum size for the string (as returned by size(), not in UTF8 codepoint units)
/// @param sEllipsis the string to insert in place of the removed mid section, default = "…" (the Unicode ellipsis character)
/// @return the input string
template<class String = KString, class StringView = KStringView>
String kLimitSizeUTF8(const StringView& sLimitMe,
					  typename String::size_type iMaxSize,
					  StringView sEllipsis = StringView{"…"})
//-----------------------------------------------------------------------------
{
	auto iSize = sLimitMe.size();

	if (iSize > iMaxSize)
	{
		auto size = detail::CalcSizeLimitUTF8(sLimitMe, iMaxSize, sEllipsis);

		KString sLimited(sLimitMe, 0, size.first);

		sLimited += sEllipsis;
		sLimited += sLimitMe.substr(size.first + size.second);

		return sLimited;
	}
	else
	{
		return sLimitMe;
	}

} // kLimitSizeUTF8

#ifndef DEKAF2_KSTRING_IS_STD_STRING
/// resize a KString without initialization
void kResizeUninitialized(KString& sStr, KString::size_type iNewSize);
#endif

/// resize a std::string without initialization
void kResizeUninitialized(std::string& sStr, std::string::size_type iNewSize);

/// convert unicode representations of ASCII punctuation into their ASCII forms. Mainly quotes, but also
/// tilde, spaces, and hyphen
KString kCurlyToStraight(KStringView sInput);

DEKAF2_NAMESPACE_END
