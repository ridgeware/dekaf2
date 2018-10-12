#include "catch.hpp"

#include <dekaf2/kxml.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KXML")
{
	SECTION("Basic construction")
	{
		KXML xmldoc;
		xmldoc.AddXMLDeclaration();
		KXMLNode root = xmldoc;
		root = root.AddNode("myroot");
		auto file = root.AddNode("file");
		file.AddAttribute("id", "f1");
		file.AddAttribute("size", "1234");
		file.AddAttribute("mime", "text/html");
		auto group = file.AddNode("group");
		group.AddAttribute("id", "g1");
		auto unit1 = group.AddNode("unit");
		auto segment1 = unit1.AddNode("segment");
		segment1.AddAttribute("id", "s1");
		segment1.SetValue("some contents");
		unit1.AddAttribute("id", "u1");
		auto unit2 = group.AddNode("unit");
		unit2.AddAttribute("id", "u2");
		group.AddNode("unit").AddAttribute("id", "u3").AddAttribute("empty", "true");

		auto sOut = xmldoc.Serialize();

static constexpr KStringView sExpected (
R"(<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<myroot>
	<file id="f1" size="1234" mime="text/html">
		<group id="g1">
			<unit id="u1">
				<segment id="s1">some contents</segment>
			</unit>
			<unit id="u2"/>
			<unit id="u3" empty="true"/>
		</group>
	</file>
</myroot>

)");

		CHECK ( sOut == sExpected );

		KXML xml2(sOut);

		root = xml2;
		root = root.Child("myroot");
		segment1 = root.Child("file").Child("group").Child("unit").Child("segment");
		auto sv = segment1.GetValue();
		CHECK ( sv == "some contents" );

		xml2.AddXMLDeclaration();
		sOut = xml2.Serialize();

		CHECK ( sOut == sExpected );
	}

}
