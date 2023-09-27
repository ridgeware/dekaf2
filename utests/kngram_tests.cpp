
#include "catch.hpp"
#include <dekaf2/kngram.h>

using namespace dekaf2;

TEST_CASE("KNGrams")
{
	SECTION("Bigrams")
	{
		using NGType = KNGrams<2, true>;

		static constexpr KStringView sSource1 = "This is the first sentence.";
		static constexpr KStringView sSource2 = "This- is[]   the=   =first sentence.";

		NGType NGrams1(sSource1);

		CHECK( NGrams1.Match(sSource1) == 100 );
		CHECK( NGrams1.Match(sSource2) == 100 );
		CHECK( NGrams1.Match("  \t\r\n " + sSource2 + "\t\r     ") == 100 );
		auto iPercent = NGrams1.Match("this is _a_ Sentence.");
		CHECK( iPercent >= 55 );
		CHECK( iPercent <= 65 );

		NGType NGrams2(sSource1);

		CHECK( NGrams1.Match(NGrams2) == 100 );
	}

	SECTION("Trigrams")
	{
		using NGType = KNGrams<3, true>;

		static constexpr KStringView sSource1 = "This is the first sentence.";
		static constexpr KStringView sSource2 = "This- is[]   the=   =first sentence.";

		NGType NGrams1(sSource1);

		CHECK( NGrams1.Match(sSource1) == 100 );
		CHECK( NGrams1.Match(sSource2) == 100 );
		CHECK( NGrams1.Match("  \t\r\n " + sSource2 + "\t\r     ") == 100 );
		auto iPercent = NGrams1.Match("this is _a_ Sentence.");
		CHECK( iPercent >= 50 );
		CHECK( iPercent <= 60 );

		NGType NGrams2(sSource1);

		CHECK( NGrams1.Match(NGrams2) == 100 );
	}

	SECTION("Pentagrams")
	{
		using NGType = KNGrams<5, true>;

		static constexpr KStringView sSource1 = "This is the first sentence.";
		static constexpr KStringView sSource2 = "This- is[]   the=   =first sentence.";

		NGType NGrams1(sSource1);

		CHECK( NGrams1.Match(sSource1) == 100 );
		CHECK( NGrams1.Match(sSource2) == 100 );
		CHECK( NGrams1.Match("  \t\r\n " + sSource2 + "\t\r     ") == 100 );
		auto iPercent = NGrams1.Match("this is _a_ Sentence.");
		CHECK( iPercent >= 40 );
		CHECK( iPercent <= 50 );

		NGType NGrams2(sSource1);

		CHECK( NGrams1.Match(NGrams2) == 100 );
	}

	SECTION("Octograms")
	{
		using NGType = KNGrams<8, true>;

		static constexpr KStringView sSource1 = "This is the first sentence.";
		static constexpr KStringView sSource2 = "This- is[]   the=   =first sentence.";

		NGType NGrams1(sSource1);

		CHECK( NGrams1.Match(sSource1) == 100 );
		CHECK( NGrams1.Match(sSource2) == 100 );
		CHECK( NGrams1.Match("  \t\r\n " + sSource2 + "\t\r     ") == 100 );
		auto iPercent = NGrams1.Match("this is _a_ Sentence.");
		CHECK( iPercent >= 20 );
		CHECK( iPercent <= 30 );

		NGType NGrams2(sSource1);

		CHECK( NGrams1.Match(NGrams2) == 100 );
	}

	SECTION("Trigrams w/o spaces")
	{
		using NGType = KNGrams<3, false>;

		static constexpr KStringView sSource1 = "This is the first sentence.";
		static constexpr KStringView sSource2 = "This- is[]   the=   =first sentence.";

		NGType NGrams1(sSource1);

		CHECK( NGrams1.Match(sSource1) == 100 );
		CHECK( NGrams1.Match(sSource2) == 100 );
		CHECK( NGrams1.Match("  \t\r\n " + sSource2 + "\t\r     ") == 100 );
		auto iPercent = NGrams1.Match("this is _a_ Sentence.");
		CHECK( iPercent >= 50 );
		CHECK( iPercent <= 60 );

		NGType NGrams2(sSource1);

		CHECK( NGrams1.Match(NGrams2) == 100 );
	}

	SECTION("Pentagrams w/o spaces")
	{
		using NGType = KNGrams<5, false>;

		static constexpr KStringView sSource1 = "This is the first sentence.";
		static constexpr KStringView sSource2 = "This- is[]   the=   =first sentence.";

		NGType NGrams1(sSource1);

		CHECK( NGrams1.Match(sSource1) == 100 );
		CHECK( NGrams1.Match(sSource2) == 100 );
		CHECK( NGrams1.Match("  \t\r\n " + sSource2 + "\t\r     ") == 100 );
		auto iPercent = NGrams1.Match("this is _a_ Sentence.");
		CHECK( iPercent >= 40 );
		CHECK( iPercent <= 50 );

		NGType NGrams2(sSource1);

		CHECK( NGrams1.Match(NGrams2) == 100 );
	}
}

