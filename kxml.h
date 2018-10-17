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

#pragma once

#include <iterator>
#include "kstringview.h"
#include "kstring.h"
#include "kreader.h"
#include "kwriter.h"


namespace dekaf2 {

class KXMLNode;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Accessor to the attributes of one KXMLNode; is also its own iterator
class KXMLAttribute
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KXMLNode;

//------
public:
//------

	using iterator_category = std::bidirectional_iterator_tag;
	using const_iterator    = KXMLAttribute;
	using iterator          = const_iterator;

	/// default ctor
	KXMLAttribute() = default;
	/// ctor from a parent node
	KXMLAttribute(const KXMLNode& node);

	/// returns count of attributes
	size_t size() const;

	/// true if there are no attributes
	bool empty() const
	{
		return !m_attribute;
	}

	/// true if there are attributes
	operator bool() const
	{
		return m_attribute;
	}

	/// ascend to parent node
	KXMLNode Parent() const;

	/// find attribute by name
	KXMLAttribute find(KStringView sName) const;

	/// return iterator on set of attributes
	iterator begin() const
	{
		return *this;
	}
	/// end iterator
	iterator end() const
	{
		return {};
	}

	KXMLAttribute& operator*() { return *this; }
	KXMLAttribute* operator&() { return this;  }

	/// iterate to next attribute
	KXMLAttribute& operator++();     // prefix
	/// iterate to next attribute
	KXMLAttribute  operator++(int);  // postfix
	/// iterate to previous attribute
	KXMLAttribute& operator--();     // prefix
	/// iterate to previous attribute
	KXMLAttribute  operator--(int);  // postfix

	/// Get next attribute of parent node
	KXMLAttribute Next() const
	{
		auto sibling = *this;
		return ++sibling;
	}

	/// Get attribute name
	KStringView GetName() const;
	/// Get attribute value
	KStringView GetValue() const;

	/// Set name of current attribute
	KXMLAttribute& SetName(KStringView sName);
	/// Set value of current attribute
	KXMLAttribute& SetValue(KStringView sValue);

	/// Add an attribute to parent of current attribute
	KXMLAttribute AddAttribute(KStringView sName, KStringView sValue = KStringView{});

	bool operator==(const KXMLAttribute& other) const
	{
		return m_attribute == other.m_attribute;
	}

	bool operator!=(const KXMLAttribute& other) const
	{
		return m_attribute != other.m_attribute;
	}

//------
protected:
//------

	void* m_attribute { nullptr };

}; // KXMLAttribute

class KXML;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Accessor to one XML node and its attributes and children; is also its own iterator
class KXMLNode
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KXMLAttribute;

//------
public:
//------

	using iterator_category = std::bidirectional_iterator_tag;
	using const_iterator    = KXMLNode;
	using iterator          = const_iterator;

	/// ctor for empty node
	KXMLNode() = default;
	/// ctor from KXML root node
	KXMLNode(const KXML& DOM);

	/// returns count of child nodes
	size_t size() const;

	/// true if this node is empty / unexisting
	bool empty() const
	{
		return !m_node;
	}

	/// true if this node is not empty / unexisting
	operator bool() const
	{
		return m_node;
	}

	/// Ascend to parent node
	KXMLNode Parent() const;

	/// Descend to first child node
	KXMLNode Child() const;
	/// Get next sibling node
	KXMLNode Next() const
	{
		auto sibling = *this;
		return ++sibling;
	}
	/// Descend to first child node with sName
	KXMLNode Child(KStringView sName) const;
	/// Get next sibling node with sName
	KXMLNode Next(KStringView sName) const;

	/// return begin iterator
	iterator begin() const
	{
		return Child();
	}
	/// return end iterator
	iterator end() const
	{
		return {};
	}

	KXMLNode& operator*() { return *this; }
	KXMLNode* operator&() { return this;  }

	/// iterate to next node
	KXMLNode& operator++();     // prefix
	/// iterate to next node
	KXMLNode  operator++(int);  // postfix
	/// iterate to previous node
	KXMLNode& operator--();     // prefix
	/// iterate to previous node
	KXMLNode  operator--(int);  // postfix

	/// Get name of current node
	KStringView GetName() const;
	/// Get value of current node
	KStringView GetValue() const;
	/// Get attributes of current node
	KXMLAttribute Attributes() const
	{
		return KXMLAttribute(*this);
	}
	/// Get attribute with sName
	KXMLAttribute Attribute(KStringView sName) const;

	/// Add a child node to current node. Only works if the current node is
	/// not empty (= part of a KXML tree).
	KXMLNode AddNode(KStringView sName, KStringView sValue = KStringView{});
	/// Set name of current node. Only works if the current node is
	/// not empty (= part of a KXML tree).
	KXMLNode& SetName(KStringView sName);
	/// Set value of current node. Only works if the current node is
	/// not empty (= part of a KXML tree).
	KXMLNode& SetValue(KStringView sValue);

	/// Add an attribute to current node. Only works if the current node is
	/// not empty (= part of a KXML tree).
	KXMLAttribute AddAttribute(KStringView sName, KStringView sValue = KStringView{});

	bool operator==(const KXMLNode& other) const
	{
		return m_node == other.m_node;
	}

	bool operator!=(const KXMLNode& other) const
	{
		return m_node != other.m_node;
	}

//------
protected:
//------

	void* m_node { nullptr };

}; // KXMLNode


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Basic XML support. Fast XML parsing and serialization.
class KXML
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KXMLNode;

//------
public:
//------

	using const_iterator    = KXMLNode;
	using iterator          = const_iterator;

	/// Construct an empty KXML DOM
	KXML();
	/// Construct a KXML DOM by parsing sDocument - content gets copied
	KXML(KStringView sDocument, bool bWhitespaceOnlyDataNodes = false);
	/// Construct a KXML DOM by parsing InStream
	KXML(KInStream& InStream, bool bWhitespaceOnlyDataNodes = false);
	KXML(KInStream&& InStream, bool bWhitespaceOnlyDataNodes = false);

	KXML(const KXML&) = delete;
	KXML(KXML&&) = default;
	KXML& operator=(const KXML&) = delete;
	KXML& operator=(KXML&&) = default;

	/// Print DOM into OutStream
	void Serialize(KOutStream& OutStream, bool bIndented = true) const;
	/// Print DOM into string
	void Serialize(KString& string, bool bIndented = true) const;
	/// Print DOM into string
	KString Serialize(bool bIndented = true) const;

	/// Parse DOM from InStream
	bool Parse(KInStream& InStream, bool bWhitespaceOnlyDataNodes = false);
	/// Parse DOM from string
	void Parse(KStringView string, bool bWhitespaceOnlyDataNodes = false);

	/// Clear all content
	void clear();

	/// Add a default XML declaration to the start of DOM
	void AddXMLDeclaration();

	/// Return first child node with sName
	KXMLNode Child(KStringView sName) const
	{
		return KXMLNode(*this).Child(sName);
	}

	/// Return begin iterator
	iterator begin() const
	{
		return KXMLNode(*this).Child();
	}

	/// Return end iterator
	iterator end() const
	{
		return {};
	}

//------
protected:
//------

	void Parse(bool bWhitespaceOnlyDataNodes = false);

	// helper types to allow for a unique_ptr<void>, which lets us hide all
	// implementation headers from the interface and nonetheless keep exception safety
	using deleter_t = std::function<void(void *)>;
	using unique_void_ptr = std::unique_ptr<void, deleter_t>;

	unique_void_ptr D;
	KString XMLData;

}; // KXML

//-----------------------------------------------------------------------------
inline KXMLNode::KXMLNode(const KXML& DOM)
//-----------------------------------------------------------------------------
: m_node { DOM.D.get() }
{
}

} // end of namespace dekaf2
