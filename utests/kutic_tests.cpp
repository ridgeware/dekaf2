#include "catch.hpp"

#include <dekaf2/kutic.h>
#include <dekaf2/kstringstream.h>
#include <dekaf2/kjson.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KUTIC")
{
	KStringView sHTML = (R"(
	<?xml-stylesheet type="text/xsl" href="style.xsl"?>
	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
	"http://www.w3.org/TR/html4/strict.dtd">
	<html>
	<head><!-- with a comment here! >> -> until here --><!--- just one more --->
	<title>A study of population dynamics</title>
	</head>
	<body>
	<!----- another comment here until here ---> <!--really?>-->
	<script> this is <a <new <a href="www.w3c.org">scripting</a> language> </script>
	<img checked href="http://www.xyz.com/my/image.png" title=Ñicé />
	<p class='fancy' id=self style="curly">And finally <i class='shallow'>some</i> content</p>
	</body>
	</html>
	)");

	SECTION("basic parsing")
	{
		KHTMLUTICParser HTML;
		bool ret = HTML.Parse(sHTML);
		CHECK ( ret == true );
	}

	SECTION("rebuilding into stream")
	{
		class KHTMLSerializer : public KHTMLUTICParser
		{
		public:

			KHTMLSerializer(KOutStream& OutStream)
			: m_OutStream(OutStream)
			{}

			std::vector<KString> m_Blocks;

		protected:

			virtual void ContentBlock(KHTMLContentBlocks::BlockContent& Block) override
			{
				KString sContent;
				Block.Serialize(sContent);

				m_OutStream.Write(sContent);

				if (MatchesUTICs())
				{
					m_Blocks.push_back(std::move(sContent));
				}
			}

			virtual void Skeleton(char ch) override
			{
				m_OutStream.Write(ch);
			}

			virtual void Skeleton(KStringView sSkeleton) override
			{
				m_OutStream.Write(sSkeleton);
			}

		private:

			KOutStream& m_OutStream;

		};

		KString sOutput;
		KOutStringStream oss(sOutput);
		KHTMLSerializer HTMLSerializer(oss);
		HTMLSerializer.AddUTIC({ "", "[ /]body[/ ][p|br]/", "/s[e|u|o]lf/", "/fancy/", "", false });
		HTMLSerializer.Parse(sHTML);

		CHECK ( sHTML == sOutput );
		CHECK ( HTMLSerializer.m_Blocks.size() == 1 );
		if (HTMLSerializer.m_Blocks.size() == 1)
		{
			CHECK ( HTMLSerializer.m_Blocks[0] == "A study of population dynamics" );
		}
	}

	SECTION("json")
	{
		auto jUTICArray = kjson::Parse(R"(
		[
			{
				"U" : "www.test.org",
				"T" : "p",
				"I" : "id",
				"C" : "base",
				"X" : "div.header",
				"key" : "any data",
				"array" : [
					"one", "two", "three"
				],
				"object" : {
					"key1" : "value1",
					"key2" : "value2"
				}
			},
			{
				"U" : "",
				"T" : "",
				"I" : "",
				"C" : "",
				"X" : ""
			},
			{
				"U" : "U",
				"T" : "T",
				"I" : "I",
				"C" : "C",
				"X" : "X"
			},
			{
				"U" : "U",
				"T" : "T",
				"I" : "I",
				"C" : "C",
				"X" : "some selector"
			},
			{
				"some" : "thing"
			},
			{
			}
		]
		)");

		auto Rules = KUTIC::Load(jUTICArray);

		CHECK ( Rules.size() == 1 );

		if (Rules.size() == 1)
		{
			KUTIC& Rule = Rules[0];
			CHECK ( Rule.URL()              == "www.test.org" );
			CHECK ( Rule.CSSSelector()      == "div.header"   );
			CHECK ( Rule.Tags()   .Regex()  == "p"    );
			CHECK ( Rule.IDs()    .Regex()  == "id"   );
			CHECK ( Rule.Classes().Regex()  == "base" );
			CHECK ( Rule.Payload().dump(-1) == R"({"array":["one","two","three"],"key":"any data","object":{"key1":"value1","key2":"value2"}})" );
			CHECK ( Rule.PositiveMatch()    == true   );
			CHECK ( Rule.empty()            == false  );
		}

		auto jOneUTIC = kjson::Parse(R"(
		{
			"U" : "www.test.org",
			"T" : "p",
			"I" : "id",
			"C" : "base",
			"X" : "div.header",
			"key" : "any data",
			"array" : [
				"one", "two", "three"
			],
			"object" : {
				"key1" : "value1",
				"key2" : "value2"
			}
		}
		)");

		Rules = KUTIC::Load(jOneUTIC);
		CHECK ( Rules.empty() );

		KUTIC Wrong(jUTICArray);
		CHECK ( Wrong.empty()          == true );
		
		KUTIC Rule(jOneUTIC);

		CHECK ( Rule.URL()             == "www.test.org" );
		CHECK ( Rule.CSSSelector()     == "div.header"   );
		CHECK ( Rule.Tags()   .Regex() == "p"    );
		CHECK ( Rule.IDs()    .Regex() == "id"   );
		CHECK ( Rule.Classes().Regex() == "base" );
		CHECK ( Rule.Payload().dump(-1) == R"({"array":["one","two","three"],"key":"any data","object":{"key1":"value1","key2":"value2"}})" );
		CHECK ( Rule.PositiveMatch()   == true   );
		CHECK ( Rule.empty() == false );

		KParsedUTIC Stack1("www.test.org/page",
						   KParsedTICElement("/div/p/a/"),
						   KParsedTICElement("/id///"),
						   KParsedTICElement("/base///"),
						   3);

		CHECK ( Stack1 == Rule  );

		Rules = KUTIC::Load(jUTICArray);

		CHECK ( Stack1 == Rules );

		KParsedUTIC Stack2("www.test.org/page");
		Stack2.Add("div", "id", "base");
		Stack2.Add("t", "none", "all");
		Stack2.Reduce();
		Stack2.Add("p", "", "");
		Stack2.Add("a", "", "");


		CHECK ( Stack2 == Stack1 );
		CHECK ( Stack2 == Rule   );
		CHECK ( Stack2 == Rules  );
	}

}
