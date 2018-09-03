
#include <dekaf2/kbitfields.h>
#include <dekaf2/kprof.h>
#include <iostream>

//#include <dekaf2/kaddrplus.h>

using namespace dekaf2;

using KBitFields = dekaf2::KBitfields<2, 4, 8, 2>;

struct myfields
{
	union {
		uint16_t all;
		struct {
			uint16_t a : 2;
			uint16_t b : 4;
			uint16_t c : 8;
			uint16_t d : 2;
		};
	};
};

void kbitfields()
{
	dekaf2::KProf ps("-KBitfields");

	{
		KBitFields bf;
		dekaf2::KProf prof("set 4 fields (0)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000 && bf.get<0>() != 2; ++ct)
		{
			KProf::Force(&bf);
			bf.set<0>(3);
			bf.set<1>(4);
			bf.set<2>(200);
			bf.set<3>(1);
			KProf::Force(&bf);
			KProf::Force();
		}
	}

	{
		KBitFields bf;
		dekaf2::KProf prof("set all 4 fields (0)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000 && bf.get<0>() != 2; ++ct)
		{
			KProf::Force(&bf);
			bf.setStore(23976);
			KProf::Force(&bf);
			KProf::Force();
		}
	}

	{
		KBitFields bf;
		bf.set<0>(3);
		bf.set<1>(4);
		bf.set<2>(200);
		bf.set<3>(1);

		dekaf2::KProf prof("get 4 fields (0)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000; ++ct)
		{
			KProf::Force(&bf);
			if (bf.get<0>() == 0) { KProf::Force(&bf); };
			if (bf.get<1>() == 0) { KProf::Force(&bf); };
			if (bf.get<2>() == 0) { KProf::Force(&bf); };
			if (bf.get<3>() == 0) { KProf::Force(&bf); };
			KProf::Force();
		}
	}

	{
		KBitFields bf;
		bf.set<0>(3);
		bf.set<1>(4);
		bf.set<2>(200);
		bf.set<3>(1);

		dekaf2::KProf prof("get all 4 fields (0)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000; ++ct)
		{
			KProf::Force(&bf);
			if (bf.getStore() == 8232) { KProf::Force(&bf); }
			KProf::Force();
		}
	}

}

void stdbitfields()
{
	dekaf2::KProf ps("-std::bitfields");

	{
		myfields bf;
		dekaf2::KProf prof("set 4 fields (1)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000 && bf.a != 2; ++ct)
		{
			KProf::Force(&bf);
			bf.a = 3;
			bf.b = 4;
			bf.c = 200;
			bf.d = 1;
			KProf::Force(&bf);
			KProf::Force();
		}
	}

	{
		myfields bf;
		dekaf2::KProf prof("set all 4 fields (1)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000 && bf.a != 2; ++ct)
		{
			KProf::Force(&bf);
			bf.all = 23976;
			KProf::Force(&bf);
			KProf::Force();
		}
	}

	{
		myfields bf;
		bf.a = 3;
		bf.b = 4;
		bf.c = 200;
		bf.d = 1;

		dekaf2::KProf prof("get 4 fields (1)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000; ++ct)
		{
			KProf::Force(&bf);
			if (bf.a == 0) {  KProf::Force(&bf); };
			if (bf.b == 0) {  KProf::Force(&bf); };
			if (bf.c == 0) {  KProf::Force(&bf); };
			if (bf.d == 0) {  KProf::Force(&bf); };
			KProf::Force();
		}
	}

	{
		myfields bf;
		bf.a = 3;
		bf.b = 4;
		bf.c = 200;
		bf.d = 1;

		dekaf2::KProf prof("get all 4 fields (1)");
		prof.SetMultiplier(10000000);
		for (int ct = 0; ct < 10000000; ++ct)
		{
			KProf::Force(&bf);
			if (bf.all == 8232) {  KProf::Force(&bf); };
			KProf::Force();
		}
	}

}

void kbitfields_bench()
{
	stdbitfields();
	kbitfields();
}


