
// This code is taken from https://github.com/JoachimSchurig/CppCDDB/blob/master/helper.hpp
//
// The original licence is
//
//  Copyright © 2016 Joachim Schurig. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// the code has been adapted to dekaf2 coding style

#pragma once

#include "kdefinitions.h"
#include "kctype.h"
#include "kutf8.h"
#include "kstringview.h"
#include <cctype>
#include <array>
#include <vector>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template <uint16_t iNGLen = 3, bool bAddSpaces = true, bool bSortedNgrams = true>
class KNGrams
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static_assert(iNGLen >= 2, "NGram minimum len is 2");

//----------
private:
//----------

	using WideChar   = KCodePoint;
	using NGram      = std::array<WideChar, iNGLen>;
	using NGramStore = std::vector<NGram>;

	NGramStore m_ngrams;

//----------
public:
//----------

	using size_type  = typename NGramStore::size_type;
	using StringView = KStringView;

	KNGrams() = default;

	//-----------------------------------------------------------------------------
	KNGrams(StringView sUTF8)
	//-----------------------------------------------------------------------------
	{
		Init(sUTF8);
	}

	//-----------------------------------------------------------------------------
	size_type Init(StringView sUTF8)
	//-----------------------------------------------------------------------------
	{
		clear();

		std::vector<WideChar> sWide;
		// worst case: ASCII
		sWide.reserve(sUTF8.size());
		bool bLastWasSpace { true };

		// transform to uc4 and lowercase, and filter anything that is not alnum
		Unicode::FromUTF8(sUTF8, [&sWide, &bLastWasSpace](WideChar CP) -> bool
		{
			if (CP.IsAlNum())
			{
				sWide.push_back(CP.ToLower());
				bLastWasSpace = false;
			}
			else
			{
				// this branch treats anything that is not alnum as space
				if (!bLastWasSpace)
				{
					bLastWasSpace = true;
					sWide.push_back(' ');
				}
			}
			return true;
		});

		if (!sWide.empty() && bLastWasSpace)
		{
			// remove trailing space
			sWide.pop_back();
		}

		// the if and else branches all get optimized
		// away during compilation as they depend solely
		// on compile time static template parameters

		std::size_t minlen;

		if (bAddSpaces)
		{
			if (iNGLen == 2) minlen = 1;
			else minlen = iNGLen-2;
		}
		else
		{
			minlen = iNGLen;
		}

		if (DEKAF2_UNLIKELY(sWide.size() < minlen))
		{
			return 0;
		}

		std::size_t iMax;

		if (bAddSpaces)
		{
			iMax = sWide.size() - (iNGLen-3);
		}
		else
		{
			iMax = sWide.size() - (iNGLen-1);
		}

		m_ngrams.reserve(iMax);

		std::size_t iCount { 0 };

		if (bAddSpaces)
		{
			AddChar(' ', iCount++, iMax);
		}

		for (const auto ch : sWide)
		{
			AddChar(ch, iCount++, iMax);
		}

		if (bAddSpaces)
		{
			AddChar(' ', iCount, iMax);
		}

		if (bSortedNgrams)
		{
			// sort the vector for fast comparison
			std::sort(m_ngrams.begin(), m_ngrams.end());
		}

		return m_ngrams.size();

	} // init

	//-----------------------------------------------------------------------------
	/// returns match percentage of Needle in Haystack
	static uint16_t Concordance(const KNGrams& Needle, const KNGrams& Haystack)
	//-----------------------------------------------------------------------------
	{
		std::size_t iFound { 0 };

		if (bSortedNgrams)
		{
			// comparing sorted ngrams is much faster than
			// unsorted ones
			auto       it1 = Needle.m_ngrams.begin();
			const auto ie1 = Needle.m_ngrams.end();
			auto       it2 = Haystack.m_ngrams.begin();
			const auto ie2 = Haystack.m_ngrams.end();

			while (it1 != ie1 && it2 != ie2)
			{
				auto iRes = Compare(*it1, *it2);

				if (iRes < 0)
				{
					++it1;
				}
				else
				{
					// check if both are equal
					if (iRes == 0)
					{
						// found one
						++iFound;
						++it1;
					}
					++it2;
				}
			}
		}
		else
		{
			// compare unsorted ngrams..
			std::vector<bool> Used;
			Used.insert(Used.begin(), Needle.m_ngrams.size(), false);

			for (const auto& HaystackGram : Haystack.m_ngrams)
			{
				auto u = Used.begin();
				for (const auto& NeedleGram : Needle.m_ngrams)
				{
					if (!*u && HaystackGram == NeedleGram)
					{
						++iFound;
						*u = true;
						break;
					}
					++u;
				}
			}
		}

		return iFound * 100 / Needle.m_ngrams.size();

	} // Concordance

	//-----------------------------------------------------------------------------
	/// returns match percentage
	uint16_t Concordance(const KNGrams& Needle)
	//-----------------------------------------------------------------------------
	{
		return Concordance(Needle, *this);
	}

	//-----------------------------------------------------------------------------
	/// returns match percentage
	uint16_t Concordance(StringView sNeedle)
	//-----------------------------------------------------------------------------
	{
		KNGrams<iNGLen, bAddSpaces, bSortedNgrams> Needle(sNeedle);
		return Concordance(Needle);
	}

	//-----------------------------------------------------------------------------
	/// returns match percentage
	static uint16_t Concordance(StringView sNeedle, StringView sHaystack)
	//-----------------------------------------------------------------------------
	{
		KNGrams<iNGLen, bAddSpaces, bSortedNgrams> Haystack(sHaystack);
		return Haystack.Concordance(sNeedle);
	}

	//-----------------------------------------------------------------------------
	/// returns match percentage, makes sure the longer sequence is matched on the
	/// shorter one
	static uint16_t Match(const KNGrams& Left, const KNGrams& Right)
	//-----------------------------------------------------------------------------
	{
		return (Left.size() > Right.size()) ? Concordance(Left, Right) : Concordance(Right, Left);
	}

	//-----------------------------------------------------------------------------
	/// returns match percentage, makes sure the longer sequence is matched on the
	/// shorter one
	uint16_t Match(const KNGrams& other)
	//-----------------------------------------------------------------------------
	{
		return Match(*this, other);
	}

	//-----------------------------------------------------------------------------
	/// returns match percentage, makes sure the longer sequence is matched on the
	/// shorter one
	uint16_t Match(StringView sOther)
	//-----------------------------------------------------------------------------
	{
		KNGrams<iNGLen, bAddSpaces, bSortedNgrams> other(sOther);
		return Match(other);
	}

	//-----------------------------------------------------------------------------
	/// returns match percentage, makes sure the longer sequence is matched on the
	/// shorter one
	static uint16_t Match(StringView sLeft, StringView sRight)
	//-----------------------------------------------------------------------------
	{
		KNGrams<iNGLen, bAddSpaces, bSortedNgrams> Left(sLeft);
		return Left.Match(sRight);
	}

	//-----------------------------------------------------------------------------
	void clear()
	//-----------------------------------------------------------------------------
	{
		m_ngrams.clear();
	}

	//-----------------------------------------------------------------------------
	/// returns count of ngrams
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return m_ngrams.size();
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	void AddChar(WideChar ch, std::size_t iPos, std::size_t iMax)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(iPos < iMax))
		{
			m_ngrams.emplace_back(NGram{});

			for (uint16_t ct = 0; ct < iNGLen && ct <= iPos; ++ct)
			{
				auto idx = iPos - ct;
				m_ngrams[idx][ct] = ch;
			}
		}
		else
		{
			// for the last iNGlen ngrams we need another index check
			for (uint16_t ct = 0; ct < iNGLen && ct < iMax; ++ct)
			{
				auto idx = iPos - ct;
				if (idx < iMax)
				{
					m_ngrams[idx][ct] = ch;
				}
			}
		}

	} // AddChar

	//-----------------------------------------------------------------------------
	static int Compare(const NGram& s1, const NGram& s2)
	//-----------------------------------------------------------------------------
	{
		for (typename NGram::size_type n = 0; n < s1.size(); ++n)
		{
			if (s1[n] < s2[n])
			{
				return -1;
			}
			if (s2[n] < s1[n])
			{
				return 1;
			}
		}

		return 0;
	}

}; // KNGrams

DEKAF2_NAMESPACE_END
