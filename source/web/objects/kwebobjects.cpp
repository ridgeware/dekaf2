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

#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/web/objects/bits/kwebobjects_templates.h>
#include <dekaf2/web/url/kmime.h>
#include <dekaf2/web/html/khtmlentities.h>
#include <dekaf2/io/streams/koutstringstream.h>

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

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// KWebObjectBase
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//-----------------------------------------------------------------------------
KWebObjectBase::KWebObjectBase(KHTMLNode parent,
                               KStringView sTag,
                               const html::Classes& cls, KStringView sID)
//-----------------------------------------------------------------------------
: KHTMLNode(parent.AddElement(sTag))
{
	if (!sID.empty())              KHTMLNode::SetAttribute("id", sID);
	if (!cls.GetClasses().empty()) KHTMLNode::SetAttribute("class", cls.GetClasses());

} // ctor

//-----------------------------------------------------------------------------
void KWebObjectBase::Serialize(KOutStream& OutStream, char chIndent) const
//-----------------------------------------------------------------------------
{
	khtml::SerializeNode(OutStream, Raw(), chIndent);

} // Serialize

//-----------------------------------------------------------------------------
void KWebObjectBase::Serialize(KStringRef& sOut, char chIndent) const
//-----------------------------------------------------------------------------
{
	KOutStringStream oss(sOut);
	khtml::SerializeNode(oss, Raw(), chIndent);

} // Serialize

//-----------------------------------------------------------------------------
KString KWebObjectBase::Serialize(char chIndent) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Serialize(sOut, chIndent);
	return sOut;

} // Serialize

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromMethod(METHOD method)
//-----------------------------------------------------------------------------
{
	switch (method)
	{
		case GET:    return "get";
		case POST:   return "post";
		case DIALOG: return "dialog";
	}
	return {};

} // FromMethod

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromEncType(ENCTYPE encoding)
//-----------------------------------------------------------------------------
{
	switch (encoding)
	{
		case URLENCODED: return KMIME::WWW_FORM_URLENCODED;
		case FORMDATA:   return KMIME::MULTIPART_FORM_DATA;
		case PLAIN:      return KMIME::TEXT_PLAIN;
	}
	return {};

} // FromEncType

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromTarget(TARGET target)
//-----------------------------------------------------------------------------
{
	switch (target)
	{
		case SELF:   return "_self";
		case BLANK:  return "_blank";
		case PARENT: return "_parent";
		case TOP:    return "_top";
	}
	return {};

} // FromTarget

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromDir(DIR dir)
//-----------------------------------------------------------------------------
{
	switch (dir)
	{
		case LTR: return "ltr";
		case RTL: return "rtl";
	}
	return {};

} // FromDir

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromLoading(LOADING loading)
//-----------------------------------------------------------------------------
{
	switch (loading)
	{
		case AUTO:  return "auto";
		case EAGER: return "eager";
		case LAZY:  return "lazy";
	}
	return {};

} // FromLoading

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromAlign(ALIGN align)
//-----------------------------------------------------------------------------
{
	switch (align)
	{
		case LEFT:   return "left";
		case CENTER: return "center";
		case RIGHT:  return "right";
	}
	return {};

} // FromAlign

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromVAlign(VALIGN valign)
//-----------------------------------------------------------------------------
{
	switch (valign)
	{
		case VTOP:    return "top";
		case VMIDDLE: return "middle";
		case VBOTTOM: return "bottom";
	}
	return {};

} // FromVAlign

//-----------------------------------------------------------------------------
KStringView KWebObjectBase::FromPreload(Preload preload)
//-----------------------------------------------------------------------------
{
	switch (preload)
	{
		case None:     return "none";
		case Metadata: return "metadata";
		case Auto:     return "auto";
	}
	return {};

} // FromPreload

namespace {

//-----------------------------------------------------------------------------
// HTML5 boolean attributes — these get emitted bare (just the attr name,
// no value) when set, and removed when unset. Lifted from the heap-DOM era.
constexpr KStringView s_BooleanAttributes[] = {
	"async", "autofocus", "autoplay", "checked", "controls", "default",
	"defer",    "disabled",   "formnovalidate", "hidden", "ismap", "loop",
	"multiple", "muted",      "novalidate",     "open",   "readonly",
	"required", "reversed",   "scoped",         "seamless",
	"selected", "typemustmatch", "playsinline", "directory",
	"webkitdirectory", "allowfullscreen"
};

//-----------------------------------------------------------------------------
bool IsBooleanAttribute(KStringView sName) noexcept
//-----------------------------------------------------------------------------
{
	for (auto s : s_BooleanAttributes)
	{
		if (s == sName) return true;
	}
	return false;
}

} // anonymous namespace

//-----------------------------------------------------------------------------
void KWebObjectBase::SetBoolAttribute(KStringView sName, bool bYesNo)
//-----------------------------------------------------------------------------
{
	if (IsBooleanAttribute(sName))
	{
		if (bYesNo)
		{
			KHTMLNode::SetAttribute(sName, KStringView{});
		}
		else
		{
			KHTMLNode::RemoveAttribute(sName);
		}
	}
	else
	{
		KHTMLNode::SetAttribute(sName, bYesNo ? KStringView{"true"} : KStringView{"false"});
	}

} // SetBoolAttribute

//-----------------------------------------------------------------------------
void KWebObjectBase::SetTextBefore(KStringView sLabel)
//-----------------------------------------------------------------------------
{
	// Insert a text node as new first child. If the current first child is
	// already a Text node, replace its content in place.
	auto first = FirstChild();
	if (first && first.IsText())
	{
		first.SetName(sLabel);
	}
	else
	{
		// Allocate a text NodePOD via the document arena and link as new
		// first child. NodePOD has AddText which appends; for prepend we
		// allocate and InsertChildBefore.
		auto* pNode = Raw();
		if (!pNode) return;
		auto* pDoc = pNode->Document();
		if (!pDoc) return;
		auto* pTxt = pDoc->Construct<khtml::NodePOD>(pNode, khtml::NodeKind::Text);
		pTxt->Name(pDoc->AllocateString(sLabel));
		pNode->InsertChildBefore(first.Raw(), pTxt);
	}

} // SetTextBefore

//-----------------------------------------------------------------------------
void KWebObjectBase::SetTextAfter(KStringView sLabel)
//-----------------------------------------------------------------------------
{
	auto last = LastChild();
	if (last && last.IsText())
	{
		last.SetName(sLabel);
	}
	else
	{
		AddText(sLabel);
	}

} // SetTextAfter

//-----------------------------------------------------------------------------
void KWebObjectBase::RegisterBinding(KHTMLNode pInputNode, const khtml::InteractiveBinding& binding)
//-----------------------------------------------------------------------------
{
	auto* pNode = pInputNode.Raw();
	if (!pNode) return;
	auto* pDoc = pNode->Document();
	if (!pDoc) return;
	pDoc->Bind(pNode, binding);

} // RegisterBinding

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// KWebObjectBase::Generate / Synchronize
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

namespace {

//-----------------------------------------------------------------------------
// GenerateNames: walk the subtree below pRoot in tree order (DFS pre-order).
// For every NodePOD that has an InteractiveBinding (input/select) but no
// `name` attribute, assign `_fobjN` in incrementing order. Inputs that share
// the same App-Var (same pResult) get the same name — that is the radio-
// button grouping semantics. For every `<option>` without a `value`
// attribute, assign one in the same counter sequence.
//-----------------------------------------------------------------------------
void GenerateNames(khtml::NodePOD* pRoot, khtml::Document* pDoc)
{
	if (!pRoot || !pDoc) return;

	std::size_t iCounter = 0;
	std::unordered_map<void*, KString> ResultToName;

	auto MakeName = [&](void* pResult, KStringView sID) -> KString
	{
		// reuse a previously-assigned name if we have one for this App-Var
		if (pResult != nullptr)
		{
			auto it = ResultToName.find(pResult);
			if (it != ResultToName.end()) return it->second;
		}

		KString sName;
		if (!sID.empty()) sName = sID;
		else              sName = kFormat("_fobj{:x}", ++iCounter);

		if (pResult != nullptr) ResultToName[pResult] = sName;
		return sName;
	};

	// Tree-walk inputs/selects/options in DFS pre-order so the auto-
	// generated names are deterministic (matches the heap-DOM-era order).
	std::function<void(khtml::NodePOD*)> walk = [&](khtml::NodePOD* p)
	{
		if (!p) return;

		for (auto* c = p->FirstChild(); c; c = c->NextSibling())
		{
			if (c->Kind() == khtml::NodeKind::Element)
			{
				const auto& sTag = c->Name();
				if ((sTag == "input" || sTag == "select")
				    && c->Attribute("name") == nullptr)
				{
					KStringView sID;
					if (auto* pID = c->Attribute("id")) sID = pID->Value();

					auto* pBinding = pDoc->FindBinding(c);
					void* pResult  = pBinding ? pBinding->pResult : nullptr;

					KHTMLNode(c).SetAttribute("name", MakeName(pResult, sID));
				}
				else if (sTag == "option" && c->Attribute("value") == nullptr)
				{
					KStringView sID;
					if (auto* pID = c->Attribute("id")) sID = pID->Value();

					KHTMLNode(c).SetAttribute("value",
						!sID.empty() ? KString(sID) : kFormat("_fobj{:x}", ++iCounter));
				}
			}
			walk(c);
		}
	};
	walk(pRoot);
}

} // anonymous namespace

//-----------------------------------------------------------------------------
void KWebObjectBase::Generate()
//-----------------------------------------------------------------------------
{
	auto* pNode = Raw();
	if (!pNode) return;
	auto* pDoc = pNode->Document();
	if (!pDoc) return;
	GenerateNames(pNode, pDoc);

} // Generate

//-----------------------------------------------------------------------------
void KWebObjectBase::Synchronize(const url::KQueryParms& Parms)
//-----------------------------------------------------------------------------
{
	auto* pNode = Raw();
	if (!pNode) return;
	auto* pDoc = pNode->Document();
	if (!pDoc) return;

	auto& bindings = pDoc->Bindings();

	// Snapshot keys first so we can iterate without invalidation if any
	// binding's pfnSync indirectly mutates the map.
	std::vector<khtml::NodePOD*> nodes;
	nodes.reserve(bindings.size());
	for (auto& kv : bindings) nodes.push_back(kv.first);

	// First pass: invoke all pfnReset (so unchecked checkboxes / unselected
	// radios get the right default before sync writes overwrites).
	for (auto* p : nodes)
	{
		auto* b = pDoc->FindBinding(p);
		if (b && b->pfnReset) b->pfnReset(b->pResult, p);
	}

	// Second pass: sync from QueryParms.
	for (auto* p : nodes)
	{
		auto* b = pDoc->FindBinding(p);
		if (!b || !b->pfnSync) continue;

		auto sNameView = KHTMLNode(p).GetAttribute("name");
		if (sNameView.empty()) continue;

		auto it = Parms.find(sNameView);
		if (it == Parms.end()) continue;

		b->pfnSync(b->pResult, p, it->second);
	}

} // Synchronize

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// html namespace
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

namespace html {

//-----------------------------------------------------------------------------
Page::Page(KStringView sTitle, KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	auto root = m_doc.Root();

	// DOCTYPE html
	root.Raw()->AddNode(khtml::NodeKind::DocumentType, "html");

	// <html lang="...">
	auto html = root.AddElement("html");
	if (!sLanguage.empty()) html.SetAttribute("lang", sLanguage);

	m_head = html.AddElement("head");
	m_body = html.AddElement("body");

	// <title>
	auto title = m_head.AddElement("title");
	title.AddText(sTitle);

} // ctor

//-----------------------------------------------------------------------------
void Page::AddMeta(KStringView sName, KStringView sContent)
//-----------------------------------------------------------------------------
{
	auto meta = m_head.AddElement("meta");
	meta.SetAttribute("name",    sName);
	meta.SetAttribute("content", sContent);

} // AddMeta

//-----------------------------------------------------------------------------
void Page::EnsureStyle()
//-----------------------------------------------------------------------------
{
	if (!m_style)
	{
		m_style = m_head.AddElement("style");
	}

} // EnsureStyle

//-----------------------------------------------------------------------------
void Page::AddStyle(KStringView sStyleDefinition)
//-----------------------------------------------------------------------------
{
	EnsureStyle();
	m_style.AddRawText(sStyleDefinition);
	m_style.AddRawText("\r\n");

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
Heading::Heading(KHTMLNode parent, uint16_t iLevel, KStringView sContent, const Classes& cls, KStringView sID)
//-----------------------------------------------------------------------------
: KWebObject<Heading>(parent, kFormat("h{}", iLevel), cls, sID)
{
	if (!sContent.empty()) AddText(sContent);

} // ctor

//-----------------------------------------------------------------------------
Form::Form(KHTMLNode parent, KStringView sAction, const Classes& cls, KStringView sID)
//-----------------------------------------------------------------------------
: KWebObject<Form>(parent, TagName, cls, sID)
{
	KHTMLNode::SetAttribute("accept-charset", "utf-8");

	if (!sAction.empty()) SetAction(sAction);

} // ctor

//-----------------------------------------------------------------------------
Form::self& Form::SetAction(KStringView sAction)
//-----------------------------------------------------------------------------
{
	if (!sAction.empty()) KHTMLNode::SetAttribute("action", sAction);
	return *this;

} // SetAction

//-----------------------------------------------------------------------------
Form::self& Form::SetEncType(ENCTYPE enctype)
//-----------------------------------------------------------------------------
{
	KHTMLNode::SetAttribute("enctype", FromEncType(enctype));
	return *this;

} // SetEncType

//-----------------------------------------------------------------------------
Form::self& Form::SetMethod(METHOD method)
//-----------------------------------------------------------------------------
{
	KHTMLNode::SetAttribute("method", FromMethod(method));
	return *this;

} // SetMethod

//-----------------------------------------------------------------------------
Form::self& Form::SetNoValidate(bool bYesNo)
//-----------------------------------------------------------------------------
{
	SetBoolAttribute("formnovalidate", bYesNo);
	return *this;

} // SetNoValidate

//-----------------------------------------------------------------------------
Button::self& Button::SetType(BUTTONTYPE type)
//-----------------------------------------------------------------------------
{
	KStringView sType;
	switch (type)
	{
		case SUBMIT: sType = "submit"; break;
		case RESET:  sType = "reset";  break;
		case BUTTON: sType = "button"; break;
	}
	KHTMLNode::SetAttribute("type", sType);
	return *this;

} // SetType

//-----------------------------------------------------------------------------
Input::Input(KHTMLNode parent,
             KStringView sName,
             KStringView sValue,
             INPUTTYPE   type,
             const Classes& cls, KStringView sID)
//-----------------------------------------------------------------------------
: KWebObject<Input>(parent, TagName, cls, sID)
{
	if (!sName.empty())  SetName(sName);
	if (!sValue.empty()) SetValue(sValue);
	SetType(type);

} // ctor

//-----------------------------------------------------------------------------
Input::self& Input::SetType(INPUTTYPE type)
//-----------------------------------------------------------------------------
{
	KStringView sType;

	switch (type)
	{
		case CHECKBOX:        sType = "checkbox";       break;
		case COLOR:           sType = "color";          break;
		case DATE:            sType = "date";           break;
		case DATETIME_LOCAL:  sType = "datetime-local"; break;
		case EMAIL:           sType = "email";          break;
		case FILE:            sType = "file";           break;
		case HIDDEN:          sType = "hidden";         break;
		case IMAGE:           sType = "image";          break;
		case MONTH:           sType = "month";          break;
		case NUMBER:          sType = "number";         break;
		case PASSWORD:        sType = "password";       break;
		case RADIO:           sType = "radio";          break;
		case RANGE:           sType = "range";          break;
		case SEARCH:          sType = "search";         break;
		case SUBMIT:          sType = "submit";         break;
		case TEL:             sType = "tel";            break;
		case TEXT:            sType = "text";           break;
		case TIME:            sType = "time";           break;
		case URL:             sType = "url";            break;
		case WEEK:            sType = "week";           break;
	}

	KHTMLNode::SetAttribute("type", sType);
	return *this;

} // SetType

//-----------------------------------------------------------------------------
Input::self& Input::SetChecked(bool bYesNo)
//-----------------------------------------------------------------------------
{
	SetBoolAttribute("checked", bYesNo);
	return *this;

} // SetChecked

//-----------------------------------------------------------------------------
Input::self& Input::SetStep(float step)
//-----------------------------------------------------------------------------
{
	KHTMLNode::SetAttribute("step", kFormat("{}", step));
	return *this;

} // SetStep

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringView Div::TagName;
constexpr KStringView Span::TagName;
constexpr KStringView Paragraph::TagName;
constexpr KStringView Table::TagName;
constexpr KStringView TableRow::TagName;
constexpr KStringView TableData::TagName;
constexpr KStringView TableHeader::TagName;
constexpr KStringView Link::TagName;
constexpr KStringView Script::TagName;
constexpr KStringView StyleSheet::TagName;
constexpr KStringView FavIcon::TagName;
constexpr KStringView Meta::TagName;
constexpr KStringView Break::TagName;
constexpr KStringView HorizontalRuler::TagName;
constexpr KStringView Header::TagName;
constexpr KStringView Image::TagName;
constexpr KStringView Form::TagName;
constexpr KStringView Legend::TagName;
constexpr KStringView FieldSet::TagName;
constexpr KStringView Button::TagName;
constexpr KStringView Output::TagName;
constexpr KStringView Input::TagName;
constexpr KStringView Option::TagName;
constexpr KStringView Select::TagName;
constexpr KStringView Preformatted::TagName;
constexpr KStringView IFrame::TagName;
constexpr KStringView Video::TagName;
constexpr KStringView Audio::TagName;
constexpr KStringView Source::TagName;
// Additional elements
constexpr KStringView Base::TagName;
constexpr KStringView Footer::TagName;
constexpr KStringView Nav::TagName;
constexpr KStringView Main::TagName;
constexpr KStringView Article::TagName;
constexpr KStringView Section::TagName;
constexpr KStringView Aside::TagName;
constexpr KStringView Address::TagName;
constexpr KStringView Figure::TagName;
constexpr KStringView FigureCaption::TagName;
constexpr KStringView UnorderedList::TagName;
constexpr KStringView OrderedList::TagName;
constexpr KStringView ListItem::TagName;
constexpr KStringView DescriptionList::TagName;
constexpr KStringView DescriptionTerm::TagName;
constexpr KStringView DescriptionDetail::TagName;
constexpr KStringView TableCaption::TagName;
constexpr KStringView TableHead::TagName;
constexpr KStringView TableBody::TagName;
constexpr KStringView TableFoot::TagName;
constexpr KStringView ColumnGroup::TagName;
constexpr KStringView Column::TagName;
constexpr KStringView TextArea::TagName;
constexpr KStringView DataList::TagName;
constexpr KStringView OptionGroup::TagName;
constexpr KStringView Meter::TagName;
constexpr KStringView Progress::TagName;
constexpr KStringView Details::TagName;
constexpr KStringView Summary::TagName;
constexpr KStringView Dialog::TagName;
constexpr KStringView Picture::TagName;
constexpr KStringView Canvas::TagName;
constexpr KStringView Time::TagName;
constexpr KStringView BlockQuote::TagName;
constexpr KStringView InlineQuote::TagName;
constexpr KStringView Abbreviation::TagName;
constexpr KStringView Citation::TagName;
constexpr KStringView Strong::TagName;
constexpr KStringView Emphasis::TagName;
constexpr KStringView Bold::TagName;
constexpr KStringView Italic::TagName;
constexpr KStringView Underline::TagName;
constexpr KStringView Strikethrough::TagName;
constexpr KStringView Small::TagName;
constexpr KStringView Subscript::TagName;
constexpr KStringView Superscript::TagName;
constexpr KStringView Mark::TagName;
constexpr KStringView Code::TagName;
constexpr KStringView Keyboard::TagName;
constexpr KStringView Sample::TagName;
constexpr KStringView Variable::TagName;
constexpr KStringView Deleted::TagName;
constexpr KStringView Inserted::TagName;
constexpr KStringView Map::TagName;
constexpr KStringView Area::TagName;
constexpr KStringView WordBreak::TagName;
constexpr KStringView BiDirIsolate::TagName;
constexpr KStringView BiDirOverride::TagName;
constexpr KStringView Ruby::TagName;
constexpr KStringView RubyParen::TagName;
constexpr KStringView RubyText::TagName;
constexpr KStringView Embed::TagName;
constexpr KStringView Object::TagName;
constexpr KStringView Param::TagName;
#endif

} // end of namespace html

DEKAF2_NAMESPACE_END
