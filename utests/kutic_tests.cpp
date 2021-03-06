#include "catch.hpp"

#include <dekaf2/kutic.h>
#include <dekaf2/kstringstream.h>
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
		HTMLSerializer.AddUTIC({ false, "", "/body/p/", "/self/", "/fancy/" });
		HTMLSerializer.Parse(sHTML);

		CHECK ( sHTML == sOutput );
		CHECK ( HTMLSerializer.m_Blocks.size() == 1 );
		if (HTMLSerializer.m_Blocks.size() == 1)
		{
			CHECK ( HTMLSerializer.m_Blocks[0] == "A study of population dynamics" );
		}
	}

}
