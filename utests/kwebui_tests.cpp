#include "catch.hpp"

#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/ui/kwebui.h>

using namespace dekaf2;

// Short-form sample code for the html::ui:: composite components.
// These exercise three different composition styles:
//   * Stack  — pure layout container (children added via `.Add<>()`)
//   * Card   — slot accessors (Header / Body / Footer return KHTMLNode)
//   * Modal  — lambda slots (callable receives the slot's KHTMLNode)

TEST_CASE("KWebUI")
{
	SECTION("Stack — layout container with inline style")
	{
		html::Page page("Stack Demo");
		auto body = page.Body();

		auto stack = body.Add<html::ui::Stack>(html::ui::Stack::VERTICAL, "0.5rem");
		stack.Add<html::Paragraph>().AddText("first");
		stack.Add<html::Paragraph>().AddText("second");

		auto sOut = page.Serialize();

		CHECK( sOut.contains("stack-v")                                       );
		CHECK( sOut.contains("display:flex;flex-direction:column;gap:0.5rem") );
		CHECK( sOut.contains("first")                                         );
		CHECK( sOut.contains("second")                                        );
	}

	SECTION("Card — slot accessors")
	{
		html::Page page("Card Demo");
		auto body = page.Body();

		auto card = body.Add<html::ui::Card>("Welcome");
		card.Body  ().AddElement("p").AddText("Greetings from dekaf2.");
		card.Footer().AddElement("small").AddText("Built with KWebObjects.");

		auto sOut = page.Serialize();

		CHECK( sOut.contains("card-header") );
		CHECK( sOut.contains("Welcome"    ) );
		CHECK( sOut.contains("card-body"  ) );
		CHECK( sOut.contains("Greetings from dekaf2.") );
		CHECK( sOut.contains("card-footer") );
		CHECK( sOut.contains("Built with KWebObjects.") );
	}

	SECTION("Modal — lambda slots")
	{
		html::Page page("Modal Demo");
		auto body = page.Body();

		body.Add<html::ui::Modal>("Confirm deletion?", "del-modal")
		    .Body  ([](KHTMLNode b)
		    {
		        b.AddElement("p").AddText("This action is permanent.");
		    })
		    .Footer([](KHTMLNode f)
		    {
		        f.AddElement("button").AddText("Cancel");
		        f.AddElement("button").AddText("Delete");
		    });

		auto sOut = page.Serialize();

		CHECK( sOut.contains(R"(id="del-modal")"  ) );
		CHECK( sOut.contains("modal-dialog"       ) );
		CHECK( sOut.contains("modal-content"      ) );
		CHECK( sOut.contains("Confirm deletion?"  ) );
		CHECK( sOut.contains("This action is permanent.") );
		CHECK( sOut.contains("Cancel"             ) );
		CHECK( sOut.contains("Delete"             ) );
	}

	SECTION("Composing components — Card inside a Stack")
	{
		html::Page page("Composition Demo");
		auto body = page.Body();

		auto stack = body.Add<html::ui::Stack>();
		stack.Add<html::ui::Card>("Card 1").Body().AddElement("p").AddText("A");
		stack.Add<html::ui::Card>("Card 2").Body().AddElement("p").AddText("B");
		stack.Add<html::ui::Card>("Card 3").Body().AddElement("p").AddText("C");

		auto sOut = page.Serialize();
		// Three card-bodies, one stack-v wrapper.
		CHECK( sOut.contains("Card 1") );
		CHECK( sOut.contains("Card 2") );
		CHECK( sOut.contains("Card 3") );
		std::size_t iCount = 0;
		KStringView sView{sOut};
		for (auto pos = sView.find("card-body"); pos != KStringView::npos;
		     pos = sView.find("card-body", pos + 1))
		{
			++iCount;
		}
		CHECK( iCount == 3 );
	}
}
