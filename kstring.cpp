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
KString& KString::append(const KString& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.append(str.m_rep, pos, n);
	DEKAF2_LOG_EXCEPTION
	return *this;
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
KString& KString::assign(const KString& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.assign(str.m_rep, pos, n);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n, const KString& str)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize = size();

	if (n > iSize || pos + n > iSize)
	{
		n = iSize - pos;
	}

	m_rep.replace(pos, n, str.m_rep);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize = size();

	if (n1 > iSize || pos1 + n1 > iSize)
	{
		n1 = iSize - pos1;
	}

	m_rep.replace(pos1, n1, str.m_rep, pos2, n2);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::replace(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize1 = size();
	const auto iSize2 = str.size();

	if (n1 > iSize1 || pos1 + n1 > iSize1)
	{
		n1 = iSize1 - pos1;
	}
	// avoid segfaults
	if (pos2 > iSize2)
	{
		kDebug (1, "pos2 ({}) exceeds size ({})", pos2, iSize2);
		pos2 = iSize2;
	}
	if (pos2 + n2 > iSize2)
	{
		kDebug (1, "pos2 ({}) + n ({}) exceeds size ({})", pos2, n2, iSize2);
		n2 = iSize2 - pos2;
	}

	m_rep.replace(pos1, n1, str.data()+pos2, n2);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

#endif

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, const value_type* s, size_type n2)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize = size();

	if (n1 > iSize || pos + n1 > iSize)
	{
		n1 = iSize - pos;
	}
	m_rep.replace(pos, n1, s ? s : "", n2);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, const value_type* s)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize = size();

	if (n1 > iSize || pos + n1 > iSize)
	{
		n1 = iSize - pos;
	}
	m_rep.replace(pos, n1, s ? s : "");
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, size_type n2, value_type c)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize = size();

	if (n1 > iSize || pos + n1 > iSize)
	{
		n1 = iSize - pos;
	}

	m_rep.replace(pos, n1, n2, c);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, const KString& str)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(i1, i2, str.m_rep);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(i1, i2, s ? s : "", n);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(i1, i2, il);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n, KStringView sv)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize = size();

	if (n > iSize || pos + n > iSize)
	{
		n = iSize - pos;
	}

	m_rep.replace(pos, n, sv.data(), sv.size());
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, KStringView sv)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.replace(i1, i2, sv.data(), sv.size());
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString KString::substr(size_type pos, size_type n/*=npos*/) const &
//------------------------------------------------------------------------------
{
	const auto iSize = size();

	if (DEKAF2_UNLIKELY(pos >= iSize))
	{
		return KString{};
	}

	if (n > iSize || pos + n > iSize)
	{
		n = iSize - pos;
	}
	return KString(data() + pos, n);
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
KString& KString::insert(size_type pos, const KString& str)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos, str.m_rep);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos1, const KString& str, size_type pos2, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos1, str.m_rep, pos2, n);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::insert(size_type pos1, const std::string& str, size_type pos2, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	const auto iSize2 = str.size();

	// avoid segfaults
	if (pos2 > iSize2)
	{
		kDebug (1, "pos2 ({}) exceeds size ({})", pos2, iSize2);
		pos2 = iSize2;
	}
	if (pos2 + n > iSize2)
	{
		kDebug (1, "pos2 ({}) + n ({}) exceeds size ({})", pos2, n, iSize2);
		n = iSize2 - pos2;
	}

	m_rep.insert(pos1, str.data()+pos2, n);
	DEKAF2_LOG_EXCEPTION
	return *this;
}
#endif

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos, s ? s : "", n);
	DEKAF2_LOG_EXCEPTION
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const value_type* s)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(pos, s ? s : "");
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
	DEKAF2_TRY_EXCEPTION
	return m_rep.insert(it, c);
	DEKAF2_LOG_EXCEPTION
	return end();
}

//------------------------------------------------------------------------------
KString& KString::insert (iterator it, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	DEKAF2_TRY_EXCEPTION
	m_rep.insert(it, il);
	DEKAF2_LOG_EXCEPTION
	return *this;
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
KString& KString::erase(size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	// std::string::erase(pos, n) only throws when pos > size()
	if (DEKAF2_LIKELY(pos < size()))
	{
		m_rep.erase(pos, n);
	}
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

	if (DEKAF2_LIKELY(pos < size()))
	{
		m_rep.erase(pos, 1);
	}

	return position;
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator first, iterator last)
//------------------------------------------------------------------------------
{
	// we turn this into an indexed erase because
	// the std::string iterator erase does not test for
	// iterator out of range and segfaults if out of range..
	auto pos = static_cast<size_type>(first - begin());

	if (DEKAF2_LIKELY(pos < size()))
	{
		auto n = static_cast<size_type>(last - first);
		m_rep.erase(pos, n);
	}

	return first;
}

//----------------------------------------------------------------------
KString::size_type KString::Replace(
		const KStringView sSearch,
		const KStringView sReplace,
		size_type pos,
		bool bReplaceAll)
//----------------------------------------------------------------------
{
	const auto iSize = size();

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
		return Replace(sSearch.front(), sReplace.front(), pos, bReplaceAll);
	}

	size_type iNumReplacement = 0;
	// use a non-const ref to the first element, as .data() is const with C++ < 17
	value_type* haystack = &m_rep[pos];
	size_type haystackSize = iSize - pos;

	auto pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

	if (DEKAF2_LIKELY(pszFound != nullptr))
	{

		if (iReplaceSize <= iSearchSize)
		{
			// execute an in-place substitution (C++17 actually has a non-const string.data())

			auto pszTarget = const_cast<value_type*>(haystack);

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);

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

				pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

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

			auto iResultSize = static_cast<size_type>(pszTarget - data());
			resize(iResultSize);

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
				sResult.append(*this, 0, pos);
			}

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);
				sResult.append(haystack, untouchedSize);
				sResult.append(sReplace.data(), iReplaceSize);

				haystack = pszFound + iSearchSize;
				haystackSize -= (iSearchSize + untouchedSize);

				pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), iSearchSize));

				++iNumReplacement;

				if (DEKAF2_UNLIKELY(bReplaceAll == false))
				{
					break;
				}
			}

			sResult.append(haystack, haystackSize);
			swap(sResult);

		}
	}

	return iNumReplacement;

} // Replace

//----------------------------------------------------------------------
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

	if (pos > iSize)
	{
		kDebug (1, "pos ({}) exceeds size ({})", pos, iSize);
		pos = iSize;
	}
	
	if (n > iSize)
	{
		n = iSize - pos;
	}
	else if (pos + n > iSize)
	{
		n = iSize - pos;
	}

	return KStringView(data() + pos, n);

} // ToView

//----------------------------------------------------------------------
KString& KString::MakeLower() &
//----------------------------------------------------------------------
{
	char* sOut { data() };
	Unicode::FromUTF8(*this, [&](Unicode::codepoint_t ch)
	{
		return Unicode::ToUTF8(kToLower(ch), sOut);
	});
	return *this;

} // MakeLower

//----------------------------------------------------------------------
KString& KString::MakeUpper() &
//----------------------------------------------------------------------
{
	char* sOut { data() };
	Unicode::FromUTF8(*this, [&](Unicode::codepoint_t ch)
	{
		return Unicode::ToUTF8(kToUpper(ch), sOut);
	});
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
	dekaf2::kTrimLeft(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
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
	dekaf2::kTrimRight(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
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
	dekaf2::kTrim(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
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

#if (__GNUC__ > 10)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
//----------------------------------------------------------------------
KString::size_type KString::RemoveChars(KStringView sChars)
//----------------------------------------------------------------------
{
	auto ilen = size();
	size_type pos;

	for (size_type lastpos = size(); (pos = find_last_of(sChars, lastpos)) != npos; )
	{
		erase(pos, 1);
		lastpos = pos;
	}

	return ilen - size();
}
#if (__GNUC__ > 10)
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
		return Unicode::ToUTF8(kToUpper(ch), sTransformed);
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
		return Unicode::ToUTF8(kToLower(ch), sTransformed);
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

