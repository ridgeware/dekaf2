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
/// fast unicode character type detection and conversion

#include "kdefinitions.h"
#include <type_traits>
#include <cstdint>
#include <cwctype>
#include <array>

DEKAF2_NAMESPACE_BEGIN

namespace Unicode {

#ifdef __cpp_unicode_characters
using codepoint_t = char32_t;
#else
using codepoint_t = uint32_t;
#endif

} // end of namespace Unicode

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KCodePoint
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using value_type = Unicode::codepoint_t;

	//-----------------------------------------------------------------------------
	/// Cast any integral type into a value_type (codepoint_t), without signed bit expansion.
	template<typename Ch>
	static constexpr
	value_type CodepointCast(Ch sch)
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_integral<Ch>::value, "can only convert integral types");

		return (sizeof(Ch) == 1) ? static_cast<uint8_t>(sch)
			:  (sizeof(Ch) == 2) ? static_cast<uint16_t>(sch)
			:  static_cast<uint32_t>(sch);
	}

	constexpr
	KCodePoint() = default;

	template<class CP>
	constexpr
	KCodePoint(CP cp) : m_CodePoint(CodepointCast(cp)) {}

	/// explicit conversion to const value_type (const Unicode::codepoint_t)
	constexpr
	const value_type& value() const { return m_CodePoint; }

	/// explicit conversion to value_type (Unicode::codepoint_t)
	DEKAF2_CONSTEXPR_14
	value_type& value() { return m_CodePoint; }

	constexpr
	operator const value_type&() const { return value(); }

	DEKAF2_CONSTEXPR_14
	operator value_type&() { return value(); }

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

		uint8_t Category : 5; // NOLINT: we do not need to specify a label here, it is a bit value
		uint8_t Type     : 3; // NOLINT: we do not need to specify a label here, it is a bit value
		uint8_t CaseFold;

		//-----------------------------------------------------------------------------
		constexpr
		bool IsSpace() const
		//-----------------------------------------------------------------------------
		{
			return Type == Separator;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsBlank() const
		//-----------------------------------------------------------------------------
		{
			return Category == SeparatorSpace;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsLower() const
		//-----------------------------------------------------------------------------
		{
			return Category == LetterLowercase;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsUpper() const
		//-----------------------------------------------------------------------------
		{
			return Category == LetterUppercase;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsTitle() const
		//-----------------------------------------------------------------------------
		{
			return Category == LetterTitlecase;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsAlpha() const
		//-----------------------------------------------------------------------------
		{
			return Type == Letter;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsAlNum() const
		//-----------------------------------------------------------------------------
		{
			return Type == Letter || Category == NumberDecimalDigit;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsPunct() const
		//-----------------------------------------------------------------------------
		{
			return Type == Punctuation
				|| Type == Symbol
				|| Type == Mark
				|| Category == NumberOther
				|| Category == OtherPrivateUse
				|| Category == OtherFormat;
		}

		//-----------------------------------------------------------------------------
		constexpr
		bool IsUnicodeDigit() const
		//-----------------------------------------------------------------------------
		{
			return Category == NumberDecimalDigit;
		}

	}; // Property

	enum CTYPE : uint8_t
	{
		NOT_ASCII = 0,     // not an ASCII value

		CC = 1 << 0,       // Control
		SP = 1 << 1,       // Space
		BL = 1 << 2,       // Blank
		NN = 1 << 3,       // Digit
		XX = 1 << 4,       // XDigit
		PP = 1 << 5,       // Punct
		LL = 1 << 6,       // Lower
		UU = 1 << 7,       // Upper

		UX = UU | XX,      // Upper and XDigit
		LX = LL | XX,      // Lower and XDigit
		NX = NN | XX,      // Digit and XDigit

		AA = LL | UU,      // Alpha == LL | UU
		AN = AA | NN,      // Alnum == Alpha | NN
	};

//------
private:
//------

	//-----------------------------------------------------------------------------
	Property GetHighUnicodeProperty() const;
	//-----------------------------------------------------------------------------

	static constexpr value_type MAX_ASCII     = 0x7F;
	static constexpr value_type MAX_CASEFOLDS = 0xFF;
	static constexpr value_type MAX_TABLE     = 0x1FFFF;

	static constexpr std::array<CTYPE, MAX_ASCII + 1> ASCIITable
	{
	 // 0x0 0x1 0x2 0x3 0x4 0x5 0x6 0x7 0x8 0x9 0xA 0xB 0xC 0xD 0xE 0xF
		CC, CC, CC, CC, CC, CC, CC, CC, CC, BL, SP, SP, SP, SP, SP, CC, // 0x00
		CC, CC, CC, CC, CC, CC, CC, CC, CC, CC, CC, CC, CC, CC, CC, CC, // 0x10
		BL, PP, PP, PP, PP, PP, PP, PP, PP, PP, PP, PP, PP, PP, PP, PP, // 0x20
		NX, NX, NX, NX, NX, NX, NX, NX, NX, NX, PP, PP, PP, PP, PP, PP, // 0x30
		PP, UX, UX, UX, UX, UX, UX, UU, UU, UU, UU, UU, UU, UU, UU, UU, // 0x40
		UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, PP, PP, PP, PP, PP, // 0x50
		PP, LX, LX, LX, LX, LX, LX, LL, LL, LL, LL, LL, LL, LL, LL, LL, // 0x60
		LL, LL, LL, LL, LL, LL, LL, LL, LL, LL, LL, PP, PP, PP, PP, CC, // 0x70
	};

	static const std::array<int32_t , MAX_CASEFOLDS + 1> CaseFolds;
	static const std::array<Property, MAX_TABLE     + 1> CodePoints;

	value_type m_CodePoint { 0 };

//------
public:
//------

	//-----------------------------------------------------------------------------
	Property GetProperty() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint];
		}
		else
		{
			return GetHighUnicodeProperty();
		}
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

		if (Prop.Category == LetterUppercase)
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
	constexpr
	CTYPE GetASCIIType() const
	//-----------------------------------------------------------------------------
	{
		return (m_CodePoint <= MAX_ASCII) ? ASCIITable[m_CodePoint] : NOT_ASCII;
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIISpace(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & (SP | BL));
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIISpace() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIISpace(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsSpace() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].IsSpace();
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIBlank(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & BL);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIBlank() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIBlank(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsBlank() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].IsBlank();
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIILower(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & LL);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIILower() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIILower(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsLower() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].IsLower();
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIUpper(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & UU);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIUpper() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIUpper(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsUpper() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].IsUpper();
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIITitle(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & UU);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIITitle() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIITitle(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsTitle() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].IsTitle();
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIAlpha(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & AA);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIAlpha() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIAlpha(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsAlpha() const
	//-----------------------------------------------------------------------------
	{
		return GetProperty().IsAlpha();
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIAlNum(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & AN);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIAlNum() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIAlNum(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsAlNum() const
	//-----------------------------------------------------------------------------
	{
		return GetProperty().IsAlNum();
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIPunct(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & PP);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIPunct() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIPunct(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	bool IsPunct() const
	//-----------------------------------------------------------------------------
	{
		return GetProperty().IsPunct();
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIDigit(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & NN);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIDigit() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIDigit(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsDigit() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIDigit();
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIXDigit(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & NX);
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIXDigit() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIXDigit(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsXDigit() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIXDigit();
	}

	//-----------------------------------------------------------------------------
	bool IsUnicodeDigit() const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(m_CodePoint <= MAX_TABLE))
		{
			return CodePoints[m_CodePoint].IsUnicodeDigit();
		}
		else
		{
			return false;
		}
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIIPrint(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & (BL | NN | PP | AA));
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIIPrint() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIIPrint(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	constexpr
	static bool IsASCIICntrl(CTYPE ctype)
	//-----------------------------------------------------------------------------
	{
		return (ctype & (CC | SP));
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool IsASCIICntrl() const
	//-----------------------------------------------------------------------------
	{
		return IsASCIICntrl(GetASCIIType());
	}

	//-----------------------------------------------------------------------------
	KCodePoint ToUpper() const
	//-----------------------------------------------------------------------------
	{
		return m_CodePoint + GetCaseFoldToUpper();
	}

	//-----------------------------------------------------------------------------
	KCodePoint ToLower() const
	//-----------------------------------------------------------------------------
	{
		return m_CodePoint - GetCaseFoldToLower();
	}

	//-----------------------------------------------------------------------------
	void MakeUpper()
	//-----------------------------------------------------------------------------
	{
		m_CodePoint += GetCaseFoldToUpper();
	}

	//-----------------------------------------------------------------------------
	void MakeLower()
	//-----------------------------------------------------------------------------
	{
		m_CodePoint -= GetCaseFoldToLower();
	}

}; // class KCodePoint

//=============================================================================

namespace KASCII {

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline KCodePoint::CTYPE kCharType(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).GetASCIIType();
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsSpace(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIISpace(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsSpace(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsSpace(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsBlank(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIBlank(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsBlank(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsBlank(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsDigit(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIDigit(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsDigit(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsDigit(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsXDigit(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIXDigit(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsXDigit(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsXDigit(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsAlpha(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIAlpha(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsAlpha(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsAlpha(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsAlNum(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIAlNum(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsAlNum(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsAlNum(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsPunct(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIPunct(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsPunct(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsPunct(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsLower(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIILower(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsLower(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsLower(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsUpper(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIUpper(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsUpper(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsUpper(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsPrint(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIIPrint(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsPrint(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsPrint(kCharType(ch));
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsCntrl(KCodePoint::CTYPE ctype)
//-----------------------------------------------------------------------------
{
	return KCodePoint::IsASCIICntrl(ctype);
}

//-----------------------------------------------------------------------------
constexpr
DEKAF2_PUBLIC
inline bool kIsCntrl(uint16_t ch)
//-----------------------------------------------------------------------------
{
	return kIsCntrl(kCharType(ch));
}

//-----------------------------------------------------------------------------
template<class CP>
constexpr
inline CP kToLower(CP ch)
//-----------------------------------------------------------------------------
{
	return (kIsUpper(ch)) ? (ch + ('a' - 'A')) : ch;
}

//-----------------------------------------------------------------------------
template<class CP>
constexpr
inline CP kToUpper(CP ch)
//-----------------------------------------------------------------------------
{
	return (kIsLower(ch)) ? (ch - ('a' - 'A')) : ch;
}

} // end of namespace KASCII

//=============================================================================

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
         typename std::enable_if<std::is_integral<CP>::value, int>::type = 0
>
inline CP kToLower(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).ToLower();
}

//-----------------------------------------------------------------------------
template<class CP,
         typename std::enable_if<std::is_integral<CP>::value, int>::type = 0
>
inline CP kToUpper(CP ch)
//-----------------------------------------------------------------------------
{
	return KCodePoint(ch).ToUpper();
}

DEKAF2_NAMESPACE_END
