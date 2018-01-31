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
/// provides support for UTF8 encoding

#include <cstdint>
#include <string>

namespace dekaf2 {
namespace Unicode {

template<typename Ch>
uint32_t CodepointCast(Ch sch)
{
	// All this code gets completely eliminated during
	// compilation. All it does is to make sure we can
	// expand any char type to a uint32_t without signed
	// bit expansion, treating all char types as unsigned.

	if (sizeof(Ch) == 1)
	{
		return static_cast<uint8_t>(sch);
	}
	else if (sizeof(Ch) == 2)
	{
		return static_cast<uint16_t>(sch);
	}
	else
	{
		return static_cast<uint32_t>(sch);
	}
}

template<typename Ch, typename NarrowString,
         typename = std::enable_if_t<std::is_integral<Ch>::value> >
void ToUTF8(Ch sch, NarrowString& sNarrow)
{
	using N=typename NarrowString::value_type;

	uint32_t ch = CodepointCast(sch);

	if (ch < 0x0080)
	{
		sNarrow += N(ch);
	}
	else if (ch < 0x0800)
	{
		sNarrow += N(0xc0 | ((ch >> 6) & 0x1f));
		sNarrow += N(0x80 | (ch & 0x3f));
	}
	else if (ch < 0x010000)
	{
		sNarrow += N(0xe0 | ((ch >> 12) & 0x0f));
		sNarrow += N(0x80 | ((ch >> 6) & 0x3f));
		sNarrow += N(0x80 | (ch & 0x3f));
	}
	else if (ch < 0x0110000)
	{
		sNarrow += N(0xf0 | ((ch >> 18) & 0x07));
		sNarrow += N(0x80 | ((ch >> 12) & 0x3f));
		sNarrow += N(0x80 | ((ch >> 6) & 0x3f));
		sNarrow += N(0x80 | (ch & 0x3f));
	}
	else
	{
		sNarrow += '?';
	}
}

template<typename WideString, typename NarrowString,
         typename = std::enable_if_t<!std::is_integral<WideString>::value> >
void ToUTF8(const WideString& sWide, NarrowString& sNarrow)
{
	for (auto ch : sWide)
	{
		ToUTF8(ch, sNarrow);
	}
}

template<typename NarrowString>
bool ValidUTF8(const NarrowString& sNarrow)
{
	using N=typename NarrowString::value_type;

	uint16_t remaining { 0 };
	uint32_t codepoint { 0 };
	uint32_t lower_limit { 0 };

	for (const auto sch : sNarrow)
	{

		uint32_t ch = CodepointCast(sch);

		if (sizeof(N) > 1 && ch > 0x0ff)
		{
			return false;
		}

		switch (remaining)
		{

			default:
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

			case 5:
			case 4:
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
					--remaining;
					if (!remaining)
					{
						if (codepoint < lower_limit)
						{
							return false;
						}
					}
					break;
				}

		}

	}
	return true;
}

template<typename NarrowString, class Functor,
         typename = std::enable_if_t<std::is_class<Functor>::value
                                  || std::is_function<Functor>::value> >
bool FromUTF8(const NarrowString& sNarrow, Functor func)
{
	using N=typename NarrowString::value_type;

	uint16_t remaining { 0 };
	uint32_t codepoint { 0 };
	uint32_t lower_limit { 0 };

	for (const auto sch : sNarrow)
	{

		uint32_t ch = CodepointCast(sch);

		if (sizeof(N) > 1 && ch > 0x0ff)
		{
			return false;
		}

		switch (remaining)
		{

			default:
			case 0:
				{
					codepoint = 0;
					if (ch < 128)
					{
						func(ch);
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

			case 5:
			case 4:
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
					--remaining;
					if (!remaining)
					{
						if (codepoint < lower_limit)
						{
							return false;
						}

						// take care - this code is written for platforms with 32 bit sWide chars -
						// if you compile this code on Windows, or want to explicitly support 16
						// bit sWide chars on other platforms, you need to supply surrogate pair
						// replacements for characters > 0x0ffffffff at exactly this point before
						// adding a new codepoint to 'sWide'

						func(codepoint);
					}
					break;
				}

		}

	}

	return true;
}

template<typename NarrowString, typename WideString,
         typename = std::enable_if_t<!std::is_function<WideString>::value
                                     && !std::is_class<WideString>::value> >
bool FromUTF8(const NarrowString& sNarrow, WideString& sWide)
{
	using W=typename WideString::value_type;

	return FromUTF8(sNarrow, [&sWide](uint32_t uch)
	{
		sWide += static_cast<W>(uch);
	});

}

} // namespace Unicode

} // of namespace dekaf2

