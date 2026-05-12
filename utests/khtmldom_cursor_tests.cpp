#include "catch.hpp"

#include <dekaf2/web/html/khtmldom.h>
#include <dekaf2/web/html/khtmldom_cursor.h>

#include <vector>

using namespace dekaf2;

TEST_CASE("KHTML POD cursor")
{
	// ------------------------------------------------------------------
	// Basic walk: every cursor obtained from Root() must reflect the
	// arena-backed POD tree directly, with no heap-DOM materialization.
	// ------------------------------------------------------------------
	SECTION("Root() returns a synthetic root with the parsed children")
	{
		KHTML doc;
		doc.Parse("<html><body><p>hi</p></body></html>");

		auto root = doc.Root();
		REQUIRE ( root );
		CHECK   ( root.IsElement() );

		// the root itself has no name (it's the synthetic container)
		CHECK   ( root.Name().empty() );

		auto html = root.FirstChild();
		REQUIRE ( html );
		CHECK   ( html.IsElement() );
		CHECK   ( html.Name() == "html" );

		auto body = html.FirstChild();
		REQUIRE ( body );
		CHECK   ( body.Name() == "body" );

		auto p = body.FirstChild();
		REQUIRE ( p );
		CHECK   ( p.Name() == "p" );

		auto txt = p.FirstChild();
		REQUIRE ( txt );
		CHECK   ( txt.IsText() );
		CHECK   ( txt.Name() == "hi" );

		// reverse traversal via Parent()
		CHECK ( txt.Parent().Raw() == p.Raw()    );
		CHECK ( p  .Parent().Raw() == body.Raw() );
		CHECK ( body.Parent().Raw() == html.Raw());
	}

	// ------------------------------------------------------------------
	// Range-based for over Children() and Attributes()
	// ------------------------------------------------------------------
	SECTION("Children() and Attributes() ranges work with range-for")
	{
		KHTML doc;
		doc.Parse(KStringView{
			"<root><a id='x' class=\"y\"/><b/><c/></root>"
		});

		auto root = doc.Root();
		auto rootChild = root.FirstChild();
		REQUIRE ( rootChild );
		CHECK   ( rootChild.Name() == "root" );

		std::vector<KString> names;
		for (auto child : rootChild.Children())
		{
			names.emplace_back(child.Name());
		}
		REQUIRE ( names.size() == 3 );
		CHECK   ( names[0] == "a" );
		CHECK   ( names[1] == "b" );
		CHECK   ( names[2] == "c" );

		// attributes on <a>
		auto a = rootChild.FirstChild();
		REQUIRE ( a );

		// attribute order on the POD chain mirrors the heap-DOM
		// KHTMLAttributes container (which sorts lexicographically by
		// name): expect "class" before "id".
		std::vector<KString> attrs;
		for (auto attr : a.Attributes())
		{
			attrs.emplace_back(KString(attr.Name()) + "=" + KString(attr.Value()));
		}
		REQUIRE ( attrs.size() == 2 );
		CHECK   ( attrs[0] == "class=y" );
		CHECK   ( attrs[1] == "id=x"    );

		// quote chars must be preserved exactly as parsed
		auto idAttr = a.FindAttr("id");
		REQUIRE ( idAttr );
		CHECK   ( idAttr.Quote() == '\'' );

		auto cls = a.FindAttr("class");
		REQUIRE ( cls );
		CHECK   ( cls.Quote() == '"' );

		// unknown attribute returns an empty cursor
		auto missing = a.FindAttr("not-there");
		CHECK ( !missing );
	}

	// ------------------------------------------------------------------
	// CountChildren / CountAttrs
	// ------------------------------------------------------------------
	SECTION("CountChildren and CountAttrs match manual traversal")
	{
		KHTML doc;
		doc.Parse(KStringView{
			"<div a='1' b='2' c='3'><x/><y/></div>"
		});

		auto div = doc.Root().FirstChild();
		REQUIRE ( div );
		CHECK   ( div.Name()          == "div" );
		CHECK   ( div.CountChildren() == 2 );
		CHECK   ( div.CountAttrs()    == 3 );
	}

	// ------------------------------------------------------------------
	// Sibling navigation: NextSibling / PrevSibling round-trip
	// ------------------------------------------------------------------
	SECTION("NextSibling / PrevSibling")
	{
		KHTML doc;
		doc.Parse("<root><a/><b/><c/></root>");

		auto root = doc.Root().FirstChild();
		auto a = root.FirstChild();
		auto b = a.NextSibling();
		auto c = b.NextSibling();
		auto past = c.NextSibling();

		REQUIRE ( a );
		REQUIRE ( b );
		REQUIRE ( c );
		CHECK   ( !past );

		CHECK ( a.Name() == "a" );
		CHECK ( b.Name() == "b" );
		CHECK ( c.Name() == "c" );

		// reverse walk
		CHECK ( c.PrevSibling().Raw() == b.Raw() );
		CHECK ( b.PrevSibling().Raw() == a.Raw() );
		CHECK ( !a.PrevSibling()                  );

		// LastChild
		CHECK ( root.LastChild().Raw() == c.Raw() );
	}

	// ------------------------------------------------------------------
	// Text-vs-RawText (TextDoNotEscape flag) round-trip
	// ------------------------------------------------------------------
	SECTION("IsTextRaw reflects TextDoNotEscape for <script> content")
	{
		KHTML doc;
		doc.Parse(KStringView{
			"<html><body>"
			"<p>plain</p>"
			"<script>if (a < b) {}</script>"
			"</body></html>"
		});

		// walk to <script> child
		auto html   = doc.Root().FirstChild();
		auto body   = html.FirstChild();
		auto script = body.LastChild();
		REQUIRE ( script );
		CHECK   ( script.Name() == "script" );

		auto raw = script.FirstChild();
		REQUIRE ( raw );
		CHECK   ( raw.IsText()    );
		CHECK   ( raw.IsTextRaw() );
		// content stored verbatim in Name
		CHECK   ( raw.Name() == "if (a < b) {}" );

		// the <p> content is a normal (escaped) text node
		auto p     = body.FirstChild();
		auto plain = p.FirstChild();
		REQUIRE ( plain );
		CHECK   ( plain.IsText() );
		CHECK   ( !plain.IsTextRaw() );
	}

	// ------------------------------------------------------------------
	// Comment / CData / ProcessingInstruction / DocumentType discrimination
	// ------------------------------------------------------------------
	SECTION("Special node kinds are reflected by IsXxx() helpers")
	{
		KHTML doc;
		doc.Parse(KStringView{
			"<?xml version=\"1.0\"?>"
			"<!DOCTYPE html>"
			"<root>"
			"<!-- a comment -->"
			"<![CDATA[some <raw> data]]>"
			"</root>"
		});

		// Walk top-level children of the synthetic root and tally kinds.
		std::size_t pi = 0, dt = 0, el = 0;
		for (auto child : doc.Root().Children())
		{
			if (child.IsProcessingInstruction()) ++pi;
			if (child.IsDocumentType())          ++dt;
			if (child.IsElement())               ++el;
		}
		CHECK ( pi == 1 );
		CHECK ( dt == 1 );
		CHECK ( el == 1 );

		// Inside <root>: one comment, one CData
		auto root = doc.Root().LastChild();   // <root>
		REQUIRE ( root.IsElement() );
		CHECK   ( root.Name() == "root" );

		std::size_t cm = 0, cd = 0;
		for (auto child : root.Children())
		{
			if (child.IsComment()) ++cm;
			if (child.IsCData())   ++cd;
		}
		CHECK ( cm == 1 );
		CHECK ( cd == 1 );
	}

	// ------------------------------------------------------------------
	// Recursive collector: equivalent to xapis/html.cpp::TraverseHTML on
	// the heap DOM, but operating purely on POD cursors.
	// ------------------------------------------------------------------
	SECTION("Recursive traversal collects text + lang attributes (heap-free)")
	{
		KHTML doc;
		doc.Parse(KStringView{
			"<html lang='en'>"
			"<body><p lang='de'>Hallo</p><p>World</p></body>"
			"</html>"
		});

		std::vector<KString> texts;
		std::vector<KString> langs;

		std::function<void(khtml::NodeCursor)> walk;
		walk = [&](khtml::NodeCursor cur)
		{
			if (!cur) return;
			switch (cur.Kind())
			{
				case khtml::NodeKind::Element:
				{
					if (auto lang = cur.FindAttr("lang"))
					{
						langs.emplace_back(lang.Value());
					}
					for (auto child : cur.Children())
					{
						walk(child);
					}
					break;
				}
				case khtml::NodeKind::Text:
					texts.emplace_back(cur.Name());
					break;
				default:
					break;
			}
		};

		walk(doc.Root());

		REQUIRE ( langs.size() == 2 );
		CHECK   ( langs[0] == "en" );
		CHECK   ( langs[1] == "de" );

		REQUIRE ( texts.size() == 2 );
		CHECK   ( texts[0] == "Hallo" );
		CHECK   ( texts[1] == "World" );

		// Crucially: the heap DOM was never touched.
		// (Calling DOM() now would lazily materialize, but we never did.)
	}

	// ------------------------------------------------------------------
	// Empty / build-path KHTML behaviour
	// ------------------------------------------------------------------
	SECTION("Root() on a freshly-constructed KHTML is empty but valid")
	{
		KHTML doc;

		auto root = doc.Root();
		REQUIRE ( root );
		CHECK   ( root.IsElement() );
		CHECK   ( root.Name().empty() );
		CHECK   ( root.CountChildren() == 0 );
		CHECK   ( !root.FirstChild() );
		CHECK   ( !root.LastChild()  );
	}

	SECTION("Root() returns a writeable handle into the arena")
	{
		// Phase 4b: KHTML has only one tree (arena-backed POD).
		// Root() yields a KHTMLNode that can both read and write that tree.
		KHTML doc;
		auto html = doc.Root().AddElement("html");
		html.AddElement("body");

		auto root = doc.Root();
		REQUIRE ( root );
		CHECK   ( root.CountChildren() == 1 );
		CHECK   ( root.FirstChild().Name() == "html" );
	}
}
