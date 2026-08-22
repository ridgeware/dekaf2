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

	SECTION("Flash — levels and custom classes")
	{
		html::Page page("Flash Demo");
		auto body = page.Body();

		body.Add<html::ui::Flash>("saved.");
		body.Add<html::ui::Flash>("boom & bust", html::ui::Flash::Error);
		body.Add<html::ui::Flash>("styled", html::ui::Flash::Info, html::Classes{"alert alert-info"});

		auto sOut = page.Serialize();

		CHECK( sOut.contains(R"(class="flash ok")")        );
		CHECK( sOut.contains("saved.")                     );
		CHECK( sOut.contains(R"(class="flash err")")       );
		CHECK( sOut.contains("boom &amp; bust")            ); // escaped by construction
		CHECK( sOut.contains(R"(class="alert alert-info")"));
	}

	SECTION("NavBar — brand, links, active marker")
	{
		html::Page page("Nav Demo");
		auto body = page.Body();

		auto nav = body.Add<html::ui::NavBar>("myapp admin");
		nav.Link("Dashboard", "/",      false)
		   .Link("Users",     "/users", true)
		   .Link("Logout",    "/logout");

		auto sOut = page.Serialize();

		CHECK( sOut.contains(R"(class="top")")             );
		CHECK( sOut.contains(R"(class="brand")")           );
		CHECK( sOut.contains("myapp admin")                );
		CHECK( sOut.contains("<nav>")                      );
		CHECK( sOut.contains(R"(href="/users")")           );
		CHECK( sOut.contains(R"(class="active")")          );
		// exactly one active entry
		CHECK( sOut.find(R"(class="active")") == sOut.rfind(R"(class="active")") );
	}

	SECTION("Table — headers, text rows, complex cells")
	{
		html::Page page("Table Demo");
		auto body = page.Body();

		auto table = body.Add<html::ui::Table>();
		table.Headers({ "Name", "Created", "" });
		table.AddRow({ "alice", "2026-01-01" });

		auto tr = table.AddRow();
		tr.Add<html::TableData>("bob <admin>");
		tr.Add<html::TableData>("2026-02-02");
		auto actions = tr.Add<html::TableData>();
		auto form    = actions.Add<html::Form>("/delete");
		form.SetMethod(html::Form::POST);
		form.Add<html::Button>("Delete");

		auto sOut = page.Serialize();

		CHECK( sOut.contains(R"(class="grid")")      );
		CHECK( sOut.contains("<thead>")              );
		CHECK( sOut.contains("<th>") );
		CHECK( sOut.contains("Name") );
		CHECK( sOut.contains("<tbody>")              );
		CHECK( sOut.contains("alice")                );
		CHECK( sOut.contains("bob &lt;admin&gt;")    ); // escaped by construction
		CHECK( sOut.contains(R"(action="/delete")")  );
		CHECK( sOut.contains(R"(method="post")")     );
	}

	SECTION("Field — label plus input")
	{
		html::Page page("Field Demo");
		auto body = page.Body();

		auto form  = body.Add<html::Form>("/login");
		auto field = form.Add<html::ui::Field>("Username", "username");
		field.Input().SetRequired(true);
		form.Add<html::ui::Field>("Password", "password", "", html::Input::PASSWORD);

		auto sOut = page.Serialize();

		CHECK( sOut.contains(R"(class="field")")       );
		CHECK( sOut.contains("<label>Username</label>"));
		CHECK( sOut.contains(R"(name="username")")     );
		CHECK( sOut.contains("required")               );
		CHECK( sOut.contains(R"(type="password")")     );
	}
}
