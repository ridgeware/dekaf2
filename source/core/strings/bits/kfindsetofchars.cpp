/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2023, Ridgeware, Inc.
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

#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/bits/kfindsetofchars.h>

#if DEKAF2_FIND_FIRST_OF_USE_SV_NEEDLE
#include <dekaf2/core/strings/bits/simd/kfindfirstof.h>
#endif

#if DEKAF2_USE_COMPRESSED_SEARCH_TABLES && DEKAF2_HAS_NEON
#include <dekaf2/core/strings/bits/simd/kmemsearch_neon.h>
#endif

DEKAF2_NAMESPACE_BEGIN

// this selection must mirror the member selection in kfindsetofchars.h:
// the m_sNeedles based methods below only exist in the SV_NEEDLE configuration
#if DEKAF2_FIND_FIRST_OF_USE_SV_NEEDLE

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (pos)
	{
		sHaystack.remove_prefix(pos);
		auto found = detail::sse::kFindFirstOf(sHaystack, m_sNeedles);
		return (found == KStringView::npos) ? found : found + pos;
	}
	else
	{
		return detail::sse::kFindFirstOf(sHaystack, m_sNeedles);
	}
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (pos)
	{
		sHaystack.remove_prefix(pos);
		auto found = detail::sse::kFindFirstNotOf(sHaystack, m_sNeedles);
		return (found == KStringView::npos) ? found : found + pos;
	}
	else
	{
		return detail::sse::kFindFirstNotOf(sHaystack, m_sNeedles);
	}
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (pos < sHaystack.size())
	{
		sHaystack.remove_suffix(sHaystack.size() - (pos+1));
	}
	return detail::sse::kFindLastOf(sHaystack, m_sNeedles);
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (pos < sHaystack.size())
	{
		sHaystack.remove_suffix(sHaystack.size() - (pos+1));
	}
	return detail::sse::kFindLastNotOf(sHaystack, m_sNeedles);
}

#elif DEKAF2_USE_COMPRESSED_SEARCH_TABLES

#if DEKAF2_HAS_NEON
/* NEON byteset search over the 4 x 64 bit membership mask, using the in-house
   kFindByteset / kRFindByteset kernels (see kmemsearch_neon). find_*_not_in
   passes the original mask to the kFindBytesetNot variants, which negate the
   membership test (ASCII sets) or search an internally inverted mask. Sets
   with exactly one member (like the default comma delimiter of kSplit)
   dispatch to the single-character search instead, which runs at memchr
   speed - over twice the byteset throughput on long scans. */

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(KStringView sHaystack, const size_type pos) const
//-----------------------------------------------------------------------------
{
	if (is_single_char())
	{
		return kFind(sHaystack, static_cast<value_type>(m_iSingleChar), pos);
	}

	if (pos >= sHaystack.length())
	{
		return KStringView::npos;
	}

	const char* pHaystack       = (sHaystack.data() + pos);
	size_t      iHaystackLength = (sHaystack.length() - pos);

	const char* pResult         = detail::neon::kFindByteset(pHaystack, iHaystackLength, m_iMask);

	if (pResult)
	{
		return (pResult - pHaystack + pos);
	}
	else
	{
		return KStringView::npos;
	}
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(KStringView sHaystack, const size_type pos) const
//-----------------------------------------------------------------------------
{
	if (is_single_char())
	{
		return kFindNot(sHaystack, static_cast<value_type>(m_iSingleChar), pos);
	}

	if (pos >= sHaystack.length())
	{
		return KStringView::npos;
	}

	const char* pHaystack       = (sHaystack.data() + pos);
	size_t      iHaystackLength = (sHaystack.length() - pos);

	const char* pResult = detail::neon::kFindBytesetNot(pHaystack, iHaystackLength, m_iMask);

	if (pResult)
	{
		return (pResult - pHaystack + pos);
	}
	else
	{
		return KStringView::npos;
	}
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (is_single_char())
	{
		return kRFind(sHaystack, static_cast<value_type>(m_iSingleChar), pos);
	}

	const char* pHaystack       = (sHaystack.data());
	size_t      iHaystackLength = (pos < sHaystack.length()? (pos + 1) : sHaystack.length());

	const char* pResult = detail::neon::kRFindByteset(pHaystack, iHaystackLength, m_iMask);

	if (pResult)
	{
		return (pResult - pHaystack);
	}
	else
	{
		return KStringView::npos;
	}
}

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	if (is_single_char())
	{
		return kRFindNot(sHaystack, static_cast<value_type>(m_iSingleChar), pos);
	}

	const char* pHaystack       = (sHaystack.data());
	size_t      iHaystackLength = (pos < sHaystack.length()? (pos + 1) : sHaystack.length());

	const char* pResult = detail::neon::kRFindBytesetNot(pHaystack, iHaystackLength, m_iMask);

	if (pResult)
	{
		return (pResult - pHaystack);
	}
	else
	{
		return KStringView::npos;
	}
}

#else
/* Non-SIMD Implementation based on compressed search tables (needle bitset) */

//-----------------------------------------------------------------------------
// this is in fact constexpr, but we do not use it
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(KStringView sHaystack, size_type pos, bool bNot) const
//-----------------------------------------------------------------------------
{
	if (pos > sHaystack.size())
	{
		pos = sHaystack.size();
	}

	auto it = sHaystack.begin() + pos;
	auto ie = sHaystack.end();

	for (;; ++it)
	{
		if (it >= ie)
		{
			return KStringView::npos;
		}

		auto ch = static_cast<uint8_t>(*it);

		if (ch >= m_iOffset /* && ch < m_iOffset + 64 */)
		{
			auto iBucket = (ch - m_iOffset) / 64;

			if (iBucket < m_iBuckets)
			{
				auto iBitValue = 1ull << (ch - (m_iOffset + 64 * iBucket));

				if (static_cast<bool>(iBitValue & m_iMask[iBucket]) != bNot)
				{
					break;
				}
			}
			else if (bNot)
			{
				break;
			}
		}
		else if (bNot)
		{
			break;
		}
	}

	return it - sHaystack.begin();
}

//-----------------------------------------------------------------------------
// this is in fact constexpr, but we do not use it
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(KStringView sHaystack, size_type pos, bool bNot) const
//-----------------------------------------------------------------------------
{
	if (pos < sHaystack.size())
	{
		sHaystack.remove_suffix(sHaystack.size() - (pos + 1));
	}

	auto it = sHaystack.end();
	auto ie = sHaystack.begin();

	for (;;)
	{
		if (it <= ie)
		{
			return KStringView::npos;
		}

		auto ch = static_cast<uint8_t>(*--it);

		if (ch >= m_iOffset /* && ch < m_iOffset + 64 */)
		{
			auto iBucket = (ch - m_iOffset) / 64;

			if (iBucket < m_iBuckets)
			{
				auto iBitValue = 1ull << (ch - (m_iOffset + 64 * iBucket));

				if (static_cast<bool>(iBitValue & m_iMask[iBucket]) != bNot)
				{
					break;
				}
			}
			else if (bNot)
			{
				break;
			}
		}
		else if (bNot)
		{
			break;
		}
	}

	return it - ie;
}

#endif // DEKAF2_HAS_NEON

#endif // DEKAF2_FIND_FIRST_OF_USE_SV_NEEDLE


DEKAF2_NAMESPACE_END
