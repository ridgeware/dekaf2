#include "catch.hpp"

#include <dekaf2/khtmlcontentblocks.h>
#include <dekaf2/kstringstream.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KHTMLContentBlocks")
{
	KStringView sHTML;
	sHTML = (R"(
	<?xml-stylesheet type="text/xsl" href="style.xsl"?>
	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
	"http://www.w3.org/TR/html4/strict.dtd">
	<html>
	<head><!-- with a comment here! >> -> until here --><!--- just one more --->
	<title>A study of population dynamics</title>
	</head>
	<body>
	<!----- another comment here until here ---> <!--really?>-->
	<img checked href="http://www.xyz.com/my/image.png" title=Ñicé />
	<p class='fancy' id=self style="curly">And finally <i class='shallow'>some</i> content</p>
	</body>
	</html>
	)");

	SECTION("basic parsing")
	{
		KHTMLContentBlocks HTML;
		bool ret = HTML.Parse(sHTML);
		CHECK ( ret == true );
	}

	SECTION("rebuilding into stream")
	{
		class KHTMLSerializer : public KHTMLContentBlocks
		{
		public:

			KHTMLSerializer(KOutStream& OutStream)
			: m_OutStream(OutStream)
			{}

			std::vector<KString> m_Blocks;

		protected:

			virtual void ContentBlock(KStringView sContentBlock) override
			{
				m_Blocks.push_back(sContentBlock);
				m_OutStream.Write(sContentBlock);
			}

			virtual void Skeleton(char ch) override
			{
				m_OutStream.Write(ch);
			}

		private:

			KOutStream& m_OutStream;

		};

		KString sOutput;
		KOutStringStream oss(sOutput);
		KHTMLSerializer HTMLSerializer(oss);
		HTMLSerializer.Parse(sHTML);

		CHECK ( sHTML == sOutput );
		CHECK ( HTMLSerializer.m_Blocks.size() == 2 );
		CHECK ( HTMLSerializer.m_Blocks[0] == "A study of population dynamics" );
		CHECK ( HTMLSerializer.m_Blocks[1] == "And finally <i class='shallow'>some</i> content" );

	}

}
