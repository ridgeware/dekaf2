/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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
 //
 */

#include <dekaf2/crypto/hash/kcrc.h>
#include <array>

DEKAF2_NAMESPACE_BEGIN

// On full C++17 the byte table is a compile-time constant; otherwise it is built
// exactly once, at first use. A local macro is needed because a block-scope
// `static` cannot be tagged `inline` (which is what DEKAF2_CONSTEXPR_17 expands
// to before C++17).
#ifdef DEKAF2_HAS_FULL_CPP_17
	#define DEKAF2_KCRC_CONSTEXPR constexpr
#else
	#define DEKAF2_KCRC_CONSTEXPR
#endif

namespace {

using CRC32Table = std::array<uint32_t, 256>;

//-----------------------------------------------------------------------------
// the reflected CRC-32 byte table for polynomial 0xEDB88320 (each entry folds
// eight bits at once - the table-driven equivalent of the bitwise kCRC32())
DEKAF2_KCRC_CONSTEXPR CRC32Table BuildTable()
//-----------------------------------------------------------------------------
{
	CRC32Table t {};
	for (uint32_t n = 0; n < 256; ++n)
	{
		uint32_t c = n;
		for (int k = 0; k < 8; ++k)
		{
			c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
		}
		t[n] = c;
	}
	return t;

} // BuildTable

//-----------------------------------------------------------------------------
// the shared table - a compile-time constant (full C++17) or built once at first
// use; lives in this single translation unit, so it is never emitted per-header
const CRC32Table& Table()
//-----------------------------------------------------------------------------
{
	static DEKAF2_KCRC_CONSTEXPR CRC32Table s = BuildTable();
	return s;

} // Table

} // anonymous namespace

//-----------------------------------------------------------------------------
bool KCRC32::Update(KStringView sInput)
//-----------------------------------------------------------------------------
{
	const auto& T   = Table();
	uint32_t    crc = m_iCRC;
	for (std::size_t i = 0; i < sInput.size(); ++i)
	{
		crc = T[(crc ^ static_cast<uint8_t>(sInput[i])) & 0xFFu] ^ (crc >> 8);
	}
	m_iCRC = crc;
	return true;

} // Update

//-----------------------------------------------------------------------------
bool KCRC32::Update(KInStream& InputStream)
//-----------------------------------------------------------------------------
{
	const auto& T   = Table();
	uint32_t    crc = m_iCRC;
	std::array<char, KDefaultCopyBufSize> Buffer;

	for (;;)
	{
		auto iReadChunk = InputStream.Read(Buffer.data(), Buffer.size());

		for (std::size_t i = 0; i < iReadChunk; ++i)
		{
			crc = T[(crc ^ static_cast<uint8_t>(Buffer[i])) & 0xFFu] ^ (crc >> 8);
		}

		if (iReadChunk < Buffer.size())
		{
			m_iCRC = crc;
			return true;
		}
	}

} // Update

#undef DEKAF2_KCRC_CONSTEXPR

DEKAF2_NAMESPACE_END
