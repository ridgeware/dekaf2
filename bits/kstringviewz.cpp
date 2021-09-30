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

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) && defined(DEKAF2_IS_GCC)
// In contrast to most of the other optimized find functions we do not
// delegate this one to KStringView. The reason is that for find_first_of()
// we can use the ultra fast glibc strcspn() function, it even outrivals
// by a factor of two the sse 4.2 vector search implemented for folly::Range.
// We can however not use strcspn() for ranges (including KStringView),
// as there is no trailing zero byte.
//----------------------------------------------------------------------
KStringViewZ::size_type KStringViewZ::find_first_of(KString search, size_type pos) const
//----------------------------------------------------------------------
{
	const auto iSize = size();

	if (DEKAF2_UNLIKELY(pos >= iSize))
	{
		return npos;
	}

	if (DEKAF2_UNLIKELY(search.size() == 1))
	{
		return find(sv[0], pos);
	}

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

	if (bHasZero && search.empty())
	{
		kDebug(1, "had all zero search set, fall back to KStringView");
		return KStringView::find_first_of('\0', pos);
	}

	// we now can safely use strcspn(), as all strings are 0 terminated.
	for (;;)
	{
		auto retval = std::strcspn(c_str() + pos, search.c_str()) + pos;

		if (retval >= iSize)
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

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) && defined(DEKAF2_IS_GCC)
// In contrast to most of the other optimized find functions we do not
// delegate this one to KStringView. The reason is that for find_first_not_of()
// we can use the ultra fast glibc strspn() function, it even outrivals
// by a factor of two the sse 4.2 vector search implemented for folly::Range.
// We can however not use strspn() for ranges (including KStringView),
// as there is no trailing zero byte.
//----------------------------------------------------------------------
KStringViewZ::size_type KStringViewZ::find_first_not_of(KString search, size_type pos) const
//----------------------------------------------------------------------
{
	const auto iSize = size();

	if (DEKAF2_UNLIKELY(pos >= iSize))
	{
		return npos;
	}

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

	if (bHasZero && search.empty())
	{
		kDebug(1, "had all zero search set, fall back to KStringView");
		return KStringView::find_first_not_of('\0', pos);
	}

	// we now can safely use strspn(), as all strings are 0 terminated.
	for (;;)
	{
		auto retval = std::strspn(c_str() + pos, search.c_str()) + pos;

		if (retval >= iSize)
		{
			return npos;
		}
		else if (m_rep[retval] == '\0' && bHasZero)
		{
			// we stopped on a zero char in the middle of the string and
			// had a zero in the search - restart the search
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
	const auto iSize = size();

	if (iCount > iSize)
	{
		// do not warn
		iCount = iSize;
	}
	return KStringViewZ(data() + iSize - iCount, iCount);

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

//-----------------------------------------------------------------------------
KStringViewZ::self& KStringViewZ::erase(size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	if (pos)
	{
#ifndef NDEBUG
		Warn(DEKAF2_FUNCTION_NAME, "impossible to erase past the begin in a KStringViewZ");
#endif
	}
	else
	{
		n = std::min(n, size());
		unchecked_remove_prefix(n);
	}
	return *this;
}

//-----------------------------------------------------------------------------
KStringViewZ::iterator KStringViewZ::erase(const_iterator position)
//-----------------------------------------------------------------------------
{
	if (position != begin())
	{
#ifndef NDEBUG
		Warn(DEKAF2_FUNCTION_NAME, "impossible to erase past the begin in a KStringViewZ");
#endif
		return end();
	}
	erase(static_cast<size_type>(position - begin()), 1);
	return begin();
}

//-----------------------------------------------------------------------------
KStringViewZ::iterator KStringViewZ::erase(const_iterator first, const_iterator last)
//-----------------------------------------------------------------------------
{
	if (first != begin())
	{
#ifndef NDEBUG
		Warn(DEKAF2_FUNCTION_NAME, "impossible to erase past the begin in a KStringViewZ");
#endif
		return end();
	}
	erase(static_cast<size_type>(first - begin()), static_cast<size_type>(last - first));
	return begin();
}

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ::value_type KStringViewZ::s_empty;
#endif

static_assert(std::is_nothrow_move_constructible<KStringViewZ>::value,
			  "KStringViewZ is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2

