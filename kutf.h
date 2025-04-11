/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018-2025, Ridgeware, Inc.
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

/// @file kutf.h
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
/// (nearly) comparable results. When using this header standalone, please #define KUTF_WITH_SIMDUTF 1
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
///
/// This header can also be used standalone, in which case it falls back to the C library functions for case
/// changing instead of the at least 5 times faster dekaf2 kctype.h functions. It still provides a really fast
/// access to UTF conversions and validations, particularly if the simdutf library is linked in.

/*

 This header provides the following methods:

 --- Convert

 /// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32 (from an iterator pair)
 bool Convert(Iterator it, Iterator ie, OutType& sOutput)

 /// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
 bool Convert(const InpType& sInput, OutType& sOutput)

 /// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
 OutType Convert(Iterator it, Iterator ie)

 /// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
 OutType Convert(const InpType& sInp)

 /// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/16/32 string
 bool Convert(const wchar_t* it, OutType& Output)

 /// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/16/32 string
 OutType Convert(const wchar_t* it)

 --- transformation

 /// Convert range between it and ie from UTF8/UTF16/UTF32, calling functor func for every codepoint
 bool ForEach(Iterator it, Iterator ie, Functor func)

 /// Convert string from UTF8/UTF16/UTF32, calling functor func for every codepoint
 bool ForEach(const UTFString& sUTF, Functor func)

 /// Transform a string in UTF8, UTF16, or UTF32 into another string in UTF8, UTF16, or UTF32 (also mixed), calling a transformation
 /// functor for each codepoint.
 bool Transform(Iterator it, Iterator ie, OutType& Output, Functor func)

 /// Transform a string in UTF8, UTF16, or UTF32 into another string in UTF8, UTF16, or UTF32 (also mixed), calling a transformation
 /// functor for each codepoint.
 bool Transform(const InputString& sInput, OutType& Output, Functor func)

 /// Transform a Unicode string into a lowercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
 bool ToLower(Iterator it, Iterator ie, OutType& Output)

 /// Transform a Unicode string into a lowercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
 bool ToLower(const InputString& sInput, OutType& Output)

 /// Transform a Unicode string into an uppercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
 bool ToUpper(Iterator it, Iterator ie, OutType& Output)

 /// Transform a Unicode string into an uppercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
 bool ToUpper(const InputString& sInput, OutType& Output)

 --- ToUTF

 /// Convert a codepoint into a UTF8/16/32 sequence written at iterator it
 void ToUTF(Char codepoint, Iterator& it)

 /// Convert a codepoint into a UTF8/16/32 sequence appended to UTF
 void ToUTF(Char codepoint, UTFContainer& UTF)

 /// Convert a codepoint into a UTF8/16/32 sequence returned as object of type UTFContainer
 UTFContainer ToUTF(Char codepoint)

 --- Latin1ToUTF

 /// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string (from two iterators)
 bool Latin1ToUTF(Iterator it, Iterator ie, UTFContainer& UTF)

 /// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string (from two iterators)
 UTFContainer Latin1ToUTF(Iterator it, Iterator ie)

 /// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string
 bool Latin1ToUTF(const Latin1String& sLatin1, UTFContainer& UTF)

 /// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string
 UTFContainer Latin1ToUTF(const Latin1String& sLatin1)

 --- get codepoint

 /// Returns a 32 bit codepoint from either UTF8, UTF16, or UTF32. If at call (it == ie) -> undefined behaviour
 codepoint_t Codepoint(Iterator& it, Iterator ie)

 /// Return codepoint before position it in range ibegin-it, decrement it to point
 /// to the begin of the new (previous) codepoint
 codepoint_t PrevCodepoint(Iterator ibegin, Iterator& it)

 /// Return next codepoint at position it in UTF8 range it-ie, increment it to point
 /// to the begin of the following codepoint. If at call it == ie -> undefined behavior
 codepoint_t CodepointFromUTF8(Iterator& it, Iterator ie)

 /// Return next codepoint from repeatedly calling a ReadFunc that returns single chars
 codepoint_t CodepointFromUTF8Reader(ReadFunc Read, int eof=-1)

 --- move iterator

 /// if not at the start of a UTF8/16/32 sequence then advance the input iterator until the end of the current UTF sequence
 void Sync(Iterator& it, Iterator ie)

 /// increment the input iterator by n codepoints - this works with UTF8, UTF16, or UTF32
 bool Increment(Iterator& it, Iterator ie, std::size_t n = 1)

 /// decrement the input iterator it by n codepoints - this works with UTF8, UTF16, or UTF32
 bool Decrement(Iterator ibegin, Iterator& it, std::size_t n = 1)

 --- validation

 /// Return iterator at position where a 8 bit string uses invalid ASCII sequences (that is, a value > 0x7f)
 Iterator InvalidASCII(Iterator it, Iterator ie)

 /// Return iterator at position where a 8 bit string uses invalid ASCII sequences (that is, a value > 0x7f)
 std::size_t InvalidASCII(const String& sASCII)

 /// Check if a 8 bit string uses only ASCII codepoints  (that is, values < 0x80)
 bool ValidASCII(Iterator it, Iterator ie)

 /// Check if a UTF string uses only ASCII codepoints  (that is, values < 0x80)
 bool ValidASCII(const String& sASCII)

 /// Return iterator at position where a UTF8/16/32 string uses invalid sequences
 Iterator Invalid(Iterator it, Iterator ie)

 /// Check if a UTF8/16/32 string uses only valid sequences
 bool Valid(Iterator it, Iterator ie)

 /// Check if a UTF8/16/32 string uses only valid sequences
 bool Valid(const UTFString& sUTF)

 /// Check if a UTF8/16/32 string uses only valid sequences
 typename UTFString::size_type Invalid(const UTFString& sUTF)

 /// Returns true if the first non-ASCII codepoint is valid UTF8
 bool HasUTF8(Iterator it, Iterator ie)

 /// Returns true if the first non-ASCII codepoint is valid UTF8
 bool HasUTF8(const UTF8String& sUTF8)

 --- counting

 /// Returns the count of bytes that a UTF representation for a given codepoint would need
 std::size_t UTFChars<std::size_t iWidth = 8>(codepoint_t codepoint)

/// Count number of codepoints in UTF range (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
 std::size_t Count(Iterator it, Iterator ie, std::size_t iMaxCount = std::size_t(-1))

 /// Count number of codepoints in UTF string (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
 std::size_t Count(const UTFContainer& UTF, std::size_t iMaxCount = std::size_t(-1))

 --- substrings, iterator movement

 /// Return iterator after max n UTF8/UTF16/UTF32 codepoints
 Iterator Left(Iterator it, Iterator ie, std::size_t n)

 /// Return string with max n left UTF8/UTF16/UTF32 codepoints in sUTF
 ReturnType Left(const UTFString& sUTF, std::size_t n)

 /// Return iterator max n UTF8/UTF16/UTF32 codepoints before it
 Iterator Right(Iterator ibegin, Iterator it, std::size_t n)

 /// Return string with max n right UTF8/UTF16/UTF32 codepoints in sUTF
 ReturnType Right(const UTFContainer& UTF, std::size_t n)

 /// Return string with max n UTF8/UTF16/UTF32 codepoints in sUTF, starting after pos UTF8/UTF16/UTF32 codepoints
 ReturnType Mid(const UTFContainer& UTF, std::size_t pos, std::size_t n)

 /// Return codepoint after pos UTF8/UTF16/UTF32 codepoints
 codepoint_t At(Iterator it, Iterator ie, std::size_t pos)

 /// Return codepoint after pos UTF8/UTF16/UTF32 codepoints
 codepoint_t At(const UTFContainer& UTF, std::size_t pos)

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

 /// a tiny input iterator implementation that can be constructed around a function which returns characters
 /// until the input is exhausted, in which case the function returns EOF, after which the iterator compares
 /// equal with a default constructed iterator.

 /// constructs the end iterator
 ReadIterator::ReadIterator()
 /// construct with a function that returns the next character or EOF, after which the iterator compares equal to the end iterator
 ReadIterator::ReadIterator(std::function<value_type()> func)

 */

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <iterator>
#include <functional>
#include <cassert>
#include <algorithm> // for std::transform
#include <cstring>   // for std::memcpy

static_assert(__cplusplus >= 201103L, "The UTF code lib needs at least a C++11 compiler");

#if defined(__GNUC__) && __GNUC__ >= 4
	#define KUTF_LIKELY(expression)   (__builtin_expect((expression), 1))
	#define KUTF_UNLIKELY(expression) (__builtin_expect((expression), 0))
#else
	#define KUTF_LIKELY(expression)   (expression)
	#define KUTF_UNLIKELY(expression) (expression)
#endif

#if __cplusplus >= 201402L
	#define KUTF_CONSTEXPR_14 constexpr
#else
	#define KUTF_CONSTEXPR_14 inline
#endif

#if __cplusplus >= 202002L
	#define KUTF_CONSTEXPR_20 constexpr
#else
	#define KUTF_CONSTEXPR_20 inline
#endif

#ifdef __has_include
	#if __has_include("kconfiguration.h") && __has_include("kctype.h")
		#include "kconfiguration.h"
	#endif
#endif

#if defined(DEKAF2) || DEKAF_MAJOR_VERSION >= 2 \
 || (__has_include("kdefinitions.h") && __has_include("kctype.h"))
	#define KUTF_DEKAF2 1
	#ifndef KUTF_WITH_SIMDUTF
		#ifdef DEKAF2_WITH_SIMDUTF
			#define KUTF_WITH_SIMDUTF 1
		#endif
	#endif
#endif

#if KUTF_DEKAF2
	#undef KUTF_NAMESPACE // we need the namespace being the default
	#include "kctype.h"
	#if KUTF_WITH_SIMDUTF
		#include "bits/simd/kutf.h"
	#endif
	DEKAF2_NAMESPACE_BEGIN
#else
	#include <cwctype>
	#if KUTF_WITH_SIMDUTF
		// make sure this include is found at compile time, and do not forget
		// to link against libsimdutf.a
		#include <simdutf.h>
		namespace simd = ::simdutf;
	#endif
#endif

#ifndef KUTF_NAMESPACE
	#define KUTF_NAMESPACE kutf
#endif

namespace KUTF_NAMESPACE {

#ifdef __cpp_unicode_characters
	#ifndef KUTF_DEKAF2
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
	#ifndef KUTF_DEKAF2
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
/// Returns true if the given Unicode codepoint is valid (= < 0x0110000, and not in 0x0d800 .. 0x0dfff)
inline constexpr
bool IsValid(codepoint_t codepoint)
//-----------------------------------------------------------------------------
{
	return codepoint <= CODEPOINT_MAX && !IsSurrogate(codepoint);
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
	static_assert(std::is_integral<Char>::value, "can only cast integral types");

	return (sizeof(Char) == 1) ? static_cast<utf8_t>(ch)
		:  (sizeof(Char) == 2) ? static_cast<utf16_t>(ch)
		:  static_cast<utf32_t>(ch);
}

//-----------------------------------------------------------------------------
/// Returns the count of characters that a UTF representation for a given codepoint would need for the chosen
/// UTF bit width (8/16/32)
/// @param template param iWidth: the target UTF width in bytes or bits, defaults to 8 (=UTF8)
/// @param codepoint the codepoint to check for its output count
/// @return the count of output units needed to represent the given codepoint
template<std::size_t iWidth = 8>
KUTF_CONSTEXPR_14
std::size_t UTFChars(codepoint_t codepoint)
//-----------------------------------------------------------------------------
{
	if (iWidth == 8)
	{
		if (codepoint < 0x0080)
		{
			return 1;
		}
		else if (codepoint < 0x0800)
		{
			return 2;
		}
		else if (codepoint < 0x010000)
		{
			// we return a squared question mark 0xfffd (REPLACEMENT CHARACTER)
			// for invalid Unicode codepoints, so the bytecount is 3 as well if
			// this is a surrogate character
			return 3;
		}
		else if (codepoint < 0x0110000)
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
	else if (iWidth == 16 && NeedsSurrogates(codepoint))
	{
		return 2;
	}
	else
	{
		return 1;
	}
}

namespace KUTF_detail {

#if !defined __clang__ && defined __GNUC__ && __GNUC__ < 9
// GCC < 9 erroneously see the .size() member return value not being used,
// although it is in a pure SFINAE context. Using a string class which has
// [[nodiscard]] set for the size() member would cause a build failure.
// Hence we disable the check for this class.
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

} // end of namespace KUTF_detail

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8/UTF16/UTF32 sequence written at iterator it
/// @param codepoint the codepoint to write
/// @param it the output iterator to write at, possibly a back_insert_iterator
template<typename output_value_type, typename Char, typename Iterator,
         typename std::enable_if<std::is_integral<Char>::value
                                 && !KUTF_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF_CONSTEXPR_14
void ToUTF(Char codepoint, Iterator it)
//-----------------------------------------------------------------------------
{
	constexpr std::size_t iOutputWidth = sizeof(output_value_type);

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
			if (KUTF_UNLIKELY(IsSurrogate(cp)))
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
			ToUTF<output_value_type>(REPLACEMENT_CHARACTER, it);
		}
	}
	else if (iOutputWidth == 2 && NeedsSurrogates(cp))
	{
		SurrogatePair sp(cp);
		*it++ = static_cast<output_value_type>(sp.low);
		*it++ = static_cast<output_value_type>(sp.high);
	}
	else
	{
		if (KUTF_UNLIKELY(!IsValid(cp)))
		{
			cp = REPLACEMENT_CHARACTER;
		}
		*it++ = static_cast<output_value_type>(cp);
	}
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8/UTF16/UTF32 sequence written at back_insert_iterator it
/// @param codepoint the codepoint to write
/// @param it the back_insert_iterator to write at
template<typename Char, typename Iterator,
         typename std::enable_if<std::is_integral<Char>::value
                                 && std::is_integral<typename Iterator::container_type::value_type>::value
                                 && !KUTF_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF_CONSTEXPR_14
void ToUTF(Char codepoint, Iterator it)
//-----------------------------------------------------------------------------
{
	ToUTF<typename Iterator::container_type::value_type, Char, Iterator>(codepoint, it);
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8/UTF16/UTF32 sequence written at iterator it
/// @param codepoint the codepoint to write
/// @param it the output iterator to write at
template<typename Char, typename Iterator,
         typename std::enable_if<std::is_integral<Char>::value
                                 && std::is_integral<typename Iterator::value_type>::value
                                 && !KUTF_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF_CONSTEXPR_14
void ToUTF(Char codepoint, Iterator it)
//-----------------------------------------------------------------------------
{
	ToUTF<typename Iterator::value_type, Char, Iterator>(codepoint, it);
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF8/UTF16/UTF32 sequence appended to UTF
template<typename Char, typename UTFContainer,
         typename std::enable_if<std::is_integral<Char>::value
                                 && KUTF_detail::HasSize<UTFContainer>::value, int>::type = 0>
KUTF_CONSTEXPR_14
void ToUTF(Char codepoint, UTFContainer& UTF)
/// @param codepoint the codepoint to write
/// @param UTF the output container to write into (typically a string type)
//-----------------------------------------------------------------------------
{
	ToUTF(codepoint, std::back_inserter(UTF));
}

//-----------------------------------------------------------------------------
/// Convert a codepoint into a UTF sequence returned as string of type UTFString.
/// @param codepoint the codepoint to write
/// @returns the output container written into (typically a string type)
template<typename UTFContainer, typename Char,
         typename std::enable_if<std::is_integral<Char>::value, int>::type = 0>
KUTF_CONSTEXPR_14
UTFContainer ToUTF(Char codepoint)
//-----------------------------------------------------------------------------
{
	UTFContainer UTF{};
	ToUTF(codepoint, std::back_inserter(UTF));
	return UTF;
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string (from two iterators)
/// @param it the start of an iterator range to read from
/// @param ie the end of an iterator range to read from
/// @param UTF the ouput container to write into (typically a string type)
/// @returns false if the input was not Latin1, true otherwise
template<typename UTFContainer, typename Iterator>
KUTF_CONSTEXPR_14
bool Latin1ToUTF(Iterator it, Iterator ie, UTFContainer& UTF)
//-----------------------------------------------------------------------------
{
	using input_value_type = typename std::remove_reference<decltype(*it)>::type;
	constexpr auto iInputWidth = sizeof(input_value_type);

	static_assert(iInputWidth == 1, "supporting only 8 bit strings in Latin1ToUTF");

#if KUTF_WITH_SIMDUTF

	constexpr std::size_t iOutputWidth = sizeof(typename UTFContainer::value_type);

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

	auto iOldSize = UTF.size();
	UTF.resize (iOldSize + iTargetSize);
	// pre C++17 has const .data(), so we take a ref on the first element
	void* pOut = &UTF[iOldSize];
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

	auto bi = std::back_inserter(UTF);

	for (; KUTF_LIKELY(it != ie); ++it)
	{
		ToUTF(*it, bi);
	}

	return true;

#endif // KUTF_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string (from two iterators)
/// @param it the start of an iterator range to read from
/// @param ie the end of an iterator range to read from
/// @returns the ouput container written into (typically a string type)
template<typename UTFContainer, typename Iterator,
         typename std::enable_if<!KUTF_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF_CONSTEXPR_14
UTFContainer Latin1ToUTF(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	UTFContainer UTF{};
	Latin1ToUTF(it, ie, UTF);
	return UTF;
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string
/// @param sLatin1 the input string
/// @param UTF the ouput container to write into (typically a string type)
/// @returns false if the input was not Latin1, true otherwise
template<typename UTFContainer, typename Latin1String,
         typename std::enable_if<!std::is_integral<Latin1String>::value
                              && KUTF_detail::HasSize<Latin1String>::value, int>::type = 0>
KUTF_CONSTEXPR_14
bool Latin1ToUTF(const Latin1String& sLatin1, UTFContainer& UTF)
//-----------------------------------------------------------------------------
{
	return Latin1ToUTF(sLatin1.begin(), sLatin1.end(), UTF);
}

//-----------------------------------------------------------------------------
/// Convert a latin1 encoded string into a UTF8/UTF16/UTF32 string
/// @param sLatin1 the input string
/// @returns the ouput container written into (typically a string type)
template<typename UTFContainer, typename Latin1String>
KUTF_CONSTEXPR_14
UTFContainer Latin1ToUTF(const Latin1String& sLatin1)
//-----------------------------------------------------------------------------
{
	UTFContainer UTF{};
	Latin1ToUTF(sLatin1, UTF);
	return UTF;
}

//-----------------------------------------------------------------------------
/// if not at the start of a UTF sequence then advance the input iterator until the end of the current UTF sequence
/// @param it the iterator that shall be moved forward until the start of a UTF sequence
/// @param ie the end iterator
template<typename Iterator>
KUTF_CONSTEXPR_14
void Sync(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		for (; it != ie; ++it)
		{
			if (!IsContinuationByte(CodepointCast(*it)))
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
/// Return next codepoint at position it in range it-ie, increment it to point
/// to the begin of the following codepoint. If at time of call it == ie -> undefined behavior
/// @param it the start of an iterator range to read from
/// @param ie the end of an iterator range to read from
/// @returns an assembled codepoint
template<typename Iterator>
KUTF_CONSTEXPR_14
codepoint_t CodepointFromUTF8(Iterator& it, Iterator ie)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	assert(it != ie);

	auto ch = CodepointCast(*it); ++it;

	if (ch < 128) return ch;

	if (iInputWidth > 1 && ch > 0x0ff)
	{
		// error, even with char sizes > one byte UTF8 single
		// values cannot exceed 255 (with the exception of EOF)
		return (ch == CodepointCast(EOF)) ? END_OF_INPUT : INVALID_CODEPOINT;
	}

	do
	{
		if ((ch & 0x0e0) == 0x0c0)
		{
			codepoint_t codepoint = (ch & 0x01f) << 6;

			if (it == ie) break;
			auto ch1 = CodepointCast(*it); ++it;

			// a UTF8 sequence cannot contain characters > 0xf4
			if (!IsContinuationByte(ch1)) break;

			codepoint |= (ch1 & 0x03f);

			// lower limit, protect from ambiguous encoding
			if (codepoint < 0x080) break;

			return codepoint; // valid
		}
		else if ((ch & 0x0f0) == 0x0e0)
		{
			codepoint_t codepoint    = (ch & 0x0f) << 12;
			bool bCheckForSurrogates = ch == 0xbd;

			if (it == ie) break;
			auto ch1 = CodepointCast(*it); ++it;
			if (it == ie) break;
			auto ch2 = CodepointCast(*it); ++it;

			if (!IsContinuationByte(ch1)) break;
			if (!IsContinuationByte(ch2)) break;

			ch1 &= 0x03f;
			ch2 &= 0x03f;

			codepoint |= ch2;
			codepoint |= ch1 << 6;

			// lower limit, protect from ambiguous encoding
			if (codepoint < 0x0800) break;
			if (bCheckForSurrogates && IsSurrogate(codepoint)) break;

			return codepoint; // valid
		}
		else if ((ch & 0x0f8) == 0x0f0)
		{
			// do not check for too large lead byte values at this place
			// (ch >= 0x0f5) as apparently that deranges the pipeline.
			// Testing the final codepoint value below is about 10% faster.
			codepoint_t codepoint = (ch & 0x07) << 18;

			if (it == ie) break;
			auto ch1 = CodepointCast(*it); ++it;
			if (it == ie) break;
			auto ch2 = CodepointCast(*it); ++it;
			if (it == ie) break;
			auto ch3 = CodepointCast(*it); ++it;

			if (!IsContinuationByte(ch1)) break;
			if (!IsContinuationByte(ch2)) break;
			if (!IsContinuationByte(ch3)) break;

			ch1 &= 0x03f;
			ch2 &= 0x03f;
			ch3 &= 0x03f;

			codepoint |= ch3;
			codepoint |= ch2 <<  6;
			codepoint |= ch1 << 12;

			// lower limit, protect from ambiguous encoding
			if (codepoint < 0x010000) break;
			if (codepoint > CODEPOINT_MAX) break;

			return codepoint; // valid
		}

	} while (false);

	Sync(it, ie);

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Returns a 32 bit codepoint from either UTF8, UTF16, or UTF32. If at call (it == ie) -> undefined behaviour
/// @param it the start of an iterator range to read from
/// @param ie the end of an iterator range to read from
/// @returns an assembled codepoint
template<typename Iterator>
KUTF_CONSTEXPR_14
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

		if (IsSurrogate(ch))
		{
			if (IsLeadSurrogate(ch))
			{
				SurrogatePair sp;

				sp.low = ch;

				if (it == ie)
				{
					// this is an incomplete surrogate
					return INVALID_CODEPOINT;
				}
				else
				{
					sp.high = CodepointCast(*it);

					if (!IsTrailSurrogate(sp.high))
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

		return (IsValid(cp)) ? cp : INVALID_CODEPOINT;
	}
}

//-----------------------------------------------------------------------------
/// Return iterator at position where a 8 bit string uses invalid ASCII sequences (that is, a value > 0x7f)
template<typename Iterator>
KUTF_CONSTEXPR_14
Iterator InvalidASCII(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);
	static_assert(iInputWidth == 1, "supporting only 8 bit strings in ASCII validation");

#if KUTF_WITH_SIMDUTF

	const void* buf = &*it;
	auto res = simd::validate_ascii_with_errors(static_cast<const char*>(buf), std::distance(it, ie));
	return (res.error == simd::SUCCESS) ? ie : it + res.count;

#else

	for (; std::distance(it, ie) >= 16; )
	{
		// credits: SWAR algorithm adapted from simdutf scalar implementation, license: MIT
#if __cplusplus >= 202002L
		uint64_t v1;
#else
		uint64_t v1 { 0 };
#endif
		std::memcpy(&v1, &*it, 8);
		it += 8;
#if __cplusplus >= 202002L
		uint64_t v2;
#else
		uint64_t v2 { 0 };
#endif
		std::memcpy(&v2, &*it, 8);
		it += 8;
		uint64_t v{v1 | v2};

		if ((v & 0x8080808080808080) != 0)
		{
			it -= 16;
			for (; it != ie; ++it)
			{
				if (CodepointCast(*it) >= 0x080) return it;
			}
		}
	}

	for (; it != ie; ++it)
	{
		if (CodepointCast(*it) >= 0x080) return it;
	}

	return ie;

#endif // KUTF_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Check if a 8 bit string uses only ASCII codepoints  (that is, values < 0x80)
template<typename Iterator>
KUTF_CONSTEXPR_14
bool ValidASCII(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	return InvalidASCII(it, ie) == ie;
}

//-----------------------------------------------------------------------------
/// Return iterator at position where a 8 bit string uses invalid ASCII sequences (that is, a value > 0x7f)
template<typename String>
KUTF_CONSTEXPR_14
std::size_t InvalidASCII(const String& sASCII)
//-----------------------------------------------------------------------------
{
	auto it = InvalidASCII(sASCII.begin(), sASCII.end());
	return (it == sASCII.end()) ? std::size_t(-1) : it - sASCII.begin();
}

//-----------------------------------------------------------------------------
/// Check if a UTF string uses only ASCII codepoints  (that is, values < 0x80)
template<typename String>
KUTF_CONSTEXPR_14
bool ValidASCII(const String& sASCII)
//-----------------------------------------------------------------------------
{
	return ValidASCII(sASCII.begin(), sASCII.end());
}

//-----------------------------------------------------------------------------
/// Return iterator at position where a UTF string uses invalid sequences (either of UTF8, UTF16, UTF32)
template<typename Iterator>
KUTF_CONSTEXPR_14
Iterator Invalid(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
#if KUTF_WITH_SIMDUTF

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
	}

	return it;

#else

	for (; it != ie; )
	{
		auto ti = it;

		if (KUTF_UNLIKELY(Codepoint(it, ie) == INVALID_CODEPOINT)) return ti;
	}

	return it;

#endif // KUTF_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Check if a UTF string uses only valid sequences (either of UTF8, UTF16, UTF32)
template<typename Iterator>
KUTF_CONSTEXPR_14
bool Valid(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
#if KUTF_WITH_SIMDUTF

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
	}

	return false;

#else

	return Invalid(it, ie) == ie;

#endif // KUTF_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Check if a UTF string uses only valid sequences (either of UTF8, UTF16, UTF32)
template<typename UTFContainer>
KUTF_CONSTEXPR_14
bool Valid(const UTFContainer& UTF)
//-----------------------------------------------------------------------------
{
	return Valid(UTF.begin(), UTF.end());
}

//-----------------------------------------------------------------------------
/// Check if a UTF string uses only valid sequences (either of UTF8, UTF16, UTF32)
/// @return position of first invalid sequence, or npos if not found
template<typename UTFContainer>
KUTF_CONSTEXPR_14
std::size_t Invalid(const UTFContainer& UTF)
//-----------------------------------------------------------------------------
{
	auto it = Invalid(UTF.begin(), UTF.end());
	return (it == UTF.end()) ? std::size_t(-1) : it - UTF.begin();
}

//-----------------------------------------------------------------------------
/// Return iterator after max n UTF8/UTF16/UTF32 codepoints
template<typename Iterator>
KUTF_CONSTEXPR_14
Iterator Left(Iterator it, Iterator ie, std::size_t n)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		for (; KUTF_LIKELY(n >= 8 && std::distance(it, ie) >= 8) ;)
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

		for (; it != ie && n > 0 ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			n -= !IsContinuationByte(ch);
		}

		for (; it != ie; ++it)
		{
			codepoint_t ch = CodepointCast(*it);
			if (!IsContinuationByte(ch)) break;
		}
	}
	else if (iInputWidth == 2)
	{
		for (; KUTF_LIKELY(n >= 4 && std::distance(it, ie) >= 4) ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
		}

		for (; it != ie && n > 0 ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			n -= !IsLeadSurrogate(ch);
		}
	}
	else if (iInputWidth == 4)
	{
		it += std::min(static_cast<std::size_t>(std::distance(it, ie)), n);
	}

	return it;
}

//-----------------------------------------------------------------------------
/// Return string with max n left UTF8 codepoints in sNarrow
template<typename UTFContainer, typename ReturnType = UTFContainer>
KUTF_CONSTEXPR_14
ReturnType Left(const UTFContainer& UTF, std::size_t n)
//-----------------------------------------------------------------------------
{
	auto it = Left(UTF.begin(), UTF.end(), n);
	return ReturnType(UTF.data(), it - UTF.begin());
}

//-----------------------------------------------------------------------------
/// decrement the input iterator it by n codepoints - this works with UTF8, UTF16, or UTF32
/// @param ibegin iterator that points to the start of the string.
/// @param it iterator that points to the current position in the string. Will be reduced to the new position.
/// @param n the number of unicode codepoints to advanve for iterator it (default = 1)
/// @return true if it coult be decremented by n codepoints, else false (input string exhausted)
template<typename Iterator>
KUTF_CONSTEXPR_14
bool Decrement(Iterator ibegin, Iterator& it, std::size_t n = 1)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		for (; KUTF_LIKELY(n >= 8 && std::distance(ibegin, it) >= 8) ;)
		{
			codepoint_t ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
			ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
		}

		for (; KUTF_LIKELY(n > 0 && ibegin != it) ;)
		{
			// check if this char starts a UTF8 sequence
			codepoint_t ch = CodepointCast(*--it);
			n -= !IsContinuationByte(ch);
		}
	}
	else if (iInputWidth == 2)
	{
		for (; KUTF_LIKELY(n >= 4 && std::distance(ibegin, it) >= 4) ;)
		{
			codepoint_t ch = CodepointCast(*--it);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--it);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--it);
			n -= !IsTrailSurrogate(ch);
			ch = CodepointCast(*--it);
			n -= !IsTrailSurrogate(ch);
		}
		for (; ibegin != it && n > 0 ;)
		{
			codepoint_t ch = CodepointCast(*--it);
			n -= !IsTrailSurrogate(ch);
		}
	}
	else if (iInputWidth == 4)
	{
		auto iCount = std::min(static_cast<std::size_t>(std::distance(ibegin, it)), n);
		it -= iCount;
		n  -= iCount;
	}

	return !n;
}

//-----------------------------------------------------------------------------
/// Return iterator max n UTF8/UTF16/UTF32 codepoints before it
template<typename Iterator>
KUTF_CONSTEXPR_14
Iterator Right(Iterator ibegin, Iterator it, std::size_t n)
//-----------------------------------------------------------------------------
{
	Decrement(ibegin, it, n);
	return it;
}

//-----------------------------------------------------------------------------
/// Return string with max n right UTF8/UTF16/UTF32 codepoints in sNarrow
template<typename UTFContainer, typename ReturnType = UTFContainer>
KUTF_CONSTEXPR_14
ReturnType Right(const UTFContainer& UTF, std::size_t n)
//-----------------------------------------------------------------------------
{
	auto it = Right(UTF.begin(), UTF.end(), n);
	return ReturnType(UTF.data() + (it - UTF.begin()), UTF.end() - it);
}

//-----------------------------------------------------------------------------
/// Return string with max n UTF8/UTF16/UTF32 codepoints in sNarrow, starting after pos UTF8/UTF16/UTF32 codepoints
template<typename UTFContainer, typename ReturnType = UTFContainer>
KUTF_CONSTEXPR_14
ReturnType Mid(const UTFContainer& UTF, std::size_t pos, std::size_t n)
//-----------------------------------------------------------------------------
{
	auto it = Left(UTF.begin(), UTF.end(), pos);
	auto ie = Left(it, UTF.end(), n);
	return ReturnType(UTF.data() + (it - UTF.begin()), ie - it);
}

//-----------------------------------------------------------------------------
/// increment the input iterator by n codepoints - this works with UTF8, UTF16, or UTF32
/// @param it iterator that points to the current position in the string. Will be incremented to the new position.
/// @param ie iterator that points to the end of the string. If on return it == ie the string was exhausted.
/// @param n the number of unicode codepoints to advanve for iterator it (default = 1)
/// @return true if it != ie, else false (input string exhausted)
template<typename Iterator>
KUTF_CONSTEXPR_14
bool Increment(Iterator& it, Iterator ie, std::size_t n = 1)
//-----------------------------------------------------------------------------
{
	it = Left(it, ie, n);
	return it != ie;
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF range (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
template<typename Iterator>
KUTF_CONSTEXPR_14
std::size_t Count(Iterator it, Iterator ie, std::size_t iMaxCount = std::size_t(-1))
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

#if KUTF_WITH_SIMDUTF

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
				Sync(ie, ie + 1);
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
				Sync(ie, ie + 4);
			}
			const void* buf = &*it;
			return simd::count_utf8(static_cast<const char*>(buf), std::distance(it, ie));
		}
	}

	return 0;

#else

	if (iInputWidth == 1)
	{
		std::size_t iCount { 0 };

		for (; KUTF_LIKELY(std::distance(it, ie) >= 8 && iCount < iMaxCount) ;)
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

		for (; it != ie && iCount < iMaxCount ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			iCount += !IsContinuationByte(ch);
		}

		return iCount;
	}
	else if (iInputWidth == 2)
	{
		std::size_t iCount { 0 };

		for (; KUTF_LIKELY(std::distance(it, ie) >= 4 && iCount < iMaxCount) ;)
		{
			codepoint_t ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
			ch = CodepointCast(*it++);
			iCount += !IsLeadSurrogate(ch);
		}

		for (; it != ie && iCount < iMaxCount ;)
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

#endif // KUTF_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Count number of codepoints in UTF container (either of UTF8, UTF16, UTF32), stop at iMaxCount or some more
template<typename UTFContainer>
KUTF_CONSTEXPR_14
std::size_t Count(const UTFContainer& UTF, std::size_t iMaxCount = std::size_t(-1))
//-----------------------------------------------------------------------------
{
	return Count(UTF.begin(), UTF.end());
}

//-----------------------------------------------------------------------------
/// Return codepoint before position it in range ibegin-it, decrement it to point
/// to the begin of the new (previous) codepoint
template<typename Iterator>
KUTF_CONSTEXPR_14
codepoint_t PrevCodepoint(Iterator ibegin, Iterator& it)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 1)
	{
		auto iend = it;

		while (KUTF_LIKELY(it != ibegin))
		{
			// check if this char starts a UTF8 sequence
			auto ch = CodepointCast(*--it);

			if (KUTF_LIKELY(ch < 128))
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
		if (KUTF_LIKELY(it != ibegin))
		{
			auto iend = it--;

			auto ch = CodepointCast(*it);

			if (IsTrailSurrogate(ch))
			{
				if (KUTF_LIKELY(it != ibegin))
				{
					--it;
				}
			}
			return Codepoint(it, iend);
		}
	}
	else if (iInputWidth == 4)
	{
		if (KUTF_LIKELY(it != ibegin))
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
KUTF_CONSTEXPR_14
codepoint_t At(Iterator it, Iterator ie, std::size_t pos)
//-----------------------------------------------------------------------------
{
	return Increment(it, ie, pos) ? Codepoint(it, ie) : INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Return codepoint after pos UTF8/UTF16/UTF32 codepoints
template<typename UTFContainer>
KUTF_CONSTEXPR_14
codepoint_t At(const UTFContainer& UTF, std::size_t pos)
//-----------------------------------------------------------------------------
{
	return At(UTF.begin(), UTF.end(), pos);
}

//-----------------------------------------------------------------------------
/// Returns true if the first non-ASCII codepoint is valid UTF8
template<typename Iterator>
KUTF_CONSTEXPR_14
bool HasUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	it = InvalidASCII(it, ie);
	if (it == ie) return false;
	return CodepointFromUTF8(it, ie) != INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
/// Returns true if the first non-ASCII codepoint is valid UTF8
template<typename UTF8Container>
KUTF_CONSTEXPR_14
bool HasUTF8(const UTF8Container& UTF8)
//-----------------------------------------------------------------------------
{
	return HasUTF8(UTF8.begin(), UTF8.end());
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32 (from an iterator pair)
template<typename OutType, typename Iterator>
KUTF_CONSTEXPR_14
bool Convert(Iterator it, Iterator ie, OutType& Output)
//-----------------------------------------------------------------------------
{
#if KUTF_WITH_SIMDUTF

	constexpr auto iInputWidth  = sizeof(typename std::remove_reference<decltype(*it)>::type);
	constexpr auto iOutputWidth = sizeof(typename OutType::value_type);

	auto iInputSize = std::distance(it, ie);
	if (iInputSize == 0) return true;

	const void* pIn = &*it;

	if (iInputWidth == iOutputWidth)
	{
		// do a simple memcopy
		auto iOldSize = Output.size();
		Output.resize(iOldSize + iInputSize);
		// pre C++17 has const .data()
		void* pOut = &Output[iOldSize];
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

	auto iOldSize = Output.size();
	Output.resize (iOldSize + iTargetSize);
	// pre C++17 has const .data()
	void* pOut = &Output[iOldSize];
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

#else // KUTF_WITH_SIMDUTF

	auto bi = std::back_inserter(Output);

	for (; it != ie;)
	{
		ToUTF(Codepoint(it, ie), bi);
	}

	return true;

#endif // KUTF_WITH_SIMDUTF
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename InpType,
         typename std::enable_if<!std::is_integral<InpType>::value
                              && KUTF_detail::HasSize<InpType>::value, int>::type = 0>
KUTF_CONSTEXPR_14
bool Convert(const InpType& sInput, OutType& Output)
//-----------------------------------------------------------------------------
{
	return Convert(sInput.begin(), sInput.end(), Output);
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename Iterator,
         typename std::enable_if<!KUTF_detail::HasSize<Iterator>::value, int>::type = 0>
KUTF_CONSTEXPR_14
OutType Convert(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	OutType Out{};
	Convert(it, ie, Out);
	return Out;
}

//-----------------------------------------------------------------------------
/// Convert any string in UTF8, UTF16, or UTF32 into any string in UTF8, UTF16, or UTF32
template<typename OutType, typename InpType>
KUTF_CONSTEXPR_14
OutType Convert(const InpType& sInp)
//-----------------------------------------------------------------------------
{
	return Convert<OutType>(sInp.begin(), sInp.end());
}

//-----------------------------------------------------------------------------
/// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/UTF16/UTF32 string
template<typename OutType>
KUTF_CONSTEXPR_14
bool Convert(const wchar_t* it, OutType& Output)
//-----------------------------------------------------------------------------
{
	if KUTF_UNLIKELY(it == nullptr) return false;

	auto iSize = std::wcslen(it);
	return Convert(it, it + iSize, Output);
}

//-----------------------------------------------------------------------------
/// Convert a wchar_t string (UTF16 or UTF32) into a UTF8/UTF16/UTF32 string
template<typename OutType>
KUTF_CONSTEXPR_14
OutType Convert(const wchar_t* it)
//-----------------------------------------------------------------------------
{
	OutType Out{};
	Convert(it, Out);
	return Out;
}

//-----------------------------------------------------------------------------
/// Convert range between it and ie from UTF8/UTF16/UTF32, calling functor func for every
/// codepoint (which may be INVALID_CODEPOINT for input parsing errors). If the functor returns
/// false the loop is aborted.
template<typename Iterator, class Functor>
bool ForEach(Iterator it, Iterator ie, Functor func)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth = sizeof(typename std::remove_reference<decltype(*it)>::type);

	if (iInputWidth == 4)
	{
		// we cannot use std::for_each here, because existing code expects this loop to
		// abort once false is returned by func() (std::for_each ignores all functor returns)
		for (; it != ie; )
		{
			if (!func(*it++)) return false;
		}
		return true;
	}
	else
	{
#if KUTF_WITH_SIMDUTF
		// we convert into an intermediate type
		// do it in chunks to limit memory consumption for large strings
		std::u32string sTemp;
		for (; it != ie; )
		{
			constexpr std::size_t ChunkSize = 1000;

			auto ie2 = std::min(ie, it + ChunkSize);
			Sync(ie2, ie);

			if (!Convert(it, ie2, sTemp)) return false;

			// we cannot use std::for_each here, because existing code expects this loop to
			// abort once false is returned by func() (std::for_each ignores all functor returns)
			for (const auto ch : sTemp)
			{
				if (!func(ch)) return false;
			}

			it = ie2;
			sTemp.clear();
		}
		return true;
#else
		for (; it != ie;)
		{
			if (!func(Codepoint(it, ie))) return false;
		}

		return true;
#endif
	}

	return false;
}

//-----------------------------------------------------------------------------
/// Convert string from UTF8/16/32, calling functor func for every codepoint (which may be
/// INVALID_CODEPOINT for input parsing errors). If the functor returns false the loop is aborted.
template<typename UTFString, class Functor>
bool ForEach(const UTFString& sUTF, Functor func)
//-----------------------------------------------------------------------------
{
	return ForEach(sUTF.begin(), sUTF.end(), func);
}

//-----------------------------------------------------------------------------
/// Transform a string in UTF8, UTF16, or UTF32 into another string in UTF8, UTF16, or UTF32 (also mixed), calling a transformation
/// functor for each codepoint.
/// @param it the begin iterator of the input string
/// @param ie the end iterator of the input string
/// @param Output the UTF output container (typically a string type), will be appended to
/// @param func the transformation functor, called with signature codepoint_t(codepoint_t) - has to return transformed codepoint_t
/// @return false in case of decoding errors, else true
template<typename OutType, typename Iterator, class Functor>
bool Transform(Iterator it, Iterator ie, OutType& Output, Functor func)
//-----------------------------------------------------------------------------
{
	constexpr auto iInputWidth  = sizeof(typename std::remove_reference<decltype(*it)>::type);
	constexpr auto iOutputWidth = sizeof(typename OutType::value_type);

	if (iOutputWidth == 4)
	{
		using value_type = typename OutType::value_type;

		if (iInputWidth == 4)
		{
			// we do not need to convert, but will simply transform from input to output
			auto iOriginalSize = Output.size();
			Output.resize(iOriginalSize + std::distance(it, ie));
			std::transform(it, ie, &Output[iOriginalSize], [&func](value_type cp) { return func(cp); });
		}
		else
		{
			// we can convert directly into the target
			if (!Convert(it, ie, Output)) return false;
			// and transform right there
			std::transform(Output.begin(), Output.end(), Output.begin(), [&func](value_type cp) { return func(cp); });
		}
		return true;
	}
	else
	{
#if KUTF_WITH_SIMDUTF
		// we convert into an intermediate type and back to the requested output type
		// do it in chunks to limit memory consumption for large strings
		std::u32string sTemp;
		for (; it != ie; )
		{
			constexpr std::size_t ChunkSize = 1000;

			auto ie2 = std::min(ie, it + ChunkSize);

			Sync(ie2, ie);

			if (!Convert(it, ie2, sTemp)) return false;

			std::transform(sTemp.begin(), sTemp.end(), sTemp.begin(), [&func](codepoint_t cp) { return func(cp); });

			if (!Convert(sTemp, Output)) return false;

			it = ie2;
			sTemp.clear();
		}
		return true;
#else
		return ForEach(it, ie, [&Output, &func](codepoint_t cp) { ToUTF(func(cp), Output); return true; });
#endif
	}
}

//-----------------------------------------------------------------------------
/// Transform a string in UTF8, UTF16, or UTF32 into another string in UTF8, UTF16, or UTF32 (also mixed), calling a transformation
/// functor for each codepoint.
/// @param sInput the UTF input string
/// @param Output the UTF output container (typically a string type), will be appended to
/// @param func the transformation functor, called with signature codepoint_t(codepoint_t) - has to return transformed codepoint_t
/// @return false in case of decoding errors, else true
template<typename OutType, typename InputString, class Functor>
bool Transform(const InputString& sInput, OutType& Output, Functor func)
//-----------------------------------------------------------------------------
{
	return Transform(sInput.begin(), sInput.end(), Output, func);
}

//-----------------------------------------------------------------------------
/// Transform a Unicode string into a lowercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
/// @param it the begin iterator of the input string
/// @param ie the end iterator of the input string
/// @param Output the UTF output container (typically a string type), will be appended to
template<typename OutType, typename Iterator>
bool ToLower(Iterator it, Iterator ie, OutType& Output)
//-----------------------------------------------------------------------------
{
	return Transform(it, ie, Output, [](codepoint_t cp)
	{
#ifdef KUTF_DEKAF2
		return kToLower(cp);
#else
		return std::towlower(cp);
#endif
	});
}

//-----------------------------------------------------------------------------
/// Transform a Unicode string into a lowercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
/// @param sInput the UTF8 input string
/// @param Output the UTF output container (typically a string type), will be appended to
template<typename OutType, typename InputString>
bool ToLower(const InputString& sInput, OutType& Output)
//-----------------------------------------------------------------------------
{
	return ToLower(sInput.begin(), sInput.end(), Output);
}

//-----------------------------------------------------------------------------
/// Transform a Unicode string into an uppercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
/// @param it the begin iterator of the input string
/// @param ie the end iterator of the input string
/// @param Output the UTF output container (typically a string type), will be appended to
template<typename OutType, typename Iterator>
bool ToUpper(Iterator it, Iterator ie, OutType& Output)
//-----------------------------------------------------------------------------
{
	return Transform(it, ie, Output, [](codepoint_t cp)
	{
#ifdef KUTF_DEKAF2
		return kToUpper(cp);
#else
		return std::towupper(cp);
#endif
	});
}

//-----------------------------------------------------------------------------
/// Transform a Unicode string into an uppercase Unicode string. Input and output accept UTF8, UTF16, UTF32, also mixed
/// @param sInput the UTF8 input string
/// @param Output the UTF output container (typically a string type), will be appended to
template<typename OutType, typename InputString>
bool ToUpper(const InputString& sInput, OutType& Output)
//-----------------------------------------------------------------------------
{
	return ToUpper(sInput.begin(), sInput.end(), Output);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a tiny input iterator implementation that can be constructed around a function which returns characters
/// until the input is exhausted, in which case the function returns EOF, after which the iterator compares
/// equal with a default constructed iterator.
/// Example:
/// @code
/// ReadIterator it(::getchar);
/// ReadIterator ie;
/// auto cp = CodepointFromUTF8(it, ie);
/// @endcode
class ReadIterator
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	using iterator_category = std::input_iterator_tag;
	using self_type         = ReadIterator;
	using value_type        = int;
	using reference         = const value_type&;
	using pointer           = const value_type*;

	/// constructs the end iterator
	ReadIterator() : m_Value(EOF) {}
	/// construct with a function that returns the next character or EOF, after which the iterator compares equal to the end iterator
	ReadIterator(std::function<value_type()> func) : m_Func(std::move(func)) {}

	self_type&      operator++()                  { if (m_Value != EOF) ++m_iIncrement; return *this;      }
	const self_type operator++(int)               { const self_type i = *this; operator++(); return i;     }
	reference       operator* () const            { Read(); return m_Value;                                }
	pointer         operator->() const            { Read(); return &m_Value;                               }

	bool operator==(const self_type& other) const { Read(); return m_Value == EOF && other.m_Value == EOF; }
	bool operator!=(const self_type& other) const { return !operator==(other);                             }

private:

	// Other than a std::istream_iterator we cannot read the actual character already when the operator++
	// is called - in a keyboard driven utf8 input this would block until the next key is pressed
	// - which is not expected e.g. after a command was entered with return at the end.
	// Therefore we record the count of operator++ calls, and execute them once the iterator is first
	// dereferenced or operator== is called.
	void Read() const
	{
		while (m_iIncrement)
		{
			--m_iIncrement;
			m_Value = m_Func();
			if (m_Value == EOF) m_iIncrement = 0;
		}
	}

	std::function<value_type()> m_Func;
	mutable std::size_t         m_iIncrement { 1 };
	mutable value_type          m_Value      { 0 };

}; // ReadIterator

} // namespace KUTF_NAMESPACE

#ifdef KUTF_DEKAF2
DEKAF2_NAMESPACE_END
#endif
