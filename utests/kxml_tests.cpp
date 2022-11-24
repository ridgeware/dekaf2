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
		/* auto segment2 = */ unit1.AddNode("segment", "there is a lot of value in this node").AddAttribute("id", "s2");
		auto segment3 = unit1.AddNode("segment", "another ");
		segment3.AddNode("ph", "short");
		segment3.AddValue(" segment");
		segment3.AddAttribute("id", "s3");
		segment3.SetInlineRoot();
		unit1.AddAttribute("id", "u1");
		auto unit2 = group.AddNode("unit");
		unit2.AddAttribute("id", "u2");
		group.AddNode("unit").AddAttribute("id", "u3").AddAttribute("empty", "true");

		auto sOut = xmldoc.Serialize();

static constexpr KStringView sParse (
R"(<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<myroot>
	<file id="f1" size="1234" mime="text/html">
		<group id="g1">
			<unit id="u1">
				<segment id="s1">some contents</segment>
				<segment id="s2">there is a lot of value in this node</segment>
				<segment id="s3">another <ph>short</ph> segment</segment>
			</unit>
			<unit id="u2"/>
			<unit id="u3" empty="true"/>
		</group>
	</file>
</myroot>
)");

static constexpr KStringView sExpected (
R"(<?xml version="1.0" encoding="UTF-8"?>
<myroot>
	<file id="f1" size="1234" mime="text/html">
		<group id="g1">
			<unit id="u1">
				<segment id="s1">some contents</segment>
				<segment id="s2">there is a lot of value in this node</segment>
				<segment id="s3">another <ph>short</ph> segment</segment>
			</unit>
			<unit id="u2"/>
			<unit id="u3" empty="true"/>
		</group>
	</file>
</myroot>
)");

static constexpr KStringView sExpectedNoIndent (
R"(<?xml version="1.0" encoding="UTF-8"?>
<myroot>
<file id="f1" size="1234" mime="text/html">
<group id="g1">
<unit id="u1">
<segment id="s1">some contents</segment>
<segment id="s2">there is a lot of value in this node</segment>
<segment id="s3">another <ph>short</ph> segment</segment>
</unit>
<unit id="u2"/>
<unit id="u3" empty="true"/>
</group>
</file>
</myroot>
)");
		CHECK ( sOut == sParse );

		KXML xml2(sParse);

		CHECK ( xml2.HadXMLDeclaration() == true );
		auto Decl = xml2.GetXMLDeclaration();

		CHECK ( Decl.Attribute("version").GetValue() == "1.0" );
		CHECK ( Decl.Attribute("encoding").GetValue() == "utf-8" );
		CHECK ( Decl.Attribute("standalone").GetValue() == "yes" );

		root = xml2;
		root = root.Child("myroot");
		segment1 = root.Child("file").Child("group").Child("unit").Child("segment");
		auto sv = segment1.GetValue();
		CHECK ( sv == "some contents" );
		segment3 = root.Child("file").Child("group").Child("unit").Child("segment").Next("segment").Next("segment");
		segment3.SetInlineRoot();
		auto ph = segment3.Child("ph");
		auto sv2 = ph.GetValue();
		CHECK ( sv2 == "short" );
		xml2.AddXMLDeclaration("1.0", "UTF-8", "");
		sOut = xml2.Serialize();
		CHECK ( sOut == sExpected );
		sOut = xml2.Serialize(KXML::NoIndents);
		CHECK ( sOut == sExpectedNoIndent );
	}

	SECTION("Serialize")
	{
		static constexpr KStringViewZ sSource = (R"(<sc id="1"/>This<ec startRef="1"/> is the first sentence. Here comes the <sc id="2"/>second<ec startRef="2"/> sentence. <ph id="3"/>And one more!)");
		KXML Source(sSource, true, "source");
		CHECK ( Source.Serialize(KXML::Terse) == (R"(<source><sc id="1"/>This<ec startRef="1"/> is the first sentence. Here comes the <sc id="2"/>second<ec startRef="2"/> sentence. <ph id="3"/>And one more!</source>)") );
		CHECK ( Source.Serialize(KXML::Terse, "source") == (R"(<sc id="1"/>This<ec startRef="1"/> is the first sentence. Here comes the <sc id="2"/>second<ec startRef="2"/> sentence. <ph id="3"/>And one more!)") );
	}

	SECTION("text")
	{
		static constexpr KStringView sParse (
R"(<myroot attr1="&amp;value1">Text directly in root
 <element>&lt;nothing&gt;</element>
 Text after children
</myroot>
)");
		KXML DOM(sParse);
		auto Element = DOM.begin();
		CHECK ( Element.Attribute("attr1").GetValue() == "&value1" );
		CHECK ( Element.GetName() == "myroot" );
		CHECK ( Element.GetValue() == "Text directly in root\n " );
		Element = Element.Child();
		CHECK ( Element.GetValue() == "Text directly in root\n " );
		Element = Element.Next();
		CHECK ( Element.GetName() == "element" );
		CHECK ( Element.GetValue() == "<nothing>" );
		Element = Element.Next();
		CHECK ( Element.GetValue() == "\n Text after children\n" );
	}
}
