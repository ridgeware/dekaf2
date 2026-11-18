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


#include <dekaf2/crypto/auth/bits/kargon2core.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/core/types/kbit.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

namespace {

//-----------------------------------------------------------------------------
/// reads a 64 bit word in little endian byte order
template<typename Bytes>
uint64_t LoadWord(const Bytes& Buffer, std::size_t iOffset)
//-----------------------------------------------------------------------------
{
	uint64_t iWord;
	std::memcpy(&iWord, &Buffer[iOffset], sizeof(iWord));
	kFromLittleEndian(iWord);

	return iWord;

} // LoadWord

//-----------------------------------------------------------------------------
/// appends a 64 bit word in little endian byte order
void AppendWord(KString& sBytes, uint64_t iWord)
//-----------------------------------------------------------------------------
{
	kToLittleEndian(iWord);
	sBytes.append(reinterpret_cast<const char*>(&iWord), sizeof(iWord));

} // AppendWord

//-----------------------------------------------------------------------------
/// appends a 32 bit word in little endian byte order
void AppendWord32(KString& sBytes, uint32_t iWord)
//-----------------------------------------------------------------------------
{
	kToLittleEndian(iWord);
	sBytes.append(reinterpret_cast<const char*>(&iWord), sizeof(iWord));

} // AppendWord32

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// BLAKE2b of RFC 7693 without a key
class KBlake2b
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// @param iDigestLength the length of the digest in bytes, from 1 to 64
	explicit KBlake2b(std::size_t iDigestLength)
	//-----------------------------------------------------------------------------
	: m_H(s_IV)
	, m_iDigestLength(iDigestLength)
	{
		// the parameter block: the digest length, no key, fanout 1 and depth 1
		m_H[0] ^= 0x01010000ull ^ iDigestLength;
	}

	//-----------------------------------------------------------------------------
	void Update(KStringView sInput)
	//-----------------------------------------------------------------------------
	{
		while (!sInput.empty())
		{
			if (m_iBuffered == m_Buffer.size())
			{
				// more input follows, so this is not the last block
				m_iCounter += m_Buffer.size();
				Compress(false);
				m_iBuffered = 0;
			}

			auto iTake = std::min(m_Buffer.size() - m_iBuffered, sInput.size());
			std::memcpy(&m_Buffer[m_iBuffered], sInput.data(), iTake);
			m_iBuffered += iTake;
			sInput.remove_prefix(iTake);
		}
	}

	//-----------------------------------------------------------------------------
	/// @return the digest - the object cannot be used after that
	KString Final()
	//-----------------------------------------------------------------------------
	{
		m_iCounter += m_iBuffered;
		std::fill(m_Buffer.begin() + m_iBuffered, m_Buffer.end(), 0);
		Compress(true);

		KString sDigest;
		sDigest.reserve(m_H.size() * sizeof(uint64_t));

		for (auto iWord : m_H)
		{
			AppendWord(sDigest, iWord);
		}

		sDigest.resize(m_iDigestLength);

		return sDigest;
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	static void Mix(uint64_t& a, uint64_t& b, uint64_t& c, uint64_t& d, uint64_t x, uint64_t y)
	//-----------------------------------------------------------------------------
	{
		a = a + b + x;
		d = kRotateRight(d ^ a, 32);
		c = c + d;
		b = kRotateRight(b ^ c, 24);
		a = a + b + y;
		d = kRotateRight(d ^ a, 16);
		c = c + d;
		b = kRotateRight(b ^ c, 63);
	}

	//-----------------------------------------------------------------------------
	void Compress(bool bLastBlock)
	//-----------------------------------------------------------------------------
	{
		std::array<uint64_t, 16> M;

		for (std::size_t i = 0; i < M.size(); ++i)
		{
			M[i] = LoadWord(m_Buffer, i * sizeof(uint64_t));
		}

		std::array<uint64_t, 16> V;

		for (std::size_t i = 0; i < 8; ++i)
		{
			V[i]     = m_H[i];
			V[i + 8] = s_IV[i];
		}

		// the byte counter has 128 bits, its upper half stays 0 here
		V[12] ^= m_iCounter;

		if (bLastBlock)
		{
			V[14] = ~V[14];
		}

		for (std::size_t iRound = 0; iRound < 12; ++iRound)
		{
			const auto& S = s_Sigma[iRound % s_Sigma.size()];

			Mix(V[0], V[4], V[ 8], V[12], M[S[ 0]], M[S[ 1]]);
			Mix(V[1], V[5], V[ 9], V[13], M[S[ 2]], M[S[ 3]]);
			Mix(V[2], V[6], V[10], V[14], M[S[ 4]], M[S[ 5]]);
			Mix(V[3], V[7], V[11], V[15], M[S[ 6]], M[S[ 7]]);
			Mix(V[0], V[5], V[10], V[15], M[S[ 8]], M[S[ 9]]);
			Mix(V[1], V[6], V[11], V[12], M[S[10]], M[S[11]]);
			Mix(V[2], V[7], V[ 8], V[13], M[S[12]], M[S[13]]);
			Mix(V[3], V[4], V[ 9], V[14], M[S[14]], M[S[15]]);
		}

		for (std::size_t i = 0; i < 8; ++i)
		{
			m_H[i] ^= V[i] ^ V[i + 8];
		}
	}

	static const std::array<uint64_t, 8>                 s_IV;
	static const std::array<std::array<uint8_t, 16>, 10> s_Sigma;

	std::array<uint64_t, 8>   m_H;
	std::array<uint8_t, 128>  m_Buffer;
	std::size_t               m_iBuffered     { 0 };
	std::size_t               m_iDigestLength;
	uint64_t                  m_iCounter      { 0 };

}; // KBlake2b

const std::array<uint64_t, 8> KBlake2b::s_IV
{{
	0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull, 0xa54ff53a5f1d36f1ull,
	0x510e527fade682d1ull, 0x9b05688c2b3e6c1full, 0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull
}};

const std::array<std::array<uint8_t, 16>, 10> KBlake2b::s_Sigma
{{
	{{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 }},
	{{ 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }},
	{{ 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 }},
	{{  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 }},
	{{  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 }},
	{{  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 }},
	{{ 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 }},
	{{ 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 }},
	{{  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 }},
	{{ 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 }}
}};

//-----------------------------------------------------------------------------
/// the variable length hash function H' of RFC 9106
KString HashLong(KStringView sInput, uint32_t iLength)
//-----------------------------------------------------------------------------
{
	KString sPrefix;
	AppendWord32(sPrefix, iLength);

	if (iLength <= 64)
	{
		KBlake2b Hash(iLength);
		Hash.Update(sPrefix);
		Hash.Update(sInput);

		return Hash.Final();
	}

	// a chain of digests of 64 bytes: every one but the last gives its first half
	KString sOutput;
	sOutput.reserve(iLength);

	KBlake2b First(64);
	First.Update(sPrefix);
	First.Update(sInput);
	auto sDigest = First.Final();

	sOutput.append(sDigest.data(), 32);

	auto iRemaining = iLength - 32;

	while (iRemaining > 64)
	{
		KBlake2b Next(64);
		Next.Update(sDigest);
		sDigest = Next.Final();

		sOutput.append(sDigest.data(), 32);
		iRemaining -= 32;
	}

	KBlake2b Last(iRemaining);
	Last.Update(sDigest);
	sOutput += Last.Final();

	return sOutput;

} // HashLong

// a memory block of 1024 bytes
using Block = std::array<uint64_t, 128>;

//-----------------------------------------------------------------------------
/// the round function of BLAKE2b with a multiplication added
inline void Mix(uint64_t& a, uint64_t& b, uint64_t& c, uint64_t& d)
//-----------------------------------------------------------------------------
{
	auto MultiplyAdd = [](uint64_t x, uint64_t y) -> uint64_t
	{
		return x + y + 2 * (x & 0xffffffffull) * (y & 0xffffffffull);
	};

	a = MultiplyAdd(a, b);
	d = kRotateRight(d ^ a, 32);
	c = MultiplyAdd(c, d);
	b = kRotateRight(b ^ c, 24);
	a = MultiplyAdd(a, b);
	d = kRotateRight(d ^ a, 16);
	c = MultiplyAdd(c, d);
	b = kRotateRight(b ^ c, 63);

} // Mix

//-----------------------------------------------------------------------------
/// one round over 16 words of a block
/// @param At maps the numbers 0 to 15 to the positions of the words in the block
template<typename Position>
inline void Round(Block& B, Position At)
//-----------------------------------------------------------------------------
{
	Mix(B[At(0)], B[At(4)], B[At( 8)], B[At(12)]);
	Mix(B[At(1)], B[At(5)], B[At( 9)], B[At(13)]);
	Mix(B[At(2)], B[At(6)], B[At(10)], B[At(14)]);
	Mix(B[At(3)], B[At(7)], B[At(11)], B[At(15)]);
	Mix(B[At(0)], B[At(5)], B[At(10)], B[At(15)]);
	Mix(B[At(1)], B[At(6)], B[At(11)], B[At(12)]);
	Mix(B[At(2)], B[At(7)], B[At( 8)], B[At(13)]);
	Mix(B[At(3)], B[At(4)], B[At( 9)], B[At(14)]);

} // Round

//-----------------------------------------------------------------------------
/// the compression function G of RFC 9106
/// @param Next receives the new block. May be the same block as Reference.
/// @param bXor combine the new block with the content of Next (from the second pass on)
void FillBlock(const Block& Previous, const Block& Reference, Block& Next, bool bXor)
//-----------------------------------------------------------------------------
{
	Block R;

	for (std::size_t i = 0; i < R.size(); ++i)
	{
		R[i] = Reference[i] ^ Previous[i];
	}

	Block Start = R;

	if (bXor)
	{
		for (std::size_t i = 0; i < Start.size(); ++i)
		{
			Start[i] ^= Next[i];
		}
	}

	// the block is a matrix of 8 x 8 registers of two words: first the rows ..
	for (std::size_t iRow = 0; iRow < 8; ++iRow)
	{
		Round(R, [iRow](std::size_t i) { return 16 * iRow + i; });
	}

	// .. then the columns
	for (std::size_t iColumn = 0; iColumn < 8; ++iColumn)
	{
		Round(R, [iColumn](std::size_t i) { return 2 * iColumn + 16 * (i / 2) + (i & 1); });
	}

	for (std::size_t i = 0; i < R.size(); ++i)
	{
		Next[i] = Start[i] ^ R[i];
	}

} // FillBlock

// the layout of the memory
struct Geometry
{
	uint32_t iVariant;
	uint32_t iPasses;
	uint32_t iLanes;
	uint32_t iLaneLength;
	uint32_t iSegmentLength;
	uint32_t iBlocks;
};

// every pass over a lane has four segments, the lanes are independent inside a segment
constexpr uint32_t iSlices          = 4;
// one block of pseudo random values addresses this many blocks
constexpr uint32_t iAddressesInBlock = 128;

//-----------------------------------------------------------------------------
/// fills one segment of one lane
void FillSegment(std::vector<Block>& Memory, const Geometry& Geo, uint32_t iPass, uint32_t iSlice, uint32_t iLane)
//-----------------------------------------------------------------------------
{
	// Argon2i takes the reference blocks independent of the data, Argon2d dependent
	// on it, Argon2id independent in the first half of the first pass
	auto bIndependent = (Geo.iVariant == 1) || (Geo.iVariant == 2 && iPass == 0 && iSlice < iSlices / 2);

	Block Zero;
	Block Input;
	Block Addresses;

	Zero.fill(0);
	Input.fill(0);
	Addresses.fill(0);

	if (bIndependent)
	{
		Input[0] = iPass;
		Input[1] = iLane;
		Input[2] = iSlice;
		Input[3] = Geo.iBlocks;
		Input[4] = Geo.iPasses;
		Input[5] = Geo.iVariant;
	}

	auto NextAddresses = [&]()
	{
		++Input[6];
		FillBlock(Zero, Input,     Addresses, false);
		FillBlock(Zero, Addresses, Addresses, false);
	};

	uint32_t iStart = 0;

	if (iPass == 0 && iSlice == 0)
	{
		// the first two blocks of the lane are there already
		iStart = 2;

		if (bIndependent)
		{
			NextAddresses();
		}
	}

	auto iLaneStart = iLane * Geo.iLaneLength;

	for (uint32_t iIndex = iStart; iIndex < Geo.iSegmentLength; ++iIndex)
	{
		// the position in the lane, and the block before it (around the end of the lane)
		auto iColumn   = iSlice * Geo.iSegmentLength + iIndex;
		auto iPrevious = iLaneStart + (iColumn == 0 ? Geo.iLaneLength - 1 : iColumn - 1);

		uint64_t iRandom { 0 };

		if (bIndependent)
		{
			if (iIndex % iAddressesInBlock == 0)
			{
				NextAddresses();
			}

			iRandom = Addresses[iIndex % iAddressesInBlock];
		}
		else
		{
			iRandom = Memory[iPrevious][0];
		}

		// the lane of the reference block
		auto iReferenceLane = static_cast<uint32_t>((iRandom >> 32) % Geo.iLanes);

		if (iPass == 0 && iSlice == 0)
		{
			// the other lanes have no finished segment yet
			iReferenceLane = iLane;
		}

		auto bSameLane = iReferenceLane == iLane;

		// the count of blocks that can be referenced
		uint64_t iAreaSize { 0 };

		if (iPass == 0)
		{
			if (iSlice == 0)
			{
				// all blocks of this segment but the previous one
				iAreaSize = iIndex - 1;
			}
			else if (bSameLane)
			{
				iAreaSize = iSlice * Geo.iSegmentLength + iIndex - 1;
			}
			else
			{
				iAreaSize = iSlice * Geo.iSegmentLength - (iIndex == 0 ? 1 : 0);
			}
		}
		else if (bSameLane)
		{
			iAreaSize = Geo.iLaneLength - Geo.iSegmentLength + iIndex - 1;
		}
		else
		{
			iAreaSize = Geo.iLaneLength - Geo.iSegmentLength - (iIndex == 0 ? 1 : 0);
		}

		// blocks near the current one are chosen more often
		uint64_t iRelative = iRandom & 0xffffffffull;
		iRelative = (iRelative * iRelative) >> 32;
		iRelative = iAreaSize - 1 - ((iAreaSize * iRelative) >> 32);

		// from the second pass on the area starts behind the current segment
		uint64_t iAreaStart = 0;

		if (iPass != 0 && iSlice != iSlices - 1)
		{
			iAreaStart = (iSlice + 1) * Geo.iSegmentLength;
		}

		auto iReference = iReferenceLane * Geo.iLaneLength
		                + static_cast<uint32_t>((iAreaStart + iRelative) % Geo.iLaneLength);

		FillBlock(Memory[iPrevious], Memory[iReference], Memory[iLaneStart + iColumn], iPass != 0);
	}

} // FillSegment

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KString KArgon2Core::Blake2b(KStringView sInput, std::size_t iDigestLength)
//-----------------------------------------------------------------------------
{
	if (iDigestLength < 1 || iDigestLength > 64)
	{
		return {};
	}

	KBlake2b Hash(iDigestLength);
	Hash.Update(sInput);

	return Hash.Final();

} // Blake2b

//-----------------------------------------------------------------------------
KString KArgon2Core::Derive(uint32_t    iVariant,
                            KStringView sPassword,
                            KStringView sSalt,
                            KStringView sSecret,
                            KStringView sAssociatedData,
                            uint32_t    iMemoryKiB,
                            uint32_t    iIterations,
                            uint32_t    iParallelism,
                            std::size_t iTagLength)
//-----------------------------------------------------------------------------
{
	constexpr std::size_t iMaxLength = std::numeric_limits<uint32_t>::max();

	if (iVariant        > 2
	 || iParallelism    < 1
	 || iParallelism    > 0x00ffffff
	 || iIterations     < 1
	 || iMemoryKiB / 8  < iParallelism
	 || iTagLength      < 4
	 || iTagLength      > iMaxLength
	 || sSalt.size()    < 8
	 || sSalt.size()    > iMaxLength
	 || sPassword.size()        > iMaxLength
	 || sSecret.size()          > iMaxLength
	 || sAssociatedData.size()  > iMaxLength)
	{
		return {};
	}

	// the first hash binds all parameters and inputs
	KString sHeader;
	AppendWord32(sHeader, iParallelism);
	AppendWord32(sHeader, static_cast<uint32_t>(iTagLength));
	AppendWord32(sHeader, iMemoryKiB);
	AppendWord32(sHeader, iIterations);
	AppendWord32(sHeader, 0x13);
	AppendWord32(sHeader, iVariant);

	KBlake2b First(64);
	First.Update(sHeader);

	for (auto sInput : { sPassword, sSalt, sSecret, sAssociatedData })
	{
		KString sLength;
		AppendWord32(sLength, static_cast<uint32_t>(sInput.size()));
		First.Update(sLength);
		First.Update(sInput);
	}

	auto sSeed = First.Final();

	// the memory is rounded down to a multiple of four blocks per lane
	Geometry Geo;
	Geo.iVariant       = iVariant;
	Geo.iPasses        = iIterations;
	Geo.iLanes         = iParallelism;
	Geo.iSegmentLength = iMemoryKiB / (iSlices * iParallelism);
	Geo.iLaneLength    = Geo.iSegmentLength * iSlices;
	Geo.iBlocks        = Geo.iLaneLength * iParallelism;

	std::vector<Block> Memory;

	DEKAF2_TRY
	{
		Memory.resize(Geo.iBlocks);
	}
	DEKAF2_CATCH (const std::bad_alloc&)
	{
		kSafeZeroize(sSeed);
		return {};
	}

	// the first two blocks of every lane come from the first hash
	for (uint32_t iLane = 0; iLane < Geo.iLanes; ++iLane)
	{
		for (uint32_t iColumn = 0; iColumn < 2; ++iColumn)
		{
			auto sInput = sSeed;
			AppendWord32(sInput, iColumn);
			AppendWord32(sInput, iLane);

			auto  sBytes = HashLong(sInput, sizeof(Block));
			auto& Target = Memory[iLane * Geo.iLaneLength + iColumn];

			for (std::size_t i = 0; i < Target.size(); ++i)
			{
				Target[i] = LoadWord(sBytes, i * sizeof(uint64_t));
			}

			kSafeZeroize(sInput);
			kSafeZeroize(sBytes);
		}
	}

	for (uint32_t iPass = 0; iPass < Geo.iPasses; ++iPass)
	{
		for (uint32_t iSlice = 0; iSlice < iSlices; ++iSlice)
		{
			for (uint32_t iLane = 0; iLane < Geo.iLanes; ++iLane)
			{
				FillSegment(Memory, Geo, iPass, iSlice, iLane);
			}
		}
	}

	// the tag is the hash over the last blocks of all lanes
	Block Final = Memory[Geo.iLaneLength - 1];

	for (uint32_t iLane = 1; iLane < Geo.iLanes; ++iLane)
	{
		const auto& Last = Memory[iLane * Geo.iLaneLength + Geo.iLaneLength - 1];

		for (std::size_t i = 0; i < Final.size(); ++i)
		{
			Final[i] ^= Last[i];
		}
	}

	KString sFinal;
	sFinal.reserve(sizeof(Block));

	for (auto iWord : Final)
	{
		AppendWord(sFinal, iWord);
	}

	auto sTag = HashLong(sFinal, static_cast<uint32_t>(iTagLength));

	// leave nothing of the password behind
	for (auto& B : Memory)
	{
		B.fill(0);
	}

	Final.fill(0);
	kSafeZeroize(sFinal);
	kSafeZeroize(sSeed);

	return sTag;

} // Derive

} // end of namespace detail

DEKAF2_NAMESPACE_END
