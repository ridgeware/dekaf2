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

#include "krapidxml.h"
#include "libs/rapidxml-1.13/rapidxml.hpp"
#include "libs/rapidxml-1.13/rapidxml_print.hpp"

namespace dekaf2 {

using Ch           = char;
using XMLDoc       = rapidxml::xml_document<Ch>;
using XMLNode      = rapidxml::xml_node<Ch>;
using XMLAttribute = rapidxml::xml_attribute<Ch>;

// static helpers

//-----------------------------------------------------------------------------
Ch* CreateString(XMLDoc* Document, RapidXMLDocument::StringView str)
//-----------------------------------------------------------------------------
{
	return Document->allocate_string(str.data(), str.size());
}

//-----------------------------------------------------------------------------
XMLNode* CreateNode(XMLDoc* Document, RapidXMLDocument::StringView sName, RapidXMLDocument::StringView sValue, rapidxml::node_type type)
//-----------------------------------------------------------------------------
{
	return Document->allocate_node(type, CreateString(Document, sName), CreateString(Document, sValue), 0, sValue.size());
}

//-----------------------------------------------------------------------------
XMLAttribute* CreateAttribute(XMLDoc* Document, RapidXMLDocument::StringView sName, RapidXMLDocument::StringView sValue)
//-----------------------------------------------------------------------------
{
	return Document->allocate_attribute(CreateString(Document, sName), CreateString(Document, sValue));
}

//-----------------------------------------------------------------------------
inline XMLDoc* Document(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<XMLDoc*>(p);
}

//-----------------------------------------------------------------------------
inline XMLNode* Parent(void* p)
//-----------------------------------------------------------------------------
{
	return static_cast<XMLNode*>(p);
}

//-----------------------------------------------------------------------------
RapidXMLDocument::RapidXMLDocument()
//-----------------------------------------------------------------------------
{
	auto deleter = [](void* data)
	{
		auto p = static_cast<XMLDoc*>(data);
		delete p;
	};
	D = unique_void_ptr(new XMLDoc, deleter);
	P = D.get();
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::AddNode(StringView sName, StringView sValue, bool bDescendInto)
//-----------------------------------------------------------------------------
{
	XMLNode* node = CreateNode(Document(D.get()), sName, sValue, rapidxml::node_element);
	Parent(P)->append_node(node);
	if (bDescendInto)
	{
		P = node;
	}
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::AddAttribute(StringView sName, StringView sValue)
//-----------------------------------------------------------------------------
{
	XMLAttribute* attribute = CreateAttribute(Document(D.get()), sName, sValue);
	Parent(P)->append_attribute(attribute);
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::AddValue(StringView sValue)
//-----------------------------------------------------------------------------
{
	Ch* value = CreateString(Document(D.get()), sValue);
	Parent(P)->value(value, sValue.size());
}

//-----------------------------------------------------------------------------
bool RapidXMLDocument::GetNode(StringView sName, StringView& sValue, bool bDescendInto) const
//-----------------------------------------------------------------------------
{
	XMLNode* node = Parent(P)->first_node(sName.data(), sName.size());
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
RapidXMLDocument::StringView RapidXMLDocument::GetNode(StringView sName) const
//-----------------------------------------------------------------------------
{
	StringView sValue;
	GetNode(sName, sValue);
	return sValue;
}

//-----------------------------------------------------------------------------
bool RapidXMLDocument::GetSibling(StringView sName, StringView& sValue, bool bPrevious) const
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
bool RapidXMLDocument::GetAttribute(StringView sName, StringView& sValue) const
//-----------------------------------------------------------------------------
{
	XMLAttribute* attribute = Parent(P)->first_attribute(sName.data(), sName.size());
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
RapidXMLDocument::StringView RapidXMLDocument::GetAttribute(StringView sName) const
//-----------------------------------------------------------------------------
{
	StringView sValue;
	GetAttribute(sName, sValue);
	return sValue;
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::GetValue(StringView& sValue) const
//-----------------------------------------------------------------------------
{
	sValue.assign(Parent(P)->value(), Parent(P)->value_size());
}

//-----------------------------------------------------------------------------
RapidXMLDocument::StringView RapidXMLDocument::GetValue() const
//-----------------------------------------------------------------------------
{
	StringView sValue;
	GetValue(sValue);
	return sValue;
}

//-----------------------------------------------------------------------------
RapidXMLDocument::StringView RapidXMLDocument::GetName() const
//-----------------------------------------------------------------------------
{
	return Parent(P)->name();
}

//-----------------------------------------------------------------------------
RapidXMLDocument::XMLPosition RapidXMLDocument::GetPosition() const
//-----------------------------------------------------------------------------
{
	return P;
}

//-----------------------------------------------------------------------------
bool RapidXMLDocument::SetPosition(XMLPosition position) const
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
void RapidXMLDocument::StartNode() const
//-----------------------------------------------------------------------------
{
	P = D.get();
}

//-----------------------------------------------------------------------------
bool RapidXMLDocument::DescendNode(StringView sName) const
//-----------------------------------------------------------------------------
{
	XMLNode* child = Parent(P)->first_node(sName.data(), sName.size());
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
bool RapidXMLDocument::AscendNode() const
//-----------------------------------------------------------------------------
{
	XMLNode* parent = Parent(P)->parent();
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
bool RapidXMLDocument::NextSibling(StringView sName, bool bPrevious) const
//-----------------------------------------------------------------------------
{
	XMLNode* sibling;
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
void RapidXMLDocument::AddXMLDeclaration()
//-----------------------------------------------------------------------------
{
	XMLNode* DocType = CreateNode(Document(D.get()), "", "", rapidxml::node_declaration);
	DocType->append_attribute(CreateAttribute(Document(D.get()), "version", "1.0"));
	DocType->append_attribute(CreateAttribute(Document(D.get()), "encoding", "utf-8"));
	DocType->append_attribute(CreateAttribute(Document(D.get()), "standalone", "yes"));
	Document(D.get())->prepend_node(DocType);
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::Serialize(KOutStream& OutStream) const
//-----------------------------------------------------------------------------
{
	print(OutStream.OutStream(), *Document(D.get()));
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::Serialize(String& xstring) const
//-----------------------------------------------------------------------------
{
	std::string string;
	print(std::back_inserter(string), *Document(D.get()));
}

//-----------------------------------------------------------------------------
RapidXMLDocument::String RapidXMLDocument::Serialize() const
//-----------------------------------------------------------------------------
{
	String string;
	Serialize(string);
	return string;
}

//-----------------------------------------------------------------------------
bool RapidXMLDocument::Parse(KInStream& InStream)
//-----------------------------------------------------------------------------
{
	clear();

	std::istream& stream = InStream.InStream();
	stream.unsetf(std::ios::skipws);
	XMLData.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	if (stream.fail())
	{
		return false;
	}
	else
	{
		XMLData.push_back(0);
		Parse();
		return true;
	}
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::Parse(StringView string)
//-----------------------------------------------------------------------------
{
	clear();

	XMLData.reserve(string.size() + 1);
	XMLData.assign(string.begin(), string.end());
	XMLData.push_back(0);
	Parse();
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::clear()
//-----------------------------------------------------------------------------
{
	Document(D.get())->clear();
	XMLData.clear();
	P = D.get();
}

//-----------------------------------------------------------------------------
void RapidXMLDocument::Parse()
//-----------------------------------------------------------------------------
{
	Document(D.get())->parse<rapidxml::parse_default>(&XMLData.front());
	P = D.get();
}


} // end of namespace dekaf2
