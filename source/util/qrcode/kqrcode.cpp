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

#include <dekaf2/util/qrcode/kqrcode.h>
#include <dekaf2/util/codec/kreedsolomon.h>
#include <dekaf2/core/format/kformat.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>

DEKAF2_NAMESPACE_BEGIN

namespace {

// error-correction codewords per block, indexed [ecl][version] (index 0 unused).
// ecl order matches KQRCode::ECC: Low, Medium, Quartile, High. Extracted from the
// ISO/IEC 18004 block tables.
constexpr int8_t ECC_CODEWORDS_PER_BLOCK[4][41] = {
	{ -1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }, // L
	{ -1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28 }, // M
	{ -1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }, // Q
	{ -1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 }, // H
};

// number of error-correction blocks, indexed [ecl][version]
constexpr int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
	{ -1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25 }, // L
	{ -1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49 }, // M
	{ -1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68 }, // Q
	{ -1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81 }, // H
};

constexpr int PENALTY_N1 =  3;
constexpr int PENALTY_N2 =  3;
constexpr int PENALTY_N3 = 40;
constexpr int PENALTY_N4 = 10;

// the 2-bit format value for each ECC level (NOT the same order as the enum)
uint8_t EccFormatBits(KQRCode::ECC e)
{
	switch (e)
	{
		case KQRCode::ECC::Low:      return 1;
		case KQRCode::ECC::Medium:   return 0;
		case KQRCode::ECC::Quartile: return 3;
		case KQRCode::ECC::High:     return 2;
	}
	return 0;

} // EccFormatBits

bool GetBit(int32_t x, int32_t i) { return ((x >> i) & 1) != 0; }

// number of data+ecc modules (function patterns excluded) for a version
int32_t NumRawDataModules(int32_t ver)
{
	int32_t result = (16 * ver + 128) * ver + 64;
	if (ver >= 2)
	{
		int32_t numAlign = ver / 7 + 2;
		result -= (25 * numAlign - 10) * numAlign - 55;
		if (ver >= 7) result -= 36;
	}
	return result;

} // NumRawDataModules

int32_t NumDataCodewords(int32_t ver, KQRCode::ECC ecc)
{
	int32_t ecl = static_cast<int32_t>(ecc);
	return NumRawDataModules(ver) / 8
	     - ECC_CODEWORDS_PER_BLOCK[ecl][ver] * NUM_ERROR_CORRECTION_BLOCKS[ecl][ver];

} // NumDataCodewords

// --- finder-pattern penalty helpers (rule 3), per the reference algorithm ---

void FinderAddHistory(int32_t iRun, std::array<int32_t, 7>& History, int32_t iSize)
{
	if (History[0] == 0) iRun += iSize; // add a light border to the first run
	std::copy_backward(History.begin(), History.end() - 1, History.end());
	History[0] = iRun;

} // FinderAddHistory

int32_t FinderCountPatterns(const std::array<int32_t, 7>& H)
{
	int32_t n = H[1];
	bool core = n > 0 && H[2] == n && H[3] == n * 3 && H[4] == n && H[5] == n;
	return (core && H[0] >= n * 4 && H[6] >= n ? 1 : 0)
	     + (core && H[6] >= n * 4 && H[0] >= n ? 1 : 0);

} // FinderCountPatterns

int32_t FinderTerminate(bool bRunColor, int32_t iRun, std::array<int32_t, 7>& History, int32_t iSize)
{
	if (bRunColor) { FinderAddHistory(iRun, History, iSize); iRun = 0; }
	iRun += iSize; // add the light border to the final run
	FinderAddHistory(iRun, History, iSize);
	return FinderCountPatterns(History);

} // FinderTerminate

} // anonymous namespace

//-----------------------------------------------------------------------------
void KQRCode::SetFunctionModule(int32_t x, int32_t y, bool bDark)
//-----------------------------------------------------------------------------
{
	std::size_t i = static_cast<std::size_t>(y) * m_iSize + x;
	m_Modules[i]    = bDark ? 1 : 0;
	m_IsFunction[i] = 1;

} // SetFunctionModule

//-----------------------------------------------------------------------------
std::vector<int32_t> KQRCode::AlignmentPatternPositions() const
//-----------------------------------------------------------------------------
{
	if (m_iVersion == 1) return {};
	int32_t numAlign = m_iVersion / 7 + 2;
	int32_t step = (m_iVersion == 32) ? 26
	             : (m_iVersion * 4 + numAlign * 2 + 1) / (numAlign * 2 - 2) * 2;
	std::vector<int32_t> result;
	for (int32_t i = 0, pos = m_iSize - 7; i < numAlign - 1; ++i, pos -= step)
	{
		result.insert(result.begin(), pos);
	}
	result.insert(result.begin(), 6);
	return result;

} // AlignmentPatternPositions

//-----------------------------------------------------------------------------
void KQRCode::DrawFinderPattern(int32_t x, int32_t y)
//-----------------------------------------------------------------------------
{
	for (int32_t dy = -4; dy <= 4; ++dy)
	{
		for (int32_t dx = -4; dx <= 4; ++dx)
		{
			int32_t dist = std::max(std::abs(dx), std::abs(dy)); // Chebyshev distance
			int32_t xx = x + dx, yy = y + dy;
			if (xx >= 0 && xx < m_iSize && yy >= 0 && yy < m_iSize)
			{
				SetFunctionModule(xx, yy, dist != 2 && dist != 4);
			}
		}
	}

} // DrawFinderPattern

//-----------------------------------------------------------------------------
void KQRCode::DrawAlignmentPattern(int32_t x, int32_t y)
//-----------------------------------------------------------------------------
{
	for (int32_t dy = -2; dy <= 2; ++dy)
	{
		for (int32_t dx = -2; dx <= 2; ++dx)
		{
			SetFunctionModule(x + dx, y + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
		}
	}

} // DrawAlignmentPattern

//-----------------------------------------------------------------------------
void KQRCode::DrawFunctionPatterns()
//-----------------------------------------------------------------------------
{
	// horizontal and vertical timing patterns
	for (int32_t i = 0; i < m_iSize; ++i)
	{
		SetFunctionModule(6, i, i % 2 == 0);
		SetFunctionModule(i, 6, i % 2 == 0);
	}

	// three finder patterns (corners) - each also lays down its separator
	DrawFinderPattern(3, 3);
	DrawFinderPattern(m_iSize - 4, 3);
	DrawFinderPattern(3, m_iSize - 4);

	// alignment patterns (skip the three that would collide with the finders)
	std::vector<int32_t> align = AlignmentPatternPositions();
	std::size_t n = align.size();
	for (std::size_t i = 0; i < n; ++i)
	{
		for (std::size_t j = 0; j < n; ++j)
		{
			if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0)) continue;
			DrawAlignmentPattern(align[i], align[j]);
		}
	}

	// reserve the format and version areas (filled in later)
	DrawFormatBits(0);
	DrawVersion();

} // DrawFunctionPatterns

//-----------------------------------------------------------------------------
void KQRCode::DrawFormatBits(uint8_t iMask)
//-----------------------------------------------------------------------------
{
	int32_t data = (EccFormatBits(m_Ecc) << 3) | iMask; // 5 bits
	int32_t rem  = data;
	for (int32_t i = 0; i < 10; ++i) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
	int32_t bits = ((data << 10) | rem) ^ 0x5412; // 15 bits, BCH + mask

	// first copy, around the top-left finder
	for (int32_t i = 0; i <= 5; ++i) SetFunctionModule(8, i, GetBit(bits, i));
	SetFunctionModule(8, 7, GetBit(bits, 6));
	SetFunctionModule(8, 8, GetBit(bits, 7));
	SetFunctionModule(7, 8, GetBit(bits, 8));
	for (int32_t i = 9; i < 15; ++i) SetFunctionModule(14 - i, 8, GetBit(bits, i));

	// second copy, split between the top-right and bottom-left finders
	for (int32_t i = 0; i < 8; ++i)  SetFunctionModule(m_iSize - 1 - i, 8, GetBit(bits, i));
	for (int32_t i = 8; i < 15; ++i) SetFunctionModule(8, m_iSize - 15 + i, GetBit(bits, i));
	SetFunctionModule(8, m_iSize - 8, true); // always-dark module

} // DrawFormatBits

//-----------------------------------------------------------------------------
void KQRCode::DrawVersion()
//-----------------------------------------------------------------------------
{
	if (m_iVersion < 7) return;

	int32_t rem = m_iVersion;
	for (int32_t i = 0; i < 12; ++i) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
	int32_t bits = (m_iVersion << 12) | rem; // 18 bits

	for (int32_t i = 0; i < 18; ++i)
	{
		bool    bit = GetBit(bits, i);
		int32_t a   = m_iSize - 11 + i % 3;
		int32_t b   = i / 3;
		SetFunctionModule(a, b, bit);
		SetFunctionModule(b, a, bit);
	}

} // DrawVersion

//-----------------------------------------------------------------------------
void KQRCode::DrawCodewords(const std::vector<uint8_t>& Codewords)
//-----------------------------------------------------------------------------
{
	std::size_t i = 0; // bit index into Codewords
	for (int32_t right = m_iSize - 1; right >= 1; right -= 2) // two columns at a time
	{
		if (right == 6) right = 5; // the vertical timing column is skipped
		for (int32_t vert = 0; vert < m_iSize; ++vert)
		{
			for (int32_t j = 0; j < 2; ++j)
			{
				int32_t x       = right - j;
				bool    upward  = ((right + 1) & 2) == 0;
				int32_t y       = upward ? m_iSize - 1 - vert : vert;
				std::size_t idx = static_cast<std::size_t>(y) * m_iSize + x;
				if (!m_IsFunction[idx] && i < Codewords.size() * 8)
				{
					m_Modules[idx] = GetBit(Codewords[i >> 3], 7 - static_cast<int32_t>(i & 7)) ? 1 : 0;
					++i;
				}
				// remaining modules (if any) stay 0 (light), as the spec requires
			}
		}
	}

} // DrawCodewords

//-----------------------------------------------------------------------------
void KQRCode::ApplyMask(uint8_t iMask)
//-----------------------------------------------------------------------------
{
	for (int32_t y = 0; y < m_iSize; ++y)
	{
		for (int32_t x = 0; x < m_iSize; ++x)
		{
			std::size_t idx = static_cast<std::size_t>(y) * m_iSize + x;
			if (m_IsFunction[idx]) continue;

			bool invert = false;
			switch (iMask)
			{
				case 0: invert = (x + y) % 2 == 0;                       break;
				case 1: invert = y % 2 == 0;                             break;
				case 2: invert = x % 3 == 0;                             break;
				case 3: invert = (x + y) % 3 == 0;                       break;
				case 4: invert = (x / 3 + y / 2) % 2 == 0;               break;
				case 5: invert = x * y % 2 + x * y % 3 == 0;             break;
				case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0;       break;
				case 7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0;     break;
			}
			if (invert) m_Modules[idx] ^= 1;
		}
	}

} // ApplyMask

//-----------------------------------------------------------------------------
int32_t KQRCode::PenaltyScore() const
//-----------------------------------------------------------------------------
{
	int32_t result = 0;
	int32_t size   = m_iSize;

	// rule 1 (rows) + rule 3 finder-like patterns in rows
	for (int32_t y = 0; y < size; ++y)
	{
		std::array<int32_t, 7> history {};
		bool    color = false;
		int32_t run   = 0;
		for (int32_t x = 0; x < size; ++x)
		{
			if (GetModule(x, y) == color)
			{
				++run;
				if      (run == 5) result += PENALTY_N1;
				else if (run >  5) ++result;
			}
			else
			{
				FinderAddHistory(run, history, size);
				if (!color) result += FinderCountPatterns(history) * PENALTY_N3;
				color = GetModule(x, y);
				run   = 1;
			}
		}
		result += FinderTerminate(color, run, history, size) * PENALTY_N3;
	}
	// rule 1 (columns) + rule 3 in columns
	for (int32_t x = 0; x < size; ++x)
	{
		std::array<int32_t, 7> history {};
		bool    color = false;
		int32_t run   = 0;
		for (int32_t y = 0; y < size; ++y)
		{
			if (GetModule(x, y) == color)
			{
				++run;
				if      (run == 5) result += PENALTY_N1;
				else if (run >  5) ++result;
			}
			else
			{
				FinderAddHistory(run, history, size);
				if (!color) result += FinderCountPatterns(history) * PENALTY_N3;
				color = GetModule(x, y);
				run   = 1;
			}
		}
		result += FinderTerminate(color, run, history, size) * PENALTY_N3;
	}

	// rule 2: 2x2 blocks of one color
	for (int32_t y = 0; y < size - 1; ++y)
	{
		for (int32_t x = 0; x < size - 1; ++x)
		{
			bool c = GetModule(x, y);
			if (c == GetModule(x + 1, y) && c == GetModule(x, y + 1) && c == GetModule(x + 1, y + 1))
			{
				result += PENALTY_N2;
			}
		}
	}

	// rule 4: balance of dark vs light modules
	int32_t dark = 0;
	for (uint8_t m : m_Modules) dark += m;
	int32_t total = size * size;
	int32_t k = static_cast<int32_t>((std::abs(dark * 20L - total * 10L) + total - 1) / total) - 1;
	result += k * PENALTY_N4;

	return result;

} // PenaltyScore

//-----------------------------------------------------------------------------
std::vector<uint8_t> KQRCode::AddEccAndInterleave(const std::vector<uint8_t>& Data) const
//-----------------------------------------------------------------------------
{
	int32_t ecl           = static_cast<int32_t>(m_Ecc);
	int32_t numBlocks     = NUM_ERROR_CORRECTION_BLOCKS[ecl][m_iVersion];
	int32_t blockEccLen   = ECC_CODEWORDS_PER_BLOCK[ecl][m_iVersion];
	int32_t rawCodewords  = NumRawDataModules(m_iVersion) / 8;
	int32_t numShort      = numBlocks - rawCodewords % numBlocks;
	int32_t shortBlockLen = rawCodewords / numBlocks;

	KReedSolomon rs; // QR field, generator 2, fcr 0

	std::vector<std::vector<uint8_t>> blocks;
	blocks.reserve(static_cast<std::size_t>(numBlocks));
	std::size_t k = 0;
	for (int32_t i = 0; i < numBlocks; ++i)
	{
		int32_t datLen = shortBlockLen - blockEccLen + (i < numShort ? 0 : 1);
		std::vector<uint8_t> dat(Data.begin() + k, Data.begin() + k + datLen);
		k += static_cast<std::size_t>(datLen);
		std::vector<uint8_t> ecc = rs.Encode(dat, static_cast<uint16_t>(blockEccLen));
		if (i < numShort) dat.push_back(0); // pad short data blocks to align columns
		dat.insert(dat.end(), ecc.begin(), ecc.end());
		blocks.push_back(std::move(dat));
	}

	// interleave: column by column, skipping the padding cell of the short blocks
	std::vector<uint8_t> result;
	result.reserve(static_cast<std::size_t>(rawCodewords));
	for (std::size_t i = 0; i < blocks[0].size(); ++i)
	{
		for (std::size_t j = 0; j < blocks.size(); ++j)
		{
			if (i != static_cast<std::size_t>(shortBlockLen - blockEccLen) || j >= static_cast<std::size_t>(numShort))
			{
				result.push_back(blocks[j][i]);
			}
		}
	}
	return result;

} // AddEccAndInterleave

//-----------------------------------------------------------------------------
void KQRCode::Encode(KStringView sData, ECC Ecc, uint8_t iMinVersion, uint8_t iMaxVersion, int8_t iMask)
//-----------------------------------------------------------------------------
{
	m_Ecc = Ecc;

	int32_t iMin = std::max<int32_t>(1,  iMinVersion);
	int32_t iMax = std::min<int32_t>(40, iMaxVersion);

	// pick the smallest version (in range) that holds the data in byte mode
	int32_t version = 0;
	for (int32_t v = iMin; v <= iMax; ++v)
	{
		int32_t capacityBits = NumDataCodewords(v, Ecc) * 8;
		int32_t ccBits       = (v <= 9) ? 8 : 16; // byte-mode character-count length
		long long usedBits   = 4LL + ccBits + 8LL * static_cast<long long>(sData.size());
		if (usedBits <= capacityBits) { version = v; break; }
	}
	if (version == 0) { m_iSize = 0; return; } // does not fit at this ECC level

	m_iVersion = static_cast<uint8_t>(version);
	m_iSize    = static_cast<uint16_t>(version * 4 + 17);
	m_Modules.assign(static_cast<std::size_t>(m_iSize) * m_iSize, 0);
	m_IsFunction.assign(static_cast<std::size_t>(m_iSize) * m_iSize, 0);

	// --- assemble the bit stream (byte-mode segment + terminator + padding) ---
	std::vector<uint8_t> bits; // one entry per bit (0/1)
	auto appendBits = [&bits](uint32_t value, int32_t len)
	{
		for (int32_t i = len - 1; i >= 0; --i) bits.push_back(static_cast<uint8_t>((value >> i) & 1));
	};

	appendBits(0x4, 4);                                              // byte-mode indicator
	appendBits(static_cast<uint32_t>(sData.size()), version <= 9 ? 8 : 16);
	for (char c : sData) appendBits(static_cast<uint8_t>(c), 8);

	int32_t capacityBits = NumDataCodewords(version, Ecc) * 8;
	appendBits(0, std::min<int32_t>(4, capacityBits - static_cast<int32_t>(bits.size())));   // terminator
	appendBits(0, (8 - static_cast<int32_t>(bits.size()) % 8) % 8);                          // byte align
	for (uint8_t pad = 0xEC; static_cast<int32_t>(bits.size()) < capacityBits; pad = static_cast<uint8_t>(pad ^ (0xEC ^ 0x11)))
	{
		appendBits(pad, 8);
	}

	std::vector<uint8_t> dataCodewords(bits.size() / 8, 0);
	for (std::size_t i = 0; i < bits.size(); ++i)
	{
		dataCodewords[i >> 3] |= static_cast<uint8_t>(bits[i] << (7 - (i & 7)));
	}

	// --- error correction, draw, and mask ---
	std::vector<uint8_t> allCodewords = AddEccAndInterleave(dataCodewords);
	DrawFunctionPatterns();
	DrawCodewords(allCodewords);

	if (iMask < 0) // choose the lowest-penalty mask
	{
		int32_t minPenalty = INT_MAX;
		uint8_t best       = 0;
		for (uint8_t m = 0; m < 8; ++m)
		{
			ApplyMask(m);
			DrawFormatBits(m);
			int32_t penalty = PenaltyScore();
			if (penalty < minPenalty) { minPenalty = penalty; best = m; }
			ApplyMask(m); // undo (XOR mask is its own inverse)
		}
		iMask = static_cast<int8_t>(best);
	}
	m_iMask = static_cast<uint8_t>(iMask);
	ApplyMask(m_iMask);
	DrawFormatBits(m_iMask);

} // Encode

//-----------------------------------------------------------------------------
KString KQRCode::ToSVG(uint16_t iBorder, uint16_t iModulePixels) const
//-----------------------------------------------------------------------------
{
	if (empty()) return KString{};

	int32_t dim = m_iSize + 2 * static_cast<int32_t>(iBorder);
	int32_t px  = dim * std::max<int32_t>(1, iModulePixels);

	KString sPath;
	for (int32_t y = 0; y < m_iSize; ++y)
	{
		for (int32_t x = 0; x < m_iSize; ++x)
		{
			if (GetModule(x, y))
			{
				if (!sPath.empty()) sPath += ' ';
				sPath += kFormat("M{} {}h1v1h-1z", x + iBorder, y + iBorder);
			}
		}
	}

	KString sOut;
	sOut += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	sOut += kFormat("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{}\" height=\"{}\" "
	                "viewBox=\"0 0 {} {}\" shape-rendering=\"crispEdges\">\n", px, px, dim, dim);
	sOut += "<rect width=\"100%\" height=\"100%\" fill=\"#fff\"/>\n";
	sOut += kFormat("<path d=\"{}\" fill=\"#000\"/>\n", sPath);
	sOut += "</svg>\n";
	return sOut;

} // ToSVG

//-----------------------------------------------------------------------------
KString KQRCode::ToText(uint16_t iBorder) const
//-----------------------------------------------------------------------------
{
	if (empty()) return KString{};

	int32_t b   = iBorder;
	int32_t lo  = -b;
	int32_t hi  = m_iSize + b;

	// two module-rows per text line, using upper/lower half-block glyphs
	KString sOut;
	for (int32_t y = lo; y < hi; y += 2)
	{
		for (int32_t x = lo; x < hi; ++x)
		{
			bool top = Module(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
			bool bot = (y + 1 < hi) && Module(static_cast<uint16_t>(x), static_cast<uint16_t>(y + 1));
			// a "dark" module is drawn as a filled half; light stays blank
			if      ( top &&  bot) sOut += "\xE2\x96\x88"; // U+2588 FULL BLOCK
			else if ( top && !bot) sOut += "\xE2\x96\x80"; // U+2580 UPPER HALF
			else if (!top &&  bot) sOut += "\xE2\x96\x84"; // U+2584 LOWER HALF
			else                   sOut += ' ';
		}
		sOut += '\n';
	}
	return sOut;

} // ToText

DEKAF2_NAMESPACE_END
