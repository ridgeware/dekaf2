/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2015, Ridgeware, Inc.
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

#include "kstring.h"
#include "kstringutils.h"
#include "klog.h"
#include "kregex.h"
#include "kutf8.h"
#include "kstack.h"
#include "kctype.h"

namespace dekaf2
{

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KString::size_type KString::npos;
constexpr KString::value_type KString::s_0ch;

#endif

#ifdef DEKAF2_IS_WINDOWS

void* memmem(void* _haystack, size_t haystack_len, const void* _needle, size_t needle_len)
{
	if (DEKAF2_UNLIKELY(needle_len == 0))
	{
		return nullptr;
	}

	char* haystack = static_cast<char*>(_haystack);
	const char* needle = static_cast<const char*>(_needle);
	size_t pos = 0;

	for(;;)
	{
		if (DEKAF2_UNLIKELY(pos >= haystack_len))
		{
			return nullptr;
		}

		if (DEKAF2_UNLIKELY(needle_len > haystack_len - pos))
		{
			return nullptr;
		}

		auto found = static_cast<const char*>(::memchr(haystack + pos,
													   needle[0],
													   (haystack_len - pos - needle_len) + 1));
		if (DEKAF2_UNLIKELY(!found))
		{
			return nullptr;
		}

		pos = static_cast<size_t>(found - haystack);

		// compare the full needle again, as it is most probably aligned
		if (std::memcmp(haystack + pos,
						needle,
						needle_len) == 0)
		{
			return haystack + pos;
		}

		++pos;
	}
}

#endif

KString::value_type KString::s_0ch_v { '\0' };

//------------------------------------------------------------------------------
void KString::log_exception(const std::exception& e, const char* sWhere)
//------------------------------------------------------------------------------
{
	KLog::getInstance().Exception(e, sWhere);
}

//------------------------------------------------------------------------------
void KString::resize_uninitialized(size_type n)
//------------------------------------------------------------------------------
{
#ifdef DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED
	static constexpr size_type LARGEST_SSO = 23;

	// never do this optimization for SSO strings
	if (n > LARGEST_SSO)
	{
		auto iSize = size();

		if (n > iSize)
		{
			auto iUninitialized = n - iSize;
			if (!iSize || (iUninitialized / iSize > 2))
			{
				// We can do this trick only with FBString, as std::string does not
				// offer a matching constructor: we malloc enough memory for the full
				// buffer, memcopy the existing string (if any) into it, construct a
				// new fbstring from that partly uninitialized buffer, and swap the string.

				// reserve the buffer
				char* buf = static_cast<char*>(std::malloc(iSize + iUninitialized + 1)); // do not forget the 0-terminator

				if (buf)
				{
					// copy existing string content into
					std::memcpy(buf, data(), iSize);
					// set 0 at end of buffer
					buf[iSize + iUninitialized] = 0;
					// construct string on the buffer
					KString sNew(buf, iSize + iUninitialized, iSize + iUninitialized + 1, AcquireMallocatedString{});
					// and swap it in for the existing string
					this->swap(sNew);
					// that's it
					return;
				}
			}
		}
	}
#endif

	// fallback to an initialized resize
	resize(n);
}

//------------------------------------------------------------------------------
// the method to which (nearly) all replaces reduce
KString& KString::replace(size_type pos, size_type n, KStringView sv)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(pos, n, sv.data(), sv.size());
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, KStringView sv)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed replace because
	// the std::string iterator replace does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(i1 - begin());
	auto n = static_cast<size_type>(i2 - i1);
	return replace (pos, n, sv);
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, size_type n2, value_type c)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(pos, n1, n2, c);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	// we turn this into an indexed replace because
	// the std::string iterator replace does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(i1 - begin());
	auto n = static_cast<size_type>(i2 - i1);
	m_rep.replace(pos, n, il);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString KString::substr(size_type pos, size_type n/*=npos*/) &&
//------------------------------------------------------------------------------
{
	const auto iSize = size();

	if (DEKAF2_UNLIKELY(pos >= iSize))
	{
		clear();
	}
	else
	{
		erase(0, pos);

		if (n < iSize - pos)
		{
			resize(n);
		}
	}

	return std::move(*this);
}

//------------------------------------------------------------------------------
KString::size_type KString::copy(value_type* s, size_type n, size_type pos) const
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	return m_rep.copy(s, n, pos);
	DEKAF2_LOG_EXCEPTION
	return 0;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, KStringView sv)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos, sv.data(), sv.size());
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, size_type n, value_type c)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos, n, c);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::insert(iterator it, value_type c)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed insert because
	// the std::string iterator replace does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(it - begin());
	// std::string::insert(pos, ..) only throws when pos > size()
	if (DEKAF2_LIKELY(pos <= size()))
	{
		m_rep.insert(pos, 1, c);
		return it;
	}
	else
	{
		kWarning("iterator out of range");
		return end();
	}
}

//------------------------------------------------------------------------------
KString::iterator KString::insert(iterator it, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	// this check is not part of the standard, and there is no index version of
	// the insert of an initializer list, therefore we check explicitly
	if (DEKAF2_UNLIKELY((it < begin() || it > end())))
	{
		kWarning("iterator out of range");
		return end();
	}

#if !defined(DEKAF2_USE_FBSTRING_AS_KSTRING) && \
	defined(DEKAF2_GLIBCXX_VERSION_MAJOR) && \
	(DEKAF2_GLIBCXX_VERSION_MAJOR < 9)
	// older versions of libstdc++ do not return an iterator, but void
	// see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83328
	//
	// workaround:
	// find index, insert, and calculate an iterator to that index (might point
	// to somewhere else because of reallocs)
	//
	auto pos = static_cast<size_type>(it - begin());
	m_rep.insert(it, il);
	return begin() + pos;
#else
	return m_rep.insert(it, il);
#endif
}

//------------------------------------------------------------------------------
KString& KString::erase(size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.erase(pos, n);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator position)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed erase because
	// the std::string iterator erase does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(position - begin());

	if (DEKAF2_LIKELY(pos <= size()))
	{
		m_rep.erase(pos, 1);
		return position;
	}
	else
	{
		kWarning("iterator out of range");
		return end();
	}
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator first, iterator last)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed erase because
	// the std::string iterator erase does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(first - begin());

	if (DEKAF2_LIKELY(pos <= size()))
	{
		auto n = static_cast<size_type>(last - first);
		m_rep.erase(pos, n);
		return first;
	}
	else
	{
		kWarning("iterator out of range");
		return end();
	}
}

//----------------------------------------------------------------------
KString::size_type kReplace(
		KStringRef& sString,
		KStringRef::value_type chSearch,
		KStringRef::value_type chReplace,
		KStringRef::size_type pos,
		bool bReplaceAll)
//----------------------------------------------------------------------
{
	KStringRef::size_type iReplaced{0};

	while ((pos = sString.find(chSearch, pos)) != npos)
	{
		sString[pos] = chReplace;
		++pos;
		++iReplaced;

		if (!bReplaceAll)
		{
			break;
		}
	}

	return iReplaced;

} // kReplace

//----------------------------------------------------------------------
KString::size_type kReplace(
		KStringRef& sString,
		const KStringView sSearch,
		const KStringView sReplace,
		KStringRef::size_type pos,
		bool bReplaceAll)
//----------------------------------------------------------------------
{
	const auto iSize = sString.size();

	if (DEKAF2_UNLIKELY(pos >= iSize))
	{
		return 0;
	}

	const auto iSearchSize = sSearch.size();

	if (DEKAF2_UNLIKELY(sSearch.empty() || iSize - pos < iSearchSize))
	{
		return 0;
	}

	const auto iReplaceSize = sReplace.size();

	if (DEKAF2_UNLIKELY(iSearchSize == 1 && iReplaceSize == 1))
	{
		return kReplace(sString, sSearch.front(), sReplace.front(), pos, bReplaceAll);
	}

	KStringRef::size_type iNumReplacement = 0;
	// use a non-const ref to the first element, as .data() is const with C++ < 17
	KStringRef::value_type* haystack = &sString[pos];
	KStringRef::size_type haystackSize = iSize - pos;

	auto pszFound = static_cast<KStringRef::value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

	if (DEKAF2_LIKELY(pszFound != nullptr))
	{

		if (iReplaceSize <= iSearchSize)
		{
			// execute an in-place substitution

			auto pszTarget = haystack;

			while (pszFound)
			{
				auto untouchedSize = static_cast<KStringRef::size_type>(pszFound - haystack);

				if (pszTarget < haystack)
				{
					std::memmove(pszTarget, haystack, untouchedSize);
				}

				pszTarget += untouchedSize;

				if (DEKAF2_LIKELY(iReplaceSize != 0))
				{
					std::memmove(pszTarget, sReplace.data(), iReplaceSize);
					pszTarget += iReplaceSize;
				}

				haystack = pszFound + iSearchSize;
				haystackSize -= (iSearchSize + untouchedSize);

				pszFound = static_cast<KStringRef::value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

				++iNumReplacement;

				if (DEKAF2_UNLIKELY(bReplaceAll == false))
				{
					break;
				}
			}

			if (DEKAF2_LIKELY(haystackSize > 0))
			{
				std::memmove(pszTarget, haystack, haystackSize);
				pszTarget += haystackSize;
			}

			auto iResultSize = static_cast<KStringRef::size_type>(pszTarget - sString.data());
			sString.resize(iResultSize);

		}
		else
		{
			// execute a copy substitution

			KString sResult;
			// make room for at least one replace without realloc
			sResult.reserve(iSize + iReplaceSize - iSearchSize);
			
			if (DEKAF2_UNLIKELY(pos))
			{
				// copy the skipped part of the source string
				sResult.append(sString, 0, pos);
			}

			while (pszFound)
			{
				auto untouchedSize = static_cast<KStringRef::size_type>(pszFound - haystack);
				sResult.append(haystack, untouchedSize);
				sResult.append(sReplace.data(), iReplaceSize);

				haystack = pszFound + iSearchSize;
				haystackSize -= (iSearchSize + untouchedSize);

				pszFound = static_cast<KStringRef::value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

				++iNumReplacement;

				if (DEKAF2_UNLIKELY(bReplaceAll == false))
				{
					break;
				}
			}

			sResult.append(haystack, haystackSize);
			sString.swap(sResult);

		}
	}

	return iNumReplacement;

} // kReplace

//----------------------------------------------------------------------
// we keep a KString version of kReplace, because it uses an unchecked subscript access
KString::size_type KString::Replace(
		value_type chSearch,
		value_type chReplace,
		size_type pos,
		bool bReplaceAll)
//----------------------------------------------------------------------
{
	size_type iReplaced{0};

	while ((pos = find(chSearch, pos)) != npos)
	{
		m_rep[pos] = chReplace;
		++pos;
		++iReplaced;

		if (!bReplaceAll)
		{
			break;
		}
	}

	return iReplaced;

} // Replace

//----------------------------------------------------------------------
KString::size_type KString::ReplaceRegex(const KStringView sRegEx, const KStringView sReplaceWith, bool bReplaceAll)
//----------------------------------------------------------------------
{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	return dekaf2::KRegex::Replace(*this, sRegEx, sReplaceWith, bReplaceAll);
#else
	return dekaf2::KRegex::Replace(m_rep, sRegEx, sReplaceWith, bReplaceAll);
#endif
}

//----------------------------------------------------------------------
KStringViewZ KString::ToView(size_type pos) const
//----------------------------------------------------------------------
{
	KStringViewZ sv(*this);
	sv.remove_prefix(pos);
	return sv;

} // ToView

//----------------------------------------------------------------------
KStringView KString::ToView(size_type pos, size_type n) const
//----------------------------------------------------------------------
{
	const auto iSize = size();

	if (DEKAF2_UNLIKELY(pos > iSize))
	{
		kDebug (1, "pos ({}) exceeds size ({})", pos, iSize);
		pos = iSize;
	}
	
	if (DEKAF2_UNLIKELY(n > iSize))
	{
		n = iSize - pos;
	}
	else if (DEKAF2_UNLIKELY(pos + n > iSize))
	{
		n = iSize - pos;
	}

	return KStringView(data() + pos, n);

} // ToView

/*
 for the following case folds, the utf8 sizes of
 upper and lower codepoint are not the same -
 therefore we cannot do a case change in place..

 size 2 != size 1, for fold from İ to i (toupper)
 size 2 != size 1, for fold from ı to I (tolower)
 size 2 != size 1, for fold from ſ to S (tolower)
 size 2 != size 3, for fold from Ⱥ to ⱥ (toupper)
 size 2 != size 3, for fold from Ⱦ to ⱦ (toupper)
 size 2 != size 3, for fold from ȿ to Ȿ (tolower)
 size 2 != size 3, for fold from ɐ to Ɐ (tolower)
 size 2 != size 3, for fold from ɑ to Ɑ (tolower)
 size 2 != size 3, for fold from ɒ to Ɒ (tolower)
 size 2 != size 3, for fold from ɜ to Ɜ (tolower)
 size 2 != size 3, for fold from ɡ to Ɡ (tolower)
 size 2 != size 3, for fold from ɥ to Ɥ (tolower)
 size 2 != size 3, for fold from ɦ to Ɦ (tolower)
 size 2 != size 3, for fold from ɫ to Ɫ (tolower)
 size 2 != size 3, for fold from ɬ to Ɬ (tolower)
 size 2 != size 3, for fold from ɱ to Ɱ (tolower)
 size 2 != size 3, for fold from ɽ to Ɽ (tolower)
 size 2 != size 3, for fold from ʂ to Ʂ (tolower)
 size 2 != size 3, for fold from ʇ to Ʇ (tolower)
 size 2 != size 3, for fold from ʝ to Ʝ (tolower)
 size 2 != size 3, for fold from ʞ to Ʞ (tolower)
 size 3 != size 2, for fold from ᲀ to В (tolower)
 size 3 != size 2, for fold from ᲁ to Д (tolower)
 size 3 != size 2, for fold from ᲂ to О (tolower)
 size 3 != size 2, for fold from ᲃ to С (tolower)
 size 3 != size 2, for fold from ᲅ to Т (tolower)
 size 3 != size 2, for fold from ᲆ to Ъ (tolower)
 size 3 != size 2, for fold from ᲇ to Ѣ (tolower)
 size 3 != size 2, for fold from ẞ to ß (toupper)
 size 3 != size 2, for fold from ι to Ι (tolower)
 size 3 != size 2, for fold from Ω to ω (toupper)
 size 3 != size 1, for fold from K to k (toupper)
 size 3 != size 2, for fold from Å to å (toupper)
 */

//----------------------------------------------------------------------
KString& KString::MakeLower() &
//----------------------------------------------------------------------
{
	KString sLower = kToLower(ToView());
	sLower.swap(*this);
	return *this;

} // MakeLower

//----------------------------------------------------------------------
KString& KString::MakeUpper() &
//----------------------------------------------------------------------
{
	KString sUpper = kToUpper(ToView());
	sUpper.swap(*this);
	return *this;

} // MakeUpper

//----------------------------------------------------------------------
KString& KString::MakeLowerLocale() &
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = static_cast<value_type>(std::tolower(static_cast<unsigned char>(it)));
	}
	return *this;

} // MakeLowerLocale

//----------------------------------------------------------------------
KString& KString::MakeUpperLocale() &
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = static_cast<value_type>(std::toupper(static_cast<unsigned char>(it)));
	}
	return *this;

} // MakeUpperLocale

//----------------------------------------------------------------------
KString& KString::MakeLowerASCII() &
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = KASCII::kToLower(it);
	}
	return *this;

} // MakeLowerASCII

//----------------------------------------------------------------------
KString& KString::MakeUpperASCII() &
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = KASCII::kToUpper(it);
	}
	return *this;

} // MakeUpperASCII

//----------------------------------------------------------------------
KString& KString::PadLeft(size_t iWidth, value_type chPad) &
//----------------------------------------------------------------------
{
	dekaf2::kPadLeft(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::PadRight(size_t iWidth, value_type chPad) &
//----------------------------------------------------------------------
{
	dekaf2::kPadRight(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft() &
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(value_type chTrim) &
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(KStringView sTrim) &
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight() &
//----------------------------------------------------------------------
{
	dekaf2::kTrimRight(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(value_type chTrim) &
//----------------------------------------------------------------------
{
	dekaf2::kTrimRight(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(KStringView sTrim) &
//----------------------------------------------------------------------
{
	dekaf2::kTrimRight(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim() &
//----------------------------------------------------------------------
{
	dekaf2::kTrim(*this);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(value_type chTrim) &
//----------------------------------------------------------------------
{
	dekaf2::kTrim(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(KStringView sTrim) &
//----------------------------------------------------------------------
{
	dekaf2::kTrim(*this, sTrim);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Collapse() &
//----------------------------------------------------------------------
{
	return kCollapse(*this, detail::kASCIISpaces, ' ');
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Collapse(KStringView svCollapse, value_type chTo) &
//----------------------------------------------------------------------
{
	kCollapse(*this, svCollapse, chTo);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::CollapseAndTrim() &
//----------------------------------------------------------------------
{
	return kCollapseAndTrim(*this, detail::kASCIISpaces, ' ');
	return *this;
}

//----------------------------------------------------------------------
KString& KString::CollapseAndTrim(KStringView svCollapse, value_type chTo) &
//----------------------------------------------------------------------
{
	kCollapseAndTrim(*this, svCollapse, chTo);
	return *this;
}

//----------------------------------------------------------------------
bool KString::ClipAt(KStringView sClipAt)
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
bool KString::ClipAtReverse(KStringView sClipAtReverse)
//----------------------------------------------------------------------
{
	erase(0, find(sClipAtReverse));
	return true;

} // ClipAtReverse

#if (__GNUC__ > 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
//----------------------------------------------------------------------
KString::size_type KString::RemoveChars(KStringView sChars)
//----------------------------------------------------------------------
{
	auto iOrigLen = size();
	auto pos      = iOrigLen;

	for (;;)
	{
		pos = find_last_of(sChars, pos);

		if (DEKAF2_UNLIKELY(pos >= size()))
		{
			break;
		}

		m_rep.erase(pos, 1);
	}

	return iOrigLen - size();

} // RemoveChars
#if (__GNUC__ > 6)
#pragma GCC diagnostic pop
#endif

//-----------------------------------------------------------------------------
float KString::Float() const noexcept
//-----------------------------------------------------------------------------
{
	return std::strtof(c_str(), nullptr);
}

//-----------------------------------------------------------------------------
double KString::Double() const noexcept
//-----------------------------------------------------------------------------
{
	return std::strtod(c_str(), nullptr);
}

#ifdef DEKAF2_WITH_DEPRECATED_KSTRING_MEMBER_FUNCTIONS

//----------------------------------------------------------------------
bool KString::FindRegex(KStringView regex) const
//----------------------------------------------------------------------
{
	return KRegex::Matches(ToView(), regex);
}

//----------------------------------------------------------------------
bool KString::FindRegex(KStringView regex, unsigned int* start, unsigned int* end, size_type pos) const
//----------------------------------------------------------------------
{
	KStringView sv(ToView());
	if (pos > 0)
	{
		sv.remove_prefix(pos);
	}
	size_t s, e;
	auto ret = KRegex::Matches(sv, regex, s, e);
	if (s == e)
	{
		if (start) *start = 0;
		if (end)   *end   = 0;
	}
	else
	{
		// these casts are obviously bogus as size_t on
		// 64 bit systems is larger than unsigned int
		// - but this is what the old dekaf KString expects
		// as return values.. so better do not use it with
		// strings larger than 2^31 chars
		if (start) *start = static_cast<unsigned int>(s + pos);
		if (end)   *end   = static_cast<unsigned int>(e + pos);
	}
	return ret;
}

//----------------------------------------------------------------------
KString::size_type KString::SubRegex(KStringView pszRegEx, KStringView pszReplaceWith, bool bReplaceAll, size_type* piIdxOffset)
//----------------------------------------------------------------------
{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	return KRegex::Replace(*this, pszRegEx, pszReplaceWith, bReplaceAll);
#else
	return KRegex::Replace(m_rep, pszRegEx, pszReplaceWith, bReplaceAll);
#endif
}

#endif

//----------------------------------------------------------------------
KString kToUpper(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.reserve(sInput.size());

	Unicode::FromUTF8(sInput, [&](Unicode::codepoint_t ch)
	{
		Unicode::ToUTF8(kToUpper(ch), sTransformed);
		return true;
	});

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToLower(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.reserve(sInput.size());

	Unicode::FromUTF8(sInput, [&](Unicode::codepoint_t ch)
	{
		Unicode::ToUTF8(kToLower(ch), sTransformed);
		return true;
	});

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToUpperLocale(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.reserve(sInput.size());

	for (const auto& it : sInput)
	{
		sTransformed += static_cast<KString::value_type>(std::toupper(static_cast<unsigned char>(it)));
	}

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToLowerLocale(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.reserve(sInput.size());

	for (const auto& it : sInput)
	{
		sTransformed += static_cast<KString::value_type>(std::tolower(static_cast<unsigned char>(it)));
	}

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToUpperASCII(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.reserve(sInput.size());

	for (const auto& it : sInput)
	{
		sTransformed += KASCII::kToUpper(it);
	}

	return sTransformed;
}

//----------------------------------------------------------------------
KString kToLowerASCII(KStringView sInput)
//----------------------------------------------------------------------
{
	KString sTransformed;
	sTransformed.reserve(sInput.size());

	for (const auto& it : sInput)
	{
		sTransformed += KASCII::kToLower(it);
	}

	return sTransformed;
}

//-----------------------------------------------------------------------------
KString KString::signed_to_string(int64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase)
//-----------------------------------------------------------------------------
{
	return kSignedToString<KString>(i, iBase, bZeroPad, bUppercase);
}

//-----------------------------------------------------------------------------
KString KString::unsigned_to_string(uint64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase)
//-----------------------------------------------------------------------------
{
	return kUnsignedToString<KString>(i, iBase, bZeroPad, bUppercase);
}

static_assert(std::is_nothrow_move_constructible<KString>::value,
			  "KString is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2


//----------------------------------------------------------------------
std::istream& std::getline(std::istream& stream, dekaf2::KString& str)
//----------------------------------------------------------------------
{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	dekaf2::KString::string_type& sref = str.str();
	return getline(stream, sref);
#else
	return std::getline(stream, str.str());
#endif
}

//----------------------------------------------------------------------
std::istream& std::getline(std::istream& stream, dekaf2::KString& str, dekaf2::KString::value_type delimiter)
//----------------------------------------------------------------------
{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	dekaf2::KString::string_type& sref = str.str();
	return getline(stream, sref, delimiter);
#else
	return std::getline(stream, str.str(), delimiter);
#endif
}

