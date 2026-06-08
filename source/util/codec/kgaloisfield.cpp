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

#include <dekaf2/util/codec/kgaloisfield.h>

DEKAF2_NAMESPACE_BEGIN

// A block-scope `static` cannot carry `inline`, so it needs a macro that is
// either `constexpr` or empty (unlike DEKAF2_CONSTEXPR_17, which falls back to
// `inline`). On full C++17 this makes the default table a compile-time constant;
// otherwise it is built exactly once, at first use.
#ifdef DEKAF2_HAS_FULL_CPP_17
	#define DEKAF2_KGF_TABLE_CONSTEXPR constexpr
#else
	#define DEKAF2_KGF_TABLE_CONSTEXPR
#endif

namespace {

//-----------------------------------------------------------------------------
/// carry-less multiply with polynomial reduction; used only while building the
/// tables (the runtime arithmetic path uses the tables themselves)
DEKAF2_CONSTEXPR_17 uint8_t MulNoTable(uint16_t a, uint16_t b, uint16_t iPrimitive)
//-----------------------------------------------------------------------------
{
	uint16_t result = 0;
	while (b)
	{
		if (b & 1) result ^= a;
		b >>= 1;
		a <<= 1;
		if (a & 0x100) a ^= iPrimitive;
	}
	return static_cast<uint8_t>(result);

} // MulNoTable

} // anonymous namespace

//-----------------------------------------------------------------------------
DEKAF2_FULL_CONSTEXPR_17 KGaloisField::Tables KGaloisField::BuildTables(uint16_t iPrimitive, uint8_t iGenerator)
//-----------------------------------------------------------------------------
{
	Tables t {};
	// walk the multiplicative group: x runs through generator^0, ^1, ... ^254
	uint16_t x = 1;
	for (int32_t i = 0; i < 255; ++i)
	{
		t.Exp[static_cast<std::size_t>(i)] = static_cast<uint8_t>(x);
		t.Log[x]                           = static_cast<uint8_t>(i);
		x = MulNoTable(x, iGenerator, iPrimitive);
	}
	// duplicate the table into [255, 510] so Mul() never has to reduce mod 255
	for (int32_t i = 255; i < 512; ++i)
	{
		t.Exp[static_cast<std::size_t>(i)] = t.Exp[static_cast<std::size_t>(i - 255)];
	}
	t.Log[0] = 0; // log(0) is undefined; the value is never read for a valid input
	return t;

} // BuildTables

//-----------------------------------------------------------------------------
const KGaloisField::Tables& KGaloisField::DefaultTables()
//-----------------------------------------------------------------------------
{
	// the default field (primitive 0x11D, generator 2)
	static DEKAF2_KGF_TABLE_CONSTEXPR Tables s = BuildTables(0x11D, 2);
	return s;

} // DefaultTables

//-----------------------------------------------------------------------------
KGaloisField::KGaloisField(uint16_t iPrimitive, uint8_t iGenerator)
//-----------------------------------------------------------------------------
: m_Tables( (iPrimitive == 0x11D && iGenerator == 2) ? DefaultTables()
                                                     : BuildTables(iPrimitive, iGenerator) )
{
	// the default field's tables are merely copied from the shared compile-time
	// constant - no per-construction Galois arithmetic; a custom field builds its
	// tables on the spot.

} // KGaloisField

#undef DEKAF2_KGF_TABLE_CONSTEXPR

DEKAF2_NAMESPACE_END
