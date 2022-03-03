#include "catch.hpp"

#include <dekaf2/kdiffer.h>

using namespace dekaf2;

TEST_CASE("KDiffer")
{
	SECTION("KDiffToASCI")
	{
		KStringView sText1    { "This is a fat cat"   };
		KStringView sText2    { "That is a black hat" };
		KStringView sExpected { "Th[-is][+at] is a [-fat c][+black h]at" };

		auto sDiff = KDiffToASCII(sText1, sText2);
		CHECK ( sDiff == sExpected );
	}

	SECTION("KDiffToHTML")
	{
		KStringView sText1    { "This is a fat cat"   };
		KStringView sText2    { "That is a black hat" };
		KStringView sExpected { "Th<del>is</del><ins>at</ins> is a <del>fat c</del><ins>black h</ins>at" };

		auto sDiff = KDiffToHTML(sText1, sText2);
		CHECK ( sDiff == sExpected );
	}

}
