#include "catch.hpp"

#include <dekaf2/web/html/khtmldom.h>
#include <dekaf2/web/html/bits/khtmldom_node.h>
#include <dekaf2/io/streams/kstringstream.h>
#include <dekaf2/io/readwrite/kwriter.h>
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

	// ------------------------------------------------------------------
	// KHTML mirrors every parsed node into an arena-backed POD
	// shadow tree. The tree is read-only state until later changes switch
	// the heap DOM to use it as ground truth.
	// ------------------------------------------------------------------
	SECTION("POD shadow tree mirrors a simple parse")
	{
		KHTML HTML;
		HTML.Parse(KStringView{"<html><body><p>hi</p><img src=\"x.png\"/></body></html>"});

		const auto* root = HTML.PodRoot();
		REQUIRE ( root != nullptr );
		CHECK ( root->Kind == khtml::NodeKind::Element );

		// root has one direct child: <html>
		REQUIRE ( root->FirstChild != nullptr );
		const auto* html = root->FirstChild;
		CHECK ( html->Name == "html" );
		CHECK ( html->NextSibling == nullptr );

		// <html> has one child: <body>
		REQUIRE ( html->FirstChild != nullptr );
		const auto* body = html->FirstChild;
		CHECK ( body->Name == "body" );
		CHECK ( body->Parent == html );

		// <body> has two element children: <p> and <img/>
		REQUIRE ( body->FirstChild != nullptr );
		const auto* p = body->FirstChild;
		CHECK ( p->Name == "p" );

		REQUIRE ( p->NextSibling != nullptr );
		const auto* img = p->NextSibling;
		CHECK ( img->Name == "img" );
		CHECK ( body->LastChild == img );

		// <img src="x.png"/>: one attribute, value mirrored into arena
		REQUIRE ( img->FirstAttr != nullptr );
		CHECK ( img->FirstAttr->Name  == "src" );
		CHECK ( img->FirstAttr->Value == "x.png" );
		CHECK ( img->FirstAttr->Next  == nullptr );

		// <p> contains a Text node "hi"
		REQUIRE ( p->FirstChild != nullptr );
		CHECK ( p->FirstChild->Kind == khtml::NodeKind::Text );
		CHECK ( p->FirstChild->Name == "hi" );
	}

	SECTION("POD shadow tree caches TagProperty")
	{
		KHTML HTML;
		HTML.Parse(KStringView{"<div><span>x</span></div>"});

		const auto* root = HTML.PodRoot();
		REQUIRE ( root != nullptr );

		const auto* div = root->FirstChild;
		REQUIRE ( div != nullptr );
		CHECK ( div->Name == "div" );
		CHECK ( (div->TagProps & KHTMLObject::TagProperty::Block) == KHTMLObject::TagProperty::Block );

		const auto* span = div->FirstChild;
		REQUIRE ( span != nullptr );
		CHECK ( span->Name == "span" );
		CHECK ( (span->TagProps & KHTMLObject::TagProperty::Inline) == KHTMLObject::TagProperty::Inline );
	}

	SECTION("POD shadow tree mirrors comments, doctype, PI and CDATA")
	{
		KHTML HTML;
		HTML.Parse(KStringView{
			"<?xml version=\"1.0\"?>"
			"<!DOCTYPE html>"
			"<root>"
			"<!-- a comment -->"
			"<![CDATA[some <stuff>]]>"
			"</root>"
		});

		const auto* root = HTML.PodRoot();
		REQUIRE ( root != nullptr );

		// flatten the top-level chain so we can spot-check the kinds
		const khtml::NodePOD* nodes[8] = { nullptr };
		std::size_t i = 0;
		for (auto* n = root->FirstChild; n != nullptr && i < 8; n = n->NextSibling, ++i)
		{
			nodes[i] = n;
		}

		REQUIRE ( i >= 3 );
		CHECK ( nodes[0]->Kind == khtml::NodeKind::ProcessingInstruction );
		CHECK ( nodes[1]->Kind == khtml::NodeKind::DocumentType );
		CHECK ( nodes[2]->Kind == khtml::NodeKind::Element );
		CHECK ( nodes[2]->Name == "root" );

		const auto* comment = nodes[2]->FirstChild;
		REQUIRE ( comment != nullptr );
		CHECK ( comment->Kind == khtml::NodeKind::Comment );

		REQUIRE ( comment->NextSibling != nullptr );
		CHECK ( comment->NextSibling->Kind == khtml::NodeKind::CData );
	}

	SECTION("clear() resets POD tree and frees arena")
	{
		KHTML HTML;
		HTML.Parse(KStringView{"<div><p>one</p><p>two</p></div>"});

		REQUIRE ( HTML.PodRoot() != nullptr );
		REQUIRE ( HTML.PodRoot()->FirstChild != nullptr );
		CHECK ( HTML.Arena().UsedBytes() > 0 );

		HTML.clear();

		// after clear: a fresh empty root, arena reset
		REQUIRE ( HTML.PodRoot() != nullptr );
		CHECK ( HTML.PodRoot()->FirstChild == nullptr );
		CHECK ( HTML.PodRoot()->LastChild  == nullptr );

		// re-parse must work
		HTML.Parse(KStringView{"<a/>"});
		REQUIRE ( HTML.PodRoot()->FirstChild != nullptr );
		CHECK ( HTML.PodRoot()->FirstChild->Name == "a" );
	}
}
