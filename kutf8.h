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
/// provides support for UTF8, UTF16 and UCS4 encoding

#include <cstdint>
#include <string>
#include "kstring.h"

namespace dekaf2 {
namespace Unicode {

using codepoint_t = uint32_t;
using utf16_t     = uint16_t;
using utf8_t      = uint8_t;

static constexpr codepoint_t INVALID_CODEPOINT = 0xFFFFFFFF;

struct SurrogatePair
{
	utf16_t	first{0};
	utf16_t second{0};
};

//-----------------------------------------------------------------------------
inline constexpr
bool IsLeadSurrogate(utf16_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0xfc00) == 0xd800;
}

//-----------------------------------------------------------------------------
inline constexpr
bool IsTrailSurrogate(utf16_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0xfc00) == 0xdc00;
}

//-----------------------------------------------------------------------------
inline constexpr
bool IsSurrogate(utf16_t ch)
//-----------------------------------------------------------------------------
{
	return (ch & 0xd800) == 0xdfff;
}

//-----------------------------------------------------------------------------
inline constexpr
bool NeedsSurrogates(codepoint_t ch)
//-----------------------------------------------------------------------------
{
	return (ch >= 0x010000 && ch <= 0x010ffff);
}

//-----------------------------------------------------------------------------
/// check before calling that the input needs surrogate separation
inline
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
/// check before calling that the surrogates are valid for composition
inline constexpr
codepoint_t SurrogatesToCodepoint(SurrogatePair sp)
//-----------------------------------------------------------------------------
{
	return (sp.first << 10) + sp.second - ((0xd800 << 10) + 0xdc00 - 0x10000);
}

//-----------------------------------------------------------------------------
template<typename Ch>
constexpr
codepoint_t CodepointCast(Ch sch)
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
template<typename Ch>
constexpr
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
		return 0;
	}
}

//-----------------------------------------------------------------------------
template<typename Ch,
	 	 typename = std::enable_if_t<std::is_integral<Ch>::value> >
constexpr
bool ToUTF8(Ch sch, char*& sNarrow)
//-----------------------------------------------------------------------------
{
	using N = char;

	codepoint_t ch = CodepointCast(sch);

	if (ch < 0x0080)
	{
		*sNarrow++ = static_cast<N>(ch);
	}
	else if (ch < 0x0800)
	{
		*sNarrow++ = static_cast<N>(0xc0 | ((ch >> 6) & 0x1f));
		*sNarrow++ = static_cast<N>(0x80 | (ch & 0x3f));
	}
	else if (ch < 0x010000)
	{
		*sNarrow++ = static_cast<N>(0xe0 | ((ch >> 12) & 0x0f));
		*sNarrow++ = static_cast<N>(0x80 | ((ch >>  6) & 0x3f));
		*sNarrow++ = static_cast<N>(0x80 | (ch & 0x3f));
	}
	else if (ch < 0x0110000)
	{
		*sNarrow++ = static_cast<N>(0xf0 | ((ch >> 18) & 0x07));
		*sNarrow++ = static_cast<N>(0x80 | ((ch >> 12) & 0x3f));
		*sNarrow++ = static_cast<N>(0x80 | ((ch >>  6) & 0x3f));
		*sNarrow++ = static_cast<N>(0x80 | (ch & 0x3f));
	}
	else
	{
		*sNarrow++ = '?';
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
template<typename Ch, typename NarrowString,
         typename = std::enable_if_t<std::is_integral<Ch>::value> >
constexpr
bool ToUTF8(Ch sch, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	using N=typename NarrowString::value_type;

	codepoint_t ch = CodepointCast(sch);

	if (ch < 0x0080)
	{
		sNarrow += static_cast<N>(ch);
	}
	else if (ch < 0x0800)
	{
		sNarrow += static_cast<N>(0xc0 | ((ch >> 6) & 0x1f));
		sNarrow += static_cast<N>(0x80 | (ch & 0x3f));
	}
	else if (ch < 0x010000)
	{
		sNarrow += static_cast<N>(0xe0 | ((ch >> 12) & 0x0f));
		sNarrow += static_cast<N>(0x80 | ((ch >>  6) & 0x3f));
		sNarrow += static_cast<N>(0x80 | (ch & 0x3f));
	}
	else if (ch < 0x0110000)
	{
		sNarrow += static_cast<N>(0xf0 | ((ch >> 18) & 0x07));
		sNarrow += static_cast<N>(0x80 | ((ch >> 12) & 0x3f));
		sNarrow += static_cast<N>(0x80 | ((ch >>  6) & 0x3f));
		sNarrow += static_cast<N>(0x80 | (ch & 0x3f));
	}
	else
	{
		sNarrow += '?';
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
template<typename Ch, typename NarrowString = KString,
         typename = std::enable_if_t<std::is_integral<Ch>::value> >
constexpr
NarrowString ToUTF8(Ch sch)
//-----------------------------------------------------------------------------
{
	NarrowString sRet;
	ToUTF8(sch, sRet);
	return sRet;
}

//-----------------------------------------------------------------------------
template<typename WideString, typename NarrowString,
         typename = std::enable_if_t<!std::is_integral<WideString>::value> >
constexpr
bool ToUTF8(const WideString& sWide, NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	typename WideString::const_iterator it = sWide.cbegin();
	typename WideString::const_iterator ie = sWide.cend();

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
				if (!ToUTF8(sp.first, sNarrow))
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
					if (!ToUTF8(sp.first, sNarrow))
					{
						return false;
					}
					if (!ToUTF8(sp.second, sNarrow))
					{
						return false;
					}
				}
				else
				{
					if (!ToUTF8(SurrogatesToCodepoint(sp), sNarrow))
					{
						return false;
					}
				}
			}
		}
		else
		{
			// default case
			if (!ToUTF8(*it, sNarrow))
			{
				return false;
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
template<typename NarrowString>
constexpr
bool ValidUTF8(const NarrowString& sNarrow)
//-----------------------------------------------------------------------------
{
	using N=typename NarrowString::value_type;

	uint16_t remaining { 0 };
	codepoint_t codepoint { 0 };
	codepoint_t lower_limit { 0 };

	for (const auto sch : sNarrow)
	{

		codepoint_t ch = CodepointCast(sch);

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

//-----------------------------------------------------------------------------
template<typename NarrowString>
size_t CountUTF8(typename NarrowString::const_iterator it,
				 typename NarrowString::const_iterator ie)
//-----------------------------------------------------------------------------
{
	size_t iCount { 0 };

	for (; DEKAF2_LIKELY(it != ie) ;)
	{
		codepoint_t ch = CodepointCast(*it);

		++it;

		if (DEKAF2_LIKELY(ch < 128))
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
template<typename NarrowString>
codepoint_t NextCodepointFromUTF8(typename NarrowString::const_iterator& it,
								  typename NarrowString::const_iterator ie)
//-----------------------------------------------------------------------------
{
	using N=typename NarrowString::value_type;

	if (DEKAF2_UNLIKELY(it == ie))
	{
		return INVALID_CODEPOINT;
	}

	codepoint_t ch = CodepointCast(*it++);

	if (DEKAF2_LIKELY(ch < 128))
	{
		return ch;
	}

	if (sizeof(N) > 1 && ch > 0x0ff)
	{
		return INVALID_CODEPOINT;
	}

	uint16_t remaining;
	codepoint_t lower_limit;
	codepoint_t codepoint;

	if ((ch & 0x0e0) == 0x0c0)
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
		return INVALID_CODEPOINT;
	}

	for (; DEKAF2_LIKELY(it != ie); )
	{
		ch = CodepointCast(*it++);

		if (DEKAF2_UNLIKELY((sizeof(N) > 1 && ch > 0x0ff)))
		{
			break; // invalid
		}

		if (DEKAF2_UNLIKELY((ch & 0x0c0) != 0x080))
		{
			break; // invalid
		}

		codepoint <<= 6;
		codepoint |= (ch & 0x03f);

		if (!--remaining)
		{
			if (DEKAF2_UNLIKELY(codepoint < lower_limit))
			{
				break; // invalid
			}

			return codepoint; // valid
		}
	}

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
template<typename NarrowString>
codepoint_t PrevCodepointFromUTF8(typename NarrowString::const_iterator& it,
								  typename NarrowString::const_iterator ibegin,
								  typename NarrowString::const_iterator iend)
//-----------------------------------------------------------------------------
{
	while (DEKAF2_LIKELY(it != ibegin))
	{
		// check if this char starts a utf8 sequence
		codepoint_t ch = CodepointCast(*--it);

		if (DEKAF2_LIKELY(ch < 128))
		{
			return ch;
		}
		else if (   (ch & 0x0e0) == 0x0c0
				 || (ch & 0x0f0) == 0x0e0
				 || (ch & 0x0f8) == 0x0f0 )
		{
			typename NarrowString::const_iterator nit = it;
			return NextCodepointFromUTF8<NarrowString>(nit, iend);
		}
	}

	return INVALID_CODEPOINT;
}

//-----------------------------------------------------------------------------
template<typename NarrowString, class Functor,
         typename = std::enable_if_t<std::is_class<Functor>::value
                                  || std::is_function<Functor>::value> >
bool FromUTF8(typename NarrowString::const_iterator it,
			  typename NarrowString::const_iterator ie,
			  Functor func)
//-----------------------------------------------------------------------------
{
	for (; DEKAF2_LIKELY(it != ie);)
	{
		codepoint_t codepoint = NextCodepointFromUTF8<NarrowString>(it, ie);

		if (DEKAF2_UNLIKELY(codepoint == INVALID_CODEPOINT))
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
template<typename NarrowString, class Functor,
		 typename = std::enable_if_t<std::is_class<Functor>::value
									|| std::is_function<Functor>::value> >
bool FromUTF8(const NarrowString& sNarrow, Functor func)
//-----------------------------------------------------------------------------
{
	return FromUTF8<NarrowString>(sNarrow.begin(), sNarrow.end(), func);
}

//-----------------------------------------------------------------------------
template<typename NarrowString, typename WideString,
         typename = std::enable_if_t<!std::is_function<WideString>::value
                                     && !std::is_class<WideString>::value> >
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
template<typename NarrowString, typename NarrowReturnString>
bool ToLowerUTF8(const NarrowString& sInput, NarrowReturnString& sOutput)
//-----------------------------------------------------------------------------
{
	sOutput.reserve(sOutput.size() + sInput.size());

	return TransformUTF8(sInput, sOutput, [](codepoint_t uch, NarrowReturnString& sOut)
	{
		return ToUTF8(std::towlower(uch), sOut);
	});
}

//-----------------------------------------------------------------------------
template<typename NarrowString, typename NarrowReturnString>
bool ToUpperUTF8(const NarrowString& sInput, NarrowReturnString& sOutput)
//-----------------------------------------------------------------------------
{
	sOutput.reserve(sOutput.size() + sInput.size());

	return TransformUTF8(sInput, sOutput, [](codepoint_t uch, NarrowReturnString& sOut)
	{
		return ToUTF8(std::towupper(uch), sOut);
	});
}

} // namespace Unicode

} // of namespace dekaf2








