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

#include "kstringview.h"
#include "kstring.h"
#include "kstream.h"
#include "kbufferedreader.h"
#include "khash.h"
#include <set>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Interface
class DEKAF2_PUBLIC KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

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
	virtual void Serialize(KStringRef& sOut) const;
	KString Serialize() const;
	/// the Serialize() with KString return is shadowed in derived classes, therefore we offer
	/// a second method to make it visible..
	KString ToString() const { return Serialize(); }

	KHTMLObject& operator=(KStringView sInput)
	{
		Parse(sInput);
		return *this;
	}

	virtual void clear();
	// make this base an ABC
	virtual bool empty() const = 0;
	// we do not use typeid(*this).name because it is not constexpr, but we need the
	// constexpr property for the Type() computation in derived classes
	virtual KStringView TypeName() const = 0;
	// we do not use std::type_index(typeid(*this)) because it is not trivial nor constexpr
	// - instead, on some platforms, it computes a string hash at every call
	virtual std::size_t Type() const = 0;

	// return a copy of yourself
	virtual std::unique_ptr<KHTMLObject> Clone() const = 0;

	/// returns true if sTag is one of the predefined inline tags
	static bool IsInlineTag(KStringView sTag);
	/// returns true if sTag is one of the predefined standalone/empty/void tags
	static bool IsStandaloneTag(KStringView sTag);
	/// returns true if sAttributeName is one of the predefined boolean attributes
	static bool IsBooleanAttribute(KStringView sAttributeName);

}; // KHTMLObject

template<typename T,
		 typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
inline KOutStream& operator <<(KOutStream& stream, const T& Object)
{
	Object.Serialize(stream);
	return stream;
}

template<typename T,
		 typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
inline KInStream& operator >>(KInStream& stream, T& Object)
{
	Object.Parse(stream);
	return stream;
}

template<typename T,
		 typename std::enable_if<std::is_base_of<KHTMLObject, T>::value == true, int>::type = 0>
inline KInStream& operator <<(T& Object, KInStream& stream)
{
	Object.Parse(stream);
	return stream;
}

DEKAF2_PUBLIC
inline bool operator==(const KHTMLObject& left, const KHTMLObject& right)
{
	return left.Type() == right.Type();
}

DEKAF2_PUBLIC
inline bool operator<(const KHTMLObject& left, const KHTMLObject& right)
{
	return left.Type() < right.Type();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a text run
class DEKAF2_PUBLIC KHTMLText : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLText";

//------
public:
//------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	KHTMLText() = default;
	KHTMLText(KString _sText)
	: sText(std::move(_sText))
	{
	}

	virtual bool Parse(KStringView sInput) override;
	virtual void Serialize(KOutStream& OutStream) const override;
	virtual void Serialize(KStringRef& sOut) const override;

	virtual void clear() override;
	virtual bool empty() const override;
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLText>(*this); }

	KString sText;

}; // KHTMLText

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an object with text and a lead-in and lead-out string
class DEKAF2_PUBLIC KHTMLStringObject : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLStringObject(KStringView sLeadIn, KStringView sLeadOut, KString sValue)
	: Value(std::move(sValue))
	, m_sLeadIn(sLeadIn)
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
class DEKAF2_PUBLIC KHTMLAttribute
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLAttribute() = default;

	KHTMLAttribute(KString sName, KString sValue, char _Quote='"')
	: Name(std::move(sName))
	, Value(std::move(sValue))
	, Quote(_Quote)
	{
	}

	bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{});
	void Serialize(KOutStream& OutStream) const;
	void Serialize(KStringRef& sOut) const;

	void clear();
	bool empty() const;

	KString Name {};
	KString Value {};
	mutable std::iostream::int_type Quote { 0 };

}; // KHTMLAttribute

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTMLAttributes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
private:
//------

	struct DEKAF2_PRIVATE less_for_attribute
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

	bool Has(KStringView sAttributeName) const;

	void Set(KString sAttributeName, KString sAttributeValue, char Quote='"')
	{
		Set(KHTMLAttribute(std::move(sAttributeName), std::move(sAttributeValue), Quote));
	}

	void Set(KHTMLAttribute Attribute);

	KHTMLAttributes& operator+=(const KHTMLAttribute& Attribute)
	{
		Set(Attribute);
		return *this;
	}

	void Remove(KStringView sAttributeName);

	KHTMLAttributes& operator+=(KHTMLAttribute&& Attribute)
	{
		Set(std::move(Attribute));
		return *this;
	}

	bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{});
	void Serialize(KOutStream& OutStream) const;
	void Serialize(KStringRef& sOut) const;

	void clear();
	bool empty() const;

//------
protected:
//------

	AttributeList m_Attributes;

}; // KHTMLAttributes

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTMLTag : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLTag";

//------
public:
//------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	enum TagType { NONE, OPEN, CLOSE, STANDALONE };

#if (DEKAF2_HAS_INCOMPLETE_CPP_17) || defined(_MSC_VER)
	// older GCCs need a default constructor here as they do not honor the using directive below
	KHTMLTag() = default;
#endif

	using KHTMLObject::KHTMLObject;
	using KHTMLObject::Parse;

	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}) override;
	virtual void Serialize(KOutStream& OutStream) const override;
	virtual void Serialize(KStringRef& sOut) const override;

	virtual void clear() override;
	virtual bool empty() const override;
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLTag>(*this); }

	bool IsInline() const     { return KHTMLObject::IsInlineTag(Name); }
	bool IsOpening() const    { return TagType == OPEN;                }
	bool IsClosing() const    { return TagType == CLOSE;               }
	bool IsStandalone() const { return TagType == STANDALONE;          }

//------
public:
//------

	KString         Name;
	KHTMLAttributes Attributes;
	TagType         TagType { NONE };

}; // KHTMLTag

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML comment
class DEKAF2_PUBLIC KHTMLComment : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLComment";

//------
public:
//------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	KHTMLComment(KString sValue = KString{})
	: KHTMLStringObject(LEAD_IN, LEAD_OUT, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLComment>(*this); }

	static constexpr KStringView LEAD_IN  = "<!--";
	static constexpr KStringView LEAD_OUT = "-->";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLComment

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML document type declaration
class DEKAF2_PUBLIC KHTMLDocumentType : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLDocumentType";

//------
public:
//------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	KHTMLDocumentType(KString sValue = KString{})
	: KHTMLStringObject(LEAD_IN, LEAD_OUT, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLDocumentType>(*this); }

	static constexpr KStringView LEAD_IN  = "<!DOCTYPE ";
	static constexpr KStringView LEAD_OUT = ">";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLDocumentType

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML processing instruction
class DEKAF2_PUBLIC KHTMLProcessingInstruction : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLProcessingInstruction";

//------
public:
//------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	KHTMLProcessingInstruction(KString sValue = KString{})
	: KHTMLStringObject(LEAD_IN, LEAD_OUT, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLProcessingInstruction>(*this); }

	static constexpr KStringView LEAD_IN  = "<?";
	static constexpr KStringView LEAD_OUT = "?>";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLProcessingInstruction

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a HTML CDATA section
class DEKAF2_PUBLIC KHTMLCData : public KHTMLStringObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static constexpr KStringView s_sObjectName = "KHTMLCData";

//------
public:
//------

	static constexpr std::size_t TYPE = s_sObjectName.Hash();

	KHTMLCData(KString sValue = KString{})
	: KHTMLStringObject(LEAD_IN, LEAD_OUT, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLCData>(*this); }

	static constexpr KStringView LEAD_IN  = "<![CDATA[";
	static constexpr KStringView LEAD_OUT = "]]>";

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) override;

}; // KHTMLProcessingInstruction

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Streaming HTML parser that isolates HTML objects and content and allows
/// child classes to override output methods
class DEKAF2_PUBLIC KHTMLParser
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

	virtual ~KHTMLParser();

	virtual bool Parse(KInStream& InStream);
	virtual bool Parse(KBufferedReader& InStream);
	virtual bool Parse(KStringView sInput);

	KHTMLParser& EmitEntitiesAsUTF8() { m_bEmitEntitiesAsUTF8 = true; return *this; }

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

	DEKAF2_PRIVATE void Script(KStringView sScript);
	DEKAF2_PRIVATE void Invalid(KStringView sInvalid);
	DEKAF2_PRIVATE void Invalid(const KHTMLStringObject& Object);
	DEKAF2_PRIVATE void Invalid(const KHTMLTag& Tag);
	DEKAF2_PRIVATE void SkipScript(KBufferedReader& InStream);
	DEKAF2_PRIVATE void SkipInvalid(KBufferedReader& InStream);
	DEKAF2_PRIVATE void EmitEntityAsUTF8(KBufferedReader& InStream);

	bool m_bEmitEntitiesAsUTF8 { false };

}; // KHTMLParser

template<typename T,
		 typename std::enable_if<std::is_base_of<KHTMLParser, T>::value == true, int>::type = 0>
inline KInStream& operator >>(KInStream& stream, T& Object)
{
	Object.Parse(stream);
	return stream;
}

template<typename T,
		 typename std::enable_if<std::is_base_of<KHTMLParser, T>::value == true, int>::type = 0>
inline KInStream& operator <<(T& Object, KInStream& stream)
{
	Object.Parse(stream);
	return stream;
}

} // end of namespace dekaf2

namespace std
{
	template<>
	struct hash<dekaf2::KHTMLObject>
	{
		DEKAF2_CONSTEXPR_20
		std::size_t operator()(const dekaf2::KHTMLObject& o) const noexcept
		{
			return o.Type();
		}
	};

} // end of namespace std

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
namespace boost
#else
namespace dekaf2
#endif
{
	inline
	std::size_t hash_value(const dekaf2::KHTMLObject& o)
	{
		return o.Type();
	}
}
