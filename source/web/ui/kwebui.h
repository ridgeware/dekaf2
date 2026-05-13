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
public:
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
public:
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

private:
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
public:
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

private:
	KHTMLNode m_header;
	KHTMLNode m_body;
	KHTMLNode m_footer;
}; // Modal

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
// C++14 ODR-defs (TagName is a constexpr static, may be ODR-used)
constexpr KStringView Stack::TagName;
constexpr KStringView Card::TagName;
constexpr KStringView Modal::TagName;
#endif

} // namespace ui
} // namespace html

DEKAF2_NAMESPACE_END
