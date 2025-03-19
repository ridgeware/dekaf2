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
/// provides support for UTF8, UTF16 and UTF32 encoding with highly vectorizable template code.
/// Most of the methods are constexpr.
///
/// The discrete UTF8 decoder in this implementation, when compiled with clang for X86_64,
/// is nearly twice as fast than a DFA approach. For UTF8 codepoint counting this implementation is even
/// 4-5 times faster than a DFA approach.
///
/// Invalid UTF8 runs are not decoded (inefficient encoding, values >= 0x0110000, too long continuation
/// sequences, aborted continuation sequences both by end of input or start of a new codepoint) and signaled by
/// INVALID_CODEPOINT as the decoded value. It is left to the caller if this will be translated into the
/// REPLACEMENT_CHARACTER or lead to an abort of the operation. Input iterators will always point to the
/// next start of a sequence after a decode, and skip invalid input.
///
/// This code, if configured to do so, now (2025) uses the excellent simdutf library to perform bulk operations such
/// as converting between the various UTF versions, to validate UTF, and to case change strings. SIMD instructions
/// push performance for bulk operations by at least 2 times (for case changes) to 5 times (for encoding conversions)
/// and 10 times (for validation and count) compared to the discrete template code. simdutf is wrapped seamlessly
/// into the existing template interface. Performance measurements were done on both amd64 and aarch64 with
/// (nearly) comparable results. When using this header standalone, please #define KUTF8_WITH_SIMDUTF 1
/// and make sure <simdutf.h> is in the include path.
///
/// PLEASE NOTE: When using simdutf, invalid codepoints will not be marked with the REPLACEMENT_CHARACTER,
/// but instead lead to an abort of the entire operation. Result strings will be empty, and boolean return values false.
/// To find out about the position of an invalid codepoint, when simdutf is enabled you have to use the
/// InvalidUTF() method on the original input.
///
/// ABOUT THE INTERFACE: This code assumes that strings with 8 bit chars are UTF8 strings, strings with 16 bit
/// chars are UTF16 strings, and strings with 32 bit chars are UTF32 strings. Same is valid for iterators. In general,
/// there is one method version using iterators, and another using strings.
///
/// All methods use template types, therefore any string type that supports the usual string functions including
/// .size(), .resize(), .begin(), .end() may be used.
///
/// UTF16 and UTF32 strings are assumed to have the endianess of the platform, so, in most cases low endian.

/*

 This header provides the following methods:

 --- Convert

 /// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32 (from an iterator pair)
 bool ConvertUTF(Iterator it, Iterator ie, OutType& sOutput)

 /// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
 bool ConvertUTF(const InpType& sInput, OutType& sOutput)

 /// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
 OutType ConvertUTF(const InpType& sInp)

 /// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/16/32 string
 bool ConvertUTF(const wchar_t* it, UTFString& sUTF)

 /// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/16/32 string
 UTFString ConvertUTF(const wchar_t* it)

 --- ToUTF

 /// Convert a codepoint into a UTF8/16/32 sequence written at iterator it
 void ToUTF(Char codepoint, Iterator& it)

 /// Convert a codepoint into a UTF8/16/32 sequence appended to sUTF
 void ToUTF(Char codepoint, UTFString& sUTF)

 /// Convert a codepoint into a UTF8/16/32 sequence returned as string of type UTFString.
 UTFString ToUTF(Char codepoint)

 --- Latin1ToUTF

 /// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string (from two iterators)
 bool Latin1ToUTF(Iterator it, Iterator ie, UTFString& sUTF)

 /// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string (from two iterators)
 UTFString Latin1ToUTF(Iterator it, Iterator ie)

 /// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string
 bool Latin1ToUTF(const Latin1String& sLatin1, UTFString& sUTF)

 /// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string
 UTFString Latin1ToUTF(const Latin1String& sLatin1)

 --- get codepoint

 /// Return next codepoint from repeatedly calling a ReadFunc that returns single chars
 codepoint_t CodepointFromUTF8Reader(ReadFunc Read, int eof=-1)

 /// Return next codepoint at position it in range it-ie, increment it to point
 /// to the begin of the following codepoint. If at call it == ie -> undefined behavior
 codepoint_t CodepointFromUTF8(Iterator& it, Iterator ie)

 /// Return codepoint before position it in range ibegin-it, decrement it to point
 /// to the begin of the new (previous) codepoint
 codepoint_t PrevCodepoint(Iterator ibegin, Iterator& it)

 /// Returns a 32 bit codepoint from either UTF8, UTF16, or UTF32. If at call (it == ie) -> undefined behaviour
 codepoint_t Codepoint(Iterator& it, Iterator ie)

 --- move iterator

 /// if not at the start of a UTF8/16/32 sequence then advance the input iterator until the end of the current UTF sequence
 void SyncUTF(Iterator& it, Iterator ie)

 /// advance the input iterator by n codepoints - this works with UTF8, UTF16, or UTF32
 bool AdvanceUTF(Iterator& it, Iterator ie, std::size_t n = 1)

 --- validation

 /// Return iterator at position where a UTF8/16/32 string uses invalid sequences
 Iterator InvalidUTF(Iterator it, Iterator ie)

 /// Check if a UTF8/16/32 string uses only valid sequences
 bool ValidUTF(Iterator it, Iterator ie)

 /// Check if a UTF8/16/32 string uses only valid sequences
 bool ValidUTF(const UTFString& sUTF)

 /// Check if a UTF8/16/32 string uses only valid sequences
 typename UTFString::size_type InvalidUTF(const UTFString& sUTF)

 /// Returns true if the first non-ASCII codepoint is valid UTF8
 bool HasUTF8(Iterator it, Iterator ie)

 /// Returns true if the first non-ASCII codepoint is valid UTF8
 bool HasUTF8(const UTF8String& sUTF8)

 --- counting

 /// Returns the count of bytes that a UTF8 representation for a given codepoint would need
 std::size_t UTF8Bytes(Char codepoint)

/// Count number of codepoints in UTF range (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
 size_t CountUTF(Iterator it, Iterator ie, std::size_t iMaxCount = std::size_t(-1))

 /// Count number of codepoints in UTF string (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
 size_t CountUTF(const UTFString& sUTF, std::size_t iMaxCount = std::size_t(-1))

 --- substrings, iterator movement

 /// Return iterator after max n UTF8/UTF16/UTF32 codepoints
 Iterator LeftUTF(Iterator it, Iterator ie, std::size_t n)

 /// Return string with max n left UTF8/UTF16/UTF32 codepoints in sUTF
 ReturnString LeftUTF(const UTFString& sUTF, std::size_t n)

 /// Return iterator max n UTF8/UTF16/UTF32 codepoints before ie
 Iterator RightUTF(Iterator it, Iterator ie, std::size_t n)

 /// Return string with max n right UTF8/UTF16/UTF32 codepoints in sUTF
 ReturnString RightUTF(const UTFString& sUTF, size_t n)

 /// Return string with max n UTF8/UTF16/UTF32 codepoints in sUTF, starting after pos UTF8/UTF16/UTF32 codepoints
 ReturnString MidUTF(const UTFString& sUTF, std::size_t pos, std::size_t n)

 /// Return codepoint after pos UTF8/UTF16/UTF32 codepoints
 codepoint_t AtUTF(Iterator it, Iterator ie, std::size_t pos)

 --- transformation

 /// Convert range between it and ie from UTF8/UTF16/UTF32, calling functor func for every
 /// codepoint (which may be INVALID_CODEPOINT for input parsing errors)
 bool ForEachUTF(Iterator it, Iterator ie, Functor func)

 /// Convert string from UTF8/UTF16/UTF32, calling functor func for every codepoint
 bool ForEachUTF(const UTFString& sUTF, Functor func)

 /// Transform a string in UTF8, UTF16, or UTF32 into another string in UTF8, UTF16, or UTF32 (also mixed), calling a transformation
 /// functor for each codepoint.
 bool TransformUTF(const UTFString& sInput, ReturnUTFString& sOutput, Functor func)

 /// Transform a Unicode string into a lowercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
 void ToLowerUTF(const InputString& sInput, OutputString& sOutput)

 /// Transform a Unicode string into an uppercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
 void ToUpperUTF(const InputString& sInput, OutputString& sOutput)

 --- helpers

 /// Returns true if the given character is a single byte run, and thus an ASCII char
 bool IsSingleByte(utf8_t ch)

 /// Returns true if the given character is a UTF8 start byte (which starts a UTF8 sequence and is followed by one to three continuation bytes)
 bool IsStartByte(utf8_t ch)

 /// Returns true if the given character is a UTF8 continuation byte (which follow the start byte of a UTF8 sequence)
 bool IsContinuationByte(utf8_t ch)

 /// Returns true if the given UTF16 character is a lead surrogate
 bool IsLeadSurrogate(codepoint_t codepoint)

 /// Returns true if the given UTF16 character is a trail surrogate
 bool IsTrailSurrogate(codepoint_t codepoint)

 /// Returns true if the given UTF16 character is a lead or trail surrogate
 bool IsSurrogate(codepoint_t codepoint)

 /// Returns true if the given Unicode codepoint is valid (= < 0x0110000, not 0x0fffe, 0x0ffff, and not in 0x0d800 .. 0x0dfff)
 bool IsValid(codepoint_t codepoint)

 /// Returns true if the given codepoint needs to be represented with a UTF16 surrogate pair
 bool NeedsSurrogates(codepoint_t codepoint)

 */

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <iterator>
#include <cassert>
#include <algorithm> // for std::transform
#include <cstring>   // for std::memcpy

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
	#ifndef KUTF8_WITH_SIMDUTF
		#ifdef DEKAF2_WITH_SIMDUTF
			#define KUTF8_WITH_SIMDUTF 1
		#endif
	#endif
#endif

#if KUTF8_DEKAF2
	#undef KUTF8_NAMESPACE // we need the namespace being the default "Unicode"
	#include "kctype.h"
	#if KUTF8_WITH_SIMDUTF
		#include "bits/simd/kutf.h"
	#endif
	DEKAF2_NAMESPACE_BEGIN
#else
	#include <cwctype>
	#if KUTF8_WITH_SIMDUTF
		// make sure this include is found at compile time, and do not forget
		// to link against libsimdutf.a
		#include <simdutf.h>
		namespace simd = ::simdutf;
	#endif
#endif

#ifndef KUTF8_NAMESPACE
	#define KUTF8_NAMESPACE Unicode
#endif

namespace KUTF8_NAMESPACE {

#ifdef __cpp_unicode_characters
	#ifndef KUTF8_DEKAF2
	using codepoint_t = char32_t;
	#endif
	using utf32_t     = char32_t;
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
	using utf32_t     = uint32_t;
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
bool IsLeadSurrogate(codepoint_t codepoint)
//-----------------------------------------------------------------------------
{
	return (codepoint & 0x1ffc00) == 0x0d800;
}

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a trail surrogate
inline constexpr
bool IsTrailSurrogate(codepoint_t codepoint)
//-----------------------------------------------------------------------------
{
	return (codepoint & 0x1ffc00) == 0x0dc00;
}

//-----------------------------------------------------------------------------
/// Returns true if the given UTF16 character is a lead or trail surrogate
inline constexpr
bool IsSurrogate(codepoint_t codepoint)
//-----------------------------------------------------------------------------
{
	return (codepoint & 0x1ff800) == 0x0d800;
}

//-----------------------------------------------------------------------------
/// Returns true if the given Unicode codepoint is valid (= < 0x0110000, not 0x0fffe, 0x0ffff, and not in 0x0d800 .. 0x0dfff)
inline constexpr
bool IsValid(codepoint_t codepoint)
//-----------------------------------------------------------------------------
{
	return codepoint <= CODEPOINT_MAX
	    && !IsSurrogate(codepoint)
	    && codepoint != 0x0fffe
	    && codepoint != 0x0ffff;
}

//-----------------------------------------------------------------------------
/// Returns true if the given codepoint needs to be represented with a UTF16 surrogate pair
inline constexpr
bool NeedsSurrogates(codepoint_t codepoint)
//-----------------------------------------------------------------------------
{
	// the reason to test for NEEDS_SURROGATE_END is that this prevents from
	// creating overflow surrogates from invalid codepoints, which would be
	// difficult to detect
	return (codepoint >= NEEDS_SURROGATE_START && codepoint <= NEEDS_SURROGATE_END);
}

//-----------------------------------------------------------------------------
/// Cast any integral type into a codepoint_t, without signed bit expansion.
template<typename Char>
constexpr
codepoint_t CodepointCast(Char ch)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_integral<Char>::value, "can only convert integral types");

	return (sizeof(Char) == 1) ? static_cast<utf8_t>(ch)
		:  (sizeof(Char) == 2) ? static_cast<utf16_t>(ch)
		:  static_cast<utf32_t>(ch);
}

//-----------------------------------------------------------------------------
/// Returns the count of bytes that a UTF8 representation for a given codepoint would need
template<typename Char>
KUTF8_CONSTEXPR_14
std::size_t UTF8Bytes(Char codepoint)
//-----------------------------------------------------------------------------
{
	codepoint_t cp = CodepointCast(codepoint);

	if (cp < 0x0080)
	{
		return 1;
	}
	else if (cp < 0x0800)
	{
		return 2;
	}
	else if (cp < 0x010000)
	{
		// we return a squared question mark 0xfffd (REPLACEMENT CHARACTER)
		// for invalid Unicode codepoints, so the bytecount is 3 as well if
		// this is a surrogate character
		return 3;
	}
	else if (cp < 0x0110000)
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
/// Convert a codepoint into a UTF8/UTF16/UTF32 sequence written at iterator it
template<typename Char, typename Iterator,
         typename std::enable_if<std::is_integral<Char>::value
                                 && !KUTF8_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
void ToUTF(Char codepoint, Iterator& it)
//-----------------------------------------------------------------------------
{
	using value_type = typename std::remove_reference<decltype(*it)>::type;
	constexpr std::size_t iOutputWidth = sizeof(value_type);

	codepoint_t cp = CodepointCast(codepoint);

	if (iOutputWidth == 1)
	{
		if (cp < 0x0080)
		{
			*it++ = cp;
		}
		else if (cp < 0x0800)
		{
			*it++ = (0xc0 | ((cp >> 6) & 0x1f));
			*it++ = (0x80 | (cp & 0x3f));
		}
		else if (cp < 0x010000)
		{
			if (KUTF8_UNLIKELY(IsSurrogate(cp) || cp == 0x0fffe || cp == 0x0ffff ))
			{
				cp = REPLACEMENT_CHARACTER;
			}
			*it++ = (0xe0 | ((cp >> 12) & 0x0f));
			*it++ = (0x80 | ((cp >>  6) & 0x3f));
			*it++ = (0x80 | (cp & 0x3f));
		}
		else if (cp < 0x0110000)
		{
			*it++ = (0xf0 | ((cp >> 18) & 0x07));
			*it++ = (0x80 | ((cp >> 12) & 0x3f));
			*it++ = (0x80 | ((cp >>  6) & 0x3f));
			*it++ = (0x80 | (cp & 0x3f));
		}
		else
		{
			// emit the squared question mark 0xfffd (REPLACEMENT CHARACTER)
			// for invalid Unicode codepoints
			ToUTF(REPLACEMENT_CHARACTER, it);
		}
	}
	else if (iOutputWidth == 2 && NeedsSurrogates(cp))
	{
		SurrogatePair sp(cp);
		*it++ = static_cast<value_type>(sp.low);
		*it++ = static_cast<value_type>(sp.high);
	}
	else
	{
		if (KUTF8_UNLIKELY(!IsValid(cp)))
		{
			cp = REPLACEMENT_CHARACTER;
		}
		*it++ = static_cast<value_type>(cp);
	}
}

//-----------------------------------------------------------------------------
// for strings, this version is up to twice as fast than the use of back inserters
/// Convert a codepoint into a UTF8/UTF16/UTF32 sequence appended to sUTF
template<typename Char, typename UTFString,
         typename std::enable_if<std::is_integral<Char>::value
                                 && KUTF8_detail::HasSize<UTFString>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
void ToUTF(Char codepoint, UTFString& sUTF)
//-----------------------------------------------------------------------------
{
	using value_type = typename UTFString::value_type;
	constexpr std::size_t iOutputWidth = sizeof(value_type);

	codepoint_t cp = CodepointCast(codepoint);
	
	if (iOutputWidth == 1)
	{
		if (cp < 0x0080)
		{
			sUTF += cp;
		}
		else if (cp < 0x0800)
		{
			sUTF += (0xc0 | ((cp >> 6) & 0x1f));
			sUTF += (0x80 | (cp & 0x3f));
		}
		else if (cp < 0x010000)
		{
			if (KUTF8_UNLIKELY(IsSurrogate(cp) || cp == 0x0fffe || cp == 0x0ffff ))
			{
				cp = REPLACEMENT_CHARACTER;
			}
			sUTF += (0xe0 | ((cp >> 12) & 0x0f));
			sUTF += (0x80 | ((cp >>  6) & 0x3f));
			sUTF += (0x80 | (cp & 0x3f));
		}
		else if (cp < 0x0110000)
		{
			sUTF += (0xf0 | ((cp >> 18) & 0x07));
			sUTF += (0x80 | ((cp >> 12) & 0x3f));
			sUTF += (0x80 | ((cp >>  6) & 0x3f));
			sUTF += (0x80 | (cp & 0x3f));
		}
		else
		{
			// emit the squared question mark 0xfffd (REPLACEMENT CHARACTER)
			// for invalid Unicode codepoints
			ToUTF(REPLACEMENT_CHARACTER, sUTF);
		}
	}
	else if (KUTF8_UNLIKELY(iOutputWidth == 2 && NeedsSurrogates(cp)))
	{
		SurrogatePair sp(cp);
		sUTF += static_cast<value_type>(sp.low);
		sUTF += static_cast<value_type>(sp.high);
	}
	else
	{
		if (KUTF8_UNLIKELY(!IsValid(cp)))
		{
			cp = REPLACEMENT_CHARACTER;
		}
		sUTF += static_cast<value_type>(cp);
	}
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF sequence returned as string of type UTFString.
template<typename UTFString, typename Char,
         typename std::enable_if<std::is_integral<Char>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
UTFString ToUTF(Char codepoint)
//-----------------------------------------------------------------------------
{
	UTFString sUTF{};
	ToUTF(codepoint, sUTF);
	return sUTF;
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string (from two iterators)
template<typename UTFString, typename Iterator>
KUTF8_CONSTEXPR_14
bool Latin1ToUTF(Iterator it, Iterator ie, UTFString& sUTF)
//-----------------------------------------------------------------------------
{
	using input_value_type = typename std::remove_reference<decltype(*it)>::type;
	constexpr auto iInputWidth = sizeof(input_value_type);

	static_assert(iInputWidth == 1, "supporting only 8 bit strings in Latin1ToUTF");

#if KUTF8_WITH_SIMDUTF

	constexpr std::size_t iOutputWidth = sizeof(typename UTFString::value_type);

	auto iInputSize = std::distance(it, ie);
	if (!iInputSize) return true;

	const void* input = &*it;

	// we have to assign a value to satisfy non-C++17
	std::size_t iTargetSize = 0;

	switch (iOutputWidth)
	{
		case 4:
			iTargetSize = iInputSize;
			break;
		case 2:
			iTargetSize = iInputSize;
			break;
		case 1:
			iTargetSize = simd::utf8_length_from_latin1(static_cast<const char*>(input), iInputSize);
			break;
	}

	if (!iTargetSize) return false;

	auto iOldSize = sUTF.size();
	sUTF.resize (iOldSize + iTargetSize);
	// pre C++17 has const .data(), so we take a ref on the first element
	void* pOut = &sUTF[0] + iOldSize;
	// we have to assign a value to satisfy non-C++17
	std::size_t iWrote = 0;

	switch (iOutputWidth)
	{
		case 4:
			iWrote = simd::convert_latin1_to_utf32(static_cast<const char*>(input), iInputSize, static_cast<char32_t*>(pOut));
			break;
		case 2:
			iWrote = simd::convert_latin1_to_utf16(static_cast<const char*>(input), iInputSize, static_cast<char16_t*>(pOut));
			break;
		case 1:
			iWrote = simd::convert_latin1_to_utf8(static_cast<const char*>(input), iInputSize, static_cast<char*>(pOut));
			break;
	}

	return iWrote == iTargetSize;

#else

	for (; KUTF8_LIKELY(it != ie); ++it)
	{
		ToUTF(*it, sUTF);
	}

	return true;

#endif // KUTF8_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string (from two iterators)
template<typename UTFString, typename Iterator,
         typename std::enable_if<!KUTF8_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
UTFString Latin1ToUTF(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	UTFString sUTF{};
	Latin1ToUTF(it, ie, sUTF);
	return sUTF;
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string
template<typename UTFString, typename Latin1String,
         typename std::enable_if<!std::is_integral<Latin1String>::value
                              && KUTF8_detail::HasSize<Latin1String>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
bool Latin1ToUTF(const Latin1String& sLatin1, UTFString& sUTF)
//-----------------------------------------------------------------------------
{
	return Latin1ToUTF(sLatin1.begin(), sLatin1.end(), sUTF);
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded narrow string into a UTF8/UTF16/UTF32 string
template<typename UTFString, typename Latin1String>
KUTF8_CONSTEXPR_14
UTFString Latin1ToUTF(const Latin1String& sLatin1)
//-----------------------------------------------------------------------------
{
	UTFString sUTF{};
	Latin1ToUTF(sLatin1, sUTF);
	return sUTF;
}

//-----------------------------------------------------------------------------
/// if not at the start of a UTF sequence then advance the input iterator until the end of the current UTF sequence
template<typename Iterator>
KUTF8_CONSTEXPR_14
void SyncUTF(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		for (; KUTF8_LIKELY(it != ie); ++it)
		{
			auto cp = CodepointCast(*it);

			if (!IsContinuationByte(cp))
			{
				break;
			}
		}
	}
	else if (iInputWidth == 2)
	{
		if (it != ie && IsTrailSurrogate(*it)) ++it;
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

	do
	{
		if ((ch & 0x0e0) == 0x0c0)
		{
			codepoint_t codepoint = ch & 0x01f;

			ch = CodepointCast(Read());

			if (ch == static_cast<codepoint_t>(eof)) break;

			// a UTF8 sequence cannot contain characters > 0xf4
			if (KUTF8_UNLIKELY(ch > 0x0f4) || !IsContinuationByte(ch)) break;

			codepoint <<= 6;
			codepoint |= (ch & 0x03f);

			// lower limit, protect from ambiguous encoding
			if (KUTF8_UNLIKELY(codepoint < 0x080)) break;

			return codepoint; // valid
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			codepoint_t codepoint    = ch & 0x0f;
			bool bCheckForSurrogates = ch == 0xbd;

			ch = CodepointCast(Read());

			if (ch == static_cast<codepoint_t>(eof)) break;

			// a UTF8 sequence cannot contain characters > 0xf4
			if (KUTF8_UNLIKELY(ch > 0x0f4) || !IsContinuationByte(ch)) break;

			codepoint <<= 6;
			codepoint |= (ch & 0x03f);

			ch = CodepointCast(Read());

			if (ch == static_cast<codepoint_t>(eof)) break;

			// a UTF8 sequence cannot contain characters > 0xf4
			if (KUTF8_UNLIKELY(ch > 0x0f4) || !IsContinuationByte(ch)) break;

			codepoint <<= 6;
			codepoint |= (ch & 0x03f);

			// lower limit, protect from ambiguous encoding
			if (KUTF8_UNLIKELY(codepoint < 0x0800)) break;
			if (KUTF8_UNLIKELY(bCheckForSurrogates && IsSurrogate(codepoint))) break;

			return codepoint; // valid
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			// do not check for too large lead byte values at this place
			// (ch >= 0x0f5) as apparently that deranges the pipeline.
			// Testing the final codepoint value below is about 10% faster.
			codepoint_t codepoint = ch & 0x07;

			ch = CodepointCast(Read());

			if (ch == static_cast<codepoint_t>(eof)) break;

			// a UTF8 sequence cannot contain characters > 0xf4
			if (KUTF8_UNLIKELY(ch > 0x0f4) || !IsContinuationByte(ch)) break;

			codepoint <<= 6;
			codepoint |= (ch & 0x03f);

			ch = CodepointCast(Read());

			if (ch == static_cast<codepoint_t>(eof)) break;

			// a UTF8 sequence cannot contain characters > 0xf4
			if (KUTF8_UNLIKELY(ch > 0x0f4) || !IsContinuationByte(ch)) break;

			codepoint <<= 6;
			codepoint |= (ch & 0x03f);

			ch = CodepointCast(Read());

			if (ch == static_cast<codepoint_t>(eof)) break;

			// a UTF8 sequence cannot contain characters > 0xf4
			if (KUTF8_UNLIKELY(ch > 0x0f4) || !IsContinuationByte(ch)) break;

			codepoint <<= 6;
			codepoint |= (ch & 0x03f);

			// lower limit, protect from ambiguous encoding
			if (KUTF8_UNLIKELY(codepoint < 0x010000)) break;
			if (KUTF8_UNLIKELY(codepoint > CODEPOINT_MAX)) break;

			return codepoint; // valid
		}

	} while (false);

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
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	assert(it != ie);

	auto ch = CodepointCast(it[0]);

	if (ch < 128)
	{
		++it;
		return ch;
	}

	if (iInputWidth > 1 && KUTF8_UNLIKELY(ch > 0x0ff))
	{
		++it;
		// error, even with char sizes > one byte UTF8 single
		// values cannot exceed 255
		return INVALID_CODEPOINT;
	}

	do
	{
		if ((ch & 0x0e0) == 0x0c0)
		{
			codepoint_t codepoint = (ch & 0x01f) << 6;

			if (std::distance(it, ie) >= 2)
			{
				auto ch1 = CodepointCast(it[1]);

				it += 2;

				// a UTF8 sequence cannot contain characters > 0xf4
				if (KUTF8_UNLIKELY(ch1 > 0x0f4 || !IsContinuationByte(ch1))) break;

				codepoint |= (ch1 & 0x03f);

				// lower limit, protect from ambiguous encoding
				if (KUTF8_UNLIKELY(codepoint < 0x080)) break;

				return codepoint; // valid
			}
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			codepoint_t codepoint    = (ch & 0x0f) << 12;
			bool bCheckForSurrogates = ch == 0xbd;

			if (std::distance(it, ie) >= 3)
			{
				auto ch1 = CodepointCast(it[1]);
				auto ch2 = CodepointCast(it[2]);

				it += 3;

				// a UTF8 sequence cannot contain characters > 0xf4
				if (KUTF8_UNLIKELY(ch1 > 0x0f4 || !IsContinuationByte(ch1))) break;
				if (KUTF8_UNLIKELY(ch2 > 0x0f4 || !IsContinuationByte(ch2))) break;

				ch1 &= 0x03f;
				ch2 &= 0x03f;

				codepoint |= ch2;
				codepoint |= ch1 << 6;

				// lower limit, protect from ambiguous encoding
				if (KUTF8_UNLIKELY(codepoint < 0x0800)) break;
				if (KUTF8_UNLIKELY(bCheckForSurrogates && IsSurrogate(codepoint))) break;

				return codepoint; // valid
			}
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			// do not check for too large lead byte values at this place
			// (ch >= 0x0f5) as apparently that deranges the pipeline.
			// Testing the final codepoint value below is about 10% faster.
			codepoint_t codepoint = (ch & 0x07) << 18;

			if (std::distance(it, ie) >= 4)
			{
				auto ch1 = CodepointCast(it[1]);
				auto ch2 = CodepointCast(it[2]);
				auto ch3 = CodepointCast(it[3]);

				it += 4;

				// a UTF8 sequence cannot contain characters > 0xf4
				if (KUTF8_UNLIKELY(ch1 > 0x0f4 || !IsContinuationByte(ch1))) break;
				if (KUTF8_UNLIKELY(ch2 > 0x0f4 || !IsContinuationByte(ch2))) break;
				if (KUTF8_UNLIKELY(ch3 > 0x0f4 || !IsContinuationByte(ch3))) break;

				ch1 &= 0x03f;
				ch2 &= 0x03f;
				ch3 &= 0x03f;

				codepoint |= ch3;
				codepoint |= ch2 <<  6;
				codepoint |= ch1 << 12;

				// lower limit, protect from ambiguous encoding
				if (KUTF8_UNLIKELY(codepoint < 0x010000)) break;
				if (KUTF8_UNLIKELY(codepoint > CODEPOINT_MAX)) break;

				return codepoint; // valid
			}
		}
		else
		{
			++it;
		}

	} while (false);

	SyncUTF(it, ie);

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Returns a 32 bit codepoint from either UTF8, UTF16, or UTF32. If at call (it == ie) -> undefined behaviour
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t Codepoint(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	assert(it != ie);

	if (iInputWidth == 1)
	{
		return CodepointFromUTF8(it, ie);
	}
	else if (iInputWidth == 2)
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
		auto cp = CodepointCast(*it++);

		if (KUTF8_UNLIKELY(cp > CODEPOINT_MAX ||
		                  (cp >= SURROGATE_LOW_START && cp <= SURROGATE_HIGH_END)))
		{
			return INVALID_CODEPOINT;
		}

		return cp;
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
bool AdvanceUTF(Iterator& it, Iterator ie, std::size_t n = 1)
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
/// Return iterator at position where a UTF string uses invalid sequences (either of UTF8, UTF16, UTF32)
template<typename Iterator>
KUTF8_CONSTEXPR_14
Iterator InvalidUTF(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
#if KUTF8_WITH_SIMDUTF

	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	const void* buf = &*it;

	switch (iInputWidth)
	{
		case 4:
		{
			auto res = simd::validate_utf32_with_errors(static_cast<const char32_t*>(buf), std::distance(it, ie));
			return (res.error == simd::SUCCESS) ? ie : it + res.count;
		}
		case 2:
		{
			auto res = simd::validate_utf16_with_errors(static_cast<const char16_t*>(buf), std::distance(it, ie));
			return (res.error == simd::SUCCESS) ? ie : it + res.count;
		}
		case 1:
		{
			auto res = simd::validate_utf8_with_errors(static_cast<const char*>(buf), std::distance(it, ie));
			return (res.error == simd::SUCCESS) ? ie : it + res.count;
		}
		default:
			break;
	}

	return it;

#else

	for (;KUTF8_LIKELY(it != ie);)
	{
		auto ti = it;

		if (KUTF8_UNLIKELY(Codepoint(it, ie) == INVALID_CODEPOINT)) return ti;
	}

	return it;

#endif // KUTF8_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Check if a UTF string uses only valid sequences (either of UTF8, UTF16, UTF32)
template<typename Iterator>
KUTF8_CONSTEXPR_14
bool ValidUTF(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
#if KUTF8_WITH_SIMDUTF

	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	const void* buf = &*it;

	switch (iInputWidth)
	{
		case 4:
			return simd::validate_utf32(static_cast<const char32_t*>(buf), std::distance(it, ie));
		case 2:
			return simd::validate_utf16(static_cast<const char16_t*>(buf), std::distance(it, ie));
		case 1:
			return simd::validate_utf8(static_cast<const char*>(buf), std::distance(it, ie));
		default:
			break;
	}

	return false;

#else

	return InvalidUTF(it, ie) == ie;

#endif // KUTF8_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Check if a UTF string uses only valid sequences (either of UTF8, UTF16, UTF32)
template<typename UTFString>
KUTF8_CONSTEXPR_14
bool ValidUTF(const UTFString& sUTF)
//-----------------------------------------------------------------------------
{
	return ValidUTF(sUTF.begin(), sUTF.end());
}

//-----------------------------------------------------------------------------
/// Check if a UTF string uses only valid sequences (either of UTF8, UTF16, UTF32)
/// @return position of first invalid sequence, or npos if not found
template<typename UTFString>
KUTF8_CONSTEXPR_14
typename UTFString::size_type InvalidUTF(const UTFString& sUTF)
//-----------------------------------------------------------------------------
{
	auto it = InvalidUTF(sUTF.begin(), sUTF.end());

	if (it == sUTF.end())
	{
		return UTFString::npos;
	}
	else
	{
		return it - sUTF.begin();
	}
}

//-----------------------------------------------------------------------------
/// Return iterator after max n UTF8/UTF16/UTF32 codepoints
template<typename Iterator>
KUTF8_CONSTEXPR_14
Iterator LeftUTF(Iterator it, Iterator ie, std::size_t n)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		for (; KUTF8_LIKELY(n >= 8 && std::distance(it, ie) >= 8) ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
		}

		for (; it < ie && n > 0 ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
		}

		for (; it < ie; ++it)
		{
			codepoint_t ch = CodepointCast(*it);
			if (!IsContinuationByte(ch)) break;
		}
	}
	else if (iInputWidth == 2)
	{
		for (; KUTF8_LIKELY(n >= 8 && std::distance(it, ie) >= 8) ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
		}

		for (; it < ie && n > 0 ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
		}
	}
	else if (iInputWidth == 4)
	{
		if (it < ie)
		{
			it += std::min(static_cast<std::size_t>(std::distance(it, ie)), n);
		}
	}
	
	return it;
}

//-----------------------------------------------------------------------------
/// Return string with max n left UTF8 codepoints in sNarrow
template<typename UTFString, typename ReturnString = UTFString>
KUTF8_CONSTEXPR_14
ReturnString LeftUTF(const UTFString& sUTF, std::size_t n)
//-----------------------------------------------------------------------------
{
	auto it = LeftUTF(sUTF.begin(), sUTF.end(), n);
	return ReturnString(sUTF.data(), it - sUTF.begin());
}

//-----------------------------------------------------------------------------
/// Return iterator max n UTF8/UTF16/UTF32 codepoints before ie
template<typename Iterator>
KUTF8_CONSTEXPR_14
Iterator RightUTF(Iterator it, Iterator ie, std::size_t n)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		for (; KUTF8_LIKELY(n >= 8 && std::distance(it, ie) >= 8) ;)
		{
			codepoint_t ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
		}

		for (; KUTF8_LIKELY(n > 0 && it != ie) ;)
		{
			// check if this char starts a UTF8 sequence
			codepoint_t ch = CodepointCast(*--ie);
			n -= !IsContinuationByte(ch);
		}
	}
	else if (iInputWidth == 2)
	{
		for (; KUTF8_LIKELY(n >= 8 && std::distance(it, ie) >= 8) ;)
		{
			codepoint_t ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
		}
		for (; it < ie && n > 0 ;)
		{
			codepoint_t ch = CodepointCast(*--ie);
			n -= !IsTrailSurrogate(ch);
		}
	}
	else if (iInputWidth == 4)
	{
		if (it < ie)
		{
			it -= std::min(static_cast<std::size_t>(std::distance(it, ie)), n);
		}
	}

	return ie;
}

//-----------------------------------------------------------------------------
/// Return string with max n right UTF8/UTF16/UTF32 codepoints in sNarrow
template<typename UTFString, typename ReturnString = UTFString>
KUTF8_CONSTEXPR_14
ReturnString RightUTF(const UTFString& sUTF, std::size_t n)
//-----------------------------------------------------------------------------
{
	auto it = RightUTF(sUTF.begin(), sUTF.end(), n);
	return ReturnString(sUTF.data() + (it - sUTF.begin()), sUTF.end() - it);
}

//-----------------------------------------------------------------------------
/// Return string with max n UTF8/UTF16/UTF32 codepoints in sNarrow, starting after pos UTF8/UTF16/UTF32 codepoints
template<typename UTFString, typename ReturnString = UTFString>
KUTF8_CONSTEXPR_14
ReturnString MidUTF(const UTFString& sUTF, std::size_t pos, std::size_t n)
//-----------------------------------------------------------------------------
{
	auto it = LeftUTF(sUTF.begin(), sUTF.end(), pos);
	auto ie = LeftUTF(it, sUTF.end(), n);
	return ReturnString(sUTF.data() + (it - sUTF.begin()), ie - it);
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF range (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
template<typename Iterator>
KUTF8_CONSTEXPR_14
std::size_t CountUTF(Iterator it, Iterator ie, std::size_t iMaxCount = std::size_t(-1))
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

#if KUTF8_WITH_SIMDUTF

	switch (iInputWidth)
	{
		case 4:
		{
			return std::distance(it, ie);
		}
		case 2:
		{
			if (iMaxCount != std::size_t(-1) && std::distance(it, ie) > static_cast<std::ptrdiff_t>(iMaxCount))
			{
				// reduce the end iterator
				ie = it + iMaxCount;
				SyncUTF(ie, ie + 1);
			}
			const void* buf = &*it;
			return simd::count_utf16(static_cast<const char16_t*>(buf), std::distance(it, ie));
		}
		case 1:
		{
			if (iMaxCount != std::size_t(-1) && std::distance(it, ie) > static_cast<std::ptrdiff_t>(iMaxCount * 4 + 3))
			{
				// assuming a max of 4 bytes per codepoint, reduce the end iterator
				ie = it + iMaxCount * 4;
				SyncUTF(ie, ie + 4);
			}
			const void* buf = &*it;
			return simd::count_utf8(static_cast<const char*>(buf), std::distance(it, ie));
		}
		default:
			break;
	}

	return 0;

#else

	if (iInputWidth == 1)
	{
		std::size_t iCount { 0 };

		for (; KUTF8_LIKELY(std::distance(it, ie) >= 8 && iCount < iMaxCount) ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
			ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
		}

		for (; it < ie && iCount < iMaxCount ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
		}

		return iCount;
	}
	else if (iInputWidth == 2)
	{
		std::size_t iCount { 0 };

		for (; KUTF8_LIKELY(std::distance(it, ie) >= 8 && iCount < iMaxCount) ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
		}

		for (; it < ie && iCount < iMaxCount ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
		}

		return iCount;
	}
	else if (iInputWidth == 4)
	{
		return std::distance(it, ie);
	}

	return 0;

#endif // KUTF8_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF string (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
template<typename UTFString>
KUTF8_CONSTEXPR_14
std::size_t CountUTF(const UTFString& sUTF, std::size_t iMaxCount = std::size_t(-1))
//-----------------------------------------------------------------------------
{
	return CountUTF(sUTF.begin(), sUTF.end());
}

//-----------------------------------------------------------------------------
/// Return codepoint before position it in range ibegin-it, decrement it to point
/// to the begin of the new (previous) codepoint
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t PrevCodepoint(Iterator ibegin, Iterator& it)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		auto iend = it;

		while (KUTF8_LIKELY(it != ibegin))
		{
			// check if this char starts a UTF8 sequence
			auto ch = CodepointCast(*--it);

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
	}
	else if (iInputWidth == 2)
	{
		if (KUTF8_LIKELY(it != ibegin))
		{
			auto iend = it--;

			auto ch = CodepointCast(*it);

			if (IsTrailSurrogate(ch))
			{
				if (KUTF8_LIKELY(it != ibegin))
				{
					--it;
				}
			}
			return Codepoint(it, iend);
		}
	}
	else if (iInputWidth == 4)
	{
		if (KUTF8_LIKELY(it != ibegin))
		{
			auto iend = it--;
			return Codepoint(it, iend);
		}
	}

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Return codepoint after pos UTF8/UTF16/UTF32 codepoints
template<typename Iterator>
KUTF8_CONSTEXPR_14
codepoint_t AtUTF(Iterator it, Iterator ie, std::size_t pos)
//-----------------------------------------------------------------------------
{
	auto it2 = LeftUTF(it, ie, pos);

	if (KUTF8_LIKELY(it2 < ie))
	{
		return Codepoint(it2, ie);
	}
	else
	{
		return INVALID_CODEPOINT;
	}
}

//-----------------------------------------------------------------------------
/// Return codepoint after pos UTF8/UTF16/UTF32 codepoints
template<typename UTFString>
KUTF8_CONSTEXPR_14
codepoint_t AtUTF(const UTFString& sUTF, std::size_t pos)
//-----------------------------------------------------------------------------
{
	return AtUTF(sUTF.begin(), sUTF.end(), pos);
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
template<typename UTF8String>
KUTF8_CONSTEXPR_14
bool HasUTF8(const UTF8String& sUTF8)
//-----------------------------------------------------------------------------
{
	return HasUTF8(sUTF8.begin(), sUTF8.end());
}

//-----------------------------------------------------------------------------
/// Convert range between it and ie from UTF8/UTF16/UTF32, calling functor func for every
/// codepoint (which may be INVALID_CODEPOINT for input parsing errors)
template<typename Iterator, class Functor>
KUTF8_CONSTEXPR_14
bool ForEachUTF(Iterator it, Iterator ie, Functor func)
//-----------------------------------------------------------------------------
{
	for (; KUTF8_LIKELY(it != ie);)
	{
		codepoint_t codepoint = Codepoint(it, ie);

		if (!func(codepoint))
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
/// Convert string from UTF8, calling functor func for every codepoint
template<typename UTFString, class Functor>
KUTF8_CONSTEXPR_14
bool ForEachUTF(const UTFString& sUTF, Functor func)
//-----------------------------------------------------------------------------
{
	return ForEachUTF(sUTF.begin(), sUTF.end(), func);
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32 (from an iterator pair)
template<typename OutType, typename Iterator>
KUTF8_CONSTEXPR_14
bool ConvertUTF(Iterator it, Iterator ie, OutType& sOutput)
//-----------------------------------------------------------------------------
{
#if	KUTF8_WITH_SIMDUTF

	constexpr auto iInputWidth  = sizeof(typename std::remove_reference<decltype(*it)>::type);
	constexpr auto iOutputWidth = sizeof(typename OutType::value_type);

	auto iInputSize = std::distance(it, ie);
	if (iInputSize == 0) return true;

	const void* pIn = &*it;

	if (iInputWidth == iOutputWidth)
	{
		// do a simple memcopy
		auto iOldSize = sOutput.size();
		sOutput.resize(iOldSize + iInputSize);
		// pre C++17 has const .data()
		void* pOut = &sOutput[iOldSize];
		std::memcpy(pOut, pIn, iInputSize * iInputWidth);
		return true;
	}

	// we have to assign a value to satisfy non-C++17
	std::size_t iTargetSize = 0;

	switch (iInputWidth)
	{
		case 4:
			switch (iOutputWidth)
			{
				case 4:
					// need this because clang does not see that the 4 : 4 case is handled by the memcpy branch above
					break;

				case 2:
					iTargetSize = simd::utf16_length_from_utf32(static_cast<const char32_t*>(pIn), iInputSize);
					break;

				case 1:
					iTargetSize = simd::utf8_length_from_utf32(static_cast<const char32_t*>(pIn), iInputSize);
					break;
			}
			break;

		case 2:
			switch (iOutputWidth)
			{
				case 4:
					iTargetSize = simd::utf32_length_from_utf16(static_cast<const char16_t*>(pIn), iInputSize);
					break;

				case 2:
					// need this because clang does not see that the 2 : 2 case is handled by the memcpy branch above
					break;

				case 1:
					iTargetSize = simd::utf8_length_from_utf16(static_cast<const char16_t*>(pIn), iInputSize);
					break;
			}
			break;

		case 1:
			switch (iOutputWidth)
			{
				case 4:
					iTargetSize = simd::utf32_length_from_utf8(static_cast<const char*>(pIn), iInputSize);
					break;

				case 2:
					iTargetSize = simd::utf16_length_from_utf8(static_cast<const char*>(pIn), iInputSize);
					break;

				case 1:
					// need this because clang does not see that the 1 : 1 case is handled by the memcpy branch above
					break;
			}
			break;
	}

	if (!iTargetSize) return false;

	auto iOldSize = sOutput.size();
	sOutput.resize (iOldSize + iTargetSize);
	// pre C++17 has const .data()
	void* pOut = &sOutput[iOldSize];
	// we have to assign a value to satisfy non-C++17
	std::size_t iWrote = 0;

	switch (iInputWidth)
	{
		case 4:
			switch (iOutputWidth)
			{
				case 4:
					break;

				case 2:
					iWrote = simd::convert_utf32_to_utf16(static_cast<const char32_t*>(pIn), iInputSize, static_cast<char16_t*>(pOut));
					break;

				case 1:
					iWrote = simd::convert_utf32_to_utf8(static_cast<const char32_t*>(pIn), iInputSize, static_cast<char*>(pOut));
					break;
			}
			break;

		case 2:
			switch (iOutputWidth)
			{
				case 4:
					iWrote = simd::convert_utf16_to_utf32(static_cast<const char16_t*>(pIn), iInputSize, static_cast<char32_t*>(pOut));
					break;

				case 2:
					break;

				case 1:
					iWrote = simd::convert_utf16_to_utf8(static_cast<const char16_t*>(pIn), iInputSize, static_cast<char*>(pOut));
					break;
			}
			break;

		case 1:
			switch (iOutputWidth)
			{
				case 4:
					iWrote = simd::convert_utf8_to_utf32(static_cast<const char*>(pIn), iInputSize, static_cast<char32_t*>(pOut));
					break;

				case 2:
					iWrote = simd::convert_utf8_to_utf16(static_cast<const char*>(pIn), iInputSize, static_cast<char16_t*>(pOut));
					break;

				case 1:
					break;
			}
			break;
	}

	return iWrote == iTargetSize;

#else // KUTF8_WITH_SIMDUTF

	for (; KUTF8_LIKELY(it != ie);)
	{
		ToUTF(Codepoint(it, ie), sOutput);
	}

	return true;

#endif // KUTF8_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename InpType,
         typename std::enable_if<!std::is_integral<InpType>::value
                              && KUTF8_detail::HasSize<InpType>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
bool ConvertUTF(const InpType& sInput, OutType& sOutput)
//-----------------------------------------------------------------------------
{
	return ConvertUTF(sInput.begin(), sInput.end(), sOutput);
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename Iterator,
         typename std::enable_if<!KUTF8_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF8_CONSTEXPR_14
OutType ConvertUTF(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	OutType sOut{};
	ConvertUTF(it, ie, sOut);
	return sOut;
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename InpType>
KUTF8_CONSTEXPR_14
OutType ConvertUTF(const InpType& sInp)
//-----------------------------------------------------------------------------
{
	return ConvertUTF<OutType>(sInp.begin(), sInp.end());
}

//-----------------------------------------------------------------------------
/// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/UTF16/UTF32 string
template<typename UTFString>
KUTF8_CONSTEXPR_14
bool ConvertUTF(const wchar_t* it, UTFString& sUTF)
//-----------------------------------------------------------------------------
{
	if KUTF8_UNLIKELY(it == nullptr) return false;

	auto iSize = std::wcslen(it);
	return ConvertUTF(it, it + iSize, sUTF);
}

//-----------------------------------------------------------------------------
/// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/UTF16/UTF32 string
template<typename UTFString>
KUTF8_CONSTEXPR_14
UTFString ConvertUTF(const wchar_t* it)
//-----------------------------------------------------------------------------
{
	UTFString sUTF{};
	ConvertUTF(it, sUTF);
	return sUTF;
}

//-----------------------------------------------------------------------------
/// Transform a string in UTF8, UTF16, or UTF32 into another string in UTF8, UTF16, or UTF32 (also mixed), calling a transformation
/// functor for each codepoint.
/// @param sInput the UTF input string
/// @param sOutput the UTF output string
/// @param func the transformation functor, called with signature bool(codepoint_t)
/// @return always true
template<typename UTFString, typename ReturnUTFString, class Functor>
bool TransformUTF(const UTFString& sInput, ReturnUTFString& sOutput, Functor func)
//-----------------------------------------------------------------------------
{
	return ForEachUTF(sInput, [&sOutput, &func](codepoint_t cp)
	{
		ToUTF(func(cp), sOutput);
		return true;
	});
}

//-----------------------------------------------------------------------------
/// Transform a Unicode string into a lowercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
/// @param sInput the UTF8 input string
/// @param sOutput the output UTF8 string
template<typename InputString, typename OutputString>
void ToLowerUTF(const InputString& sInput, OutputString& sOutput)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth  = sizeof(typename InputString::value_type);
	constexpr auto iOutputWidth = sizeof(typename OutputString::value_type);

	if (iOutputWidth == 4)
	{
		using value_type = typename OutputString::value_type;

		if (iInputWidth == 4)
		{
			// we do not need to convert, but will simply transform from input to output
			std::transform(sInput.begin(), sInput.end(), sOutput.begin(), [](value_type cp)
			{
	#ifdef KUTF8_DEKAF2
				return kToLower(cp);
	#else
				return std::towlower(cp);
	#endif
			});
		}
		else
		{
			// we can convert directly into the target
			ConvertUTF(sInput, sOutput);
			// and transform right there
			std::transform(sOutput.begin(), sOutput.end(), sOutput.begin(), [](value_type cp)
			{
	#ifdef KUTF8_DEKAF2
				return kToLower(cp);
	#else
				return std::towlower(cp);
	#endif
			});
		}
	}
	else
	{

#if	KUTF8_WITH_SIMDUTF

		// we convert into an intermediate type and back to the requested output type
		std::u32string sTemp;
		ConvertUTF(sInput, sTemp);
		std::transform(sTemp.begin(), sTemp.end(), sTemp.begin(), [](codepoint_t cp)
		{
	#ifdef KUTF8_DEKAF2
			return kToLower(cp);
	#else
			return std::towlower(cp);
	#endif
		});
		ConvertUTF(sTemp, sOutput);

#else

		sOutput.reserve(sOutput.size() + sInput.size());

		TransformUTF(sInput, sOutput, [](codepoint_t cp)
		{
	#ifdef KUTF8_DEKAF2
			return kToLower(cp);
	#else
			return std::towlower(cp);
	#endif
		});

#endif

	}
}

//-----------------------------------------------------------------------------
/// Transform a Unicode string into an uppercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
/// @param sInput the UTF8 input string
/// @param sOutput the output UTF8 string
template<typename InputString, typename OutputString>
void ToUpperUTF(const InputString& sInput, OutputString& sOutput)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth  = sizeof(typename InputString::value_type);
	constexpr auto iOutputWidth = sizeof(typename OutputString::value_type);

	if (iOutputWidth == 4)
	{
		using value_type = typename OutputString::value_type;

		if (iInputWidth == 4)
		{
			// we do not need to convert, but will simply transform from input to output
			std::transform(sInput.begin(), sInput.end(), sOutput.begin(), [](value_type cp)
			{
	#ifdef KUTF8_DEKAF2
				return kToUpper(cp);
	#else
				return std::towupper(cp);
	#endif
			});
		}
		else
		{
			// we can convert directly into the target
			ConvertUTF(sInput, sOutput);
			// and transform right there
			std::transform(sOutput.begin(), sOutput.end(), sOutput.begin(), [](value_type cp)
			{
	#ifdef KUTF8_DEKAF2
				return kToUpper(cp);
	#else
				return std::towupper(cp);
	#endif
			});
		}
	}
	else
	{

#if	KUTF8_WITH_SIMDUTF

		// we convert into an intermediate type and back to the requested output type
		std::u32string sTemp;
		ConvertUTF(sInput, sTemp);
		std::transform(sTemp.begin(), sTemp.end(), sTemp.begin(), [](codepoint_t cp)
		{
	#ifdef KUTF8_DEKAF2
			return kToUpper(cp);
	#else
			return std::towupper(cp);
	#endif
		});
		ConvertUTF(sTemp, sOutput);

#else

		sOutput.reserve(sOutput.size() + sInput.size());

		TransformUTF(sInput, sOutput, [](codepoint_t cp)
		{
	#ifdef KUTF8_DEKAF2
			return kToUpper(cp);
	#else
			return std::towupper(cp);
	#endif
		});

#endif

	}
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
			ToUTF(REPLACEMENT_CHARACTER, sNarrow);
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
							ToUTF(sp.ToCodepoint(), sNarrow);
						}
						else
						{
							// the second surrogate is not valid
							ToUTF(REPLACEMENT_CHARACTER, sNarrow);
						}
					}
					else
					{
						ToUTF(REPLACEMENT_CHARACTER, sNarrow);
					}
				}
				else
				{
					// we treat incomplete surrogates as simple ucs2
					ToUTF(sp.low, sNarrow);
				}
			}
			else
			{
				ToUTF(sp.low, sNarrow);
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
	using value_type = typename ByteString::value_type;

	ByteString sOut{};
	sOut.reserve(sUTF8String.size() * 2);

	ForEachUTF(sUTF8String, [&sOut](codepoint_t cp)
	{
		if (NeedsSurrogates(cp))
		{
			SurrogatePair sp(cp);
			sOut += static_cast<value_type>(sp.low  >> 8 & 0x0ff);
			sOut += static_cast<value_type>(sp.low       & 0x0ff);
			sOut += static_cast<value_type>(sp.high >> 8 & 0x0ff);
			sOut += static_cast<value_type>(sp.high      & 0x0ff);
		}
		else
		{
			sOut += static_cast<value_type>(cp >>  8 & 0x0ff);
			sOut += static_cast<value_type>(cp       & 0x0ff);
		}
		return true;

	});

	return sOut;
}

} // namespace CESU8

} // namespace KUTF8_NAMESPACE

#ifdef KUTF8_DEKAF2
DEKAF2_NAMESPACE_END
#endif
