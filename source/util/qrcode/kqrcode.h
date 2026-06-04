/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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
 // |\|   documentation files (the "Software"), to deal in the              |/|
 // |\|   Software without restriction, including without limitation        |/|
 // |\|   the rights to use, copy, modify, merge, publish,                  |/|
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

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <cstdint>
#include <vector>

/// @file kqrcode.h
/// QR code (model 2) generator: encodes data into a module matrix and renders
/// it as SVG or as block-character text. Self-contained, building on
/// KReedSolomon - no external library required.
///
/// @note "QR Code" is a registered trademark of DENSO WAVE INCORPORATED.

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_qrcode
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generates a QR code (ISO/IEC 18004, model 2) from arbitrary byte data using
/// byte mode. The encoder picks the smallest fitting version (1-40) and the
/// lowest-penalty mask automatically; both can be constrained for testing. The
/// result is a square grid of dark/light modules, renderable as SVG (for the
/// web) or as block-character text (for a terminal).
class DEKAF2_PUBLIC KQRCode
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the error-correction level: higher levels survive more damage but need a
	/// larger code for the same data (L ~7%, M ~15%, Q ~25%, H ~30% recoverable)
	enum class ECC : uint8_t { Low = 0, Medium = 1, Quartile = 2, High = 3 };

	/// Encode sData (treated as raw bytes, byte mode) at the given ECC level,
	/// choosing the smallest version that fits and the best mask. On overflow
	/// (more data than version 40 holds) the result is empty().
	explicit KQRCode(KStringView sData, ECC Ecc = ECC::Medium)
	{
		Encode(sData, Ecc, 1, 40, -1);
	}

	/// Encode with explicit control, mostly for testing: constrain the version to
	/// [iMinVersion, iMaxVersion] (1..40) and force a mask (0..7), or pass -1 to
	/// pick the best mask automatically.
	KQRCode(KStringView sData, ECC Ecc, uint8_t iMinVersion, uint8_t iMaxVersion, int8_t iMask)
	{
		Encode(sData, Ecc, iMinVersion, iMaxVersion, iMask);
	}

	/// true if encoding failed (data too large for version 40 at this ECC level)
	bool     empty()   const { return m_iSize == 0; }
	/// the side length in modules (4 * version + 17), or 0 if empty()
	uint16_t Size()    const { return m_iSize; }
	/// the chosen version (1..40), or 0 if empty()
	uint8_t  Version() const { return m_iVersion; }
	/// the applied mask pattern (0..7)
	uint8_t  MaskPattern() const { return m_iMask; }

	/// is the module at column x, row y dark? out-of-range coordinates are light
	bool Module(uint16_t x, uint16_t y) const
	{
		return (x < m_iSize && y < m_iSize) && m_Modules[static_cast<std::size_t>(y) * m_iSize + x] != 0;
	}

	/// Render as a standalone SVG document. @param iBorder quiet-zone width in
	/// modules (the spec recommends 4). @param iModulePixels pixels per module.
	KString ToSVG(uint16_t iBorder = 4, uint16_t iModulePixels = 4) const;

	/// Render as text using Unicode half-block characters (two module rows per
	/// line), suitable for printing in a terminal. @param iBorder quiet zone.
	KString ToText(uint16_t iBorder = 2) const;

//----------
private:
//----------

	void    Encode(KStringView sData, ECC Ecc, uint8_t iMinVersion, uint8_t iMaxVersion, int8_t iMask);
	void    DrawFunctionPatterns();
	void    DrawFinderPattern(int32_t x, int32_t y);
	void    DrawAlignmentPattern(int32_t x, int32_t y);
	void    DrawFormatBits(uint8_t iMask);
	void    DrawVersion();
	void    DrawCodewords(const std::vector<uint8_t>& Codewords);
	void    ApplyMask(uint8_t iMask);
	int32_t PenaltyScore() const;

	std::vector<uint8_t> AddEccAndInterleave(const std::vector<uint8_t>& Data) const;
	std::vector<int32_t> AlignmentPatternPositions() const;

	bool GetModule(int32_t x, int32_t y) const { return m_Modules[static_cast<std::size_t>(y) * m_iSize + x] != 0; }
	void SetFunctionModule(int32_t x, int32_t y, bool bDark);

	ECC      m_Ecc      { ECC::Medium };
	uint8_t  m_iVersion { 0 };
	uint8_t  m_iMask    { 0 };
	uint16_t m_iSize    { 0 };
	std::vector<uint8_t> m_Modules;     ///< 1 = dark, 0 = light; row-major, size*size
	std::vector<uint8_t> m_IsFunction;  ///< 1 = reserved function module (never masked)

}; // KQRCode

/// @} util_qrcode

DEKAF2_NAMESPACE_END
