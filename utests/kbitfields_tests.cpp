#include "catch.hpp"

#include <cstring>
#include <dekaf2/kbitfields.h>
#include <dekaf2/kaddrplus.h>

using namespace dekaf2;

TEST_CASE("KBitfields")
{
	SECTION("Basic construction")
	{
		{
			KBitfields<4, 4, 4, 4> bf16;
			bf16.setStore(0x1327);
			CHECK ( bf16.get<3>() == 1 );
			CHECK ( bf16.get<2>() == 3 );
			CHECK ( bf16.get<1>() == 2 );
			CHECK ( bf16.get<0>() == 7 );
		}
		struct field
		{
			uint16_t b1 : 4;
			uint16_t b2 : 4;
			uint16_t b3 : 4;
			uint16_t b4 : 4;
		};
		{
			field bf;
			bf.b1 = 7;
			bf.b2 = 2;
			bf.b3 = 3;
			bf.b4 = 1;
			CHECK ( sizeof(bf) == 2 );
			CHECK ( sizeof(uint64_t) == 8 );
		}
		{
			KBitfields<4, 4, 4, 4> bf16;
			bf16.set<0>(12);
			bf16.set<1>(11);
			bf16.set<2>(10);
			bf16.set<3>(9);
			CHECK ( bf16.get<3>() == 9 );
			CHECK ( bf16.get<2>() == 10 );
			CHECK ( bf16.get<1>() == 11 );
			CHECK ( bf16.get<0>() == 12 );
		}
		{
			KBitfields<2, 6, 4, 4> bf16;
			bf16.set<0>(3);
			bf16.set<1>(11);
			bf16.set<2>(10);
			bf16.set<3>(9);
			CHECK ( bf16.get<3>() == 9 );
			CHECK ( bf16.get<2>() == 10 );
			CHECK ( bf16.get<1>() == 11 );
			CHECK ( bf16.get<0>() == 3 );
		}
	}

	SECTION("permutation")
	{
		KBitfields<2, 6, 1, 7> bf16;

		for (int i = 0; i <= UINT16_MAX; ++i)
		{
			bf16.setStore(0);
			bf16.set<0>(i);
			CHECK ( bf16.get<0>() == (i & 0x03) );
			CHECK ( bf16.get<1>() == 0 );
			CHECK ( bf16.get<2>() == 0 );
			CHECK ( bf16.get<3>() == 0 );
			CHECK ( bf16.getStore() == (i & 0x03) );
		}
		for (int i = 0; i <= UINT16_MAX; ++i)
		{
			bf16.setStore(0);
			bf16.set<1>(i);
			CHECK ( bf16.get<0>() == 0 );
			CHECK ( bf16.get<1>() == (i & 0x3f) );
			CHECK ( bf16.get<2>() == 0 );
			CHECK ( bf16.get<3>() == 0 );
			CHECK ( bf16.getStore() == ((i & 0x3f) << 2) );
		}
		for (int i = 0; i <= UINT16_MAX; ++i)
		{
			bf16.setStore(0);
			bf16.set<2>(i);
			CHECK ( bf16.get<0>() == 0 );
			CHECK ( bf16.get<1>() == 0 );
			CHECK ( bf16.get<2>() == (i & 0x01) );
			CHECK ( bf16.get<3>() == 0 );
			CHECK ( bf16.getStore() == ((i & 0x01) << 8) );
		}
		for (int i = 0; i <= UINT16_MAX; ++i)
		{
			bf16.setStore(0);
			bf16.set<3>(i);
			CHECK ( bf16.get<0>() == 0 );
			CHECK ( bf16.get<1>() == 0 );
			CHECK ( bf16.get<2>() == 0 );
			CHECK ( bf16.get<3>() == (i & 0x7f) );
			CHECK ( bf16.getStore() == ((i & 0x7f) << 9) );
		}
	}

	SECTION("64 bit")
	{
		using AddressPlus = KBitfields<56, 8>;
		CHECK ( sizeof(AddressPlus) == 8 );
		AddressPlus Address;
		Address.set<0>(0xffffffffffffff);
		Address.set<1>(0x9a);
		CHECK ( Address.get<0>() == 0xffffffffffffff );
		CHECK ( Address.get<1>() == 0x9a );
		Address.set<0>(40000000000000000);
		CHECK ( Address.get<0>() == 40000000000000000 );

		KBitfields<56, 8> bf = 0x9affffffffffffff;
		CHECK ( bf.get<0>() == 0xffffffffffffff );
		CHECK ( bf.get<1>() == 0x9a );
		CHECK ( bf == 0x9affffffffffffff );

		KBitfields<64> bf1;
		CHECK ( sizeof(bf1) == 8 );
		bf1.set<0>(0xffffffffffffffff);
		CHECK ( bf1.get<0>() == 0xffffffffffffffff );
		CHECK ( bf1 == 0xffffffffffffffff );
	}

	SECTION("32 bit")
	{
		KBitfields<24, 8> bf;
		CHECK ( sizeof(bf) == 4 );
		bf.set<0>(0xffffff);
		bf.set<1>(0x9a);
		CHECK ( bf.get<0>() == 0xffffff );
		CHECK ( bf.get<1>() == 0x9a );

		bf = 0x9befffff;
		CHECK ( bf.get<0>() == 0xefffff );
		CHECK ( bf.get<1>() == 0x9b );
		CHECK ( bf == 0x9befffff );

		KBitfields<32> bf1;
		CHECK ( sizeof(bf1) == 4 );
		bf1.set<0>(0xffffffff);
		CHECK ( bf1.get<0>() == 0xffffffff );
		CHECK ( bf1 == 0xffffffff );
	}

	SECTION("16 bit")
	{
		KBitfields<8, 8> bf;
		CHECK ( sizeof(bf) == 2 );
		bf.set<0>(0xff);
		bf.set<1>(0x9a);
		CHECK ( bf.get<0>() == 0xff );
		CHECK ( bf.get<1>() == 0x9a );

		bf = 0x9bef;
		CHECK ( bf.get<0>() == 0xef );
		CHECK ( bf.get<1>() == 0x9b );
		CHECK ( bf == 0x9bef );

		KBitfields<16> bf1;
		CHECK ( sizeof(bf1) == 2 );
		bf1.set<0>(0xffff);
		CHECK ( bf1.get<0>() == 0xffff );
		CHECK ( bf1 == 0xffff );
	}

	SECTION("8 bit")
	{
		KBitfields<4, 4> bf;
		CHECK ( sizeof(bf) == 1 );
		bf.set<0>(0xf);
		bf.set<1>(0x9);
		CHECK ( bf.get<0>() == 0xf );
		CHECK ( bf.get<1>() == 0x9 );

		bf = 0x9b;
		CHECK ( bf.get<0>() == 0xb );
		CHECK ( bf.get<1>() == 0x9 );
		CHECK ( bf == 0x9b );

		KBitfields<8> bf1;
		CHECK ( sizeof(bf1) == 1 );
		bf1.set<0>(0xff);
		CHECK ( bf1.get<0>() == 0xff );
		CHECK ( bf1 == 0xff );
	}

	SECTION("KAddrPlus")
	{
		KAddrPlus<const char, 8> address;
		CHECK ( sizeof(address) == sizeof(void*) );
		const char* p = "hello world";
		address = p;
		address.set<0>('c');
		auto a = address;
		CHECK ( a == p );
		CHECK ( address.get<0>() == 'c' );

		address = "another world";
		CHECK ( !strcmp(address, "another world") );
		CHECK ( address.get<0>() == 'c' );

		KAddrPlus<const char, 1, 7> address2{p};
		address2.set<1>('c');
		CHECK ( a == p );
		CHECK ( address2.get<1>() == 'c' );
	}
}
