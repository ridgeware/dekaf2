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
#include "bits/kunique_deleter.h"

namespace dekaf2 {

class KXMLNode;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Accessor to the attributes of one KXMLNode; is also its own iterator
class DEKAF2_PUBLIC KXMLAttribute
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
	const KXMLAttribute  operator++(int);  // postfix
	/// iterate to previous attribute
	KXMLAttribute& operator--();     // prefix
	/// iterate to previous attribute
	const KXMLAttribute  operator--(int);  // postfix

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
class DEKAF2_PUBLIC KXMLNode
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KXMLAttribute;
	friend class KXML;

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
	const KXMLNode  operator++(int);  // postfix
	/// iterate to previous node
	KXMLNode& operator--();     // prefix
	/// iterate to previous node
	const KXMLNode  operator--(int);  // postfix

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
	/// When printing, do not indent nor add linefeed for all children of this node,
	/// thus preserve space. Only valid for Element nodes.
	KXMLNode& SetInlineRoot(bool bInlineRoot = true);
	/// Is the InlineRoot property set for this element?
	bool IsInlineRoot() const;
	/// Set name of current node. Only works if the current node is
	/// not empty (= part of a KXML tree).
	KXMLNode& SetName(KStringView sName);
	/// Set value of current node. Only works if the current node is
	/// not empty (= part of a KXML tree). This sets the first data child node.
	KXMLNode& SetValue(KStringView sValue);
	/// Add value to current node. Only works if the current node is
	/// not empty (= part of a KXML tree). This adds another data child node.
	KXMLNode& AddValue(KStringView sValue);

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
class DEKAF2_PUBLIC KXML
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KXMLNode;

//------
public:
//------

	enum PrintFlags { Default = 0, NoIndents = 1, NoLinefeeds = 2, Terse = NoIndents | NoLinefeeds };

	using const_iterator    = KXMLNode;
	using iterator          = const_iterator;

	/// Construct an empty KXML DOM
	KXML();
	/// Construct a KXML DOM by parsing sDocument - content gets copied
	KXML(KStringView sDocument, bool bPreserveWhiteSpace = false, KStringView sCreateRoot = KStringView{});
	/// Construct a KXML DOM by parsing InStream
	KXML(KInStream& InStream, bool bPreserveWhiteSpace = false, KStringView sCreateRoot = KStringView{});
	KXML(KInStream&& InStream, bool bPreserveWhiteSpace = false, KStringView sCreateRoot = KStringView{});

	KXML(const KXML&) = delete;
	KXML(KXML&&) = default;
	KXML& operator=(const KXML&) = delete;
	KXML& operator=(KXML&&) = default;

	/// returns false if exceptions are disabled GetLastError() and return codes are in use
	bool GetThrowOnParseError () const
	{
		return m_bThrowOnParseError;
	}

	/// set false if you want to disable exceptions and use GetLastError() and return codes, returns old value (in case you want to restore it)
	bool SetThrowOnParseError (bool bTrueFalse)
	{
		auto bOldValue = m_bThrowOnParseError;
		m_bThrowOnParseError = bTrueFalse;
		return bOldValue;
	}

	const KString& GetLastError () const
	{
		return m_sLastError;
	}

	/// Print DOM into OutStream
	void Serialize(KOutStream& OutStream, int iPrintFlags = PrintFlags::Default, KStringView sDropRoot = KStringView{}) const;
	/// Print DOM into OutStream
	void Serialize(KOutStream&& OutStream, int iPrintFlags = PrintFlags::Default, KStringView sDropRoot = KStringView{}) const;
	/// Print DOM into string
	void Serialize(KStringRef& string, int iPrintFlags = PrintFlags::Default, KStringView sDropRoot = KStringView{}) const;
	/// Print DOM into string
	KString Serialize(int iPrintFlags = PrintFlags::Default, KStringView sDropRoot = KStringView{}) const;

	/// Parse DOM from InStream
	bool Parse(KInStream& InStream, bool bPreserveWhiteSpace = false, KStringView sCreateRoot = KStringView{});
	/// Parse DOM from InStream
	bool Parse(KInStream&& InStream, bool bPreserveWhiteSpace = false, KStringView sCreateRoot = KStringView{});
	/// Parse DOM from string
	bool Parse(KStringView string, bool bPreserveWhiteSpace = false, KStringView sCreateRoot = KStringView{});

	/// Clear all content
	void clear();

	/// No content?
	bool empty() const
	{
		return begin() == end();
	}

	/// true if we have content
	operator bool() const
	{
		return !empty();
	}
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

	bool Parse(bool bPreserveWhiteSpace = false);

	KUniqueVoidPtr D;
	KString        XMLData;
	KString        m_sLastError;
	bool           m_bThrowOnParseError{true}; // <-- defaults to true (we will throw)

}; // KXML

//-----------------------------------------------------------------------------
inline KXMLNode::KXMLNode(const KXML& DOM)
//-----------------------------------------------------------------------------
: m_node { DOM.D.get() }
{
}

} // end of namespace dekaf2
