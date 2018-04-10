#include "catch.hpp"

#include <dekaf2/khtmlparser.h>
#include <dekaf2/kstringstream.h>
#include <vector>

using namespace dekaf2;

TEST_CASE("KHTMLParser")
{
	KStringView sHTML;
	sHTML = (R"(
	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
	"http://www.w3.org/TR/html4/strict.dtd">
	<html>
	<head><!-- with a comment here! >> -> until here -->
	<title>A study of population dynamics</title>
	</head>
	<body>
			 <!----- another comment here until here -->
			 <img checked href="http://www.xyz.com/my/image.png" title=Ñice/>
			 <p>And finally <i>some</i> content</p>
	</body>
	</html>
	)");

	SECTION("basic parsing")
	{
		KHTMLParser HTML;
		bool ret = HTML.Parse(sHTML);
		CHECK ( ret == true );
	}

	SECTION("rebuilding")
	{
		class KHTMLSerializer : public KHTMLParser
		{
		public:

			KHTMLSerializer(KOutStream& OutStream)
			: m_OutStream(OutStream)
			{}

		protected:

			virtual void Tag(KHTMLTag& Tag) override
			{
				Tag.Serialize(m_OutStream);
			}

			virtual void Content(char ch) override
			{
				m_OutStream.Write(ch);
			}

			virtual void Comment(char ch) override
			{
				m_OutStream.Write(ch);
			}

			virtual void ProcessingInstruction(char ch) override
			{
				m_OutStream.Write(ch);
			}

			virtual void Output(OutputType Type) override
			{
				switch (Type)
				{
					case COMMENT:
						m_OutStream.Write("<!--");
						break;

					case PROCESSINGINSTRUCTION:
						m_OutStream.Write("<!");
						break;

					default:
						if (m_Output == COMMENT)
						{
							m_OutStream.Write("-->");
						}
						else if (m_Output == PROCESSINGINSTRUCTION)
						{
							m_OutStream.Write(">");
						}
						break;
				}
				m_Output = Type;
			}

		private:

			KOutStream& m_OutStream;
			OutputType m_Output { NONE };

		};

		KString sOutput;
		{
			KOutStringStream oss(sOutput);
			KHTMLSerializer HTMLSerializer(oss);
			HTMLSerializer.Parse(sHTML);
		}
		CHECK ( sHTML == sOutput );

	}

}
