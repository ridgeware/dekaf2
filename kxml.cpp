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
#include "libs/rapidxml-1.13/rapidxml.hpp"
#include "libs/rapidxml-1.13/rapidxml_print.hpp"

namespace dekaf2 {

using Ch                = char;
using rapidXMLDoc       = rapidxml::xml_document<Ch>;
using rapidXMLNode      = rapidxml::xml_node<Ch>;
using rapidXMLAttribute = rapidxml::xml_attribute<Ch>;


// helpers for type casting

//-----------------------------------------------------------------------------
inline rapidXMLDoc* Document(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<rapidXMLDoc*>(p);
}

//-----------------------------------------------------------------------------
inline rapidXMLNode* Parent(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<rapidXMLNode*>(p);
}

//-----------------------------------------------------------------------------
// alias to make it look syntactically correct for siblings..
inline rapidXMLNode* Node(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<rapidXMLNode*>(p);
}

//-----------------------------------------------------------------------------
inline rapidXMLAttribute* Attribute(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<rapidXMLAttribute*>(p);
}

// static helpers

//-----------------------------------------------------------------------------
Ch* CreateString(rapidXMLDoc* Document, KStringView str)
//-----------------------------------------------------------------------------
{
	if (!str.empty())
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
	return Document->allocate_node(type, CreateString(Document, sName), CreateString(Document, sValue), sName.size(), sValue.size());
}

//-----------------------------------------------------------------------------
rapidXMLAttribute* CreateAttribute(rapidXMLDoc* Document, KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	return Document->allocate_attribute(CreateString(Document, sName), CreateString(Document, sValue), sName.size(), sValue.size());
}

// class members

// ================================= KXML =====================================

//-----------------------------------------------------------------------------
auto rapidXMLDocDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	auto p = static_cast<rapidXMLDoc*>(data);
	delete p;
};

//-----------------------------------------------------------------------------
KXML::KXML()
//-----------------------------------------------------------------------------
{
	D = unique_void_ptr(new rapidXMLDoc, rapidXMLDocDeleter);
}

//-----------------------------------------------------------------------------
KXML::KXML(KStringView sDocument)
//-----------------------------------------------------------------------------
: KXML()
{
	Parse(sDocument);
}

//-----------------------------------------------------------------------------
KXML::KXML(KInStream& InStream)
//-----------------------------------------------------------------------------
: KXML()
{
	Parse(InStream);
}

//-----------------------------------------------------------------------------
void KXML::Serialize(KOutStream& OutStream, bool bIndented) const
//-----------------------------------------------------------------------------
{
	print(OutStream.OutStream(), *Document(D.get()), bIndented ? 0 : rapidxml::print_no_indenting);
}

//-----------------------------------------------------------------------------
void KXML::Serialize(KString& string, bool bIndented) const
//-----------------------------------------------------------------------------
{
	print(std::back_inserter(string), *Document(D.get()), bIndented ? 0 : rapidxml::print_no_indenting);
}

//-----------------------------------------------------------------------------
KString KXML::Serialize(bool bIndented) const
//-----------------------------------------------------------------------------
{
	KString string;
	Serialize(string, bIndented);
	return string;
}

//-----------------------------------------------------------------------------
bool KXML::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	clear();
	kReadAll(InStream, XMLData, false);
	Parse();
	return true;
}

//-----------------------------------------------------------------------------
void KXML::Parse(KStringView string)
//-----------------------------------------------------------------------------
{
	clear();
	XMLData = string;
	Parse();
}

//-----------------------------------------------------------------------------
void KXML::clear()
//-----------------------------------------------------------------------------
{
	D = unique_void_ptr(new rapidXMLDoc, rapidXMLDocDeleter);
	XMLData.clear();
}

//-----------------------------------------------------------------------------
void KXML::Parse()
//-----------------------------------------------------------------------------
{
	Document(D.get())->parse<rapidxml::parse_no_string_terminators>(&XMLData.front());
}

//-----------------------------------------------------------------------------
void KXML::AddXMLDeclaration()
//-----------------------------------------------------------------------------
{
	rapidXMLNode* DocType = CreateNode(Document(D.get()), "", "", rapidxml::node_declaration);
	DocType->append_attribute(CreateAttribute(Document(D.get()), "version", "1.0"));
	DocType->append_attribute(CreateAttribute(Document(D.get()), "encoding", "utf-8"));
	DocType->append_attribute(CreateAttribute(Document(D.get()), "standalone", "yes"));
	Document(D.get())->prepend_node(DocType);
}

// =============================== KXMLNode ===================================

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::operator++()
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		m_node = Node(m_node)->next_sibling();
	}

	return *this;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::operator++(int)
//-----------------------------------------------------------------------------
{
	KXMLNode it = *this;

	if (m_node)
	{
		m_node = Node(m_node)->next_sibling();
	}

	return it;
}

//-----------------------------------------------------------------------------
KXMLNode& KXMLNode::operator--()
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		m_node = Node(m_node)->previous_sibling();
	}

	return *this;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::operator--(int)
//-----------------------------------------------------------------------------
{
	KXMLNode it = *this;

	if (m_node)
	{
		m_node = Node(m_node)->previous_sibling();
	}

	return it;
}

//-----------------------------------------------------------------------------
size_t KXMLNode::size() const
//-----------------------------------------------------------------------------
{
	size_t count { 0 };

	if (m_node)
	{
		rapidXMLNode* child = Node(m_node)->first_node();
		while (child)
		{
			++count;
			child = child->next_sibling();
		}
	}

	return count;
}

//-----------------------------------------------------------------------------
KXMLNode KXMLNode::find(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KXMLNode child;

	if (m_node)
	{
		child.m_node = Node(m_node)->first_node(sName.data(), sName.size());
	}

	return child;
}

//-----------------------------------------------------------------------------
KXMLNode::iterator KXMLNode::begin() const
//-----------------------------------------------------------------------------
{
	iterator it;

	if (m_node)
	{
		it.m_node = Node(m_node)->first_node();
	}

	return it;
}

//-----------------------------------------------------------------------------
KStringView KXMLNode::GetName() const
//-----------------------------------------------------------------------------
{
	KStringView sName;

	if (m_node)
	{
		sName.assign(Node(m_node)->name(), Node(m_node)->name_size());
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
		sValue.assign(Node(m_node)->value(), Node(m_node)->value_size());
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
		node.m_node = CreateNode(Node(m_node)->document(), sName);

		if (!sValue.empty())
		{
			Node(node.m_node)->append_node(CreateNode(Node(m_node)->document(), KStringView{}, sValue, rapidxml::node_data));
		}

		Node(m_node)->append_node(Node(node.m_node));
	}

	return node;
}

//-----------------------------------------------------------------------------
void KXMLNode::SetName(KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		Ch* name = CreateString(Node(m_node)->document(), sName);
		Node(m_node)->name(name, sName.size());
	}
}

//-----------------------------------------------------------------------------
void KXMLNode::SetValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (m_node)
	{
		Ch* value = CreateString(Node(m_node)->document(), sValue);
		Node(m_node)->value(value, sValue.size());
	}
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLNode::AddAttribute(KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	KXMLAttribute attribute;

	if (m_node)
	{
		attribute.m_attribute = CreateAttribute(Node(m_node)->document(), sName, sValue);
		Node(m_node)->append_attribute(Attribute(attribute.m_attribute));
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
		m_attribute = Node(node.m_node)->first_attribute();
	}
}

//-----------------------------------------------------------------------------
KXMLAttribute& KXMLAttribute::operator++()
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		m_attribute = Attribute(m_attribute)->next_attribute();
	}

	return *this;
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLAttribute::operator++(int)
//-----------------------------------------------------------------------------
{
	KXMLAttribute it = *this;

	if (m_attribute)
	{
		m_attribute = Attribute(m_attribute)->next_attribute();
	}

	return it;
}

//-----------------------------------------------------------------------------
KXMLAttribute& KXMLAttribute::operator--()
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		m_attribute = Attribute(m_attribute)->previous_attribute();
	}

	return *this;
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLAttribute::operator--(int)
//-----------------------------------------------------------------------------
{
	KXMLAttribute it = *this;

	if (m_attribute)
	{
		m_attribute = Attribute(m_attribute)->previous_attribute();
	}

	return it;
}

//-----------------------------------------------------------------------------
size_t KXMLAttribute::size() const
//-----------------------------------------------------------------------------
{
	size_t count { 0 };

	if (m_attribute)
	{
		rapidXMLNode* parent = Attribute(m_attribute)->parent();

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
KXMLAttribute KXMLAttribute::find(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KXMLAttribute sibling;

	if (m_attribute)
	{
		rapidXMLNode* parent = Attribute(m_attribute)->parent();

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
		sName.assign(Attribute(m_attribute)->name(), Attribute(m_attribute)->name_size());
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
		sValue.assign(Attribute(m_attribute)->value(), Attribute(m_attribute)->value_size());
	}

	return sValue;
}

//-----------------------------------------------------------------------------
void KXMLAttribute::SetName(KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		Ch* name = CreateString(Attribute(m_attribute)->document(), sName);
		Attribute(m_attribute)->name(name, sName.size());
	}
}

//-----------------------------------------------------------------------------
void KXMLAttribute::SetValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (m_attribute)
	{
		Ch* value = CreateString(Attribute(m_attribute)->document(), sValue);
		Attribute(m_attribute)->value(value, sValue.size());
	}
}

//-----------------------------------------------------------------------------
KXMLAttribute KXMLAttribute::AddAttribute(KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	KXMLAttribute sibling;

	if (m_attribute)
	{
		rapidXMLNode* parent = Attribute(m_attribute)->parent();

		if (parent)
		{
			sibling.m_attribute = CreateAttribute(parent->document(), sName, sValue);
			parent->append_attribute(Attribute(sibling.m_attribute));
		}
	}

	return sibling;
}

// ============================= KXMLDocument =================================

//-----------------------------------------------------------------------------
void KXMLDocument::AddNode(KStringView sName, KStringView sValue, bool bDescendInto)
//-----------------------------------------------------------------------------
{
	rapidXMLNode* node = CreateNode(Document(D.get()), sName, sValue);
	Parent(P)->append_node(node);
	if (bDescendInto)
	{
		P = node;
	}
}

//-----------------------------------------------------------------------------
void KXMLDocument::AddAttribute(KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	rapidXMLAttribute* attribute = CreateAttribute(Document(D.get()), sName, sValue);
	Parent(P)->append_attribute(attribute);
}

//-----------------------------------------------------------------------------
void KXMLDocument::AddValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	KStringView sExisting = GetValue();

	if (!sExisting.empty())
	{
		KString string { sExisting };
		string += sValue;
		Ch* value = CreateString(Document(D.get()), string);
		Parent(P)->value(value, string.size());
	}
	else
	{
		Ch* value = CreateString(Document(D.get()), sValue);
		Parent(P)->value(value, sValue.size());
	}

}

//-----------------------------------------------------------------------------
void KXMLDocument::SetValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	Ch* value = CreateString(Document(D.get()), sValue);
	Parent(P)->value(value, sValue.size());
}

//-----------------------------------------------------------------------------
bool KXMLDocument::GetNode(KStringView sName, KStringView& sValue, bool bDescendInto) const
//-----------------------------------------------------------------------------
{
	rapidXMLNode* node = Parent(P)->first_node(sName.data(), sName.size());
	if (!node)
	{
		sValue.clear();
		return false;
	}
	else
	{
		sValue.assign(node->value(), node->value_size());

		if (bDescendInto)
		{
			P = node;
		}

		return true;
	}
}

//-----------------------------------------------------------------------------
KStringView KXMLDocument::GetNode(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KStringView sValue;
	GetNode(sName, sValue);
	return sValue;
}

//-----------------------------------------------------------------------------
bool KXMLDocument::GetSibling(KStringView sName, KStringView& sValue, bool bPrevious) const
//-----------------------------------------------------------------------------
{
	if (!NextSibling(sName, bPrevious))
	{
		sValue.clear();
		return false;
	}

	GetValue(sValue);
	return true;
}

//-----------------------------------------------------------------------------
bool KXMLDocument::GetAttribute(KStringView sName, KStringView& sValue) const
//-----------------------------------------------------------------------------
{
	rapidXMLAttribute* attribute = Parent(P)->first_attribute(sName.data(), sName.size());
	if (!attribute)
	{
		sValue.clear();
		return false;
	}
	else
	{
		sValue.assign(attribute->value(), attribute->value_size());
		return true;
	}
}

//-----------------------------------------------------------------------------
KStringView KXMLDocument::GetAttribute(KStringView sName) const
//-----------------------------------------------------------------------------
{
	KStringView sValue;
	GetAttribute(sName, sValue);
	return sValue;
}

//-----------------------------------------------------------------------------
void KXMLDocument::GetValue(KStringView& sValue) const
//-----------------------------------------------------------------------------
{
	sValue.assign(Parent(P)->value(), Parent(P)->value_size());
}

//-----------------------------------------------------------------------------
KStringView KXMLDocument::GetValue() const
//-----------------------------------------------------------------------------
{
	KStringView sValue;
	GetValue(sValue);
	return sValue;
}

//-----------------------------------------------------------------------------
KStringView KXMLDocument::GetName() const
//-----------------------------------------------------------------------------
{
	KStringView sName;
	sName.assign(Parent(P)->name(), Parent(P)->name_size());
	return sName;
}

//-----------------------------------------------------------------------------
KXMLDocument::XMLPosition KXMLDocument::GetPosition() const
//-----------------------------------------------------------------------------
{
	return P;
}

//-----------------------------------------------------------------------------
bool KXMLDocument::SetPosition(XMLPosition position) const
//-----------------------------------------------------------------------------
{
	if (!position)
	{
		// error
		return false;
	}
	else
	{
		P = position;
		return true;
	}
}

//-----------------------------------------------------------------------------
void KXMLDocument::StartNode() const
//-----------------------------------------------------------------------------
{
	P = D.get();
}

//-----------------------------------------------------------------------------
bool KXMLDocument::DescendNode(KStringView sName) const
//-----------------------------------------------------------------------------
{
	rapidXMLNode* child = Parent(P)->first_node(sName.data(), sName.size());
	if (!child)
	{
		return false;
	}
	else
	{
		P = child;
		return true;
	}
}

//-----------------------------------------------------------------------------
bool KXMLDocument::AscendNode() const
//-----------------------------------------------------------------------------
{
	rapidXMLNode* parent = Parent(P)->parent();
	if (!parent)
	{
		return false;
	}
	else
	{
		P = parent;
		return true;
	}
}

//-----------------------------------------------------------------------------
bool KXMLDocument::NextSibling(KStringView sName, bool bPrevious) const
//-----------------------------------------------------------------------------
{
	rapidXMLNode* sibling;
	if (!bPrevious)
	{
		sibling = Parent(P)->next_sibling(sName.data(), sName.size());
	}
	else
	{
		sibling = Parent(P)->previous_sibling(sName.data(), sName.size());
	}

	if (!sibling)
	{
		return false;
	}
	else
	{
		P = sibling;
		return true;
	}
}

//-----------------------------------------------------------------------------
void KXMLDocument::clear()
//-----------------------------------------------------------------------------
{
	KXML::clear();
	P = D.get();
}

} // end of namespace dekaf2


