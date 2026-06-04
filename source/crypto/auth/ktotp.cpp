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

#include <dekaf2/crypto/auth/ktotp.h>
#include <dekaf2/crypto/encoding/kbase32.h>
#include <dekaf2/crypto/hash/khmac.h>
#include <dekaf2/crypto/hash/kmessagedigest.h>
#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/web/url/kurlencode.h>
#include <dekaf2/core/format/kformat.h>

DEKAF2_NAMESPACE_BEGIN

namespace {

// unambiguous alphabet for printed backup codes (no 0/1/l/o confusion); 32 chars
constexpr KStringView s_sBackupAlphabet = "23456789abcdefghijkmnpqrstuvwxyz";

//-----------------------------------------------------------------------------
KDigest::Digest ToDigest(KTOTP::Algorithm Algo)
//-----------------------------------------------------------------------------
{
	switch (Algo)
	{
		case KTOTP::Algorithm::SHA256: return KDigest::SHA256;
		case KTOTP::Algorithm::SHA512: return KDigest::SHA512;
		case KTOTP::Algorithm::SHA1:   break;
	}
	return KDigest::SHA1;
}

//-----------------------------------------------------------------------------
uint32_t Pow10(uint8_t n)
//-----------------------------------------------------------------------------
{
	uint32_t r = 1;
	while (n--) r *= 10;
	return r;
}

//-----------------------------------------------------------------------------
// render v as exactly n decimal digits, zero-padded (n <= 9)
KString FormatDigits(uint32_t v, uint8_t n)
//-----------------------------------------------------------------------------
{
	KString s;
	for (int32_t i = static_cast<int32_t>(n) - 1; i >= 0; --i)
	{
		s += static_cast<char>('0' + (v / Pow10(static_cast<uint8_t>(i))) % 10);
	}
	return s;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
KTOTP::KTOTP(KString sBase32Secret)
//-----------------------------------------------------------------------------
: m_sSecret(std::move(sBase32Secret))
, m_sKey(KBase32::Decode(m_sSecret))
{
}

//-----------------------------------------------------------------------------
KTOTP KTOTP::Generate(uint16_t iSecretBytes)
//-----------------------------------------------------------------------------
{
	// store the secret unpadded - the compact form authenticator apps expect
	return KTOTP(KBase32::Encode(kGetRandom(iSecretBytes), /*bPadding=*/false));

} // Generate

//-----------------------------------------------------------------------------
uint32_t KTOTP::HOTP(KStringView sRawKey, uint64_t iCounter, uint8_t iDigits, Algorithm Algo)
//-----------------------------------------------------------------------------
{
	// the counter is the HMAC message, 8 bytes big-endian (RFC 4226)
	unsigned char Msg[8];
	for (int32_t i = 7; i >= 0; --i) { Msg[i] = static_cast<unsigned char>(iCounter & 0xFF); iCounter >>= 8; }

	KString sHmac = KHMAC(ToDigest(Algo), sRawKey,
	                      KStringView(reinterpret_cast<const char*>(Msg), sizeof(Msg))).Digest();
	if (sHmac.size() < 20) return 0; // never happens for SHA-1/256/512

	// dynamic truncation: the low nibble of the LAST byte picks a 4-byte window
	auto     p       = reinterpret_cast<const unsigned char*>(sHmac.data());
	unsigned iOffset = p[sHmac.size() - 1] & 0x0F;
	uint32_t iBin    = (static_cast<uint32_t>(p[iOffset]     & 0x7F) << 24)
	                 | (static_cast<uint32_t>(p[iOffset + 1] & 0xFF) << 16)
	                 | (static_cast<uint32_t>(p[iOffset + 2] & 0xFF) <<  8)
	                 |  static_cast<uint32_t>(p[iOffset + 3] & 0xFF);
	return iBin % Pow10(iDigits);

} // HOTP

//-----------------------------------------------------------------------------
uint32_t KTOTP::Code(KUnixTime tWhen) const
//-----------------------------------------------------------------------------
{
	uint64_t iCounter = static_cast<uint64_t>(tWhen.to_time_t()) / m_iPeriod;
	return HOTP(m_sKey, iCounter, m_iDigits, m_Algorithm);

} // Code

//-----------------------------------------------------------------------------
KString KTOTP::FormatCode(KUnixTime tWhen) const
//-----------------------------------------------------------------------------
{
	return FormatDigits(Code(tWhen), m_iDigits);

} // FormatCode

//-----------------------------------------------------------------------------
bool KTOTP::Verify(KStringView sCode, int32_t iSkewSteps, KUnixTime tWhen) const
//-----------------------------------------------------------------------------
{
	if (m_sKey.empty()) return false;

	// parse the entered code (ignore spaces/dashes); must be exactly m_iDigits digits
	uint32_t    iGiven  = 0;
	std::size_t iDigits = 0;
	for (char ch : sCode)
	{
		if (ch == ' ' || ch == '\t' || ch == '-') continue;
		if (ch < '0' || ch > '9') return false;
		iGiven = iGiven * 10 + static_cast<uint32_t>(ch - '0');
		++iDigits;
	}
	if (iDigits != m_iDigits) return false;

	int64_t iStep = static_cast<int64_t>(static_cast<uint64_t>(tWhen.to_time_t()) / m_iPeriod);
	// scan the whole +/- window without an early return (constant work over the window)
	bool bMatch = false;
	for (int32_t s = -iSkewSteps; s <= iSkewSteps; ++s)
	{
		bMatch |= (HOTP(m_sKey, static_cast<uint64_t>(iStep + s), m_iDigits, m_Algorithm) == iGiven);
	}
	return bMatch;

} // Verify

//-----------------------------------------------------------------------------
KString KTOTP::URI(KStringView sIssuer, KStringView sAccount) const
//-----------------------------------------------------------------------------
{
	KStringView sAlgo = (m_Algorithm == Algorithm::SHA256) ? "SHA256"
	                  : (m_Algorithm == Algorithm::SHA512) ? "SHA512" : "SHA1";

	// label is "Issuer:Account" (both path-encoded); issuer is repeated in the query
	return kFormat("otpauth://totp/{}:{}?secret={}&issuer={}&algorithm={}&digits={}&period={}",
	               kUrlEncode<KString>(sIssuer,  URIPart::Path),
	               kUrlEncode<KString>(sAccount, URIPart::Path),
	               m_sSecret,
	               kUrlEncode<KString>(sIssuer,  URIPart::Query),
	               sAlgo, m_iDigits, m_iPeriod);

} // URI

//-----------------------------------------------------------------------------
KString KTOTP::GenerateNumericCode(uint8_t iDigits)
//-----------------------------------------------------------------------------
{
	KString  sRnd = kGetRandom(4);
	uint32_t r    = (static_cast<uint32_t>(static_cast<uint8_t>(sRnd[0])) << 24)
	              | (static_cast<uint32_t>(static_cast<uint8_t>(sRnd[1])) << 16)
	              | (static_cast<uint32_t>(static_cast<uint8_t>(sRnd[2])) <<  8)
	              |  static_cast<uint32_t>(static_cast<uint8_t>(sRnd[3]));
	return FormatDigits(r % Pow10(iDigits), iDigits);

} // GenerateNumericCode

//-----------------------------------------------------------------------------
std::vector<KString> KTOTP::GenerateBackupCodes(uint16_t iCount, uint8_t iGroupLen)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Out;
	Out.reserve(iCount);
	std::size_t iChars = static_cast<std::size_t>(iGroupLen) * 2; // two groups: xxxxx-xxxxx

	for (uint16_t n = 0; n < iCount; ++n)
	{
		KString sRaw = kGetRandom(iChars);
		KString sCode;
		for (std::size_t i = 0; i < iChars; ++i)
		{
			sCode += s_sBackupAlphabet[static_cast<unsigned char>(sRaw[i]) & 0x1F];
			if (i + 1 == iGroupLen) sCode += '-'; // group separator after the first half
		}
		Out.push_back(std::move(sCode));
	}
	return Out;

} // GenerateBackupCodes

//-----------------------------------------------------------------------------
KString KTOTP::HashCode(KStringView sCode)
//-----------------------------------------------------------------------------
{
	KString sNorm;
	for (char ch : sCode)
	{
		if (ch == '-' || ch == ' ' || ch == '\t') continue;
		sNorm += (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + 32) : ch;
	}
	return KSHA256(sNorm).HexDigest();

} // HashCode

DEKAF2_NAMESPACE_END
