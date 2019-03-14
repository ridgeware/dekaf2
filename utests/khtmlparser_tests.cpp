#include "catch.hpp"

#include <dekaf2/khtmlparser.h>
#include <dekaf2/kstringstream.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KHTMLParser")
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
			 <tr><td align="center" nowrap>hi</td></tr>
			 <!----- another comment here until here ---> <!--really?>-->
			 <script> this is <a <new <a href="www.w3c.org">scripting</a> language> </script>
			 <img checked href="http://www.xyz.com/my/image.png" title=Ñicé />
			 <p class='fancy' id=self style="curly">And finally <i class='shallow'>some</i> content</p>
	</body>
	</html>
	)");

	SECTION("basic parsing")
	{
		KHTMLParser HTML;
		bool ret = HTML.Parse(sHTML);
		CHECK ( ret == true );
	}

	SECTION("rebuilding into stream")
	{
		class KHTMLSerializer : public KHTMLParser
		{
		public:

			KHTMLSerializer(KOutStream& OutStream)
			: m_OutStream(OutStream)
			{}

		protected:

			virtual void Object(KHTMLObject& Object) override
			{
				Object.Serialize(m_OutStream);
			}

			virtual void Content(char ch) override
			{
				m_OutStream.Write(ch);
			}

			virtual void Script(char ch) override
			{
				m_OutStream.Write(ch);
			}

			virtual void Invalid(char ch) override
			{
				m_OutStream.Write(ch);
			}

		private:

			KOutStream& m_OutStream;

		};

		KString sOutput;
		{
			KOutStringStream oss(sOutput);
			KHTMLSerializer HTMLSerializer(oss);
			HTMLSerializer.Parse(sHTML);
		}
		CHECK ( sHTML == sOutput );

	}

	SECTION("rebuilding into string")
	{
		class KHTMLSerializer : public KHTMLParser
		{
		public:

			KHTMLSerializer(KString& OutString)
			: m_OutString(OutString)
			{}

		protected:

			virtual void Object(KHTMLObject& Object) override
			{
				Object.Serialize(m_OutString);
			}

			virtual void Content(char ch) override
			{
				m_OutString += ch;
			}

			virtual void Script(char ch) override
			{
				m_OutString += ch;
			}

			virtual void Invalid(char ch) override
			{
				m_OutString += ch;
			}

		private:

			KString& m_OutString;

		};

		KString sOutput;
		{
			KHTMLSerializer HTMLSerializer(sOutput);
			HTMLSerializer.Parse(sHTML);
		}
		CHECK ( sHTML == sOutput );

	}
	SECTION("attribute extraction")
	{
		class KHTMLScanner : public KHTMLParser
		{
		public:

			KHTMLScanner()
			{}

			bool Found() const { return m_bFound; }

		protected:

			virtual void Object(KHTMLObject& Object) override
			{
				if (Object.Type() == KHTMLObject::TAG)
				{
					KHTMLTag& tag = static_cast<KHTMLTag&>(Object);
					if (tag.Attributes.Get("class") == "shallow")
					{
						m_bFound = true;
					}
				}
			}

		private:

			bool m_bFound { false };

		};

		KHTMLScanner HTMLScanner;
		HTMLScanner.Parse(sHTML);
		CHECK ( HTMLScanner.Found() == true );
	}

}
