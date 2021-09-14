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

#ifndef __linux__
//-----------------------------------------------------------------------------
void* memrchr(const void* s, int c, size_t n)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_X86_64
#ifdef DEKAF2_HAS_MINIFOLLY
	static bool has_sse42 = dekaf2::Dekaf::getInstance().GetCpuId().sse42();
	if (DEKAF2_LIKELY(has_sse42))
#endif
	{
		const char* p = static_cast<const char*>(s);
		char ch = static_cast<char>(c);
		size_t pos = dekaf2::detail::sse::kFindLastOf(dekaf2::KStringView(p, n), dekaf2::KStringView(&ch, 1));
		if (pos != dekaf2::KStringView::npos)
		{
			return const_cast<char*>(p + pos);
		}
		return nullptr;
	}
#endif

	auto p = static_cast<const unsigned char*>(s);
	for (p += n; n > 0; --n)
	{
		if (*--p == c)
		{
			return const_cast<unsigned char*>(p);
		}
	}
	return nullptr;
}
#endif

namespace dekaf2 {

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

namespace detail {
constexpr KStringView kASCIISpaces;
}
constexpr KStringView::size_type KStringView::npos;
constexpr KStringView::value_type KStringView::s_0ch;

#endif

//-----------------------------------------------------------------------------
size_t kFind(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		// flip to single char search if only one char is in the search argument
		return kFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(pos >= haystack.size()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(needle.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(needle.size() > (haystack.size() - pos)))
	{
		return KStringView::npos;
	}

#ifndef DEKAF2_NO_GCC

	// glibc has an excellent memmem implementation, so we use it

	auto found = static_cast<const char*>(::memmem(haystack.data() + pos,
	                                               haystack.size() - pos,
	                                               needle.data(),
	                                               needle.size()));

	if (DEKAF2_UNLIKELY(!found))
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>(found - haystack.data());
	}

#else

	// libc has a very slow memmem implementation (about 100 times slower than glibc),
	// so we write our own

	for(;;)
	{
		auto found = static_cast<const char*>(::memchr(haystack.data() + pos,
													  needle[0],
													  (haystack.size() - pos - needle.size()) + 1));
		if (DEKAF2_UNLIKELY(!found))
		{
			return KStringView::npos;
		}

		pos = static_cast<size_t>(found - haystack.data());

		// due to aligned loads it is faster to compare the full needle again
		if (std::memcmp(haystack.data() + pos,
						needle.data(),
						needle.size()) == 0)
		{
			return pos;
		}

		++pos;
	}

#endif

#else // non-optimized

	return static_cast<KStringView::rep_type>(haystack).find(needle, pos);

#endif

}

#if (__GNUC__ > 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
//-----------------------------------------------------------------------------
size_t kRFind(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)

	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return kRFind(haystack, needle[0], pos);
	}

	if (DEKAF2_LIKELY(needle.size() <= haystack.size()))
	{
		pos = std::min(haystack.size() - needle.size(), pos);

		for(;;)
		{
			auto found = static_cast<const char*>(memrchr(haystack.data(),
			                                                needle[0],
			                                                pos+1));
			if (!found)
			{
				break;
			}

			pos = static_cast<size_t>(found - haystack.data());

			if (std::memcmp(haystack.data() + pos + 1,
			                needle.data() + 1,
			                needle.size() - 1) == 0)
			{
				return pos;
			}

			--pos;
		}
	}

	return KStringView::npos;

#else

	return static_cast<KStringView::rep_type>(haystack).rfind(needle, pos);

#endif

}
#if (__GNUC__ > 6)
#pragma GCC diagnostic pop
#endif

namespace detail { namespace stringview {

//-----------------------------------------------------------------------------
size_t kFindFirstOfInt(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return kFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(pos >= haystack.size()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(pos > 0))
	{
		haystack.remove_prefix(pos);
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
	if (result == KStringView::npos)
	{
		return result;
	}
	return result + pos;

} // kFindFirstOfInt

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfInt(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos >= haystack.size()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(pos > 0))
	{
		haystack.remove_prefix(pos);
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
	if (DEKAF2_LIKELY(result == KStringView::npos || pos == 0))
	{
		return result;
	}
	return result + pos;

} // kFindFirstNotOfInt

//-----------------------------------------------------------------------------
size_t kFindLastOfInt(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(needle.size() == 1))
	{
		return kRFind(haystack, needle[0], pos);
	}

	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(pos != KStringView::npos))
	{
		if (pos < haystack.size() - 1)
		{
			haystack.remove_suffix((haystack.size() - 1) - pos);
		}
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
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(haystack.empty()))
	{
		return KStringView::npos;
	}

	if (DEKAF2_UNLIKELY(pos != KStringView::npos))
	{
		if (pos < haystack.size() - 1)
		{
			haystack.remove_suffix((haystack.size() - 1) - pos);
		}
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
size_t kFindFirstOfUnescaped(KStringView haystack,
                             KStringView needle,
                             KStringView::value_type chEscape,
                             KStringView::size_type pos)
//-----------------------------------------------------------------------------
{
	auto iFound = haystack.find_first_of (needle, pos);

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

		iFound = haystack.find_first_of (needle, iFound + 1);
	} // while iFound

	return iFound;

} // kFindFirstOfUnescaped

//-----------------------------------------------------------------------------
size_t kFindUnescaped(KStringView haystack,
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
size_t kFindUnescaped(KStringView haystack,
                      KStringView needle,
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
		// having too much warnings
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
bool KStringView::In (KStringView sHaystack, value_type iDelim/*=','*/) const
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
	dekaf2::KLog::getInstance().debug_fun(1, sWhere, sWhat);
}

//----------------------------------------------------------------------
KStringView KStringView::MatchRegex(KStringView sRegEx, size_type pos) const
//----------------------------------------------------------------------
{
	return dekaf2::KRegex::Match(*this, sRegEx, pos);
}

//----------------------------------------------------------------------
std::vector<KStringView> KStringView::MatchRegexGroups(KStringView sRegEx, size_type pos) const
//----------------------------------------------------------------------
{
	return dekaf2::KRegex::MatchGroups(*this, sRegEx, pos);
}

//----------------------------------------------------------------------
KStringView KStringView::Right(size_type iCount) const
//----------------------------------------------------------------------
{
	if (iCount > size())
	{
		// do not warn
		iCount = size();
	}
	return KStringView(data() + size() - iCount, iCount);

} // Right

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
	dekaf2::kTrimLeft(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimLeft(KStringView sTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(*this, sTrim);
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
	dekaf2::kTrimRight(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KStringView& KStringView::TrimRight(KStringView sTrim)
//----------------------------------------------------------------------
{
	// for some reason the template generalization of kTrimRight(KStringView, KStringView)
	// does not work on entirely trimmable strings (with apple clang 12) - so we code the
	// same algorithm here again
	auto iDelete = find_last_not_of(sTrim);

	if (iDelete == npos)
	{
		clear();
	}
	else
	{
		erase(iDelete + 1);
	}

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
KStringView& KStringView::Trim(KStringView sTrim)
//----------------------------------------------------------------------
{
	TrimRight(sTrim);
	TrimLeft(sTrim);
	return *this;
}

//----------------------------------------------------------------------
bool KStringView::ClipAt(KStringView sClipAt)
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
bool KStringView::ClipAtReverse(KStringView sClipAtReverse)
//----------------------------------------------------------------------
{
	erase(0, find(sClipAtReverse));
	return true;

} // ClipAtReverse

static_assert(std::is_nothrow_move_constructible<KStringView>::value,
			  "KStringView is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2

