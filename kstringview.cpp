/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "kstringview.h"
#include "kstringutils.h"
#include "klog.h"
#include "dekaf2.h"
#include "bits/simd/kfindfirstof.h"

#ifndef __linux__
void* memrchr(const void* s, int c, size_t n)
{
#ifdef __x86_64__
	static bool has_sse42 = dekaf2::Dekaf().GetCpuId().sse42();

	if (DEKAF2_LIKELY(has_sse42))
	{
		const char* p = static_cast<const char*>(s);
		char ch = static_cast<char>(c);
		size_t pos = dekaf2::detail::kFindLastOfSSE(dekaf2::KStringView(p, n), dekaf2::KStringView(&ch, 1));
		if (pos != dekaf2::KStringView::npos)
		{
			return const_cast<char*>(p + pos);
		}
		else
		{
			return nullptr;
		}
	}
#endif

	const unsigned char* p = static_cast<const unsigned char*>(s);
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

#ifndef DEKAF2_HAS_CPP17
	
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

	if (DEKAF2_UNLIKELY(pos > haystack.size()))
	{
		return KStringView::npos;
	}

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

	return static_cast<KStringView::rep_type>(haystack).find(needle, pos);

#endif

}

//-----------------------------------------------------------------------------
size_t kRFind(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) || defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)
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

namespace detail { namespace stringview {

//-----------------------------------------------------------------------------
size_t kFindFirstOfBool(
        KStringView haystack,
        KStringView needle,
        size_t pos,
        bool bNot)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!bNot && needle.size() == 1))
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

#ifdef __x86_64__
	static bool has_sse42 = Dekaf().GetCpuId().sse42();

	if (DEKAF2_LIKELY(has_sse42))
	{
		if (DEKAF2_UNLIKELY(bNot))
		{
			auto result = detail::kFindFirstNotOfSSE(haystack, needle);
			if (DEKAF2_LIKELY(result == KStringView::npos || pos == 0))
			{
				return result;
			}
			else
			{
				return result + pos;
			}
		}
		else
		{
			auto result = detail::kFindFirstOfSSE(haystack, needle);
			if (DEKAF2_LIKELY(result == KStringView::npos || pos == 0))
			{
				return result;
			}
			else
			{
				return result + pos;
			}
		}
	}
#endif

	auto result = detail::kFindFirstOfNoSSE(haystack, needle, bNot);
	if (DEKAF2_LIKELY(result == KStringView::npos || pos == 0))
	{
		return result;
	}
	else
	{
		return result + pos;
	}

}

//-----------------------------------------------------------------------------
size_t kFindLastOfBool(
        KStringView haystack,
        KStringView needle,
        size_t pos,
        bool bNot)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(!bNot && needle.size() == 1))
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

#ifdef __x86_64__
	static bool has_sse42 = Dekaf().GetCpuId().sse42();

	if (DEKAF2_LIKELY(has_sse42))
	{
		if (DEKAF2_UNLIKELY(bNot))
		{
			return detail::kFindLastNotOfSSE(haystack, needle);
		}
		else
		{
			return detail::kFindLastOfSSE(haystack, needle);
		}
	}
#endif

	return detail::kFindLastOfNoSSE(haystack, needle, bNot);

}

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
		size_t iEscapes = 0;
		size_t iStart = iFound;

		while (iStart)
		{
			// count number of escape characters
			--iStart;
			if (haystack[iStart] != chEscape)
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
		size_t iEscapes = 0;
		size_t iStart = iFound;

		while (iStart)
		{
			// count number of escape characters
			--iStart;
			if (haystack[iStart] != chEscape)
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
KStringView::size_type KStringView::copy(iterator dest, size_type count, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos > size()))
	{
		kWarning("attempt to copy from past the end of string view of size {}: pos {}",
		         size(), pos);

		pos = size();
	}

	count = std::min(size() - pos, count);

	return static_cast<size_type>(std::copy(const_cast<char*>(begin() + pos),
	                                        const_cast<char*>(begin() + pos + count),
	                                        const_cast<char*>(dest))
	                              - dest);
}

//-----------------------------------------------------------------------------
/// nonstandard: emulate erase if range is at begin or end
KStringView::self_type& KStringView::erase(size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos > size()))
	{
			kWarning("attempt to erase past end of string view of size {}: pos {}, n {}",
			         size(), pos, n);
			pos = size();
	}

	n = std::min(n, size() - pos);

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

	return *this;

}

//-----------------------------------------------------------------------------
bool KStringView::In (KStringView sHaystack, value_type iDelim/*=','*/) const
//-----------------------------------------------------------------------------
{
	// gcc 4.8.5 needs the non-brace initialization here..
	auto& sNeedle(m_rep);

	size_t iNeedle = 0, iHaystack = 0; // Beginning indices
	size_t iNsize = sNeedle.size ();
	size_t iHsize = sHaystack.size (); // Ending

	while (iHaystack < iHsize)
	{
		iNeedle = 0;

		// Search for matching tokens
		while ( (iNeedle < iNsize) &&
			   (sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ((iNeedle >= iNsize) &&
			((sHaystack[iHaystack] == iDelim) || iHaystack >= iHsize))
		{
			return true;
		}

		// Advance to next delimiter
		while (iHaystack < iHsize && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// Pass by the delimiter if it exists
		iHaystack += (iHaystack < iHsize);
	}
	return false;

} // In

//-----------------------------------------------------------------------------
int16_t KStringView::Int16(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int16_t>(data(), size(), bIsHex);
}

//-----------------------------------------------------------------------------
uint16_t KStringView::UInt16(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint16_t>(data(), size(), bIsHex);
}

//-----------------------------------------------------------------------------
int32_t KStringView::Int32(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int32_t>(data(), size(), bIsHex);
}

//-----------------------------------------------------------------------------
uint32_t KStringView::UInt32(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint32_t>(data(), size(), bIsHex);
}

//-----------------------------------------------------------------------------
int64_t KStringView::Int64(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int64_t>(data(), size(), bIsHex);
}

//-----------------------------------------------------------------------------
uint64_t KStringView::UInt64(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint64_t>(data(), size(), bIsHex);
}

//-----------------------------------------------------------------------------
int128_t KStringView::Int128(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<int128_t>(data(), size(), bIsHex);
}

//-----------------------------------------------------------------------------
uint128_t KStringView::UInt128(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return kToInt<uint128_t>(data(), size(), bIsHex);
}

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
void KStringView::Warn(KStringView sWhat) const
//-----------------------------------------------------------------------------
{
	kWarning("{}", sWhat);
}

} // end of namespace dekaf2

