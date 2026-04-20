/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#pragma once

/// @file ksetofchars.h
/// string operations for a set of chars

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/types/kbit.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/bits/simd/kmemsearch_neon.h>
#include <cinttypes>

#if !DEKAF2_FIND_FIRST_OF_USE_SIMD
// Use bit shifted search tables (compressed into 4 x 64bits)
// instead of flat tables (256 x 8bits).
// The compressed table approach is slower though on larger
// needles, and on setup. As the only disadvantage of the
// flat uncompressed tables is their size (256 bytes instead of 32)
// we opt to use those. As flat tables are the default, simply
// do not define any other option.
// #define DEKAF2_USE_COMPRESSED_SEARCH_TABLES 1
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// This is a replacement for the string find_first/last_of type of methods for non-SSE 4.2 architectures
/// which operate repeatedly on the same set of characters. Why not simply using std::string's methods?
/// They are orders of magnitude slower as they do not use a table based approach (which of course
/// only works with 8 bit characters..).
/// When this class is used on X86_64 architectures we assume that SSE 4.2 is available, and delegate
/// the call to the SSE implementation, which is still an order of magnitude faster.
/// Please note that the construction can happen constexpr, and therefore the class instance itself declared
/// as a constexpr variable.
class DEKAF2_PUBLIC KFindSetOfChars
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

#if !DEKAF2_FIND_FIRST_OF_USE_SIMD && !DEKAF2_USE_COMPRESSED_SEARCH_TABLES

	// State is encoded in the top 2 bits of m_table[0]. The lower 6 bits of
	// m_table[0] are free for per-state metadata:
	//
	//   Multi     (00......): flat 256-byte membership table. Bit 0 of each
	//                         m_table[ch] indicates whether ch is in the set.
	//                         Because the entire table defaults to all zeros,
	//                         Multi MUST be encoded as the value 0.
	//   Single    (01......): m_table[1] holds the single needle char.
	//   Empty     (10......): no needles at all.
	//   MultiSimd (11NNNNNN): 2..16 needle chars stored contiguously in
	//                         m_table[1..N]. The low 6 bits of m_table[0]
	//                         carry the needle count N. Used on platforms
	//                         with a fast NEON dispatch (see find_first_in).
	enum State : uint8_t {
		Multi     = 0 << 6,
		Single    = 1 << 6,
		Empty     = 2 << 6,
		MultiSimd = 3 << 6,
	};
	static constexpr uint8_t Mask      = 3 << 6;
	static constexpr uint8_t CountMask = static_cast<uint8_t>(~Mask);
	static_assert(Multi == 0, "Multi enum must be zero to match the algorithm");

	// MultiSimd is the NEON fast-path that stores raw needle bytes inline
	// and dispatches through detail::neon::kFindFirstOf / kFindLastOf at
	// runtime. Those helpers are not constexpr, so in NEON builds we cannot
	// keep find_first_in / find_last_in constexpr either. We still need
	// `inline` on the header-defined bodies to avoid ODR / duplicate-symbol
	// errors, since `constexpr` is implicitly inline but our empty expansion
	// is not.
	#if DEKAF2_HAS_NEON
		#define DEKAF2_FINDSETOFCHARS_CONSTEXPR inline
	#else
		#define DEKAF2_FINDSETOFCHARS_CONSTEXPR DEKAF2_CONSTEXPR_14
	#endif

#else

	#define DEKAF2_FINDSETOFCHARS_CONSTEXPR

#endif

//------
public:
//------

	using size_type  = KStringView::size_type;
	using value_type = KStringView::value_type;

	DEKAF2_CONSTEXPR_14
	KFindSetOfChars() = default;

	DEKAF2_CONSTEXPR_14
	KFindSetOfChars(KStringView sNeedles);

	DEKAF2_CONSTEXPR_14
	KFindSetOfChars(const char* szNeedles);

	KFindSetOfChars(const KString& sNeedles)
	: KFindSetOfChars(KStringView(sNeedles))
	{
	}

	/// find first occurence of needles in haystack, start search at pos (default 0)
	DEKAF2_FINDSETOFCHARS_CONSTEXPR
	size_type find_first_in(KStringView sHaystack, const size_type pos = 0) const;

	/// find first occurence of character in haystack that is not in needles, start search at pos (default 0)
	DEKAF2_FINDSETOFCHARS_CONSTEXPR
	size_type find_first_not_in(KStringView sHaystack, const size_type pos = 0) const;

	/// find last occurence of needles in haystack, start backward search at pos (default: end of string)
	DEKAF2_FINDSETOFCHARS_CONSTEXPR
	size_type find_last_in(KStringView sHaystack, size_type pos = KStringView::npos) const;

	/// find last occurence of character in haystack that is not in needles, start backward search at pos (default: end of string)
	DEKAF2_FINDSETOFCHARS_CONSTEXPR
	size_type find_last_not_in(KStringView sHaystack, size_type pos = KStringView::npos) const;

	/// is the set of characters empty?
	DEKAF2_CONSTEXPR_14
	bool empty() const;

	/// has the set of characters only one single char?
	DEKAF2_CONSTEXPR_14
	bool is_single_char() const;

	/// does the set of characters contain ch
	DEKAF2_CONSTEXPR_14
	bool contains(const value_type ch) const;

//------
private:
//------

#if DEKAF2_FIND_FIRST_OF_USE_SIMD

	KStringView m_sNeedles;

#elif DEKAF2_USE_COMPRESSED_SEARCH_TABLES

	size_type find_first_in(KStringView sHaystack, size_type pos, bool bNot) const;
	size_type find_last_in (KStringView sHaystack, size_type pos, bool bNot) const;

	uint64_t m_iMask[4] { 0, 0, 0, 0 };
	uint8_t  m_iOffset  { 0 };
	uint8_t  m_iBuckets { 0 };

#else // plain tables

	DEKAF2_CONSTEXPR_14
	State GetState() const { return static_cast<State>(m_table[0] & Mask); }

	DEKAF2_CONSTEXPR_14
	void SetState(State state) { m_table[0] = state; }

	DEKAF2_CONSTEXPR_14
	value_type GetSingleChar() const { return m_table[1]; }

	DEKAF2_CONSTEXPR_14
	void SetSingleChar(value_type ch) { m_table[1] = ch; }

	// ---- MultiSimd accessors (only used on NEON builds) ----

	// Store up to 16 needle chars at m_table[1..N] and the count N in the
	// low 6 bits of m_table[0] (state occupies the top 2 bits).
	DEKAF2_CONSTEXPR_14
	void SetSimdNeedles(KStringView sNeedles)
	{
		const size_type iCount = sNeedles.size();
		m_table[0] = static_cast<uint8_t>(State::MultiSimd | static_cast<uint8_t>(iCount));
		for (size_type i = 0; i < iCount; ++i)
		{
			m_table[1 + i] = sNeedles[i];
		}
	}

	DEKAF2_CONSTEXPR_14
	uint8_t GetSimdCount() const { return static_cast<uint8_t>(m_table[0]) & CountMask; }

	DEKAF2_CONSTEXPR_14
	const value_type* GetSimdNeedles() const { return m_table + 1; }

	// largest needle set we inline in MultiSimd mode (NEON broadcast stays
	// faster than table-lookup up to this size)
	static constexpr size_type kSimdMaxNeedles = 16;

	DEKAF2_FINDSETOFCHARS_CONSTEXPR
	size_type find_first_in(KStringView sHaystack, size_type pos, bool bNot) const;

	DEKAF2_FINDSETOFCHARS_CONSTEXPR
	size_type find_last_in (KStringView sHaystack, size_type pos, bool bNot) const;

	// Note: we use a char array instead of a std::array<char> because the
	// latter is not completely constexpr until C++20

	value_type m_table[256] = { 0 };

#endif

}; // KFindSetOfChars



#if DEKAF2_FIND_FIRST_OF_USE_SIMD

DEKAF2_CONSTEXPR_14
KFindSetOfChars::KFindSetOfChars(KStringView sNeedles) : m_sNeedles(sNeedles) {}

DEKAF2_CONSTEXPR_14
KFindSetOfChars::KFindSetOfChars(const value_type* sNeedles) : m_sNeedles(sNeedles) {}

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::empty() const { return m_sNeedles.empty(); }

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::is_single_char() const { return m_sNeedles.size() == 1; }

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::contains(const value_type ch) const { return m_sNeedles.contains(ch); }

#elif DEKAF2_USE_COMPRESSED_SEARCH_TABLES 

// ********* the compressed table search ************

// This is a compressed form of the table search algorithm, testing
// single bits in a 4 x 64 bit mask instead of a 256 x 8 bit table
// (where the table only uses one bit of eight available).
// The algorithm remodels what optimizers in newest clang and gcc
// generate if the needles fit into one 64 bit mask, but extended
// to the full 256 needle range. As the construction happens constexpr,
// this algorithm is as performant as the optimizations in clang and gcc.

DEKAF2_CONSTEXPR_14
KFindSetOfChars::KFindSetOfChars(KStringView sNeedles)
{
	// precondition:
	// C++17
	// sNeedles = constant evaluated

	if (!sNeedles.empty())
	{
		uint8_t iLowest  { 255 };
		uint8_t iHighest {   0 };

		for (auto Needle : sNeedles)
		{
			iLowest  = std::min(iLowest , static_cast<uint8_t>(Needle));
			iHighest = std::max(iHighest, static_cast<uint8_t>(Needle));
		}

		m_iBuckets  = (iHighest - iLowest) / 64 + 1;
		// try to align on a multiple of 64
		auto iLowest64 = (iLowest / 64) * 64;
		m_iOffset = (iHighest < iLowest64 + 64 * m_iBuckets) ? iLowest64 : iLowest;

		for (auto Needle : sNeedles)
		{
			auto ch = static_cast<uint8_t>(Needle);

			auto iBucket = (ch - m_iOffset) / 64;
			m_iMask[iBucket] |= 1ull << (ch - (m_iOffset + 64 * iBucket));
		}
	}
}

DEKAF2_CONSTEXPR_14
KFindSetOfChars::KFindSetOfChars(const char* szNeedles) : KFindSetOfChars(KStringView(szNeedles)) {}

inline
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(KStringView sHaystack, const size_type pos) const
{
	return find_first_in(sHaystack, pos, false);
}

inline
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(KStringView sHaystack, const size_type pos) const
{
	return find_first_in(sHaystack, pos, true);
}

inline
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(KStringView sHaystack, size_type pos) const
{
	return find_last_in(sHaystack, pos, false);
}

inline
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(KStringView sHaystack, size_type pos) const
{
	return find_last_in(sHaystack, pos, true);
}

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::empty() const { return m_iBuckets == 0; }

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::is_single_char() const { return m_iBuckets == 1 && kBitCountOne(m_iMask[0]) == 1; }

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::contains(const value_type ch) const
{
	auto uch = static_cast<uint8_t>(ch);
	if (uch < m_iOffset) return false;
	uch -= m_iOffset;
	auto bucket = uch / 64;
	if (m_iBuckets < bucket) return false;
	return m_iMask[bucket] & (1ull << (uch - bucket * 64));
}

#else // plain tables

// ******** the uncompressed table search ***********

DEKAF2_CONSTEXPR_14
KFindSetOfChars::KFindSetOfChars(KStringView sNeedles)
{
	switch (sNeedles.size())
	{
		case 0:
			SetState(State::Empty);
			break;

		case 1:
			SetSingleChar(sNeedles.front());
			SetState(State::Single);
			break;

		default:
#if DEKAF2_HAS_NEON
			// 2..16 needles: inline them for the NEON broadcast-and-compare
			// fast path. Larger sets fall through to the flat membership
			// table below because broadcast cost scales linearly with N.
			if (sNeedles.size() <= kSimdMaxNeedles)
			{
				SetSimdNeedles(sNeedles);
				break;
			}
#endif
			// assign the needles to the table
			for (auto c : sNeedles)
			{
				m_table[static_cast<unsigned char>(c)] = 1;
			}
			// do not set the state for Multi, it is 0 anyway, but could
			// overwrite the 1 from a NUL char
			// SetState(State::Multi);
			break;
	}
}

DEKAF2_CONSTEXPR_14
KFindSetOfChars::KFindSetOfChars(const value_type* sNeedles)
{
	if (!sNeedles || !*sNeedles)
	{
		SetState(State::Empty);
	}
	else
	{
		if (sNeedles[1] == 0)
		{
			SetSingleChar(*sNeedles);
			SetState(State::Single);
		}
		else
		{
#if DEKAF2_HAS_NEON
			// count chars first - if it's <= kSimdMaxNeedles, take the
			// NEON MultiSimd fast path
			size_type iLen = 0;
			while (sNeedles[iLen] && iLen <= kSimdMaxNeedles)
			{
				++iLen;
			}
			if (iLen <= kSimdMaxNeedles)
			{
				SetSimdNeedles(KStringView(sNeedles, iLen));
				return;
			}
#endif
			// assign the needles to the table
			for(;;)
			{
				auto c = static_cast<unsigned char>(*sNeedles++);
				if (!c) break;
				m_table[c] = 1;
			}
			// do not set the state for Multi, it is 0 anyway
			// SetState(State::Multi);
		}
	}
}

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::empty() const { return GetState() == State::Empty; }

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::is_single_char() const { return GetState() == State::Single; }

DEKAF2_CONSTEXPR_14
bool KFindSetOfChars::contains(const value_type ch) const
{
	switch (GetState())
	{
		case State::Empty:
			return false;

		case State::Single:
			return ch == GetSingleChar();

		case State::MultiSimd:
		{
			const uint8_t       iCount   = GetSimdCount();
			const value_type* const pNeedles = GetSimdNeedles();
			for (uint8_t i = 0; i < iCount; ++i)
			{
				if (pNeedles[i] == ch)
				{
					return true;
				}
			}
			return false;
		}

		case State::Multi:
			return (m_table[static_cast<unsigned char>(ch)] & 0x01) != 0;
	}

	return false; // unreachable, silences compiler warning
}

DEKAF2_FINDSETOFCHARS_CONSTEXPR
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(KStringView sHaystack, const size_type pos) const
{
	return find_first_in(sHaystack, pos, false);
}

DEKAF2_FINDSETOFCHARS_CONSTEXPR
KFindSetOfChars::size_type KFindSetOfChars::find_first_not_in(KStringView sHaystack, const size_type pos) const
{
	return find_first_in(sHaystack, pos, true);
}

DEKAF2_FINDSETOFCHARS_CONSTEXPR
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(KStringView sHaystack, size_type pos) const
{
	return find_last_in(sHaystack, pos, false);
}

DEKAF2_FINDSETOFCHARS_CONSTEXPR
KFindSetOfChars::size_type KFindSetOfChars::find_last_not_in(KStringView sHaystack, size_type pos) const
{
	return find_last_in(sHaystack, pos, true);
}

DEKAF2_FINDSETOFCHARS_CONSTEXPR
KFindSetOfChars::size_type KFindSetOfChars::find_first_in(KStringView sHaystack, const size_type pos, bool bNot) const
{
	switch (GetState())
	{
		case State::Empty:
			return (!bNot || sHaystack.empty()) ? KStringView::npos : 0;

		case State::Single:
			return bNot ? kFindNot(sHaystack, GetSingleChar(), pos) : kFind(sHaystack, GetSingleChar(), pos);

#if DEKAF2_HAS_NEON
		case State::MultiSimd:
			return detail::neon::kFindFirstOf(
				sHaystack.data(),
				sHaystack.size(),
				pos,
				GetSimdNeedles(),
				GetSimdCount(),
				bNot);
#endif

		case State::Multi:
			if (pos < sHaystack.size())
			{
				for (auto it = sHaystack.begin() + pos, ie = sHaystack.end(); it != ie; ++it)
				{
					if ((m_table[static_cast<unsigned char>(*it)] & 0x01) != bNot)
					{
						return it - sHaystack.begin();
					}
				}
			}
			break;

#if !DEKAF2_HAS_NEON
		// MultiSimd is not reachable on non-NEON builds (constructors never
		// select it), but the switch still needs to be exhaustive to silence
		// -Wswitch.
		case State::MultiSimd:
			break;
#endif
	}

	return KStringView::npos;

} // find_first_in

DEKAF2_FINDSETOFCHARS_CONSTEXPR
KFindSetOfChars::size_type KFindSetOfChars::find_last_in(KStringView sHaystack, size_type pos, bool bNot) const
{
	switch (GetState())
	{
		case State::Empty:
			return (!bNot || sHaystack.empty()) ? KStringView::npos : sHaystack.size() - 1;

		case State::Single:
			return bNot ? kRFindNot(sHaystack, GetSingleChar(), pos) : kRFind(sHaystack, GetSingleChar(), pos);

#if DEKAF2_HAS_NEON
		case State::MultiSimd:
			return detail::neon::kFindLastOf(
				sHaystack.data(),
				sHaystack.size(),
				pos,
				GetSimdNeedles(),
				GetSimdCount(),
				bNot);
#endif

		case State::Multi:
			pos = (pos >= sHaystack.size()) ? sHaystack.size() : pos + 1;

			for (auto it = sHaystack.begin() + pos, ie = sHaystack.begin(); it != ie;)
			{
				if ((m_table[static_cast<unsigned char>(*--it)] & 0x01) != bNot)
				{
					return it - sHaystack.begin();
				}
			}
			break;

#if !DEKAF2_HAS_NEON
		case State::MultiSimd:
			break;
#endif
	}

	return KStringView::npos;

} // find_last_in
#endif

namespace detail {

static constexpr KFindSetOfChars kASCIISpacesSet(kASCIISpaces);
static constexpr KFindSetOfChars kCommaSet(",");

} // end of namespace detail

DEKAF2_NAMESPACE_END
