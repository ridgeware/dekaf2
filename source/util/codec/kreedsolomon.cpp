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

#include <dekaf2/util/codec/kreedsolomon.h>
#include <algorithm>

DEKAF2_NAMESPACE_BEGIN

namespace {

using Bytes = std::vector<uint8_t>;

// All polynomials are big-endian: index 0 is the highest-degree coefficient.

//-----------------------------------------------------------------------------
// add two polynomials (aligned at the least-significant end)
Bytes PolyAdd(const Bytes& A, const Bytes& B)
//-----------------------------------------------------------------------------
{
	Bytes R(std::max(A.size(), B.size()), 0);
	for (std::size_t i = 0; i < A.size(); ++i) R[i + R.size() - A.size()]  = A[i];
	for (std::size_t i = 0; i < B.size(); ++i) R[i + R.size() - B.size()] ^= B[i];
	return R;

} // PolyAdd

//-----------------------------------------------------------------------------
// multiply every coefficient by a scalar
Bytes PolyScale(const Bytes& P, uint8_t x, const KGaloisField& GF)
//-----------------------------------------------------------------------------
{
	Bytes R(P.size());
	for (std::size_t i = 0; i < P.size(); ++i) R[i] = GF.Mul(P[i], x);
	return R;

} // PolyScale

//-----------------------------------------------------------------------------
// polynomial division; returns { quotient, remainder }
std::pair<Bytes, Bytes> PolyDiv(const Bytes& Dividend, const Bytes& Divisor, const KGaloisField& GF)
//-----------------------------------------------------------------------------
{
	Bytes Out(Dividend);
	if (Divisor.size() <= Out.size())
	{
		for (std::size_t i = 0; i + (Divisor.size() - 1) < Out.size(); ++i)
		{
			uint8_t coef = Out[i];
			if (coef != 0)
			{
				for (std::size_t j = 1; j < Divisor.size(); ++j)
				{
					if (Divisor[j] != 0) Out[i + j] ^= GF.Mul(Divisor[j], coef);
				}
			}
		}
	}
	std::size_t iSep = Divisor.size() - 1;          // remainder length
	std::size_t iCut = Out.size() - iSep;            // start of the remainder
	return { Bytes(Out.begin(), Out.begin() + iCut), Bytes(Out.begin() + iCut, Out.end()) };

} // PolyDiv

} // anonymous namespace

//-----------------------------------------------------------------------------
uint8_t KReedSolomon::PolyEval(const Bytes& Poly, uint8_t x) const
//-----------------------------------------------------------------------------
{
	uint8_t y = Poly.empty() ? 0 : Poly[0];
	for (std::size_t i = 1; i < Poly.size(); ++i) y = static_cast<uint8_t>(m_GF.Mul(y, x) ^ Poly[i]);
	return y;

} // PolyEval

//-----------------------------------------------------------------------------
KReedSolomon::Bytes KReedSolomon::PolyMul(const Bytes& A, const Bytes& B) const
//-----------------------------------------------------------------------------
{
	if (A.empty() || B.empty()) return {};
	Bytes R(A.size() + B.size() - 1, 0);
	for (std::size_t j = 0; j < B.size(); ++j)
	{
		for (std::size_t i = 0; i < A.size(); ++i)
		{
			R[i + j] ^= m_GF.Mul(A[i], B[j]);
		}
	}
	return R;

} // PolyMul

//-----------------------------------------------------------------------------
KReedSolomon::Bytes KReedSolomon::GeneratorPoly(uint16_t iNumEccSymbols) const
//-----------------------------------------------------------------------------
{
	Bytes g { 1 };
	for (uint16_t i = 0; i < iNumEccSymbols; ++i)
	{
		// multiply by (x - alpha^(fcr+i)) = (x + alpha^(fcr+i)) over GF(2)
		Bytes factor { 1, m_GF.Exp(static_cast<int32_t>(m_iFirstConsecutiveRoot) + i) };
		g = PolyMul(g, factor);
	}
	return g;

} // GeneratorPoly

//-----------------------------------------------------------------------------
KReedSolomon::Bytes KReedSolomon::EncodeAppend(const Bytes& Message, uint16_t iNumEccSymbols) const
//-----------------------------------------------------------------------------
{
	Bytes Ecc = Encode(Message, iNumEccSymbols);
	Bytes Out;
	Out.reserve(Message.size() + Ecc.size());
	Out.insert(Out.end(), Message.begin(), Message.end());
	Out.insert(Out.end(), Ecc.begin(),     Ecc.end());
	return Out;

} // EncodeAppend

//-----------------------------------------------------------------------------
KReedSolomon::Bytes KReedSolomon::Encode(const Bytes& Message, uint16_t iNumEccSymbols) const
//-----------------------------------------------------------------------------
{
	if (iNumEccSymbols == 0) return {};

	Bytes gen = GeneratorPoly(iNumEccSymbols);

	// synthetic division of Message * x^n by gen; the remainder is the parity
	Bytes Out(Message.size() + iNumEccSymbols, 0);
	std::copy(Message.begin(), Message.end(), Out.begin());

	for (std::size_t i = 0; i < Message.size(); ++i)
	{
		uint8_t coef = Out[i];
		if (coef != 0)
		{
			for (std::size_t j = 1; j < gen.size(); ++j) // gen[0] == 1, so skip it
			{
				Out[i + j] ^= m_GF.Mul(gen[j], coef);
			}
		}
	}
	return Bytes(Out.begin() + Message.size(), Out.end());

} // Encode

//-----------------------------------------------------------------------------
bool KReedSolomon::Decode(Bytes& Codeword, uint16_t iNumEccSymbols,
                          const std::vector<std::size_t>& ErasurePositions, uint16_t* piCorrected) const
//-----------------------------------------------------------------------------
{
	if (piCorrected) *piCorrected = 0;
	if (iNumEccSymbols == 0) return ErasurePositions.empty(); // no parity: nothing recoverable
	if (Codeword.empty() || Codeword.size() > 255) return false; // GF(2^8): codeword <= 255 symbols

	const std::size_t nmess = Codeword.size();

	// distinct, in-range erasure positions; more erasures than parity is hopeless
	std::vector<std::size_t> erasures(ErasurePositions);
	std::sort(erasures.begin(), erasures.end());
	erasures.erase(std::unique(erasures.begin(), erasures.end()), erasures.end());
	for (std::size_t pos : erasures) { if (pos >= nmess) return false; }
	if (erasures.size() > iNumEccSymbols) return false;

	// --- syndromes: S_i = R(alpha^(fcr+i)); synd[0] is a left-pad for indexing ---
	Bytes synd(static_cast<std::size_t>(iNumEccSymbols) + 1, 0);
	bool bAllZero = true;
	for (uint16_t i = 0; i < iNumEccSymbols; ++i)
	{
		synd[i + 1] = PolyEval(Codeword, m_GF.Exp(static_cast<int32_t>(i) + m_iFirstConsecutiveRoot));
		if (synd[i + 1] != 0) bAllZero = false;
	}
	if (bAllZero) return true; // nothing wrong (any erased symbols already held the right value)

	// --- Forney syndromes: hide the erasures so Berlekamp-Massey sees errors only ---
	Bytes fsynd(synd.begin() + 1, synd.end()); // S_0 .. S_{n-1}
	for (std::size_t pos : erasures)
	{
		uint8_t x = m_GF.Exp(static_cast<int32_t>(nmess - 1 - pos));
		for (std::size_t j = 0; j + 1 < fsynd.size(); ++j) fsynd[j] = static_cast<uint8_t>(m_GF.Mul(fsynd[j], x) ^ fsynd[j + 1]);
	}

	// --- Berlekamp-Massey on the Forney syndromes: the error locator (big-endian) ---
	Bytes err_loc { 1 };
	Bytes old_loc { 1 };
	std::size_t iters = static_cast<std::size_t>(iNumEccSymbols) - erasures.size();
	for (std::size_t i = 0; i < iters; ++i)
	{
		uint8_t delta = fsynd[i];
		for (std::size_t j = 1; j < err_loc.size(); ++j)
		{
			delta ^= m_GF.Mul(err_loc[err_loc.size() - 1 - j], fsynd[i - j]);
		}
		old_loc.push_back(0); // multiply old_loc by x
		if (delta != 0)
		{
			if (old_loc.size() > err_loc.size())
			{
				Bytes new_loc = PolyScale(old_loc, delta, m_GF);
				old_loc       = PolyScale(err_loc, m_GF.Inverse(delta), m_GF);
				err_loc       = new_loc;
			}
			err_loc = PolyAdd(err_loc, PolyScale(old_loc, delta, m_GF));
		}
	}

	// drop leading zeros, then errs = degree (number of errors at unknown positions)
	std::size_t iLead = 0;
	while (iLead < err_loc.size() && err_loc[iLead] == 0) ++iLead;
	err_loc.erase(err_loc.begin(), err_loc.begin() + iLead);
	std::size_t errs = err_loc.size() - 1;

	// the Singleton bound: each unknown error costs two parity symbols, each erasure one
	if (errs * 2 + erasures.size() > iNumEccSymbols) return false;
	if (errs == 0 && erasures.empty())              return false; // errors present but unlocatable

	// --- Chien search: the error positions are the roots of the error locator ---
	Bytes err_loc_rev(err_loc.rbegin(), err_loc.rend());
	std::vector<std::size_t> err_pos;
	for (std::size_t i = 0; i < nmess; ++i)
	{
		if (PolyEval(err_loc_rev, m_GF.Exp(static_cast<int32_t>(i))) == 0)
		{
			err_pos.push_back(nmess - 1 - i);
		}
	}
	if (err_pos.size() != errs) return false; // could not locate all errors

	// the full errata set: known erasures plus the located errors
	std::vector<std::size_t> errata(erasures);
	errata.insert(errata.end(), err_pos.begin(), err_pos.end());

	// --- Forney: magnitudes for every errata position, then correct in place ---
	std::vector<uint8_t> X;
	X.reserve(errata.size());
	for (std::size_t pos : errata) X.push_back(m_GF.Exp(static_cast<int32_t>(nmess - 1 - pos)));

	// errata locator = product (1 + X_i x)
	Bytes errata_loc { 1 };
	for (uint8_t Xi : X) errata_loc = PolyMul(errata_loc, PolyAdd(Bytes{ 1 }, Bytes{ Xi, 0 }));

	// error evaluator Omega(x) = (synd * errata_loc) mod x^(|errata|+1); the
	// reference reverses the full syndrome poly INCLUDING its leading 0
	Bytes synd_rev(synd);
	std::reverse(synd_rev.begin(), synd_rev.end());
	Bytes prod = PolyMul(synd_rev, errata_loc);
	Bytes xpow(errata.size() + 2, 0); xpow[0] = 1;     // x^(|errata|+1)
	Bytes err_eval = PolyDiv(prod, xpow, m_GF).second; // big-endian remainder Omega(x)

	for (std::size_t i = 0; i < X.size(); ++i)
	{
		uint8_t Xi     = X[i];
		uint8_t Xi_inv = m_GF.Inverse(Xi);

		// formal derivative of the errata locator at Xi_inv (product form)
		uint8_t denom = 1;
		for (std::size_t j = 0; j < X.size(); ++j)
		{
			if (j != i) denom = m_GF.Mul(denom, static_cast<uint8_t>(1 ^ m_GF.Mul(Xi_inv, X[j])));
		}
		if (denom == 0) return false; // numerically impossible unless mislocated

		uint8_t y = PolyEval(err_eval, Xi_inv);
		y = m_GF.Mul(m_GF.Pow(Xi, 1 - static_cast<int32_t>(m_iFirstConsecutiveRoot)), y);

		Codeword[errata[i]] ^= m_GF.Div(y, denom);
	}

	// verify the correction actually cleared the syndromes
	for (uint16_t i = 0; i < iNumEccSymbols; ++i)
	{
		if (PolyEval(Codeword, m_GF.Exp(static_cast<int32_t>(i) + m_iFirstConsecutiveRoot)) != 0)
		{
			return false;
		}
	}

	if (piCorrected) *piCorrected = static_cast<uint16_t>(errata.size());
	return true;

} // Decode

DEKAF2_NAMESPACE_END
