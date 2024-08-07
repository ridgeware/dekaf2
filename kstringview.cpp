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

DEKAF2_NAMESPACE_BEGIN

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

//-----------------------------------------------------------------------------
size_t kFindFirstOfUnescaped(const KStringView haystack,
							 const KFindSetOfChars& needles,
							 KStringView::value_type chEscape,
							 KStringView::size_type pos)
//-----------------------------------------------------------------------------
{
	auto iFound = needles.find_first_in(haystack, pos);

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

		iFound = needles.find_first_in(haystack, iFound + 1);

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
bool KStringView::Bool() const noexcept
//-----------------------------------------------------------------------------
{
	// * we need to map literal "true" --> true
	// * and literal "false" --> false
	// * as well as non-0 --> true
	// the literal versions are needed for conversions between JSON and KSQL,
	// and for various XML formats
	switch (size())
	{
		case 0:
			return false;

		case 1:
			return KASCII::kIsDigit(m_rep[0]) && m_rep[0] != '0';

		case 2:
			// check on,On,ON
			if (m_rep[0] == 'o')
			{
				if (m_rep[1] == 'n') return true;
			}
			else if (m_rep[0] == 'O' && (m_rep[1] == 'n' || m_rep[1] == 'N')) return true;
			break;

		case 3:
			// check yes,Yes,YES
			if (m_rep[0] == 'y')
			{
				if (m_rep[1] == 'e' && m_rep[2] == 's') return true;
			}
			else if (m_rep[0] == 'Y')
			{
				if (m_rep[1] == 'e' && m_rep[2] == 's') return true;
				if (m_rep[1] == 'E' && m_rep[2] == 'S') return true;
			}
			break;

		case 4:
			// check true,True,TRUE
			if (m_rep[0] == 't')
			{
				if (m_rep[1] == 'r' && m_rep[2] == 'u' && m_rep[3] == 'e') return true;
			}
			else if (m_rep[0] == 'T')
			{
				if (m_rep[1] == 'r')
				{
					if (m_rep[2] == 'u' && m_rep[3] == 'e') return true;
				}
				else if (m_rep[1] == 'R' && m_rep[2] == 'U' && m_rep[3] == 'E') return true;
			}
			break;

		default:
			break;
	}

	return Int16() != 0;
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
	kTrim(*this, [chTrim](value_type ch){ return ch == chTrim; } );
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


static_assert(std::is_nothrow_move_constructible<KStringView>::value,
			  "KStringView is intended to be nothrow move constructible, but is not!");


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringView::size_type KStringView::npos;
constexpr KStringView::value_type KStringView::s_0ch;

#endif

DEKAF2_NAMESPACE_END
