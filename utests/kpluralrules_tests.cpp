#include "catch.hpp"

#include <dekaf2/util/i18n/kpluralrules.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/strings/kstringutils.h>
// the CLDR sample numbers, written into this directory by from/cldr/generate.sh
#include "kcldr_plural_samples.h"

using namespace dekaf2;

namespace {

// the sample numbers of a CLDR rule, "@integer 1, 21~24, 1001, … @decimal 0.1~0.9, 10.1, …" -
// ranges are expanded in steps of their last digit, compact forms (1c6) and the
// ellipsis are skipped
std::vector<KString> Samples(KStringView sSamples)
{
	std::vector<KString> Result;

	for (auto sPart : sSamples.Split("@", " "))
	{
		auto iSpace = sPart.find(' ');

		if (iSpace == KStringView::npos)
		{
			continue;
		}

		for (auto sSample : sPart.substr(iSpace + 1).Split(",", " "))
		{
			if (sSample.empty() || sSample.contains("\xE2\x80\xA6") || sSample.contains('c') || sSample.contains('e'))
			{
				continue;
			}

			auto iTilde = sSample.find('~');

			if (iTilde == KStringView::npos)
			{
				Result.emplace_back(sSample);
				continue;
			}

			auto sLo = sSample.substr(0, iTilde);
			auto sHi = sSample.substr(iTilde + 1);
			auto iDot = sLo.find('.');
			auto iDecimals = iDot == KStringView::npos ? 0 : sLo.size() - iDot - 1;

			// step through the range with the precision of the bounds
			int64_t iScale = 1;
			for (std::size_t i = 0; i < iDecimals; ++i) iScale *= 10;

			auto iLo = static_cast<int64_t>(sLo.Double() * iScale + 0.5);
			auto iHi = static_cast<int64_t>(sHi.Double() * iScale + 0.5);

			for (auto i = iLo; i <= iHi && Result.size() < 100000; ++i)
			{
				if (iDecimals == 0)
				{
					Result.push_back(kFormat("{}", i));
				}
				else
				{
					Result.push_back(kFormat("{}.{:0{}}", i / iScale, i % iScale, iDecimals));
				}
			}
		}
	}

	return Result;
}

template<std::size_t N>
void CheckSamples(const KCLDRPluralSample (&Table)[N], KPluralRules::Type Type, std::size_t& iChecked)
{
	for (const auto& Entry : Table)
	{
		for (const auto& sSample : Samples(Entry.sText))
		{
			auto sCategory = KPluralRules::Select(Entry.sLocale, KStringView(sSample), Type);

			if (sCategory != Entry.sCategory)
			{
				INFO(kFormat("{} {}: sample {} gave {}", Entry.sLocale, Entry.sCategory, sSample, sCategory));
				CHECK ( sCategory == Entry.sCategory );
			}

			++iChecked;
		}
	}
}

} // end of anonymous namespace

TEST_CASE("KPluralRules")
{
	SECTION("operands")
	{
		auto Ops = KPluralRules::GetOperands("-1.50");
		CHECK ( Ops.n == 1.5 );
		CHECK ( Ops.i == 1   );
		CHECK ( Ops.v == 2   );
		CHECK ( Ops.w == 1   );
		CHECK ( Ops.f == 50  );
		CHECK ( Ops.t == 5   );
		CHECK ( Ops.e == 0   );

		Ops = KPluralRules::GetOperands("1");
		CHECK ( Ops.n == 1 );
		CHECK ( Ops.i == 1 );
		CHECK ( Ops.v == 0 );
		CHECK ( Ops.f == 0 );

		Ops = KPluralRules::GetOperands("1.0");
		CHECK ( Ops.n == 1 );
		CHECK ( Ops.v == 1 );
		CHECK ( Ops.w == 0 );

		// a double has no visible fraction digits beyond its value: 1.0 is 1
		Ops = KPluralRules::GetOperands(1.0);
		CHECK ( Ops.v == 0 );
		Ops = KPluralRules::GetOperands(2.25);
		CHECK ( Ops.i == 2  );
		CHECK ( Ops.v == 2  );
		CHECK ( Ops.f == 25 );
		Ops = KPluralRules::GetOperands(-7.0);
		CHECK ( Ops.n == 7  );
		CHECK ( Ops.i == 7  );

		// compact notation from the CLDR samples: 1c6 is 1000000 with exponent 6
		Ops = KPluralRules::GetOperands("1c6");
		CHECK ( Ops.i == 1000000 );
		CHECK ( Ops.e == 6       );
		Ops = KPluralRules::GetOperands("1.1c6");
		CHECK ( Ops.i == 1100000 );
		CHECK ( Ops.v == 0       );
	}

	SECTION("every CLDR sample falls into its own category")
	{
		std::size_t iCardinal { 0 };
		std::size_t iOrdinal  { 0 };
		CheckSamples(kCLDRCardinalSamples, KPluralRules::Type::Cardinal, iCardinal);
		CheckSamples(kCLDROrdinalSamples,  KPluralRules::Type::Ordinal,  iOrdinal);
		// the tables have samples for every language - the counts guard the parser of
		// the samples (CLDR 48: about 12,000 cardinal and 1,500 ordinal samples)
		CHECK ( iCardinal > 10000 );
		CHECK ( iOrdinal  >  1000 );
	}

	SECTION("cardinal categories")
	{
		CHECK ( KPluralRules::Select("en", 0.0)  == "other" );
		CHECK ( KPluralRules::Select("en", 1.0)  == "one"   );
		CHECK ( KPluralRules::Select("en", 1.5)  == "other" );
		CHECK ( KPluralRules::Select("en", 2.0)  == "other" );
		CHECK ( KPluralRules::Select("en", KStringView("1.0")) == "other" ); // one visible fraction digit
		CHECK ( KPluralRules::Select("de", 1.0)  == "one"   );
		CHECK ( KPluralRules::Select("fr", 0.0)  == "one"   );
		CHECK ( KPluralRules::Select("fr", 1.5)  == "one"   );
		CHECK ( KPluralRules::Select("fr", 2.0)  == "other" );
		CHECK ( KPluralRules::Select("fr", 1000000.0) == "many" );
		CHECK ( KPluralRules::Select("ru", 1.0)  == "one"   );
		CHECK ( KPluralRules::Select("ru", 3.0)  == "few"   );
		CHECK ( KPluralRules::Select("ru", 5.0)  == "many"  );
		CHECK ( KPluralRules::Select("ru", 21.0) == "one"   );
		CHECK ( KPluralRules::Select("ru", 1.5)  == "other" );
		CHECK ( KPluralRules::Select("pl", 2.0)  == "few"   );
		CHECK ( KPluralRules::Select("pl", 12.0) == "many"  );
		CHECK ( KPluralRules::Select("pl", 22.0) == "few"   );
		CHECK ( KPluralRules::Select("ar", 0.0)  == "zero"  );
		CHECK ( KPluralRules::Select("ar", 1.0)  == "one"   );
		CHECK ( KPluralRules::Select("ar", 2.0)  == "two"   );
		CHECK ( KPluralRules::Select("ar", 3.0)  == "few"   );
		CHECK ( KPluralRules::Select("ar", 11.0) == "many"  );
		CHECK ( KPluralRules::Select("ar", 100.0) == "other" );
		CHECK ( KPluralRules::Select("ja", 1.0)  == "other" );
		CHECK ( KPluralRules::Select("en", -1.0) == "one"   );
	}

	SECTION("ordinal categories")
	{
		using T = KPluralRules::Type;
		CHECK ( KPluralRules::Select("en", 1.0,   T::Ordinal) == "one"   );
		CHECK ( KPluralRules::Select("en", 2.0,   T::Ordinal) == "two"   );
		CHECK ( KPluralRules::Select("en", 3.0,   T::Ordinal) == "few"   );
		CHECK ( KPluralRules::Select("en", 4.0,   T::Ordinal) == "other" );
		CHECK ( KPluralRules::Select("en", 11.0,  T::Ordinal) == "other" );
		CHECK ( KPluralRules::Select("en", 21.0,  T::Ordinal) == "one"   );
		CHECK ( KPluralRules::Select("en", 112.0, T::Ordinal) == "other" );
		CHECK ( KPluralRules::Select("fr", 1.0,   T::Ordinal) == "one"   );
		CHECK ( KPluralRules::Select("fr", 2.0,   T::Ordinal) == "other" );
		CHECK ( KPluralRules::Select("de", 1.0,   T::Ordinal) == "other" );
	}

	SECTION("language tags")
	{
		CHECK ( KPluralRules::HasLanguage("de")    == true  );
		CHECK ( KPluralRules::HasLanguage("de-DE") == true  );
		CHECK ( KPluralRules::HasLanguage("de_AT") == true  );
		CHECK ( KPluralRules::HasLanguage("pt-PT") == true  );
		CHECK ( KPluralRules::HasLanguage("xx")    == false );
		CHECK ( KPluralRules::HasLanguage("")      == false );

		// pt-PT has its own rules: 0 is "other" there, "one" in Brazilian Portuguese
		CHECK ( KPluralRules::Select("pt",    0.0) == "one"   );
		CHECK ( KPluralRules::Select("pt-BR", 0.0) == "one"   );
		CHECK ( KPluralRules::Select("pt-PT", 0.0) == "other" );
		CHECK ( KPluralRules::Select("pt_pt", 0.0) == "other" );

		// unknown languages use the English rules, like a browser
		CHECK ( KPluralRules::Select("xx", 1.0) == "one"   );
		CHECK ( KPluralRules::Select("xx", 2.0) == "other" );
	}

	SECTION("categories of a language")
	{
		CHECK ( (KPluralRules::GetCategories("en") == std::vector<KStringView>{ "one", "other" }) );
		CHECK ( (KPluralRules::GetCategories("ar") == std::vector<KStringView>{ "zero", "one", "two", "few", "many", "other" }) );
		CHECK ( (KPluralRules::GetCategories("ru") == std::vector<KStringView>{ "one", "few", "many", "other" }) );
		CHECK ( (KPluralRules::GetCategories("ja") == std::vector<KStringView>{ "other" }) );
		CHECK ( (KPluralRules::GetCategories("en", KPluralRules::Type::Ordinal) == std::vector<KStringView>{ "one", "two", "few", "other" }) );
	}

	SECTION("rules as text")
	{
		auto One  = KPluralRules::GetOperands("1");
		auto Two  = KPluralRules::GetOperands("2");
		auto Ten1 = KPluralRules::GetOperands("1.0");
		CHECK ( KPluralRules::Matches("i = 1 and v = 0", One)  == true  );
		CHECK ( KPluralRules::Matches("i = 1 and v = 0", Two)  == false );
		CHECK ( KPluralRules::Matches("i = 1 and v = 0", Ten1) == false );
		CHECK ( KPluralRules::Matches("n % 10 = 2..4 and n % 100 != 12..14", KPluralRules::GetOperands("23"))  == true  );
		CHECK ( KPluralRules::Matches("n % 10 = 2..4 and n % 100 != 12..14", KPluralRules::GetOperands("13"))  == false );
		CHECK ( KPluralRules::Matches("n = 0,1", KPluralRules::GetOperands("0"))   == true  );
		CHECK ( KPluralRules::Matches("n = 0,1", KPluralRules::GetOperands("0.5")) == false );
		CHECK ( KPluralRules::Matches("i = 0 or n = 1", KPluralRules::GetOperands("0.5")) == true );
		CHECK ( KPluralRules::Matches("", One)                 == true  ); // "other"
		CHECK ( KPluralRules::Matches("i = 1 and", One)        == false ); // not a rule
		CHECK ( KPluralRules::Matches("x = 1", One)            == false );
		// a range holds integers only
		CHECK ( KPluralRules::Matches("n = 2..4", KPluralRules::GetOperands("2.5")) == false );
		CHECK ( KPluralRules::Matches("n != 2..4", KPluralRules::GetOperands("2.5")) == true );
	}

	SECTION("version")
	{
		CHECK ( KPluralRules::GetCLDRVersion().empty() == false );
	}
}
