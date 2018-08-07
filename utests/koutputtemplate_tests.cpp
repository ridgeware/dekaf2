#include "catch.hpp"

#include <dekaf2/koutputtemplate.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KOutputTemplate")
{
	SECTION("Basic functionality")
	{
		KOutputTemplate Template;
		Template += R"(
Line 1
Line 2 BEGIN more text
Line 3
END more text
Line 4
)";

		KReplacer Replacer;
		Replacer.insert("Line", "Zeile");

		CHECK ( Template.Write("BEGIN", "END") == "Line 3\n" );
		CHECK ( Template.Write("BEGIN", "END", Replacer) == "Zeile 3\n" );
		CHECK ( Template.Write("", "BEGIN", Replacer) == "\nZeile 1\n" );
		CHECK ( Template.Write("END", "", Replacer) == "Zeile 4\n" );
	}
}
