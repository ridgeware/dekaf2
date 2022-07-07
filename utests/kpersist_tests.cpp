#include "catch.hpp"
#include <dekaf2/kpersist.h>
#include <dekaf2/kstring.h>

using namespace dekaf2;

TEST_CASE("KPersist")
{
	SECTION("Basic")
	{
		KPersistStrings<KString, true> Strings;

		KStringView sv = Strings.Persist("in data section");
		CHECK ( kIsInsideDataSegment(sv.data()) == true );
		CHECK ( Strings.empty() );
		CHECK ( sv == "in data section" );

		sv.clear();
		sv = Strings.Persist("in data section"_ksv);
		CHECK ( kIsInsideDataSegment(sv.data()) == true );
		CHECK ( Strings.empty() );
		CHECK ( sv == "in data section" );

		sv.clear();
		KStringView sv2 = "also in data section";
		sv = Strings.Persist(sv2);
		CHECK ( kIsInsideDataSegment(sv.data()) == true );
		CHECK ( kIsInsideDataSegment(&sv) == false );
		CHECK ( Strings.empty() );
		CHECK ( sv == sv2 );

		sv.clear();
		KStringViewZ sv3 = "KStringViewZ also in data section";
		sv = Strings.Persist(sv3);
		CHECK ( kIsInsideDataSegment(sv.data()) == true );
		CHECK ( Strings.empty() );
		CHECK ( sv == sv3 );

		sv.clear();
		std::string_view sv4 = "std::string_view also in data section";
		sv = Strings.Persist(sv4);
		CHECK ( kIsInsideDataSegment(sv.data()) == true );
		CHECK ( Strings.empty() );
		CHECK ( sv == sv4 );

		sv.clear();
		static constexpr KStringViewZ csv = "constexpr also in data section";
		sv = Strings.Persist(csv);
		CHECK ( kIsInsideDataSegment(csv.data()) == true );
		CHECK ( kIsInsideDataSegment(&csv) == true );
		CHECK ( Strings.empty() );
		CHECK ( sv == csv );

		KString sEmpty;
		sv = Strings.Persist(sEmpty);
		// this case is not constructed from the data segment on all
		// operating systems, let us excempt it from checking (however,
		// the default construction is caught, and no element is added
		// to the forward list)
//		CHECK ( kIsInsideDataSegment(sv.data()) == true );
		CHECK ( Strings.empty() == true );
		CHECK ( sv == sEmpty );

		sv.clear();
		char cp[30] = "not in data section";
		KStringViewZ svz = Strings.Persist(cp);
		CHECK ( kIsInsideDataSegment(svz.data()) == false );
		CHECK ( Strings.empty() == false );
		CHECK ( !strncmp(cp, svz.c_str(), strlen(cp)) );

		{
			sv.clear();
			KString sTest ("short string");
			sv = Strings.Persist(sTest);
		}
		CHECK ( kIsInsideDataSegment(sv.data()) == false );
		CHECK ( Strings.empty() == false );
		CHECK ( sv == "short string" );

		{
			sv.clear();
			KString sTest ("short string");
			sv2 = sTest;
			sv = Strings.Persist(sv2);
		}
		CHECK ( kIsInsideDataSegment(sv.data()) == false );
		CHECK ( Strings.empty() == false );
		CHECK ( sv == "short string" );

		{
			KString s1 = "duplicate string values get deduplicated";
			KString s2 = "duplicate string values get deduplicated";
			CHECK ( s1 == s2 );
			CHECK ( s1.data() != s2.data() );
			KStringView sv1 = Strings.Persist(s1);
			KStringView sv2 = Strings.Persist(s2);
			CHECK ( sv1 == sv2 );
			CHECK ( sv1.data() == sv2.data() );
			CHECK ( kIsInsideDataSegment(sv1.data()) == false );
			CHECK ( kIsInsideDataSegment(sv2.data()) == false );
		}

		CHECK ( std::distance(Strings.begin(), Strings.end()) == 3 );
	}
}
