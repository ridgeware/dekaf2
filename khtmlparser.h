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
#include "kstream.h"
#include <set>

namespace dekaf2 {


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLAttribute
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLAttribute() = default;
	KHTMLAttribute(const KHTMLAttribute&) = default;
	KHTMLAttribute(KHTMLAttribute&&) = default;
	KHTMLAttribute& operator=(const KHTMLAttribute&) = default;
	KHTMLAttribute& operator=(KHTMLAttribute&&) = default;

	KHTMLAttribute(KStringView sName, KStringView sValue, char _Quote='"')
	: Name(sName)
	, Value(sValue)
	, Quote(_Quote)
	{
	}

	KHTMLAttribute(KStringView sInput)
	{
		Parse(sInput);
	}

	KHTMLAttribute(KInStream& InStream)
	{
		Parse(InStream);
	}

	void Parse(KStringView sInput);
	void Parse(KInStream& InStream);
	void Serialize(KOutStream& OutStream) const;

	KHTMLAttribute& operator=(KStringView sInput)
	{
		Parse(sInput);
		return *this;
	}

	void clear();
	bool empty() const
	{
		return Name.empty();
	}

	KString Name;
	KString Value;
	mutable std::iostream::int_type Quote { 0 };

}; // KHTMLAttribute

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLAttributes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
private:
//------

	struct less_for_attribute
	{
		//-----------------------------------------------------------------------------
		bool operator()(const KHTMLAttribute& a1, const KHTMLAttribute& a2) const
		//-----------------------------------------------------------------------------
		{
			return a1.Name < a2.Name;
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

	void clear()
	{
		m_Attributes.clear();
	}

	bool empty() const
	{
		return m_Attributes.empty();
	}

	KStringView Get(KStringView sAttributeName) const;

	void Set(KStringView sAttributeName, KStringView sAttributeValue, char Quote='"')
	{
		Add(KHTMLAttribute({sAttributeName, sAttributeValue, Quote}));
	}

	void Add(const KHTMLAttribute& Attribute);
	void Add(KHTMLAttribute&& Attribute);

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

	void Parse(KStringView sInput);
	void Parse(KInStream& InStream);
	void Serialize(KOutStream& OutStream) const;

//------
protected:
//------

	AttributeList m_Attributes;

}; // KHTMLAttributes

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTMLTag
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLTag() = default;
	KHTMLTag(const KHTMLTag&) = default;
	KHTMLTag(KHTMLTag&&) = default;
	KHTMLTag& operator=(const KHTMLTag&) = default;
	KHTMLTag& operator=(KHTMLTag&&) = default;


	KHTMLTag(KStringView sInput)
	{
		Parse(sInput);
	}

	KHTMLTag(KInStream& InStream, bool bHadOpenAngleBracket = false)
	{
		Parse(InStream, bHadOpenAngleBracket);
	}

	void Parse(KStringView sInput);
	void Parse(KInStream& InStream, bool bHadOpenAngleBracket = false);
	void Serialize(KOutStream& OutStream) const;

	void clear();
	bool empty() const
	{
		return Name.empty();
	}

	bool IsInline() const;

//------
public:
//------

	KHTMLTag& operator=(KStringView sInput)
	{
		Parse(sInput);
		return *this;
	}

	KString         Name;
	KHTMLAttributes Attributes;
	bool            bSelfClosing { false };
	bool            bClosing { false };

}; // KHTMLTag

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

	KHTMLParser(KStringView sInput)
	{
		Parse(sInput);
	}

	virtual ~KHTMLParser();

	virtual bool Parse(KInStream& InStream);
	virtual bool Parse(KStringView sInput);

//------
protected:
//------

	enum OutputType { NONE, CONTENT, TAG, COMMENT, PROCESSINGINSTRUCTION, INVALID };

	virtual void Tag(KHTMLTag& Tag);
	virtual void Content(char ch);
	virtual void Comment(char ch);
	virtual void ProcessingInstruction(char ch);
	virtual void Invalid(char ch);
	virtual void Output(OutputType Type);

//------
private:
//------

	bool ParseComment(KInStream& InStream, uint16_t iConsumed = 0);
	bool ParseProcessingInstruction(KInStream& InStream, uint16_t iConsumed = 0);
	void SwitchOutput(OutputType To)
	{
		if (m_Output != To)
		{
			m_Output = To;
			Output(To);
		}
	}

	OutputType m_Output { NONE };

}; // KHTMLParser


} // end of namespace dekaf2

