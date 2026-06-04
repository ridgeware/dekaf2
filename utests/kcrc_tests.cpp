#include "catch.hpp"

#include <dekaf2/crypto/hash/kcrc.h>
#include <vector>
#include <cstdint>

using namespace dekaf2;

TEST_CASE("KCRC")
{
	SECTION("KCRC32")
	{
		KCRC32 crc1("abcdefg");
		crc1 += "hijklm";
		crc1("nopq");
		crc1.Update("rstuvw");
		KCRC32 crc2("abcdefghijklmnopqrstuvw");
		CHECK( crc2() == crc1() );
		CHECK( KString::to_string(crc1()) == "955461994"_ks );
		crc1.clear();
		crc2.clear();
		CHECK( crc2() == crc1() );
	}

	// known-good CRC-32 (poly 0x04C11DB7, reflected, init/xorout 0xFFFFFFFF) values.
	// These are stable, published reference vectors - no external library needed.
	SECTION("known reference values")
	{
		CHECK( KCRC32("").CRC()          == 0x00000000u );
		CHECK( KCRC32("123456789").CRC() == 0xCBF43926u ); // the canonical CRC-32 check value
		CHECK( KCRC32("a").CRC()         == 0xE8B7BE43u );
		CHECK( KCRC32("abc").CRC()       == 0x352441C2u );
		CHECK( KCRC32("The quick brown fox jumps over the lazy dog").CRC() == 0x414FA339u );
		CHECK( KCRC32("abcdefghijklmnopqrstuvw").CRC() == 0x38F3316Au );          // == 955461994

		// every byte value 0x00..0xFF in order
		KString sAllBytes;
		for (int i = 0; i < 256; ++i) sAllBytes += static_cast<KString::value_type>(i);
		CHECK( KCRC32(sAllBytes).CRC() == 0x29058C73u );

		// binary data with embedded NULs
		CHECK( KCRC32(KString("a\0b\0c", 5)).CRC() == 0xFD98864Cu );
		CHECK( KCRC32(KString("\0\0\0\0", 4)).CRC() == 0x2144DF1Cu );
	}

	// the table-driven class and the bitwise free function are two independent
	// implementations; agreeing with each other AND the anchors above is a strong
	// correctness signal without depending on a third-party CRC.
	SECTION("table-driven KCRC32 and bitwise kCRC32 agree")
	{
		std::vector<KString> corpus = {
			"", "a", "ab", "abc", "123456789", "kssod", "login", "logout",
			"The quick brown fox jumps over the lazy dog",
			"abcdefghijklmnopqrstuvw",
		};
		for (int i = 0; i < 256; ++i) corpus.push_back(KString(1, static_cast<KString::value_type>(i)));
		corpus.push_back(KString("a\0b\0c", 5));
		corpus.push_back(KString("\0\0\0\0", 4));

		for (const auto& s : corpus)
		{
			CHECK( KCRC32(s).CRC() == kCRC32(s) );
		}
	}

	SECTION("kCRC32 is evaluated at compile time")
	{
		// these would fail to compile if kCRC32 were not constexpr
		static_assert(kCRC32("")          == 0x00000000u, "");
		static_assert(kCRC32("123456789") == 0xCBF43926u, "");
		static_assert(kCRC32("abc")       == 0x352441C2u, "");
		static_assert(kCRC32("The quick brown fox jumps over the lazy dog") == 0x414FA339u, "");
	}

	SECTION("incremental updates match a one-shot computation")
	{
		KString sWhole;
		KCRC32  Incremental;
		for (int i = 0; i < 500; ++i)
		{
			KString sChunk = KString(1, static_cast<KString::value_type>((i * 37) & 0xFF));
			sChunk += KString(1, static_cast<KString::value_type>((i * 11 + 3) & 0xFF));
			Incremental += sChunk;
			sWhole      += sChunk;
		}
		CHECK( Incremental.CRC() == KCRC32(sWhole).CRC() );
	}
}
