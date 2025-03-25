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

/// @file kutfcesu.h
/// helpers to repair badly converted UTF8

#include "kutf.h"

#if KUTF_DEKAF2
DEKAF2_NAMESPACE_BEGIN
#endif

namespace KUTF_NAMESPACE {
namespace CESU8 {

// see https://en.wikipedia.org/wiki/CESU-8

//-----------------------------------------------------------------------------
/// Convert a wide string (UTF16 LE) that was encoded in a byte stream into a UTF8 string (for recovery purposes only)
template<typename NarrowString, typename Iterator>
KUTF_CONSTEXPR_14
NarrowString UTF16BytesToUTF8(Iterator it, Iterator ie)
//-----------------------------------------------------------------------------
{
	NarrowString sNarrow{};
	
	for (; KUTF_LIKELY(it != ie); )
	{
		SurrogatePair sp;
		sp.low = CodepointCast(*it++) << 8;
		
		if (KUTF_UNLIKELY(it == ie))
		{
			ToUTF(REPLACEMENT_CHARACTER, sNarrow);
		}
		else
		{
			sp.low += CodepointCast(*it++);
			
			if (KUTF_UNLIKELY(IsLeadSurrogate(sp.low)))
			{
				// collect another 2 bytes for surrogate replacement
				if (KUTF_LIKELY(it != ie))
				{
					sp.high = CodepointCast(*it++) << 8;
					
					if (KUTF_LIKELY(it != ie))
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
KUTF_CONSTEXPR_14
NarrowString UTF16BytesToUTF8(const ByteString& sUTF16Bytes)
//-----------------------------------------------------------------------------
{
	return UTF16BytesToUTF8<NarrowString>(sUTF16Bytes.begin(), sUTF16Bytes.end());
}

//-----------------------------------------------------------------------------
/// Convert a UTF8 string into a byte stream with UTF16 LE encoding (for recovery purposes only)
template<typename NarrowString, typename ByteString = NarrowString>
KUTF_CONSTEXPR_14
ByteString UTF8ToUTF16Bytes(const NarrowString& sUTF8String)
//-----------------------------------------------------------------------------
{
	using value_type = typename ByteString::value_type;
	
	ByteString sOut{};
	sOut.reserve(sUTF8String.size() * 2);
	
	ForEach(sUTF8String, [&sOut](codepoint_t cp)
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
} // namespace KUTF_NAMESPACE

#ifdef KUTF_DEKAF2
DEKAF2_NAMESPACE_END
#endif
