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
#include <dekaf2/util/codec/kgaloisfield.h>
#include <cstddef>
#include <cstdint>
#include <vector>

/// @file kreedsolomon.h
/// A Reed-Solomon error-correction codec over GF(2^8): systematic encoding plus
/// full decoding (syndromes, Berlekamp-Massey, Chien search, Forney).

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_codec
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Reed-Solomon codec over GF(2^8). Encoding appends parity symbols to a
/// message; decoding repairs a received codeword in place, correcting up to
/// (parity count / 2) symbol errors at unknown positions.
///
/// The defaults (primitive polynomial 0x11D, generator 2, first root exponent
/// 0) match QR codes and most barcode standards; other code families can be
/// served by changing them. Symbols are bytes, so a codeword may be at most
/// 255 symbols long (message + parity).
class DEKAF2_PUBLIC KReedSolomon
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Bytes = std::vector<uint8_t>;

	/// @param iPrimitive the field's primitive polynomial (see KGaloisField)
	/// @param iGenerator the field generator
	/// @param iFirstConsecutiveRoot the exponent of the first root of the
	/// generator polynomial (the "fcr"); QR and most barcodes use 0
	explicit KReedSolomon(uint16_t iPrimitive            = 0x11D,
	                      uint8_t  iGenerator            = 2,
	                      uint8_t  iFirstConsecutiveRoot = 0)
	: m_GF(iPrimitive, iGenerator)
	, m_iFirstConsecutiveRoot(iFirstConsecutiveRoot)
	{
	}

	/// Compute and return iNumEccSymbols parity symbols for Message.
	Bytes Encode(const Bytes& Message, uint16_t iNumEccSymbols) const;

	/// Return Message with its iNumEccSymbols parity symbols appended (the full
	/// systematic codeword).
	Bytes EncodeAppend(const Bytes& Message, uint16_t iNumEccSymbols) const;

	/// Repair a received codeword (message followed by its iNumEccSymbols parity
	/// symbols) in place. Corrects up to iNumEccSymbols/2 errors at unknown
	/// positions.
	/// @param piCorrected if not null, receives the number of corrected symbols
	/// @returns true if the codeword was already valid or could be corrected,
	/// false if it carries more errors than are correctable
	bool Decode(Bytes& Codeword, uint16_t iNumEccSymbols, uint16_t* piCorrected = nullptr) const
	{
		return Decode(Codeword, iNumEccSymbols, std::vector<std::size_t>{}, piCorrected);
	}

	/// Repair a received codeword in place, given a list of erasure positions -
	/// symbols already known to be wrong (e.g. a lost packet, a flagged sector).
	/// A known position is "half the cost" of an unknown error, so the limit is
	/// 2 * (errors at unknown positions) + (erasures) <= iNumEccSymbols.
	/// @param ErasurePositions zero-based indices into Codeword that are suspect
	/// @param piCorrected if not null, receives the number of repaired symbols
	/// (erasures plus located errors)
	/// @returns true if the codeword could be (or already was) fully repaired
	bool Decode(Bytes& Codeword, uint16_t iNumEccSymbols,
	            const std::vector<std::size_t>& ErasurePositions, uint16_t* piCorrected = nullptr) const;

	/// the underlying field (exposed for callers that need raw GF arithmetic)
	const KGaloisField& Field() const { return m_GF; }

//----------
private:
//----------

	/// the generator polynomial of degree iNumEccSymbols (big-endian: index 0 is
	/// the highest-degree coefficient)
	Bytes GeneratorPoly(uint16_t iNumEccSymbols) const;
	/// evaluate a (big-endian) polynomial at x
	uint8_t PolyEval(const Bytes& Poly, uint8_t x) const;
	/// multiply two (big-endian) polynomials over the field
	Bytes PolyMul(const Bytes& A, const Bytes& B) const;

	KGaloisField m_GF;
	uint8_t      m_iFirstConsecutiveRoot;

}; // KReedSolomon

/// @} util_codec

DEKAF2_NAMESPACE_END
