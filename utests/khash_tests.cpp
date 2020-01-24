#include "catch.hpp"
#include <dekaf2/khash.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kcasestring.h>



using namespace dekaf2;


TEST_CASE("KHash")
{
	SECTION("Values")
	{
		KStringView sv;

		if (kIs64Bits())
		{
			CHECK ( sv.Hash() == UINT64_C(0) );
		}
		else
		{
			CHECK ( sv.Hash() == UINT32_C(0) );
		}

		sv = "hello";

		if (kIs64Bits())
		{
			CHECK ( sv.Hash() == UINT64_C(11831194018420276491) );
		}
		else
		{
			CHECK ( sv.Hash() == UINT32_C(1335831723) );
		}
		sv = "hällo";

		if (kIs64Bits())
		{
			CHECK ( sv.Hash() == UINT64_C(4976760422236553373) );
		}
		else
		{
			CHECK ( sv.Hash() == UINT32_C(1372156989) );
		}
	}

	SECTION("Literals")
	{
		constexpr const char huhu[] = "huhu";

		constexpr uint64_t iVal = 3823873453456;

		if (kHash(&iVal, sizeof(iVal)) == 0)
		{
			CHECK (false);
		}

		auto i1 = kHash("hällo");
		auto i2 = kHash("hällo", strlen("hällo")); // be careful, the umlaut consists of 2 chars..
		auto i3 = "hällo"_hash;

		CHECK ( i1 == i2 );
		CHECK ( i2 == i3 );

		KStringView sHello("hällo");

		switch (sHello.Hash())
		{
			default:
				CHECK ( false );
				break;

			case "hällo"_hash:
				CHECK ( true );
				break;

			case kHash("hehe"):
				CHECK ( false );
				break;

#ifdef DEKAF2_HAS_CPP_14
			case kHash("helo"_ksv.data(), 4):
				CHECK ( false );
				break;

			case KStringViewZ("hillo").Hash():
				CHECK ( false );
				break;
			case kHash("hihi", 4):
				CHECK ( false );
				break;
			case kHash(huhu, 4):
				CHECK ( false );
				break;

			case "hollo"_ksv.Hash():
				CHECK ( false );
				break;
#endif
		}
	}

	SECTION("CaseString")
	{
		KCaseStringView sHello("HäLlO");

		switch (sHello.Hash())
		{
			default:
				CHECK ( false );
				break;

			case "hällo"_hash:
				CHECK ( true );
				break;

#ifdef DEKAF2_HAS_CPP_14
			case KStringView("halo").Hash():
				CHECK ( false );
				break;

			case KStringViewZ("hello").Hash():
				CHECK ( false );
				break;

			case "hollo"_ksv.Hash():
				CHECK ( false );
				break;
#endif
		}
	}

}
