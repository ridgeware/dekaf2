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
#include "kutf.h"
#include "ktemplate.h"
#include "kcrashexit.h"
#include <cinttypes>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <map>
#include <array>

DEKAF2_NAMESPACE_BEGIN

class KInStream;
class KOutStream;

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

namespace detail {

void kMakeUpperASCII  (const char* it, const char* ie, char* out);
void kMakeLowerASCII  (const char* it, const char* ie, char* out);
void kMakeUpperLocale (const char* it, const char* ie, char* out);
void kMakeLowerLocale (const char* it, const char* ie, char* out);

} // end of namespace detail

//----------------------------------------------------------------------
/// converts the string to uppercase assuming UTF encoding
template<class String>
DEKAF2_PUBLIC
void kMakeUpper(String& sString)
//----------------------------------------------------------------------
{
	if (kutf::ValidASCII(sString))
	{
		kMakeUpperASCII(sString);
	}
	else
	{
		String sLower;
		kutf::ToUpper(sString, sLower);
		sLower.swap(sString);
	}
}

//----------------------------------------------------------------------
/// converts the string to lowercase assuming UTF encoding
template<class String>
DEKAF2_PUBLIC
void kMakeLower(String& sString)
//----------------------------------------------------------------------
{
	if (kutf::ValidASCII(sString))
	{
		kMakeLowerASCII(sString);
	}
	else
	{
		String sLower;
		kutf::ToLower(sString, sLower);
		sLower.swap(sString);
	}
}

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase (UTF8)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kToUpper(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase (UTF8)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kToLower(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kToUpperLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kToLowerLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase assuming ASCII encoding
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kToUpperASCII(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase assuming ASCII encoding
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kToLowerASCII(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// converts the string to uppercase assuming ASCII encoding
template<class InputIterator, class OutputIterator>
DEKAF2_PUBLIC
void kMakeUpperASCII(InputIterator it, InputIterator ie, OutputIterator Out)
//----------------------------------------------------------------------
{
#if !DEKAF2_IS_WINDOWS
	detail::kMakeUpperASCII(static_cast<const char*>(&*it), static_cast<const char*>(&*ie), static_cast<char*>(&*Out));
#else
	// the windows std lib throws when referencing the end iter of a stringview, so we
	// let it reference the element before and add one to the pointer
	if (ie > it)
	{
		--ie;
		detail::kMakeUpperASCII(static_cast<const char*>(&*it), static_cast<const char*>(&*ie)+1, static_cast<char*>(&*Out));
	}
#endif
}

//----------------------------------------------------------------------
/// converts the string to uppercase assuming ASCII encoding
template<class String>
DEKAF2_PUBLIC
void kMakeUpperASCII(String& sString)
//----------------------------------------------------------------------
{
	kMakeUpperASCII(sString.begin(), sString.end(), sString.begin());
}

//----------------------------------------------------------------------
/// converts the string to lowercase assuming ASCII encoding
template<class InputIterator, class OutputIterator>
DEKAF2_PUBLIC
void kMakeLowerASCII(InputIterator it, InputIterator ie, OutputIterator Out)
//----------------------------------------------------------------------
{
#if !DEKAF2_IS_WINDOWS
	detail::kMakeLowerASCII(static_cast<const char*>(&*it), static_cast<const char*>(&*ie), static_cast<char*>(&*Out));
#else
	if (ie > it)
	{
		--ie;
		detail::kMakeLowerASCII(static_cast<const char*>(&*it), static_cast<const char*>(&*ie) + 1, static_cast<char*>(&*Out));
	}
#endif
}

//----------------------------------------------------------------------
/// converts the string to lowercase assuming ASCII encoding
template<class String>
DEKAF2_PUBLIC
void kMakeLowerASCII(String& sString)
//----------------------------------------------------------------------
{
	kMakeLowerASCII(sString.begin(), sString.end(), sString.begin());
}

//----------------------------------------------------------------------
/// converts the string to uppercase assuming current locale encoding (does not work with UTF8 strings)
template<class InputIterator, class OutputIterator>
DEKAF2_PUBLIC
void kMakeUpperLocale(InputIterator it, InputIterator ie, OutputIterator Out)
//----------------------------------------------------------------------
{
#if !DEKAF2_IS_WINDOWS
	detail::kMakeUpperLocale(static_cast<const char*>(&*it), static_cast<const char*>(&*ie), static_cast<char*>(&*Out));
#else
	if (ie > it)
	{
		--ie;
		detail::kMakeUpperLocale(static_cast<const char*>(&*it), static_cast<const char*>(&*ie) + 1, static_cast<char*>(&*Out));
	}
#endif
}

//----------------------------------------------------------------------
/// converts the string to uppercase assuming current locale encoding (does not work with UTF8 strings)
template<class String>
DEKAF2_PUBLIC
void kMakeUpperLocale(String& sString)
//----------------------------------------------------------------------
{
	kMakeUpperLocale(sString.begin(), sString.end(), sString.begin());
}

//----------------------------------------------------------------------
/// converts the string to lowercase assuming current locale encoding (does not work with UTF8 strings)
template<class InputIterator, class OutputIterator>
DEKAF2_PUBLIC
void kMakeLowerLocale(InputIterator it, InputIterator ie, OutputIterator Out)
//----------------------------------------------------------------------
{
#if !DEKAF2_IS_WINDOWS
	detail::kMakeLowerLocale(static_cast<const char*>(&*it), static_cast<const char*>(&*ie), static_cast<char*>(&*Out));
#else
	if (ie > it)
	{
		--ie;
		detail::kMakeLowerLocale(static_cast<const char*>(&*it), static_cast<const char*>(&*ie) + 1, static_cast<char*>(&*Out));
	}
#endif
}

//----------------------------------------------------------------------
/// converts the string to lowercase assuming current locale encoding (does not work with UTF8 strings)
template<class String>
DEKAF2_PUBLIC
void kMakeLowerLocale(String& sString)
//----------------------------------------------------------------------
{
	kMakeLowerLocale(sString.begin(), sString.end(), sString.begin());
}

//-----------------------------------------------------------------------------
/// returns leftmost iCount chars of string
template<class String, class StringView = KStringView>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
StringView kLeft(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return StringView(sInput.data(), std::min(iCount, sInput.size()));
}

//-----------------------------------------------------------------------------
/// reduces string to leftmost iCount chars of string
template<class String>
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
String& kMakeLeft(String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	sInput.erase(std::min(iCount, sInput.size()));
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns substring starting at iStart for remaining chars
template<class String, class StringView = KStringView>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
StringView kMid(const String& sInput, std::size_t iStart)
//-----------------------------------------------------------------------------
{
	if (iStart > sInput.size()) iStart = sInput.size();
	return StringView(&sInput[iStart], sInput.size() - iStart);
}

//-----------------------------------------------------------------------------
/// returns substring starting at iStart for remaining chars
template<class String, class StringView = KStringView>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
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
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
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
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
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
/// returns leftmost iCount UTF codepoints of string
template<class String, class StringView = KStringView>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
StringView kLeftUTF(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return kutf::Left<String, StringView>(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to leftmost iCount UTF codepoints of string
template<class String>
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
String& kMakeLeftUTF(String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto it = kutf::Left(sInput.begin(), sInput.end(), iCount);
	sInput.erase(it, sInput.end());
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns substring starting at UTF codepoint iStart for iCount UTF codepoints
template<class String, class StringView = KStringView>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
StringView kMidUTF(const String& sInput, std::size_t iStart, std::size_t iCount = npos)
//-----------------------------------------------------------------------------
{
	return kutf::Mid<String, StringView>(sInput, iStart, iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to starting at UTF codepoint iStart with iCount UTF codepoints
template<class String>
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
String& kMakeMidUTF(String& sInput, std::size_t iStart, std::size_t iCount = npos)
//-----------------------------------------------------------------------------
{
	auto it = kutf::Left(sInput.begin(), sInput.end(), iStart);
	sInput.erase(sInput.begin(), it);
	return kMakeLeftUTF(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// returns rightmost iCount UTF codepoints of string
template<class String, class StringView = KStringView>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
StringView kRightUTF(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return kutf::Right<String, StringView>(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// reduces string to rightmost iCount UTF codepoints of string
template<class String>
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
String& kMakeRightUTF(String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	auto it = kutf::Right(sInput.begin(), sInput.end(), iCount);
	sInput.erase(sInput.begin(), it);
	return sInput;
}

//-----------------------------------------------------------------------------
/// returns KCodePoint at UTF position iCount
template<class String>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
KCodePoint kAtUTF(const String& sInput, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	return kutf::At(sInput, iCount);
}

//-----------------------------------------------------------------------------
/// returns true if string contains UTF8 runs
template<class String>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kHasUTF8(const String& sInput)
//-----------------------------------------------------------------------------
{
	return kutf::HasUTF8(sInput);
}

//-----------------------------------------------------------------------------
/// returns the count of unicode codepoints (or, UTF8/UTF16/UTF32 sequences)
template<class String>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
std::size_t kSizeUTF(const String& sInput)
//-----------------------------------------------------------------------------
{
	return kutf::Count(sInput);
}

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim = ',');
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kStrIn (KStringView sNeedle, KStringView sHaystack, char iDelim = ',');
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline bool kStrIn (const char* sNeedle, KStringView sHaystack, char iDelim = ',')
//----------------------------------------------------------------------
{
	return kStrIn(KStringView(sNeedle), sHaystack, iDelim);
}

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline bool kStrIn (KStringView sNeedle, const char* sHaystack, char iDelim = ',')
//----------------------------------------------------------------------
{
	return kStrIn(sNeedle, KStringView(sHaystack), iDelim);
}

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kStrIn (KStringView sNeedle, const char* Haystack[]);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
template<class Container,
	typename std::enable_if<!detail::is_cpp_str<Container>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kStrIn (KStringView sNeedle, const Container& Haystack)
//----------------------------------------------------------------------
{
	return std::find(std::begin(Haystack), std::end(Haystack), sNeedle) != std::end(Haystack);
}

//-----------------------------------------------------------------------------
/// pads string at the left up to iWidth size with chPad
template<class String>
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
String& kTrim(String& string, Compare cmp)
//-----------------------------------------------------------------------------
{
	kTrimRight(string, cmp);
	return kTrimLeft(string, cmp);
}

//-----------------------------------------------------------------------------
/// Collapses consecutive characters for which cmp returns true to one instance of chTo
template<class String, class Compare>
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
String& kCollapseAndTrim(String& string, const StringView& svCollapse, typename String::value_type chTo)
//-----------------------------------------------------------------------------
{
	return kCollapseAndTrim(string, chTo, [&svCollapse](typename String::value_type ch)
	{
		return svCollapse.find(ch) != StringView::npos;
	});
}

//-----------------------------------------------------------------------------
// version for string views
/// Get the first word from an input string, and remove it there
/// @param sInput the input string
/// @return the first word of the input string
template<typename String,
         typename std::enable_if<detail::is_string_view<String>::value == true, int>::type = 0>
DEKAF2_PUBLIC
String kGetWord(String& sInput)
//-----------------------------------------------------------------------------
{
	typename String::size_type iPos = 0;
	// loop over all whitespace
	while (iPos < sInput.size() && KASCII::kIsSpace(sInput[iPos])) ++iPos;
	auto iStart = iPos;
	// collect all chars until next whitespace
	while (iPos < sInput.size() && !KASCII::kIsSpace(sInput[iPos])) ++iPos;
	// copy all leading non-whitespace
	String sWord(sInput.data() + iStart, iPos - iStart);
	// and erase all leading chars, whitespace or not
	sInput.remove_prefix(iPos);

	return sWord;

} // kGetWord

//-----------------------------------------------------------------------------
// version for strings
/// Get the first word from an input string, and remove it there
/// @param sInput the input string
/// @return the first word of the input string
template<typename String,
         typename std::enable_if<detail::is_string_view<String>::value == false, int>::type = 0>
DEKAF2_PUBLIC
String kGetWord(String& sInput)
//-----------------------------------------------------------------------------
{
	typename String::size_type iPos = 0;
	// loop over all whitespace
	while (iPos < sInput.size() && KASCII::kIsSpace(sInput[iPos])) ++iPos;
	auto iStart = iPos;
	// collect all chars until next whitespace
	while (iPos < sInput.size() && !KASCII::kIsSpace(sInput[iPos])) ++iPos;
	// copy all leading non-whitespace
	String sWord(sInput.data() + iStart, iPos - iStart);
	// and erase all leading chars, whitespace or not
	sInput.erase(0, iPos);

	return sWord;

} // kGetWord

//-----------------------------------------------------------------------------
/// Get the first word from an input string, but do not remove it there
/// @param sInput the input string
/// @return the first word of the input string
DEKAF2_NODISCARD DEKAF2_PUBLIC 
KStringView kFirstWord(const KStringView& sInput);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Replace variables in sString found in a map Variables, search for sOpen/sClose for lead-in and lead-out.
/// Returns count of replaces
template<class String, class StringView, class ReplaceMap = std::map<StringView, StringView> >
DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsBinary(KStringView sBuffer);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Convert value into string and insert selectable separator every n digits, with requested precision
/// @param i the arithmetic value to format
/// @param chSeparator the "thousands" separator char, defaults to ',', setting to '\0' disables insertion
/// @param iEvery insert the chSeparator every iEvery chars, set to 0 to disable
/// @param iPrecision decimal precision (also for non-floating point values), defaults to 0
/// @param bRoundToNearest true if the output shall be rounded to nearest value, defaults to true
/// @return the formatted number as a String
template <class Arithmetic, class String = KString,
          typename std::enable_if<!detail::is_duration<Arithmetic>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
String kFormNumber(Arithmetic i, typename String::value_type chSeparator = ',', uint16_t iEvery = 3, uint16_t iPrecision = 0, bool bRoundToNearest = true)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_arithmetic<Arithmetic>::value, "arithmetic type required");

	String sResult;

	DEKAF2_TRY
	{
#if !DEKAF2_KSTRING_IS_STD_STRING
		sResult = KString::to_string(i);
#else
		sResult = std::to_string(i);
#endif
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
				// there is no decimal separator - add a placeholder character,
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
		else if (iPrecision != uint16_t(-1))
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

		if (iPrecision > 0 && iPrecision != uint16_t(-1))
		{
			sResult += (chSeparator != '.') ? '.' : ',';

			auto iCount = iPrecision;

			while (iCount--)
			{
				sResult += '0';
			}
		}
	}

	if (iEvery > 0 && chSeparator)
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

		if (iEvery < iPrecision && iPrecision != uint16_t(-1))
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
DEKAF2_NODISCARD
String kFormNumber(Duration duration, typename String::value_type chSeparator = ',', uint16_t iEvery = 3, uint16_t iPrecision = 0)
//-----------------------------------------------------------------------------
{
	return kFormNumber(duration.count(), chSeparator, iEvery, iPrecision);
}

//-----------------------------------------------------------------------------
/// Copy sInp and insert separator every n digits
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormString(KStringView sInp, typename KString::value_type separator = ',', typename KString::size_type every = 3);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Count occurrence of ch in container
template<class Container>
DEKAF2_NODISCARD
inline size_t kCountChar(const Container& container, const typename Container::value_type ch) noexcept
//-----------------------------------------------------------------------------
{
	return std::count(container.begin(), container.end(), ch);
}

//-----------------------------------------------------------------------------
/// Count occurrence of ch in str (to catch char* as well)
DEKAF2_PUBLIC
inline size_t kCountChar(KStringView str, const char ch) noexcept
//-----------------------------------------------------------------------------
{
	return std::count(str.begin(), str.end(), ch);
}

//-----------------------------------------------------------------------------
/// Returns true if str contains an integer, possibly with a leading + or -
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsInteger(KStringView str, bool bSigned = true) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains an unsigned integer, possibly with a leading +
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline bool kIsUnsigned(KStringView str) noexcept
//-----------------------------------------------------------------------------
{
	return kIsInteger(str, false);
}

//-----------------------------------------------------------------------------
/// Returns true if str contains a float, possibly with a leading + or -
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsFloat(KStringView str, KStringView::value_type chDecimalSeparator = '.') noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains a syntactically valid email address. This function always returns false if
/// KRegex is not available.
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsEmail(KStringView str) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Returns true if str contains a syntactically valid URL. This function always returns false if
/// KURL is not available.
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsURL(KStringView str) noexcept;
//-----------------------------------------------------------------------------

// exception free conversions

namespace detail {

extern DEKAF2_PUBLIC const uint8_t LookupBase36[256];

}

//-----------------------------------------------------------------------------
/// Converts any base36 digit into the corresponding integer value. Returns 0xFF if not
/// a valid digit
DEKAF2_NODISCARD inline DEKAF2_PUBLIC
uint8_t kFromBase36(char ch) noexcept
//-----------------------------------------------------------------------------
{
	return detail::LookupBase36[static_cast<unsigned char>(ch)];
}

//-----------------------------------------------------------------------------
/// Converts a hex digit into the corresponding integer value. Returns > 15 if not
/// a valid digit
DEKAF2_NODISCARD inline DEKAF2_PUBLIC
uint8_t kFromHexChar(char ch) noexcept
//-----------------------------------------------------------------------------
{
	return kFromBase36(ch);
}

//-----------------------------------------------------------------------------
template<class Integer, class Iterator,
         typename std::enable_if<std::is_arithmetic<Integer>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
Integer kToInt(const String& sNumber, uint16_t iBase = 10) noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<Integer>(sNumber.begin(), sNumber.end(), iBase);
}

//-----------------------------------------------------------------------------
template<class Integer, class String,
		 typename std::enable_if<std::is_arithmetic<Integer>::value &&
								 detail::is_narrow_c_str<String>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
Integer kToInt(const String sNumber, uint16_t iBase = 10) noexcept
//-----------------------------------------------------------------------------
{
	return DEKAF2_LIKELY(sNumber != nullptr) ? kToInt<Integer>(sNumber, sNumber + std::strlen(sNumber), iBase) : 0;
}

//-----------------------------------------------------------------------------
template<class Integer, class String,
		 typename std::enable_if<std::is_arithmetic<Integer>::value &&
								 detail::is_wide_c_str<String>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
Integer kToInt(const String sNumber, uint16_t iBase = 10) noexcept
//-----------------------------------------------------------------------------
{
	return DEKAF2_LIKELY(sNumber != nullptr) ? kToInt<Integer>(sNumber, sNumber + std::wcslen(sNumber), iBase) : 0;
}

//-----------------------------------------------------------------------------
template<class First>
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstNonZero(const First& iFirst)
//-----------------------------------------------------------------------------
{
	return iFirst;
}

//-----------------------------------------------------------------------------
/// return the first in a sequence of objects that is not zero (or false)
template<class First, class...More, typename std::enable_if<sizeof...(More) != 0, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstNonZero(const First& iFirst, More&&...more)
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstNonEmpty(const First& sFirst)
//-----------------------------------------------------------------------------
{
	return sFirst;
}

//-----------------------------------------------------------------------------
/// return the first in a sequence of objects that is not .empty()
template<class First, class...More, typename std::enable_if<sizeof...(More) != 0, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstNonEmpty(const First& sFirst, More&&...more)
//-----------------------------------------------------------------------------
{
	if (!sFirst.empty())
	{
		return sFirst;
	}
	return kFirstNonEmpty<First>(std::forward<More>(more)...);
}

//-----------------------------------------------------------------------------
/// return first if func returns true for first - if false returns default constructed First (e.g. the empty string, if First is a string class)
template<class First, class Functor>
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstTrue(Functor func, const First& first)
//-----------------------------------------------------------------------------
{
	if (func(first))
	{
		return first;
	}
	return First{};
}

//-----------------------------------------------------------------------------
/// return the first in a sequence of objects on which func returns true - if none matches, returns default constructed First (e.g. the empty string, if First is a string class)
template<class First, class Functor, class...More, typename std::enable_if<sizeof...(More) != 0, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstTrue(Functor func, const First& first, More&&...more)
//-----------------------------------------------------------------------------
{
	if (func(first))
	{
		return first;
	}
	return kFirstTrue<First, Functor>(func, std::forward<More>(more)...);
}

//-----------------------------------------------------------------------------
/// return first if func returns false for first - if true returns default constructed First (e.g. the empty string, if First is a string class)
template<class First, class Functor>
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstFalse(Functor func, const First& first)
//-----------------------------------------------------------------------------
{
	if (!func(first))
	{
		return first;
	}
	return First{};
}

//-----------------------------------------------------------------------------
/// return the first in a sequence of objects on which func returns false - if none matches, returns default constructed First (e.g. the empty string, if First is a string class)
template<class First, class Functor, class...More, typename std::enable_if<sizeof...(More) != 0, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
First kFirstFalse(Functor func, const First& first, More&&...more)
//-----------------------------------------------------------------------------
{
	if (!func(first))
	{
		return first;
	}
	return kFirstFalse<First, Functor>(func, std::forward<More>(more)...);
}

//-----------------------------------------------------------------------------
template<class String = KString>
DEKAF2_NODISCARD DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
String kIntToString(T i, uint16_t iBase = 10, bool bZeroPad = false, bool bUppercase = false)
//-----------------------------------------------------------------------------
{
	return kUnsignedToString(i, iBase, bZeroPad, bUppercase);
}

//-----------------------------------------------------------------------------
template<typename T, class String = KString,
         typename std::enable_if<std::is_signed<T>::value && std::is_arithmetic<T>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
String kIntToString(T i, uint16_t iBase = 10, bool bZeroPad = false, bool bUppercase = false)
//-----------------------------------------------------------------------------
{
	return kSignedToString(i, iBase, bZeroPad, bUppercase);
}

//-----------------------------------------------------------------------------
inline DEKAF2_PUBLIC
void kFromString(bool& Value, KStringView sValue)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_KSTRING_IS_STD_STRING
	return 	kToInt<int>(sValue) != 0 || kIn(sValue, "true,True,TRUE,on,On,ON,yes,Yes,YES");
#else
	Value = sValue.Bool();
#endif
}

//-----------------------------------------------------------------------------
inline DEKAF2_PUBLIC
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
inline DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value = sValue;
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<detail::has_Parse<T>::value == true, int>::type = 0>
DEKAF2_PUBLIC
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value.Parse(sValue);
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<std::is_integral<T>::value == true, int>::type = 0>
DEKAF2_PUBLIC
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value = kToInt<T>(sValue, iBase);
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<detail::is_duration<T>::value == true, int>::type = 0>
DEKAF2_PUBLIC
void kFromString(T& Value, KStringView sValue)
//-----------------------------------------------------------------------------
{
	// the rep type may be something other than an int
	typename T::rep rep_value;
	kFromString(rep_value, sValue);
	Value = T(rep_value);
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<std::is_enum<T>::value == true, int>::type = 0>
DEKAF2_PUBLIC
void kFromString(T& Value, KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	Value = static_cast<T>(kToInt<int64_t>(sValue, iBase));
}

//-----------------------------------------------------------------------------
template<typename T>
DEKAF2_NODISCARD DEKAF2_PUBLIC
T kFromString(KStringView sValue, uint16_t iBase = 10)
//-----------------------------------------------------------------------------
{
	T Value;
	kFromString(Value, sValue, iBase);
	return Value;
}

//-----------------------------------------------------------------------------
/// Escape problematic characters for command input in sInput, return escaped string
/// @param sInput the input string
/// @param sCharsToEscape the list of chars to escape
/// @param chEscapeChar the escape character, typically a backslash. If it is \0, the escaped character
/// itself is used as the escape char, it then gets doubled
/// @returns the escaped string
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kEscapeChars(KStringView sInput, KStringView sCharsToEscape, KString::value_type chEscapeChar);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Escape problematic characters for command input in sInput, return escaped string
/// @param sInput the input string
/// @returns the escaped string
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline KString kEscapeForCommands(KStringView sInput)
//-----------------------------------------------------------------------------
{
	return kEscapeChars(sInput, "'\"\\`\0", '\\');
}

//-----------------------------------------------------------------------------
/// Escape or hex encode problematic characters in sInput, append to sLog
/// @param sLog a reference for the output string
/// @param sInput the input string
DEKAF2_PUBLIC
void kEscapeForLogging(KStringRef& sLog, KStringView sInput);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Escape or hex encode problematic characters in sInput, return escaped string
/// @param sInput the input string
/// @returns the escaped string
DEKAF2_NODISCARD DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsAtStartofWordASCII(Iterator start_iter, Iterator at_iter)
//-----------------------------------------------------------------------------
{
	return (start_iter == at_iter || (start_iter < at_iter && KASCII::kIsAlNum(*(at_iter-1)) == false));

} // kIsAtStartofWordASCII

//-----------------------------------------------------------------------------
/// returns true if iterator at_iter points to end of word in iterator end_iter (end_iter is either bigger or equal with at_iter)
template<class Iterator,
		 typename std::enable_if<detail::is_cpp_str<Iterator>::value == false, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsAtEndofWordASCII(Iterator end_iter, Iterator at_iter)
//-----------------------------------------------------------------------------
{
	return (at_iter >= end_iter || KASCII::kIsAlNum(*at_iter) == false);

} // kIsAtEndofWordASCII

//-----------------------------------------------------------------------------
/// returns true if pos iPos points to start of word in string sHaystack
template<class String,
		 typename std::enable_if<detail::is_cpp_str<String>::value == true, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsAtStartofWordASCII(const String& sHaystack, typename String::size_type iPos)
//-----------------------------------------------------------------------------
{
	return iPos != String::npos && kIsAtStartofWordASCII(sHaystack.begin(), sHaystack.begin() + iPos);

} // kIsAtStartofWordASCII

//-----------------------------------------------------------------------------
/// returns true if pos iPos points to end of word in string sHaystack
template<class String,
		 typename std::enable_if<detail::is_cpp_str<String>::value == true, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsAtEndofWordASCII(const String& sHaystack, typename String::size_type iPos)
//-----------------------------------------------------------------------------
{
	return iPos != String::npos && kIsAtEndofWordASCII(sHaystack.end(), sHaystack.begin() + iPos);

} // kIsAtEndofWordASCII

namespace detail {

//-----------------------------------------------------------------------------
template<class String = KString, class StringView = KStringView>
std::pair<typename String::size_type, typename String::size_type>
DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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

		while (iStart && kutf::IsContinuationByte(ch))
		{
			ch = static_cast<uchar_t>(sLimitMe[--iStart]);
		}

		ch = static_cast<uchar_t>(sLimitMe[iStart + iRemove]);

		while (iStart + iRemove < iSize && kutf::IsContinuationByte(ch))
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

		if (iStart && kutf::IsTrailSurrogate(ch))
		{
			--iStart;
		}

		ch = static_cast<uchar_t>(sLimitMe[iStart + iRemove]);

		if (iStart + iRemove < iSize && kutf::IsLeadSurrogate(ch))
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
DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
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
DEKAF2_NODISCARD DEKAF2_PUBLIC
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
DEKAF2_PUBLIC
void kResizeUninitialized(KString& sStr, KString::size_type iNewSize);
#endif

/// resize a std::string without initialization
DEKAF2_PUBLIC
void kResizeUninitialized(std::string& sStr, std::string::size_type iNewSize);

/// convert unicode representations of ASCII punctuation into their ASCII forms. Mainly quotes, but also
/// tilde, spaces, and hyphen
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kCurlyToStraight(KStringView sInput);

/// returns true if sInput starts with a UTF8 BOM (Windows programs have the habit to add them to UTF8)
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kHasUTF8BOM(KStringView sInput);

/// returns true if InStream starts with a UTF8 BOM (Windows programs have the habit to add them to UTF8)
DEKAF2_PUBLIC
bool kHasUTF8BOM(KInStream& InStream, bool bSkipIfExisting);

/// returns a string view with skipped UTF8 BOM (if found at start of sInput)
DEKAF2_NODISCARD DEKAF2_PUBLIC
KStringView kSkipUTF8BOM(KStringView sInput);

/// returns a stream with skipped UTF8 BOM (if found at start of InStream)
DEKAF2_PUBLIC
KInStream& kSkipUTF8BOM(KInStream& InStream);

/// removes the UTF8 BOM in sInput in place, returning true if removed
DEKAF2_PUBLIC
bool kSkipUTF8BOMInPlace(KStringRef& sInput);

/// Explicitly write the ByteOrderMark in UTF8 encoding. This is deprecated, but some Microsoft applications
/// require this to display non-ASCII characters correctly. Write the BOM only as the first output, before
/// any other output, and particularly avoid this if the target could be Unix applications.
DEKAF2_PUBLIC
KOutStream& kWriteUTF8BOM(KOutStream& OutStream);

/// Explicitly write the ByteOrderMark in UTF8 encoding. This is deprecated, but some Microsoft applications
/// require this to display non-ASCII characters correctly. Write the BOM only as the first output, before
/// any other output, and particularly avoid this if the target could be Unix applications.
DEKAF2_NODISCARD DEKAF2_PUBLIC
KStringView kWriteUTF8BOM();

namespace detail
{

static constexpr KStringView sByteUnits   { "BKMGTPEZY" };
static constexpr KStringView sNumberUnits { " KMBTPEZY" };

DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormScaledUnsignedNumber (
	uint64_t    iNumber,
	uint16_t    iPrecision,
	KStringView sSeparator,
	bool        bDeleteZeroDecimals,
	uint16_t    iDivisor,
	uint16_t    iMaxDigits,
	KStringView sUnits,
	bool        bIsNegative
);

} // end of namespace detail

/// @param iNumber the number to transform
/// @param iPrecision how many digits to retain after the decimal separator (1 by default)
/// @param sSeparator the separator between the number and the unit, default none
/// @param bDeleteZeroDecimals if true, all trailing 0 decimals will be removed - defaults to false
/// @param iDivisor the divisor to apply for each magnitude, defaults to 1000
/// @param iMaxDigits how many max digits before the decimal separator (3 by default)
/// @return the formatted number as a KString
template <class T,
          typename std::enable_if<std::is_unsigned<T>    ::value &&
                                  std::is_arithmetic<T>  ::value &&
                                  !detail::is_duration<T>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormScaledNumber (
	T           iNumber,
	uint16_t    iPrecision = 1,
	KStringView sSeparator = KStringView{},
	bool        bDeleteZeroDecimals = false,
	uint16_t    iDivisor   = 1000,
	uint16_t    iMaxDigits = 3,
	KStringView sUnits     = detail::sNumberUnits
)
{
	return detail::kFormScaledUnsignedNumber (
		iNumber,
		iPrecision,
		sSeparator,
		bDeleteZeroDecimals,
		iDivisor,
		iMaxDigits,
		sUnits,
		false
	);
}

/// @param iNumber the number to transform
/// @param iPrecision how many digits to retain after the decimal separator (1 by default)
/// @param sSeparator the separator between the number and the unit, default none
/// @param bDeleteZeroDecimals if true, all trailing 0 decimals will be removed - defaults to false
/// @param iDivisor the divisor to apply for each magnitude, defaults to 1000
/// @param iMaxDigits how many max digits before the decimal separator (3 by default)
/// @return the formatted number as a KString
template <class T,
          typename std::enable_if<std::is_signed<T>      ::value &&
                                  std::is_arithmetic<T>  ::value &&
                                  !detail::is_duration<T>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormScaledNumber (
	T           iNumber,
	uint16_t    iPrecision = 1,
	KStringView sSeparator = KStringView{},
	bool        bDeleteZeroDecimals = false,
	uint16_t    iDivisor   = 1000,
	uint16_t    iMaxDigits = 3,
	KStringView sUnits     = detail::sNumberUnits
)
{
	bool bIsNegative { false };

	auto iUnsignedNumber = static_cast<uint64_t>(iNumber);

	if (iNumber < 0)
	{
		bIsNegative = true;
		iUnsignedNumber = (~iUnsignedNumber + 1);
	}

	return detail::kFormScaledUnsignedNumber (
		iUnsignedNumber,
		iPrecision,
		sSeparator,
		bDeleteZeroDecimals,
		iDivisor,
		iMaxDigits,
		sUnits,
		bIsNegative
	);
}

/// @param iBytes the bytes to transform
/// @param iPrecision how many digits to retain after the decimal separator (1 by default)
/// @param sSeparator the separator between the number and the unit, default none
/// @param bDeleteZeroDecimals if true, all trailing 0 decimals will be removed - defaults to true
/// @param iDivisor the divisor to apply for each magnitude, defaults to 1024, could reasonably be set to 1000 as well.
/// @param iMaxDigits how many max digits before the decimal separator (3 by default)
/// @return a string with the count of input bytes transformed into the relevant tera/giga/mega/kilo/byte unit
DEKAF2_NODISCARD inline DEKAF2_PUBLIC
KString kFormBytes (
	std::size_t iBytes,
	uint16_t    iPrecision = 1,
	KStringView sSeparator = KStringView{},
	bool        bDeleteZeroDecimals = true,
	uint16_t    iDivisor = 1024,
	uint16_t    iMaxDigits = 3
)
{
	return detail::kFormScaledUnsignedNumber (
		iBytes,
		iPrecision,
		sSeparator,
		bDeleteZeroDecimals,
		iDivisor,
		3,
		detail::sByteUnits,
		false
	);
}

/// @param iNumber the number to transform
/// @param iPrecision how many digits to retain after the decimal separator (1 by default)
/// @param sSeparator the separator between the number and the unit, default none
/// @param bDeleteZeroDecimals if true, all trailing 0 decimals will be removed - defaults to false
/// @param iDivisor the divisor to apply for each magnitude, defaults to 1000
/// @param iMaxDigits how many max digits before the decimal separator (3 by default)
/// @return the formatted number as a KString
template <class T,
          typename std::enable_if<std::is_arithmetic<T>  ::value &&
                                  !detail::is_duration<T>::value, int>::type = 0>
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kFormRoundedNumber (
	T           iNumber,
	uint16_t    iPrecision = 0,
	KStringView sSeparator = KStringView{},
	bool        bDeleteZeroDecimals = false,
	uint16_t    iDivisor   = 1000,
	uint16_t    iMaxDigits = 3,
	KStringView sUnits     = detail::sNumberUnits
)
{
	return kFormScaledNumber (
		iNumber,
		iPrecision,
		sSeparator,
		bDeleteZeroDecimals,
		iDivisor,
		3,
		sUnits
	);
}

/// safely erase a string/container type - content is first overwritten by default constructed elements, then removed
template <typename T>
DEKAF2_PUBLIC
typename std::enable_if<detail::has_capacity<T>::value == true, void>::type
kSafeErase(T& Container)
{
	auto iCap = Container.capacity();
	Container.assign(iCap, typename T::value_type());
	Container.clear();
}

/// safely erase a container type - content is first overwritten by default constructed elements, then removed
template <typename T>
DEKAF2_PUBLIC
typename std::enable_if<detail::has_capacity<T>::value == false, void>::type
kSafeErase(T& Container)
{
	std::fill(Container.begin(), Container.end(), typename T::value_type());
	Container.clear();
}

/// safely zeroize a string/container type - content is overwritten by default constructed elements
template <typename T>
DEKAF2_PUBLIC
typename std::enable_if<detail::has_capacity<T>::value == true, void>::type
kSafeZeroize(T& Container)
{
	auto iCap = Container.size();
	Container.assign(iCap, typename T::value_type());
}

/// safely zeroize a container type - content is overwritten by default constructed elements
template <typename T>
DEKAF2_PUBLIC
typename std::enable_if<detail::has_capacity<T>::value == false, void>::type
kSafeZeroize(T& Container)
{
	std::fill(Container.begin(), Container.end(), typename T::value_type());
}

/// serialize any trivially copyable type into a KStringView
template <typename T>
DEKAF2_NODISCARD DEKAF2_PUBLIC
typename std::enable_if<std::is_trivially_copyable<T>::value, KStringView>::type
kToStringView(T& Trivial)
{
	return KStringView(reinterpret_cast<const char*>(&Trivial), sizeof(Trivial));
}

/// deserialize any trivially copyable type from a KStringView
template <typename T>
DEKAF2_NODISCARD DEKAF2_PUBLIC
typename std::enable_if<std::is_trivially_copyable<T>::value, T>::type
kFromStringView(KStringView sData)
{
	kAssert(sData.size() == sizeof(T), "sizeof(T) is not equal the string size");
	return T(*reinterpret_cast<T*>(const_cast<char*>(sData.data())));
}

/// deserialize any trivially copyable type from a KStringView
template <typename T>
DEKAF2_PUBLIC // do not make this nodiscard
typename std::enable_if<std::is_trivially_copyable<T>::value, T&>::type
kFromStringView(T& Trivial, KStringView sData)
{
	kAssert(sData.size() == sizeof(T), "sizeof(T) is not equal the string size");
	Trivial = (*reinterpret_cast<T*>(const_cast<char*>(sData.data())));
	return Trivial;
}

DEKAF2_NAMESPACE_END
