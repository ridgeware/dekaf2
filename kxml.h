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

	/// Get attribute name
	KStringView GetName() const;
	/// Get attribute value
	KStringView GetValue() const;

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
class KXMLDocument;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Accessor to one XML node; is also its own iterator
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
	/// ctor from current node in KXMLDocument
	KXMLNode(const KXMLDocument& document);

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

	/// find a child node
	KXMLNode find(KStringView sName) const;

	/// return begin iterator
	iterator begin() const;
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
	/// Get Attributes of current node
	KXMLAttribute GetAttributes() const
	{
		return KXMLAttribute(*this);
	}

	/// Add a child node to current node
	KXMLNode AddNode(KStringView sName, KStringView sValue = KStringView{});
	/// Set name of current node
	void SetName(KStringView sName);
	/// Set value of current node
	void SetValue(KStringView sValue);

	/// Add an attribute to current node
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
/// Basic XML support. Fast XML parsing and DOM traversal.
class KXML
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KXMLNode;

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct an empty KXML DOM
	KXML();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a KXML DOM by parsing sDocument - content gets copied
	KXML(KStringView sDocument);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a KXML DOM by parsing InStream
	KXML(KInStream& InStream);
	//-----------------------------------------------------------------------------

	KXML(const KXML&) = delete;
	KXML(KXML&&) = default;
	KXML& operator=(const KXML&) = delete;
	KXML& operator=(KXML&&) = default;

	//-----------------------------------------------------------------------------
	/// Print DOM into OutStream
	void Serialize(KOutStream& OutStream) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Print DOM into string
	void Serialize(KString& string) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Print DOM into string
	KString Serialize() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Parse DOM from InStream
	bool Parse(KInStream& InStream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Parse DOM from string
	void Parse(KStringView string);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Clear all content
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a default XML declaration to the start of DOM
	void AddXMLDeclaration();
	//-----------------------------------------------------------------------------

//------
protected:
//------

	//-----------------------------------------------------------------------------
	virtual void Clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void Parse();
	//-----------------------------------------------------------------------------

	// helper types to allow for a unique_ptr<void>, which lets us hide all
	// implementation headers from the interface and nonetheless keep exception safety
	using deleter_t = std::function<void(void *)>;
	using unique_void_ptr = std::unique_ptr<void, deleter_t>;

	unique_void_ptr D;
	KString XMLData;

}; // KXML


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KXMLDocument adds string based traversal and manipulation methods to a KXML
/// DOM.
class KXMLDocument : public KXML
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KXMLNode;

//------
public:
//------

	using XMLPosition  = void*;

	// make base class constructors available
	using KXML::KXML;

	//-----------------------------------------------------------------------------
	/// Add a node as child to the current node
	void AddNode(KStringView sName, KStringView sValue = KStringView{}, bool bDescendInto = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add an attribute to the current node
	void AddAttribute(KStringView sName, KStringView sValue = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a value to the current node's value
	void AddValue(KStringView sValue);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the value of the current node
	void SetValue(KStringView sValue);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get value of node with name sName, descend into it if bDescendInto is true
	bool GetNode(KStringView sName, KStringView& sValue, bool bDescendInto = false) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get value of node with name sName
	KStringView GetNode(KStringView sName) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get sibling of current node with name sName
	bool GetSibling(KStringView sName, KStringView& sValue, bool bPrevious = false) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get attribute value of attribute sName of current node
	bool GetAttribute(KStringView sName, KStringView& sValue) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get attribute value of attribute sName of current node
	KStringView GetAttribute(KStringView sName) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get value of current node
	void GetValue(KStringView& sValue) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get value of current node
	KStringView GetValue() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get name of current node
	KStringView GetName() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a pointer to the current position in the DOM tree
	XMLPosition GetPosition() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the position in the DOM tree
	bool SetPosition(XMLPosition position) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the position to the start node in the DOM
	void StartNode() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Descend to first child of current node
	bool DescendNode(KStringView sName) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ascend to parent of current node
	bool AscendNode() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Advance to next sibling of name sName of current node
	bool NextSibling(KStringView sName, bool bPrevious = false) const;
	//-----------------------------------------------------------------------------

//------
protected:
//------

	//-----------------------------------------------------------------------------
	/// Clear all content - called from base through virtual inheritance
	virtual void Clear() override;
	//-----------------------------------------------------------------------------

	mutable void* P { D.get() };

}; // KXMLDocument


//-----------------------------------------------------------------------------
inline KXMLNode::KXMLNode(const KXML& DOM)
//-----------------------------------------------------------------------------
: m_node { DOM.D.get() }
{
}

//-----------------------------------------------------------------------------
inline KXMLNode::KXMLNode(const KXMLDocument& document)
//-----------------------------------------------------------------------------
: m_node { document.P }
{
}

} // end of namespace dekaf2
