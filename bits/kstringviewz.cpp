/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "../kstringview.h"
#include "../kstringutils.h"
#include "../klog.h"
#include "../kctype.h"

namespace dekaf2 {

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) && !defined(DEKAF2_NO_GCC)
// In contrast to most of the other optimized find functions we do not
// delegate this one to KStringView. The reason is that for find_first_of()
// we can use the ultra fast glibc strcspn() function, it even outrivals
// by a factor of two the sse 4.2 vector search implemented for folly::Range.
// We can however not use strcspn() for ranges (including KStringView),
// as there is no trailing zero byte.
//----------------------------------------------------------------------
KStringViewZ::size_type KStringViewZ::find_first_of(KStringView sv, size_type pos) const
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

} // find_first_of
#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) && !defined(DEKAF2_NO_GCC)
// In contrast to most of the other optimized find functions we do not
// delegate this one to KStringView. The reason is that for find_first_not_of()
// we can use the ultra fast glibc strspn() function, it even outrivals
// by a factor of two the sse 4.2 vector search implemented for folly::Range.
// We can however not use strspn() for ranges (including KStringView),
// as there is no trailing zero byte.
//----------------------------------------------------------------------
KStringViewZ::size_type KStringViewZ::find_first_not_of(KStringView sv, size_type pos) const
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

} // find_first_not_of
#endif

//----------------------------------------------------------------------
KStringViewZ KStringViewZ::Right(size_type iCount) const noexcept
//----------------------------------------------------------------------
{
	if (iCount > size())
	{
		kWarning("count ({}) exceeds size ({})", iCount, size());
		iCount = size();
	}
	return KStringViewZ(data() + size() - iCount, iCount);

} // Right

//----------------------------------------------------------------------
KStringViewZ& KStringViewZ::TrimLeft()
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(*this, [](value_type ch){ return KASCII::kIsSpace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KStringViewZ& KStringViewZ::TrimLeft(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(*this, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KStringViewZ& KStringViewZ::TrimLeft(KStringView sTrim)
//----------------------------------------------------------------------
{
	if (sTrim.size() == 1)
	{
		return TrimLeft(sTrim[0]);
	}
	dekaf2::kTrimLeft(*this, [sTrim](value_type ch){ return memchr(sTrim.data(), ch, sTrim.size()) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
bool KStringViewZ::ClipAtReverse(KStringView sClipAtReverse)
//----------------------------------------------------------------------
{
	size_type pos = find(sClipAtReverse);
	if (pos != npos)
	{
		erase(0, pos);
		return true;
	}
	return false;

} // ClipAtReverse

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ::value_type KStringViewZ::s_empty;
#endif

static_assert(std::is_nothrow_move_constructible<KStringViewZ>::value,
			  "KStringViewZ is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2

