/*
 //-----------------------------------------------------------------------------//
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

#include "kstringview.h"
#include "kstring.h"
#include "kstream.h"
#include "kbufferedreader.h"
#include <set>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Interface
class KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum ObjectType
	{
		NONE,
		CONTENT,
		TAG,
		COMMENT,
		DOCUMENTTYPE,
		PROCESSINGINSTRUCTION,
		CDATA,
		INVALID
	};

	KHTMLObject() = default;
	KHTMLObject(const KHTMLObject&) = default;
	KHTMLObject(KHTMLObject&&) = default;
	KHTMLObject& operator=(const KHTMLObject&) = default;
	KHTMLObject& operator=(KHTMLObject&&) = default;

	virtual ~KHTMLObject();

	virtual bool Parse(KInStream& InStream, KStringView sOpening = KStringView{});
	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{});
	virtual void Serialize(KOutStream& OutStream) const;

	virtual bool Parse(KStringView sInput);
	virtual void Serialize(KString& sOut) const;

	KHTMLObject& operator=(KStringView sInput)
	{
		Parse(sInput);
		return *this;
	}

	virtual void clear();
	// make this base an ABC
	virtual bool empty() const = 0;
	virtual ObjectType Type() const;

}; // KHTMLObject

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an object with text and a lead-in and lead-out string
class KHTMLStringObject : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLStringObject(KStringView sLeadIn, KStringView sLeadOut)
	: m_sLeadIn(sLeadIn)
	, m_sLeadOut(sLeadOut)
	{}

	// forward all base class constructors
	using KHTMLObject::KHTMLObject;

	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}) override;
	virtual void Serialize(KOutStream& OutStream) const override;

	virtual void clear() override;
	virtual bool empty() const override;

	KString Value {}; // {} = make sure Value is initialized, we may not have a constructor here!

	KStringView LeadIn() const
	{
		return m_sLeadIn;
	}

	KStringView LeadOut() const
	{
		return m_sLeadOut;
	}

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) = 0;

	KStringView m_sLeadIn {};
	KStringView m_sLeadOut {};

}; // KHTMLStringObject

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLAttribute : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

#if (!DEKAF2_NO_GCC && DEKAF2_GCC_VERSION < 70000) || defined(_MSC_VER)
	// older GCCs need a default constructor here as they do not honor the using directive below
	KHTMLAttribute() = default;
#endif

	KHTMLAttribute(KStringView sName, KStringView sValue=KStringView{}, char _Quote='"')
	: Name(sName)
	, Value(sValue)
	, Quote(_Quote)
	{
	}

	using KHTMLObject::KHTMLObject;

	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}) override;
	virtual void Serialize(KOutStream& OutStream) const override;
	virtual void Serialize(KString& sOut) const override;

	virtual void clear() override;
	virtual bool empty() const override;

	KString Name {};
	KString Value {};
	mutable std::iostream::int_type Quote { 0 };

}; // KHTMLAttribute

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLAttributes : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
private:
//------

	struct less_for_attribute
	{
		// enable find(const T&) overloads in C++14
		using is_transparent = void;

		//-----------------------------------------------------------------------------
		bool operator()(const KHTMLAttribute& a1, const KHTMLAttribute& a2) const
		//-----------------------------------------------------------------------------
		{
			return a1.Name < a2.Name;
		}

		// add comparators for strings so that C++14 finds them with find()
		//-----------------------------------------------------------------------------
		bool operator()(KStringView s1, const KHTMLAttribute& a2) const
		//-----------------------------------------------------------------------------
		{
			return s1 < a2.Name;
		}

		//-----------------------------------------------------------------------------
		bool operator()(const KHTMLAttribute& a1, KStringView s2) const
		//-----------------------------------------------------------------------------
		{
			return a1.Name < s2;
		}

	};

//------
public:
//------

	using AttributeList = std::set<KHTMLAttribute, less_for_attribute>;

	AttributeList::iterator begin()
	{
		return m_Attributes.begin();
	}

	AttributeList::iterator end()
	{
		return m_Attributes.end();
	}

	AttributeList::const_iterator begin() const
	{
		return m_Attributes.begin();
	}

	AttributeList::const_iterator end() const
	{
		return m_Attributes.end();
	}

	KStringView Get(KStringView sAttributeName) const;

	void Set(KStringView sAttributeName, KStringView sAttributeValue, char Quote='"')
	{
		Replace(KHTMLAttribute({sAttributeName, sAttributeValue, Quote}));
	}

	bool Add(KHTMLAttribute&& Attribute);
	bool Add(const KHTMLAttribute& Attribute)
	{
		return Add(KHTMLAttribute(Attribute));
	}

	void Replace(KHTMLAttribute&& Attribute);
	void Replace(const KHTMLAttribute& Attribute)
	{
		Replace(KHTMLAttribute(Attribute));
	}

	KHTMLAttributes& operator+=(const KHTMLAttribute& Attribute)
	{
		Add(Attribute);
		return *this;
	}

	KHTMLAttributes& operator+=(KHTMLAttribute&& Attribute)
	{
		Add(std::move(Attribute));
		return *this;
	}

	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}) override;
	virtual void Serialize(KOutStream& OutStream) const override;
	virtual void Serialize(KString& sOut) const override;

	virtual void clear() override;
	virtual bool empty() const override;

//------
protected:
//------

	AttributeList m_Attributes;

}; // KHTMLAttributes

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLTag : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

#if (!DEKAF2_NO_GCC && DEKAF2_GCC_VERSION < 70000) || defined(_MSC_VER)
	// older GCCs need a default constructor here as they do not honor the using directive below
	KHTMLTag() = default;
#endif

	KHTMLTag(KStringView sInput)
	{
		KHTMLObject::Parse(sInput);
	}

	KHTMLTag(KBufferedReader& InStream, KStringView sOpening = KStringView{})
	{
		Parse(InStream, sOpening);
	}

	using KHTMLObject::KHTMLObject;

	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}) override;
	virtual void Serialize(KOutStream& OutStream) const override;
	virtual void Serialize(KString& sOut) const override;

	virtual void clear() override;
	virtual bool empty() const override;
	virtual ObjectType Type() const override;

	bool IsInline() const;

//------
public:
//------

	KString         Name;
	KHTMLAttributes Attributes;
	bool            bSelfClosing { false };
	bool            bClosing { false };

}; // KHTMLTag


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML comment
class KHTMLComment : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLComment()
	: KHTMLStringObject(LEAD_IN, LEAD_OUT)
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual ObjectType Type() const override;

	static constexpr KStringView LEAD_IN  = "<!--";
	static constexpr KStringView LEAD_OUT = "-->";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLComment

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML document type declaration
class KHTMLDocumentType : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLDocumentType()
	: KHTMLStringObject(LEAD_IN, LEAD_OUT)
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual ObjectType Type() const override;

	static constexpr KStringView LEAD_IN  = "<!";
	static constexpr KStringView LEAD_OUT = ">";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLDocumentType

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML processing instruction
class KHTMLProcessingInstruction : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLProcessingInstruction()
	: KHTMLStringObject(LEAD_IN, LEAD_OUT)
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual ObjectType Type() const override;

	static constexpr KStringView LEAD_IN  = "<?";
	static constexpr KStringView LEAD_OUT = "?>";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLProcessingInstruction

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML CDATA section
class KHTMLCData : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLCData()
	: KHTMLStringObject(LEAD_IN, LEAD_OUT)
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual ObjectType Type() const override;

	static constexpr KStringView LEAD_IN  = "<![CDATA[";
	static constexpr KStringView LEAD_OUT = "]]>";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLProcessingInstruction

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLParser
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLParser() = default;
	KHTMLParser(const KHTMLParser&) = default;
	KHTMLParser(KHTMLParser&&) = default;
	KHTMLParser& operator=(const KHTMLParser&) = default;
	KHTMLParser& operator=(KHTMLParser&&) = default;

	KHTMLParser(KInStream& InStream)
	{
		Parse(InStream);
	}

	KHTMLParser(KBufferedReader& InStream)
	{
		Parse(InStream);
	}

	KHTMLParser(KStringView sInput)
	{
		Parse(sInput);
	}

	virtual ~KHTMLParser();

	virtual bool Parse(KInStream& InStream);
	virtual bool Parse(KBufferedReader& InStream);
	virtual bool Parse(KStringView sInput);

//------
protected:
//------

	virtual void Object(KHTMLObject& Object);
	virtual void Content(char ch);
	virtual void Script(char ch);
	virtual void Invalid(char ch);
	virtual void Finished();

//------
private:
//------

	void Invalid(KStringView sInvalid);
	void Invalid(const KHTMLStringObject& Object);
	void SkipScript(KBufferedReader& InStream);
	void SkipInvalid(KBufferedReader& InStream);

}; // KHTMLParser


} // end of namespace dekaf2

