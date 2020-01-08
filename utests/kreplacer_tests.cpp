#include "catch.hpp"

#include <dekaf2/kreplacer.h>
#include <vector>
#include <dekaf2/kprops.h>

using namespace dekaf2;

TEST_CASE("KReplacer")
{
	SECTION("No LeadIn/Out")
	{
		KReplacer Replacer;
		Replacer.insert("{{TEST}}", "test");
		Replacer.insert("{{VARIABLE}}", "variable");
		Replacer.insert("{{SHORT}}", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{TEST}} with a {{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );
	}

	SECTION("No LeadIn/Out tricky")
	{
		KReplacer Replacer;
		Replacer.insert("{{TEST}}", "test");
		Replacer.insert("{{VARIABLE}}", "variable");
		Replacer.insert("{{SHORT}}", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{{TEST}} with a {{{{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a {test with a {{verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a {test with a {{verylongvaluevariable" );
	}

	SECTION("LeadIn/Out")
	{
		KReplacer Replacer("{{", "}}");
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{TEST}} with a {{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );
	}

	SECTION("LeadIn/no LeadOut")
	{
		KReplacer Replacer("__", "");
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This __IS a __TEST with a __SHORT__VARIABLE";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This __IS a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This __IS a test with a verylongvaluevariable" );
	}

	SECTION("LeadIn/no LeadOut, longer matches")
	{
		KReplacer Replacer("__", "");
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT__VARIABLE", "short value");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 4 );

		KString sIn = "This __IS a __TEST with a __SHORT__VARIABLE";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This __IS a test with a short value" );
	}

SECTION("LeadIn/no LeadOut, delete all")
	{
		// Delete is only valid when there is both a lead in/out.
		// This test checks if the delete is ignored if not.
		KReplacer Replacer("__", "", true);
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This __IS a __TEST with a __SHORT__VARIABLE";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This __IS a test with a verylongvaluevariable" );
	}

	SECTION("no LeadIn/Lead Out")
	{
		KReplacer Replacer("", "__");
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This IS__ a TEST__ with a SHORT__VARIABLE__";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This IS__ a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This IS__ a test with a verylongvaluevariable" );
	}

	SECTION("no LeadIn/Lead Out, longer matches")
	{
		KReplacer Replacer("", "__");
		Replacer.insert("TEST", "test");
		Replacer.insert("RIABLE", "riable");
		Replacer.insert("XVARIABLE", "xvariable");
		Replacer.insert("VARIABLE", "variable");
		CHECK ( Replacer.size() == 4 );

		KString sIn = "This IS__ a TEST__ with a VARIABLE__ XVARIABLE__";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This IS__ a test with a variable xvariable" );
	}

	SECTION("LeadIn/Out missing closer")
	{
		KReplacer Replacer("{{", "}}");
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{TEST.. with a {{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a {{TEST.. with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a {{TEST.. with a verylongvaluevariable" );
	}

	SECTION("Tricky")
	{
		KReplacer Replacer("{{", "}}");
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{SHORT {{{TEST}} with a {{{{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a {{SHORT {test with a {{verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a {{SHORT {test with a {{verylongvaluevariable" );
	}

	SECTION("LeadIn/Out delete all")
	{
		KReplacer Replacer("{{", "}}", true);
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{TEST}} with a {{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This  a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This  a test with a verylongvaluevariable" );
	}

	SECTION("Tricky delete all")
	{
		KReplacer Replacer("{{", "}}", true);
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{{TEST}} with a {{{{SHORT}}{{VARIABLE}}{{TEST abcde";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This  a  with a variable{{TEST abcde" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This  a  with a variable{{TEST abcde" );
	}

	SECTION("Load from KReplacer")
	{
		KReplacer Map("{{", "}}", true);
		Map.insert("TEST", "test");
		Map.insert("VARIABLE", "variable");
		Map.insert("SHORT", "verylongvalue");

		KReplacer Replacer("{{", "}}");
		Replacer.insert(Map);
		CHECK ( Replacer.size() == 3 );
		// test a repetitive insertion (should have no effect)
		Replacer.insert(Map);
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{TEST}} with a {{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );
	}

	SECTION("Load from KProps")
	{
		KProps<KString, KString> Map;
		Map.Add("TEST", "test");
		Map.Add("VARIABLE", "variable");
		Map.Add("SHORT", "verylongvalue");
		
		KReplacer Replacer("{{", "}}");
		Replacer.insert(Map);
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{TEST}} with a {{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );
	}

	SECTION("KVariables")
	{
		KVariables Replacer;
		Replacer.insert("TEST", "test");
		Replacer.insert("VARIABLE", "variable");
		Replacer.insert("SHORT", "verylongvalue");
		CHECK ( Replacer.size() == 3 );

		KString sIn = "This {{IS}} a {{TEST}} with a {{SHORT}}{{VARIABLE}}";

		KString sOut = Replacer.Replace(sIn);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );

		sOut.clear();

		sOut = sIn;
		Replacer.ReplaceInPlace(sOut);
		CHECK ( sOut == "This {{IS}} a test with a verylongvaluevariable" );
	}

}
