#include "catch.hpp"

#include <cstring>
#include <dekaf2/kbit.h>

using namespace dekaf2;

TEST_CASE("KBit")
{
	SECTION("kByteSwap")
	{
		uint64_t a = 0x0102030405060708;
		auto b = kByteSwap(a);
		CHECK ( b == 0x0807060504030201 );
	}

	SECTION("kRotateLeft")
	{
		uint16_t a = 5;
		a = kRotateLeft(a, 4);
		CHECK ( a == 80 );
		a = kRotateLeft(a, 12);
		CHECK ( a == 5 );
	}

	SECTION("kRotateRight")
	{
		uint16_t a = 5;
		a = kRotateRight(a, 4);
		CHECK ( a == 20480 );
		a = kRotateRight(a, 12);
		CHECK ( a == 5 );
	}

	SECTION("kHasSingleBit")
	{
		auto b = kHasSingleBit(5u);
		CHECK ( b == false );
		b = kHasSingleBit(8u);
		CHECK ( b == true );
	}

	SECTION("kBitCountLeftZero")
	{
		uint32_t a = 5;
		a = kBitCountLeftZero(a);
		CHECK ( a == 29 );
	}

	SECTION("kBitCountRightZero")
	{
		uint32_t a = 5;
		a = kBitCountRightZero(a);
		CHECK ( a == 0 );
		a = kBitCountRightZero(58608u);
		CHECK ( a == 4 );
	}

	SECTION("kBitCountLeftOne")
	{
		uint16_t a = 5;
		a = kBitCountLeftOne(a);
		CHECK ( a == 0 );
		a = 58608;
		a = kBitCountLeftOne(a);
		CHECK ( a == 3 );
	}

	SECTION("kBitCountRightOne")
	{
		uint16_t a = 5;
		a = kBitCountRightOne(a);
		CHECK ( a == 1 );
		a = 58608;
		a = kBitCountRightOne(a);
		CHECK ( a == 0 );
	}

	SECTION("kBitCeil")
	{
		auto a = kBitCeil(5u);
		CHECK ( a == 8 );
	}

	SECTION("kBitFloor")
	{
		auto a = kBitFloor(5u);
		CHECK ( a == 4 );
	}

	SECTION("kBitWidth")
	{
		auto a = kBitWidth(5u);
		CHECK ( a == 3 );
	}

	SECTION("kBitCountOne")
	{
		auto a = kBitCountOne(5u);
		CHECK ( a == 2 );
	}

	SECTION("Endianess")
	{
		if (kIsLittleEndian())
		{
			CHECK ( kIsBigEndian() == false );
			uint32_t iVal = 0x89abcdef;
			uint32_t iBE  = iVal;
			kToBigEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			kToLittleEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			iBE = kByteSwap(iBE);
			CHECK ( iBE == 0x89abcdef );
		}
		else
		{
			CHECK ( kIsBigEndian()    == true  );
			CHECK ( kIsLittleEndian() == false );
			uint32_t iVal = 0x89abcdef;
			uint32_t iBE  = iVal;
			kToLittleEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			kToBigEndian(iBE);
			CHECK ( iBE == 0xefcdab89 );
			iBE = kByteSwap(iBE);
			CHECK ( iBE == 0x89abcdef );
		}

		{
			uint8_t iVal = 0x89;
			uint8_t iBE  = iVal;
			kToBigEndian(iBE);
			CHECK ( iBE == 0x89 );
			kToLittleEndian(iBE);
			CHECK ( iBE == 0x89 );
		}
	}
}
