/*
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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
/// contains a wrapper around simdutf to avoid namespace clashing with externally installed library

#include "../../kdefinitions.h"

#if DEKAF2_WITH_SIMDUTF

DEKAF2_NAMESPACE_BEGIN

namespace Unicode {
namespace simd {

enum error_code {
	SUCCESS = 0,
	HEADER_BITS, // Any byte must have fewer than 5 header bits.
	TOO_SHORT,   // The leading byte must be followed by N-1 continuation bytes,
				 // where N is the UTF-8 character length This is also the error
				 // when the input is truncated.
	TOO_LONG,    // We either have too many consecutive continuation bytes or the
				 // string starts with a continuation byte.
	OVERLONG, // The decoded character must be above U+7F for two-byte characters,
			  // U+7FF for three-byte characters, and U+FFFF for four-byte
			  // characters.
	TOO_LARGE, // The decoded character must be less than or equal to
			   // U+10FFFF,less than or equal than U+7F for ASCII OR less than
			   // equal than U+FF for Latin1
	SURROGATE, // The decoded character must be not be in U+D800...DFFF (UTF-8 or
			   // UTF-32) OR a high surrogate must be followed by a low surrogate
			   // and a low surrogate must be preceded by a high surrogate
			   // (UTF-16) OR there must be no surrogate at all (Latin1)
	INVALID_BASE64_CHARACTER, // Found a character that cannot be part of a valid
							  // base64 string. This may include a misplaced
							  // padding character ('=').
	BASE64_INPUT_REMAINDER,   // The base64 input terminates with a single
							  // character, excluding padding (=).
	BASE64_EXTRA_BITS,        // The base64 input terminates with non-zero
							  // padding bits.
	OUTPUT_BUFFER_TOO_SMALL,  // The provided buffer is too small.
	OTHER                     // Not related to validation/transcoding.
};

struct result {
	error_code error;
	size_t count; // In case of error, indicates the position of the error. In
				  // case of success, indicates the number of code units
				  // validated/written.
	result(error_code ec, size_t ct) : error(ec), count(ct) {}
};

/**
 * Compute the number of bytes that this UTF-32 string would require in UTF-8
 * format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-32 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as UTF-8
 */
DEKAF2_NODISCARD
std::size_t utf8_length_from_latin1(const char* input, std::size_t length) noexcept;

/**
 * Using native endianness; Compute the number of bytes that this UTF-16
 * string would require in UTF-8 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-8
 */
DEKAF2_NODISCARD
std::size_t utf8_length_from_utf16(const char16_t* input, std::size_t length) noexcept;

/**
 * Compute the number of bytes that this UTF-32 string would require in UTF-8
 * format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-32 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as UTF-8
 */
DEKAF2_NODISCARD
std::size_t utf8_length_from_utf32(const char32_t* input, std::size_t length) noexcept;

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-8.
 * It is acceptable to pass invalid UTF-8 strings but in such cases
 * the result is implementation defined.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return number of code points
 */
DEKAF2_NODISCARD
std::size_t count_utf8(const char* input, std::size_t length) noexcept;

/**
 * Count the number of code points (characters) in the string assuming that
 * it is valid.
 *
 * This function assumes that the input string is valid UTF-16 (native
 * endianness). It is acceptable to pass invalid UTF-16 strings but in such
 * cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to process
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return number of code points
 */
DEKAF2_NODISCARD
std::size_t count_utf16(const char16_t* input, std::size_t length) noexcept;

/**
 * Compute the number of 4-byte code units that this UTF-8 string would require
 * in UTF-32 format.
 *
 * This function is equivalent to count_utf8
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-8 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the number of char32_t code units required to encode the UTF-8 string
 * as UTF-32
 */
DEKAF2_NODISCARD
std::size_t utf32_length_from_utf8(const char* input, std::size_t length) noexcept;

/**
 * Compute the number of 2-byte code units that this UTF-8 string would require
 * in UTF-16LE format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-8 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-8 string to process
 * @param length        the length of the string in bytes
 * @return the number of char16_t code units required to encode the UTF-8 string
 * as UTF-16LE
 */
DEKAF2_NODISCARD
size_t utf16_length_from_utf8(const char* input, std::size_t length) noexcept;

/**
 * Compute the number of two-byte code units that this UTF-32 string would
 * require in UTF-16 format.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-32 strings but in such cases the result is implementation defined.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @return the number of bytes required to encode the UTF-32 string as UTF-16
 */
DEKAF2_NODISCARD
std::size_t utf16_length_from_utf32(const char32_t* input, std::size_t length) noexcept;

/**
 * Using native endianness; Compute the number of bytes that this UTF-16
 * string would require in UTF-32 format.
 *
 * This function is equivalent to count_utf16.
 *
 * This function does not validate the input. It is acceptable to pass invalid
 * UTF-16 strings but in such cases the result is implementation defined.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @return the number of bytes required to encode the UTF-16LE string as UTF-32
 */
DEKAF2_NODISCARD
std::size_t utf32_length_from_utf16(const char16_t* input, std::size_t length) noexcept;

/**
 * Convert Latin1 string into UTF8 string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return the number of written char; 0 if conversion is not possible
 */
DEKAF2_NODISCARD
std::size_t convert_latin1_to_utf8(const char* input, std::size_t length, char* utf8_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-8
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
DEKAF2_NODISCARD
std::size_t convert_utf16_to_utf8(const char16_t* input, std::size_t length, char* utf8_buffer) noexcept;

/**
 * Convert possibly broken UTF-32 string into UTF-8 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf8_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
DEKAF2_NODISCARD
std::size_t convert_utf32_to_utf8(const char32_t* input, std::size_t length, char* utf8_buffer) noexcept;

/**
 * Using native endianness, convert a Latin1 string into a UTF-16 string.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t.
 */
DEKAF2_NODISCARD
std::size_t convert_latin1_to_utf16(const char* input, std::size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-32 string into a UTF-16
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-32 string to convert
 * @param length        the length of the string in 4-byte code units (char32_t)
 * @param utf16_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-32 string
 */
DEKAF2_NODISCARD
std::size_t convert_utf32_to_utf16(const char32_t* input, std::size_t length, char16_t* utf16_buffer) noexcept;

/**
 * Convert Latin1 string into UTF-32 string.
 *
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the Latin1 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t; 0 if conversion is not possible
 */
DEKAF2_NODISCARD
std::size_t convert_latin1_to_utf32(const char* input, std::size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-16 string into UTF-32
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * This function is not BOM-aware.
 *
 * @param input         the UTF-16 string to convert
 * @param length        the length of the string in 2-byte code units (char16_t)
 * @param utf32_buffer   the pointer to buffer that can hold conversion result
 * @return number of written code units; 0 if input is not a valid UTF-16LE
 * string
 */
DEKAF2_NODISCARD
std::size_t convert_utf16_to_utf32(const char16_t* input, std::size_t length, char32_t* utf32_buffer) noexcept;

/**
 * Convert possibly broken UTF-8 string into UTF-32 string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf32_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char32_t; 0 if the input was not valid UTF-8
 * string
 */
DEKAF2_NODISCARD
std::size_t convert_utf8_to_utf32(const char* input, std::size_t length, char32_t* utf32_output) noexcept;

/**
 * Using native endianness, convert possibly broken UTF-8 string into a UTF-16
 * string.
 *
 * During the conversion also validation of the input string is done.
 * This function is suitable to work with inputs from untrusted sources.
 *
 * @param input         the UTF-8 string to convert
 * @param length        the length of the string in bytes
 * @param utf16_buffer  the pointer to buffer that can hold conversion result
 * @return the number of written char16_t; 0 if the input was not valid UTF-8
 * string
 */
DEKAF2_NODISCARD
std::size_t convert_utf8_to_utf16(const char* input, std::size_t length, char16_t* utf16_output) noexcept;

/***
 * Validate the UTF-8 string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf8_with_errors.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-8 string to validate.
 * @param len the length of the string in bytes.
 * @return true if and only if the string is valid UTF-8.
 */
DEKAF2_NODISCARD
bool validate_utf8(const char* buf, std::size_t len) noexcept;

/**
 * Validate the UTF-8 string and stop on error.
 *
 * Overridden by each implementation.
 *
 * @param buf the UTF-8 string to validate.
 * @param len the length of the string in bytes.
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
DEKAF2_NODISCARD
result validate_utf8_with_errors(const char* buf, std::size_t len) noexcept;

/**
 * Using native endianness; Validate the UTF-16 string.
 * This function may be best when you expect the input to be almost always
 * valid. Otherwise, consider using validate_utf16_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16 string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return true if and only if the string is valid UTF-16.
 */
DEKAF2_NODISCARD
bool validate_utf16(const char16_t* buf, std::size_t len) noexcept;

/**
 * Using native endianness; Validate the UTF-16 string and stop on error.
 * It might be faster than validate_utf16 when an error is expected to occur
 * early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-16 string to validate.
 * @param len the length of the string in number of 2-byte code units
 * (char16_t).
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
DEKAF2_NODISCARD
result validate_utf16_with_errors(const char16_t* buf, std::size_t len) noexcept;

/**
 * Validate the UTF-32 string. This function may be best when you expect
 * the input to be almost always valid. Otherwise, consider using
 * validate_utf32_with_errors.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-32 string to validate.
 * @param len the length of the string in number of 4-byte code units
 * (char32_t).
 * @return true if and only if the string is valid UTF-32.
 */
DEKAF2_NODISCARD
bool validate_utf32(const char32_t* buf, std::size_t len) noexcept;

/**
 * Validate the UTF-32 string and stop on error. It might be faster than
 * validate_utf32 when an error is expected to occur early.
 *
 * Overridden by each implementation.
 *
 * This function is not BOM-aware.
 *
 * @param buf the UTF-32 string to validate.
 * @param len the length of the string in number of 4-byte code units
 * (char32_t).
 * @return a result pair struct (of type simdutf::result containing the two
 * fields error and count) with an error code and either position of the error
 * (in the input in code units) if any, or the number of code units validated if
 * successful.
 */
DEKAF2_NODISCARD
result validate_utf32_with_errors(const char32_t* buf, std::size_t len) noexcept;

} // end of namespace simd
} // end of namespace Unicode

DEKAF2_NAMESPACE_END

#endif // DEKAF2_WITH_SIMDUTF
