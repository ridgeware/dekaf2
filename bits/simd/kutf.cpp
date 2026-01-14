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

#include "kutf.h"

#if DEKAF2_WITH_SIMDUTF

#if DEKAF2_IS_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#if DEKAF2_IS_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#endif

#include "../../from/simdutf/simdutf.h"
#include "../../from/simdutf/simdutf.cpp"

#if DEKAF2_IS_CLANG
#pragma clang diagnostic pop
#endif

#if DEKAF2_IS_GCC
#pragma GCC diagnostic pop
#endif

DEKAF2_NAMESPACE_BEGIN

namespace kutf {
namespace simd {

inline
result ToResult(::simdutf::result r1) noexcept
{
	return result(error_code(r1.error), r1.count);
}

std::size_t utf8_length_from_latin1(const char* input, std::size_t length) noexcept
{
	return ::simdutf::utf8_length_from_latin1(input, length);
}

std::size_t utf8_length_from_utf16(const char16_t* input, std::size_t length) noexcept
{
	return ::simdutf::utf8_length_from_utf16(input, length);
}

std::size_t utf8_length_from_utf32(const char32_t* input, std::size_t length) noexcept
{
	return ::simdutf::utf8_length_from_utf32(input, length);
}

std::size_t count_utf8(const char* input, std::size_t length) noexcept
{
	return ::simdutf::count_utf8(input, length);
}

std::size_t count_utf16(const char16_t* input, std::size_t length) noexcept
{
	return ::simdutf::count_utf16(input, length);
}

std::size_t utf32_length_from_utf8(const char* input, std::size_t length) noexcept
{
	return ::simdutf::utf32_length_from_utf8(input, length);
}

size_t utf16_length_from_utf8(const char* input, std::size_t length) noexcept
{
	return ::simdutf::utf16_length_from_utf8(input, length);
}

std::size_t utf16_length_from_utf32(const char32_t* input, std::size_t length) noexcept
{
	return ::simdutf::utf16_length_from_utf32(input, length);
}

std::size_t utf32_length_from_utf16(const char16_t* input, std::size_t length) noexcept
{
	return ::simdutf::utf32_length_from_utf16(input, length);
}

std::size_t convert_latin1_to_utf8(const char* input, std::size_t length, char* utf8_buffer) noexcept
{
	return ::simdutf::convert_latin1_to_utf8(input, length, utf8_buffer);
}

std::size_t convert_utf16_to_utf8(const char16_t* input, std::size_t length, char* utf8_buffer) noexcept
{
	return ::simdutf::convert_utf16_to_utf8(input, length, utf8_buffer);
}

std::size_t convert_utf32_to_utf8(const char32_t* input, std::size_t length, char* utf8_buffer) noexcept
{
	return ::simdutf::convert_utf32_to_utf8(input, length, utf8_buffer);
}

std::size_t convert_latin1_to_utf16(const char* input, std::size_t length, char16_t* utf16_buffer) noexcept
{
	return ::simdutf::convert_latin1_to_utf16(input, length, utf16_buffer);
}

std::size_t convert_utf32_to_utf16(const char32_t* input, std::size_t length, char16_t* utf16_buffer) noexcept
{
	return ::simdutf::convert_utf32_to_utf16(input, length, utf16_buffer);
}

std::size_t convert_latin1_to_utf32(const char* input, std::size_t length, char32_t* utf32_buffer) noexcept
{
	return ::simdutf::convert_latin1_to_utf32(input, length, utf32_buffer);
}

std::size_t convert_utf16_to_utf32(const char16_t* input, std::size_t length, char32_t* utf32_buffer) noexcept
{
	return ::simdutf::convert_utf16_to_utf32(input, length, utf32_buffer);
}

std::size_t convert_utf8_to_utf16(const char* input, std::size_t length, char16_t* utf16_output) noexcept
{
	return ::simdutf::convert_utf8_to_utf16(input, length, utf16_output);
}

std::size_t convert_utf8_to_utf32(const char* input, std::size_t length, char32_t* utf32_output) noexcept
{
	return ::simdutf::convert_utf8_to_utf32(input, length, utf32_output);
}

bool validate_ascii(const char *buf, std::size_t len) noexcept
{
	return ::simdutf::validate_ascii(buf, len);
}

result validate_ascii_with_errors(const char *buf, std::size_t len) noexcept
{
	return ToResult(::simdutf::validate_ascii_with_errors(buf, len));
}

bool validate_utf8(const char* buf, std::size_t len) noexcept
{
	return ::simdutf::validate_utf8(buf, len);
}

result validate_utf8_with_errors(const char* buf, std::size_t len) noexcept
{
	return ToResult(::simdutf::validate_utf8_with_errors(buf, len));
}

bool validate_utf16(const char16_t* buf, std::size_t len) noexcept
{
	return ::simdutf::validate_utf16(buf, len);
}

result validate_utf16_with_errors(const char16_t* buf, std::size_t len) noexcept
{
	return ToResult(::simdutf::validate_utf16_with_errors(buf, len));
}

bool validate_utf32(const char32_t* buf, std::size_t len) noexcept
{
	return ::simdutf::validate_utf32(buf, len);
}

result validate_utf32_with_errors(const char32_t* buf, std::size_t len) noexcept
{
	return ToResult(::simdutf::validate_utf32_with_errors(buf, len));
}

} // end of namespace simd
} // end of namespace kutf

DEKAF2_NAMESPACE_END

#endif // DEKAF2_WITH_SIMDUTF
