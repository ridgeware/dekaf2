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

#include "kstring.h"
#include "kstringview.h"
#include "kmime.h"
#include "khtmldom.h"
#include "kurl.h"
#include <vector>
#include <limits>
#include <chrono>
#include <unordered_map>

namespace dekaf2 {
namespace html {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Class
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

protected:
//----------

	KString m_sClassName;
	KString m_sClassDefinition;

}; // Class

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A collection of class names to add to an element's class attribute
class Classes
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
		return Add(m_sClassNames.ToView());
	}

	self& Add(KStringView sClassName);

	const KString& GetClasses() const { return m_sClassNames; }

//----------
protected:
//----------

	KString m_sClassNames;

}; // KHTMLClass

} // end of namespace html

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KWebObjectBase : public KHTMLElement
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KWebObjectBase";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	enum ENCTYPE { URLENCODED, FORMDATA, PLAIN };
	enum METHOD  { GET, POST, DIALOG };
	enum LOADING { AUTO, EAGER, LAZY };
	enum TARGET  { SELF, BLANK, PARENT, TOP };
	enum DIR     { LTR, RTL };

	using Pixels = uint32_t;

	using KHTMLElement::KHTMLElement;

	KWebObjectBase() = default;
	KWebObjectBase(KString sElement, KStringView sID = KStringView{})
	: KHTMLElement(std::move(sElement), sID, KStringView{})
	{
	}
	KWebObjectBase(KString sElement, KStringView sID, const html::Classes& Classes)
	: KHTMLElement(std::move(sElement), sID, Classes.GetClasses())
	{
	}
	KWebObjectBase(KString sElement, KStringView sID, KStringView sClass) = delete;

	/// Traverse all objects and call Sync(KStringView) with the query parm value if it has a "name" attribute that matches a query parm name
	void Synchronize(const url::KQueryParms& QueryParms)
	{
		TraverseAndSync(this, nullptr, nullptr, QueryParms);
	}

	/// Generate missing names of input fields, group radio buttons automatically by variable reference
	void Generate();

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const = 0;

//----------
protected:
//----------

	static KStringView FromMethod  (METHOD  method  );
	static KStringView FromTarget  (TARGET  target  );
	static KStringView FromDir     (DIR     dir     );
	static KStringView FromLoading (LOADING loading );
	static KStringView FromEncType (ENCTYPE encoding);

	virtual void Reset(KWebObjectBase* Element);
	virtual void Sync(KWebObjectBase* Element, KStringView sValue);
	virtual void* AddressOfInputStorage();

	void TraverseAndSync(KWebObjectBase* Element,
						 KWebObjectBase* LastLabeledInput,
						 KWebObjectBase* LastSelect,
						 const url::KQueryParms& QueryParms);

	using namemap = std::unordered_map<void*, KStringView>;

	void    Generate(KWebObjectBase* Element,
					 KWebObjectBase* LastLabeledInput,
					 std::size_t& iNameCounter,
					 namemap& NameMap);
	KString GenerateName(KStringView sPrefix, std::size_t iNameCounter);


	void SetTextBefore(KStringView sLabel);
	void SetTextAfter(KStringView sLabel);

}; // KWebObjectBase

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// CRTP web object
template<typename Derived>
class KWebObject : public KWebObjectBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using KWebObjectBase::KWebObjectBase;
	using self = Derived;

	/// Append an element to the list of children, return parent lvalue reference. Use Add() instead to return child reference
	template<typename Element,
	typename std::enable_if<std::is_base_of<KHTMLObject, Element>::value == true, int>::type = 0>
	Derived& Append(Element Object) &
	{
		Add(std::move(Object));
		return This();
	}

	/// Append an element to the list of children, return parent rvalue reference. Use Add() instead to return child reference
	template<typename Element,
	typename std::enable_if<std::is_base_of<KHTMLObject, Element>::value == true, int>::type = 0>
	Derived&& Append(Element Object) &&
	{
		return std::move(Append(std::move(Object)));
	}

	// **** overrides for similar methods in KHTMLElement ****
	virtual std::unique_ptr<KHTMLObject> Clone() const override
	{
		return std::make_unique<Derived>(This());
	}

	self& AddText(KStringView sContent) &
	{
		KWebObjectBase::AddText(sContent);
		return This();
	}

	self&& AddText(KStringView sContent) &&
	{
		return std::move(AddText(sContent));
	}

	self& AddRawText(KStringView sContent) &
	{
		KWebObjectBase::AddRawText(sContent);
		return This();
	}

	self&& AddRawText(KStringView sContent) &&
	{
		return std::move(AddRawText(sContent));
	}

	self& SetID(KString sValue) &
	{
		KHTMLElement::SetID(std::move(sValue));
		return This();
	}

	self&& SetID(KString sValue) &&
	{
		return std::move(SetID(sValue));
	}

	self& SetClass(const html::Classes& Classes) &
	{
		KHTMLElement::SetClass(Classes.GetClasses());
		return This();
	}

	self&& SetClass(const html::Classes& Classes) &&
	{
		return std::move(SetClass(Classes));
	}
	// ^^^^ KHTMLElement overrides until here ^^^^

	//  **** universal attributes ****
	self& SetDir(DIR dir) &
	{
		SetAttribute("dir", FromDir(dir), false);
		return This();
	}

	self&& SetDir(DIR dir) &&
	{
		return std::move(SetDir(dir));
	}

	self& SetDraggable(bool bYesNo) &
	{
		SetAttribute("draggable", bYesNo);
		return This();
	}

	self&& SetDraggable(bool bYesNo) &&
	{
		return std::move(SetDraggable(bYesNo));
	}

	self& SetHidden(bool bYesNo) &
	{
		SetAttribute("hidden", bYesNo);
		return This();
	}

	self&& SetHidden(bool bYesNo) &&
	{
		return std::move(SetHidden(bYesNo));
	}

	self& SetLanguage(KStringView sLanguage) &
	{
		SetAttribute("lang", sLanguage, true);
		return This();
	}

	self&& SetLanguage(KStringView sLanguage) &&
	{
		return std::move(SetLanguage(sLanguage));
	}

	self& SetStyle(KStringView sStyle) &
	{
		SetAttribute("style", sStyle, true);
		return This();
	}

	self&& SetStyle(KStringView sStyle) &&
	{
		return std::move(SetStyle(sStyle));
	}

	self& SetTitle(KStringView sTitle) &
	{
		SetAttribute("title", sTitle, true);
		return This();
	}

	self&& SetTitle(KStringView sTitle) &&
	{
		return std::move(SetTitle(sTitle));
	}
	// ^^^^ universal attributes until here ^^^^

//----------
protected:
//----------

	// do not allow an instance of this class without child
	KWebObject() = default;
	KWebObject(const KWebObject&) = default;
	KWebObject(KWebObject&&) = default;

	self& SetSource(KStringView sSource) &
	{
		SetAttribute("src", sSource, true);
		return This();
	}

	self&& SetSource(KStringView sSource) &&
	{
		return std::move(SetSource(sSource));
	}

	self& SetDescription(KStringView sDescription) &
	{
		SetAttribute("alt", sDescription, true);
		return This();
	}

	self&& SetDescription(KStringView sDescription) &&
	{
		return std::move(SetDescription(sDescription));
	}

	self& SetHeigth(Pixels iHeigth) &
	{
		SetAttribute("heigth", iHeigth);
		return This();
	}

	self&& SetHeigth(Pixels iHeigth) &&
	{
		return std::move(SetHeigth(iHeigth));
	}

	self& SetWidth(Pixels iWidth) &
	{
		SetAttribute("width", iWidth);
		return This();
	}

	self&& SetWidth(Pixels iWidth) &&
	{
		return std::move(SetWidth(iWidth));
	}

	self& SetSize(Pixels iSize) &
	{
		SetAttribute("size", iSize);
		return This();
	}

	self&& SetSize(Pixels iSize) &&
	{
		return std::move(SetSize(iSize));
	}

	self& SetLoading(LOADING loading) &
	{
		SetAttribute("loading", FromLoading(loading), false);
		return This();
	}

	self&& SetLoading(LOADING loading) &&
	{
		return std::move(SetLoading(loading));
	}

	self& SetName(KStringView sName) &
	{
		SetAttribute("name", sName, true);
		return This();
	}

	self&& SetName(KStringView sName) &&
	{
		return std::move(SetName(sName));
	}

	self& SetValue(KStringView sValue) &
	{
		SetAttribute("value", sValue, true);
		return This();
	}

	self&& SetValue(KStringView sValue) &&
	{
		return std::move(SetValue(sValue));
	}

	self& SetTarget(TARGET target) &
	{
		SetAttribute("target", FromTarget(target), false);
		return This();
	}

	self&& SetTarget(TARGET target) &&
	{
		return std::move(SetTarget(target));
	}

	self& SetLink(KStringView sURL) &
	{
		SetAttribute("href", sURL, true);
		return This();
	}

	self&& SetLink(KStringView sURL) &&
	{
		return std::move(SetLink(sURL));
	}

	self& SetRel(KStringView sRel) &
	{
		SetAttribute("rel", sRel, true);
		return This();
	}

	self&& SetRel(KStringView sRel) &&
	{
		return std::move(SetRel(sRel));
	}

	self& SetDisabled(bool bYesNo) &
	{
		SetAttribute("disabled", bYesNo);
		return This();
	}

	self&& SetDisabled(bool bYesNo) &&
	{
		return std::move(SetDisabled(bYesNo));
	}

	self& SetFormAction(KStringView sURL) &
	{
		SetAttribute("formaction", sURL, true);
		return This();
	}

	self&& SetFormAction(KStringView sURL) &&
	{
		return std::move(SetFormAction(sURL));
	}

	self& SetFormMethod(METHOD method) &
	{
		SetAttribute("formmethod", FromMethod(method), false);
		return This();
	}

	self&& SetFormMethod(METHOD method) &&
	{
		return std::move(SetFormMethod(method));
	}

	self& SetFormEncType(ENCTYPE encoding) &
	{
		SetAttribute("formenctype", FromEncType(encoding), false);
		return This();
	}

	self&& SetFormEncType(ENCTYPE encoding) &&
	{
		return std::move(SetFormEncType(encoding));
	}

	self& SetFormTarget(TARGET target) &
	{
		SetAttribute("formtarget", FromTarget(target), false);
		return This();
	}

	self&& SetFormTarget(TARGET target) &&
	{
		return std::move(SetFormTarget(target));
	}

	self& SetFormNoValidate(bool bYesNo) &
	{
		SetAttribute("formnovalidate", bYesNo);
		return This();
	}

	self&& SetFormNoValidate(bool bYesNo) &&
	{
		return std::move(SetFormNoValidate(bYesNo));
	}

	self& SetAutofocus(bool bYesNo) &
	{
		SetAttribute("autofocus", bYesNo);
		return This();
	}

	self&& SetAutofocus(bool bYesNo) &&
	{
		return std::move(SetAutofocus(bYesNo));
	}

	self& SetMultiple(bool bYesNo) &
	{
		SetAttribute("multiple", bYesNo);
		return This();
	}

	self&& SetMultiple(bool bYesNo) &&
	{
		return std::move(SetMultiple(bYesNo));
	}

	self& SetPlaceholder(KStringView sPlaceholder) &
	{
		SetAttribute("placeholder", sPlaceholder, true);
		return This();
	}

	self&& SetPlaceholder(KStringView sPlaceholder) &&
	{
		return std::move(SetPlaceholder(sPlaceholder));
	}

	self& SetReadOnly(bool bYesNo) &
	{
		SetAttribute("readonly", bYesNo);
		return This();
	}

	self&& SetReadOnly(bool bYesNo) &&
	{
		return std::move(SetReadOnly(bYesNo));
	}

	self& SetRequired(bool bYesNo) &
	{
		SetAttribute("required", bYesNo);
		return This();
	}

	self&& SetRequired(bool bYesNo) &&
	{
		return std::move(SetRequired(bYesNo));
	}

	self& SetLabel(KStringView sLabel) &
	{
		SetAttribute("label", sLabel, true);
		return This();
	}

	self&& SetLabel(KStringView sLabel) &&
	{
		return std::move(SetLabel(sLabel));
	}

	template<typename Arithmetic>
	self& SetMin(Arithmetic iMin) &
	{
		static_assert(std::is_arithmetic<Arithmetic>::value, "need arithmetic type");
		SetAttribute("min", iMin);
		return This();
	}

	template<typename Arithmetic>
	self&& SetMin(Arithmetic iMin) &&
	{
		return std::move(SetMin(iMin));
	}

	template<typename Arithmetic>
	self& SetMax(Arithmetic iMax) &
	{
		static_assert(std::is_arithmetic<Arithmetic>::value, "need arithmetic type");
		SetAttribute("max", iMax);
		return This();
	}

	template<typename Arithmetic>
	self&& SetMax(Arithmetic iMax) &&
	{
		return std::move(SetMax(iMax));
	}

	template<typename Arithmetic>
	self& SetRange(Arithmetic iMin, Arithmetic iMax) &
	{
		SetMin(iMin);
		SetMax(iMax);
		return This();
	}

	template<typename Arithmetic>
	self&& SetRange(Arithmetic iMin, Arithmetic iMax) &&
	{
		return std::move(SetRange(iMin, iMax));
	}

	self& SetAsync(bool bYesNo) &
	{
		SetAttribute("async", bYesNo);
		return This();
	}

	self&& SetAsync(bool bYesNo) &&
	{
		return SetAsync(bYesNo);
	}

	self& SetDefer(bool bYesNo) &
	{
		SetAttribute("defer", bYesNo);
		return This();
	}

	self&& SetDefer(bool bYesNo) &&
	{
		return SetDefer(bYesNo);
	}

//----------
private:
//----------

	const Derived& This() const { return static_cast<const Derived&>(*this); }
	      Derived& This()       { return static_cast<      Derived&>(*this); }

}; // KWebObject


namespace html {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class HTML : public KWebObject<HTML>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "HTML";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	HTML(KString sElementName, KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject(std::move(sElementName), sID, Classes)
	{
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // HTML

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Page : public KWebObject<Page>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "Page";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Page(KStringView sTitle, KStringView sLanguage = KStringView{});

	KHTMLElement& Head() { return *m_head; }
	KHTMLElement& Body() { return *m_body; }

	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<Page>(*this); }

	void AddMeta(KStringView sName, KStringView sContent);
	void AddStyle(KStringView sStyleDefinition);
	void AddClass(const Class& _class);

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
private:
//----------

	KHTMLElement* m_head  { nullptr };
	KHTMLElement* m_style { nullptr };
	KHTMLElement* m_body  { nullptr };

}; // Page

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Div : public KWebObject<Div>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Div";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Div(KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("div", sID, Classes)
	{
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Div

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Script : public KWebObject<Script>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Script";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = Script;

	Script(KString        sScript = KString{},
		   KStringView    sID     = KStringView{},
		   const Classes& Classes = html::Classes{})
	: KWebObject("script", sID, Classes)
	{
		SetAttribute("charset", "utf-8", false);
		AddRawText(std::move(sScript));
	}

	self& SetType(KMIME MIME) &
	{
		SetAttribute("type", MIME.Serialize());
		return *this;
	}

	self&& SetType(KMIME MIME) &&
	{
		return std::move(SetType(MIME));
	}

	using KWebObject::SetAsync;
	using KWebObject::SetDefer;
	using KWebObject::SetSource;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Script

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Span : public KWebObject<Span>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Span";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Span(KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("span", sID, Classes)
	{
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Span

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Paragraph : public KWebObject<Paragraph>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Paragraph";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Paragraph(KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("p", sID, Classes)
	{
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Paragraph

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Link : public KWebObject<Link>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Link";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Link(KStringView sURL       = KStringView{},
		 KStringView sContent   = KStringView{},
		 KStringView sID        = KStringView{},
		 const Classes& Classes = html::Classes{})
	: KWebObject("a", sID, Classes)
	{
		SetLink(sURL);
		AddText(sContent);
	}

	using KWebObject::SetLink;
	using KWebObject::SetTarget;
	using KWebObject::SetRel;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Link

// A is a pretty meaningless type name, but as most people know a link as <a href=..>
// and because HTML also knows the link element (which we condense to StyleSheet and
// FavIcon) we add the alias A
using A = Link;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class StyleSheet : public KWebObject<StyleSheet>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "StyleSheet";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	StyleSheet(KStringView sURL       = KStringView{},
			   KStringView sContent   = KStringView{},
			   KStringView sID        = KStringView{},
			   const Classes& Classes = html::Classes{})
	: KWebObject("link", sID, Classes)
	{
		if (!sURL.empty())
		{
			SetLink(sURL);
		}
		SetRel("stylesheet");
	}

	using KWebObject::SetLink;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // StyleSheet

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class FavIcon : public KWebObject<FavIcon>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "FavIcon";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	FavIcon(KStringView sURL       = KStringView{},
			KStringView sContent   = KStringView{},
			KStringView sID        = KStringView{},
			const Classes& Classes = html::Classes{})
	: KWebObject("link", sID, Classes)
	{
		if (!sURL.empty())
		{
			SetLink(sURL);
		}
		SetRel("icon");
	}

	self& SetType(KMIME MIME) &
	{
		SetAttribute("type", MIME.Serialize());
		return *this;
	}

	self&& SetType(KMIME MIME) &&
	{
		return std::move(SetType(MIME));
	}

	self& SetSizes(KStringView sSizes) &
	{
		SetAttribute("sizes", sSizes);
		return *this;
	}

	self&& SetSizes(KStringView sSizes) &&
	{
		return std::move(SetSizes(sSizes));
	}

	using KWebObject::SetLink;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // FavIcon

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Break : public KWebObject<Break>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Break";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Break(KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("br", sID, Classes)
	{
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Break

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class HorizontalRuler : public KWebObject<HorizontalRuler>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static constexpr KStringView s_sObjectName = "HorizontalRuler";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	HorizontalRuler(KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("hr", sID, Classes)
	{
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // HorizontalRuler

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// <header>
class Header : public KWebObject<Header>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Heading";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Header(KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("header", sID, Classes)
	{
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Header

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// <h1/h2/h3/h4/h5/h6>
class Heading : public KWebObject<Heading>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Heading";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Heading(uint16_t iLevel, KStringView sContent = KStringView{}, KStringView sID = KStringView{}, const Classes& Classes = html::Classes{});

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Heading

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Image : public KWebObject<Image>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Image";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Image(KStringView sURL, KStringView sDescription, KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("img", sID, Classes)
	{
		SetSource(sURL);
		SetDescription(sDescription);
	}

	using KWebObject::SetSource;
	using KWebObject::SetDescription;
	using KWebObject::SetWidth;
	using KWebObject::SetHeigth;
	using KWebObject::SetLoading;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Image

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Form : public KWebObject<Form>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Form";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = Form;

	Form(KStringView sAction = KStringView{}, KStringView sID = KStringView{}, const Classes& Classes = html::Classes{});

	self&  SetAction(KStringView sAction) &;
	self&& SetAction(KStringView sAction) &&    { return std::move(SetAction(sAction));    }
	self&  SetEncType(ENCTYPE enctype) &;
	self&& SetEncType(ENCTYPE enctype) &&       { return std::move(SetEncType(enctype));   }
	self&  SetMethod(METHOD method) &;
	self&& SetMethod(METHOD method) &&          { return std::move(SetMethod(method));     }
	self&  SetNoValidate(bool bYesNo = true) &;
	self&& SetNoValidate(bool bYesNo = true) && { return std::move(SetNoValidate(bYesNo)); }

	using KWebObject::SetName;
	using KWebObject::SetTarget;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
protected:
//----------

}; // Form

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Legend : public KWebObject<Legend>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Legend";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Legend(KStringView sLegend, KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("legend", sID, Classes)
	{
		AddText(sLegend);
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Legend

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class FieldSet : public KWebObject<FieldSet>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "FieldSet";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	FieldSet(KStringView sLegend, KStringView sID = KStringView{}, const Classes& Classes = html::Classes{})
	: KWebObject("fieldset", sID, Classes)
	{
		if (!sLegend.empty())
		{
			Add(Legend(sLegend));
		}
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // FieldSet

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Button : public KWebObject<Button>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Button";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = Button;

	enum BUTTONTYPE { SUBMIT, RESET, BUTTON };

	Button(KStringView sText = KStringView{},
		   BUTTONTYPE  type  = SUBMIT,
		   KStringView sID   = KStringView{},
		   const Classes& Classes = html::Classes{})
	: KWebObject("button", sID, Classes)
	{
		SetType(type);
		AddText(sText);
	}

	self&  SetType(BUTTONTYPE type) &;
	self&& SetType(BUTTONTYPE type) && { return std::move(SetType(type)); }

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

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Button

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Input : public KWebObject<Input>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Input";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = Input;

	enum INPUTTYPE { CHECKBOX, COLOR, DATE, DATETIME_LOCAL, EMAIL, FILE, HIDDEN,
	                 IMAGE, MONTH, NUMBER, PASSWORD, RADIO, RANGE, SEARCH,
	                 TEL, TEXT, TIME, URL, WEEK };

	Input(KStringView sName  = KStringView{},
		  KStringView sValue = KStringView{},
		  INPUTTYPE   type   = TEXT,
		  KStringView sID    = KStringView{},
		  const Classes& Classes = html::Classes{});

	self&  SetType(INPUTTYPE type) &;
	self&& SetType(INPUTTYPE type) && { return std::move(SetType(type));      }
	self&  SetChecked(bool bYesNo) &;
	self&& SetChecked(bool bYesNo) && { return std::move(SetChecked(bYesNo)); }
	self&  SetStep(float step) &;
	self&& SetStep(float step) && { return std::move(SetStep(step)); }

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
	using KWebObject::SetName;
	using KWebObject::SetValue;
	using KWebObject::SetSize;
	using KWebObject::SetSource;
	using KWebObject::SetMin;
	using KWebObject::SetMax;
	using KWebObject::SetRange;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Input

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Derived, typename Base = Input>
class LabeledInput : public KWebObject<LabeledInput<Derived>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "LabeledInput";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = Derived;
	using wobj = KWebObjectBase;

	template<typename... Args>
	LabeledInput(Args&&... args)
	: KWebObject<LabeledInput<Derived>>("label")
	, m_Base(wobj::Add(Base(std::forward<Args>(args)...)))
	{
	}

	/// Append an element to the list of children, return parent lvalue reference. Use Add() instead to return child reference
	template<typename Element,
	         typename std::enable_if<std::is_base_of<KHTMLObject, Element>::value == true, int>::type = 0>
	Derived& Append(Element Object) &
	{
		m_Base.Append(std::move(Object));
		return This();
	}

	/// Append an element to the list of children, return parent rvalue reference. Use Add() instead to return child reference
	template<typename Element,
	         typename std::enable_if<std::is_base_of<KHTMLObject, Element>::value == true, int>::type = 0>
	Derived&& Append(Element Object) &&
	{
		return std::move(Append(std::move(Object)));
	}

	self& SetLabelBefore(KStringView sLabel) &
	{
		wobj::SetTextBefore(sLabel);
		return This();
	}
	self&& SetLabelBefore(KStringView sLabel) &&
	{
		return std::move(SetLabelBefore(sLabel));
	}
	self& SetLabelAfter (KStringView sLabel) &
	{
		wobj::SetTextAfter(sLabel);
		return This();
	}
	self&& SetLabelAfter (KStringView sLabel) &&
	{
		return std::move(SetLabelAfter(sLabel));
	}
	self& SetType(Input::INPUTTYPE type) &
	{
		m_Base.SetType(type);
		return This();
	}
	self&& SetType(Input::INPUTTYPE type) &&
	{
		return std::move(SetType(type));
	}
	self& SetChecked(bool bYesNo) &
	{
		m_Base.SetChecked(bYesNo);
		return This();
	}
	self&& SetChecked(bool bYesNo) &&
	{
		return std::move(SetChecked(bYesNo));
	}
	self& SetDescription(KStringView sStr) &
	{
		m_Base.SetDescription(sStr);
		return This();
	}
	self&& SetDescription(KStringView sStr) &&
	{
		return std::move(SetDescription(sStr));
	}
	self& SetName(KStringView sStr) &
	{
		m_Base.SetName(sStr);
		return This();
	}
	self&& SetName(KStringView sStr) &&
	{
		return std::move(SetName(sStr));
	}
	self& SetValue(KStringView sStr) &
	{
		m_Base.SetValue(sStr);
		return This();
	}
	self&& SetValue(KStringView sStr) &&
	{
		return std::move(SetValue(sStr));
	}
	self& SetSource(KStringView sStr) &
	{
		m_Base.SetSource(sStr);
		return This();
	}
	self&& SetSource(KStringView sStr) &&
	{
		return std::move(SetSource(sStr));
	}
	self& SetFormAction(KStringView sURL) &
	{
		m_Base.SetFormAction(sURL);
		return This();
	}
	self&& SetFormAction(KStringView sURL) &&
	{
		return std::move(SetFormAction(sURL));
	}
	self& SetAutofocus(bool bYesNo) &
	{
		m_Base.SetAutofocus(bYesNo);
		return This();
	}
	self&& SetAutofocus(bool bYesNo) &&
	{
		return std::move(SetAutofocus(bYesNo));
	}
	self& SetDisabled(bool bYesNo) &
	{
		m_Base.SetDisabled(bYesNo);
		return This();
	}
	self&& SetDisabled(bool bYesNo) &&
	{
		return std::move(SetDisabled(bYesNo));
	}
	self& SetMultiple(bool bYesNo) &
	{
		m_Base.SetMultiple(bYesNo);
		return This();
	}
	self&& SetMultiple(bool bYesNo) &&
	{
		return std::move(SetMultiple(bYesNo));
	}
	self& SetFormNoValidate(bool bYesNo) &
	{
		m_Base.SetFormNoValidate(bYesNo);
		return This();
	}
	self&& SetFormNoValidate(bool bYesNo) &&
	{
		return std::move(SetFormNoValidate(bYesNo));
	}
	self& SetHeigth(wobj::Pixels iInt) &
	{
		m_Base.SetHeigth(iInt);
		return This();
	}
	self&& SetHeigth(wobj::Pixels iInt) &&
	{
		return std::move(SetHeigth(iInt));
	}
	self& SetWidth(wobj::Pixels iInt) &
	{
		m_Base.SetWidth(iInt);
		return This();
	}
	self&& SetWidth(wobj::Pixels iInt) &&
	{
		return std::move(SetWidth(iInt));
	}
	self& SetSize(wobj::Pixels iInt) &
	{
		m_Base.SetSize(iInt);
		return This();
	}
	self&& SetSize(wobj::Pixels iInt) &&
	{
		return std::move(SetSize(iInt));
	}
	self& SetFormMethod(wobj::METHOD method) &
	{
		m_Base.SetFormMethod(method);
		return This();
	}
	self&& SetFormMethod(wobj::METHOD method) &&
	{
		return std::move(SetFormMethod(method));
	}
	self& SetFormEncType(wobj::ENCTYPE encoding) &
	{
		m_Base.SetFormEncType(encoding);
		return This();
	}
	self&& SetFormEncType(wobj::ENCTYPE encoding) &&
	{
		return std::move(SetFormEncType(encoding));
	}
	self& SetFormTarget(wobj::TARGET target) &
	{
		m_Base.SetFormTarget(target);
		return This();
	}
	self&& SetFormTarget(wobj::TARGET target) &&
	{
		return std::move(SetFormTarget(target));
	}
	template<typename Arithmetic>
	self& SetMin(Arithmetic iMin) &
	{
		m_Base.SetMin(iMin);
		return This();
	}
	template<typename Arithmetic>
	self&& SetMin(Arithmetic iMin) &&
	{
		return std::move(SetMin(iMin));
	}
	template<typename Arithmetic>
	self& SetMax(Arithmetic iMax) &
	{
		m_Base.SetMax(iMax);
		return This();
	}
	template<typename Arithmetic>
	self&& SetMax(Arithmetic iMax) &&
	{
		return std::move(SetMax(iMax));
	}
	template<typename Arithmetic>
	self& SetRange(Arithmetic iMin, Arithmetic iMax) &
	{
		SetMin(iMin);
		SetMax(iMax);
		return This();
	}

	template<typename Arithmetic>
	self&& SetRange(Arithmetic iMin, Arithmetic iMax) &&
	{
		return std::move(SetRange(iMin, iMax));
	}

	self& SetStep(float step) &
	{
		m_Base.SetStep(step);
		return This();
	}
	self&& SetStep(float step) &&
	{
		return std::move(SetStep(step));
	}

	Input& GetBase()
	{
		return m_Base;
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
private:
//----------

	Base& m_Base;

	const Derived& This() const { return static_cast<const Derived&>(*this); }
	      Derived& This()       { return static_cast<      Derived&>(*this); }

}; // LabeledInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename String>
class TextInput : public LabeledInput<TextInput<String>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "TextInput";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = TextInput<String>;
	using parent = LabeledInput<self>;

	TextInput(String&    sValue,
			  KStringView sName  = KStringView{},
			  KStringView sID    = KStringView{},
			  const Classes& Classes = html::Classes{})
	: parent(sName, "", Input::TEXT, sID, Classes)
	, m_sValue(sValue)
	{
		if (!m_sValue.empty())
		{
			parent::SetValue(m_sValue);
		}
	}

	TextInput(const TextInput&) = default;
	TextInput(TextInput&&) = default;

	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<self>(*this); }
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
protected:
//----------

	virtual void Reset(KWebObjectBase* Element) override
	{
		// no need to do anything as Sync is always called for text inputs
	}

	virtual void Sync(KWebObjectBase* Element, KStringView sValue) override
	{
		m_sValue = sValue;
		static_cast<Input*>(Element)->SetValue(sValue);
	}

	virtual void* AddressOfInputStorage() override
	{
		// only needed for radio buttons, but we implement it anyways
		return &m_sValue;
	}

	String& m_sValue;

}; // TextInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Arithmetic>
class NumericInput : public LabeledInput<NumericInput<Arithmetic>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "NumericInput";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = NumericInput<Arithmetic>;
	using parent = LabeledInput<self>;

	static_assert(std::is_arithmetic<Arithmetic>::value, "NumericInput needs arithmetic template type");

	NumericInput(Arithmetic& iValue,
			  KStringView sName  = KStringView{},
			  KStringView sID    = KStringView{},
			  const Classes& Classes = html::Classes{})
	: parent(sName, "", Input::NUMBER, sID, Classes)
	, m_iValue(iValue)
	{
		SetMin(std::numeric_limits<Arithmetic>::min());
		SetMax(std::numeric_limits<Arithmetic>::max());
		DispValue();
	}

	self& SetMin(Arithmetic iMin) &
	{
		m_iMin = iMin;
		parent::SetMin(iMin);
		return *this;
	}
	self&& SetMin(Arithmetic iMin) &&
	{
		return std::move(SetMin(iMin));
	}
	self& SetMax(Arithmetic iMax) &
	{
		m_iMax = iMax;
		parent::SetMax(iMax);
		return *this;
	}
	self&& SetMax(Arithmetic iMax) &&
	{
		return std::move(SetMax(iMax));
	}
	self& SetRange(Arithmetic iMin, Arithmetic iMax) &
	{
		SetMin(iMin);
		SetMax(iMax);
		return *this;
	}
	self&& SetRange(Arithmetic iMin, Arithmetic iMax) &&
	{
		return std::move(SetRange(iMin, iMax));
	}

	using parent::SetStep;

	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<self>(*this); }
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
protected:
//----------

	void DispValue()
	{
		parent::SetValue(kFormat("{}", m_iValue));
	}

	void SetNumericValue(Arithmetic iValue)
	{
		m_iValue = std::min(std::max(iValue, m_iMin), m_iMax);
		DispValue();
	}

	template<typename X = Arithmetic,
			 typename std::enable_if<std::is_signed<X>::value == false, int>::type = 0>
	void SetNumericValue(KStringView sValue)
	{
		SetNumericValue(sValue.UInt64());
	}

	template<typename X = Arithmetic,
	         typename std::enable_if<std::is_signed<X>::value == true &&
	                                 std::is_floating_point<X>::value == false, int>::type = 0>
	void SetNumericValue(KStringView sValue)
	{
		SetNumericValue(sValue.Int64());
	}

	template<typename X = Arithmetic,
	         typename std::enable_if<std::is_same<double, X>::value, int>::type = 0>
	void SetNumericValue(KStringView sValue)
	{
		SetNumericValue(sValue.Double());
	}

	template<typename X = Arithmetic,
	         typename std::enable_if<std::is_same<float, X>::value, int>::type = 0>
	void SetNumericValue(KStringView sValue)
	{
		SetNumericValue(sValue.Float());
	}

	virtual void Reset(KWebObjectBase* Element) override
	{
		// no need to do anything as Sync is always called for number inputs
	}

	virtual void Sync(KWebObjectBase* Element, KStringView sValue) override
	{
		SetNumericValue(sValue);
	}

	virtual void* AddressOfInputStorage() override
	{
		// only needed for radio buttons, but we implement it anyways
		return &m_iValue;
	}

	Arithmetic& m_iValue;
	Arithmetic  m_iMin;
	Arithmetic  m_iMax;

}; // NumericInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Unit = std::chrono::seconds, typename Duration = std::chrono::steady_clock::duration>
class DurationInput : public LabeledInput<DurationInput<Unit, Duration>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "DurationInput";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = DurationInput<Unit, Duration>;
	using parent = LabeledInput<self>;

	static_assert(detail::is_chrono_duration<Duration>::value,
				  "DurationInput needs std::chrono::duration Duration template type");
	static_assert(detail::is_chrono_duration<Unit>::value,
				  "DurationInput needs std::chrono::duration Unit template type");

	DurationInput(Duration& iValue,
				  KStringView sName  = KStringView{},
				  KStringView sID    = KStringView{},
				  const Classes& Classes = html::Classes{})
	: parent(sName, "", Input::NUMBER, sID, Classes)
	, m_iValue(iValue)
	{
		SetMin(Unit::min().count());
		SetMax(Unit::max().count());
		DispValue();
	}

	self& SetMin(typename Unit::rep iMin) &
	{
		m_iMin = iMin;
		parent::SetMin(iMin);
		return *this;
	}
	self&& SetMin(typename Unit::rep iMin) &&
	{
		return std::move(SetMin(iMin));
	}
	self& SetMax(typename Unit::rep iMax) &
	{
		m_iMax = iMax;
		parent::SetMax(iMax);
		return *this;
	}
	self&& SetMax(typename Unit::rep iMax) &&
	{
		return std::move(SetMax(iMax));
	}
	self& SetRange(typename Unit::rep iMin, typename Unit::rep iMax) &
	{
		SetMin(iMin);
		SetMax(iMax);
		return *this;
	}
	self&& SetRange(typename Unit::rep iMin, typename Unit::rep iMax) &&
	{
		return std::move(SetRange(iMin, iMax));
	}

	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<self>(*this); }
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
protected:
//----------

	void DispValue()
	{
		parent::SetValue(KString::to_string(std::chrono::duration_cast<Unit>(m_iValue).count()));
	}

	void SetDurationValue(typename Unit::rep iValue)
	{
		m_iValue = Unit(std::min(std::max(iValue, m_iMin), m_iMax));
		DispValue();
	}

	virtual void Reset(KWebObjectBase* Element) override
	{
		// no need to do anything as Sync is always called for number inputs
	}

	virtual void Sync(KWebObjectBase* Element, KStringView sValue) override
	{
		SetDurationValue(sValue.Int64());
	}

	virtual void* AddressOfInputStorage() override
	{
		// only needed for radio buttons, but we implement it anyways
		return &m_iValue;
	}

	Duration&          m_iValue;
	typename Unit::rep m_iMin;
	typename Unit::rep m_iMax;

}; // DurationInput

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename ValueType>
class RadioButton : public LabeledInput<RadioButton<ValueType>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "RadioButton";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = RadioButton<ValueType>;
	using parent = LabeledInput<self>;

	RadioButton(ValueType&  Result,
				KStringView sName  = KStringView{},
				ValueType   Value  = ValueType{},
				KStringView sID    = KStringView{},
				const Classes& Classes = html::Classes{})
	: parent(sName, "", Input::RADIO, sID, Classes)
	, m_Result(Result)
	{
		auto sValue = kFormat("{}", Value);

		if (!sValue.empty())
		{
			parent::SetValue(sValue);

			if (Value == Result)
			{
				parent::SetChecked(true);
			}
		}
	}

	self& SetValue(const ValueType& Value) &
	{
		parent::SetChecked(Value == m_Result);
		auto sValue = kFormat("{}", Value);
		parent::SetValue(sValue);
		return *this;
	}
	self&& SetValue(const ValueType& Value) &&
	{
		return std::move(SetValue(Value));
	}


	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<self>(*this); }
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
protected:
//----------

	virtual void Reset(KWebObjectBase* Element) override
	{
		static_cast<Input*>(Element)->SetChecked(false);
	}

	template<typename X = ValueType,
	         typename std::enable_if<std::is_signed<X>::value == false &&
	                                 detail::is_narrow_cpp_str<X>::value == false, int>::type = 0>
	ValueType GetRadioValue(KStringView sValue)
	{
		return static_cast<ValueType>(sValue.UInt64());
	}

	template<typename X = ValueType,
	         typename std::enable_if<std::is_signed<X>::value == true &&
	                                 std::is_floating_point<X>::value == false, int>::type = 0>
	ValueType GetRadioValue(KStringView sValue)
	{
		return static_cast<ValueType>(sValue.Int64());
	}

	template<typename X = ValueType,
	         typename std::enable_if<std::is_same<double, X>::value, int>::type = 0>
	ValueType GetRadioValue(KStringView sValue)
	{
		return sValue.Double();
	}

	template<typename X = ValueType,
	         typename std::enable_if<std::is_same<float, X>::value, int>::type = 0>
	ValueType GetRadioValue(KStringView sValue)
	{
		return sValue.Float();
	}

	template<typename X = ValueType,
	         typename std::enable_if<detail::is_narrow_cpp_str<X>::value, int>::type = 0>
	ValueType GetRadioValue(KStringView sValue)
	{
		return sValue;
	}

	virtual void Sync(KWebObjectBase* Element, KStringView sValue) override
	{
		if (sValue == Element->GetAttribute("value"))
		{
			m_Result = GetRadioValue(sValue);
			static_cast<Input*>(Element)->SetChecked(true);
		}
	}

	virtual void* AddressOfInputStorage() override
	{
		return &m_Result;
	}

	ValueType& m_Result;

}; // RadioButton

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Boolean>
class CheckBox : public LabeledInput<CheckBox<Boolean>>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "CheckBox";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = CheckBox<Boolean>;
	using parent = LabeledInput<self>;

	CheckBox(Boolean&    bValue,
			 KStringView sName  = KStringView{},
			 KStringView sID    = KStringView{},
			 const Classes& Classes = html::Classes{})
	: parent(sName, "", Input::CHECKBOX, sID, Classes)
	, m_bValue(bValue)
	{
		parent::SetChecked(m_bValue);
	}

	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<self>(*this); }
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
protected:
//----------

	virtual void Reset(KWebObjectBase* Element) override
	{
		m_bValue = false;
		static_cast<Input*>(Element)->SetChecked(m_bValue);
	}

	virtual void Sync(KWebObjectBase* Element, KStringView sValue) override
	{
		m_bValue = sValue.Bool();
		parent::SetChecked(m_bValue);
	}

	virtual void* AddressOfInputStorage() override
	{
		// only needed for radio buttons, but we implement it anyways
		return &m_bValue;
	}

	Boolean& m_bValue;

}; // CheckBox

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Option : public KWebObject<Option>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Option";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = Option;

	Option(KStringView sText      = KStringView{},
		   KStringView sID        = KStringView{},
		   const Classes& Classes = html::Classes{})
	: KWebObject("option", sID, Classes)
	{
		AddText(sText);
	}

	self& SetSelected(bool bYesNo) &
	{
		SetAttribute("selected", bYesNo);
		return *this;
	}

	self&& SetSelected(bool bYesNo) &&
	{
		return std::move(SetSelected(bYesNo));
	}

	using KWebObject::SetDisabled;
	using KWebObject::SetLabel;
	using KWebObject::SetValue;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Option

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Select : public KWebObject<Select>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Select";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self = Select;

	Select(KStringView sName = KStringView{},
		   uint16_t    iSize = 1,
		   KStringView sID   = KStringView{},
		   const Classes& Classes = html::Classes{})
	: KWebObject("select", sID, Classes)
	{
		SetName(sName);
		SetSize(iSize);
	}

	using KWebObject::SetAutofocus;
	using KWebObject::SetDisabled;
	using KWebObject::SetMultiple;
	using KWebObject::SetName;
	using KWebObject::SetRequired;
	using KWebObject::SetSize;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

}; // Select

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename ValueType>
class Selection : public LabeledInput<Selection<ValueType>, Select>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Selection";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using self   = Selection<ValueType>;
	using parent = LabeledInput<self, Select>;

	Selection(ValueType&     Value,
			  KStringView    sName   = KStringView{},
			  uint16_t       iSize   = 1,
			  KStringView    sID     = KStringView{},
			  const Classes& Classes = html::Classes{})
	: parent(sName, iSize, sID, Classes)
	, m_Result(Value)
	{
	}

	self& SetOptions(KStringView sOptions) &
	{
		for (const auto& sOption : sOptions.Split())
		{
			bool bIsSelected = (sOption == m_Result);
			parent::Append(Option(sOption).SetSelected(bIsSelected));
		}
		return *this;
	}

	self&& SetOptions(KStringView sOptions) &&
	{
		return std::move(SetOptions(sOptions));
	}

	template<typename Container,
	         typename std::enable_if<detail::is_str<Container>::value == false, int>::type = 0>
	self& SetOptions(const Container& list) &
	{
		for (const auto& sItem : list)
		{
			bool bIsSelected = (sItem == m_Result);
			parent::Append(Option(sItem).SetSelected(bIsSelected));
		}
		return *this;
	}

	template<typename Container,
	         typename std::enable_if<detail::is_str<Container>::value == false, int>::type = 0>
	self&& SetOptions(const Container& list) &&
	{
		return std::move(SetOptions(list));
	}

	using parent::SetLabelAfter;
	using parent::SetLabelBefore;
	using parent::SetAutofocus;
	using parent::SetDisabled;
	using parent::SetMultiple;
	using parent::SetName;
	using parent::SetRequired;
	using parent::SetSize;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t WebObjectType() const override { return TYPE; }

//----------
public:
//----------

	virtual void Reset(KWebObjectBase* Element) override
	{
		static_cast<Option*>(Element)->SetSelected(false);
	}

	virtual void Sync(KWebObjectBase* Element, KStringView sValue) override
	{
		auto* option = static_cast<Option*>(Element);
		// now get the first text node
		KHTMLText* textelement { nullptr };
		for (const auto& ele : option->GetChildren())
		{
			if (ele->Type() == KHTMLText::TYPE)
			{
				textelement = static_cast<KHTMLText*>(ele.get());
				break;
			}
		}
		if (textelement)
		{
			auto sValAttr = option->GetAttribute("value");
			if ((!sValAttr.empty() && sValAttr == sValue) ||
				(textelement->sText == sValue))
			{
				// we're selected
				m_Result = textelement->sText;
				option->SetSelected(true);
				return;
			}
		}
		option->SetSelected(false);
	}

	virtual void* AddressOfInputStorage() override
	{
		// only needed for radio buttons, but we implement it anyways
		return &m_Result;
	}

	ValueType& m_Result;

}; // Selection

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Text : public KHTMLText
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "Text";

//----------
public:
//----------

	using self = Text;

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	Text() = default;
	Text(KStringView sText = KStringView{});

	self&  AddText(KStringView sContent) &;
	self&& AddText(KStringView sContent) &&
	{
		return std::move(AddText(sContent));
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }

}; // Text

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class RawText : public KHTMLText
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "RawText";

//----------
public:
//----------

	using self = RawText;

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	RawText() = default;
	RawText(KString sText = KString{});

	self&  AddRawText(KStringView sContent) &;
	self&& AddRawText(KStringView sContent) &&
	{
		return std::move(AddRawText(sContent));
	}

	virtual KStringView TypeName() const override { return s_sObjectName;  }

}; // Text

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class LineBreak : public KHTMLText
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "LineBreak";

//----------
public:
//----------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	LineBreak();

	virtual KStringView TypeName() const override { return s_sObjectName;  }

}; // Text

} // end of namespace html
} // end of namespace dekaf2
