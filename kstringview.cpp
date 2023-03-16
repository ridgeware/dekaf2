/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017-2019, Ridgeware, Inc.
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
//
*/

#include "kstringview.h"
#include "kstringutils.h"
#include "kregex.h"
#include "klog.h"
#include "dekaf2.h"
#include "bits/simd/kfindfirstof.h"
#include "kctype.h"
#include "kcaseless.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
size_t kFind(
        const KStringView haystack,
        const KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

	const auto iNeedleSize = needle.size();

	if (DEKAF2_UNLIKELY(iNeedleSize == 1))
	{
		// flip to single char search if only one char is in the search argument
		return kFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(!iNeedleSize))
	{
		// the empty string is always existing at the start of haystack
		return 0;
	}

	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		// glibc memmem fails if haystack is null
		return KStringView::npos;
	}

	// glibc has an excellent memmem implementation, so we use it if available.
	// libc has a very slow memmem implementation (about 100 times slower than glibc),
	// so we use our own, which is only about 2 times slower, by overloading the
	// function signature in the dekaf2 namespace

	auto found = static_cast<const char*>(memmem(haystack.data() + pos,
												 haystack.size() - pos,
												 needle.data(),
												 iNeedleSize));

	if (DEKAF2_UNLIKELY(!found))
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>(found - haystack.data());
	}

#else // non-optimized

	return static_cast<KStringView::rep_type>(haystack).find(needle, pos);

#endif

} // kFind

//-----------------------------------------------------------------------------
size_t kRFind(
        const KStringView haystack,
        const KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)

	const auto iNeedleSize = needle.size();

	if (DEKAF2_UNLIKELY(iNeedleSize == 1))
	{
		return kRFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(!iNeedleSize))
	{
		return haystack.size();
	}

	const auto iHaystackSize = haystack.size();

	if (DEKAF2_LIKELY(iNeedleSize <= iHaystackSize))
	{
		pos = std::min(iHaystackSize - iNeedleSize, pos);

		KStringView::size_type iJump = 0;

		for(;;)
		{
			auto found = static_cast<const char*>(memrchr(haystack.data(), needle[0], pos+1));

			if (!found)
			{
				break;
			}

			pos = static_cast<size_t>(found - haystack.data());

			if (std::memcmp(haystack.data() + pos + 1,
			                needle.data() + 1,
			                iNeedleSize - 1) == 0)
			{
				return pos;
			}

			if (DEKAF2_UNLIKELY(!iJump))
			{
				// compute the minimum repeat interval of the first char
				// we know that the needle size is > 1 as we tested that above
				iJump = needle.find(needle[0], 1);

				if (iJump == KStringView::npos)
				{
					iJump = iNeedleSize;
				}
			}

			if (pos < iJump)
			{
				break;
			}

			pos -= iJump;
		}
	}

	return KStringView::npos;

#else

	return static_cast<KStringView::rep_type>(haystack).rfind(needle, pos);

#endif

} // kRFind

namespace detail { namespace stringview {

//-----------------------------------------------------------------------------
size_t kFindFirstOfInt(
        KStringView haystack,
        const KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return kFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(pos > 0))
	{
		haystack.remove_prefix(pos);
	}

	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		return KStringView::npos;
	}

#ifdef DEKAF2_X86_64
#ifdef DEKAF2_HAS_MINIFOLLY
	static bool has_sse42 = Dekaf::getInstance().GetCpuId().sse42();

	if (DEKAF2_LIKELY(has_sse42))
#endif
	{
		auto result = detail::sse::kFindFirstOf(haystack, needle);
		if (result == KStringView::npos)
		{
			return result;
		}
		return result + pos;
	}
#endif

	auto result = detail::no_sse::kFindFirstOf(haystack, needle, false);
	
	if (DEKAF2_LIKELY(pos == 0 || result == KStringView::npos))
	{
		return result;
	}
	else
	{
		return result + pos;
	}

} // kFindFirstOfInt

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfInt(
        KStringView haystack,
        const KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos > 0))
	{
		haystack.remove_prefix(pos);
	}

	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(needle.empty()))
	{
		return 0;
	}

#ifdef DEKAF2_X86_64
#ifdef DEKAF2_HAS_MINIFOLLY
	static bool has_sse42 = Dekaf::getInstance().GetCpuId().sse42();

	if (DEKAF2_LIKELY(has_sse42))
#endif
	{
		auto result = detail::sse::kFindFirstNotOf(haystack, needle);

		if (DEKAF2_LIKELY(result == KStringView::npos || pos == 0))
		{
			return result;
		}
		return result + pos;
	}
#endif

	auto result = detail::no_sse::kFindFirstOf(haystack, needle, true);

	if (DEKAF2_LIKELY(pos == 0 || result == KStringView::npos))
	{
		return result;
	}
	else
	{
		return result + pos;
	}

} // kFindFirstNotOfInt

//-----------------------------------------------------------------------------
size_t kFindLastOfInt(
        KStringView haystack,
        const KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return kRFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(pos < haystack.size()))
	{
		haystack.remove_suffix(haystack.size() - (pos+1));
	}

	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		return KStringView::npos;
	}

#ifdef DEKAF2_X86_64
#ifdef DEKAF2_HAS_MINIFOLLY
	static bool has_sse42 = Dekaf::getInstance().GetCpuId().sse42();

	if (DEKAF2_LIKELY(has_sse42))
#endif
	{
		return detail::sse::kFindLastOf(haystack, needle);
	}
#endif

	return detail::no_sse::kFindLastOf(haystack, needle, false);

} // kFindLastOfInt

//-----------------------------------------------------------------------------
size_t kFindLastNotOfInt(
        KStringView haystack,
        const KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos < haystack.size()))
	{
		haystack.remove_suffix(haystack.size() - (pos+1));
	}

	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(needle.empty()))
	{
		return haystack.size() - 1;
	}

#ifdef DEKAF2_X86_64
#ifdef DEKAF2_HAS_MINIFOLLY
	static bool has_sse42 = Dekaf::getInstance().GetCpuId().sse42();

	if (DEKAF2_LIKELY(has_sse42))
#endif
	{
		return detail::sse::kFindLastNotOf(haystack, needle);
	}
#endif

	return detail::no_sse::kFindLastOf(haystack, needle, true);

} // kFindLastNotOfInt

} } // end of namespace detail::stringview

//-----------------------------------------------------------------------------
size_t kFindFirstOfUnescaped(const KStringView haystack,
							 const KFindSetOfChars& needle,
							 KStringView::value_type chEscape,
							 KStringView::size_type pos)
//-----------------------------------------------------------------------------
{
	auto iFound = needle.find_first_in(haystack, pos);

	if (!chEscape || iFound == 0)
	{
		// If no escape char is given or
		// the searched character was first on the line...
		return iFound;
	}

	while (iFound != KStringView::npos)
	{
		size_t iEscapes { 0 };
		size_t iStart { iFound };

		while (iStart)
		{
			// count number of escape characters
			if (haystack[--iStart] != chEscape)
			{
				break;
			}

			++iEscapes;

		} // while iStart

		if (!(iEscapes & 1))  // if even number of escapes
		{
			break;
		}

		iFound = needle.find_first_in(haystack, iFound + 1);

	} // while iFound

	return iFound;

} // kFindFirstOfUnescaped

//-----------------------------------------------------------------------------
size_t kFindUnescaped(const KStringView haystack,
                      KStringView::value_type needle,
                      KStringView::value_type chEscape,
                      KStringView::size_type pos)
//-----------------------------------------------------------------------------
{
	auto iFound = haystack.find (needle, pos);

	if (!chEscape || iFound == 0)
	{
		// If no escape char is given or
		// the searched character was first on the line...
		return iFound;
	}

	while (iFound != KStringView::npos)
	{
		size_t iEscapes { 0 };
		size_t iStart { iFound };

		while (iStart)
		{
			// count number of escape characters
			if (haystack[--iStart] != chEscape)
			{
				break;
			}

			++iEscapes;

		} // while iStart

		if (!(iEscapes & 1))  // if even number of escapes
		{
			break;
		}

		iFound = haystack.find (needle, iFound + 1);

	} // while iFound

	return iFound;

} // kFindUnescaped

//-----------------------------------------------------------------------------
size_t kFindUnescaped(const KStringView haystack,
                      const KStringView needle,
                      KStringView::value_type chEscape,
                      KStringView::size_type pos)
//-----------------------------------------------------------------------------
{
	auto iFound = haystack.find (needle, pos);

	if (!chEscape || iFound == 0)
	{
		// If no escape char is given or
		// the searched character was first on the line...
		return iFound;
	}

	while (iFound != KStringView::npos)
	{
		size_t iEscapes { 0 };
		size_t iStart { iFound };

		while (iStart)
		{
			// count number of escape characters
			if (haystack[--iStart] != chEscape)
			{
				break;
			}
			++iEscapes;
		} // while iStart

		if (!(iEscapes & 1))  // if even number of escapes
		{
			break;
		}

		iFound = haystack.find (needle, iFound + 1);

	} // while iFound

	return iFound;

} // kFindUnescaped

//-----------------------------------------------------------------------------
bool kContainsWord(const KStringView sInput, const KStringView sPattern) noexcept
//-----------------------------------------------------------------------------
{
	KStringView::size_type iPos = 0;

	for (;;)
	{
		iPos = sInput.find(sPattern, iPos);

		if (iPos == KStringView::npos)
		{
			return false;
		}

		if (iPos > 0 && !KASCII::kIsSpace(sInput[iPos - 1]))
		{
			++iPos;
			continue;
		}

		auto iEnd = iPos + sPattern.size();

		if (iEnd < sInput.size() && !KASCII::kIsSpace(sInput[iEnd]))
		{
			++iPos;
			continue;
		}

		return true;
	}

} // kContainsWord

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::copy(value_type* dest, size_type count, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos > size()))
	{
		kDebug(3, "attempt to copy from past the end of string view of size {}: pos {}",
		         size(), pos);
		return 0;
	}

	count = std::min(size() - pos, count);

	return static_cast<size_type>(std::copy(const_cast<char*>(data() + pos),
											const_cast<char*>(data() + pos + count),
											const_cast<char*>(dest))
								  - dest);
} // copy

//-----------------------------------------------------------------------------
/// nonstandard: emulate erase if range is at begin or end
KStringView::self_type& KStringView::erase(size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos > size()))
	{
		// this allows constructs like sStr.erase(sStr.find("clipme")) without
		// having too many warnings
		kDebug(3, "attempt to erase past end of string view of size {}: pos {}, n {}",
				 size(), pos, n);
		return *this;
	}

	n = std::min(n, size() - pos);

	if (n > 0)
	{
		if (pos == 0)
		{
			remove_prefix(n);
		}
		else if (pos + n == size())
		{
			remove_suffix(n);
		}
		else
		{
			KString sError(kFormat("impossible to remove {} chars at pos {} in a string view of size {}",
								   n, pos, size()));
#ifdef DEKAF2_EXCEPTIONS
			DEKAF2_THROW(std::runtime_error(sError.c_str()));
#else
			kWarning(sError);
#endif
		}
	}

	return *this;

} // erase

//-----------------------------------------------------------------------------
bool KStringView::In (const KStringView sHaystack, value_type iDelim) const
//-----------------------------------------------------------------------------
{
	// gcc 4.8.5 needs the non-brace initialization here..
	auto& sNeedle(m_rep);

	size_type iNeedle = 0;
	size_type iHaystack = 0; // Beginning indices
	size_type iNsize = sNeedle.size ();
	size_type iHsize = sHaystack.size (); // Ending

	while (iHaystack < iHsize)
	{
		iNeedle = 0;

		// Search for matching tokens
		while ( (iNeedle < iNsize)
		     && (iHaystack < iHsize)
		     && (sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ((iNeedle >= iNsize) &&
			(iHaystack >= iHsize || (sHaystack[iHaystack] == iDelim)))
		{
			return true;
		}

		// Advance to next delimiter
		while (iHaystack < iHsize && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// skip delimiter (if present, otherwise we are at end of string)
		++iHaystack;
	}

	return false;

} // In

//-----------------------------------------------------------------------------
bool KStringView::Bool() const noexcept
//-----------------------------------------------------------------------------
{
	// * we need to map literal "true" --> true
	// * and literal "false" --> false
	// * as well as non-0 --> true
	// the literal version is needed for conversions between JSON and KSQL
	return Int16() != 0 || In("true,TRUE,on,ON");
}

//-----------------------------------------------------------------------------
int16_t KStringView::Int16(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int16_t>(*this, bIsHex ? 16 : 10);
}

//-----------------------------------------------------------------------------
uint16_t KStringView::UInt16(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint16_t>(*this, bIsHex ? 16 : 10);
}

//-----------------------------------------------------------------------------
int32_t KStringView::Int32(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int32_t>(*this, bIsHex ? 16 : 10);
}

//-----------------------------------------------------------------------------
uint32_t KStringView::UInt32(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint32_t>(*this, bIsHex ? 16 : 10);
}

//-----------------------------------------------------------------------------
int64_t KStringView::Int64(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int64_t>(*this, bIsHex ? 16 : 10);
}

//-----------------------------------------------------------------------------
uint64_t KStringView::UInt64(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint64_t>(*this, bIsHex ? 16 : 10);
}

#ifdef DEKAF2_HAS_INT128
//-----------------------------------------------------------------------------
int128_t KStringView::Int128(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int128_t>(*this, bIsHex ? 16 : 10);
}

//-----------------------------------------------------------------------------
uint128_t KStringView::UInt128(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint128_t>(*this, bIsHex ? 16 : 10);
}
#endif

//-----------------------------------------------------------------------------
float KStringView::Float() const noexcept
//-----------------------------------------------------------------------------
{
	KString sTmp(*this);
	return std::strtof(sTmp.c_str(), nullptr);
}

//-----------------------------------------------------------------------------
double KStringView::Double() const noexcept
//-----------------------------------------------------------------------------
{
	KString sTmp(*this);
	return std::strtod(sTmp.c_str(), nullptr);
}

//-----------------------------------------------------------------------------
void KStringView::Warn(KStringView sWhere, KStringView sWhat) const
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_WITH_KLOG
	KLog::getInstance().debug_fun(1, sWhere, sWhat);
#endif
}

//----------------------------------------------------------------------
KStringView KStringView::MatchRegex(const KStringView sRegEx, size_type pos) const
//----------------------------------------------------------------------
{
	return KRegex::Match(*this, sRegEx, pos);
}

//----------------------------------------------------------------------
std::vector<KStringView> KStringView::MatchRegexGroups(const KStringView sRegEx, size_type pos) const
//----------------------------------------------------------------------
{
	return KRegex::MatchGroups(*this, sRegEx, pos);
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimLeft()
//----------------------------------------------------------------------
{
	return TrimLeft(detail::kASCIISpaces);
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimLeft(value_type chTrim)
//----------------------------------------------------------------------
{
	kTrimLeft(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimLeft(const KStringView sTrim)
//----------------------------------------------------------------------
{
	kTrimLeft(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimRight()
//----------------------------------------------------------------------
{
	return TrimRight(detail::kASCIISpaces);
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimRight(value_type chTrim)
//----------------------------------------------------------------------
{
	kTrimRight(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimRight(const KStringView sTrim)
//----------------------------------------------------------------------
{
	kTrimRight(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KStringView& KStringView::Trim()
//----------------------------------------------------------------------
{
	return Trim(detail::kASCIISpaces);
}

//----------------------------------------------------------------------
KStringView& KStringView::Trim(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrim(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KStringView& KStringView::Trim(const KStringView sTrim)
//----------------------------------------------------------------------
{
	TrimRight(sTrim);
	TrimLeft(sTrim);
	return *this;
}

//----------------------------------------------------------------------
bool KStringView::ClipAt(const KStringView sClipAt)
//----------------------------------------------------------------------
{
	size_type pos = find(sClipAt);
	if (pos != npos)
	{
		erase(pos);
		return (true); // we modified the string
	}
	return (false); // we did not modify the string

} // ClipAt

//----------------------------------------------------------------------
bool KStringView::ClipAtReverse(const KStringView sClipAtReverse)
//----------------------------------------------------------------------
{
	erase(0, find(sClipAtReverse));
	return true;

} // ClipAtReverse

//-----------------------------------------------------------------------------
KStringView::size_type KStringView::FindCaselessASCII(const self_type str, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	return kCaselessFind(*this, str, pos);
}

#ifndef DEKAF2_X86_64

//-----------------------------------------------------------------------------
KFindSetOfChars::KFindSetOfChars(const KStringView& sNeedles)
//-----------------------------------------------------------------------------
{
	switch (sNeedles.size())
	{
		case 0:
			break;

		case 1:
			m_state = STATE::SINGLE;
			m_chSingle = sNeedles.front();
			break;

		default:
			m_state = STATE::MULTI;
			for (auto c : sNeedles)
			{
				m_table[static_cast<unsigned char>(c)] = true;
			}
			break;
	}

} // ctor

//-----------------------------------------------------------------------------
KFindSetOfChars::KFindSetOfChars(const char* sNeedles)
//-----------------------------------------------------------------------------
{
	if (sNeedles && *sNeedles)
	{
		if (sNeedles[1] == 0)
		{
			m_state = STATE::SINGLE;
			m_chSingle = *sNeedles;
		}
		else
		{
			m_state = STATE::MULTI;
			for(;;)
			{
				auto c = static_cast<unsigned char>(*sNeedles++);
				if (!c) break;
				m_table[c] = true;
			}
		}
	}

} // ctor

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in_impl(const KStringView& sHaystack, bool bNot) const
//-----------------------------------------------------------------------------
{
	switch (m_state)
	{
		case STATE::EMPTY:
			return (!bNot) ? KStringView::npos : sHaystack.empty() ? KStringView::npos : 0;

		case STATE::SINGLE:
		{
			if (!bNot) return sHaystack.find(m_chSingle);

			auto it = std::find_if(sHaystack.begin(),
								   sHaystack.end(),
								   [this](const char c)
			{
				return c != m_chSingle;
			});

			return (it == sHaystack.end()) ? KStringView::npos : static_cast<std::size_t>(it - sHaystack.begin());
		}

		case STATE::MULTI:
		{
			auto it = std::find_if(sHaystack.begin(),
								   sHaystack.end(),
								   [this, bNot](const char c)
			{
				return m_table[static_cast<unsigned char>(c)] != bNot;
			});

			return (it == sHaystack.end()) ? KStringView::npos : static_cast<std::size_t>(it - sHaystack.begin());
		}
	}

	// for gcc..
	return KStringView::npos;

} // find_first_in_impl

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_prefix(pos);
	auto found = find_first_in_impl(sNewHaystack, false);
	return (found == KStringView::npos) ? found : found + pos;

} // find_first_in

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_prefix(pos);
	auto found = find_first_in_impl(sNewHaystack, true);
	return (found == KStringView::npos) ? found : found + pos;

} // find_first_not_in

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in_impl(const KStringView& sHaystack, bool bNot) const
//-----------------------------------------------------------------------------
{
	switch (m_state)
	{
		case STATE::EMPTY:
			return (!bNot) ? KStringView::npos : sHaystack.empty() ? KStringView::npos : sHaystack.size() - 1;

		case STATE::SINGLE:
		{
			if (!bNot) return sHaystack.rfind(m_chSingle);

			auto it = std::find_if(sHaystack.rbegin(),
								   sHaystack.rend(),
								   [this](const char c)
			{
				return c != m_chSingle;
			});

			return (it == sHaystack.rend()) ? KStringView::npos : static_cast<std::size_t>((it.base() - 1) - sHaystack.begin());
		}

		case STATE::MULTI:
		{
			auto it = std::find_if(sHaystack.rbegin(),
								   sHaystack.rend(),
								   [this, bNot](const char c)
			{
				return m_table[static_cast<unsigned char>(c)] != bNot;
			});

			return (it == sHaystack.rend()) ? KStringView::npos : static_cast<std::size_t>((it.base() - 1) - sHaystack.begin());
		}
	}

	// for gcc..
	return KStringView::npos;

} // find_last_in_impl

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_suffix(sNewHaystack.size() - (pos+1));
	auto found = find_last_in_impl(sNewHaystack, false);
	return (found == KStringView::npos) ? found : found + pos;

} // find_last_in

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_suffix(sNewHaystack.size() - (pos+1));
	auto found = find_last_in_impl(sNewHaystack, true);
	return (found == KStringView::npos) ? found : found + pos;

} // find_last_not_in

#else // is 86_64

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(const KStringView& sHaystack) const
//-----------------------------------------------------------------------------
{
	return detail::sse::kFindFirstOf(sHaystack, m_sNeedles);
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_prefix(pos);
	auto found = find_first_in(sNewHaystack);
	return (found == KStringView::npos) ? found : found + pos;
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(const KStringView& sHaystack) const
//-----------------------------------------------------------------------------
{
	return detail::sse::kFindFirstNotOf(sHaystack, m_sNeedles);
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_prefix(pos);
	auto found = find_first_not_in(sNewHaystack);
	return (found == KStringView::npos) ? found : found + pos;
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(const KStringView& sHaystack) const
//-----------------------------------------------------------------------------
{
	return detail::sse::kFindLastOf(sHaystack, m_sNeedles);
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(const KStringView& sHaystack) const
//-----------------------------------------------------------------------------
{
	return detail::sse::kFindLastNotOf(sHaystack, m_sNeedles);
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_suffix(sNewHaystack.size() - (pos+1));
	auto found = find_last_in(sNewHaystack);
	return (found == KStringView::npos) ? found : found + pos;

} // find_last_in

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(const KStringView& sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	auto sNewHaystack = sHaystack;
	sNewHaystack.remove_suffix(sNewHaystack.size() - (pos+1));
	auto found = find_last_not_in(sNewHaystack);
	return (found == KStringView::npos) ? found : found + pos;

} // find_last_not_in

#endif // is 86_64



static_assert(std::is_nothrow_move_constructible<KStringView>::value,
			  "KStringView is intended to be nothrow move constructible, but is not!");


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringView::size_type KStringView::npos;
constexpr KStringView::value_type KStringView::s_0ch;

#endif

} // end of namespace dekaf2
