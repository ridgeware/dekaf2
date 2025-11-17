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

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstring.h"
#include "kstream.h"
#include "kbufferedreader.h"
#include "khash.h"
#include <set>

DEKAF2_NAMESPACE_BEGIN

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

	virtual bool Parse(KInStream& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false);
	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false);
	virtual void Serialize(KOutStream& OutStream) const;

	virtual bool Parse(KStringView sInput, bool bDecodeEntities = false);
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

	enum TagProperty
	{
		None              = 0,
		Inline            = 1 << 0,
		InlineBlock       = 1 << 1,
		Standalone        = 1 << 2,
		Block             = 1 << 3,
		Embedded          = 1 << 4,
		NotInlineInHead   = 1 << 5,
		Preformatted      = 1 << 6
	};

	/// returns the tag property, which can be a mix of Inline, InlineBlock, Standalone or Block
	static TagProperty GetTagProperty (KStringView sTag);
	/// returns true if sTag is one of the predefined inline tags
	static bool IsInlineTag        (KStringView sTag) { return (GetTagProperty(sTag) & TagProperty::Inline         ) == TagProperty::Inline;          }
	/// returns true if sTag is one of the predefined inline-block tags (they behave
	/// like inline tags from the outside, but like block tags from the inside)
	static bool IsInlineBlockTag   (KStringView sTag) { return (GetTagProperty(sTag) & TagProperty::InlineBlock    ) == TagProperty::InlineBlock;     }
	/// returns true if sTag is one of the predefined standalone/empty/void tags
	static bool IsStandaloneTag    (KStringView sTag) { return (GetTagProperty(sTag) & TagProperty::Standalone     ) == TagProperty::Standalone;      }
	/// returns true if sTag is one of the predefined preformatted tags
	static bool IsPreformattedTag  (KStringView sTag) { return (GetTagProperty(sTag) & TagProperty::Preformatted   ) == TagProperty::Preformatted;    }
	/// returns true if sTag is one of the predefined inline tags
	static bool IsInline           (TagProperty Prop) { return (Prop                 & TagProperty::Inline         ) == TagProperty::Inline;          }
	/// returns true if sTag is one of the predefined inline-block tags (they behave
	/// like inline tags from the outside, but like block tags from the inside)
	static bool IsInlineBlock      (TagProperty Prop) { return (Prop                 & TagProperty::InlineBlock    ) == TagProperty::InlineBlock;     }
	/// returns true if sTag is one of the predefined standalone/empty/void tags
	static bool IsStandalone       (TagProperty Prop) { return (Prop                 & TagProperty::Standalone     ) == TagProperty::Standalone;      }
	/// returns true if sTag is one of the predefined block tags (like p or div)
	static bool IsBlock            (TagProperty Prop) { return (Prop                 & TagProperty::Block          ) == TagProperty::Block;           }
	/// returns true if sTag is one of the predefined embedded tags (like video or img)
	static bool IsEmbedded         (TagProperty Prop) { return (Prop                 & TagProperty::Embedded       ) == TagProperty::Embedded;        }
	/// returns true if sTag is one of the predefined tags that are not inline inside the head section (like meta or link)
	static bool IsNotInlineInHead  (TagProperty Prop) { return (Prop                 & TagProperty::NotInlineInHead) == TagProperty::NotInlineInHead; }
	/// returns true if sTag is one of the predefined preformatted tags (like pre)
	static bool IsPreformatted     (TagProperty Prop) { return (Prop                 & TagProperty::Preformatted   ) == TagProperty::Preformatted;    }
	/// returns true if sAttributeName is one of the predefined boolean attributes
	static bool IsBooleanAttribute (KStringView sAttributeName);
	/// returns a decoded entity read from InStream, which must point to the character after '&'
	static KString DecodeEntity    (KBufferedReader& InStream);

}; // KHTMLObject

DEKAF2_ENUM_IS_FLAG(KHTMLObject::TagProperty)

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
	KHTMLText(KString sText, bool bIsEntityEncoded = false)
	: m_sText(std::move(sText))
	, m_bIsEntityEncoded(bIsEntityEncoded)
	{
	}

	virtual bool Parse(KStringView sInput, bool bDecodeEntities = false) override;
	virtual void Serialize(KOutStream& OutStream) const override;
	virtual void Serialize(KStringRef& sOut) const override;

	virtual void clear() override;
	virtual bool empty() const override;
	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLText>(*this); }

	/// @returns the text of this text element
	const KString& GetText        () const { return m_sText;            }
	/// @returns true if the text of this text element is already entity encoded
	bool           IsEntityEncoded() const { return m_bIsEntityEncoded; }

	KHTMLText& AddLeft (const KHTMLText& other);
	KHTMLText& AddRight(const KHTMLText& other);

	KHTMLText& TrimLeft()  { m_sText.TrimLeft();  return *this; }
	KHTMLText& TrimRight() { m_sText.TrimRight(); return *this; }

//------
protected:
//------

	KString m_sText;
	bool    m_bIsEntityEncoded { false };

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
	: sValue(std::move(sValue))
	, m_sLeadIn(sLeadIn)
	, m_sLeadOut(sLeadOut)
	{}

	// forward all base class constructors
	using KHTMLObject::KHTMLObject;

	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false) override;
	virtual void Serialize(KOutStream& OutStream) const override;

	virtual void clear() override;
	virtual bool empty() const override;

	KString sValue {}; // {} = make sure sValue is initialized, we may not have a constructor here!

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

class KFindSetOfChars; // fwd decl to not having to include kstringutils.h here

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KHTMLAttribute
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	friend class KHTMLAttributes;

//------
public:
//------

	KHTMLAttribute() = default;

	KHTMLAttribute(KString sName, KString sValue, char chQuote='"', bool bIsEntityEncoded = false)
	: m_sName(std::move(sName))
	, m_sValue(std::move(sValue))
	, m_chQuote(chQuote)
	, m_bIsEntityEncoded(bIsEntityEncoded)
	{
	}

	/// parses attribute from an input stream
	bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false);
	/// serializes attribute into an output stream
	void Serialize(KOutStream& OutStream) const;
	/// serializes attribute into an output string
	void Serialize(KStringRef& sOut) const;

	/// clears attribute name and value
	void clear();
	/// @returns true if attribute is empty
	bool empty() const;

	/// @returns the attribute name
	const KString& GetName()  const { return m_sName;  }
	/// @returns the attribute value
	const KString& GetValue() const { return m_sValue; }

//------
protected:
//------

	static KFindSetOfChars s_NeedsQuotes;

	KString         m_sName;
	mutable KString m_sValue;
	mutable char    m_chQuote          { 0     };
	bool            m_bIsEntityEncoded { false };

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
			return a1.m_sName < a2.m_sName;
		}

		// add comparators for strings so that C++14 finds them with find()
		//-----------------------------------------------------------------------------
		bool operator()(KStringView s1, const KHTMLAttribute& a2) const
		//-----------------------------------------------------------------------------
		{
			return s1 < a2.m_sName;
		}

		//-----------------------------------------------------------------------------
		bool operator()(const KHTMLAttribute& a1, KStringView s2) const
		//-----------------------------------------------------------------------------
		{
			return a1.m_sName < s2;
		}

	};

//------
public:
//------

	using AttributeList = std::set<KHTMLAttribute, less_for_attribute>;

	/// @returns an iterator to the begin of the attribute list
	AttributeList::iterator begin()
	{
		return m_Attributes.begin();
	}

	/// @returns an iterator to the end of the attribute list
	AttributeList::iterator end()
	{
		return m_Attributes.end();
	}

	/// @returns a const_iterator to the begin of the attribute list
	AttributeList::const_iterator begin() const
	{
		return m_Attributes.begin();
	}

	/// @returns a const_iterator to the end of the attribute list
	AttributeList::const_iterator end() const
	{
		return m_Attributes.end();
	}

	/// @returns the value of an attribute with the given name (or an empty string if the attribute does not exist)
	KStringView Get(KStringView sAttributeName) const;

	///@returns true if an attribute with the given name exists
	bool Has(KStringView sAttributeName) const;

	/// sets an attribute's name and value, creates a new attribute if the name was not yet existing
	KHTMLAttributes& Set(KString sAttributeName, KString sAttributeValue, char Quote='"', bool bDoNotEscape = false)
	{
		Set(KHTMLAttribute(std::move(sAttributeName), std::move(sAttributeValue), Quote, bDoNotEscape));
		return *this;
	}

	/// sets an attribute, creates a new attribute if the name was not yet existing
	KHTMLAttributes& Set(KHTMLAttribute Attribute);

	/// sets all attributes from another attribute list, creates new attributes if the name was not yet existing
	KHTMLAttributes& Set(const KHTMLAttributes& Attributes);

	/// sets an attribute, creates a new attribute if the name was not yet existing
	KHTMLAttributes& operator+=(KHTMLAttribute Attribute)
	{
		return Set(std::move(Attribute));
	}

	/// sets all attributes from another attribute list, creates new attributes if the name was not yet existing
	KHTMLAttributes& operator+=(KHTMLAttributes Attributes)
	{
		return Set(std::move(Attributes));
	}

	/// removes an attribute with the given name, if existing
	void Remove(KStringView sAttributeName);

	/// parses attributes from an input stream
	bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false);
	/// serializes all attributes into an output stream
	void Serialize(KOutStream& OutStream) const;
	/// serializes all attributes into an output string
	void Serialize(KStringRef& sOut) const;

	/// clears all attributes
	void clear();
	/// @returns true if the attribute list is empty
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

	enum class TagType { None, Open, Close, Standalone };

#if (DEKAF2_HAS_INCOMPLETE_CPP_17) || defined(DEKAF2_IS_MSC)
	// older GCCs need a default constructor here as they do not honor the using directive below
	KHTMLTag() = default;
#endif

	using KHTMLObject::KHTMLObject;
	using KHTMLObject::Parse;

	/// parse this tag from a stream
	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false) override;
	/// serialize this tag to a stream
	virtual void Serialize(KOutStream& OutStream) const override;
	/// serialize this tag to a string
	virtual void Serialize(KStringRef& sOut) const override;

	/// clears the tag
	virtual void clear() override;
	/// is this tag empty / not yet parsed ?
	virtual bool empty() const override;
	virtual KStringView TypeName() const override { return s_sObjectName; }
	virtual std::size_t Type() const override { return TYPE; }
	/// creates a copy of this class, even when called without the exact type information
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLTag>(*this); }

	/// @returns true if this is an inline tag
	bool IsInline    () const { return KHTMLObject::IsInlineTag(m_sName); }
	/// @returns true if this is an opening tag
	bool IsOpening   () const { return m_TagType == TagType::Open;        }
	/// @returns true if this is a closing tag
	bool IsClosing   () const { return m_TagType == TagType::Close;       }
	/// @returns true if this is a standalone tag
	bool IsStandalone() const { return m_TagType == TagType::Standalone;  }

	/// @returns the tag name
	const KString&         GetName      () const { return m_sName;        }
	/// @returns the tag attributes
	const KHTMLAttributes& GetAttributes() const { return m_Attributes;   }
	/// @returns the tag type (Open/Close/Standalone)
	TagType                GetTagType   () const { return m_TagType;      }

	/// @returns true if this tag has an attribute with the given name
	bool        HasAttribute (KStringView sAttributeName) const { return GetAttributes().Has(sAttributeName); }
	/// @returns the value of an attribute with the given name (or the empty string if not found)
	KStringView GetAttribute (KStringView sAttributeName) const { return GetAttributes().Get(sAttributeName); }

	/// @returns the tag name as an rvalue reference
	KString&&         MoveName      () { return std::move(m_sName);       }
	/// @returns the attributes as an rvalue reference
	KHTMLAttributes&& MoveAttributes() { return std::move(m_Attributes);  }

//------
protected:
//------

	KString         m_sName;
	KHTMLAttributes m_Attributes;
	enum TagType    m_TagType { TagType::None };

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
	: KHTMLStringObject(s_sLeadIn, s_sLeadOut, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLComment>(*this); }

	static constexpr KStringView s_sLeadIn  = "<!--";
	static constexpr KStringView s_sLeadOut = "-->";

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
	: KHTMLStringObject(s_sLeadIn, s_sLeadOut, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLDocumentType>(*this); }

	static constexpr KStringView s_sLeadIn  = "<!DOCTYPE ";
	static constexpr KStringView s_sLeadOut = ">";

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
	: KHTMLStringObject(s_sLeadIn, s_sLeadOut, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLProcessingInstruction>(*this); }

	static constexpr KStringView s_sLeadIn  = "<?";
	static constexpr KStringView s_sLeadOut = "?>";

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
	: KHTMLStringObject(s_sLeadIn, s_sLeadOut, std::move(sValue))
	{}

	// forward all constructors
	using KHTMLStringObject::KHTMLStringObject;

	virtual KStringView TypeName() const override { return s_sObjectName;  }
	virtual std::size_t Type() const override { return TYPE; }
	virtual std::unique_ptr<KHTMLObject> Clone() const override { return std::make_unique<KHTMLCData>(*this); }

	static constexpr KStringView s_sLeadIn  = "<![CDATA[";
	static constexpr KStringView s_sLeadOut = "]]>";

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

DEKAF2_NAMESPACE_END

namespace std
{
	template<>
	struct hash<DEKAF2_PREFIX KHTMLObject>
	{
		DEKAF2_CONSTEXPR_20
		std::size_t operator()(const DEKAF2_PREFIX KHTMLObject& o) const noexcept
		{
			return o.Type();
		}
	};

} // end of namespace std

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
namespace boost {
#else
DEKAF2_NAMESPACE_BEGIN
#endif
	inline
	std::size_t hash_value(const DEKAF2_PREFIX KHTMLObject& o)
	{
		return o.Type();
	}
#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
}
#else
DEKAF2_NAMESPACE_END
#endif
