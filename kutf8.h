/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kutf8.h
/// provides support for UTF8, UTF16 and UTF32 encoding

#include <cstdint>
#include <cstddef>

static_assert(__cplusplus >= 201103L, "The UTF code lib needs at least a C++11 compiler");

#if defined(__GNUC__) && __GNUC__ >= 4
	#define KUTF8_LIKELY(expression)   (__builtin_expect((expression), 1))
	#define KUTF8_UNLIKELY(expression) (__builtin_expect((expression), 0))
#else
	#define KUTF8_LIKELY(expression)   (expression)
	#define KUTF8_UNLIKELY(expression) (expression)
#endif

#if __cplusplus >= 201402L
	#define KUTF8_CONSTEXPR_14 constexpr
#else
	#define KUTF8_CONSTEXPR_14
#endif

#if __cplusplus > 201402L
	#if __has_include("kconfiguration.h")
		#include "kconfiguration.h"
	#endif
#endif

#ifdef DEKAF2
#include "kctype.h"
namespace dekaf2 {
#else
#include <cwctype>
#endif

namespace Unicode {

#ifndef DEKAF2
using codepoint_t = uint32_t;
#endif
using utf16_t     = uint16_t;
using utf8_t      = uint8_t;

static constexpr codepoint_t INVALID_CODEPOINT = UINT32_MAX;
static constexpr codepoint_t REPLACEMENT_CHARACTER = 0x0fffd;

struct SurrogatePair
{
	utf16_t	first  { 0 };
	utf16_t second { 0 };
};

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a lead surrogate
inline constexpr
bool IsLeadSurrogate(utf16_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0xfc00) == 0xd800;
}

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a trail surrogate
inline constexpr
bool IsTrailSurrogate(utf16_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0xfc00) == 0xdc00;
}

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a lead or trail surrogate
inline constexpr
bool IsSurrogate(utf16_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0xf800) == 0xd800;
}

//-----------------------------------------------------------------------------
/// Returns true if the given codepoint needs to be represented with a UTF16
/// surrogate pair
inline constexpr
bool NeedsSurrogates(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	return (ch >= 0x010000 && ch <= 0x010ffff);
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a surrogate pair. Check before calling that the
/// input needs surrogate separation.
inline KUTF8_CONSTEXPR_14
SurrogatePair CodepointToSurrogates(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	SurrogatePair sp;
	ch -= 0x10000;
	sp.second = 0xdc00 + (ch & 0x03ff);
	sp.first  = 0xd800 + ((ch >> 10) & 0x03ff);
	return sp;
}

//-----------------------------------------------------------------------------
/// Convert a surrogate pair into a codepoint. Check before calling that the
/// surrogates are valid for composition.
inline constexpr
codepoint_t SurrogatesToCodepoint(SurrogatePair sp)
//-----------------------------------------------------------------------------
{
	return (sp.first << 10) + sp.second - ((0xd800 << 10) + 0xdc00 - 0x10000);
}

//-----------------------------------------------------------------------------
/// Cast any integral type into a codepoint_t, without signed bit expansion.
template<typename Ch>
constexpr
codepoint_t CodepointCast(Ch sch)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_integral<Ch>::value, "can only convert integral types");

	return (sizeof(Ch) == 1) ? static_cast<uint8_t>(sch)
		:  (sizeof(Ch) == 2) ? static_cast<uint16_t>(sch)
		:  static_cast<uint32_t>(sch);
}

//-----------------------------------------------------------------------------
/// Returns the count of bytes that a UTF8 representation for a given codepoint would need
template<typename Ch>
KUTF8_CONSTEXPR_14
size_t UTF8Bytes(Ch sch)
//-----------------------------------------------------------------------------
{
	codepoint_t ch = CodepointCast(sch);

	if (ch < 0x0080)
	{
		return 1;
	}
	else if (ch < 0x0800)
	{
		return 2;
	}
	else if (ch < 0x010000)
	{
		return 3;
	}
	else if (ch < 0x0110000)
	{
		return 4;
	}
	else
	{
		// we return a squared question mark 0xfffd (REPLACEMENT CHARACTER)
		// for invalid Unicode codepoints, so the bytecount is 3
		return 3;
	}
}

namespace detail {

// SFINAE helper to distinguish between string classes and iterators
template <typename T>
class HasSize
{
private:
	typedef char Yes;
	typedef char No[2];

	template<typename C> static auto Test(void*)
	-> decltype(size_t{std::declval<C const>().size()}, Yes{});

	template<typename> static No& Test(...);

public:
	static constexpr bool const value = sizeof(Test<T>(0)) == sizeof(Yes);
};

} // end of namespace detail

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8 sequence written at iterator it
template<typename Ch, typename Iterator,
         typename std::enable_if_t<std::is_integral<Ch>::value
							&& !detail::HasSize<Iterator>::value>* = nullptr>
KUTF8_CONSTEXPR_14
bool ToUTF8(Ch sch, Iterator& it)
//-----------------------------------------------------------------------------
{
	codepoint_t ch = CodepointCast(sch);

	if (ch < 0x0080)
	{
		*it++ = ch;
	}
	else if (ch < 0x0800)
	{
		*it++ = (0xc0 | ((ch >> 6) & 0x1f));
		*it++ = (0x80 | (ch & 0x3f));
	}
	else if (ch < 0x010000)
	{
		*it++ = (0xe0 | ((ch >> 12) & 0x0f));
		*it++ = (0x80 | ((ch >>  6) & 0x3f));
		*it++ = (0x80 | (ch & 0x3f));
	}
	else if (ch < 0x0110000)
	{
		*it++ = (0xf0 | ((ch >> 18) & 0x07));
		*it++ = (0x80 | ((ch >> 12) & 0x3f));
		*it++ = (0x80 | ((ch >>  6) & 0x3f));
		*it++ = (0x80 | (ch & 0x3f));
	}
	else
	{
		// emit the squared question mark 0xfffd (REPLACEMENT CHARACTER)
		// for invalid Unicode codepoints
		ToUTF8(REPLACEMENT_CHARACTER, it);
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8 sequence appended to string sNarrow.
template<typename Ch, typename NarrowString,
         typename std::enable_if_t<std::is_integral<Ch>::value
	                            && detail::HasSize<NarrowString>::value>* = nullptr>
KUTF8_CONSTEXPR_14
bool ToUTF8(Ch sch, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	auto it = std::back_inserter(sNarrow);
	return ToUTF8(sch, it);
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8 sequence returned as string of type NarrowString.
template<typename Ch, typename NarrowString,
         typename = std::enable_if_t<std::is_integral<Ch>::value> >
KUTF8_CONSTEXPR_14
NarrowString ToUTF8(Ch sch)
//-----------------------------------------------------------------------------
{
	NarrowString sRet;
	ToUTF8(sch, sRet);
	return sRet;
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 or UTF32) into a UTF8 string
template<typename WideString, typename NarrowString,
         typename std::enable_if_t<!std::is_integral<WideString>::value>* = nullptr>
KUTF8_CONSTEXPR_14
bool ToUTF8(const WideString& sWide, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	typename WideString::const_iterator it = sWide.cbegin();
	typename WideString::const_iterator ie = sWide.cend();

	auto OutIter = std::back_inserter(sNarrow);

	for (; it != ie; ++it)
	{
		// make sure all surrogate logic is only compiled in for 16 bit strings
		if (sizeof(typename WideString::value_type) == 2 && IsLeadSurrogate(*it))
		{
			SurrogatePair sp;
			sp.first = CodepointCast(*it++);
			if (it == ie)
			{
				// we treat incomplete surrogates as simple ucs2
				if (!ToUTF8(sp.first, OutIter))
				{
					return false;
				}
			}
			else
			{
				sp.second = CodepointCast(*it++);
				if (!IsTrailSurrogate(sp.second))
				{
					// the second surrogate is not valid - simply treat them both as ucs2
					if (!ToUTF8(sp.first, OutIter))
					{
						return false;
					}
					if (!ToUTF8(sp.second, OutIter))
					{
						return false;
					}
				}
				else
				{
					if (!ToUTF8(SurrogatesToCodepoint(sp), OutIter))
					{
						return false;
					}
				}
			}
		}
		else
		{
			// default case
			if (!ToUTF8(*it, OutIter))
			{
				return false;
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
/// Check if a UTF8 string uses only valid sequences
template<typename Iterator>
KUTF8_CONSTEXPR_14
bool ValidUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	uint16_t remaining { 0 };
	codepoint_t codepoint { 0 };
	codepoint_t lower_limit { 0 };

	for (; it != ie; ++it)
	{
		codepoint_t ch = CodepointCast(*it);

		if (sizeof(*it) > 1 && ch > 0x0ff)
		{
			return false;
		}

		switch (remaining)
		{
			case 0:
				{
					codepoint = 0;
					if (ch < 128)
					{
						break;
					}
					else if ((ch & 0x0e0) == 0x0c0)
					{
						remaining = 1;
						lower_limit = 0x080;
						codepoint = ch & 0x01f;
					}
					else if ((ch & 0x0f0) == 0x0e0)
					{
						remaining = 2;
						lower_limit = 0x0800;
						codepoint = ch & 0x0f;
					}
					else if ((ch & 0x0f8) == 0x0f0)
					{
						remaining = 3;
						lower_limit = 0x010000;
						codepoint = ch & 0x07;
					}
					else
					{
						return false;
					}
					break;
				}

			case 3:
			case 2:
			case 1:
				{
					if ((ch & 0x0c0) != 0x080)
					{
						return false;
					}
					codepoint <<= 6;
					codepoint |= (ch & 0x03f);
					if (!--remaining)
					{
						if (codepoint < lower_limit)
						{
							return false;
						}
					}
					break;
				}

			default:
				return false;

		}

	}
	return !remaining;
}

//-----------------------------------------------------------------------------
/// Check if a UTF8 string uses only valid sequences
template<typename NarrowString>
KUTF8_CONSTEXPR_14
bool ValidUTF8(const NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	return ValidUTF8(sNarrow.begin(), sNarrow.end());
}

//-----------------------------------------------------------------------------
/// Return iterator after max n UTF8 codepoints, begin (it) should not point inside
/// a UTF8 sequence
template<typename Iterator>
KUTF8_CONSTEXPR_14
Iterator LeftUTF8(Iterator it, Iterator ie, size_t n)
//-----------------------------------------------------------------------------
{
	for (; KUTF8_LIKELY(it != ie && n-- > 0) ;)
	{
		codepoint_t ch = CodepointCast(*it);

		++it;

		if (KUTF8_LIKELY(ch < 128))
		{
		}
		else if ((ch & 0x0e0) == 0x0c0)
		{
			if (it != ie) ++it;
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			if (it != ie) ++it;
			if (it != ie) ++it;
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			if (it != ie) ++it;
			if (it != ie) ++it;
			if (it != ie) ++it;
		}
		else
		{
			break; // invalid..
		}
	}

	return it;
}

//-----------------------------------------------------------------------------
/// Return string with max n left UTF8 codepoints in sNarrow
template<typename NarrowString, typename ReturnString = NarrowString>
KUTF8_CONSTEXPR_14
ReturnString LeftUTF8(const NarrowString& sNarrow, size_t n)
//-----------------------------------------------------------------------------
{
	auto it = LeftUTF8(sNarrow.begin(), sNarrow.end(), n);
	return ReturnString(sNarrow.data(), it - sNarrow.begin());
}

//-----------------------------------------------------------------------------
/// Return iterator max n UTF8 codepoints before ie, end (ie) should not point inside
/// a UTF8 sequence
template<typename Iterator>
KUTF8_CONSTEXPR_14
Iterator RightUTF8(Iterator it, Iterator ie, size_t n)
//-----------------------------------------------------------------------------
{
	while (KUTF8_LIKELY(ie != it && n > 0))
	{
		// check if this char starts a UTF8 sequence
		codepoint_t ch = CodepointCast(*--ie);

		if (KUTF8_LIKELY(ch < 128))
		{
			--n;
		}
		else if (   (ch & 0x0e0) == 0x0c0
				 || (ch & 0x0f0) == 0x0e0
				 || (ch & 0x0f8) == 0x0f0 )
		{
			--n;
		}
	}

	return ie;
}

//-----------------------------------------------------------------------------
/// Return string with max n right UTF8 codepoints in sNarrow
template<typename NarrowString, typename ReturnString = NarrowString>
KUTF8_CONSTEXPR_14
ReturnString RightUTF8(const NarrowString& sNarrow, size_t n)
//-----------------------------------------------------------------------------
{
	auto it = RightUTF8(sNarrow.begin(), sNarrow.end(), n);
#ifndef _MSC_VER
	return ReturnString(it, sNarrow.end() - it);
#else
	return ReturnString(sNarrow.data() + (it - sNarrow.begin()), sNarrow.end() - it);
#endif
}

//-----------------------------------------------------------------------------
/// Return string with max n UTF8 codepoints in sNarrow, starting after pos UTF8 codepoints
template<typename NarrowString, typename ReturnString = NarrowString>
KUTF8_CONSTEXPR_14
ReturnString MidUTF8(const NarrowString& sNarrow, size_t pos, size_t n)
//-----------------------------------------------------------------------------
{
	auto it = LeftUTF8(sNarrow.begin(), sNarrow.end(), pos);
	auto ie = LeftUTF8(it, sNarrow.end(), n);
#ifndef _MSC_VER
	return ReturnString(it, ie - it);
#else
	return ReturnString(sNarrow.data() + (it - sNarrow.begin()), ie - it);
#endif
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF8 range
template<typename Iterator>
KUTF8_CONSTEXPR_14
size_t CountUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	size_t iCount { 0 };

	for (; KUTF8_LIKELY(it != ie) ;)
	{
		codepoint_t ch = CodepointCast(*it);

		++it;

		if (KUTF8_LIKELY(ch < 128))
		{
		}
		else if ((ch & 0x0e0) == 0x0c0)
		{
			if (it != ie) ++it;
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			if (it != ie) ++it;
			if (it != ie) ++it;
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			if (it != ie) ++it;
			if (it != ie) ++it;
			if (it != ie) ++it;
		}
		else
		{
			break; // invalid..
		}

		++iCount;

	}

	return iCount;
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF8 string
template<typename NarrowString>
KUTF8_CONSTEXPR_14
size_t CountUTF8(const NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	return CountUTF8(sNarrow.begin(), sNarrow.end());
}

//-----------------------------------------------------------------------------
/// Return next codepoint at position it in range it-ie, increment it to point
/// to the begin of the following codepoint
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t NextCodepointFromUTF8(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	using N = typename std::remove_reference<decltype(*it)>::type;

	if (KUTF8_UNLIKELY(it == ie))
	{
		return INVALID_CODEPOINT;
	}

	codepoint_t ch = CodepointCast(*it++);

	if (KUTF8_LIKELY(ch < 128))
	{
		return ch;
	}

	if (sizeof(N) > 1 && ch > 0x0ff)
	{
		return INVALID_CODEPOINT;
	}

	// need to initialize the vars for constexpr, so let's take
	// the most probable value (for 2 byte sequences)
	uint16_t remaining = 1;
	codepoint_t lower_limit = 0x080;
	codepoint_t codepoint = ch & 0x01f;

	if ((ch & 0x0e0) == 0x0c0)
	{
	}
	else if ((ch & 0x0f0) == 0x0e0)
	{
		remaining = 2;
		lower_limit = 0x0800;
		codepoint = ch & 0x0f;
	}
	else if ((ch & 0x0f8) == 0x0f0)
	{
		remaining = 3;
		lower_limit = 0x010000;
		codepoint = ch & 0x07;
	}
	else
	{
		return INVALID_CODEPOINT;
	}

	for (; KUTF8_LIKELY(it != ie); )
	{
		ch = CodepointCast(*it++);

		if (KUTF8_UNLIKELY((sizeof(N) > 1 && ch > 0x0ff)))
		{
			break; // invalid
		}

		if (KUTF8_UNLIKELY((ch & 0x0c0) != 0x080))
		{
			break; // invalid
		}

		codepoint <<= 6;
		codepoint |= (ch & 0x03f);

		if (!--remaining)
		{
			if (KUTF8_UNLIKELY(codepoint < lower_limit))
			{
				break; // invalid
			}

			return codepoint; // valid
		}
	}

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Return codepoint before position it in range ibegin-it, decrement it to point
/// to the begin of the new (previous) codepoint
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t PrevCodepointFromUTF8(Iterator& it, Iterator ibegin)
//-----------------------------------------------------------------------------
{
	auto iend = it;

	while (KUTF8_LIKELY(it != ibegin))
	{
		// check if this char starts a UTF8 sequence
		codepoint_t ch = CodepointCast(*--it);

		if (KUTF8_LIKELY(ch < 128))
		{
			return ch;
		}
		else if (   (ch & 0x0e0) == 0x0c0
				 || (ch & 0x0f0) == 0x0e0
				 || (ch & 0x0f8) == 0x0f0 )
		{
			Iterator nit = it;
			return NextCodepointFromUTF8(nit, iend);
		}
	}

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Convert range between it and ie from UTF8, calling functor func for every
/// codepoint
template<typename Iterator, class Functor,
         typename std::enable_if_t<(std::is_class<Functor>::value && !detail::HasSize<Functor>::value)
		                        || std::is_function<Functor>::value>* = nullptr>
KUTF8_CONSTEXPR_14
bool FromUTF8(Iterator it, Iterator ie, Functor func)
//-----------------------------------------------------------------------------
{
	for (; KUTF8_LIKELY(it != ie);)
	{
		codepoint_t codepoint = NextCodepointFromUTF8(it, ie);

		if (KUTF8_UNLIKELY(codepoint == INVALID_CODEPOINT))
		{
			return false;
		}

		if (!func(codepoint))
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
/// Convert string from UTF8, calling functor func for every codepoint
template<typename NarrowString, class Functor,
		 typename std::enable_if_t<(std::is_class<Functor>::value && !detail::HasSize<Functor>::value)
								|| std::is_function<Functor>::value>* = nullptr>
KUTF8_CONSTEXPR_14
bool FromUTF8(const NarrowString& sNarrow, Functor func)
//-----------------------------------------------------------------------------
{
	return FromUTF8(sNarrow.begin(), sNarrow.end(), func);
}

//-----------------------------------------------------------------------------
/// Convert string from UTF8 to wide string (either UTF16 or UTF32)
template<typename NarrowString, typename WideString,
         typename std::enable_if_t<!std::is_function<WideString>::value
		                        && (!std::is_class<WideString>::value || detail::HasSize<WideString>::value)>* = nullptr>
bool FromUTF8(const NarrowString& sNarrow, WideString& sWide)
//-----------------------------------------------------------------------------
{
	using W = typename WideString::value_type;

	static_assert(sizeof(W) > 1, "target string needs to be at least 16 bit wide");

	return FromUTF8(sNarrow, [&sWide](codepoint_t uch)
	{
		if (sizeof(W) == 2 && NeedsSurrogates(uch))
		{
			SurrogatePair sp = CodepointToSurrogates(uch);
			sWide += static_cast<W>(sp.first);
			sWide += static_cast<W>(sp.second);
		}
		else
		{
			sWide += static_cast<W>(uch);
		}
		return true;
	});
}

//-----------------------------------------------------------------------------
/// Transform a UTF8 string into another UTF8 string, calling a transformation
/// functor for each codepoint.
template<typename NarrowString, typename NarrowReturnString, class Functor>
bool TransformUTF8(const NarrowString& sInput, NarrowReturnString& sOutput, Functor func)
//-----------------------------------------------------------------------------
{
	return FromUTF8(sInput, [&sOutput, &func](codepoint_t uch)
	{
		return func(uch, sOutput);
	});
}

//-----------------------------------------------------------------------------
/// Transform a UTF8 string into a lowercase UTF8 string.
template<typename NarrowString, typename NarrowReturnString>
bool ToLowerUTF8(const NarrowString& sInput, NarrowReturnString& sOutput)
//-----------------------------------------------------------------------------
{
	sOutput.reserve(sOutput.size() + sInput.size());

	return TransformUTF8(sInput, sOutput, [](codepoint_t uch, NarrowReturnString& sOut)
	{
#ifdef DEKAF2
		return ToUTF8(kToLower(uch), sOut);
#else
		return ToUTF8(std::towlower(uch), sOut);
#endif
	});
}

//-----------------------------------------------------------------------------
/// Transform a UTF8 string into a uppercase UTF8 string.
template<typename NarrowString, typename NarrowReturnString>
bool ToUpperUTF8(const NarrowString& sInput, NarrowReturnString& sOutput)
//-----------------------------------------------------------------------------
{
	sOutput.reserve(sOutput.size() + sInput.size());

	return TransformUTF8(sInput, sOutput, [](codepoint_t uch, NarrowReturnString& sOut)
	{
#ifdef DEKAF2
		return ToUTF8(kToUpper(uch), sOut);
#else
		return ToUTF8(std::towupper(uch), sOut);
#endif
	});
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 LE) that was encoded in a byte stream
/// into a UTF8 string
template<typename NarrowString, typename Iterator>
KUTF8_CONSTEXPR_14
NarrowString UTF16BytesToUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	NarrowString sNarrow;
	auto OutIter = std::back_inserter(sNarrow);

	for (; it != ie; )
	{
		SurrogatePair sp;
		sp.first = CodepointCast(*it++) << 8;

		if (KUTF8_UNLIKELY(it == ie))
		{
			ToUTF8(REPLACEMENT_CHARACTER, OutIter);
		}
		else
		{
			sp.first += CodepointCast(*it++);

			if (KUTF8_UNLIKELY(IsLeadSurrogate(sp.first)))
			{
				// collect another 2 bytes for surrogate replacement
				if (KUTF8_LIKELY(it != ie))
				{
					sp.second = CodepointCast(*it++) << 8;

					if (KUTF8_LIKELY(it != ie))
					{
						sp.second += CodepointCast(*it++);

						if (IsTrailSurrogate(sp.second))
						{
							ToUTF8(SurrogatesToCodepoint(sp), OutIter);
						}
						else
						{
							// the second surrogate is not valid - simply treat them both as ucs2
							ToUTF8(sp.first, OutIter);
							ToUTF8(sp.second, OutIter);
						}
					}
					else
					{
						ToUTF8(REPLACEMENT_CHARACTER, OutIter);
					}
				}
				else
				{
					// we treat incomplete surrogates as simple ucs2
					ToUTF8(sp.first, OutIter);
				}
			}
			else
			{
				ToUTF8(sp.first, OutIter);
			}
		}
	}

	return sNarrow;
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 LE) that was encoded in a byte stream
/// into a UTF8 string
template<typename ByteString, typename NarrowString = ByteString>
KUTF8_CONSTEXPR_14
NarrowString UTF16BytesToUTF8(const ByteString& sUTF16Bytes)
//-----------------------------------------------------------------------------
{
	return UTF16BytesToUTF8<NarrowString>(sUTF16Bytes.begin(), sUTF16Bytes.end());
}

//-----------------------------------------------------------------------------
/// Convert a UTF8 string into a byte stream with UTF16 LE encoding
template<typename NarrowString, typename ByteString = NarrowString>
KUTF8_CONSTEXPR_14
ByteString UTF8ToUTF16Bytes(const NarrowString& sUTF8String)
//-----------------------------------------------------------------------------
{
	using W = typename ByteString::value_type;

	ByteString sUTF16ByteString;
	sUTF16ByteString.reserve(sUTF8String.size() * 2);

	TransformUTF8(sUTF8String, sUTF16ByteString, [](codepoint_t uch, ByteString& sOut)
	{
		if (NeedsSurrogates(uch))
		{
			SurrogatePair sp = CodepointToSurrogates(uch);
			sOut += static_cast<W>(sp.first  >> 8 & 0x0ff);
			sOut += static_cast<W>(sp.first       & 0x0ff);
			sOut += static_cast<W>(sp.second >> 8 & 0x0ff);
			sOut += static_cast<W>(sp.second      & 0x0ff);
		}
		else
		{
			sOut += static_cast<W>(uch >>  8 & 0x0ff);
			sOut += static_cast<W>(uch       & 0x0ff);
		}
		return true;

	});

	return sUTF16ByteString;
}

} // namespace Unicode

#ifdef DEKAF2
} // of namespace dekaf2
#endif
