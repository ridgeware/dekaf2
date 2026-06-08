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

#include <dekaf2/core/init/kdefinitions.h>
#include <array>
#include <cstddef>
#include <cstdint>

/// @file kgaloisfield.h
/// Arithmetic in the Galois field GF(2^8), the algebraic basis of Reed-Solomon
/// and BCH error-correcting codes.

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_codec
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Arithmetic in the Galois field GF(2^8). The field is fixed by a primitive
/// polynomial (default 0x11D = x^8+x^4+x^3+x^2+1, the choice used by QR codes
/// and most barcodes) and a generator element (default 2). Multiplication,
/// division and powers run in O(1) through precomputed exp/log tables. The
/// default field's tables are a compile-time constant (full C++17), built once
/// in a single translation unit; the hot arithmetic stays inline here. Used by
/// KReedSolomon.
class DEKAF2_PUBLIC KGaloisField
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Build the field for the given primitive polynomial and generator.
	/// @param iPrimitive the primitive (irreducible) polynomial, with bit 8 set,
	/// e.g. 0x11D. @param iGenerator a generator (primitive element) of the field.
	explicit KGaloisField(uint16_t iPrimitive = 0x11D, uint8_t iGenerator = 2);

	/// addition in GF(2^8) - identical to subtraction, it is just XOR
	static constexpr uint8_t Add(uint8_t a, uint8_t b) { return static_cast<uint8_t>(a ^ b); }
	/// subtraction in GF(2^8) - identical to addition (provided for readability)
	static constexpr uint8_t Sub(uint8_t a, uint8_t b) { return static_cast<uint8_t>(a ^ b); }

	/// multiply two field elements
	uint8_t Mul(uint8_t a, uint8_t b) const
	{
		if (a == 0 || b == 0) return 0;
		return m_Tables.Exp[static_cast<std::size_t>(m_Tables.Log[a]) + m_Tables.Log[b]];
	}

	/// divide a by b; b must not be 0
	uint8_t Div(uint8_t a, uint8_t b) const
	{
		if (a == 0) return 0;
		// log(a) - log(b), kept positive by adding the group order 255
		return m_Tables.Exp[static_cast<std::size_t>(m_Tables.Log[a]) + 255 - m_Tables.Log[b]];
	}

	/// the generator raised to the i-th power; i may be negative or exceed 254
	uint8_t Exp(int32_t i) const
	{
		i %= 255;
		if (i < 0) i += 255;
		return m_Tables.Exp[static_cast<std::size_t>(i)];
	}

	/// the discrete logarithm (base generator) of a; a must not be 0
	uint8_t Log(uint8_t a) const { return m_Tables.Log[a]; }

	/// a raised to the n-th power
	uint8_t Pow(uint8_t a, int32_t n) const
	{
		if (a == 0) return (n == 0) ? 1 : 0;
		return Exp(static_cast<int32_t>(m_Tables.Log[a]) * n);
	}

	/// the multiplicative inverse of a; a must not be 0
	uint8_t Inverse(uint8_t a) const { return m_Tables.Exp[static_cast<std::size_t>(255 - m_Tables.Log[a])]; }

//----------
private:
//----------

	struct Tables
	{
		std::array<uint8_t, 512> Exp; ///< generator^i, doubled to avoid a mod in Mul()
		std::array<uint8_t, 256> Log; ///< inverse of Exp over [1, 255]
	};

	// the cold path (table generation) lives in kgaloisfield.cpp so it is compiled
	// once, not in every TU that includes this header.
	static DEKAF2_FULL_CONSTEXPR_17 Tables BuildTables(uint16_t iPrimitive, uint8_t iGenerator);
	static const Tables& DefaultTables();

	Tables m_Tables;

}; // KGaloisField

/// @} util_codec

DEKAF2_NAMESPACE_END
