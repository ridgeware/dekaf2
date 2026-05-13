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

	// Universal HTML attributes — always public, applicable to every
	// element. The element-specific setters below are protected and
	// re-exposed selectively by each derived class via `using`-decls,
	// so an IDE's autocomplete on a concrete element type shows only
	// the setters that actually apply to it.
	self& SetDir       (DIR    dir    ) { KHTMLNode::SetAttribute("dir",     FromDir    (dir    )); return This(); }
	self& SetDraggable (bool   bYesNo ) { SetBoolAttribute       ("draggable",          bYesNo  ); return This(); }
	self& SetHidden    (bool   bYesNo ) { SetBoolAttribute       ("hidden",             bYesNo  ); return This(); }
	self& SetLanguage  (KStringView v ) { if (!v.empty()) KHTMLNode::SetAttribute("lang",  v);     return This(); }
	self& SetStyle     (KStringView v ) { if (!v.empty()) KHTMLNode::SetAttribute("style", v);     return This(); }
	self& SetTitle     (KStringView v ) { if (!v.empty()) KHTMLNode::SetAttribute("title", v);     return This(); }

	// -- generic attribute access (always public — escape hatch) --

	self& SetAttribute(KString sName, KStringView sValue, bool bRemoveIfEmpty = false, bool bDoNotEscape = false)
	{
		if (bRemoveIfEmpty && sValue.empty()) { KHTMLNode::RemoveAttribute(sName); }
		else                                  { KHTMLNode::SetAttribute(sName, sValue, '"', /*esc*/!bDoNotEscape); }
		return This();
	}

	/// Generic value setter — accepts anything `kFormat("{}", v)` can
	/// stringify (arithmetics, enums, std::chrono types, KDate, ...).
	/// Disabled for string-like types (those go to the KStringView
	/// overload) and for `bool` (that has its own boolean-attribute
	/// overload).
	template<typename T,
	         std::enable_if_t<!std::is_convertible<T, KStringView>::value
	                       && !std::is_same<typename std::remove_cv<typename std::remove_reference<T>::type>::type, bool>::value, int> = 0>
	self& SetAttribute(KString sName, const T& value)
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
protected:
//----------

	// Element-specific attribute setters. Protected so they don't pollute
	// the autocomplete surface of unrelated elements; each derived
	// class lifts the ones that apply via `using KWebObject::SetX;`.

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

	// shared by quotation elements (blockquote, q) and ins/del
	self& SetCite    (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("cite",     v); return This(); }
	// shared by details and dialog
	self& SetOpen    (bool b)        { SetBoolAttribute("open", b); return This(); }
	// shared by a (Link) and area — sets the filename or just emits bare
	self& SetDownload(KStringView v = KStringView{})
	{
		if (v.empty()) KHTMLNode::SetAttribute("download", KStringView{}, /*q*/0, /*esc*/false);
		else           KHTMLNode::SetAttribute("download", v);
		return This();
	}
	// shared by col and colgroup
	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetSpan(A v)               { KHTMLNode::SetAttribute("span", kFormat("{}", v)); return This(); }

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

// -- shape templates --------------------------------------------------------
// Three element "shapes" (container / text-content / void) parameterised by
// a tag struct. The tag struct provides two constexpr accessors:
//   static constexpr KStringView name();  // C++ class name — used for TYPE hash
//   static constexpr KStringView tag();   // HTML tag name — emitted in output
// Reduces ~50 trivial element classes to one-line `using`-aliases at the
// bottom of this file.

namespace detail {

template<typename Tag>
class DEKAF2_PUBLIC HtmlContainer : public KWebObject<HtmlContainer<Tag>>
{
public:
	static constexpr std::size_t TYPE    = Tag::name().Hash();
	static constexpr KStringView TagName = Tag::tag();

	HtmlContainer(KHTMLNode parent,
	              const html::Classes& cls = html::Classes{},
	              KStringView sID          = KStringView{})
	: KWebObject<HtmlContainer<Tag>>(parent, TagName, cls, sID) {}
};

template<typename Tag>
class DEKAF2_PUBLIC HtmlText : public KWebObject<HtmlText<Tag>>
{
public:
	static constexpr std::size_t TYPE    = Tag::name().Hash();
	static constexpr KStringView TagName = Tag::tag();

	HtmlText(KHTMLNode parent,
	         KStringView sContent     = KStringView{},
	         const html::Classes& cls = html::Classes{},
	         KStringView sID          = KStringView{})
	: KWebObject<HtmlText<Tag>>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) this->AddText(sContent); }
};

template<typename Tag>
class DEKAF2_PUBLIC HtmlVoid : public KWebObject<HtmlVoid<Tag>>
{
public:
	static constexpr std::size_t TYPE    = Tag::name().Hash();
	static constexpr KStringView TagName = Tag::tag();

	HtmlVoid(KHTMLNode parent)
	: KWebObject<HtmlVoid<Tag>>(parent, TagName) {}
};

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
// C++14 ODR-defs for the template statics — one set covers all instantiations.
template<typename Tag> constexpr KStringView HtmlContainer<Tag>::TagName;
template<typename Tag> constexpr std::size_t HtmlContainer<Tag>::TYPE;
template<typename Tag> constexpr KStringView HtmlText     <Tag>::TagName;
template<typename Tag> constexpr std::size_t HtmlText     <Tag>::TYPE;
template<typename Tag> constexpr KStringView HtmlVoid     <Tag>::TagName;
template<typename Tag> constexpr std::size_t HtmlVoid     <Tag>::TYPE;
#endif

} // namespace detail

// -- tag structs ------------------------------------------------------------
// Per-element marker structs that carry the HTML tag name and C++ class name.
// Grouped by section, mirroring the using-aliases throughout the file.
namespace tags {

// generic block / inline (pre-existing shapes refactored to the template)
struct Div               { static constexpr KStringView name() { return "Div";               } static constexpr KStringView tag() { return "div";        } };
struct Span              { static constexpr KStringView name() { return "Span";              } static constexpr KStringView tag() { return "span";       } };
struct Paragraph         { static constexpr KStringView name() { return "Paragraph";         } static constexpr KStringView tag() { return "p";          } };
struct Header            { static constexpr KStringView name() { return "Header";            } static constexpr KStringView tag() { return "header";     } };
struct Preformatted      { static constexpr KStringView name() { return "Preformatted";      } static constexpr KStringView tag() { return "pre";        } };
struct Break             { static constexpr KStringView name() { return "Break";             } static constexpr KStringView tag() { return "br";         } };
struct HorizontalRuler   { static constexpr KStringView name() { return "HorizontalRuler";   } static constexpr KStringView tag() { return "hr";         } };
struct Legend            { static constexpr KStringView name() { return "Legend";            } static constexpr KStringView tag() { return "legend";     } };
struct Table             { static constexpr KStringView name() { return "Table";             } static constexpr KStringView tag() { return "table";      } };
struct TableRow          { static constexpr KStringView name() { return "TableRow";          } static constexpr KStringView tag() { return "tr";         } };

// semantic structural
struct Footer            { static constexpr KStringView name() { return "Footer";            } static constexpr KStringView tag() { return "footer";     } };
struct Nav               { static constexpr KStringView name() { return "Nav";               } static constexpr KStringView tag() { return "nav";        } };
struct Main              { static constexpr KStringView name() { return "Main";              } static constexpr KStringView tag() { return "main";       } };
struct Article           { static constexpr KStringView name() { return "Article";           } static constexpr KStringView tag() { return "article";    } };
struct Section           { static constexpr KStringView name() { return "Section";           } static constexpr KStringView tag() { return "section";    } };
struct Aside             { static constexpr KStringView name() { return "Aside";             } static constexpr KStringView tag() { return "aside";      } };
struct Address           { static constexpr KStringView name() { return "Address";           } static constexpr KStringView tag() { return "address";    } };
struct Figure            { static constexpr KStringView name() { return "Figure";            } static constexpr KStringView tag() { return "figure";     } };
struct FigureCaption     { static constexpr KStringView name() { return "FigureCaption";     } static constexpr KStringView tag() { return "figcaption"; } };

// lists
struct UnorderedList     { static constexpr KStringView name() { return "UnorderedList";     } static constexpr KStringView tag() { return "ul";         } };
struct DescriptionList   { static constexpr KStringView name() { return "DescriptionList";   } static constexpr KStringView tag() { return "dl";         } };
struct DescriptionTerm   { static constexpr KStringView name() { return "DescriptionTerm";   } static constexpr KStringView tag() { return "dt";         } };
struct DescriptionDetail { static constexpr KStringView name() { return "DescriptionDetail"; } static constexpr KStringView tag() { return "dd";         } };

// table sub-structure
struct TableCaption      { static constexpr KStringView name() { return "TableCaption";      } static constexpr KStringView tag() { return "caption";    } };
struct TableHead         { static constexpr KStringView name() { return "TableHead";         } static constexpr KStringView tag() { return "thead";      } };
struct TableBody         { static constexpr KStringView name() { return "TableBody";         } static constexpr KStringView tag() { return "tbody";      } };
struct TableFoot         { static constexpr KStringView name() { return "TableFoot";         } static constexpr KStringView tag() { return "tfoot";      } };

// form
struct DataList          { static constexpr KStringView name() { return "DataList";          } static constexpr KStringView tag() { return "datalist";   } };

// interactive
struct Summary           { static constexpr KStringView name() { return "Summary";           } static constexpr KStringView tag() { return "summary";    } };

// media-ish
struct Picture           { static constexpr KStringView name() { return "Picture";           } static constexpr KStringView tag() { return "picture";    } };

// quotes / annotations
struct Abbreviation      { static constexpr KStringView name() { return "Abbreviation";      } static constexpr KStringView tag() { return "abbr";       } };
struct Citation          { static constexpr KStringView name() { return "Citation";          } static constexpr KStringView tag() { return "cite";       } };

// inline text decoration
struct Strong            { static constexpr KStringView name() { return "Strong";            } static constexpr KStringView tag() { return "strong";     } };
struct Emphasis          { static constexpr KStringView name() { return "Emphasis";          } static constexpr KStringView tag() { return "em";         } };
struct Bold              { static constexpr KStringView name() { return "Bold";              } static constexpr KStringView tag() { return "b";          } };
struct Italic            { static constexpr KStringView name() { return "Italic";            } static constexpr KStringView tag() { return "i";          } };
struct Underline         { static constexpr KStringView name() { return "Underline";         } static constexpr KStringView tag() { return "u";          } };
struct Strikethrough     { static constexpr KStringView name() { return "Strikethrough";     } static constexpr KStringView tag() { return "s";          } };
struct Small             { static constexpr KStringView name() { return "Small";             } static constexpr KStringView tag() { return "small";      } };
struct Subscript         { static constexpr KStringView name() { return "Subscript";         } static constexpr KStringView tag() { return "sub";        } };
struct Superscript       { static constexpr KStringView name() { return "Superscript";       } static constexpr KStringView tag() { return "sup";        } };
struct Mark              { static constexpr KStringView name() { return "Mark";              } static constexpr KStringView tag() { return "mark";       } };
struct Code              { static constexpr KStringView name() { return "Code";              } static constexpr KStringView tag() { return "code";       } };
struct Keyboard          { static constexpr KStringView name() { return "Keyboard";          } static constexpr KStringView tag() { return "kbd";        } };
struct Sample            { static constexpr KStringView name() { return "Sample";            } static constexpr KStringView tag() { return "samp";       } };
struct Variable          { static constexpr KStringView name() { return "Variable";          } static constexpr KStringView tag() { return "var";        } };

// niche
struct WordBreak         { static constexpr KStringView name() { return "WordBreak";         } static constexpr KStringView tag() { return "wbr";        } };
struct BiDirIsolate      { static constexpr KStringView name() { return "BiDirIsolate";      } static constexpr KStringView tag() { return "bdi";        } };
struct Ruby              { static constexpr KStringView name() { return "Ruby";              } static constexpr KStringView tag() { return "ruby";       } };
struct RubyParen         { static constexpr KStringView name() { return "RubyParen";         } static constexpr KStringView tag() { return "rp";         } };
struct RubyText          { static constexpr KStringView name() { return "RubyText";          } static constexpr KStringView tag() { return "rt";         } };

} // namespace tags

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generic free-name HTML element. The tag name is supplied at construction
/// time — use this when you need an element that doesn't have a dedicated
/// `KWebObject` subclass (e.g. `<label>`, `<aside>`, custom elements, ...).
/// The `<html>` document root is created by `html::Page`, not by this class.
class DEKAF2_PUBLIC Element : public KWebObject<Element>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Element";
//----------
public:
//----------
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	// no static TagName — the tag is dynamic, supplied per instance

	Element(KHTMLNode parent,
	        KStringView sTagName,
	        const Classes& cls = html::Classes{},
	        KStringView sID    = KStringView{})
	: KWebObject<Element>(parent, sTagName, cls, sID)
	{
	}

}; // Element

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

// Div, Span, Paragraph, Table, TableRow — pure container shape.
using Div       = detail::HtmlContainer<tags::Div>;
using Span      = detail::HtmlContainer<tags::Span>;
using Paragraph = detail::HtmlContainer<tags::Paragraph>;
using Table     = detail::HtmlContainer<tags::Table>;
using TableRow  = detail::HtmlContainer<tags::TableRow>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC TableData : public KWebObject<TableData>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "TableData";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "td";

	using KWebObject::SetColSpan;
	using KWebObject::SetRowSpan;
	using KWebObject::SetAlign;
	using KWebObject::SetVAlign;

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

	using KWebObject::SetColSpan;
	using KWebObject::SetRowSpan;
	using KWebObject::SetAlign;
	using KWebObject::SetVAlign;

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

	using KWebObject::SetLink;
	using KWebObject::SetTarget;
	using KWebObject::SetRel;
	using KWebObject::SetDownload;

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

	// element-specific setters (lifted from KWebObject's protected pool)
	using KWebObject::SetAsync;
	using KWebObject::SetDefer;
	using KWebObject::SetSource;
	using KWebObject::SetType;

	Script(KHTMLNode parent,
	       KStringView sBody    = KStringView{},
	       KStringView sCharset = "utf-8")
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

	using KWebObject::SetLink;

	StyleSheet(KHTMLNode parent, KStringView sStyleContent = KStringView{})
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

	using KWebObject::SetLink;

	FavIcon(KHTMLNode parent,
	        KStringView sURL,
	        KStringView sMIME  = KStringView{},
	        const Classes& cls = html::Classes{},
	        KStringView sID    = KStringView{})
	: KWebObject<FavIcon>(parent, TagName, cls, sID)
	{
		SetRel("icon");
		if (!sURL.empty())  SetLink(sURL);
		if (!sMIME.empty()) SetType(sMIME);
	}
}; // FavIcon

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// `<meta>` element. Covers all four meta variants (name/content,
/// http-equiv, charset, property/Open-Graph) via the respective setters.
/// The default ctor signature matches the most common form:
///   `head.Add<html::Meta>("description", "...");`
class DEKAF2_PUBLIC Meta : public KWebObject<Meta>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Meta";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "meta";

	using KWebObject::SetName;          // name="..."

	Meta(KHTMLNode parent,
	     KStringView sName    = KStringView{},
	     KStringView sContent = KStringView{},
	     const Classes& cls   = html::Classes{},
	     KStringView sID      = KStringView{})
	: KWebObject<Meta>(parent, TagName, cls, sID)
	{
		if (!sName.empty())    SetName(sName);
		if (!sContent.empty()) SetContent(sContent);
	}

	/// Value half of `<meta name="..." content="...">`.
	self& SetContent  (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("content",    v); return *this; }
	/// `<meta http-equiv="..." content="...">` — emit an HTTP-equivalent header.
	self& SetHttpEquiv(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("http-equiv", v); return *this; }
	/// `<meta charset="...">` — document character set.
	self& SetCharset  (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("charset",    v); return *this; }
	/// `<meta property="og:..." content="...">` — Open Graph / RDFa property.
	self& SetProperty (KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("property",   v); return *this; }
}; // Meta

// Break, HorizontalRuler — void shape. Header — container shape.
using Break           = detail::HtmlVoid     <tags::Break>;
using HorizontalRuler = detail::HtmlVoid     <tags::HorizontalRuler>;
using Header          = detail::HtmlContainer<tags::Header>;

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

	using KWebObject::SetSource;
	using KWebObject::SetDescription;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;
	using KWebObject::SetLoading;

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
	using KWebObject::SetName;
	using KWebObject::SetTarget;

	Form(KHTMLNode parent,
	     KStringView sAction = KStringView{},
	     const Classes& cls = html::Classes{},
	     KStringView sID = KStringView{});

	self& SetAction    (KStringView sAction);
	self& SetEncType   (ENCTYPE enctype);
	self& SetMethod    (METHOD  method);
	self& SetNoValidate(bool    bYesNo = true);
}; // Form

// Legend — text-content shape. NOTE: the legacy ctor required sLegend; the
// template-form makes it optional (matching the rest of the *Text aliases),
// which is purely additive.
using Legend = detail::HtmlText<tags::Legend>;

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

	using KWebObject::SetDisabled;
	using KWebObject::SetFormAction;
	using KWebObject::SetFormMethod;
	using KWebObject::SetFormEncType;
	using KWebObject::SetFormTarget;
	using KWebObject::SetName;
	using KWebObject::SetValue;
	using KWebObject::SetPlaceholder;
	using KWebObject::SetReadOnly;
	using KWebObject::SetRequired;
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

	using KWebObject::SetFor;
	using KWebObject::SetForm;
	using KWebObject::SetName;

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

	self& SetType   (INPUTTYPE type);
	self& SetChecked(bool      bYesNo);
	self& SetStep   (float     step);

	using KWebObject::SetType;          // KString MIME version
	using KWebObject::SetDescription;
	using KWebObject::SetAutofocus;
	using KWebObject::SetDisabled;
	using KWebObject::SetFormAction;
	using KWebObject::SetFormMethod;
	using KWebObject::SetFormEncType;
	using KWebObject::SetFormNoValidate;
	using KWebObject::SetFormTarget;
	using KWebObject::SetHeigth;
	using KWebObject::SetWidth;
	using KWebObject::SetMultiple;
	using KWebObject::SetDirectory;
	using KWebObject::SetAccept;
	using KWebObject::SetName;
	using KWebObject::SetValue;
	using KWebObject::SetSize;
	using KWebObject::SetSource;
	using KWebObject::SetMin;
	using KWebObject::SetMax;
	using KWebObject::SetRange;
	using KWebObject::SetReadOnly;
	using KWebObject::SetRequired;
	using KWebObject::SetPlaceholder;
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
	using KWebObject::SetDisabled;
	using KWebObject::SetLabel;
	using KWebObject::SetValue;

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
	using KWebObject::SetAutofocus;
	using KWebObject::SetDisabled;
	using KWebObject::SetMultiple;
	using KWebObject::SetName;
	using KWebObject::SetRequired;
	using KWebObject::SetSize;

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

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
template<typename Derived, typename Base>
constexpr KStringView LabeledInput<Derived, Base>::TagName;
#endif

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename String>
class TextInput : public LabeledInput<TextInput<String>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "TextInput";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = TextInput<String>;
	using base   = LabeledInput<self>;

	TextInput(KHTMLNode parent,
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
	using base   = LabeledInput<self>;

	NumericInput(KHTMLNode parent,
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
	using base   = LabeledInput<self>;

	DurationInput(KHTMLNode parent,
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
	using base   = LabeledInput<self>;

	RadioButton(KHTMLNode parent,
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
	using base   = LabeledInput<self>;

	CheckBox(KHTMLNode parent,
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
	using base   = LabeledInput<self, Select>;

	Selection(KHTMLNode parent,
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

// Preformatted — container shape.
using Preformatted = detail::HtmlContainer<tags::Preformatted>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC IFrame : public KWebObject<IFrame>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "IFrame";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "iframe";

	using KWebObject::SetSource;
	using KWebObject::SetLoading;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;
	using KWebObject::SetAllow;
	using KWebObject::SetAllowFullscreen;
	using KWebObject::SetScrolling;

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

	using KWebObject::SetSource;
	using KWebObject::SetPoster;
	using KWebObject::SetPreload;
	using KWebObject::SetAutoplay;
	using KWebObject::SetControls;
	using KWebObject::SetLoop;
	using KWebObject::SetPlaysInline;
	using KWebObject::SetMuted;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;

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

	using KWebObject::SetSource;
	using KWebObject::SetAutoplay;
	using KWebObject::SetPreload;
	using KWebObject::SetControls;
	using KWebObject::SetLoop;
	using KWebObject::SetMuted;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;

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

	using KWebObject::SetSource;
	using KWebObject::SetType;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;

	Source(KHTMLNode parent, KStringView sURL = KStringView{}, const Classes& cls = html::Classes{}, KStringView sID = KStringView{})
	: KWebObject<Source>(parent, TagName, cls, sID)
	{
		if (!sURL.empty()) SetSource(sURL);
	}
}; // Source

// =============================================================================
// Additional elements — added in bulk. Each class follows one of three shapes:
//   (a) content-with-text: ctor takes leading KStringView sContent
//   (b) container:         ctor takes only (parent, cls, sID)
//   (c) void:              no content, no children
// Element-specific setters are defined inline; shared setters are lifted from
// KWebObject's protected pool via using-decls.
// =============================================================================

// -- head: <base> -----------------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Base : public KWebObject<Base>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Base";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "base";

	using KWebObject::SetLink;
	using KWebObject::SetTarget;

	Base(KHTMLNode parent,
	     KStringView sURL   = KStringView{},
	     const Classes& cls = html::Classes{},
	     KStringView sID    = KStringView{})
	: KWebObject<Base>(parent, TagName, cls, sID)
	{
		if (!sURL.empty()) SetLink(sURL);
	}
}; // Base

using Footer = detail::HtmlContainer<tags::Footer>;
using Nav = detail::HtmlContainer<tags::Nav>;
using Main = detail::HtmlContainer<tags::Main>;
using Article = detail::HtmlContainer<tags::Article>;
using Section = detail::HtmlContainer<tags::Section>;
using Aside = detail::HtmlContainer<tags::Aside>;
using Address = detail::HtmlContainer<tags::Address>;
using Figure = detail::HtmlContainer<tags::Figure>;
using FigureCaption = detail::HtmlText<tags::FigureCaption>;

// -- lists ------------------------------------------------------------------

using UnorderedList = detail::HtmlContainer<tags::UnorderedList>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC OrderedList : public KWebObject<OrderedList>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "OrderedList";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "ol";

	using KWebObject::SetType;     // "1", "A", "a", "I", "i"

	OrderedList(KHTMLNode parent,
	            const Classes& cls = html::Classes{},
	            KStringView sID    = KStringView{})
	: KWebObject<OrderedList>(parent, TagName, cls, sID) {}

	self& SetReversed(bool b) { SetBoolAttribute("reversed", b); return *this; }
	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetStart(A v)       { KHTMLNode::SetAttribute("start", kFormat("{}", v)); return *this; }
}; // OrderedList

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC ListItem : public KWebObject<ListItem>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "ListItem";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "li";

	using KWebObject::SetValue;    // for <li> inside <ol>

	ListItem(KHTMLNode parent,
	         KStringView sContent = KStringView{},
	         const Classes& cls   = html::Classes{},
	         KStringView sID      = KStringView{})
	: KWebObject<ListItem>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) AddText(sContent); }
}; // ListItem

using DescriptionList = detail::HtmlContainer<tags::DescriptionList>;
using DescriptionTerm = detail::HtmlText<tags::DescriptionTerm>;
using DescriptionDetail = detail::HtmlText<tags::DescriptionDetail>;

// -- table sub-structure ----------------------------------------------------

using TableCaption = detail::HtmlText<tags::TableCaption>;
using TableHead = detail::HtmlContainer<tags::TableHead>;
using TableBody = detail::HtmlContainer<tags::TableBody>;
using TableFoot = detail::HtmlContainer<tags::TableFoot>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC ColumnGroup : public KWebObject<ColumnGroup>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "ColumnGroup";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "colgroup";

	using KWebObject::SetSpan;

	ColumnGroup(KHTMLNode parent,
	            const Classes& cls = html::Classes{},
	            KStringView sID    = KStringView{})
	: KWebObject<ColumnGroup>(parent, TagName, cls, sID) {}
}; // ColumnGroup

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Column : public KWebObject<Column>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Column";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "col";

	using KWebObject::SetSpan;

	Column(KHTMLNode parent,
	       const Classes& cls = html::Classes{},
	       KStringView sID    = KStringView{})
	: KWebObject<Column>(parent, TagName, cls, sID) {}
}; // Column

// -- form ------------------------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC TextArea : public KWebObject<TextArea>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "TextArea";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "textarea";

	enum WRAP { SOFT, HARD };

	using KWebObject::SetName;
	using KWebObject::SetPlaceholder;
	using KWebObject::SetReadOnly;
	using KWebObject::SetRequired;
	using KWebObject::SetDisabled;
	using KWebObject::SetAutofocus;
	using KWebObject::SetForm;

	TextArea(KHTMLNode parent,
	         KStringView sName    = KStringView{},
	         KStringView sContent = KStringView{},
	         const Classes& cls   = html::Classes{},
	         KStringView sID      = KStringView{})
	: KWebObject<TextArea>(parent, TagName, cls, sID)
	{
		if (!sName.empty())    SetName(sName);
		if (!sContent.empty()) AddText(sContent);
	}

	self& SetRows     (uint16_t r) { KHTMLNode::SetAttribute("rows",      kFormat("{}", r)); return *this; }
	self& SetCols     (uint16_t c) { KHTMLNode::SetAttribute("cols",      kFormat("{}", c)); return *this; }
	self& SetMaxLength(uint32_t n) { KHTMLNode::SetAttribute("maxlength", kFormat("{}", n)); return *this; }
	self& SetMinLength(uint32_t n) { KHTMLNode::SetAttribute("minlength", kFormat("{}", n)); return *this; }
	self& SetWrap     (WRAP w)     { KHTMLNode::SetAttribute("wrap", w == HARD ? KStringView("hard") : KStringView("soft")); return *this; }
}; // TextArea

using DataList = detail::HtmlContainer<tags::DataList>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC OptionGroup : public KWebObject<OptionGroup>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "OptionGroup";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "optgroup";

	using KWebObject::SetLabel;
	using KWebObject::SetDisabled;

	OptionGroup(KHTMLNode parent,
	            KStringView sLabel = KStringView{},
	            const Classes& cls = html::Classes{},
	            KStringView sID    = KStringView{})
	: KWebObject<OptionGroup>(parent, TagName, cls, sID)
	{
		if (!sLabel.empty()) SetLabel(sLabel);
	}
}; // OptionGroup

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Meter : public KWebObject<Meter>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Meter";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "meter";

	using KWebObject::SetMin;
	using KWebObject::SetMax;
	using KWebObject::SetRange;
	using KWebObject::SetValue;
	using KWebObject::SetForm;

	Meter(KHTMLNode parent,
	      KStringView sValue = KStringView{},
	      const Classes& cls = html::Classes{},
	      KStringView sID    = KStringView{})
	: KWebObject<Meter>(parent, TagName, cls, sID)
	{
		if (!sValue.empty()) SetValue(sValue);
	}

	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetLow    (A v) { KHTMLNode::SetAttribute("low",     kFormat("{}", v)); return *this; }
	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetHigh   (A v) { KHTMLNode::SetAttribute("high",    kFormat("{}", v)); return *this; }
	template<typename A, std::enable_if_t<std::is_arithmetic<A>::value, int> = 0>
	self& SetOptimum(A v) { KHTMLNode::SetAttribute("optimum", kFormat("{}", v)); return *this; }
}; // Meter

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Progress : public KWebObject<Progress>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Progress";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "progress";

	using KWebObject::SetMax;
	using KWebObject::SetValue;

	Progress(KHTMLNode parent,
	         KStringView sValue = KStringView{},
	         const Classes& cls = html::Classes{},
	         KStringView sID    = KStringView{})
	: KWebObject<Progress>(parent, TagName, cls, sID)
	{
		if (!sValue.empty()) SetValue(sValue);
	}
}; // Progress

// -- interactive ------------------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Details : public KWebObject<Details>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Details";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "details";

	using KWebObject::SetOpen;

	Details(KHTMLNode parent,
	        const Classes& cls = html::Classes{},
	        KStringView sID    = KStringView{})
	: KWebObject<Details>(parent, TagName, cls, sID) {}
}; // Details

using Summary = detail::HtmlText<tags::Summary>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Dialog : public KWebObject<Dialog>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Dialog";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "dialog";

	using KWebObject::SetOpen;

	Dialog(KHTMLNode parent,
	       const Classes& cls = html::Classes{},
	       KStringView sID    = KStringView{})
	: KWebObject<Dialog>(parent, TagName, cls, sID) {}
}; // Dialog

// -- media-ish --------------------------------------------------------------

using Picture = detail::HtmlContainer<tags::Picture>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Canvas : public KWebObject<Canvas>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Canvas";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "canvas";

	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;

	Canvas(KHTMLNode parent,
	       const Classes& cls = html::Classes{},
	       KStringView sID    = KStringView{})
	: KWebObject<Canvas>(parent, TagName, cls, sID) {}
}; // Canvas

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Time : public KWebObject<Time>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Time";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "time";

	Time(KHTMLNode parent,
	     KStringView sContent = KStringView{},
	     const Classes& cls   = html::Classes{},
	     KStringView sID      = KStringView{})
	: KWebObject<Time>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) AddText(sContent); }

	self& SetDateTime(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("datetime", v); return *this; }
}; // Time

// -- quotes / annotations ---------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC BlockQuote : public KWebObject<BlockQuote>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "BlockQuote";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "blockquote";

	using KWebObject::SetCite;

	BlockQuote(KHTMLNode parent,
	           KStringView sContent = KStringView{},
	           const Classes& cls   = html::Classes{},
	           KStringView sID      = KStringView{})
	: KWebObject<BlockQuote>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) AddText(sContent); }
}; // BlockQuote

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC InlineQuote : public KWebObject<InlineQuote>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "InlineQuote";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "q";

	using KWebObject::SetCite;

	InlineQuote(KHTMLNode parent,
	            KStringView sContent = KStringView{},
	            const Classes& cls   = html::Classes{},
	            KStringView sID      = KStringView{})
	: KWebObject<InlineQuote>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) AddText(sContent); }
}; // InlineQuote

using Abbreviation = detail::HtmlText<tags::Abbreviation>;
using Citation = detail::HtmlText<tags::Citation>;

// -- inline text decoration -------------------------------------------------

using Strong = detail::HtmlText<tags::Strong>;
using Emphasis = detail::HtmlText<tags::Emphasis>;
using Bold = detail::HtmlText<tags::Bold>;
using Italic = detail::HtmlText<tags::Italic>;
using Underline = detail::HtmlText<tags::Underline>;
using Strikethrough = detail::HtmlText<tags::Strikethrough>;
using Small = detail::HtmlText<tags::Small>;
using Subscript = detail::HtmlText<tags::Subscript>;
using Superscript = detail::HtmlText<tags::Superscript>;
using Mark = detail::HtmlText<tags::Mark>;
using Code = detail::HtmlText<tags::Code>;
using Keyboard = detail::HtmlText<tags::Keyboard>;
using Sample = detail::HtmlText<tags::Sample>;
using Variable = detail::HtmlText<tags::Variable>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Deleted : public KWebObject<Deleted>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Deleted";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "del";

	using KWebObject::SetCite;

	Deleted(KHTMLNode parent,
	        KStringView sContent = KStringView{},
	        const Classes& cls   = html::Classes{},
	        KStringView sID      = KStringView{})
	: KWebObject<Deleted>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) AddText(sContent); }

	self& SetDateTime(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("datetime", v); return *this; }
}; // Deleted

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Inserted : public KWebObject<Inserted>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Inserted";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "ins";

	using KWebObject::SetCite;

	Inserted(KHTMLNode parent,
	         KStringView sContent = KStringView{},
	         const Classes& cls   = html::Classes{},
	         KStringView sID      = KStringView{})
	: KWebObject<Inserted>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) AddText(sContent); }

	self& SetDateTime(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("datetime", v); return *this; }
}; // Inserted

// -- niche / specialised ----------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Map : public KWebObject<Map>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Map";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "map";

	using KWebObject::SetName;

	Map(KHTMLNode parent,
	    KStringView sName  = KStringView{},
	    const Classes& cls = html::Classes{},
	    KStringView sID    = KStringView{})
	: KWebObject<Map>(parent, TagName, cls, sID)
	{
		if (!sName.empty()) SetName(sName);
	}
}; // Map

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Area : public KWebObject<Area>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Area";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "area";

	enum SHAPE { DEFAULT, RECT, CIRCLE, POLY };

	using KWebObject::SetLink;
	using KWebObject::SetTarget;
	using KWebObject::SetRel;
	using KWebObject::SetDescription;
	using KWebObject::SetDownload;
	using KWebObject::SetType;

	Area(KHTMLNode parent,
	     const Classes& cls = html::Classes{},
	     KStringView sID    = KStringView{})
	: KWebObject<Area>(parent, TagName, cls, sID) {}

	self& SetCoords(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("coords", v); return *this; }
	self& SetShape (SHAPE s)
	{
		KStringView sV;
		switch (s) { case DEFAULT: sV = "default"; break; case RECT: sV = "rect"; break;
		             case CIRCLE:  sV = "circle";  break; case POLY: sV = "poly"; break; }
		KHTMLNode::SetAttribute("shape", sV);
		return *this;
	}
}; // Area

using WordBreak = detail::HtmlVoid<tags::WordBreak>;
using BiDirIsolate = detail::HtmlText<tags::BiDirIsolate>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC BiDirOverride : public KWebObject<BiDirOverride>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "BiDirOverride";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "bdo";

	// SetDir is universal (public in KWebObject), so it is already visible.
	BiDirOverride(KHTMLNode parent,
	              KStringView sContent = KStringView{},
	              const Classes& cls   = html::Classes{},
	              KStringView sID      = KStringView{})
	: KWebObject<BiDirOverride>(parent, TagName, cls, sID)
	{ if (!sContent.empty()) AddText(sContent); }
}; // BiDirOverride

using Ruby = detail::HtmlContainer<tags::Ruby>;
using RubyParen = detail::HtmlText<tags::RubyParen>;
using RubyText = detail::HtmlText<tags::RubyText>;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Embed : public KWebObject<Embed>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Embed";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "embed";

	using KWebObject::SetSource;
	using KWebObject::SetType;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;

	Embed(KHTMLNode parent,
	      KStringView sURL   = KStringView{},
	      const Classes& cls = html::Classes{},
	      KStringView sID    = KStringView{})
	: KWebObject<Embed>(parent, TagName, cls, sID)
	{
		if (!sURL.empty()) SetSource(sURL);
	}
}; // Embed

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Object : public KWebObject<Object>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Object";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "object";

	using KWebObject::SetName;
	using KWebObject::SetType;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;
	using KWebObject::SetForm;

	Object(KHTMLNode parent,
	       KStringView sData  = KStringView{},
	       const Classes& cls = html::Classes{},
	       KStringView sID    = KStringView{})
	: KWebObject<Object>(parent, TagName, cls, sID)
	{
		if (!sData.empty()) KHTMLNode::SetAttribute("data", sData);
	}

	self& SetData(KStringView v) { if (!v.empty()) KHTMLNode::SetAttribute("data", v); return *this; }
}; // Object

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC Param : public KWebObject<Param>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Param";
public:
	static constexpr std::size_t TYPE = s_sObjectName.Hash();
	static constexpr KStringView TagName = "param";

	using KWebObject::SetName;
	using KWebObject::SetValue;

	Param(KHTMLNode parent,
	      KStringView sName  = KStringView{},
	      KStringView sValue = KStringView{},
	      const Classes& cls = html::Classes{},
	      KStringView sID    = KStringView{})
	: KWebObject<Param>(parent, TagName, cls, sID)
	{
		if (!sName.empty())  SetName(sName);
		if (!sValue.empty()) SetValue(sValue);
	}
}; // Param

} // end of namespace html

/// @}

DEKAF2_NAMESPACE_END

// Template implementations
#include <dekaf2/web/objects/bits/kwebobjects_templates.h>
