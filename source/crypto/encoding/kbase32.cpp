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

#include <dekaf2/crypto/encoding/kbase32.h>
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN

namespace {
// the RFC 4648 base32 alphabet (index 0..31 -> character)
constexpr KStringView s_sAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
}

//-----------------------------------------------------------------------------
KString KBase32::Encode(KStringView sInput, bool bPadding)
//-----------------------------------------------------------------------------
{
	KString  sOut;
	uint32_t iBuffer = 0; // bit accumulator (big-endian)
	int32_t  iBits   = 0; // valid bits currently in the accumulator

	for (unsigned char ch : sInput)
	{
		iBuffer = (iBuffer << 8) | ch;
		iBits  += 8;
		while (iBits >= 5)
		{
			iBits -= 5;
			sOut  += s_sAlphabet[(iBuffer >> iBits) & 0x1F];
		}
	}
	if (iBits > 0) // flush the remaining bits, zero-padded on the right
	{
		sOut += s_sAlphabet[(iBuffer << (5 - iBits)) & 0x1F];
	}
	if (bPadding)
	{
		while (sOut.size() % 8 != 0) sOut += '=';
	}
	return sOut;

} // Encode

//-----------------------------------------------------------------------------
KString KBase32::Decode(KStringView sInput)
//-----------------------------------------------------------------------------
{
	KString  sOut;
	uint32_t iBuffer = 0;
	int32_t  iBits   = 0;

	for (char chIn : sInput)
	{
		if (chIn == ' ' || chIn == '\t' || chIn == '\r' || chIn == '\n' || chIn == '=') continue;
		char ch   = (chIn >= 'a' && chIn <= 'z') ? static_cast<char>(chIn - 32) : chIn; // upper-case
		auto iPos = s_sAlphabet.find(ch);
		if (iPos == KStringView::npos) return KString{}; // not a base32 character
		iBuffer = (iBuffer << 5) | static_cast<uint32_t>(iPos);
		iBits  += 5;
		if (iBits >= 8)
		{
			iBits -= 8;
			sOut  += static_cast<char>((iBuffer >> iBits) & 0xFF);
		}
	}
	return sOut;

} // Decode

DEKAF2_NAMESPACE_END
