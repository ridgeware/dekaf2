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
///
/// The discrete UTF8 decoder in this implementation, when compiled with clang for X86_64,
/// is nearly twice as fast than a DFA approach.
/// This indicates that optimization in the compiler and branch prediction on modern CPUs work great.
/// For UTF8 codepoint counting this implementation is even 4-5 times faster than a DFA approach. That
/// is due to the reason that it does not check validity of the UTF8 while counting. It does during decode though.
/// Invalid UTF8 runs are not decoded (inefficient encoding, values >= 0x0110000, too long continuation
/// sequences, aborted continuation sequences both by end of input or start of a new codepoint) and signaled by
/// INVALID_CODEPOINT as the decoded value. It is left to the caller if this will be translated into the
/// REPLACEMENT_CHARACTER or lead to an abort of the operation. Input iterators will always point to the
/// next start of a sequence after a decode, and skip invalid input.

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <iterator>
#include <cassert>

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
	#define KUTF8_CONSTEXPR_14 inline
#endif

#if __cplusplus >= 202002L
	#define KUTF8_CONSTEXPR_20 constexpr
#else
	#define KUTF8_CONSTEXPR_20 inline
#endif

#ifdef __has_include
	#if __has_include("kconfiguration.h") && __has_include("kctype.h")
		#include "kconfiguration.h"
	#endif
#endif

#if defined(DEKAF2) || DEKAF_MAJOR_VERSION >= 2 \
 || (__has_include("kdefinitions.h") && __has_include("kctype.h"))
	#define KUTF8_DEKAF2 1
#endif

#if KUTF8_DEKAF2
#include "kctype.h"
DEKAF2_NAMESPACE_BEGIN
#else
#include <cwctype>
#endif

namespace Unicode {

#ifdef __cpp_unicode_characters
	#ifndef KUTF8_DEKAF2
	using codepoint_t = char32_t;
	#endif
	using utf16_t     = char16_t;
	#ifdef __cpp_char8_t
	using utf8_t      = char8_t;
	#else
	using utf8_t      = uint8_t;
	#endif
#else
	#ifndef KUTF8_DEKAF2
	using codepoint_t = uint32_t;
	#endif
	using utf16_t     = uint16_t;
	using utf8_t      = uint8_t;
#endif

static constexpr codepoint_t INVALID_CODEPOINT     = UINT32_MAX;     ///< our flag for invalid codepoints
static constexpr codepoint_t END_OF_INPUT          = UINT32_MAX - 1; ///< input is exhausted
static constexpr codepoint_t REPLACEMENT_CHARACTER = 0x0fffd;        ///< the replacement character to signal invalid codepoints in strings
static constexpr codepoint_t CODEPOINT_MAX         = 0x010ffff;      ///< the largest legal unicode codepoint
static constexpr codepoint_t SURROGATE_LOW_START   = 0x0d800;        ///< the lower bound of the low surrogate range in UTF16 encoding
static constexpr codepoint_t SURROGATE_LOW_END     = 0x0dbff;        ///< the upper bound of the low surrogate range in UTF16 encoding
static constexpr codepoint_t SURROGATE_HIGH_START  = 0x0dc00;        ///< the lower bound of the high surrogate range in UTF16 encoding
static constexpr codepoint_t SURROGATE_HIGH_END    = 0x0dfff;        ///< the upper bound of the high surrogate range in UTF16 encoding
static constexpr codepoint_t NEEDS_SURROGATE_START = 0x010000;       ///< the lower bound of codepoints that need surrogate substitution in UTF16 encoding
static constexpr codepoint_t NEEDS_SURROGATE_END   = 0x010ffff;      ///< the upper bound of codepoints that need (or fit for) surrogate substitution in UTF16 encoding

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper type to convert between UTF16 and other encodings
struct SurrogatePair
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----------------------------------------------------------------------------
	// default ctor, empty surrogate pair
	constexpr
	SurrogatePair() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// construct from low and high surrogate
	constexpr
	SurrogatePair(utf16_t low, utf16_t high)
	//-----------------------------------------------------------------------------
	: high(high)
	, low (low)
	{
	}

	//-----------------------------------------------------------------------------
	/// construct from codepoint. Before calling, check if the codepoint needs a surrogate pair!
	constexpr
	SurrogatePair(codepoint_t codepoint)
	//-----------------------------------------------------------------------------
	: high (SURROGATE_HIGH_START + ((codepoint -= NEEDS_SURROGATE_START) & 0x03ff))
	, low  (SURROGATE_LOW_START  + ((codepoint >> 10) & 0x03ff))
	{
	}

	//-----------------------------------------------------------------------------
	// return a codepoint from a surrogate pair
	constexpr
	codepoint_t ToCodepoint() const
	//-----------------------------------------------------------------------------
	{
		return (low << 10) + high
			- ((SURROGATE_LOW_START << 10) + SURROGATE_HIGH_START)
			+ NEEDS_SURROGATE_START;
	}

	utf16_t high { 0 };
	utf16_t	low  { 0 };

}; // SurrogatePair

//-----------------------------------------------------------------------------
/// Returns true if the given character is a single byte run, and thus an ASCII char
inline constexpr
bool IsSingleByte(utf8_t ch)
//-----------------------------------------------------------------------------
{
	return ch < 0x080;
}

//-----------------------------------------------------------------------------
/// Returns true if the given character is a UTF8 start byte (which starts a UTF8 sequence and is followed by one to three continuation bytes)
inline constexpr
bool IsStartByte(utf8_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0x0c0) == 0x0c0;
}

//-----------------------------------------------------------------------------
/// Returns true if the given character is a UTF8 continuation byte (which follow the start byte of a UTF8 sequence)
inline constexpr
bool IsContinuationByte(utf8_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0x0c0) == 0x080;
}

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a lead surrogate
inline constexpr
bool IsLeadSurrogate(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0x1ffc00) == 0x0d800;
}

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a trail surrogate
inline constexpr
bool IsTrailSurrogate(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0x1ffc00) == 0x0dc00;
}

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a lead or trail surrogate
inline constexpr
bool IsSurrogate(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0x1ff800) == 0x0d800;
}

//-----------------------------------------------------------------------------
/// Returns true if the given Unicode codepoint is valid (= < 0x0110000, not 0x0fffe, 0x0ffff, and not in 0x0d800 .. 0x0dfff)
inline constexpr
bool IsValid(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	return ch <= CODEPOINT_MAX
		&& !IsSurrogate(ch)
		&& ch != 0x0fffe
		&& ch != 0x0ffff;
}

//-----------------------------------------------------------------------------
/// Returns true if the given codepoint needs to be represented with a UTF16 surrogate pair
inline constexpr
bool NeedsSurrogates(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	// the reason to test for NEEDS_SURROGATE_END is that this prevents from
	// creating overflow surrogates from invalid codepoints, which would be
	// difficult to detect
	return (ch >= NEEDS_SURROGATE_START && ch <= NEEDS_SURROGATE_END);
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
		// we return a squared question mark 0xfffd (REPLACEMENT CHARACTER)
		// for invalid Unicode codepoints, so the bytecount is 3 as well if
		// this is a surrogate character
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

namespace KUTF8_detail {

#if !defined __clang__ && defined __GNUC__ && __GNUC__ < 9
// GCC < 9 erroneously see the .size() member return value not being used,
// although it is in a pure SFINAE context. Using a string class which has
// [[nodiscard]] set for the size() member would cause a build failure.
// Hence we disable the check for the following class.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
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
#if !defined __clang__ && defined __GNUC__ && __GNUC__ < 9
#pragma GCC diagnostic pop
#endif

} // end of namespace KUTF8_detail

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8 sequence written at iterator it
template<typename Ch, typename Iterator,
         typename std::enable_if<std::is_integral<Ch>::value
							&& !KUTF8_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
void ToUTF8(Ch sch, Iterator& it)
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
		if (KUTF8_UNLIKELY(IsSurrogate(ch) || ch == 0x0fffe || ch == 0x0ffff ))
		{
			ch = REPLACEMENT_CHARACTER;
		}
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
	}
}

//-----------------------------------------------------------------------------
// for strings, this version is up to twice as fast than the use of back inserters
/// Convert a codepoint into a UTF8 sequence appended to sNarrow
template<typename Ch, typename NarrowString,
		 typename std::enable_if<std::is_integral<Ch>::value
							&& KUTF8_detail::HasSize<NarrowString>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
void ToUTF8(Ch sch, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	codepoint_t ch = CodepointCast(sch);

	if (ch < 0x0080)
	{
		sNarrow += ch;
	}
	else if (ch < 0x0800)
	{
		sNarrow += (0xc0 | ((ch >> 6) & 0x1f));
		sNarrow += (0x80 | (ch & 0x3f));
	}
	else if (ch < 0x010000)
	{
		if (KUTF8_UNLIKELY(IsSurrogate(ch) || ch == 0x0fffe || ch == 0x0ffff ))
		{
			ch = REPLACEMENT_CHARACTER;
		}
		sNarrow += (0xe0 | ((ch >> 12) & 0x0f));
		sNarrow += (0x80 | ((ch >>  6) & 0x3f));
		sNarrow += (0x80 | (ch & 0x3f));
	}
	else if (ch < 0x0110000)
	{
		sNarrow += (0xf0 | ((ch >> 18) & 0x07));
		sNarrow += (0x80 | ((ch >> 12) & 0x3f));
		sNarrow += (0x80 | ((ch >>  6) & 0x3f));
		sNarrow += (0x80 | (ch & 0x3f));
	}
	else
	{
		// emit the squared question mark 0xfffd (REPLACEMENT CHARACTER)
		// for invalid Unicode codepoints
		ToUTF8(REPLACEMENT_CHARACTER, sNarrow);
	}
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8 sequence returned as string of type NarrowString.
template<typename NarrowString, typename Ch,
         typename std::enable_if<std::is_integral<Ch>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
NarrowString ToUTF8(Ch sch)
//-----------------------------------------------------------------------------
{
	NarrowString sNarrow{};
	ToUTF8(sch, sNarrow);
	return sNarrow;
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 or UTF32) into an UTF8 string from two iterators
template<typename NarrowString, typename Iterator>
KUTF8_CONSTEXPR_14
void ToUTF8(Iterator& it, Iterator ie, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	using N = typename std::remove_reference<decltype(*it)>::type;

	for (; KUTF8_LIKELY(it != ie); ++it)
	{
		// Make sure all surrogate combine logic is only compiled in for 16 bit strings.
		// If we would have surrogate pairs in 32 bit strings it is an error in their
		// construction in the first place. We will not try to reassemble them.
		// The UTF8 encoder will convert those to replacement characters.
		if (sizeof(N) == 2 && KUTF8_UNLIKELY(IsSurrogate(*it)))
		{
			if (KUTF8_LIKELY(IsLeadSurrogate(*it)))
			{
				SurrogatePair sp;

				sp.low = CodepointCast(*it++);

				if (KUTF8_UNLIKELY(it == ie))
				{
					// this is an incomplete surrogate
					ToUTF8(INVALID_CODEPOINT, sNarrow);
				}
				else
				{
					sp.high = CodepointCast(*it);

					if (KUTF8_UNLIKELY(!IsTrailSurrogate(sp.high)))
					{
						// the second surrogate is not valid
						ToUTF8(INVALID_CODEPOINT, sNarrow);
					}
					else
					{
						// and output the completed codepoint
						ToUTF8(sp.ToCodepoint(), sNarrow);
					}
				}
			}
			else
			{
				// this was a trail surrogate withoud lead..
				ToUTF8(INVALID_CODEPOINT, sNarrow);
			}
		}
		else
		{
			// default case
			ToUTF8(*it, sNarrow);
		}
	}
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 or UTF32) into a UTF8 string from two iterators
template<typename NarrowString, typename Iterator,
         typename std::enable_if<!KUTF8_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
NarrowString ToUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	NarrowString sNarrow{};
	ToUTF8(it, ie, sNarrow);
	return sNarrow;
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 or UTF32) into a UTF8 string
template<typename NarrowString, typename WideString,
         typename std::enable_if<!std::is_integral<WideString>::value
                              && KUTF8_detail::HasSize<WideString>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
void ToUTF8(const WideString& sWide, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	typename WideString::const_iterator it = sWide.cbegin();
	typename WideString::const_iterator ie = sWide.cend();

	ToUTF8(it, ie, sNarrow);
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 or UTF32) into a UTF8 string
template<typename NarrowString, typename WideString,
         typename std::enable_if<!std::is_integral<WideString>::value
                              && KUTF8_detail::HasSize<WideString>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
NarrowString ToUTF8(const WideString& sWide)
//-----------------------------------------------------------------------------
{
	NarrowString sNarrow{};
	ToUTF8(sWide, sNarrow);
	return sNarrow;
}

//-----------------------------------------------------------------------------
/// Convert a wchar_t string (UTF16 or UTF32) into a UTF8 string
template<typename NarrowString>
KUTF8_CONSTEXPR_14
void ToUTF8(const wchar_t* it, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	if KUTF8_UNLIKELY(it == nullptr)
	{
		return;
	}

	for (; KUTF8_LIKELY(*it != 0); ++it)
	{
		// Make sure all surrogate combine logic is only compiled in for 16 bit strings.
		// If we would have surrogate pairs in 32 bit strings it is an error in their
		// construction in the first place. We will not try to reassemble them.
		// The UTF8 encoder will convert those to replacement characters.
		if (sizeof(wchar_t) == 2 && KUTF8_UNLIKELY(IsSurrogate(*it)))
		{
			if (KUTF8_LIKELY(IsLeadSurrogate(*it)))
			{
				SurrogatePair sp;

				sp.low = CodepointCast(*it++);

				if (KUTF8_UNLIKELY(*it == 0))
				{
					// this is an incomplete surrogate
					ToUTF8(INVALID_CODEPOINT, sNarrow);
				}
				else
				{
					sp.high = CodepointCast(*it);

					if (KUTF8_UNLIKELY(!IsTrailSurrogate(sp.high)))
					{
						// the second surrogate is not valid
						ToUTF8(INVALID_CODEPOINT, sNarrow);
					}
					else
					{
						// and output the completed codepoint
						ToUTF8(sp.ToCodepoint(), sNarrow);
					}
				}
			}
			else
			{
				// this was a trail surrogate withoud lead..
				ToUTF8(INVALID_CODEPOINT, sNarrow);
			}
		}
		else
		{
			// default case
			ToUTF8(*it, sNarrow);
		}
	}
}

//-----------------------------------------------------------------------------
/// Convert a wchar_t string (UTF16 or UTF32) into a UTF8 string
template<typename NarrowString>
KUTF8_CONSTEXPR_14
NarrowString ToUTF8(const wchar_t* it)
//-----------------------------------------------------------------------------
{
	NarrowString sNarrow{};
	ToUTF8(it, sNarrow);
	return sNarrow;
}

//-----------------------------------------------------------------------------
/// if not at the start of a UTF8 sequence then advance the input iterator until the end of the current UTF8 sequence
template<typename Iterator>
KUTF8_CONSTEXPR_14
void SyncUTF8(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	for (; KUTF8_LIKELY(it != ie); ++it)
	{
		auto ch = CodepointCast(*it);

		if (!IsContinuationByte(ch))
		{
			break;
		}
	}
}

//-----------------------------------------------------------------------------
/// Return codepoint from repeatedly calling a ReadFunc that returns single chars
template<typename ReadFunc>
KUTF8_CONSTEXPR_14
codepoint_t CodepointFromUTF8Reader(ReadFunc Read, int eof=-1)
//-----------------------------------------------------------------------------
{
	codepoint_t ch = CodepointCast(Read());

	if (KUTF8_UNLIKELY(ch == static_cast<codepoint_t>(eof)))
	{
		return END_OF_INPUT;
	}

	if (KUTF8_LIKELY(ch < 128))
	{
		return ch;
	}

	if (KUTF8_UNLIKELY(ch > 0x0ff))
	{
		// error, even with char sizes > one byte UTF8 single
		// values cannot exceed 255
		return INVALID_CODEPOINT;
	}

#if (__cplusplus >= 202002L)
	// C++20 constexpr permits variable declarations without initialization
	codepoint_t lower_limit;
	codepoint_t codepoint;
	uint16_t    remaining;
	bool        bCheckForSurrogates;
#else
	// for C++ constexpr < 20 we need to initialize the vars at declaration,
	// so let's take the most probable values (for 2 byte sequences)
	codepoint_t lower_limit         = 0x080;
	codepoint_t codepoint           = ch & 0x01f;
	uint16_t    remaining           = 1;
	bool        bCheckForSurrogates = false;
#endif

	if ((ch & 0x0e0) == 0x0c0)
	{
#if (__cplusplus >= 202002L)
		// for C++20, finally init the vars here
		lower_limit         = 0x080;
		codepoint           = ch & 0x01f;
		remaining           = 1;
		bCheckForSurrogates = false;
#endif
	}
	else if ((ch & 0x0f0) == 0x0e0)
	{
		lower_limit         = 0x0800;
		codepoint           = ch & 0x0f;
		remaining           = 2;
		bCheckForSurrogates = ch == 0xbd;
	}
	else if ((ch & 0x0f8) == 0x0f0)
	{
		// do not check for too large lead byte values at this place
		// (ch >= 0x0f5) as apparently that deranges the pipeline.
		// Testing the final codepoint value below is about 10% faster.
		lower_limit         = 0x010000;
		codepoint           = ch & 0x07;
		remaining           = 3;
		bCheckForSurrogates = false;
	}
	else
	{
		// no automatic sync here
		return INVALID_CODEPOINT;
	}

	for (;;)
	{
		ch = CodepointCast(Read());

		if (ch == static_cast<codepoint_t>(eof))
		{
			return INVALID_CODEPOINT;
		}

		if (KUTF8_UNLIKELY(ch > 0x0f4))
		{
			// a UTF8 sequence cannot contain characters > 0xf4
			break;
		}

		if (KUTF8_UNLIKELY((ch & 0x0c0) != 0x080))
		{
			// error, not a continuation byte
			break;
		}

		codepoint <<= 6;
		codepoint |= (ch & 0x03f);

		if (!--remaining)
		{
			if (KUTF8_UNLIKELY(codepoint < lower_limit))
			{
				// error, ambiguous encoding
				break;
			}

			if (KUTF8_UNLIKELY(codepoint > CODEPOINT_MAX))
			{
				// error, too large for Unicode codepoint
				break;
			}

			if (KUTF8_UNLIKELY(bCheckForSurrogates && IsSurrogate(codepoint)))
			{
				break;
			}

			return codepoint; // valid
		}
	}

	// no sync here ..

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Return next codepoint at position it in range it-ie, increment it to point
/// to the begin of the following codepoint. If it == ie -> undefined behavior
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t CodepointFromUTF8(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	using N = typename std::remove_reference<decltype(*it)>::type;

	assert(it != ie);

	codepoint_t ch = CodepointCast(*it++);

	if (KUTF8_LIKELY(ch < 128))
	{
		return ch;
	}

	if (sizeof(N) > 1 && KUTF8_UNLIKELY(ch > 0x0ff))
	{
		// error, even with char sizes > one byte UTF8 single
		// values cannot exceed 255
		return INVALID_CODEPOINT;
	}

#if (__cplusplus >= 202002L)
	// C++20 constexpr permits variable declarations without initialization
	codepoint_t lower_limit;
	codepoint_t codepoint;
	uint16_t    remaining;
	bool        bCheckForSurrogates;
#else
	// for C++ constexpr < 20 we need to initialize the vars at declaration,
	// so let's take the most probable values (for 2 byte sequences)
	codepoint_t lower_limit         = 0x080;
	codepoint_t codepoint           = ch & 0x01f;
	uint16_t    remaining           = 1;
	bool        bCheckForSurrogates = false;
#endif

	if ((ch & 0x0e0) == 0x0c0)
	{
#if (__cplusplus >= 202002L)
		// for C++20, finally init the vars here
		lower_limit         = 0x080;
		codepoint           = ch & 0x01f;
		remaining           = 1;
		bCheckForSurrogates = false;
#endif
	}
	else if ((ch & 0x0f0) == 0x0e0)
	{
		lower_limit         = 0x0800;
		codepoint           = ch & 0x0f;
		remaining           = 2;
		bCheckForSurrogates = ch == 0xbd;
	}
	else if ((ch & 0x0f8) == 0x0f0)
	{
		// do not check for too large lead byte values at this place
		// (ch >= 0x0f5) as apparently that deranges the pipeline.
		// Testing the final codepoint value below is about 10% faster.
		lower_limit         = 0x010000;
		codepoint           = ch & 0x07;
		remaining           = 3;
		bCheckForSurrogates = false;
	}
	else
	{
		SyncUTF8(it, ie);

		return INVALID_CODEPOINT;
	}

	for (; KUTF8_LIKELY(it != ie); )
	{
		ch = CodepointCast(*it++);

		if (KUTF8_UNLIKELY(ch > 0x0f4))
		{
			// a UTF8 sequence cannot contain characters > 0xf4
			break;
		}

		if (KUTF8_UNLIKELY((ch & 0x0c0) != 0x080))
		{
			// error, not a continuation byte
			break;
		}

		codepoint <<= 6;
		codepoint |= (ch & 0x03f);

		if (!--remaining)
		{
			if (KUTF8_UNLIKELY(codepoint < lower_limit))
			{
				// error, ambiguous encoding
				break;
			}

			if (KUTF8_UNLIKELY(codepoint > CODEPOINT_MAX))
			{
				// error, too large for Unicode codepoint
				break;
			}

			if (KUTF8_UNLIKELY(bCheckForSurrogates && IsSurrogate(codepoint)))
			{
				break;
			}

			return codepoint; // valid
		}
	}

	SyncUTF8(it, ie);

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Returns a 32 bit codepoint from either UTF8, UTF16, or UTF32. If at call (it == ie) -> undefined behaviour
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t Codepoint(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	using N = typename std::remove_reference<decltype(*it)>::type;

	assert(it != ie);

	if (sizeof(N) == 1)
	{
		return CodepointFromUTF8(it, ie);
	}
	else if (sizeof(N) == 2)
	{
		codepoint_t ch = CodepointCast(*it++);

		if (KUTF8_UNLIKELY(IsSurrogate(ch)))
		{
			if (KUTF8_LIKELY(IsLeadSurrogate(ch)))
			{
				SurrogatePair sp;

				sp.low = ch;

				if (KUTF8_UNLIKELY(it == ie))
				{
					// this is an incomplete surrogate
					return INVALID_CODEPOINT;
				}
				else
				{
					sp.high = CodepointCast(*it);

					if (KUTF8_UNLIKELY(!IsTrailSurrogate(sp.high)))
					{
						// the second surrogate is not valid - do not advance input a second time
						return INVALID_CODEPOINT;
					}
					else
					{
						// advance the input iterator
						++it;
						// and output the completed codepoint
						return sp.ToCodepoint();
					}
				}
			}
			else
			{
				// this was a trail surrogate withoud lead..
				return INVALID_CODEPOINT;
			}
		}
		else
		{
			return ch;
		}
	}
	else
	{
		return CodepointCast(*it++);
	}
}

//-----------------------------------------------------------------------------
/// advance the input iterator by n codepoints - this works with UTF8, UTF16, or UTF32
/// @param it iterator that points to the current position in the string. Will be advanced to the new position.
/// @param ie iterator that points to the end of the string. If on return it == ie the string was exhausted.
/// @param n the number of unicode codepoints to advanve for iterator it (default = 1)
/// @return true if it != ie, else false (input string exhausted)
template<typename Iterator>
KUTF8_CONSTEXPR_14
bool Advance(Iterator& it, Iterator ie, std::size_t n = 1)
//-----------------------------------------------------------------------------
{
	for (; n-- > 0 && it != ie; )
	{
		if (Codepoint(it, ie) == INVALID_CODEPOINT)
		{
			return false;
		}
	}

	return it != ie;
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename InpType>
KUTF8_CONSTEXPR_14
void Convert(const InpType& sInp, OutType& sOut)
//-----------------------------------------------------------------------------
{
	constexpr uint16_t out_width = sizeof(typename OutType::value_type);

	typename InpType::const_iterator it = sInp.cbegin();
	typename InpType::const_iterator ie = sInp.cend();

	for (; KUTF8_LIKELY(it != ie);)
	{
		// we only get valid unicode codepoints from Codepoint()
		// (including the replacement character)
		codepoint_t ch = Codepoint(it, ie);

		if (out_width == 1)
		{
			ToUTF8(ch, sOut);
		}
		else if (out_width == 2)
		{
			if (NeedsSurrogates(ch))
			{
				SurrogatePair sp(ch);
				sOut += sp.low;
				sOut += sp.high;
			}
			else
			{
				sOut += ch;
			}

		}
		else
		{
			sOut += ch;
		}
	}
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename InpType>
KUTF8_CONSTEXPR_14
OutType Convert(const InpType& sInp)
//-----------------------------------------------------------------------------
{
	OutType sOut{};
	Convert(sInp, sOut);
	return sOut;
}

//-----------------------------------------------------------------------------
/// Return iterator at position where a UTF8 string uses invalid sequences
template<typename Iterator>
KUTF8_CONSTEXPR_14
Iterator InvalidUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	for (;KUTF8_LIKELY(it != ie);)
	{
		auto ti = it;

		if (KUTF8_UNLIKELY(CodepointFromUTF8(ti, ie) == INVALID_CODEPOINT))
		{
			break;
		}

		it = ti;
	}

	return it;
}

//-----------------------------------------------------------------------------
/// Check if a UTF8 string uses only valid sequences
template<typename Iterator>
KUTF8_CONSTEXPR_14
bool ValidUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	return InvalidUTF8(it, ie) == ie;
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
/// Check if a UTF8 string uses only valid sequences
/// @return position of first invalid sequence, or npos if not found
template<typename NarrowString>
KUTF8_CONSTEXPR_14
typename NarrowString::size_type InvalidUTF8(const NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	auto it = InvalidUTF8(sNarrow.begin(), sNarrow.end());

	if (it == sNarrow.end())
	{
		return NarrowString::npos;
	}
	else
	{
		return it - sNarrow.begin();
	}
}

//-----------------------------------------------------------------------------
/// Return iterator after max n UTF8 codepoints, begin (it) should not point inside
/// a UTF8 sequence
template<typename Iterator>
KUTF8_CONSTEXPR_14
Iterator LeftUTF8(Iterator it, Iterator ie, size_t n)
//-----------------------------------------------------------------------------
{
	for (; KUTF8_LIKELY(it < ie && n-- > 0) ;)
	{
		codepoint_t ch = CodepointCast(*it);

		if (KUTF8_LIKELY(ch < 128))
		{
			it += 1;
		}
		else if ((ch & 0x0e0) == 0x0c0)
		{
			it += 2;
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			it += 3;
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			it += 4;
		}
		else
		{
			break; // invalid..
		}
	}

	if (it > ie)
	{
		it = ie;
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
	if (n)
	{
		for (; KUTF8_LIKELY(it != ie) ;)
		{
			// check if this char starts a UTF8 sequence
			codepoint_t ch = CodepointCast(*--ie);

			if (!IsContinuationByte(ch))
			{
				if (!--n)
				{
					break;
				}
			}
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
	return ReturnString(sNarrow.data() + (it - sNarrow.begin()), sNarrow.end() - it);
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
	return ReturnString(sNarrow.data() + (it - sNarrow.begin()), ie - it);
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF8 range
template<typename Iterator>
KUTF8_CONSTEXPR_14
size_t CountUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	size_t iCount { 0 };

	for (; KUTF8_LIKELY(it < ie) ;)
	{
		codepoint_t ch = CodepointCast(*it);

		if (KUTF8_LIKELY(ch < 128))
		{
			it += 1;
		}
		else if ((ch & 0x0e0) == 0x0c0)
		{
			it += 2;
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			it += 3;
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			it += 4;
		}
		else
		{
			break; // invalid..
		}

		++iCount;
	}

	if (it > ie && iCount)
	{
		// the last codepoint was not complete - subtract one
		--iCount;
	}

	return iCount;
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF8 range
template<typename Iterator>
KUTF8_CONSTEXPR_14
size_t LazyCountUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	size_t iCount { 0 };

	for (; KUTF8_LIKELY(it < ie) ;)
	{
		codepoint_t ch = CodepointCast(*it++);
		iCount += (ch & 0x0c0) != 0x080;
	}

	return iCount;
}

//-----------------------------------------------------------------------------
// we repeat most of the code of the simple CountUTF8 here because
// the comparison with iMaxCount costs around 20% of performance
/// Count number of codepoints in UTF8 range, stop at iMaxCount.
template<typename Iterator>
KUTF8_CONSTEXPR_14
size_t CountUTF8(Iterator it, Iterator ie, std::size_t iMaxCount)
//-----------------------------------------------------------------------------
{
	size_t iCount { 0 };

	for (; KUTF8_LIKELY(it < ie) ;)
	{
		codepoint_t ch = CodepointCast(*it);

		if (KUTF8_LIKELY(ch < 128))
		{
			it += 1;
		}
		else if ((ch & 0x0e0) == 0x0c0)
		{
			it += 2;
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			it += 3;
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			it += 4;
		}
		else
		{
			break; // invalid..
		}

		if (KUTF8_UNLIKELY(++iCount >= iMaxCount))
		{
			break;
		}
	}

	if (it > ie && iCount)
	{
		// the last codepoint was not complete - subtract one
		--iCount;
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
/// Count number of codepoints in UTF8 string, stop at iMaxCount
template<typename NarrowString>
KUTF8_CONSTEXPR_14
size_t CountUTF8(const NarrowString& sNarrow, std::size_t iMaxCount)
//-----------------------------------------------------------------------------
{
	return CountUTF8(sNarrow.begin(), sNarrow.end(), iMaxCount);
}

//-----------------------------------------------------------------------------
/// Return codepoint before position it in range ibegin-it, decrement it to point
/// to the begin of the new (previous) codepoint
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t PrevCodepointFromUTF8(Iterator ibegin, Iterator& it)
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
		else if (!IsContinuationByte(ch))
		{
			Iterator nit = it;
			return CodepointFromUTF8(nit, iend);
		}
	}

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Return codepoint after pos UTF8 codepoints
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t AtUTF8(Iterator it, Iterator ie, size_t pos)
//-----------------------------------------------------------------------------
{
	auto it2 = LeftUTF8(it, ie, pos);

	if (KUTF8_LIKELY(it2 < ie))
	{
		return CodepointFromUTF8(it2, ie);
	}
	else
	{
		return INVALID_CODEPOINT;
	}
}

//-----------------------------------------------------------------------------
/// Return codepoint after pos UTF8 codepoints
template<typename NarrowString>
KUTF8_CONSTEXPR_14
codepoint_t AtUTF8(const NarrowString& sNarrow, size_t pos)
//-----------------------------------------------------------------------------
{
	return AtUTF8(sNarrow.begin(), sNarrow.end(), pos);
}

//-----------------------------------------------------------------------------
/// Returns true if the first non-ASCII codepoint is valid UTF8
template<typename Iterator>
KUTF8_CONSTEXPR_14
bool HasUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	for (; KUTF8_LIKELY(it != ie) ;)
	{
		codepoint_t ch = CodepointCast(*it);

		if (KUTF8_LIKELY(ch < 128))
		{
			++it;
		}
		else
		{
			return CodepointFromUTF8(it, ie) != INVALID_CODEPOINT;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
/// Returns true if the first non-ASCII codepoint is valid UTF8
template<typename NarrowString>
KUTF8_CONSTEXPR_14
bool HasUTF8(const NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	return HasUTF8(sNarrow.begin(), sNarrow.end());
}

//-----------------------------------------------------------------------------
/// Convert range between it and ie from UTF8, calling functor func for every
/// codepoint (which may be INVALID_CODEPOINT for input parsing errors)
template<typename Iterator, class Functor,
         typename std::enable_if<(std::is_class<Functor>::value && !KUTF8_detail::HasSize<Functor>::value)
		                        || std::is_function<Functor>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
bool FromUTF8(Iterator it, Iterator ie, Functor func)
//-----------------------------------------------------------------------------
{
	for (; KUTF8_LIKELY(it != ie);)
	{
		codepoint_t codepoint = CodepointFromUTF8(it, ie);

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
		 typename std::enable_if<(std::is_class<Functor>::value && !KUTF8_detail::HasSize<Functor>::value)
								|| std::is_function<Functor>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
bool FromUTF8(const NarrowString& sNarrow, Functor func)
//-----------------------------------------------------------------------------
{
	return FromUTF8(sNarrow.begin(), sNarrow.end(), func);
}

//-----------------------------------------------------------------------------
/// Convert string from UTF8 to wide string (either UTF16 or UTF32)
template<typename NarrowString, typename WideString,
         typename std::enable_if<!std::is_function<WideString>::value
		                        && (!std::is_class<WideString>::value || KUTF8_detail::HasSize<WideString>::value), int>::type = 0>
KUTF8_CONSTEXPR_20
bool FromUTF8(const NarrowString& sNarrow, WideString& sWide)
//-----------------------------------------------------------------------------
{
	using W = typename WideString::value_type;

	static_assert(sizeof(W) > 1, "target string needs to be at least 16 bit wide");

	return FromUTF8(sNarrow, [&sWide](codepoint_t uch)
	{
		if (sizeof(W) == 2 && NeedsSurrogates(uch))
		{
			SurrogatePair sp(uch);
			sWide += static_cast<W>(sp.low);
			sWide += static_cast<W>(sp.high);
		}
		else
		{
			sWide += static_cast<W>(uch);
		}
		return true;
	});
}

//-----------------------------------------------------------------------------
/// Convert string from UTF8 to wide string (either UTF16 or UTF32, depending on the value_type of the string)
template<typename WideString = std::u32string, typename NarrowString = std::string,
		 typename std::enable_if<!std::is_function<WideString>::value
								&& (!std::is_class<WideString>::value || KUTF8_detail::HasSize<WideString>::value), int>::type = 0>
KUTF8_CONSTEXPR_20
WideString FromUTF8(const NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	WideString sWide{};
	FromUTF8(sNarrow, sWide);
	return sWide;
}

//-----------------------------------------------------------------------------
/// Transform a UTF8 string into another string, narrow or wide, calling a transformation
/// functor for each codepoint.
/// @param sInput the UTF8 input string
/// @param sOutput the output string, passed on to the functor
/// @param func the transformation functor, called with signature bool(codepoint_t, ReturnString&)
/// @return true for success, false for failure (any failure in the functor)
template<typename NarrowString, typename ReturnString, class Functor>
bool TransformUTF8(const NarrowString& sInput, ReturnString& sOutput, Functor func)
//-----------------------------------------------------------------------------
{
	return FromUTF8(sInput, [&sOutput, &func](codepoint_t uch)
	{
		return func(uch, sOutput);
	});
}

//-----------------------------------------------------------------------------
/// Transform a UTF8 string into a lowercase UTF8 string.
/// @param sInput the UTF8 input string
/// @param sOutput the output UTF8 string
template<typename NarrowString, typename NarrowReturnString>
void ToLowerUTF8(const NarrowString& sInput, NarrowReturnString& sOutput)
//-----------------------------------------------------------------------------
{
	sOutput.reserve(sOutput.size() + sInput.size());

	TransformUTF8(sInput, sOutput, [](codepoint_t uch, NarrowReturnString& sOut)
	{
#ifdef KUTF8_DEKAF2
		ToUTF8(kToLower(uch), sOut);
#else
		ToUTF8(std::towlower(uch), sOut);
#endif
		return true;
	});
}

//-----------------------------------------------------------------------------
/// Transform a UTF8 string into a uppercase UTF8 string.
/// @param sInput the UTF8 input string
/// @param sOutput the output UTF8 string
template<typename NarrowString, typename NarrowReturnString>
void ToUpperUTF8(const NarrowString& sInput, NarrowReturnString& sOutput)
//-----------------------------------------------------------------------------
{
	sOutput.reserve(sOutput.size() + sInput.size());

	TransformUTF8(sInput, sOutput, [](codepoint_t uch, NarrowReturnString& sOut)
	{
#ifdef KUTF8_DEKAF2
		ToUTF8(kToUpper(uch), sOut);
#else
		ToUTF8(std::towupper(uch), sOut);
#endif
		return true;
	});
}

namespace CESU8 {

// see https://en.wikipedia.org/wiki/CESU-8

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 LE) that was encoded in a byte stream into a UTF8 string (for recovery purposes only)
template<typename NarrowString, typename Iterator>
KUTF8_CONSTEXPR_14
NarrowString UTF16BytesToUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	NarrowString sNarrow{};

	for (; KUTF8_LIKELY(it != ie); )
	{
		SurrogatePair sp;
		sp.low = CodepointCast(*it++) << 8;

		if (KUTF8_UNLIKELY(it == ie))
		{
			ToUTF8(REPLACEMENT_CHARACTER, sNarrow);
		}
		else
		{
			sp.low += CodepointCast(*it++);

			if (KUTF8_UNLIKELY(IsLeadSurrogate(sp.low)))
			{
				// collect another 2 bytes for surrogate replacement
				if (KUTF8_LIKELY(it != ie))
				{
					sp.high = CodepointCast(*it++) << 8;

					if (KUTF8_LIKELY(it != ie))
					{
						sp.high += CodepointCast(*it);

						if (IsTrailSurrogate(sp.high))
						{
							++it;
							ToUTF8(sp.ToCodepoint(), sNarrow);
						}
						else
						{
							// the second surrogate is not valid
							ToUTF8(REPLACEMENT_CHARACTER, sNarrow);
						}
					}
					else
					{
						ToUTF8(REPLACEMENT_CHARACTER, sNarrow);
					}
				}
				else
				{
					// we treat incomplete surrogates as simple ucs2
					ToUTF8(sp.low, sNarrow);
				}
			}
			else
			{
				ToUTF8(sp.low, sNarrow);
			}
		}
	}

	return sNarrow;
}

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 LE) that was encoded in a byte stream into a UTF8 string (for recovery purposes only)
template<typename ByteString, typename NarrowString = ByteString>
KUTF8_CONSTEXPR_14
NarrowString UTF16BytesToUTF8(const ByteString& sUTF16Bytes)
//-----------------------------------------------------------------------------
{
	return UTF16BytesToUTF8<NarrowString>(sUTF16Bytes.begin(), sUTF16Bytes.end());
}

//-----------------------------------------------------------------------------
/// Convert a UTF8 string into a byte stream with UTF16 LE encoding (for recovery purposes only)
template<typename NarrowString, typename ByteString = NarrowString>
KUTF8_CONSTEXPR_14
ByteString UTF8ToUTF16Bytes(const NarrowString& sUTF8String)
//-----------------------------------------------------------------------------
{
	using W = typename ByteString::value_type;

	ByteString sUTF16ByteString{};
	sUTF16ByteString.reserve(sUTF8String.size() * 2);

	TransformUTF8(sUTF8String, sUTF16ByteString, [](codepoint_t uch, ByteString& sOut)
	{
		if (NeedsSurrogates(uch))
		{
			SurrogatePair sp(uch);
			sOut += static_cast<W>(sp.low  >> 8 & 0x0ff);
			sOut += static_cast<W>(sp.low       & 0x0ff);
			sOut += static_cast<W>(sp.high >> 8 & 0x0ff);
			sOut += static_cast<W>(sp.high      & 0x0ff);
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

} // namespace CESU8

} // namespace Unicode

#ifdef KUTF8_DEKAF2
DEKAF2_NAMESPACE_END
#endif
