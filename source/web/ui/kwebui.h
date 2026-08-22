/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
 //
 // +-------------------------------------------------------------------------+
 // | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
 // |/+---------------------------------------------------------------------+/|
 // |/|                                                                     |/|
 // |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
 // |/|                                                                     |/|
 // |\|   OPEN SOURCE LICENSE                                               |\|
 // |/|                                                                     |/|
 // |\|   Permission is hereby granted, free of charge, to any person       |\|
 // |/|   obtaining a copy of this software and associated                  |/|
 // |\|   documentation files (the "Software"), to deal in the              |\|
 // |/|   Software without restriction, including without limitation        |/|
 // |\|   the rights to use, copy, modify, merge, publish,                  |\|
 // |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
 // |\|   and to permit persons to whom the Software is furnished to        |\|
 // |/|   do so, subject to the following conditions:                       |/|
 // |\|                                                                     |\|
 // |/|   The above copyright notice and this permission notice shall       |/|
 // |\|   be included in all copies or substantial portions of the          |\|
 // |/|   Software.                                                         |/|
 // |\|                                                                     |\|
 // |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
 // |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
 // |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
 // |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 //
 */
#pragma once

/// @file kwebui.h
/// Composite UI components built on top of `html::` primitives.
/// Each component is a KWebObject<Derived> just like the primitives, so it
/// integrates seamlessly with the `parent.Add<T>(...)` builder pattern, with
/// `Page::Generate()` / `Synchronize()`, and with the arena allocator.
///
/// Three composition styles are demonstrated here as the prototype trio:
///   * `Card`  — slot accessors that return `KHTMLNode` (Header/Body/Footer)
///   * `Stack` — pure layout container (children added directly via `.Add<>()`)
///   * `Modal` — lambda-slot style (`.Header([](auto h){...}).Body(...)...`)
///
/// CSS class names follow Bootstrap conventions for now (`.card`, `.modal`, …)
/// — themeing/CSS-framework selection will land in a future iteration.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <optional>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

namespace html {
namespace ui {

// =============================================================================
// -- Layout -------------------------------------------------------------------
// =============================================================================

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `Stack` — vertical or horizontal layout container with gap. Children are
/// added directly through the inherited `.Add<>()` API.
///
/// Uses inline `style` so it works without any CSS file. If you've defined
/// a `.stack-v` / `.stack-h` rule in your stylesheet, the matching class is
/// also applied.
class DEKAF2_PUBLIC Stack : public KWebObject<Stack>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Stack";

//----------
public:
//----------

	static constexpr std::size_t TYPE    = s_sObjectName.Hash();
	static constexpr KStringView TagName = "div";

	enum DIRECTION { VERTICAL, HORIZONTAL };

	/// `sGap` is a raw CSS length (e.g. "1rem", "8px", "0.5em").
	Stack(KHTMLNode parent,
	      DIRECTION dir   = VERTICAL,
	      KStringView sGap = "1rem",
	      KStringView sID  = KStringView{})
	: KWebObject<Stack>(parent, TagName,
	                    dir == VERTICAL ? html::Classes{"stack-v"}
	                                    : html::Classes{"stack-h"},
	                    sID)
	{
		KString sStyle{"display:flex;flex-direction:"};
		sStyle += (dir == VERTICAL) ? "column" : "row";
		sStyle += ";gap:";
		sStyle += sGap;
		SetStyle(sStyle);
	}

}; // Stack


// =============================================================================
// -- Composite block (slot-accessor style) ------------------------------------
// =============================================================================

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `Card` — three-zone composite (header/body/footer) exposed as slot
/// accessors. The body is the most common target — `.Body().Add<...>()` is
/// the canonical pattern.
///
///   auto card = parent.Add<html::ui::Card>("Welcome");
///   card.Body().Add<html::Paragraph>("Hello.");
///   card.Footer().Add<html::Button>("OK");
class DEKAF2_PUBLIC Card : public KWebObject<Card>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Card";

//----------
public:
//----------

	static constexpr std::size_t TYPE    = s_sObjectName.Hash();
	static constexpr KStringView TagName = "div";

	Card(KHTMLNode parent,
	     KStringView sTitle = KStringView{},
	     KStringView sID    = KStringView{})
	: KWebObject<Card>(parent, TagName, html::Classes{"card"}, sID)
	{
		m_header = this->template Add<html::Div>(html::Classes{"card-header"});
		if (!sTitle.empty())
		{
			m_header.AddElement("h3").AddText(sTitle);
		}
		m_body   = this->template Add<html::Div>(html::Classes{"card-body"});
		m_footer = this->template Add<html::Div>(html::Classes{"card-footer"});
	}

	/// Slot accessors. Each returns the underlying KHTMLNode so the caller
	/// can `.Add<>()` further children into that zone.
	KHTMLNode Header() { return m_header; }
	KHTMLNode Body()   { return m_body;   }
	KHTMLNode Footer() { return m_footer; }

//----------
private:
//----------

	KHTMLNode m_header;
	KHTMLNode m_body;
	KHTMLNode m_footer;

}; // Card


// =============================================================================
// -- Interactive (lambda-slot style) ------------------------------------------
// =============================================================================

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `Modal` — Bootstrap-style modal dialog skeleton. Children are placed via
/// lambda slots, which reads similar to Blazor's `RenderFragment` or Vue's
/// named slots:
///
///   body.Add<html::ui::Modal>("Confirm")
///       .Body  ([](KHTMLNode b) { b.AddElement("p").AddText("Are you sure?"); })
///       .Footer([](KHTMLNode f) {
///           f.AddElement("button").AddText("OK");
///           f.AddElement("button").AddText("Cancel");
///       });
///
/// Show/hide behaviour is left to the host page (Bootstrap JS, HTMX, etc.).
class DEKAF2_PUBLIC Modal : public KWebObject<Modal>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Modal";

//----------
public:
//----------

	static constexpr std::size_t TYPE    = s_sObjectName.Hash();
	static constexpr KStringView TagName = "div";

	Modal(KHTMLNode parent,
	      KStringView sTitle = KStringView{},
	      KStringView sID    = KStringView{})
	: KWebObject<Modal>(parent, TagName, html::Classes{"modal"}, sID)
	{
		// .modal > .modal-dialog > .modal-content > {header, body, footer}
		auto dialog  = this->template Add<html::Div>(html::Classes{"modal-dialog"});
		auto content = dialog.template Add<html::Div>(html::Classes{"modal-content"});

		m_header = content.template Add<html::Div>(html::Classes{"modal-header"});
		if (!sTitle.empty())
		{
			m_header.AddElement("h2").AddText(sTitle);
		}
		m_body   = content.template Add<html::Div>(html::Classes{"modal-body"});
		m_footer = content.template Add<html::Div>(html::Classes{"modal-footer"});
	}

	/// Lambda-slot accessors. The supplied callable receives a `KHTMLNode`
	/// pointing at the corresponding zone and adds whatever children it
	/// wants. Returns `Modal&` so the three slots can be chained.
	template<typename F> Modal& Header(F&& f) { std::forward<F>(f)(m_header); return *this; }
	template<typename F> Modal& Body  (F&& f) { std::forward<F>(f)(m_body);   return *this; }
	template<typename F> Modal& Footer(F&& f) { std::forward<F>(f)(m_footer); return *this; }

//----------
private:
//----------

	KHTMLNode m_header;
	KHTMLNode m_body;
	KHTMLNode m_footer;

}; // Modal

// =============================================================================
// -- Feedback -----------------------------------------------------------------
// =============================================================================

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `Flash` — one-shot request feedback banner (notice, error, info).
///
///   if (!sNotice.empty()) main.Add<html::ui::Flash>(sNotice);
///   if (!sError.empty())  main.Add<html::ui::Flash>(sError, html::ui::Flash::Error);
///
/// Default CSS classes are `flash ok|err|info`; pass explicit Classes to
/// override (e.g. Bootstrap's `alert alert-danger`).
class DEKAF2_PUBLIC Flash : public KWebObject<Flash>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Flash";

//----------
public:
//----------

	static constexpr std::size_t TYPE    = s_sObjectName.Hash();
	static constexpr KStringView TagName = "div";

	enum LEVEL { Success, Error, Info };

	Flash(KHTMLNode parent,
	      KStringView sMessage,
	      LEVEL level = Success,
	      Classes cls = Classes{},
	      KStringView sID = KStringView{})
	: KWebObject<Flash>(parent, TagName,
	                    !cls.empty() ? std::move(cls)
	                                 : Classes{level == Success ? "flash ok"
	                                         : level == Error   ? "flash err"
	                                                            : "flash info"},
	                    sID)
	{
		AddText(sMessage);
	}

}; // Flash


// =============================================================================
// -- Navigation ---------------------------------------------------------------
// =============================================================================

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `NavBar` — brand text plus a row of links, of which one can be marked
/// active:
///
///   auto nav = body.Add<html::ui::NavBar>("myapp admin");
///   nav.Link("Dashboard", "/",       bSection == "dashboard")
///      .Link("Users",     "/users",  bSection == "users")
///      .Link("Logout",    "/logout");
///
/// Structure: div.top > div.brand + nav > a(.active)*
class DEKAF2_PUBLIC NavBar : public KWebObject<NavBar>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "NavBar";

//----------
public:
//----------

	static constexpr std::size_t TYPE    = s_sObjectName.Hash();
	static constexpr KStringView TagName = "div";

	NavBar(KHTMLNode parent,
	       KStringView sBrand = KStringView{},
	       const Classes& cls = Classes{"top"},
	       KStringView sID    = KStringView{})
	: KWebObject<NavBar>(parent, TagName, cls, sID)
	{
		m_brand = this->template Add<html::Div>(html::Classes{"brand"});
		if (!sBrand.empty())
		{
			m_brand.AddText(sBrand);
		}
		m_nav = AddElement("nav");
	}

	/// append a nav link - bActive marks the current section's entry
	NavBar& Link(KStringView sLabel, KStringView sURL, bool bActive = false)
	{
		m_nav.template Add<html::Link>(sURL, sLabel,
		                               bActive ? Classes{"active"} : Classes{});
		return *this;
	}

	/// slot accessor for richer brand content
	KHTMLNode Brand() { return m_brand; }

//----------
private:
//----------

	KHTMLNode m_brand;
	KHTMLNode m_nav;

}; // NavBar


// =============================================================================
// -- Data ---------------------------------------------------------------------
// =============================================================================

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `Table` — thead/tbody scaffolding with convenient row building:
///
///   auto table = sec.Add<html::ui::Table>();
///   table.Headers({ "Name", "Created", "" });
///   for (const auto& Row : Rows)
///   {
///       auto tr = table.AddRow();
///       tr.Add<html::TableData>(Row.sName);
///       tr.Add<html::TableData>(Row.sCreated);
///       auto actions = tr.Add<html::TableData>();  // complex cell content
///       actions.Add<html::Form>("/delete") ...;
///   }
///
/// Plain text rows go in one call: `table.AddRow({ "a", "b", "c" });`
class DEKAF2_PUBLIC Table : public KWebObject<Table>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "UITable";

//----------
public:
//----------

	static constexpr std::size_t TYPE    = s_sObjectName.Hash();
	static constexpr KStringView TagName = "table";

	Table(KHTMLNode parent,
	      const Classes& cls = Classes{"grid"},
	      KStringView sID    = KStringView{})
	: KWebObject<Table>(parent, TagName, cls, sID)
	{
		m_head = AddElement("thead");
		m_body = AddElement("tbody");
	}

	/// set the header row (thead > tr > th*)
	Table& Headers(const std::vector<KStringView>& Columns)
	{
		auto tr = m_head.template Add<html::TableRow>();

		for (auto sColumn : Columns)
		{
			tr.template Add<html::TableHeader>(sColumn);
		}

		return *this;
	}

	/// append an empty body row - add cells with Add<html::TableData>()
	KHTMLNode AddRow()
	{
		return m_body.template Add<html::TableRow>();
	}

	/// append a body row of plain text cells, and return it for amendments
	KHTMLNode AddRow(const std::vector<KStringView>& Cells)
	{
		auto tr = AddRow();

		for (auto sCell : Cells)
		{
			tr.template Add<html::TableData>(sCell);
		}

		return tr;
	}

//----------
private:
//----------

	KHTMLNode m_head;
	KHTMLNode m_body;

}; // Table


// =============================================================================
// -- Forms --------------------------------------------------------------------
// =============================================================================

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `Field` — a labeled form input:
///
///   auto field = form.Add<html::ui::Field>("Username", "username");
///   field.Input().SetRequired().SetAutoComplete("off");
///
/// Structure: div.field > label + input
class DEKAF2_PUBLIC Field : public KWebObject<Field>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Field";

//----------
public:
//----------

	static constexpr std::size_t TYPE    = s_sObjectName.Hash();
	static constexpr KStringView TagName = "div";

	Field(KHTMLNode parent,
	      KStringView sLabel,
	      KStringView sName,
	      KStringView sValue         = KStringView{},
	      html::Input::INPUTTYPE type = html::Input::TEXT,
	      const Classes& cls          = Classes{"field"},
	      KStringView sID             = KStringView{})
	: KWebObject<Field>(parent, TagName, cls, sID)
	{
		AddElement("label").AddText(sLabel);
		m_input.emplace(this->template Add<html::Input>(sName, sValue, type));
	}

	/// the wrapped input, for SetRequired(), SetPlaceholder(), ...
	html::Input& Input() { return *m_input; }

//----------
private:
//----------

	std::optional<html::Input> m_input;

}; // Field

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
// C++14 ODR-defs (TagName is a constexpr static, may be ODR-used)
constexpr KStringView Stack::TagName;
constexpr KStringView Card::TagName;
constexpr KStringView Modal::TagName;
constexpr KStringView Flash::TagName;
constexpr KStringView NavBar::TagName;
constexpr KStringView Table::TagName;
constexpr KStringView Field::TagName;
#endif

} // namespace ui
} // namespace html

DEKAF2_NAMESPACE_END
