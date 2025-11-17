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
		sOut += kFormat("differs at pos {}:\n'", iPos);
		if (iPos > 3 )
		{
			iPos -= 3;
		}
		else
		{
			iPos = 0;
		}
		sOut += s1.Mid(iPos, 20);
		sOut += "' != '";
		sOut += s2.Mid(iPos, 20);
		sOut += "'";
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
			<script type="lang/nicely"> this is <a <new <a href="www.w3c.org">scripting</a> language> </script><img checked src="http://www.xyz.com/my/image.png" title=Ñicé><br>
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
		<meta viewport="width=device-width, initial-scale=1.0">
	</head>
	<body>
		<p id="MyPar">
			This &lt;is&gt; &amp;a <i>web</i>page<br>
			More text
		</p>
		<p class="emptyClass" id="emptyPar"></p>
		<h2>
			This is the title
		</h2>
		<a href="/some/link"><img src="/another/link/img.png"></a>that is all
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

	SECTION("only text")
	{
		static constexpr KStringView sSample { "This is free standing text" };
		KHTML HTML;
		HTML.Parse(sSample);
		CHECK ( HTML.Serialize() == sSample );
	}

	SECTION("text with inlines")
	{
		static constexpr KStringView sSample { "This is <b>free <i>standing</i></b> text" };
		KHTML HTML;
		HTML.Parse(sSample);
		CHECK ( HTML.Serialize() == sSample );
	}

	SECTION("preformatted text")
	{
		static constexpr KStringView sSample   { "<pre>This is  \r\n  preformatted    text</pre>" };
		static constexpr KStringView sExpected { "<pre>\r\nThis is  \r\n  preformatted    text</pre>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		CHECK ( HTML.Serialize() == sExpected );
	}

	SECTION("preformatted text in outer block")
	{
		static constexpr KStringView sSample   { "<div><div><pre>This is  \r\n  preformatted    text</pre></div></div>" };
		static constexpr KStringView sExpected { "<div>\r\n\t<div>\r\n\t\t<pre>\r\nThis is  \r\n  preformatted    text</pre>\r\n\t</div>\r\n</div>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		CHECK ( HTML.Serialize() == sExpected );
	}

	SECTION("preformatted text with inlines")
	{
		static constexpr KStringView sSample   { "<pre>This is  \r\n  <b>preformatted</b>    text</pre>" };
		static constexpr KStringView sExpected { "<pre>\r\nThis is  \r\n  <b>preformatted</b>    text</pre>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		CHECK ( HTML.Serialize() == sExpected );
	}

	SECTION("block framed")
	{
		static constexpr KStringView sSample   { "<p>This is    <b>block <i>framed</i></b>    text</p>" };
		static constexpr KStringView sExpected { "<p>\r\n\tThis is <b>block <i>framed</i></b> text\r\n</p>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		auto sOut = HTML.Serialize();
		auto sDiff = print_diff(sOut, sExpected);
		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}
		CHECK ( sOut == sExpected );
	}

	SECTION("auto close 1")
	{
		static constexpr KStringView sSample   { "<p>This is <b>unbalanced <i>framed</b> text</p>" };
		static constexpr KStringView sExpected { "<p>\r\n\tThis is <b>unbalanced <i>framed</i></b> text\r\n</p>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		auto sOut = HTML.Serialize();
		auto sDiff = print_diff(sOut, sExpected);
		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}
		CHECK ( sOut == sExpected );
	}

	SECTION("auto close 2")
	{
		static constexpr KStringView sSample   { "<p>This is <b>unbalanced <i>framed text</p>" };
		static constexpr KStringView sExpected { "<p>\r\n\tThis is <b>unbalanced <i>framed text</i></b>\r\n</p>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		auto sOut = HTML.Serialize();
		auto sDiff = print_diff(sOut, sExpected);
		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}
		CHECK ( sOut == sExpected );
	}

	SECTION("auto close 3")
	{
		static constexpr KStringView sSample   { "<p>This is <b>unbalanced <i>framed</u> text</p>" };
		static constexpr KStringView sExpected { "<p>\r\n\tThis is <b>unbalanced <i>framed text</i></b>\r\n</p>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		auto sOut = HTML.Serialize();
		auto sDiff = print_diff(sOut, sExpected);
		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}
		CHECK ( sOut == sExpected );
	}

	SECTION("auto close 4")
	{
		static constexpr KStringView sSample   { "<p>This is <B>unbalanced <i>framed</b> text</B></p>" };
		static constexpr KStringView sExpected { "<p>\r\n\tThis is <b>unbalanced <i>framed</i></b> text\r\n</p>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		auto sOut = HTML.Serialize();
		auto sDiff = print_diff(sOut, sExpected);
		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}
		CHECK ( sOut == sExpected );
	}

	SECTION("encoding")
	{
		static constexpr KStringView sSample   { "<p>This is <b>bl&ouml;ck <i>framed</i></b> text</p>" };
		static constexpr KStringView sExpected { "<p>\r\n\tThis is <b>blöck <i>framed</i></b> text\r\n</p>\r\n" };
		KHTML HTML;
		HTML.Parse(sSample);
		auto sOut = HTML.Serialize();
		auto sDiff = print_diff(sOut, sExpected);
		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}
		CHECK ( sOut == sExpected );
	}

	SECTION("ambiguous")
	{
		static constexpr KStringView sSample {
R"(
<html>
	<head/>
	<body>
		</p>
		<div attr="that"/><div/>
		<img attr="this"></img attr="those">
		<p>
			<img attr=1><img attr=2/>
		</p>
		<p> <b> Bold</b> <i>italic</i></p>
		</img attr=3><img attr=4>
)"
		};
		static constexpr KStringView sExpected {
R"(<html>
	<head></head>
	<body>
		<div attr="that"></div>
		<div></div>
		<img attr="this">
		<p>
			<img attr=1><img attr=2>
		</p>
		<p>
			<b>Bold</b> <i>italic</i>
		</p>
		<img attr=3><img attr=4>
	</body>
</html>
)"
		};

		KHTML HTML;
		HTML.Parse(sSample);
		auto sOut = HTML.Serialize();
		KString sCRLF = sExpected;
		sCRLF.Replace("\n", "\r\n");
		auto sDiff = print_diff(sOut, sCRLF);
		if (!sDiff.empty())
		{
			FAIL_CHECK ( sDiff );
		}
		CHECK ( sOut == sCRLF );
	}

	SECTION("TextMerge")
	{
		KHTMLElement Element("div");
		Element.Add(KHTMLElement("img"));
		Element.AddText("text1.");
		Element.AddText("text2.");
		Element.Insert(Element.begin() + 1, KHTMLText("before1."));
		Element.Insert(Element.end(), KHTMLText("after1."));
		CHECK (Element.size() == 2);
		if (Element.size() == 2)
		{
			Element.Delete(Element.begin());
			CHECK (Element.size() == 1);
			CHECK (Element.begin()->get()->Type() == KHTMLText::TYPE);
			if (Element.begin()->get()->Type() == KHTMLText::TYPE)
			{
				auto Text = static_cast<KHTMLText*>(Element.begin()->get());
				CHECK (Text->GetText() == "before1.text1.text2.after1.");
			}
		}
	}
}
