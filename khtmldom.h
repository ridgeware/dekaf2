/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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

#include "kdefinitions.h"
#include "khtmlparser.h"
#include "klog.h"
#include "kerror.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Basic element of a HTML DOM structure
class DEKAF2_PUBLIC KHTMLElement : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLElement";

//------
public:
//------

	using self = KHTMLElement;

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	using KHTMLObject::KHTMLObject;
	using KHTMLObject::Parse;
	using KHTMLObject::Serialize;

	KHTMLElement() = default;
	KHTMLElement(const KHTMLElement& other);
	KHTMLElement(KHTMLElement&&) = default;
	KHTMLElement& operator=(const KHTMLElement&) = default;
	KHTMLElement& operator=(KHTMLElement&&) = default;

	/// Construct an element with name and set of attributes
	KHTMLElement(KString sName, KHTMLAttributes Attributes);
	/// Construct an element with name, ID, and class
	KHTMLElement(KString sName, KStringView sID = KStringView{}, KStringView sClass = KStringView{});
	/// Copy construct an element from a KHTMLTag
	KHTMLElement(const KHTMLTag& Tag);
	/// Move construct an element from a KHTMLTag
	KHTMLElement(KHTMLTag&& Tag);

	/// Set element name from sInput
	virtual bool Parse(KStringView sInput, bool bDummyParam = false) override;
	/// Serializes element and all children. Same as Print() with default parameters.
	virtual void Serialize(KOutStream& OutStream) const override;

	/// Serializes element and all children. Allows to chose indent character (0 for no indent, default = tab).
	KString Print(char chIndent = '\t', uint16_t iIndent = 0) const;
	/// Serializes element and all children. Allows to chose indent character (0 for no indent, default = tab).
	void Print(KStringRef& sOut, char chIndent = '\t', uint16_t iIndent = 0) const;
	/// Serializes element and all children. Allows to chose indent character (0 for no indent, default = tab).
	bool Print(KOutStream& OutStream, char chIndent = '\t', uint16_t iIndent = 0, bool bIsFirstAfterLinefeed = true, bool bIsInsideHead = false) const;

	using value_type     = std::unique_ptr<KHTMLObject>;
	using ElementVector  = std::vector<value_type>;
	using iterator       = ElementVector::iterator;
	using const_iterator = ElementVector::const_iterator;
	using size_type      = ElementVector::size_type;

	// data access

	const_iterator begin() const { return m_Children.begin(); }
	const_iterator end()   const { return m_Children.end();   }
	iterator       begin()       { return m_Children.begin(); }
	iterator       end()         { return m_Children.end();   }

	const KHTMLObject& operator[](size_type index) const { return *m_Children[index]; }
	KHTMLObject&       operator[](size_type index)       { return *m_Children[index]; }

	/// Returns count of children
	size_type size() const { return m_Children.size(); }
	/// Reset the element
	virtual void clear() override;
	/// Is this element unset?
	virtual bool empty() const override;

	/// Type name of this element
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	/// Type hash of this element
	virtual std::size_t Type() const override { return TYPE; }
	/// Clone this element
	/// @return a copy of the element
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLElement>(*this); }
	/// Copy this element without children
	/// @return a flat copy of the element without children
	KHTMLElement CopyWithoutChildren() const { return KHTMLElement(m_Name, m_Attributes); }

	/// Controls Text element merges when inserting new children
	enum Merge { Not = 0, Before = 1 << 0, After = 1 << 1 };

	/// Insert a Text element into the list of children, return child reference. If the previous or following child element is also a
	/// text element, both elements will be merged and a reference to the existing element returned.
	KHTMLText& Insert(iterator it, KHTMLText Object, Merge merge = Merge(Merge::Before | Merge::After));

	/// Insert an element into the list of children, return child reference
	template<typename T,
	         typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
	T& Insert(iterator it, T Object)
	{
		auto up = std::make_unique<T>(std::move(Object));
		auto* p = up.get();
		m_Children.insert(it, std::move(up));
		return *p;
	}

	/// Add a Text element to the list of children, return child reference. If the previous child element is also a
	/// text element, both elements will be merged and a reference to the previous element returned.
	KHTMLText& Add(KHTMLText Object, Merge merge = Merge::Before);

	/// Add an element to the list of children, return child reference
	template<typename T,
		typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
	T& Add(T Object)
	{
		auto up = std::make_unique<T>(std::move(Object));
		auto* p = up.get();
		m_Children.push_back(std::move(up));
		return *p;
	}

	/// Construct a new KHTMLElement and add it to the list of children, return child reference
	KHTMLElement& Add(KString sElementName, KStringView sID = KStringView{}, KStringView sClass = KStringView{})
	{
		return Add(KHTMLElement(std::move(sElementName), sID, sClass));
	}

	/// Add an element to the list of children, return parent reference
	template<typename T,
	         typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
	T& operator+=(T Object)
	{
		return Add(std::move(Object));
	}

	/// Construct a new KHTMLElement and add it to the list of children, return child reference
	KHTMLElement& operator+=(KString sElementName)
	{
		return Add(KHTMLElement(std::move(sElementName)));
	}

	/// Add a text element to the list of children. Adjacent text elements get merged. Text is automatically escaped.
	/// @param sContent the text content. If empty, no element is added
	self& AddText(KStringView sContent);

	/// Add a text element to the list of children. Adjacent text elements do not get merged. Text is not escaped.
	/// @param sContent the text content. If empty, no element is added
	self& AddRawText(KStringView sContent);

	/// Delete an element from the list of children, returns iterator to next element
	iterator Delete(const_iterator it)
	{
		return m_Children.erase(it);
	}

	/// Set an attribute
	/// @param sName the attribute name
	/// @param sValue the attribute value
	/// @param bRemoveIfEmptyValue removes attribute if value is empty
	self& SetAttribute(KString sName, KString sValue, bool bRemoveIfEmptyValue = false);

	/// Set an attribute
	/// @param sName the attribute name
	/// @param bYesNo boolean attribute value
	template<typename Boolean,
	         typename std::enable_if<std::is_same<Boolean, bool>::value == true, int>::type = 0>
	self& SetAttribute(KString sName, Boolean bYesNo)
	{
		return SetBoolAttribute(std::move(sName), bYesNo);
	}

	/// Set an attribute
	/// @param sName the attribute name
	/// @param iValue numeric attribute value
	template<typename Numeric,
	         typename std::enable_if<std::is_arithmetic<Numeric>::value   == true
	                              && std::is_same<Numeric, bool>::value == false, int>::type = 0>
	self& SetAttribute(KString sName, Numeric iValue, KStringView sFormat = "{}")
	{
		return SetAttribute(std::move(sName), kFormat(sFormat, iValue));
	}

	/// Set all attributes - removes all previously existing attributes
	/// @param Attributes the new set of attributes
	self& SetAttributes(KHTMLAttributes Attributes)
	{
		m_Attributes = std::move(Attributes);
		return *this;
	}

	/// Add multiple attributes to the existing set of attributes
	/// @param Attributes the set of attributes to be added
	self& AddAttributes(const KHTMLAttributes& Attributes)
	{
		m_Attributes += Attributes;
		return *this;
	}

	/// Get the value of an attribute
	/// @param sName the attribute name
	/// @return the attribute value
	KStringView GetAttribute(KStringView sName) const
	{
		return m_Attributes.Get(sName);
	}

	/// Get all attributes on this element
	const KHTMLAttributes& GetAttributes() const
	{
		return m_Attributes;
	}

	/// Check for existence of an attribute
	/// @param sName the attribute name
	/// @return true it exists, otherwise false
	bool HasAttribute(KStringView sName) const
	{
		return m_Attributes.Has(sName);
	}

	/// Remove an attribute
	/// @param sName the attribute name
	void RemoveAttribute(KStringView sName)
	{
		m_Attributes.Remove(sName);
	}

	/// Return element name
	const KString& GetName() const
	{
		return m_Name;
	}

	/// Return children as const reference
	const ElementVector& GetChildren() const
	{
		return m_Children;
	}

	/// Return children as reference
	ElementVector& GetChildren()
	{
		return m_Children;
	}

	/// Set the id attribute. If value is empty, the attribute will be removed.
	/// @param sValue the attribute value
	self& SetID(KString sValue)
	{
		return SetAttribute("id", std::move(sValue), true);
	}

	/// Get the value of the id attribute
	/// @return the attribute value
	KStringView GetID() const
	{
		return GetAttribute("id");
	}

	/// Set the class attribute. If value is empty, the attribute will be removed.
	/// @param sValue the attribute value
	self& SetClass(KString sValue)
	{
		return SetAttribute("class", std::move(sValue), true);
	}

	/// Get the value of the class attribute
	/// @return the attribute value
	KStringView GetClass() const
	{
		return GetAttribute("class");
	}

	/// Is this element an inline element?
	/// @return true if element is inline, false otherwise
	bool IsInline()       const { return KHTMLObject::IsInline(m_Property);     }
	/// Is this element an inline-block element?
	/// @return true if element is inline-block, false otherwise
	bool IsInlineBlock() const { return KHTMLObject::IsInlineBlock(m_Property); }
	/// Is this element a standalone/void element?
	/// @return true if element is standalone, false otherwise
	bool IsStandalone()  const { return KHTMLObject::IsStandalone(m_Property);  }
	/// Is this element a block element?
	/// @return true if element is block, false otherwise
	bool IsBlock()       const { return KHTMLObject::IsBlock(m_Property);       }
	/// Is this element an embedded element?
	/// @return true if element is embedded, false otherwise
	bool IsEmbedded()    const { return KHTMLObject::IsEmbedded(m_Property);    }
	/// Is this element not an inline element inside header?
	/// @return true if element is not an inline element inside header, false otherwise
	bool IsNotInlineInHead() const { return KHTMLObject::IsNotInlineInHead(m_Property); }

	/// Merge adjacent Text elements
	/// @param bHierarchically If set to true will walk all children's children too. Defaults to true.
	/// @return reference to self
	self& MergeTextElements(bool bHierarchically = true);

	/// Walks the DOM from the first child forward, and removes all leading whitespace
	/// @param bStopAtBlockElement if true (the default) traversal will stop at the first block element, even
	/// if no non-whitespace has been seen
	/// @return true if traversal stopped before the end, either due to non-whitespace text or a block element
	bool RemoveLeadingWhitespace(bool bStopAtBlockElement = true);
	/// Walks the DOM from the last child backward, and removes all trailing whitespace
	/// @param bStopAtBlockElement if true (the default) traversal will stop at the first block element, even
	/// if no non-whitespace has been seen
	/// @return true if traversal stopped before the (reverse) end, either due to non-whitespace text or a block element
	bool RemoveTrailingWhitespace(bool bStopAtBlockElement = true);

//------
private:
//------

	self& SetBoolAttribute (KString sName, bool bYesNo);
	/// Append right to left, and return reference to left
	static KHTMLText& AppendTextObject(KHTMLText& left, KHTMLText& right);
	/// Insert right at start of left, and return reference to left
	static KHTMLText& PrependTextObject(KHTMLText& left, KHTMLText& right);

	KString                    m_Name;
	KHTMLAttributes            m_Attributes;
	ElementVector              m_Children;
	KHTMLObject::TagProperty   m_Property      { KHTMLObject::TagProperty::None };

}; // KHTMLElement

DEKAF2_ENUM_IS_FLAG(KHTMLElement::Merge)

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Parses HTML into a DOM structure
class DEKAF2_PUBLIC KHTML : public KErrorBase, public KHTMLParser
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTML();
	KHTML(const KHTML&) = default;
	KHTML(KHTML&&) = default;
	KHTML& operator=(const KHTML&) = default;
	KHTML& operator=(KHTML&&) = default;
	virtual ~KHTML();

	using const_iterator = KHTMLElement::const_iterator;
	using iterator       = KHTMLElement::iterator;

	/// if the HTML is unbalanced (forgotten close tag), for how many levels down should the resynchronisation be tried (default = 2)
	KHTML& SetMaxAutoCloseLevels(std::size_t iMaxLevels) { m_iMaxAutoCloseLevels = iMaxLevels; return *this; }

	// SetThrowOnError is in base class

	// parsing is in base class

	void Serialize(KOutStream& Stream, char chIndent = '\t') const;
	void Serialize(KStringRef& sOut, char chIndent = '\t') const;
	KString Serialize(char chIndent = '\t') const;

	/// Clear all content
	void clear();

	/// No content?
	bool empty() const
	{
		return m_Root.empty();
	}

	/// true if we have content
	operator bool() const
	{
		return !empty();
	}

	/// Return begin iterator
	iterator begin()
	{
		return m_Root.begin();
	}

	/// Return begin iterator
	const_iterator begin() const
	{
		return m_Root.begin();
	}

	/// Return end iterator
	const_iterator end() const
	{
		return m_Root.end();
	}

	/// Return end iterator
	iterator end()
	{
		return m_Root.end();
	}

	const std::vector<KString>& Issues() const
	{
		return m_Issues;
	}

	/// Return reference on root element
	KHTMLElement& DOM()
	{
		return m_Root;
	}

	/// Return const reference on root element
	const KHTMLElement& DOM() const
	{
		return m_Root;
	}

	/// Proxy KHTMLElement's Add() to add an element to the list of children. Returns reference to child.
	template<typename T,
	typename std::enable_if<std::is_constructible<KStringView, T>::value == false, int>::type = 0>
	T& Add(T&& o)
	{
		return m_Root.Add(std::forward(o));
	}

	/// Proxy KHTMLElement's Add() to construct and add an element to the list of children. Returns reference to child.
	KHTMLElement& Add(KStringView sElementName)
	{
		return m_Root.Add(KHTMLElement(sElementName));
	}

//------
protected:
//------

	virtual void Object(KHTMLObject& Object) override;
	virtual void Content(char ch) override;
	virtual void Script(char ch) override;
	virtual void Invalid(char ch) override;
	virtual void Finished() override;

	void FlushText();
	void SetIssue(KString sIssue);

//------
private:
//------

	KHTMLElement               m_Root;
	std::vector<KHTMLElement*> m_Hierarchy     { &m_Root };
	KString                    m_sContent;
	std::vector<KString>       m_Issues;
	std::size_t                m_iMaxAutoCloseLevels { 2 };
	bool                       m_bLastWasSpace { true  };
	bool                       m_bDoNotEscape  { false };
	bool                       m_bInsideStyle  { false };

}; // KHTML

DEKAF2_NAMESPACE_END
