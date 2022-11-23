/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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
*/

#include "kxml.h"
#include "klog.h"
#include "kexception.h"
#include "libs/rapidxml-1.13/rapidxml.hpp"
#include "libs/rapidxml-1.13/rapidxml_print.hpp"

namespace dekaf2 {

using Ch                = char;
using rapidXMLDoc       = rapidxml::xml_document<Ch>;
using rapidXMLNode      = rapidxml::xml_node<Ch>;
using rapidXMLAttribute = rapidxml::xml_attribute<Ch>;


// helpers for type casting

//-----------------------------------------------------------------------------
inline rapidXMLDoc* pDocument(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<rapidXMLDoc*>(p);
}

//-----------------------------------------------------------------------------
// alias to make it look syntactically correct for siblings..
inline rapidXMLNode* pNode(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<rapidXMLNode*>(p);
}

//-----------------------------------------------------------------------------
inline rapidXMLAttribute* pAttribute(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<rapidXMLAttribute*>(p);
}

// static helpers

//-----------------------------------------------------------------------------
Ch* CreateString(rapidXMLDoc* Document, KStringView str)
//-----------------------------------------------------------------------------
{
	if (!str.empty() && DEKAF2_LIKELY(Document != nullptr))
	{
		return Document->allocate_string(str.data(), str.size());
	}
	else
	{
		return nullptr;
	}
}

//-----------------------------------------------------------------------------
rapidXMLNode* CreateNode(rapidXMLDoc* Document, KStringView sName, KStringView sValue = KStringView{}, rapidxml::node_type type = rapidxml::node_element)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(Document != nullptr))
	{
		return Document->allocate_node(type, CreateString(Document, sName), CreateString(Document, sValue), sName.size(), sValue.size());
	}
	else
	{
		return nullptr;
	}
}

//-----------------------------------------------------------------------------
rapidXMLAttribute* CreateAttribute(rapidXMLDoc* Document, KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(Document != nullptr))
	{
		return Document->allocate_attribute(CreateString(Document, sName), CreateString(Document, sValue), sName.size(), sValue.size());
	}
	else
	{
		return nullptr;
	}
}

// class members

// ================================= KXML =====================================

//-----------------------------------------------------------------------------
auto rapidXMLDocDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	delete static_cast<rapidXMLDoc*>(data);
};

//-----------------------------------------------------------------------------
KXML::KXML()
//-----------------------------------------------------------------------------
: D(new rapidXMLDoc, rapidXMLDocDeleter)
{
}

//-----------------------------------------------------------------------------
KXML::KXML(KStringView sDocument, bool bPreserveWhiteSpace, KStringView sCreateRoot)
//-----------------------------------------------------------------------------
: KXML()
{
	Parse(sDocument, bPreserveWhiteSpace, sCreateRoot);
}

//-----------------------------------------------------------------------------
KXML::KXML(KInStream& InStream, bool bPreserveWhiteSpace, KStringView sCreateRoot)
//-----------------------------------------------------------------------------
: KXML()
{
	Parse(InStream, bPreserveWhiteSpace, sCreateRoot);
}

//-----------------------------------------------------------------------------
KXML::KXML(KInStream&& InStream, bool bPreserveWhiteSpace, KStringView sCreateRoot)
//-----------------------------------------------------------------------------
: KXML()
{
	Parse(InStream, bPreserveWhiteSpace, sCreateRoot);
}

//-----------------------------------------------------------------------------
void KXML::Serialize(KOutStream& OutStream, int iPrintFlags, KStringView sDropRoot) const
//-----------------------------------------------------------------------------
{
	auto TempRoot = sDropRoot.empty() ? KXMLNode() : KXMLNode(*this).Child(sDropRoot);

	if (!TempRoot.empty())
	{
		// check if the new root was an inline root
		if (TempRoot.IsInlineRoot())
		{
			// switch to terse print flags immediately
			iPrintFlags = KXML::Terse;
		}
		// create temporary document root out of the root node
		auto node = pNode(TempRoot.m_node);
		// store original type
		auto oldtype = node->type();
		// make this a document node..
		node->type(rapidxml::node_document);
		// and print from there
		print(OutStream.OutStream(), *node, iPrintFlags);
		// and restore the original type
		node->type(oldtype);
	}
	else
	{
		// in all other cases print the whole document
		print(OutStream.OutStream(), *pDocument(D.get()), iPrintFlags);
	}
}

//-----------------------------------------------------------------------------
void KXML::Serialize(KOutStream&& OutStream, int iPrintFlags, KStringView sDropRoot) const
//-----------------------------------------------------------------------------
{
	Serialize(OutStream, iPrintFlags, sDropRoot);
}

//-----------------------------------------------------------------------------
void KXML::Serialize(KStringRef& string, int iPrintFlags, KStringView sDropRoot) const
//-----------------------------------------------------------------------------
{
	auto TempRoot = sDropRoot.empty() ? KXMLNode() : KXMLNode(*this).Child(sDropRoot);

	if (!TempRoot.empty())
	{
		// check if the new root was an inline root
		if (TempRoot.IsInlineRoot())
		{
			// switch to terse print flags immediately
			iPrintFlags = KXML::Terse;
		}
		// create temporary document root out of the root node
		auto node = pNode(TempRoot.m_node);
		// store original type
		auto oldtype = node->type();
		// make this a document node..
		node->type(rapidxml::node_document);
		// and print from there
		print(std::back_inserter(string), *node, iPrintFlags);
		// and restore the original type
		node->type(oldtype);
	}
	else
	{
		// in all other cases print the whole document
		print(std::back_inserter(string), *pDocument(D.get()), iPrintFlags);
	}
}

//-----------------------------------------------------------------------------
KString KXML::Serialize(int iPrintFlags, KStringView sDropRoot) const
//-----------------------------------------------------------------------------
{
	KString string;
	Serialize(string, iPrintFlags, sDropRoot);
	return string;
}

//-----------------------------------------------------------------------------
bool KXML::Parse(KInStream& InStream, bool bPreserveWhiteSpace, KStringView sCreateRoot)
//-----------------------------------------------------------------------------
{
	clear();

	if (!sCreateRoot.empty())
	{
		XMLData += '<';
		XMLData += sCreateRoot;
		XMLData += '>';
	}

	kAppendAll(InStream, XMLData, false);

	if (!sCreateRoot.empty())
	{
		XMLData += "</";
		XMLData += sCreateRoot;
		XMLData += '>';
	}

	return Parse(bPreserveWhiteSpace);
}

//-----------------------------------------------------------------------------
bool KXML::Parse(KInStream&& InStream, bool bPreserveWhiteSpace, KStringView sCreateRoot)
//-----------------------------------------------------------------------------
{
	return Parse(InStream, bPreserveWhiteSpace, sCreateRoot);
}

//-----------------------------------------------------------------------------
bool KXML::Parse(KStringView string, bool bPreserveWhiteSpace, KStringView sCreateRoot)
//-----------------------------------------------------------------------------
{
	clear();

	if (!sCreateRoot.empty())
	{
		XMLData += '<';
		XMLData += sCreateRoot;
		XMLData += '>';
	}

	XMLData += string;

	if (!sCreateRoot.empty())
	{
		XMLData += "</";
		XMLData += sCreateRoot;
		XMLData += '>';
	}

	return Parse(bPreserveWhiteSpace);
}

//-----------------------------------------------------------------------------
void KXML::clear()
//-----------------------------------------------------------------------------
{
	D = KUniqueVoidPtr(new rapidXMLDoc, rapidXMLDocDeleter);
	XMLData.clear();
}

//-----------------------------------------------------------------------------
bool KXML::HadXMLDeclaration() const
//-----------------------------------------------------------------------------
{
	return pDocument(D.get())->get_xml_declaration();
}

//-----------------------------------------------------------------------------
const KXMLNode KXML::GetXMLDeclaration() const
//-----------------------------------------------------------------------------
{
	KXMLNode XMLDeclaration;

	XMLDeclaration.m_node = pDocument(D.get())->get_xml_declaration();

	return XMLDeclaration;
}

//-----------------------------------------------------------------------------
bool KXML::Parse(bool bPreserveWhiteSpace)
//-----------------------------------------------------------------------------
{
	try {

		if (bPreserveWhiteSpace)
		{
			pDocument(D.get())->parse<rapidxml::parse_no_string_terminators
			                        | rapidxml::parse_preserve_whitespace>(&XMLData.front());
		}
		else
		{
			pDocument(D.get())->parse<rapidxml::parse_no_string_terminators>(&XMLData.front());
		}

		return true;

	}
	catch (const rapidxml::parse_error& ex)
	{
		// build our own exception to include the information from
		// rapidxml's .where() funtion

		KStringView sWhere { ex.where<Ch>() };

		KStringView::size_type iPos { 0 };

		if (!sWhere.empty())
		{
			iPos = XMLData.find(sWhere);

			if (iPos == KStringView::npos)
			{
				iPos = 0;
			}
		}

		m_sLastError.Format ("{} at pos {}, next input: \"{}\"",
			ex.what(),
			iPos,
			sWhere.substr(0, 20));

		if (m_bStackTraceOnParseError)
		{
			KException kEx(m_sLastError);
			kException (kEx);
		}

		clear();
	}

	return false;
}

//-----------------------------------------------------------------------------
void KXML::AddXMLDeclaration()
//-----------------------------------------------------------------------------
{
	AddXMLDeclaration("1.0", "utf-8", "yes");
}

//-----------------------------------------------------------------------------
void KXML::AddXMLDeclaration(KStringView sVersion, KStringView sEncoding, KStringView sStandalone)
//-----------------------------------------------------------------------------
{
	rapidXMLDoc* doc = pDocument(D.get());
	rapidXMLNode* DocType = CreateNode(doc, "", "", rapidxml::node_declaration);
	if (!sVersion.empty())
	{
		DocType->append_attribute(CreateAttribute(doc, "version", sVersion));
	}
	if (!sEncoding.empty())
	{
		DocType->append_attribute(CreateAttribute(doc, "encoding", sEncoding));
	}
	if (!sStandalone.empty())
	{
		DocType->append_attribute(CreateAttribute(doc, "standalone", sStandalone));
	}
	doc->prepend_node(DocType);
}

// =============================== KXMLNode ===================================

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::operator++()
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		m_node = pNode(m_node)->next_sibling();
	}

	return *this;
}

//-----------------------------------------------------------------------------
const KXMLNode KXMLNode::operator++(int)
//-----------------------------------------------------------------------------
{
	const KXMLNode it = *this;

	operator++();

	return it;
}

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::operator--()
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		m_node = pNode(m_node)->previous_sibling();
	}

	return *this;
}

//-----------------------------------------------------------------------------
const KXMLNode KXMLNode::operator--(int)
//-----------------------------------------------------------------------------
{
	const KXMLNode it = *this;

	operator--();

	return it;
}

//-----------------------------------------------------------------------------
size_t KXMLNode::size() const
//-----------------------------------------------------------------------------
{
	size_t count { 0 };

	if (m_node)
	{
		rapidXMLNode* child = pNode(m_node)->first_node();
		while (child)
		{
			++count;
			child = child->next_sibling();
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::Parent() const
//-----------------------------------------------------------------------------
{
	KXMLNode parent;

	if (m_node)
	{
		parent.m_node = pNode(m_node)->parent();
	}

	return parent;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::Child() const
//-----------------------------------------------------------------------------
{
	KXMLNode child;

	if (m_node)
	{
		child.m_node = pNode(m_node)->first_node();
	}

	return child;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::Child(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KXMLNode child;

	if (m_node)
	{
		child.m_node = pNode(m_node)->first_node(sName.data(), sName.size());
	}

	return child;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::Next(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KXMLNode sibling;

	if (m_node)
	{
		sibling.m_node = pNode(m_node)->next_sibling(sName.data(), sName.size());
	}

	return sibling;
}

//-----------------------------------------------------------------------------
KStringView KXMLNode::GetName() const
//-----------------------------------------------------------------------------
{
	KStringView sName;

	if (m_node)
	{
		sName.assign(pNode(m_node)->name(), pNode(m_node)->name_size());
	}

	return sName;
}

//-----------------------------------------------------------------------------
KStringView KXMLNode::GetValue() const
//-----------------------------------------------------------------------------
{
	KStringView sValue;

	if (m_node)
	{
		sValue.assign(pNode(m_node)->value(), pNode(m_node)->value_size());
	}

	return sValue;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::AddNode(KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	KXMLNode node;

	if (m_node)
	{
		rapidXMLDoc* doc = pNode(m_node)->document();

		node.m_node = CreateNode(doc, sName);

		if (!sValue.empty())
		{
			pNode(node.m_node)->append_node(CreateNode(doc, KStringView{}, sValue, rapidxml::node_data));
		}

		pNode(m_node)->append_node(pNode(node.m_node));
	}

	return node;
}

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::SetInlineRoot(bool bInlineRoot)
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		auto* node = pNode(m_node);

		if (bInlineRoot)
		{
			if (node->type() == rapidxml::node_element)
			{
				node->type(rapidxml::node_element_inline_root);
			}
		}
		else
		{
			if (node->type() == rapidxml::node_element_inline_root)
			{
				node->type(rapidxml::node_element);
			}
		}
	}

	return *this;
}

//-----------------------------------------------------------------------------
bool KXMLNode::IsInlineRoot() const
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		return (pNode(m_node)->type() == rapidxml::node_element_inline_root);
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::SetName(KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		Ch* name = CreateString(pNode(m_node)->document(), sName);
		pNode(m_node)->name(name, sName.size());
	}
	return *this;
}

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::SetValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		Ch* value = CreateString(pNode(m_node)->document(), sValue);
		pNode(m_node)->value(value, sValue.size());
	}
	return *this;
}

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::AddValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		if (!sValue.empty())
		{
			rapidXMLDoc* doc = pNode(m_node)->document();
			pNode(m_node)->append_node(CreateNode(doc, KStringView{}, sValue, rapidxml::node_data));
		}
	}
	return *this;
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLNode::Attribute(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KXMLAttribute attribute;

	if (m_node)
	{
		attribute.m_attribute = pNode(m_node)->first_attribute(sName.data(), sName.size());
	}

	return attribute;
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLNode::AddAttribute(KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	KXMLAttribute attribute;

	if (m_node)
	{
		attribute.m_attribute = CreateAttribute(pNode(m_node)->document(), sName, sValue);
		pNode(m_node)->append_attribute(pAttribute(attribute.m_attribute));
	}

	return attribute;
}

// ============================= KXMLAttribute ================================

//-----------------------------------------------------------------------------
KXMLAttribute::KXMLAttribute(const KXMLNode& node)
//-----------------------------------------------------------------------------
{
	if (node.m_node)
	{
		m_attribute = pNode(node.m_node)->first_attribute();
	}
}

//-----------------------------------------------------------------------------
KXMLAttribute& KXMLAttribute::operator++()
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		m_attribute = pAttribute(m_attribute)->next_attribute();
	}

	return *this;
}

//-----------------------------------------------------------------------------
const KXMLAttribute KXMLAttribute::operator++(int)
//-----------------------------------------------------------------------------
{
	const KXMLAttribute it = *this;

	operator++();

	return it;
}

//-----------------------------------------------------------------------------
KXMLAttribute& KXMLAttribute::operator--()
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		m_attribute = pAttribute(m_attribute)->previous_attribute();
	}

	return *this;
}

//-----------------------------------------------------------------------------
const KXMLAttribute KXMLAttribute::operator--(int)
//-----------------------------------------------------------------------------
{
	const KXMLAttribute it = *this;

	operator--();

	return it;
}

//-----------------------------------------------------------------------------
size_t KXMLAttribute::size() const
//-----------------------------------------------------------------------------
{
	size_t count { 0 };

	if (m_attribute)
	{
		rapidXMLNode* parent = pAttribute(m_attribute)->parent();

		if (parent)
		{
			rapidXMLAttribute* attribute = parent->first_attribute();

			while (attribute)
			{
				++count;
				attribute = attribute->next_attribute();
			}
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLAttribute::Parent() const
//-----------------------------------------------------------------------------
{
	KXMLNode parent;

	if (m_attribute)
	{
		parent.m_node = pAttribute(m_attribute)->parent();
	}

	return parent;
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLAttribute::find(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KXMLAttribute sibling;

	if (m_attribute)
	{
		rapidXMLNode* parent = pAttribute(m_attribute)->parent();

		if (parent)
		{
			sibling.m_attribute = parent->first_attribute(sName.data(), sName.size());
		}
	}

	return sibling;
}

//-----------------------------------------------------------------------------
KStringView KXMLAttribute::GetName() const
//-----------------------------------------------------------------------------
{
	KStringView sName;

	if (m_attribute)
	{
		sName.assign(pAttribute(m_attribute)->name(), pAttribute(m_attribute)->name_size());
	}

	return sName;
}

//-----------------------------------------------------------------------------
KStringView KXMLAttribute::GetValue() const
//-----------------------------------------------------------------------------
{
	KStringView sValue;

	if (m_attribute)
	{
		sValue.assign(pAttribute(m_attribute)->value(), pAttribute(m_attribute)->value_size());
	}

	return sValue;
}

//-----------------------------------------------------------------------------
KXMLAttribute& KXMLAttribute::SetName(KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		Ch* name = CreateString(pAttribute(m_attribute)->document(), sName);
		pAttribute(m_attribute)->name(name, sName.size());
	}
	return *this;
}

//-----------------------------------------------------------------------------
KXMLAttribute& KXMLAttribute::SetValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		Ch* value = CreateString(pAttribute(m_attribute)->document(), sValue);
		pAttribute(m_attribute)->value(value, sValue.size());
	}
	return *this;
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLAttribute::AddAttribute(KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	KXMLAttribute sibling;

	if (m_attribute)
	{
		rapidXMLNode* parent = pAttribute(m_attribute)->parent();

		if (parent)
		{
			sibling.m_attribute = CreateAttribute(parent->document(), sName, sValue);
			parent->append_attribute(pAttribute(sibling.m_attribute));
		}
	}

	return sibling;
}

} // end of namespace dekaf2


