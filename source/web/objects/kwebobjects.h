/*
// Copyright (c) 2021, Joachim Schurig (jschurig@gmx.net)
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


/// @file kwebobjects.h
/// Phase 4 design: KWebObjects are non-virtual 8-byte handles (KHTMLNode)
/// into a KHTML's arena-backed POD tree. Construction always goes through
/// a parent KHTMLNode — there is no "detached" mode any more; the
/// canonical idiom is `parent.Add<html::T>(args...)`. See
/// `notes/kwebobjects-migration-guide.md` for the conversion patterns.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstringutils.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/web/html/khtmldom.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/system/os/ksystem.h>
#include <vector>
#include <limits>
#include <chrono>
#include <type_traits>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup web_objects
/// @{

namespace html {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// CSS class definition (selector + body). Held by user code, not part of
/// the DOM. Registered with a Page via `page.AddClass(c)` to emit into the
/// page's `<style>` block.
class DEKAF2_PUBLIC Class
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Class() = default;
	Class(KString sClassName, KString sClassDefinition = KString{});

	const KString GetName()       const { return m_sClassName;       }
	const KString GetDefinition() const { return m_sClassDefinition; }

	bool empty() const { return m_sClassName.empty() || m_sClassDefinition.empty(); }

//----------
protected:
//----------

	KString m_sClassName;
	KString m_sClassDefinition;

}; // Class

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A collection of class names to apply to an element's `class` attribute.
class DEKAF2_PUBLIC Classes
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using self = Classes;

	Classes() = default;
	Classes(const Class& Class)
	{
		Add(Class);
	}
	Classes(KString sClassName)
	{
		Add(sClassName.ToView());
	}
	Classes(KStringView sClassName)
	{
		Add(sClassName);
	}
	Classes(const char* sClassName)
	{
		Add(KStringView(sClassName));
	}

	self& Add(const Class& Class);

	self& Add(const Classes& Classes)
	{
		return Add(Classes.m_sClassNames.ToView());
	}

	self& Add(KStringView sClassName);

	const KString& GetClasses() const { return m_sClassNames; }

	bool empty() const { return m_sClassNames.empty(); }

//----------
protected:
//----------

	KString m_sClassNames;

}; // Classes

} // end of namespace html

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Non-virtual base of all KWebObject builders. Inherits the 8-byte
/// `KHTMLNode` handle and adds the universal enums + helpers + Generate /
/// Synchronize entry points. All actual setters live in
/// `KWebObject<Derived>` (CRTP mixin).
class DEKAF2_PUBLIC KWebObjectBase : public KHTMLNode
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static constexpr KStringView s_sObjectName = "KWebObjectBase";
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	enum ENCTYPE { URLENCODED, FORMDATA, PLAIN };
	enum METHOD  { GET, POST, DIALOG };
	enum LOADING { AUTO, EAGER, LAZY };
	enum TARGET  { SELF, BLANK, PARENT, TOP };
	enum DIR     { LTR, RTL };
	enum ALIGN   { LEFT, CENTER, RIGHT };
	enum VALIGN  { VTOP, VMIDDLE, VBOTTOM };
	enum Preload { None, Metadata, Auto };

	using Pixels = uint32_t;

	KWebObjectBase() = default;
	/// Construct an arena-backed element as child of `parent`. Allocates
	/// the NodePOD, appends to `parent`, and (if non-empty) applies the
	/// initial `id` / `class` attributes.
	KWebObjectBase(KHTMLNode parent,
	               KStringView sTag,
	               const html::Classes& cls = html::Classes{},
	               KStringView sID = KStringView{});

	/// Construct from an existing KHTMLNode handle (no allocation). Used
	/// by composites that need to point at an existing node.
	explicit KWebObjectBase(KHTMLNode node) : KHTMLNode(node) {}

	/// Serialize this element (and its subtree). Output matches the
	/// document-level serialize for this subtree.
	void    Serialize(KOutStream& OutStream, char chIndent = '\t') const;
	void    Serialize(KStringRef& sOut, char chIndent = '\t') const;
	KString Serialize(char chIndent = '\t') const;

	/// Print(...) is an alias for Serialize(...), preserved for back-compat.
	KString Print(char chIndent = '\t', uint16_t /*iIndent*/ = 0) const { return Serialize(chIndent); }

	/// Walk the entire document this node belongs to and auto-assign
	/// missing `name` / `value` attributes for inputs / options.
	void Generate();

	/// Walk the document's bindings (sparse side-map keyed by NodePOD*)
	/// and call each binding's pfnSync with the matching query-parm value.
	void Synchronize(const url::KQueryParms& Parms);

	// -- static helpers (unchanged from pre-Phase-4) ---------------------------

	static KStringView FromMethod  (METHOD  method  );
	static KStringView FromTarget  (TARGET  target  );
	static KStringView FromDir     (DIR     dir     );
	static KStringView FromLoading (LOADING loading );
	static KStringView FromEncType (ENCTYPE encoding);
	static KStringView FromAlign   (ALIGN   align   );
	static KStringView FromVAlign  (VALIGN  valign  );
	static KStringView FromPreload (Preload preload );

//----------
protected:
//----------

	/// Set a boolean HTML attribute. Boolean HTML attrs (disabled,
	/// required, ...) get emitted bare (e.g. `<input disabled>`) when set,
	/// and removed when unset. Non-boolean attrs get `"true"`/`"false"`.
	void SetBoolAttribute(KStringView sName, bool bYesNo);

	/// Insert a text node before the first input child of this label. For
	/// LabeledInput composites.
	void SetTextBefore(KStringView sLabel);
	/// Append a text node after the last input child of this label.
	void SetTextAfter (KStringView sLabel);

	/// Helper for the bound-input templates to register a binding in the
	/// document's side-map.
	void RegisterBinding(KHTMLNode pInputNode, const khtml::InteractiveBinding& binding);

}; // KWebObjectBase

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// CRTP setter mixin. Defines all universal HTML attribute setters once,
/// each returning `Derived&` so call chains preserve the concrete type.
/// No virtual functions — every dispatch is static.
template<typename Derived>
class KWebObject : public KWebObjectBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using KWebObjectBase::KWebObjectBase;
	using self = Derived;

	// -- universal attribute setters --

	self& SetID(KStringView sID)
	{
		if (!sID.empty()) KHTMLNode::SetAttribute("id", sID);
		else              KHTMLNode::RemoveAttribute("id");
		return This();
	}

	self& SetClass(const html::Classes& cls)
	{
		if (!cls.empty()) KHTMLNode::SetAttribute("class", cls.GetClasses());
		else              KHTMLNode::RemoveAttribute("class");
		return This();
	}

	self& SetDir       (DIR    dir    ) { KHTMLNode::SetAttribute("dir",     FromDir    (dir    )); return This(); }
	self& SetDraggable (bool   bYesNo ) { SetBoolAttribute       ("draggable",          bYesNo  ); return This(); }
	self& SetHidden    (bool   bYesNo ) { SetBoolAttribute       ("hidden",             bYesNo  ); return This(); }
	self& SetLanguage  (KStringView v ) { if (!v.empty()) KHTMLNode::SetAttribute("lang",  v);     return This(); }
	self& SetStyle     (KStringView v ) { if (!v.empty()) KHTMLNode::SetAttribute("style", v);     return This(); }
	self& SetTitle     (KStringView v ) { if (!v.empty()) KHTMLNode::SetAttribute("title", v);     return This(); }

	self& SetLink      (KStringView sURL, bool bDoNotEscape = true) { if (!sURL.empty())  KHTMLNode::SetAttribute("href",   sURL, '"', /*esc*/!bDoNotEscape); return This(); }
	self& SetSource    (KStringView sURL, bool bDoNotEscape = true) { if (!sURL.empty())  KHTMLNode::SetAttribute("src",    sURL, '"', /*esc*/!bDoNotEscape); return This(); }
	self& SetPoster    (KStringView sURL, bool bDoNotEscape = true) { if (!sURL.empty())  KHTMLNode::SetAttribute("poster", sURL, '"', /*esc*/!bDoNotEscape); return This(); }
	self& SetDescription(KStringView v)                             { if (!v.empty())    KHTMLNode::SetAttribute("alt",    v);                                  return This(); }

	self& SetHeigth(Pixels p) { KHTMLNode::SetAttribute("heigth", kFormat("{}", p)); return This(); }
	self& SetWidth (Pixels p) { KHTMLNode::SetAttribute("width",  kFormat("{}", p)); return This(); }
	self& SetSize  (Pixels p) { KHTMLNode::SetAttribute("size",   kFormat("{}", p)); return This(); }

	self& SetLoading (LOADING l) { KHTMLNode::SetAttribute("loading", FromLoading(l)); return This(); }
	self& SetAlign   (ALIGN   a) { KHTMLNode::SetAttribute("align",   FromAlign  (a)); return This(); }
	self& SetVAlign  (VALIGN  v) { KHTMLNode::SetAttribute("valign",  FromVAlign (v)); return This(); }

	self& SetName  (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("name",  v); return This(); }
	self& SetValue (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("value", v); return This(); }

	self& SetTarget(TARGET  t) { KHTMLNode::SetAttribute("target", FromTarget (t)); return This(); }

	self& SetRel(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("rel", v); return This(); }

	self& SetDisabled        (bool b) { SetBoolAttribute("disabled",        b); return This(); }
	self& SetAutofocus       (bool b) { SetBoolAttribute("autofocus",       b); return This(); }
	self& SetMultiple        (bool b) { SetBoolAttribute("multiple",        b); return This(); }
	self& SetDirectory       (bool b) { SetBoolAttribute("directory",       b);
	                                    SetBoolAttribute("webkitdirectory", b); return This(); }
	self& SetReadOnly        (bool b) { SetBoolAttribute("readonly",        b); return This(); }
	self& SetRequired        (bool b) { SetBoolAttribute("required",        b); return This(); }
	self& SetAsync           (bool b) { SetBoolAttribute("async",           b); return This(); }
	self& SetDefer           (bool b) { SetBoolAttribute("defer",           b); return This(); }
	self& SetLoop            (bool b) { SetBoolAttribute("loop",            b); return This(); }
	self& SetPlaysInline     (bool b) { SetBoolAttribute("playsinline",     b); return This(); }
	self& SetMuted           (bool b) { SetBoolAttribute("muted",           b); return This(); }
	self& SetFormNoValidate  (bool b) { SetBoolAttribute("formnovalidate",  b); return This(); }

	self& SetFormAction(KStringView v)  { if (!v.empty()) KHTMLNode::SetAttribute("formaction",  v);                       return This(); }
	self& SetFormMethod (METHOD  m)     { KHTMLNode::SetAttribute("formmethod",  FromMethod (m));                          return This(); }
	self& SetFormEncType(ENCTYPE e)     { KHTMLNode::SetAttribute("formenctype", FromEncType(e));                          return This(); }
	self& SetFormTarget (TARGET  t)     { KHTMLNode::SetAttribute("formtarget",  FromTarget (t));                          return This(); }

	self& SetAccept     (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("accept",      v); return This(); }
	self& SetPlaceholder(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("placeholder", v); return This(); }
	self& SetLabel      (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("label",       v); return This(); }
	self& SetFor        (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("for",         v); return This(); }
	self& SetForm       (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("form",        v); return This(); }
	self& SetAllow      (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("allow",       v); return This(); }
	self& SetScrolling  (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("scrolling",   v); return This(); }

	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetMin(A v)        { KHTMLNode::SetAttribute("min", kFormat("{}", v)); return This(); }

	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetMax(A v)        { KHTMLNode::SetAttribute("max", kFormat("{}", v)); return This(); }

	template<typename A>
	self& SetRange(A mn, A mx) { SetMin(mn); SetMax(mx); return This(); }

	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetColSpan(A v)    { KHTMLNode::SetAttribute("colspan", kFormat("{}", v)); return This(); }

	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetRowSpan(A v)    { KHTMLNode::SetAttribute("rowspan", kFormat("{}", v)); return This(); }

	self& SetAutoplay()                                              { KHTMLNode::SetAttribute("autoplay", "", /*q*/0, /*esc*/false);        return This(); }
	self& SetControls()                                              { KHTMLNode::SetAttribute("controls", "", /*q*/0, /*esc*/false);        return This(); }
	self& SetAllowFullscreen()                                       { KHTMLNode::SetAttribute("allowfullscreen", "", /*q*/0, /*esc*/false); return This(); }
	self& SetPreload(Preload p)                                      { KHTMLNode::SetAttribute("preload", FromPreload(p));                   return This(); }
	self& SetType   (KStringView mime)                               { if (!mime.empty()) KHTMLNode::SetAttribute("type", mime);             return This(); }

	// -- generic attribute access (KString name overload) --

	self& SetAttribute(KString sName, KStringView sValue, bool bRemoveIfEmpty = false, bool bDoNotEscape = false)
	{
		if (bRemoveIfEmpty && sValue.empty()) { KHTMLNode::RemoveAttribute(sName); }
		else                                  { KHTMLNode::SetAttribute(sName, sValue, '"', /*esc*/!bDoNotEscape); }
		return This();
	}

	template<typename N, std::enable_if_t<std::is_arithmetic<N>::value && !std::is_same<N, bool>::value, int> = 0>
	self& SetAttribute(KString sName, N value)
	{
		KHTMLNode::SetAttribute(sName, kFormat("{}", value));
		return This();
	}

	self& SetAttribute(KString sName, bool bYesNo)
	{
		SetBoolAttribute(sName, bYesNo);
		return This();
	}

	// -- text children --

	self& AddText   (KStringView s) { KHTMLNode::AddText   (s);        return This(); }
	self& AddRawText(KStringView s) { KHTMLNode::AddRawText(s);        return This(); }

//----------
private:
//----------

	Derived&       This()       { return static_cast<      Derived&>(*this); }
	const Derived& This() const { return static_cast<const Derived&>(*this); }

}; // KWebObject

namespace html {

namespace detail {

	template<class T, class = void>
	struct is_str : std::false_type {};

	template<class T>
	struct is_str<T, std::void_t<decltype(KStringView(std::declval<T>()))>> : std::true_type {};

} // namespace detail

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC HTML : public KWebObject<HTML>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "HTML";
//----------
public:
//----------
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "html";

	HTML(KHTMLNode parent, KStringView sLanguage = KStringView{})
	: KWebObject<HTML>(parent, TagName)
	{
		if (!sLanguage.empty()) KHTMLNode::SetAttribute("lang", sLanguage);
	}

	/// Free-name HTML element constructor for generic tags (mirrors the old
	/// `html::HTML("label")` shortcut used by some consumers).
	HTML(KHTMLNode parent, KStringView sTagName, KStringView sID)
	: KWebObject<HTML>(parent, sTagName, sID)
	{
	}

}; // HTML

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A self-contained HTML document. Owns its own `KHTML` (which owns the
/// arena and the POD tree). Provides convenience accessors for `<head>`
/// and `<body>`, plus class-aggregation into a `<style>` block.
class DEKAF2_PUBLIC Page
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------

	Page(KStringView sTitle, KStringView sLanguage = KStringView{});

	Page(const Page&)            = delete;
	Page& operator=(const Page&) = delete;
	Page(Page&&)                 = default;
	// no move-assign: the embedded KHTML (and underlying khtml::Document)
	// has none either, see khtml::Document's class-level UB caveat.

	/// Add an element as child of the body. Mirrors `body.Add<T>(...)`.
	template<class T, class... Args>
	T Add(Args&&... args) { return m_body.Add<T>(std::forward<Args>(args)...); }

	/// Add a text child to the body.
	Page& AddText   (KStringView s) { m_body.AddText   (s); return *this; }
	Page& AddRawText(KStringView s) { m_body.AddRawText(s); return *this; }

	/// Read/write handles into the page's internal KHTML.
	KHTMLNode Head() { return m_head; }
	KHTMLNode Body() { return m_body; }

	/// Add a `<meta name=... content=...>` to the head.
	void AddMeta(KStringView sName, KStringView sContent);
	/// Append a CSS fragment to the page's (lazily-created) `<style>` block.
	void AddStyle(KStringView sStyleDefinition);
	/// Register a class definition; emitted into the page's style block.
	void AddClass(const Class& cls);

	/// Walk the entire document and auto-assign missing input/option names.
	void Generate()
	{
		KWebObjectBase node{m_body};
		node.Generate();
	}

	/// Synchronise input bindings from the supplied query parameters.
	void Synchronize(const url::KQueryParms& parms)
	{
		KWebObjectBase node{m_body};
		node.Synchronize(parms);
	}

	/// Serialize the entire page.
	KString Print(char chIndent = '\t') const                          { return m_doc.Serialize(chIndent); }
	void    Print(KStringRef& s, char chIndent = '\t') const           { m_doc.Serialize(s, chIndent); }
	void    Serialize(KOutStream& os, char chIndent = '\t') const      { m_doc.Serialize(os, chIndent); }
	KString Serialize(char chIndent = '\t') const                      { return m_doc.Serialize(chIndent); }

	/// Direct access to the underlying KHTML — for advanced consumers
	/// (binding registration, arena diagnostics, etc.).
	KHTML&       Doc()       { return m_doc; }
	const KHTML& Doc() const { return m_doc; }

//----------
private:
//----------

	void EnsureStyle();

	KHTML     m_doc;
	KHTMLNode m_head;
	KHTMLNode m_body;
	KHTMLNode m_style;   // optional <style> element, created lazily

}; // Page

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Div : public KWebObject<Div>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Div";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "div";

	Div(KHTMLNode parent, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Div>(parent, TagName, cls, sID) {}
}; // Div

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Span : public KWebObject<Span>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Span";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "span";

	Span(KHTMLNode parent, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Span>(parent, TagName, cls, sID) {}
}; // Span

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Paragraph : public KWebObject<Paragraph>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Paragraph";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "p";

	Paragraph(KHTMLNode parent, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Paragraph>(parent, TagName, cls, sID) {}
}; // Paragraph

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Table : public KWebObject<Table>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Table";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "table";

	Table(KHTMLNode parent, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Table>(parent, TagName, cls, sID) {}
}; // Table

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC TableRow : public KWebObject<TableRow>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "TableRow";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "tr";

	TableRow(KHTMLNode parent, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<TableRow>(parent, TagName, cls, sID) {}
}; // TableRow

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC TableData : public KWebObject<TableData>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "TableData";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "td";

	TableData(KHTMLNode parent,
	          KStringView    sContent = KStringView{},
	          const Classes& cls      = html::Classes{},
	          KStringView    sID      = KStringView{})
	: KWebObject<TableData>(parent, TagName, cls, sID)
	{
		if (!sContent.empty()) AddText(sContent);
	}
}; // TableData

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC TableHeader : public KWebObject<TableHeader>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "TableHeader";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "th";

	TableHeader(KHTMLNode parent,
	            KStringView    sContent = KStringView{},
	            const Classes& cls      = html::Classes{},
	            KStringView    sID      = KStringView{})
	: KWebObject<TableHeader>(parent, TagName, cls, sID)
	{
		if (!sContent.empty()) AddText(sContent);
	}
}; // TableHeader

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Link : public KWebObject<Link>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Link";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "a";

	Link(KHTMLNode parent,
	     KStringView    sURL  = KStringView{},
	     KStringView    sText = KStringView{},
	     const Classes& cls   = html::Classes{},
	     KStringView    sID   = KStringView{})
	: KWebObject<Link>(parent, TagName, cls, sID)
	{
		if (!sURL.empty())  SetLink(sURL);
		if (!sText.empty()) AddText(sText);
	}
}; // Link

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Script : public KWebObject<Script>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Script";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "script";

	Script(KHTMLNode parent, KStringView sCharset = "utf-8")
	: KWebObject<Script>(parent, TagName)
	{
		if (!sCharset.empty()) KHTMLNode::SetAttribute("charset", sCharset);
	}

	/// Convenience: construct + add a raw-text body.
	Script(KHTMLNode parent, KStringView sBody, KStringView sCharset)
	: KWebObject<Script>(parent, TagName)
	{
		if (!sCharset.empty()) KHTMLNode::SetAttribute("charset", sCharset);
		if (!sBody.empty())    AddRawText(sBody);
	}
}; // Script

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC StyleSheet : public KWebObject<StyleSheet>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "StyleSheet";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "style";

	StyleSheet(KHTMLNode parent)
	: KWebObject<StyleSheet>(parent, TagName) {}

	StyleSheet(KHTMLNode parent, KStringView sStyleContent)
	: KWebObject<StyleSheet>(parent, TagName)
	{
		if (!sStyleContent.empty()) AddRawText(sStyleContent);
	}
}; // StyleSheet

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC FavIcon : public KWebObject<FavIcon>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "FavIcon";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "link";

	FavIcon(KHTMLNode parent, KStringView sURL, KStringView sMIME = KStringView{})
	: KWebObject<FavIcon>(parent, TagName)
	{
		SetRel("icon");
		if (!sURL.empty())  SetLink(sURL);
		if (!sMIME.empty()) SetType(sMIME);
	}
}; // FavIcon

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Break : public KWebObject<Break>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Break";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "br";

	Break(KHTMLNode parent) : KWebObject<Break>(parent, TagName) {}
}; // Break

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC HorizontalRuler : public KWebObject<HorizontalRuler>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "HorizontalRuler";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "hr";

	HorizontalRuler(KHTMLNode parent) : KWebObject<HorizontalRuler>(parent, TagName) {}
}; // HorizontalRuler

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Header : public KWebObject<Header>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Header";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "header";

	Header(KHTMLNode parent, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Header>(parent, TagName, cls, sID) {}
}; // Header

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Heading : public KWebObject<Heading>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Heading";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Heading(KHTMLNode parent, uint16_t iLevel, KStringView sContent = KStringView{}, const Classes& cls = html::Classes{}, KStringView sID = KStringView{});
}; // Heading

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Image : public KWebObject<Image>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Image";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "img";

	Image(KHTMLNode parent,
	      KStringView sURL,
	      KStringView sDescription = KStringView{},
	      const Classes& cls = html::Classes{},
	      KStringView sID = KStringView{})
	: KWebObject<Image>(parent, TagName, cls, sID)
	{
		if (!sURL.empty())         SetSource(sURL);
		if (!sDescription.empty()) SetDescription(sDescription);
	}
}; // Image

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Form : public KWebObject<Form>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Form";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "form";

	using self = Form;

	Form(KHTMLNode parent,
	     KStringView sAction = KStringView{},
	     const Classes& cls = html::Classes{},
	     KStringView sID = KStringView{});

	self& SetAction    (KStringView sAction);
	self& SetEncType   (ENCTYPE enctype);
	self& SetMethod    (METHOD  method);
	self& SetNoValidate(bool    bYesNo = true);
}; // Form

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Legend : public KWebObject<Legend>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Legend";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "legend";

	Legend(KHTMLNode parent, KStringView sLegend, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Legend>(parent, TagName, cls, sID)
	{
		AddText(sLegend);
	}
}; // Legend

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC FieldSet : public KWebObject<FieldSet>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "FieldSet";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "fieldset";

	FieldSet(KHTMLNode parent, KStringView sLegend = KStringView{}, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<FieldSet>(parent, TagName, cls, sID)
	{
		if (!sLegend.empty()) this->template Add<Legend>(sLegend);
	}
}; // FieldSet

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Button : public KWebObject<Button>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Button";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "button";

	enum BUTTONTYPE { SUBMIT, RESET, BUTTON };

	using self = Button;

	Button(KHTMLNode parent, KStringView sLabel = KStringView{}, BUTTONTYPE type = SUBMIT,
	       const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Button>(parent, TagName, cls, sID)
	{
		SetType(type);
		if (!sLabel.empty()) AddText(sLabel);
	}

	self& SetType(BUTTONTYPE type);

	using KWebObject::SetName;
	using KWebObject::SetValue;
	using KWebObject::SetType;          // KString MIME version (used by buttons too)
}; // Button

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Output : public KWebObject<Output>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Output";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "output";

	Output(KHTMLNode parent,
	       KStringView sName,
	       KStringView sText  = KStringView{},
	       const Classes& cls = html::Classes{},
	       KStringView sID = KStringView{})
	: KWebObject<Output>(parent, TagName, cls, sID)
	{
		if (!sName.empty()) SetName(sName);
		if (!sText.empty()) AddText(sText);
	}
}; // Output

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Input : public KWebObject<Input>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Input";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "input";

	enum INPUTTYPE {
		CHECKBOX,
		COLOR,
		DATE,
		DATETIME_LOCAL,
		EMAIL,
		FILE,
		HIDDEN,
		IMAGE,
		MONTH,
		NUMBER,
		PASSWORD,
		RADIO,
		RANGE,
		SEARCH,
		SUBMIT,
		TEL,
		TEXT,
		TIME,
		URL,
		WEEK
	};

	using self = Input;

	Input(KHTMLNode parent,
	      KStringView sName  = KStringView{},
	      KStringView sValue = KStringView{},
	      INPUTTYPE   type   = TEXT,
	      const Classes& cls = html::Classes{},
	      KStringView sID = KStringView{});

	/// Convenience overload — type-first.
	Input(KHTMLNode parent,
	      INPUTTYPE type,
	      KStringView sName  = KStringView{},
	      const Classes& cls = html::Classes{},
	      KStringView sID = KStringView{})
	: Input(parent, sName, KStringView{}, type, cls, sID)
	{
	}

	self& SetType   (INPUTTYPE type);
	self& SetChecked(bool      bYesNo);
	self& SetStep   (float     step);

	using KWebObject::SetType;          // KString MIME version
}; // Input

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Option : public KWebObject<Option>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Option";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "option";

	using self = Option;

	Option(KHTMLNode parent,
	       KStringView sLabel,
	       KStringView sValue = KStringView{},
	       const Classes& cls = html::Classes{},
	       KStringView sID = KStringView{})
	: KWebObject<Option>(parent, TagName, cls, sID)
	{
		if (!sValue.empty()) KHTMLNode::SetAttribute("value", sValue);
		if (!sLabel.empty()) AddText(sLabel);
	}

	self& SetSelected(bool bYesNo)
	{
		SetBoolAttribute("selected", bYesNo);
		return *this;
	}
}; // Option

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Select : public KWebObject<Select>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Select";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "select";

	using self = Select;

	Select(KHTMLNode parent,
	       KStringView sName  = KStringView{},
	       uint16_t    iSize  = 1,
	       const Classes& cls = html::Classes{},
	       KStringView sID = KStringView{})
	: KWebObject<Select>(parent, TagName, cls, sID)
	{
		if (!sName.empty()) SetName(sName);
		SetSize(iSize);
	}

	using KWebObject::SetAutofocus;
	using KWebObject::SetDisabled;
	using KWebObject::SetMultiple;
	using KWebObject::SetName;
	using KWebObject::SetRequired;
	using KWebObject::SetSize;
}; // Select

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// CRTP wrapper that wraps `Base` (default `Input`) inside a `<label>`.
template<typename Derived, typename Base = Input>
class LabeledInput : public KWebObject<LabeledInput<Derived, Base>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "LabeledInput";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "label";

	using self = Derived;
	using wobj = KWebObjectBase;
	using parent_kwobj = KWebObject<LabeledInput<Derived, Base>>;

	template<typename... Args>
	LabeledInput(KHTMLNode parent, Args&&... args)
	: parent_kwobj(parent, TagName)
	, m_Base(KHTMLNode(static_cast<const KHTMLNode&>(*this)), std::forward<Args>(args)...)
	{
	}

	self& SetLabelBefore(KStringView sLabel) { wobj::SetTextBefore(sLabel); return This(); }
	self& SetLabelAfter (KStringView sLabel) { wobj::SetTextAfter (sLabel); return This(); }

	self& SetType        (Input::INPUTTYPE t) { m_Base.SetType        (t); return This(); }
	self& SetChecked     (bool b)             { m_Base.SetChecked     (b); return This(); }
	self& SetDescription (KStringView v)      { m_Base.SetDescription (v); return This(); }
	self& SetName        (KStringView v)      { m_Base.SetName        (v); return This(); }
	self& SetValue       (KStringView v)      { m_Base.SetValue       (v); return This(); }
	self& SetSource      (KStringView v)      { m_Base.SetSource      (v); return This(); }
	self& SetFormAction  (KStringView v)      { m_Base.SetFormAction  (v); return This(); }
	self& SetAutofocus   (bool b)             { m_Base.SetAutofocus   (b); return This(); }
	self& SetDisabled    (bool b)             { m_Base.SetDisabled    (b); return This(); }
	self& SetMultiple    (bool b)             { m_Base.SetMultiple    (b); return This(); }
	self& SetDirectory   (bool b)             { m_Base.SetDirectory   (b); return This(); }
	self& SetAccept      (KStringView v)      { m_Base.SetAccept      (v); return This(); }
	self& SetFormNoValidate(bool b)           { m_Base.SetFormNoValidate(b); return This(); }
	self& SetHeigth      (typename wobj::Pixels p) { m_Base.SetHeigth (p); return This(); }
	self& SetWidth       (typename wobj::Pixels p) { m_Base.SetWidth  (p); return This(); }
	self& SetSize        (typename wobj::Pixels p) { m_Base.SetSize   (p); return This(); }
	self& SetFormMethod  (typename wobj::METHOD  m){ m_Base.SetFormMethod (m); return This(); }
	self& SetFormEncType (typename wobj::ENCTYPE e){ m_Base.SetFormEncType(e); return This(); }
	self& SetFormTarget  (typename wobj::TARGET  t){ m_Base.SetFormTarget (t); return This(); }
	self& SetStyle       (KStringView v)      { m_Base.SetStyle       (v); return This(); }
	self& SetReadOnly    (bool b)             { m_Base.SetReadOnly    (b); return This(); }
	self& SetRequired    (bool b)             { m_Base.SetRequired    (b); return This(); }

	template<typename A> self& SetMin(A v)         { m_Base.SetMin(v); return This(); }
	template<typename A> self& SetMax(A v)         { m_Base.SetMax(v); return This(); }
	template<typename A> self& SetRange(A mn, A mx){ m_Base.SetRange(mn, mx); return This(); }

	self& SetStep        (float step)         { m_Base.SetStep        (step); return This(); }

	self& SetAttribute(KStringView sName, KStringView sValue)
	{
		m_Base.KHTMLNode::SetAttribute(sName, sValue);
		return This();
	}

	Base&       GetBase()       { return m_Base; }
	const Base& GetBase() const { return m_Base; }

//----------
protected:
//----------

	Base m_Base;   // 8-byte value handle to the child <input> / <select>

	Derived&       This()       { return static_cast<      Derived&>(*this); }
	const Derived& This() const { return static_cast<const Derived&>(*this); }

}; // LabeledInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename String>
class TextInput : public LabeledInput<TextInput<String>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "TextInput";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = TextInput<String>;
	using parent = LabeledInput<self>;

	TextInput(KHTMLNode where,
	          String&    rResult,
	          KStringView sName  = KStringView{},
	          const html::Classes& cls = html::Classes{},
	          KStringView sID = KStringView{});
}; // TextInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Arithmetic>
class NumericInput : public LabeledInput<NumericInput<Arithmetic>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static_assert(std::is_arithmetic<Arithmetic>::value, "Arithmetic type required");
	static constexpr KStringView s_sObjectName = "NumericInput";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = NumericInput<Arithmetic>;
	using parent = LabeledInput<self>;

	NumericInput(KHTMLNode where,
	             Arithmetic& rResult,
	             KStringView sName  = KStringView{},
	             const html::Classes& cls = html::Classes{},
	             KStringView sID = KStringView{});
}; // NumericInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Unit = std::chrono::seconds, typename Duration = std::chrono::high_resolution_clock::duration>
class DurationInput : public LabeledInput<DurationInput<Unit, Duration>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "DurationInput";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = DurationInput<Unit, Duration>;
	using parent = LabeledInput<self>;

	DurationInput(KHTMLNode where,
	              Duration&  rResult,
	              KStringView sName  = KStringView{},
	              const html::Classes& cls = html::Classes{},
	              KStringView sID = KStringView{});
}; // DurationInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename ValueType>
class RadioButton : public LabeledInput<RadioButton<ValueType>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "RadioButton";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = RadioButton<ValueType>;
	using parent = LabeledInput<self>;

	RadioButton(KHTMLNode where,
	            ValueType& rResult,
	            KStringView sName  = KStringView{},
	            const html::Classes& cls = html::Classes{},
	            KStringView sID = KStringView{});

	template<typename V>
	self& SetValue(V v)
	{
		this->m_Base.KHTMLNode::SetAttribute("value", kFormat("{}", v));
		return *this;
	}

	self& SetValue(KStringView v)
	{
		if (!v.empty()) this->m_Base.KHTMLNode::SetAttribute("value", v);
		return *this;
	}
}; // RadioButton

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Boolean>
class CheckBox : public LabeledInput<CheckBox<Boolean>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "CheckBox";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = CheckBox<Boolean>;
	using parent = LabeledInput<self>;

	CheckBox(KHTMLNode where,
	         Boolean&    rResult,
	         KStringView sName  = KStringView{},
	         const html::Classes& cls = html::Classes{},
	         KStringView sID = KStringView{});
}; // CheckBox

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename ValueType>
class Selection : public LabeledInput<Selection<ValueType>, Select>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Selection";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = Selection<ValueType>;
	using parent = LabeledInput<self, Select>;

	Selection(KHTMLNode where,
	          ValueType&  rResult,
	          KStringView sName  = KStringView{},
	          uint16_t    iSize  = 1,
	          const html::Classes& cls = html::Classes{},
	          KStringView sID = KStringView{});

	/// Add options as comma-separated string.
	self& SetOptions(KStringView sOptions);

	/// Add options from a container of stringable values.
	template<typename Container,
	         std::enable_if_t<!detail::is_str<Container>::value, int> = 0>
	self& SetOptions(const Container& list)
	{
		for (const auto& sItem : list)
		{
			AddOption(KStringView(sItem));
		}
		return *this;
	}

//----------
private:
//----------

	void AddOption(KStringView sLabel);

	ValueType* m_pResult { nullptr };  // captured for SetOptions to know which is "selected"
}; // Selection

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Helper that emits a text child onto a parent (entity-encoded). Provided
/// to allow `parent.Add<Text>(content)` for symmetry; equivalent to
/// `parent.AddText(content)`.
class DEKAF2_PUBLIC Text
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Text";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Text(KHTMLNode parent, KStringView sText)
	{
		if (!sText.empty()) parent.AddText(sText);
	}
}; // Text

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC RawText
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "RawText";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	RawText(KHTMLNode parent, KStringView sText)
	{
		if (!sText.empty()) parent.AddRawText(sText);
	}
}; // RawText

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC LineBreak
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "LineBreak";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	LineBreak(KHTMLNode parent)
	{
		parent.AddRawText("\r\n");
	}
}; // LineBreak

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Preformatted : public KWebObject<Preformatted>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Preformatted";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "pre";

	Preformatted(KHTMLNode parent, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Preformatted>(parent, TagName, cls, sID) {}
}; // Preformatted

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC IFrame : public KWebObject<IFrame>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "IFrame";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "iframe";

	IFrame(KHTMLNode parent, KStringView sURL = KStringView{}, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<IFrame>(parent, TagName, cls, sID)
	{
		if (!sURL.empty()) SetSource(sURL);
	}
}; // IFrame

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Video : public KWebObject<Video>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Video";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "video";

	Video(KHTMLNode parent, KStringView sURL = KStringView{}, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Video>(parent, TagName, cls, sID)
	{
		if (!sURL.empty()) SetSource(sURL);
	}
}; // Video

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Audio : public KWebObject<Audio>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Audio";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "audio";

	Audio(KHTMLNode parent, KStringView sURL = KStringView{}, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Audio>(parent, TagName, cls, sID)
	{
		if (!sURL.empty()) SetSource(sURL);
	}
}; // Audio

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Source : public KWebObject<Source>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Source";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "source";

	Source(KHTMLNode parent, KStringView sURL = KStringView{}, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Source>(parent, TagName, cls, sID)
	{
		if (!sURL.empty()) SetSource(sURL);
	}
}; // Source

} // end of namespace html

/// @}

DEKAF2_NAMESPACE_END

// Template implementations
#include <dekaf2/web/objects/bits/kwebobjects_templates.h>
