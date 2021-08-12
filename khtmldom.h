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

#include "khtmlparser.h"

namespace dekaf2 {


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLElement : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLElement";

//------
public:
//------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

#if (DEKAF2_IS_GCC && DEKAF2_GCC_VERSION < 70000) || defined(_MSC_VER)
	// older GCCs need a default constructor here as they do not honor the using directive below
	KHTMLElement() = default;
#endif

	using KHTMLObject::KHTMLObject;
	using KHTMLObject::Parse;
	using KHTMLObject::Serialize;

	KHTMLElement() = default;
	KHTMLElement(const KHTMLElement& other);
	KHTMLElement(KHTMLElement&&) = default;

	KHTMLElement(KString sName, KStringView sID = KStringView{}, KStringView sClass = KStringView{});
	KHTMLElement(const KHTMLTag& Tag);
	KHTMLElement(KHTMLTag&& Tag);

	virtual bool Parse(KStringView sInput) override;
	virtual void Serialize(KOutStream& OutStream) const override;

	KString Print(char chIndent = '\t', uint16_t iIndent = 0) const;
	void Print(KString& sOut, char chIndent = '\t', uint16_t iIndent = 0) const;
	void Print(KOutStream& OutStream, char chIndent = '\t', uint16_t iIndent = 0) const;

	using value_type = std::unique_ptr<KHTMLObject>;
	using ElementVector = std::vector<value_type>;
	using iterator = ElementVector::iterator;
	using const_iterator = ElementVector::const_iterator;
	using size_type = ElementVector::size_type;

	// data access

	const_iterator begin() const { return Children.begin(); }
	const_iterator end() const { return Children.end(); }
	iterator begin() { return Children.begin(); }
	iterator end() { return Children.end(); }
	const KHTMLObject& operator[](size_type index) const { return *Children[index]; }
	KHTMLObject& operator[](size_type index) { return *Children[index]; }
	size_type size() const { return Children.size(); }

	virtual void clear() override;
	virtual bool empty() const override;
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLElement>(*this); }

	template<typename T,
		typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
	T& Add(T Object)
	{
		auto up = std::make_unique<T>(std::move(Object));
		auto* p = up.get();
		Children.push_back(std::move(up));
		return *p;
	}

	KHTMLElement& Add(KString sElementName, KStringView sID = KStringView{}, KStringView sClass = KStringView{})
	{
		return Add(KHTMLElement(std::move(sElementName), sID, sClass));
	}


	template<typename T,
	typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
	KHTMLElement& operator+=(T Object)
	{
		Add(std::move(Object));
		return *this;
	}

	void AddText(KStringView sContent);

	void AddRawText(KStringView sContent);

	void SetAttribute(KStringView sName, KString sValue)
	{
		Attributes.Set(sName, sValue);
	}

	KStringView GetAttribute(KStringView sName) const
	{
		return Attributes.Get(sName);
	}
	
	void SetID(KString sValue)
	{
		SetAttribute("id", std::move(sValue));
	}

	KStringView GetID() const
	{
		return GetAttribute("id");
	}

	void SetClass(KString sValue)
	{
		SetAttribute("class", std::move(sValue));
	}

	KStringView GetClass() const
	{
		return GetAttribute("class");
	}

	bool IsInline()     const { return KHTMLObject::IsInlineTag(Name);     }
	bool IsStandalone() const { return KHTMLObject::IsStandaloneTag(Name); }

//------
public:
//------

	KString         Name;
	KHTMLAttributes Attributes;
	ElementVector   Children;

}; // KHTMLElement

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTML : public KHTMLParser
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using const_iterator = KHTMLElement::const_iterator;
	using iterator       = KHTMLElement::iterator;

	// parsing is in base class

	void Serialize(KOutStream& Stream, char chIndent = '\t') const;
	void Serialize(KString& sOut, char chIndent = '\t') const;
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

	const KString Error() const
	{
		return m_sError;
	}

	KHTMLElement& DOM()
	{
		return m_Root;
	}

	const KHTMLElement& DOM() const
	{
		return m_Root;
	}

	template<typename T,
	typename std::enable_if<std::is_constructible<KStringView, T>::value == false, int>::type = 0>
	T& Add(T&& o)
	{
		return m_Root.Add(std::forward(o));
	}

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
	bool SetError(KString sError);

	KHTMLElement m_Root;

//------
private:
//------

	std::vector<KHTMLElement*> m_Hierarchy { &m_Root };
	KString m_sContent;
	KString m_sError;
	bool m_bOpenElement  { false };
	bool m_bLastWasSpace { false };
	bool m_bDoNotEscape  { false };

}; // KHTML

} // end of namespace dekaf2
