/*
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

/// @file khex.h
/// provides support for hexadecimal encoding

#include <dekaf2/core/types/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringutils.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_encoding
/// @{

/// Append a char in hexadecimal to sOut
DEKAF2_PUBLIC DEKAF2_CONSTEXPR_STRING
void kHexAppend(KStringRef& sOut, char chIn)
{
	constexpr KStringView hexify { "0123456789abcdef" };

	sOut += hexify[(chIn >> 4) & 0x0f];
	sOut += hexify[(chIn     ) & 0x0f];

} // KEnc::HexAppend

/// Append a string in hexadecimal to sOut
DEKAF2_PUBLIC DEKAF2_CONSTEXPR_STRING
void kHexAppend(KStringRef& sOut, KStringView sIn)
{
	sOut.reserve(sIn.size() * 2);

	for (auto ch : sIn)
	{
		kHexAppend(sOut, ch);
	}

} // kHexAppend

/// Convert an input string to hexadecimal
DEKAF2_NODISCARD DEKAF2_PUBLIC DEKAF2_CONSTEXPR_STRING
KString kHex(KStringView sIn)
{
	KString sRet;

	sRet.reserve(sIn.size() * 2);

	for (auto ch : sIn)
	{
		kHexAppend(sRet, ch);
	}

	return sRet;
}

/// Decode a hexadecimal input string
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kUnHex(KStringView sIn);

namespace detail {

// helper for a constexpr contains(), as KStringView::contain() uses memchr() with a static_cast<>
constexpr bool ConstExprContains(KStringView sv, char needle) noexcept
{
	for (auto ch : sv)
	{
		if (ch == needle)
		{
			return true;
		}
	}
	return false;
}

} // end of namespace detail

/// constexpr return a byte array decoded from hexadecimal input with possible separators.
/// Optimized for performance in non-constexpr contexts
template
<
	// the output type, either a std::array or a C array
	typename Bytes,
	// accept max n consecutive separators, 0 = do not accept separators
	uint_fast8_t iMaxConsecutiveSeparators = 1,
	// TODO expect a separator every n chars, 0 = no expectations
	uint_fast8_t iEvery                    = 2
>
DEKAF2_NODISCARD DEKAF2_PUBLIC constexpr
Bytes kBytesFromHex(KStringView sInput, KStringView sSeparators = ":-.|") noexcept
{
#if defined(__cpp_constexpr) && __cpp_constexpr >= 202002L
	Bytes bytes;
#else
	Bytes bytes { 0 };
#endif

	static_assert(sizeof(bytes[0]) == 1 , "value type of Bytes must have 8 bit");

	// we assume Bytes having 8 bit values (== bytes ..)
	constexpr std::size_t iSizeOut = sizeof(bytes);

	std::size_t  iNibbleCount           { 0 };
	uint_fast8_t iNibble                { 0 };
	uint_fast8_t iConsecutiveSeparators { 0 };

	for (auto ch : sInput)
	{
		auto i = kFromHexChar(ch);

		if (i > 15)
		{
			// not a nibble - check if the separator is valid
			if (detail::ConstExprContains(sSeparators, ch))
			{
				// valid separator
				if (iMaxConsecutiveSeparators > 1)
				{
					if (++iConsecutiveSeparators > iMaxConsecutiveSeparators)
					{
						// too many separators
						iNibbleCount = 1;
						break;
					}
				}

				if ((iNibbleCount & 1) != 0)
				{
					// separator breaking a hex pair
					break;
				}

				// all good, separator char valid and expected
				continue;
			}

			// invalid separator char
			iNibbleCount = 1;
			break;
		}

		iNibble += i;

		if ((iNibbleCount & 1) == 1)
		{
			// output with every second nibble
			if (iNibbleCount >= iSizeOut * 2)
			{
				// max output size reached ..
				break;
			}

			bytes[iNibbleCount / 2] = iNibble;
			iNibble = 0;
		}
		else
		{
			// shift with every first nibble
			iNibble <<= 4;
		}

		// we increment after inserting the nibble
		++iNibbleCount;

		// will be optimized away for max separators < 2
		if (iMaxConsecutiveSeparators > 1)
		{
			iConsecutiveSeparators = 0;
		}
	}

	if (iNibbleCount != iSizeOut * 2)
	{
		for (std::size_t i = 0; i < iSizeOut; ++i)
		{
			bytes[i] = 0;
		}
	}

	return bytes;

} // kBytesFromHex


/// @}

DEKAF2_NAMESPACE_END
