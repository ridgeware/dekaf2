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

#include <stdio.h>
#include <assert.h>
#include "kstring.h"
#include "kstringutils.h"
#include "klog.h"
#include "kregex.h"

namespace dekaf2
{

const KString::size_type KString::npos;

//------------------------------------------------------------------------------
void KString::log_exception(std::exception& e, KStringView sWhere)
//------------------------------------------------------------------------------
{
	kException(e);
}

//------------------------------------------------------------------------------
KString& KString::append(const string_type& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.append(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::append(const std::string& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.append(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}
#endif

//------------------------------------------------------------------------------
KString& KString::assign(const string_type& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.assign(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::assign(const std::string& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.assign(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}
#endif

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n, const string_type& str)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n, str);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos1, n1, str, pos2, n2);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::replace(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2)
//------------------------------------------------------------------------------
{
	try {
		// avoid segfaults
		if (pos2 > str.size())
		{
			kWarning("pos2 ({}) exceeds size ({})", pos2, str.size());
			pos2 = str.size();
		}
		if (pos2 + n2 > str.size())
		{
			kWarning("pos2 ({}) + n ({}) exceeds size ({})", pos2, n2, str.size());
			n2 = str.size() - pos2;
		}
		m_rep.replace(pos1, n1, str.data()+pos2, n2);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#endif

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, const value_type* s, size_type n2)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n1, s ? s : "", n2);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, const value_type* s)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n1, s ? s : "");
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, size_type n2, value_type c)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n1, n2, c);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, const string_type& str)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, str);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, s ? s : "", n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, il);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n, KStringView sv)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n, sv.data(), sv.size());
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, KStringView sv)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, sv.data(), sv.size());
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString KString::substr(size_type pos, size_type n) const
//------------------------------------------------------------------------------
{
	try {
		return m_rep.substr(pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return KString();
}

//------------------------------------------------------------------------------
KString::size_type KString::copy(value_type* s, size_type n, size_type pos) const
//------------------------------------------------------------------------------
{
	try {
		return m_rep.copy(s, n, pos);
	} catch (std::exception& e) {
		kException(e);
	}
	return 0;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const string_type& str)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, str);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos1, const string_type& str, size_type pos2, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos1, str, pos2, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::insert(size_type pos1, const std::string& str, size_type pos2, size_type n)
//------------------------------------------------------------------------------
{
	try {
		// avoid segfaults
		if (pos2 > str.size())
		{
			kWarning("pos2 ({}) exceeds size ({})", pos2, str.size());
			pos2 = str.size();
		}
		if (pos2 + n > str.size())
		{
			kWarning("pos2 ({}) + n ({}) exceeds size ({})", pos2, n, str.size());
			n = str.size() - pos2;
		}
		m_rep.insert(pos1, str.data()+pos2, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}
#endif

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, s ? s : "", n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const value_type* s)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, s ? s : "");
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, size_type n, value_type c)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, n, c);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::insert(iterator it, value_type c)
//------------------------------------------------------------------------------
{
	try {
		return m_rep.insert(it, c);
	} catch (std::exception& e) {
		kException(e);
	}
	return end();
}

//------------------------------------------------------------------------------
KString& KString::insert (iterator it, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(it, il);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, KStringView sv)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, sv.data(), sv.size());
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::erase(size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.erase(pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator position)
//------------------------------------------------------------------------------
{
	try {
		// we turn this into a indexed erase, because
		// the std::string iterator erase does not test for
		// iterator out of range and segfaults if out of range..
		m_rep.erase(static_cast<size_type>(position - begin()), 1);
		return position;
	} catch (std::exception& e) {
		kException(e);
	}
	return end();
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator first, iterator last)
//------------------------------------------------------------------------------
{
	try {
		// we turn this into a indexed erase, because
		// the std::string iterator erase does not test for
		// iterator out of range and segfaults if out of range..
		m_rep.erase(static_cast<size_type>(first - begin()),
		            static_cast<size_type>(last - first));
		return first;
	} catch (std::exception& e) {
		kException(e);
	}
	return end();
}

#if (DEKAF2_GCC_VERSION >= 40600) && (DEKAF2_USE_OPTIMIZED_STRING_FIND)
// In contrast to most of the other optimized find functions we do not
// delegate this one to KStringView. The reason is that for find_first_of()
// we can use the ultra fast glibc strcspn() function, it even outrivals
// by a factor of two the sse 4.2 vector search implemented for folly::Range.
// We can however not use strcspn() for ranges (including KStringView),
// as there is no trailing zero byte.
//----------------------------------------------------------------------
KString::size_type KString::find_first_of(KStringView sv, size_type pos) const
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos >= size()))
	{
		return npos;
	}

	if (DEKAF2_UNLIKELY(sv.size() == 1))
	{
		return find(sv[0], pos);
	}

	// This is not as costly as it looks due to SSO. And there is no
	// way around it if we want to use strcspn() and its enormous performance.
	KString search(sv);

	// now we need to filter out the possible 0 chars in the search string
	bool bHasZero(false);
	size_type iHasZero(0);
	for (;;)
	{
		iHasZero = search.find('\0', iHasZero);
		if (iHasZero != npos)
		{
			search.erase(iHasZero, 1);
			bHasZero = true;
		}
		else
		{
			break;
		}
	}

	// we now can safely use strcspn(), as all strings are 0 terminated.
	for (;;)
	{
		auto retval = std::strcspn(c_str() + pos, search.c_str()) + pos;
		if (retval >= size())
		{
			return npos;
		}
		else if (m_rep[retval] != '\0' || bHasZero)
		{
			return retval;
		}
		// we stopped on a zero char in the middle of the string and
		// had no zero in the search - restart the search
		pos += retval + 1;
	}
}
#endif

#if (DEKAF2_GCC_VERSION >= 40600) && (DEKAF2_USE_OPTIMIZED_STRING_FIND)
// In contrast to most of the other optimized find functions we do not
// delegate this one to KStringView. The reason is that for find_first_not_of()
// we can use the ultra fast glibc strspn() function, it even outrivals
// by a factor of two the sse 4.2 vector search implemented for folly::Range.
// We can however not use strspn() for ranges (including KStringView),
// as there is no trailing zero byte.
//----------------------------------------------------------------------
KString::size_type KString::find_first_not_of(KStringView sv, size_type pos) const
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos >= size()))
	{
		return npos;
	}

	// This is not as costly as it looks due to SSO. And there is no
	// way around it if we want to use strspn() and its enormous performance.
	KString search(sv);

	// now we need to filter out the possible 0 chars in the search string
	bool bHasZero(false);
	size_type iHasZero(0);
	for (;;)
	{
		iHasZero = search.find('\0', iHasZero);
		if (iHasZero != npos)
		{
			search.erase(iHasZero, 1);
			bHasZero = true;
		}
		else
		{
			break;
		}
	}

	// we now can safely use strspn(), as all strings are 0 terminated.
	for (;;)
	{
		auto retval = std::strspn(c_str() + pos, search.c_str()) + pos;
		if (retval >= size())
		{
			return npos;
		}
		else if (m_rep[retval] == '\0' && bHasZero)
		{
			// we stopped on a zero char in the middle of the string and
			// had no zero in the search - restart the search
			pos += retval + 1;
		}
		else
		{
			return retval;
		}
	}
}
#endif

//----------------------------------------------------------------------
KString::size_type KString::Replace(
        KStringView sSearch,
        KStringView sReplace,
        size_type pos,
        bool bReplaceAll)
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(pos >= size()))
	{
		return 0;
	}

	if (DEKAF2_UNLIKELY(sSearch.empty() || size() - pos < sSearch.size()))
	{
		return 0;
	}

	typedef KString::size_type size_type;
	typedef KString::value_type value_type;

	size_type iNumReplacement = 0;
	// use a non-const ref to the first element, as .data() is const with C++ < 17
	value_type* haystack = &m_rep[pos];
	size_type haystackSize = size() - pos;

	value_type* pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), sSearch.size()));

	if (DEKAF2_LIKELY(pszFound != nullptr))
	{

		if (sReplace.size() <= sSearch.size())
		{
			// execute an in-place substitution (C++17 actually has a non-const string.data())
			value_type* pszTarget = const_cast<value_type*>(haystack);

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);
				if (pszTarget < haystack)
				{
					std::memmove(pszTarget, haystack, untouchedSize);
				}
				pszTarget += untouchedSize;

				if (DEKAF2_LIKELY(sReplace.empty() == false))
				{
					std::memmove(pszTarget, sReplace.data(), sReplace.size());
					pszTarget += sReplace.size();
				}

				haystack = pszFound + sSearch.size();
				haystackSize -= (sSearch.size() + untouchedSize);

				pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), sSearch.size()));

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
			sResult.reserve(size());

			while (pszFound)
			{
				auto untouchedSize = static_cast<size_type>(pszFound - haystack);
				sResult.append(haystack, untouchedSize);
				sResult.append(sReplace.data(), sReplace.size());

				haystack = pszFound + sSearch.size();
				haystackSize -= (sSearch.size() + untouchedSize);

				pszFound = static_cast<value_type*>(memmem(haystack, haystackSize, sSearch.data(), sSearch.size()));

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
}

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
}

//----------------------------------------------------------------------
KString::size_type KString::Replace(
        KStringView sSearch,
        value_type chReplace,
        size_type pos,
        bool bReplaceAll)
//----------------------------------------------------------------------
{
	size_type iReplaced{0};

	while ((pos = find_first_of(sSearch, pos)) != npos)
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
}

//----------------------------------------------------------------------
KString::size_type KString::ReplaceRegex(KStringView sRegEx, KStringView sReplaceWith, bool bReplaceAll)
//----------------------------------------------------------------------
{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	return dekaf2::KRegex::Replace(*this, sRegEx, sReplaceWith, bReplaceAll);
#else
	return dekaf2::KRegex::Replace(m_rep, sRegEx, sReplaceWith, bReplaceAll);
#endif
}

//----------------------------------------------------------------------
KStringView KString::ToView(size_type pos, size_type n) const
//----------------------------------------------------------------------
{
	if (pos > size())
	{
		kWarning("pos ({}) exceeds size ({})", pos, size());
		pos = size();
	}
	if (n == npos)
	{
		n = size() - pos;
	}
	else if (pos + n > size())
	{
		kWarning("pos ({}) + n ({}) exceeds size ({})", pos, n, size());
		n = size() - pos;
	}
	return KStringView(data() + pos, n);
}

//----------------------------------------------------------------------
KString& KString::MakeLower()
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = static_cast<value_type>(std::tolower(static_cast<unsigned char>(it)));
	}
	return *this;
} // MakeLower

//----------------------------------------------------------------------
KString& KString::MakeUpper()
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = static_cast<value_type>(std::toupper(static_cast<unsigned char>(it)));
	}
	return *this;
} // MakeUpper

//----------------------------------------------------------------------
KStringView KString::Left(size_type iCount)
//----------------------------------------------------------------------
{
	if (iCount >= size())
	{
		return *this;
	}
	return KStringView(m_rep.data(), iCount);
} // Left

//----------------------------------------------------------------------
KStringView KString::Right(size_type iCount)
//----------------------------------------------------------------------
{
	if (iCount >= size())
	{
		return *this;
	}
	else if (!iCount || !size())
	{
		// return an empty string
		return KStringView("");
	}
	else
	{
		return KStringView(m_rep.data() + size() - iCount, iCount);
	}
} // Right

//----------------------------------------------------------------------
KString& KString::PadLeft(size_t iWidth, value_type chPad)
//----------------------------------------------------------------------
{
	dekaf2::kPadLeft(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::PadRight(size_t iWidth, value_type chPad)
//----------------------------------------------------------------------
{
	dekaf2::kPadRight(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft()
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(KStringView sTrim)
//----------------------------------------------------------------------
{
	if (sTrim.size() == 1)
	{
		return TrimLeft(sTrim[0]);
	}
	dekaf2::kTrimLeft(m_rep, [sTrim](value_type ch){ return memchr(sTrim.data(), ch, sTrim.size()) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight()
//----------------------------------------------------------------------
{
	dekaf2::kTrimRight(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrimRight(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(KStringView sTrim)
//----------------------------------------------------------------------
{
	if (sTrim.size() == 1)
	{
		return TrimRight(sTrim[0]);
	}
	dekaf2::kTrimRight(m_rep, [sTrim](value_type ch){ return memchr(sTrim.data(), ch, sTrim.size()) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim()
//----------------------------------------------------------------------
{
	dekaf2::kTrim(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrim(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(KStringView sTrim)
//----------------------------------------------------------------------
{
	if (sTrim.size() == 1)
	{
		return Trim(sTrim[0]);
	}
	dekaf2::kTrim(m_rep, [sTrim](value_type ch){ return memchr(sTrim.data(), ch, sTrim.size()) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::ClipAt(KStringView sClipAt)
//----------------------------------------------------------------------
{
	size_type pos = find(sClipAt);
	if (pos != npos)
	{
		erase(pos);
	}
	return *this;
} // ClipAt

//----------------------------------------------------------------------
KString& KString::ClipAtReverse(KStringView sClipAtReverse)
//----------------------------------------------------------------------
{
	size_type pos = find(sClipAtReverse);
	if (pos != npos)
	{
		erase(0, pos);
	}
	return *this;
} // ClipAtReverse

//----------------------------------------------------------------------
void KString::RemoveIllegalChars(KStringView sIllegalChars)
//----------------------------------------------------------------------
{
	size_type pos;
	for (size_type lastpos = size(); (pos = find_last_of(sIllegalChars, lastpos)) != npos; )
	{
		erase(pos, 1);
		lastpos = pos;
	}
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

//-----------------------------------------------------------------------------
bool KString::In (KStringView sHaystack, value_type iDelim/*=','*/) const
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

} // kstrin

//-----------------------------------------------------------------------------
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim/*=','*/)
//-----------------------------------------------------------------------------
{
	if (!sHaystack || !sNeedle)
	{
		return false;
	}

	size_t iNeedle = 0, iHaystack = 0; // Beginning indices

	while (sHaystack[iHaystack])
	{
		iNeedle = 0;

		// Search for matching tokens
		while (  sNeedle[iNeedle] &&
			(sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ( !sNeedle[iNeedle] &&
			((sHaystack[iHaystack] == iDelim) || !sHaystack[iHaystack]))
		{
			return true;
		}

		// Advance to next delimiter
		while (sHaystack[iHaystack] && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// Pass by the delimiter if it exists
		iHaystack += (!!sHaystack[iHaystack]);
	}
	return false;

} // kstrin

//----------------------------------------------------------------------
KString kToUpper(KStringView sInput)
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
KString kToLower(KStringView sInput)
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

