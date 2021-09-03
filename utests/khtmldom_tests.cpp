#include "catch.hpp"

#include <dekaf2/khtmldom.h>
#include <dekaf2/kstringstream.h>
#include <dekaf2/kwriter.h>
#include <vector>

using namespace dekaf2;

KString print_diff(KStringView s1, KStringView s2)
{
	KString sOut;
	auto iSize = std::min(s1.size(), s2.size());
	std::size_t iPos = 0;
	bool bFound = false;
	for (;iPos < iSize; ++iPos)
	{
		if (s1[iPos] != s2[iPos])
		{
			bFound = true;
			break;
		}
	}
	if (bFound)
	{
		sOut += kFormat("differs at pos {}: ", iPos);
		if (iPos > 3 )
		{
			iPos -= 3;
		}
		else
		{
			iPos = 0;
		}
		sOut += s1.Mid(iPos, 20);
		sOut += " != ";
		sOut += s2.Mid(iPos, 20);
	}
	return sOut;
}

TEST_CASE("KHTML")
{
	constexpr KStringView sHTML = (R"(
	<?xml-stylesheet type="text/xsl" href="style.xsl"?>
	<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
	<html>
	<head><!-- with a comment here! >> -> until here --><!--- just one more --->
	<title>A study of population dynamics</title>
	</head>
	<body>
		<table>
			 <tr><td align="center" nowrap>hi</td></tr>
		</table>
			 <!----- another comment here until here ---> <!--really?>-->
		<p>
			<script type="lang/nicely"> this is <a <new <a href="www.w3c.org">scripting</a> language> </script>
			<img checked src="http://www.xyz.com/my/image.png" title=Ñicé /><br />
		</p>
		<p class='fancy' id=self style="curly">And <i class='shallow'>some</i> content</p>
	</body>
	</html>
	)");

	constexpr KStringView sSerialized1 = (R"(<?xml-stylesheet type="text/xsl" href="style.xsl"?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
	<head>
		<!-- with a comment here! >> -> until here -->
		<!--- just one more --->
		<title>
			A study of population dynamics
		</title>
	</head>
	<body>
		<table>
			<tr>
				<td align="center" nowrap>
					hi
				</td>
			</tr>
		</table>
		<!----- another comment here until here --->
		<!--really?>-->
		<p>
			<script type="lang/nicely"> this is <a <new <a href="www.w3c.org">scripting</a> language> </script><img checked src="http://www.xyz.com/my/image.png" title=Ñicé/><br/>
		</p>
		<p class='fancy' id=self style="curly">
			And <i class='shallow'>some</i> content
		</p>
	</body>
</html>
)");

	constexpr KStringView sSerialized2 = (R"(<html class="main">
	<head>
		<title>
			This is the title
		</title>
		<meta viewport="width=device-width, initial-scale=1.0"/>
	</head>
	<body>
		<p id="MyPar">
			This &lt;is&gt; &amp;a <i>web</i>page<br/>
			More text
		</p>
		<p class="emptyClass" id="emptyPar"></p>
		<h2>
			This is the title
		</h2>
		<a href="/some/link"><img src="/another/link/img.png"/></a>that is all
	</body>
</html>
)");

	SECTION("parsing and rebuilding into string")
	{
		KString sCRLF = sSerialized1;
		sCRLF.Replace("\n", "\r\n");

		{
			KHTML HTML;
			bool ret = HTML.Parse(sHTML);
			CHECK ( ret == true );
			CHECK ( HTML.Serialize() == sCRLF );

			auto sDiff = print_diff(HTML.Serialize(), sCRLF);
			if (!sDiff.empty())
			{
				FAIL_CHECK ( sDiff );
			}
		}
		{
			KInStringStream iss(sHTML);
			KHTML HTML;
			iss >> HTML;
			CHECK ( HTML.Serialize() == sCRLF );
		}
		{
			KInStringStream iss(sHTML);
			KHTML HTML;
			HTML << iss;
			CHECK ( HTML.Serialize() == sCRLF );
		}
	}

	SECTION("building")
	{
		{
			KHTML Page;
			auto& root = Page.DOM();
			KHTMLElement html("html");
			KHTMLElement hh;
			auto& hhh = root.Add(std::move(html));
			auto& body = hhh.Add(KHTMLElement("body"));
		}
		{
			KHTML Page;
			auto& html = Page.Add("html");
			html.SetClass("main");
			auto& head = html.Add("head");
			auto& title = head.Add("title");
			title.AddText("This is the");
			head.Add("meta").SetAttribute("viewport", "width=device-width, initial-scale=1.0");
			auto& body = html += "body";
			auto& par  = body += "p";
			par.SetID("MyPar1");
			par.SetID("MyPar");
			par.AddText("This <is> &a ");
			auto& italic = par.Add("i");
			italic.AddText("web");
			par.AddText("page");
			par.Add("br");
			par.AddText("More text");
			title.AddText(" title");
			body.Add("p", "emptyPar", "emptyClass");
			body.Add("h2").AddText("This is the title");
			body.Add("a").SetAttribute("href", "/some/link").Add("img").SetAttribute("src", "/another/link/img.png");
			body.AddText("that is all");
			KString sCRLF = sSerialized2;
			sCRLF.Replace("\n", "\r\n");
			CHECK ( Page.Serialize() == sCRLF );

			auto& DOM = Page.DOM();
			CHECK ( DOM.Print() == sCRLF );

			auto DOM2 = DOM;
			CHECK ( DOM2.Print() == sCRLF );

			KString sStream;
			KOutStringStream oss(sStream);
			oss << DOM2;
			CHECK( sStream == sCRLF );
		}
	}
}
