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

#include "kwebobjects.h"
#include "kmime.h"
#include "khtmlentities.h"

DEKAF2_NAMESPACE_BEGIN

namespace html {

//-----------------------------------------------------------------------------
Class::Class(KString sClassName, KString sClassDefinition)
//-----------------------------------------------------------------------------
: m_sClassName(std::move(sClassName))
, m_sClassDefinition(std::move(sClassDefinition))
{
}

//-----------------------------------------------------------------------------
Classes::self& Classes::Add(const Class& Class)
//-----------------------------------------------------------------------------
{
	if (!Class.empty())
	{
		return Add(Class.GetName().ToView());
	}
	else
	{
		return *this;
	}

} // Add

//-----------------------------------------------------------------------------
Classes::self& Classes::Add(KStringView sClassName)
//-----------------------------------------------------------------------------
{
	if (!m_sClassNames.empty())
	{
		m_sClassNames += ' ';
	}
	m_sClassNames += sClassName;

	return *this;

} // Add

} // end of namespace html

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromMethod(METHOD method)
//-----------------------------------------------------------------------------
{
	KStringView sMethod;
	switch (method)
	{
		case GET:
			sMethod = "get";
			break;

		case POST:
			sMethod = "post";
			break;

		case DIALOG:
			sMethod = "dialog";
			break;
	}
	return sMethod;

} // FromMethod

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromEncType(ENCTYPE encoding)
//-----------------------------------------------------------------------------
{
	switch (encoding)
	{
		case URLENCODED:
			return KMIME::WWW_FORM_URLENCODED;

		case FORMDATA:
			return KMIME::MULTIPART_FORM_DATA;

		case PLAIN:
			return KMIME::TEXT_PLAIN;
	}

	return {};

} // FromEncType

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromTarget(TARGET target)
//-----------------------------------------------------------------------------
{
	KStringView sTarget;
	switch (target)
	{
		case SELF:
			sTarget = "_self";
			break;

		case BLANK:
			sTarget = "_blank";
			break;

		case PARENT:
			sTarget = "_parent";
			break;

		case TOP:
			sTarget = "_top";
			break;
	}
	return sTarget;

} // FromTarget

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromDir(DIR dir)
//-----------------------------------------------------------------------------
{
	KStringView sDir;
	switch (dir)
	{
		case LTR:
			sDir = "ltr";
			break;

		case RTL:
			sDir = "rtl";
			break;
	}
	return sDir;

} // FromDir

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromLoading(LOADING loading)
//-----------------------------------------------------------------------------
{
	switch (loading)
	{
		case AUTO:
			return "auto";
		case EAGER:
			return "eager";
		case LAZY:
			return "lazy";
	}
	return {};

} // FromLoading

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromAlign(ALIGN align)
//-----------------------------------------------------------------------------
{
	switch (align)
	{
		case LEFT:
			return "left";
		case CENTER:
			return "center";
		case RIGHT:
			return "right";
	}
	return {};

} // FromAlign

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromVAlign(VALIGN valign)
//-----------------------------------------------------------------------------
{
	switch (valign)
	{
		case VTOP:
			return "top";
		case VMIDDLE:
			return "middle";
		case VBOTTOM:
			return "bottom";
	}
	return {};

} // FromVAlign

//-----------------------------------------------------------------------------
void KWebObjectBase::SetTextBefore(KStringView sLabel)
//-----------------------------------------------------------------------------
{
	// check if first node is text, then replace, else add

	if (!empty())
	{
		if (begin()->get()->Type() == KHTMLText::TYPE)
		{
			auto& element = static_cast<KHTMLText&>(*(begin()->get()));
			element.sText = sLabel;
			element.bIsEntityEncoded = false;
		}
		else
		{
			// insert text node at start
			Insert(begin(), KHTMLText(sLabel));
		}
	}

} // SetTextBefore

//-----------------------------------------------------------------------------
void KWebObjectBase::SetTextAfter(KStringView sLabel)
//-----------------------------------------------------------------------------
{
	// check if last node is text, then replace, else add

	if (!empty())
	{
		auto it = begin() + (size() - 1);

		if (it->get()->Type() == KHTMLText::TYPE)
		{
			auto& element = static_cast<KHTMLText&>(*(it->get()));
			element.sText = sLabel;
			element.bIsEntityEncoded = false;
		}
		else
		{
			// add text node
			Add(KHTMLText(sLabel));
		}
	}

} // SetTextAfter

//-----------------------------------------------------------------------------
void KWebObjectBase::Reset(KWebObjectBase* Element)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void KWebObjectBase::Sync(KWebObjectBase* Element, KStringView sValue)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void* KWebObjectBase::AddressOfInputStorage()
//-----------------------------------------------------------------------------
{
	return nullptr;
}

//-----------------------------------------------------------------------------
void KWebObjectBase::TraverseAndSync(KWebObjectBase* Element,
									KWebObjectBase* LastLabeledInput,
									KWebObjectBase* LastSelect,
									const url::KQueryParms& QueryParms)
//-----------------------------------------------------------------------------
{
	switch (Element->WebObjectType())
	{
		case html::Input::TYPE:
		{
			if (LastLabeledInput)
			{
				// make sure we reset checkboxes etc, as the browsers do not send
				// their status if not selected
				LastLabeledInput->Reset(Element);
			}

			auto p = QueryParms.find(Element->GetAttribute("name"));

			if (p != QueryParms.end())
			{
				kDebug(3, "found name: {} == {} for {}", p->first, p->second, Element->TypeName());

				if (LastLabeledInput)
				{
					LastLabeledInput->Sync(Element, p->second);
				}
				else
				{
					Element->Sync(Element, p->second);
				}
			}
			break;
		}

		case html::Option::TYPE:
		{
			if (LastLabeledInput && LastSelect)
			{
				LastLabeledInput->Reset(Element);

				auto p = QueryParms.find(LastSelect->GetAttribute("name"));

				if (p != QueryParms.end())
				{
					kDebug(3, "found name: {} == {} for {}", p->first, p->second, LastLabeledInput->TypeName());

					LastLabeledInput->Sync(Element, p->second);
				}
			}
			break;
		}
	}


	// (re)set the pointer on the LastLabeledInput for all children
	switch (Element->WebObjectType())
	{
		case html::Selection<KString>::TYPE:
		case html::TextInput<KString>::TYPE:
		case html::NumericInput <int>::TYPE:
		case html::DurationInput   <>::TYPE:
		case html::RadioButton <bool>::TYPE:
		case html::CheckBox    <bool>::TYPE:
			LastLabeledInput = Element;
			LastSelect = nullptr;
			break;

		case html::Select::TYPE:
			LastSelect = Element;
			break;

		case html::Option::TYPE:
			// keep the pointers to LastLabeledInput and LastSelect
			break;

		default:
			LastLabeledInput = nullptr;
			LastSelect = nullptr;
			break;
	}

	// now traverse all children
	iterator it = Element->begin();
	iterator ie = Element->end();

	for (; it != ie; ++it)
	{
		auto object = it->get();

		if (object && object->Type() == KHTMLElement::TYPE)
		{
			auto* child = dynamic_cast<KWebObjectBase*>(object);

			if (child)
			{
				TraverseAndSync(child, LastLabeledInput, LastSelect, QueryParms);
			}
		}
	}

} // TraverseAndSync

//-----------------------------------------------------------------------------
void KWebObjectBase::Generate()
//-----------------------------------------------------------------------------
{
	std::size_t iNameCounter { 0 };
	namemap NameMap;
	Generate(this, nullptr, iNameCounter, NameMap);

} // Generate

//-----------------------------------------------------------------------------
KString KWebObjectBase::GenerateName(KStringView sPrefix, std::size_t iNameCounter)
//-----------------------------------------------------------------------------
{
	return kFormat("{}{:x}", sPrefix, iNameCounter);

} // GenerateName

//-----------------------------------------------------------------------------
void KWebObjectBase::Generate(KWebObjectBase* Element,
							 KWebObjectBase* LastLabeledInput,
							 std::size_t& iNameCounter,
							 namemap& NameMap)
//-----------------------------------------------------------------------------
{
	static constexpr KStringView sPrefix = "_fobj";

	switch (Element->WebObjectType())
	{
		case html::Option::TYPE:
		{
			auto sName = Element->GetAttribute("value");

			if (sName.empty())
			{
				auto sID = Element->GetID();

				if (sID.empty())
				{
					Element->SetAttribute("value", GenerateName(sPrefix, ++iNameCounter));
				}
				else
				{
					Element->SetAttribute("value", sID);
				}
			}
			break;
		}

		case html::Input::TYPE:
		case html::Select::TYPE:
		{
			auto sName = Element->GetAttribute("name");

			if (sName.empty())
			{
				if (LastLabeledInput)
				{
					// check if we have this variable already referenced (and named)
					auto* iAddress = LastLabeledInput->AddressOfInputStorage();

					const auto it = NameMap.find(iAddress);

					if (it != NameMap.end())
					{
						// yes, there it is - reuse the name we assigned earlier
						Element->SetAttribute("name", it->second);
						// and exit the switch
						break;
					}
				}

				auto sID = Element->GetID();

				if (sID.empty())
				{
					Element->SetAttribute("name", GenerateName(sPrefix, ++iNameCounter));
				}
				else
				{
					Element->SetAttribute("name", sID);
				}
			}

			// add or update the name to our name map
			if (LastLabeledInput)
			{
				sName = Element->GetAttribute("name");
				// we can only do this if we have a variable bound to a (labeled) input
				auto* iAddress = LastLabeledInput->AddressOfInputStorage();

				if (iAddress)
				{
#ifdef DEKAF2_HAS_CPP_17
					NameMap.insert_or_assign(iAddress, sName);
#else
					auto p = NameMap.insert(namemap::value_type(iAddress, sName));

					if (!p.second)
					{
						p.first->second = sName;
					}
#endif
				}
			}
			break;
		}

		default:
			break;
	}

	// (re)set the pointer on the LastLabeledInput for all children
	switch (Element->WebObjectType())
	{
		case html::Selection<KString>::TYPE:
		case html::TextInput<KString>::TYPE:
		case html::NumericInput <int>::TYPE:
		case html::DurationInput   <>::TYPE:
		case html::RadioButton <bool>::TYPE:
		case html::CheckBox    <bool>::TYPE:
			LastLabeledInput = Element;
			break;

		default:
			LastLabeledInput = nullptr;
			break;
	}

	// now traverse all children
	iterator it = Element->begin();
	iterator ie = Element->end();

	for (; it != ie; ++it)
	{
		auto object = it->get();

		if (object && object->Type() == KHTMLElement::TYPE)
		{
			auto* child = dynamic_cast<KWebObjectBase*>(object);

			if (child)
			{
				Generate(child, LastLabeledInput, iNameCounter, NameMap);
			}
		}
	}

} // Generate


namespace html {

//-----------------------------------------------------------------------------
Page::Page(KStringView sTitle, KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	// prepare a page
	// set html5 as doctype
	Add(KHTMLDocumentType("html"));

	auto& html = Add("html");

	if (!sLanguage.empty())
	{
		html.SetAttribute("lang", sLanguage);
	}

	auto& head = html.Add("head");
	auto& body = html.Add("body");

	auto& title = head.Add("title");
	title.AddText(sTitle);

	m_head = &head;
	m_body = &body;

} // ctor

//-----------------------------------------------------------------------------
void Page::AddMeta(KStringView sName, KStringView sContent)
//-----------------------------------------------------------------------------
{
	auto& meta = m_head->Add("meta");
	meta.SetAttribute("name", sName);
	meta.SetAttribute("content", sContent);

} // AddMeta

//-----------------------------------------------------------------------------
void Page::AddStyle(KStringView sStyleDefinition)
//-----------------------------------------------------------------------------
{
	if (!m_style)
	{
		m_style = &m_head->Add("style");
	}
	m_style->AddText(sStyleDefinition);
	m_style->AddText("\r\n");

} // AddStyle

//-----------------------------------------------------------------------------
void Page::AddClass(const Class& _class)
//-----------------------------------------------------------------------------
{
	if (!_class.empty())
	{
		AddStyle(kFormat("{} {{\r\n{}\r\n}}\r\n", _class.GetName(), _class.GetDefinition()));
	}

} // AddClass

//-----------------------------------------------------------------------------
TableData::TableData(KStringView sContent, KStringView sID, const Classes& Classes)
//-----------------------------------------------------------------------------
: KWebObject("td", sID, Classes)
{
	AddText(sContent);
}

//-----------------------------------------------------------------------------
TableHeader::TableHeader(KStringView sContent, KStringView sID, const Classes& Classes)
//-----------------------------------------------------------------------------
: KWebObject("th", sID, Classes)
{
	AddText(sContent);
}

//-----------------------------------------------------------------------------
Heading::Heading(uint16_t iLevel, KStringView sContent, KStringView sID, const Classes& Classes)
//-----------------------------------------------------------------------------
: KWebObject(kFormat("h{}", iLevel), sID, Classes)
{
	AddText(sContent);
}

//-----------------------------------------------------------------------------
Form::Form(KStringView sAction, KStringView sID, const Classes& Classes)
//-----------------------------------------------------------------------------
: KWebObject("form", sID, Classes)
{
	SetAttribute("accept-charset", "utf-8", true);

	if (!sAction.empty())
	{
		SetAction(sAction);
	}
}

//-----------------------------------------------------------------------------
Form::self& Form::SetAction(KStringView sAction) &
//-----------------------------------------------------------------------------
{
	SetAttribute("action", sAction, true);
	return *this;

} // SetAction

//-----------------------------------------------------------------------------
Form::self& Form::SetEncType(ENCTYPE enctype) &
//-----------------------------------------------------------------------------
{
	SetAttribute("enctype", FromEncType(enctype), false);
	return *this;

} // SetEncType

//-----------------------------------------------------------------------------
Form::self& Form::SetMethod(METHOD method) &
//-----------------------------------------------------------------------------
{
	SetAttribute("method", FromMethod(method), false);
	return *this;

} // SetMethod

//-----------------------------------------------------------------------------
Form::self& Form::SetNoValidate(bool bYesNo) &
//-----------------------------------------------------------------------------
{
	SetAttribute("formnovalidate", bYesNo);
	return *this;

} // SetNoValidate

//-----------------------------------------------------------------------------
Button::self& Button::SetType(BUTTONTYPE type) &
//-----------------------------------------------------------------------------
{
	KStringView sType;
	switch (type)
	{
		case SUBMIT:
			sType = "submit";
			break;

		case RESET:
			sType = "reset";
			break;

		case BUTTON:
			sType = "button";
			break;
	}
	SetAttribute("type", sType, false);
	return *this;

} // SetType

//-----------------------------------------------------------------------------
Input::Input(KStringView sName,
			 KStringView sValue,
			 INPUTTYPE   type,
			 KStringView sID,
			 const Classes& Classes)
//-----------------------------------------------------------------------------
: KWebObject("input", sID, Classes)
{
	if (!sName.empty())
	{
		SetName(sName);
	}
	if (!sValue.empty())
	{
		SetValue(sValue);
	}
	SetType(type);

} // ctor

//-----------------------------------------------------------------------------
Input::self& Input::SetType(INPUTTYPE type) &
//-----------------------------------------------------------------------------
{
	KStringView sType;

	switch (type)
	{
		case CHECKBOX:
			sType = "checkbox";
			break;

		case COLOR:
			sType = "color";
			break;

		case DATE:
			sType = "date";
			break;

		case DATETIME_LOCAL:
			sType = "datetime-local";
			break;

		case EMAIL:
			sType = "email";
			break;

		case FILE:
			sType = "file";
			break;

		case HIDDEN:
			sType = "hidden";
			break;

		case IMAGE:
			sType = "image";
			break;

		case MONTH:
			sType = "month";
			break;

		case NUMBER:
			sType = "number";
			break;

		case PASSWORD:
			sType = "password";
			break;

		case RADIO:
			sType = "radio";
			break;

		case RANGE:
			sType = "range";
			break;

		case SEARCH:
			sType = "search";
			break;

		case SUBMIT:
			sType = "submit";
			break;

		case TEL:
			sType = "tel";
			break;

		case TEXT:
			sType = "text";
			break;

		case TIME:
			sType = "time";
			break;

		case URL:
			sType = "url";
			break;

		case WEEK:
			sType = "week";
			break;
	}

	SetAttribute("type", sType, false);
	return *this;

} // SetType

//-----------------------------------------------------------------------------
Input::self& Input::SetChecked(bool bYesNo) &
//-----------------------------------------------------------------------------
{
	SetAttribute("checked", bYesNo);
	return *this;

} // SetChecked

//-----------------------------------------------------------------------------
Input::self& Input::SetStep(float step) &
//-----------------------------------------------------------------------------
{
	SetAttribute("step", step);
	return *this;

} // SetStep

//-----------------------------------------------------------------------------
Text::Text(KStringView sContent)
//-----------------------------------------------------------------------------
: KHTMLText(sContent)
{
}

//-----------------------------------------------------------------------------
Text::self& Text::AddText(KStringView sContent) &
//-----------------------------------------------------------------------------
{
	sText += sContent;
	return *this;
}

//-----------------------------------------------------------------------------
RawText::RawText(KString sContent)
//-----------------------------------------------------------------------------
: KHTMLText(std::move(sContent), true)
{
}

//-----------------------------------------------------------------------------
RawText::self& RawText::AddRawText(KStringView sContent) &
//-----------------------------------------------------------------------------
{
	sText += sContent;
	return *this;
}

//-----------------------------------------------------------------------------
LineBreak::LineBreak()
//-----------------------------------------------------------------------------
: KHTMLText("\r\n")
{
}

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringView HTML::s_sObjectName;
constexpr KStringView Page::s_sObjectName;
constexpr KStringView Div::s_sObjectName;
constexpr KStringView Script::s_sObjectName;
constexpr KStringView Span::s_sObjectName;
constexpr KStringView Paragraph::s_sObjectName;
constexpr KStringView Table::s_sObjectName;
constexpr KStringView TableRow::s_sObjectName;
constexpr KStringView TableData::s_sObjectName;
constexpr KStringView TableHeader::s_sObjectName;
constexpr KStringView Link::s_sObjectName;
constexpr KStringView StyleSheet::s_sObjectName;
constexpr KStringView FavIcon::s_sObjectName;
constexpr KStringView Break::s_sObjectName;
constexpr KStringView HorizontalRuler::s_sObjectName;
constexpr KStringView Header::s_sObjectName;
constexpr KStringView Heading::s_sObjectName;
constexpr KStringView Image::s_sObjectName;
constexpr KStringView Form::s_sObjectName;
constexpr KStringView Legend::s_sObjectName;
constexpr KStringView FieldSet::s_sObjectName;
constexpr KStringView Button::s_sObjectName;
constexpr KStringView Input::s_sObjectName;
constexpr KStringView Option::s_sObjectName;
constexpr KStringView Select::s_sObjectName;
constexpr KStringView Text::s_sObjectName;
constexpr KStringView RawText::s_sObjectName;
constexpr KStringView LineBreak::s_sObjectName;
#endif

} // end of namespace html

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringView KWebObjectBase::s_sObjectName;
#endif

DEKAF2_NAMESPACE_END
