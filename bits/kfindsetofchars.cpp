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

#include "../kstringview.h"
#include "kfindsetofchars.h"
#if DEKAF2_FIND_FIRST_OF_USE_SIMD
#include "simd/kfindfirstof.h"
#endif

namespace dekaf2 {

#if DEKAF2_FIND_FIRST_OF_USE_SIMD

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

#else

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(KStringView sHaystack, const size_type pos) const
//-----------------------------------------------------------------------------
{
	switch (GetState())
	{
		case State::Empty:
			return KStringView::npos;

		case State::Single:
			return sHaystack.find(GetSingleChar(), pos);

		case State::Multi:
		{
			if (pos < sHaystack.size())
			{
				for (auto it = sHaystack.begin() + pos, ie = sHaystack.end(); it != ie; ++it)
				{
					if ((m_table[static_cast<unsigned char>(*it)] & 0x01))
					{
						return it - sHaystack.begin();
					}
				}
			}
			return KStringView::npos;
		}
	}

	// for gcc..
	return KStringView::npos;

} // find_first_in

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(KStringView sHaystack, const size_type pos) const
//-----------------------------------------------------------------------------
{
	switch (GetState())
	{
		case State::Empty:
			return sHaystack.empty() ? KStringView::npos : 0;

		case State::Single:
		{
			if (pos < sHaystack.size())
			{
				auto ch = GetSingleChar();

				for (auto it = sHaystack.begin() + pos, ie = sHaystack.end(); it != ie; ++it)
				{
					if (*it != ch)
					{
						return it - sHaystack.begin();
					}
				}
			}
			return KStringView::npos;
		}

		case State::Multi:
		{
			if (pos < sHaystack.size())
			{
				for (auto it = sHaystack.begin() + pos, ie = sHaystack.end(); it != ie; ++it)
				{
					if (!(m_table[static_cast<unsigned char>(*it)] & 0x01))
					{
						return it - sHaystack.begin();
					}
				}
			}
			return KStringView::npos;
		}
	}

	// for gcc..
	return KStringView::npos;

} // find_first_not_in

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	switch (GetState())
	{
		case State::Empty:
			return KStringView::npos;

		case State::Single:
		{
			return sHaystack.rfind(GetSingleChar(), pos);
		}

		case State::Multi:
		{
			if (pos >= sHaystack.size())
			{
				pos = sHaystack.size();
			}
			else
			{
				++pos;
			}
			for (auto it = sHaystack.begin() + pos, ie = sHaystack.begin(); it != ie;)
			{
				if ((m_table[static_cast<unsigned char>(*--it)] & 0x01))
				{
					return it - sHaystack.begin();
				}
			}
			return KStringView::npos;
		}
	}

	// for gcc..
	return KStringView::npos;

} // find_last_in

//-----------------------------------------------------------------------------
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(KStringView sHaystack, size_type pos) const
//-----------------------------------------------------------------------------
{
	switch (GetState())
	{
		case State::Empty:
			return sHaystack.empty() ? KStringView::npos : sHaystack.size() - 1;

		case State::Single:
		{
			if (pos >= sHaystack.size())
			{
				pos = sHaystack.size();
			}
			else
			{
				++pos;
			}

			auto ch = GetSingleChar();

			for (auto it = sHaystack.begin() + pos, ie = sHaystack.begin(); it != ie;)
			{
				if (*--it != ch)
				{
					return it - sHaystack.begin();
				}
			}
			return KStringView::npos;
		}

		case State::Multi:
		{
			if (pos >= sHaystack.size())
			{
				pos = sHaystack.size();
			}
			else
			{
				++pos;
			}

			for (auto it = sHaystack.begin() + pos, ie = sHaystack.begin(); it != ie;)
			{
				if (!(m_table[static_cast<unsigned char>(*--it)] & 0x01))
				{
					return it - sHaystack.begin();
				}
			}
			return KStringView::npos;
		}
	}

	// for gcc..
	return KStringView::npos;

} // find_last_not_in

#endif // DEKAF2_FIND_FIRST_OF_USE_SIMD

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

namespace detail {
constexpr KFindSetOfChars kASCIISpacesSet;
constexpr KFindSetOfChars kCommaSet;
} // end of namespace detail

#endif

} // end of namespace dekaf2

