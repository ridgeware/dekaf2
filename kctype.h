/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

/// @file kctype.h
/// fast unicode character type detection and conversion -
/// necessary because Windows does not support std::towupper() etc. for Unicode.
/// Unixes profit as well, as these routines assume the Unicode codepage without
/// checking the current locale.

#include <cstdint>
#include <cwctype>
#include "bits/kcppcompat.h"

namespace dekaf2 {

namespace Unicode {

using codepoint_t = uint32_t;

} // end of namespace Unicode

//-----------------------------------------------------------------------------
/// Cast any integral type into a codepoint_t, without signed bit expansion.
template<typename Ch>
constexpr
Unicode::codepoint_t CodepointCast(Ch sch)
//-----------------------------------------------------------------------------
{
	static_assert(std::is_integral<Ch>::value, "can only convert integral types");

	if (sizeof(Ch) == 1)
	{
		return static_cast<uint8_t>(sch);
	}
	else if (sizeof(Ch) == 2)
	{
		return static_cast<uint16_t>(sch);
	}
	else
	{
		return static_cast<uint32_t>(sch);
	}
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCodePoint
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//------
	public:
	//------

	constexpr
	KCodePoint() = default;

	template<class CP>
	constexpr
	KCodePoint(CP cp) : m_CodePoint(CodepointCast(cp)) {}

	constexpr
	operator const Unicode::codepoint_t&() const { return m_CodePoint; }

	constexpr
	operator Unicode::codepoint_t&() { return m_CodePoint; }

	enum Categories
	{
		LetterUppercase          =  1, // Lu
		LetterLowercase          =  2, // Ll
		LetterTitlecase          =  3, // Lt
		LetterModifier           =  4, // Lm
		LetterOther              =  5, // Lo

		MarkNonspacing           =  6, // Mn
		MarkSpacingCombining     =  7, // Mc
		MarkEnclosing            =  8, // Me

		NumberDecimalDigit       =  9, // Nd
		NumberLetter             = 10, // Nl
		NumberOther              = 11, // No

		PunctuationConnector     = 12, // Pc
		PunctuationDash          = 13, // Pd
		PunctuationOpen          = 14, // Ps
		PunctuationClose         = 15, // Pe
		PunctuationInitialQuote  = 16, // Pi
		PunctuationFinalQote     = 17, // Pf
		PunctuationOther         = 18, // Po

		SymbolMath               = 19, // Sm
		SymbolCurrency           = 20, // Sc
		SymbolModifier           = 21, // Sk
		SymbolOther              = 22, // So

		SeparatorSpace           = 23, // Zs
		SeparatorLine            = 24, // Zl
		SeparatorParagraph       = 25, // Zp

		OtherControl             = 26, // Cc
		OtherFormat              = 27, // Cf
		OtherSurrogate           = 28, // Cs
		OtherPrivateUse          = 29, // Co
		OtherNotAssigned         = 30  // Cn
	};

	enum Types
	{
		Letter         = 1, // L
		Mark           = 2, // M
		Number         = 3, // N
		Punctuation    = 4, // P
		Symbol         = 5, // S
		Separator      = 6, // Z
		Other          = 7  // C
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Property
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		constexpr
		Property(Categories _Category = OtherNotAssigned,
				 Types      _Type     = Other,
				 uint8_t    _CaseFold = 0)
		: Category(_Category)
		, Type(_Type)
		, CaseFold(_CaseFold)
		{}

		uint8_t Category : 5;
		uint8_t Type     : 3;
		uint8_t CaseFold;
	};

//------
private:
//------

	static constexpr size_t MAX_TABLE = 0xFFFF;

	static const int32_t  CaseFolds[];
	static const Property CodePoints[];


	Unicode::codepoint_t m_CodePoint { 0 };

//------
public:
//------

	//-----------------------------------------------------------------------------
	Property GetProperty() const
	//-----------------------------------------------------------------------------
	{
		return CodePoints[m_CodePoint & MAX_TABLE];
	}

	//-----------------------------------------------------------------------------
	uint8_t GetCategory() const
	//-----------------------------------------------------------------------------
	{
		return GetProperty().Category;
	}

	//-----------------------------------------------------------------------------
	uint8_t GetType() const
	//-----------------------------------------------------------------------------
	{
		return GetProperty().Type;
	}

	//-----------------------------------------------------------------------------
	int32_t GetCaseFoldToLower() const
	//-----------------------------------------------------------------------------
	{
		auto Prop = GetProperty();
		if (Prop.Category != LetterLowercase)
		{
			return CaseFolds[Prop.CaseFold];
		}
		else
		{
			return 0;
		}
	}

	//-----------------------------------------------------------------------------
	int32_t GetCaseFoldToUpper() const
	//-----------------------------------------------------------------------------
	{
		auto Prop = GetProperty();
		if (Prop.Category == LetterLowercase)
		{
			return CaseFolds[Prop.CaseFold];
		}
		else
		{
			return 0;
		}
	}

	//-----------------------------------------------------------------------------
	bool IsSpace() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].Type == Separator;
		}
		else
		{
	#ifdef DEKAF2_IS_WINDOWS
			return false;
	#else
			return std::iswspace(m_CodePoint);
	#endif
		}
	}

	//-----------------------------------------------------------------------------
	bool IsBlank() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].Category == SeparatorSpace;
		}
		else
		{
	#ifdef DEKAF2_IS_WINDOWS
			return false;
	#else
			return std::iswblank(m_CodePoint);
	#endif
		}
	}

	//-----------------------------------------------------------------------------
	bool IsLower() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].Category == LetterLowercase;
		}
		else
		{
	#ifdef DEKAF2_IS_WINDOWS
			return false;
	#else
			return std::iswlower(m_CodePoint);
	#endif
		}
	}

	//-----------------------------------------------------------------------------
	bool IsUpper() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].Category == LetterUppercase;
		}
		else
		{
	#ifdef DEKAF2_IS_WINDOWS
			return false;
	#else
			return std::iswupper(m_CodePoint);
	#endif
		}
	}

	//-----------------------------------------------------------------------------
	bool IsTitle() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].Category == LetterTitlecase;
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	bool IsAlpha() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].Type == Letter;
		}
		else
		{
	#ifdef DEKAF2_IS_WINDOWS
			return false;
	#else
			return std::iswalpha(m_CodePoint);
	#endif
		}
	}

	//-----------------------------------------------------------------------------
	bool IsAlNum() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			Property p = CodePoints[m_CodePoint];
			return p.Type == Letter || p.Category == NumberDecimalDigit;
		}
		else
		{
	#ifdef DEKAF2_IS_WINDOWS
			return false;
	#else
			return std::iswalnum(m_CodePoint);
	#endif
		}
	}

	//-----------------------------------------------------------------------------
	bool IsPunct() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			auto Prop = CodePoints[m_CodePoint];
			return Prop.Type == Punctuation
			    || Prop.Type == Symbol
			    || Prop.Type == Mark
		        || Prop.Category == NumberOther
				|| Prop.Category == OtherPrivateUse
		        || Prop.Category == OtherFormat;
		}
		else
		{
	#ifdef DEKAF2_IS_WINDOWS
			return false;
	#else
			return std::iswpunct(m_CodePoint);
	#endif
		}
	}

	//-----------------------------------------------------------------------------
	bool IsDigit() const
	//-----------------------------------------------------------------------------
	{
		return m_CodePoint <= '9' && m_CodePoint >= '0';
	}

	//-----------------------------------------------------------------------------
	bool IsXDigit() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool IsUnicodeDigit() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].Category == NumberDecimalDigit;
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	KCodePoint ToUpper() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return m_CodePoint + GetCaseFoldToUpper();
		}
		else
		{
#ifdef DEKAF2_IS_WINDOWS
			return m_CodePoint;
#else
			return std::towupper(m_CodePoint);
#endif
		}
	}

	//-----------------------------------------------------------------------------
	KCodePoint ToLower() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return m_CodePoint - GetCaseFoldToLower();
		}
		else
		{
#ifdef DEKAF2_IS_WINDOWS
			return m_CodePoint;
#else
			return std::towlower(m_CodePoint);
#endif
		}
	}

	//-----------------------------------------------------------------------------
	void MakeUpper()
	//-----------------------------------------------------------------------------
	{
		*this = ToUpper();
	}

	//-----------------------------------------------------------------------------
	void MakeLower()
	//-----------------------------------------------------------------------------
	{
		*this = ToLower();
	}

}; // class KCodePoint

namespace KASCII {

//-----------------------------------------------------------------------------
inline bool kIsSpace(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return ch == 0x20 || (ch <= 0x0d && ch >= 0x09);
}

//-----------------------------------------------------------------------------
inline bool kIsBlank(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return ch == 0x20 || ch == 0x09;
}

//-----------------------------------------------------------------------------
inline bool kIsDigit(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return ch <= '9' && ch >= '0';
}

//-----------------------------------------------------------------------------
inline bool kIsXDigit(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsXDigit();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsAlpha(CP Ch)
//-----------------------------------------------------------------------------
{
	auto ch = CodepointCast(Ch);
	return ch < 0x80 && KCodePoint(ch).IsAlpha();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsAlNum(CP Ch)
//-----------------------------------------------------------------------------
{
	auto ch = CodepointCast(Ch);
	return ch < 0x80 && KCodePoint(ch).IsAlNum();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsPunct(CP Ch)
//-----------------------------------------------------------------------------
{
	auto ch = CodepointCast(Ch);
	return ch < 0x80 && KCodePoint(ch).IsPunct();
}

//-----------------------------------------------------------------------------
inline bool kIsLower(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return ch >= 'a' && ch <= 'z';
}

//-----------------------------------------------------------------------------
inline bool kIsUpper(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return ch <= 'Z' && ch >= 'A';
}

//-----------------------------------------------------------------------------
template<class CP>
inline CP kToLower(CP ch)
//-----------------------------------------------------------------------------
{
	if (kIsUpper(ch))
	{
		return ch + ('a' - 'A');
	}
	else
	{
		return ch;
	}
}

//-----------------------------------------------------------------------------
template<class CP>
inline CP kToUpper(CP ch)
//-----------------------------------------------------------------------------
{
	if (kIsLower(ch))
	{
		return ch - ('a' - 'A');
	}
	else
	{
		return ch;
	}
}

} // end of namespace KASCII

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsSpace(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsSpace();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsBlank(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsBlank();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsLower(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsLower();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsUpper(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsUpper();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsTitle(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsTitle();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsAlpha(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsAlpha();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsAlNum(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsAlNum();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsPunct(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsPunct();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsDigit(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsDigit();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsUnicodeDigit(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsUnicodeDigit();
}

//-----------------------------------------------------------------------------
template<class CP>
inline bool kIsXDigit(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).IsXDigit();
}

//-----------------------------------------------------------------------------
template<class CP,
	typename std::enable_if_t<std::is_integral<CP>::value>* = nullptr>
inline CP kToLower(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).ToLower();
}

//-----------------------------------------------------------------------------
template<class CP,
	typename std::enable_if_t<std::is_integral<CP>::value>* = nullptr>
inline CP kToUpper(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).ToUpper();
}

} // end of namespace dekaf2
