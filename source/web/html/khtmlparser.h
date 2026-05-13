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


/// @file khtmlparser.h
#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/io/readwrite/kbufferedreader.h>
#include <dekaf2/crypto/hash/khash.h>
#include <cstdint>
#include <set>

DEKAF2_NAMESPACE_BEGIN

namespace khtml { class Document; }         // fwd decl for arena injection
namespace khtml { class ParseAccumulator; } // fwd decl — Parse() uses one transiently
class KArenaStringBuilder;                  // fwd decl — used by KHTMLAttribute lambdas

/// @addtogroup web_html
/// @{

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

	/// Inject an arena pointer. When non-null, derived `Parse()` methods
	/// route their char-by-char accumulators through `KArenaStringBuilder`
	/// so the resulting `m_s*` views point directly into arena memory —
	/// no follow-up copy needed in the DOM (`AllocateString`
	/// recognises arena-self views and short-circuits). Default nullptr
	/// keeps the heap-KString fallback, used by the streaming SAX path.
	virtual void SetArena(khtml::Document* pArena) noexcept { m_pArena = pArena; }
	khtml::Document* GetArena() const noexcept { return m_pArena; }

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

	enum TagProperty : std::uint8_t
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

//------
protected:
//------

	/// Optional arena. When non-null, derived Parse() methods route
	/// char-by-char accumulators through `KArenaStringBuilder` so the
	/// resulting `m_s*` views point straight into arena memory. nullptr
	/// = heap-KString fallback (the SAX default).
	khtml::Document* m_pArena { nullptr };

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
	: m_sTextOwned(std::move(sText))
	, m_sText(m_sTextOwned.ToView())
	, m_bIsEntityEncoded(bIsEntityEncoded)
	{
	}

	KHTMLText(const KHTMLText& other)
	: KHTMLObject(other)                     // explicit base init (gcc -Wextra)
	, m_sTextOwned(other.m_sText)            // deep-copy view bytes into owned
	, m_sText(m_sTextOwned.ToView())
	, m_bIsEntityEncoded(other.m_bIsEntityEncoded)
	{
	}

	KHTMLText(KHTMLText&& other) noexcept
	: KHTMLObject(std::move(other))          // explicit base init (gcc -Wextra)
	{
		bool bWasOwned = (other.m_sText.data() == other.m_sTextOwned.data())
		                  && !other.m_sTextOwned.empty();
		if (other.m_sText.empty()) { bWasOwned = true; }
		m_sTextOwned       = std::move(other.m_sTextOwned);
		m_bIsEntityEncoded = other.m_bIsEntityEncoded;
		// if/else (not ternary) — KString::ToView() returns KStringViewZ,
		// which derives PRIVATELY from KStringView; gcc rejects the
		// resulting common-type deduction in a ?:-expression.
		if (bWasOwned) m_sText = m_sTextOwned.ToView();
		else           m_sText = other.m_sText;
	}

	KHTMLText& operator=(const KHTMLText& other)
	{
		if (this != &other)
		{
			m_sTextOwned       = KString(other.m_sText);
			m_sText            = m_sTextOwned.ToView();
			m_bIsEntityEncoded = other.m_bIsEntityEncoded;
		}
		return *this;
	}

	KHTMLText& operator=(KHTMLText&& other) noexcept
	{
		if (this != &other)
		{
			bool bWasOwned = (other.m_sText.data() == other.m_sTextOwned.data())
			                  && !other.m_sTextOwned.empty();
			if (other.m_sText.empty()) { bWasOwned = true; }
			m_sTextOwned       = std::move(other.m_sTextOwned);
			m_bIsEntityEncoded = other.m_bIsEntityEncoded;
			if (bWasOwned) m_sText = m_sTextOwned.ToView();
			else           m_sText = other.m_sText;
		}
		return *this;
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
	KStringView    Text        () const { return m_sText;            }
	/// @returns true if the text of this text element is already entity encoded
	bool           IsEntityEncoded() const { return m_bIsEntityEncoded; }

//------
protected:
//------

	// owning fallback storage; non-empty when self-owning
	KString     m_sTextOwned;
	// current text - points either into m_sTextOwned or into an external arena
	KStringView m_sText;
	bool        m_bIsEntityEncoded { false };

}; // KHTMLText

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// an object with text and a lead-in and lead-out string
class DEKAF2_PUBLIC KHTMLStringObject : public KHTMLObject
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTMLStringObject(KStringView sLeadIn, KStringView sLeadOut, KString sIn)
	: sValueOwned(std::move(sIn))
	, sValue(sValueOwned.ToView())
	, m_sLeadIn(sLeadIn)
	, m_sLeadOut(sLeadOut)
	{}

	KHTMLStringObject(const KHTMLStringObject& other)
	: KHTMLObject(other)                         // explicit base init (gcc -Wextra)
	, sValueOwned(other.sValue)                  // deep-copy view bytes into owned
	, sValue(sValueOwned.ToView())
	, m_sLeadIn(other.m_sLeadIn)
	, m_sLeadOut(other.m_sLeadOut)
	{}

	KHTMLStringObject(KHTMLStringObject&& other) noexcept
	: KHTMLObject(std::move(other))
	, m_sLeadIn(other.m_sLeadIn)
	, m_sLeadOut(other.m_sLeadOut)
	{
		bool bWasOwned = (other.sValue.data() == other.sValueOwned.data())
		                  && !other.sValueOwned.empty();
		if (other.sValue.empty()) { bWasOwned = true; }
		sValueOwned = std::move(other.sValueOwned);
		// if/else (not ternary) — KString::ToView() returns KStringViewZ
		// (private base KStringView), gcc rejects the ?:-common-type.
		if (bWasOwned) sValue = sValueOwned.ToView();
		else           sValue = other.sValue;
	}

	KHTMLStringObject& operator=(const KHTMLStringObject& other)
	{
		if (this != &other)
		{
			sValueOwned = KString(other.sValue);
			sValue      = sValueOwned.ToView();
			m_sLeadIn   = other.m_sLeadIn;
			m_sLeadOut  = other.m_sLeadOut;
		}
		return *this;
	}

	KHTMLStringObject& operator=(KHTMLStringObject&& other) noexcept
	{
		if (this != &other)
		{
			bool bWasOwned = (other.sValue.data() == other.sValueOwned.data())
			                  && !other.sValueOwned.empty();
			if (other.sValue.empty()) { bWasOwned = true; }
			sValueOwned = std::move(other.sValueOwned);
			if (bWasOwned) sValue = sValueOwned.ToView();
			else           sValue = other.sValue;
			m_sLeadIn   = other.m_sLeadIn;
			m_sLeadOut  = other.m_sLeadOut;
		}
		return *this;
	}

	// forward all base class constructors
	using KHTMLObject::KHTMLObject;

	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false) override;
	virtual void Serialize(KOutStream& OutStream) const override;

	virtual void clear() override;
	virtual bool empty() const override;

	/// @returns the parsed payload (between lead-in and lead-out) as a
	/// view. Lifetime: valid as long as this object — or its backing
	/// arena / source-buffer, when arena-allocated — is alive.
	KStringView Value () const { return sValue;    }
	KStringView LeadIn() const { return m_sLeadIn; }
	KStringView LeadOut() const { return m_sLeadOut; }

//------
protected:
//------

	virtual bool SearchForLeadOut(KBufferedReader& InStream) = 0;

	// owning fallback storage; non-empty in heap-mode (no arena was
	// injected for this parse) or when the object was constructed
	// owning (via the KString-arg ctor or a copy/move). The view-only
	// arena-builder and source-slice modes leave it empty.
	KString     sValueOwned {};
	// current value — points either into sValueOwned, into the parser's
	// arena, or into a caller-supplied stable source buffer.
	KStringView sValue      {};

	/// Append one byte to the current value-accumulator. Routes through
	/// the transient `ParseAccumulator` armed by `Parse()`; falls back
	/// to growing `sValueOwned` on the heap when none is armed. Used by
	/// subclass `SearchForLeadOut()` implementations.
	///
	/// @param ch         byte to store
	/// @param pAfterPos  source position **one past** the byte just read
	///                   that produced `ch`. Used in slicing mode to
	///                   extend the slice end. Pass `nullptr` if the
	///                   byte is being re-played (not a fresh read).
	void AppendValueChar(char ch, const char* pAfterPos = nullptr);

	KStringView              m_sLeadIn  {};
	KStringView              m_sLeadOut {};
	// Transient accumulator pointer, set only while Parse() is running.
	// Non-null = arena-aware accumulation (with slicing if the source is
	// in a stable region); nullptr = heap fallback into `sValueOwned`.
	khtml::ParseAccumulator* m_pValueAcc { nullptr };

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
	: m_sNameOwned(std::move(sName))
	, m_sValueOwned(std::move(sValue))
	, m_sName(m_sNameOwned.ToView())
	, m_sValue(m_sValueOwned.ToView())
	, m_chQuote(chQuote)
	, m_bIsEntityEncoded(bIsEntityEncoded)
	{
	}

	KHTMLAttribute(const KHTMLAttribute& other)
	: m_sNameOwned(other.m_sName)         // deep-copy view bytes into owned
	, m_sValueOwned(other.m_sValue)
	, m_sName(m_sNameOwned.ToView())
	, m_sValue(m_sValueOwned.ToView())
	, m_chQuote(other.m_chQuote)
	, m_bIsEntityEncoded(other.m_bIsEntityEncoded)
	{
	}

	KHTMLAttribute(KHTMLAttribute&& other) noexcept
	{
		// determine "was-owned" status BEFORE moving (SSO may keep data pointer
		// stable, so the post-move comparison is unreliable)
		bool bNameWasOwned  = (other.m_sName.data()  == other.m_sNameOwned.data())
		                       && !other.m_sNameOwned.empty();
		bool bValueWasOwned = (other.m_sValue.data() == other.m_sValueOwned.data())
		                       && !other.m_sValueOwned.empty();
		// if the attribute was default-constructed (both empty) treat as owned
		// (so we keep views matched to our - empty - owned storage)
		if (other.m_sName.empty())  { bNameWasOwned  = true; }
		if (other.m_sValue.empty()) { bValueWasOwned = true; }

		m_sNameOwned       = std::move(other.m_sNameOwned);
		m_sValueOwned      = std::move(other.m_sValueOwned);
		m_chQuote          = other.m_chQuote;
		m_bIsEntityEncoded = other.m_bIsEntityEncoded;
		// if/else (not ternary) — KString::ToView() returns KStringViewZ,
		// which derives PRIVATELY from KStringView; gcc rejects the
		// common-type deduction in a ?:-expression.
		if (bNameWasOwned)  m_sName  = m_sNameOwned.ToView();
		else                m_sName  = other.m_sName;
		if (bValueWasOwned) m_sValue = m_sValueOwned.ToView();
		else                m_sValue = other.m_sValue;
	}

	KHTMLAttribute& operator=(const KHTMLAttribute& other)
	{
		if (this != &other)
		{
			m_sNameOwned       = KString(other.m_sName);
			m_sValueOwned      = KString(other.m_sValue);
			m_sName            = m_sNameOwned.ToView();
			m_sValue           = m_sValueOwned.ToView();
			m_chQuote          = other.m_chQuote;
			m_bIsEntityEncoded = other.m_bIsEntityEncoded;
		}
		return *this;
	}

	KHTMLAttribute& operator=(KHTMLAttribute&& other) noexcept
	{
		if (this != &other)
		{
			bool bNameWasOwned  = (other.m_sName.data()  == other.m_sNameOwned.data())
			                       && !other.m_sNameOwned.empty();
			bool bValueWasOwned = (other.m_sValue.data() == other.m_sValueOwned.data())
			                       && !other.m_sValueOwned.empty();
			if (other.m_sName.empty())  { bNameWasOwned  = true; }
			if (other.m_sValue.empty()) { bValueWasOwned = true; }
			m_sNameOwned        = std::move(other.m_sNameOwned);
			m_sValueOwned       = std::move(other.m_sValueOwned);
			m_chQuote           = other.m_chQuote;
			m_bIsEntityEncoded  = other.m_bIsEntityEncoded;
			if (bNameWasOwned)  m_sName  = m_sNameOwned.ToView();
			else                m_sName  = other.m_sName;
			if (bValueWasOwned) m_sValue = m_sValueOwned.ToView();
			else                m_sValue = other.m_sValue;
		}
		return *this;
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
	KStringView    Name () const { return m_sName;   }
	/// @returns the attribute value
	KStringView    Value() const { return m_sValue;  }
	/// @returns the original quote char as seen by the parser (' or "), 0 if none / unset
	char           Quote() const { return m_chQuote; }
	/// @returns true if the attribute value is already entity encoded
	bool    IsEntityEncoded() const { return m_bIsEntityEncoded; }

	/// Inject arena for in-situ Parse. nullptr = heap-KString fallback.
	void SetArena(khtml::Document* pArena) noexcept { m_pArena = pArena; }
	khtml::Document* GetArena() const noexcept { return m_pArena; }

//------
protected:
//------

	static KFindSetOfChars s_NeedsQuotes;

	// owning fallback storage; non-empty when self-owning, empty when views point into an external arena/source
	mutable KString     m_sNameOwned;
	mutable KString     m_sValueOwned;
	// current name/value - always points either into m_sNameOwned / m_sValueOwned, or into an external arena
	mutable KStringView m_sName;
	mutable KStringView m_sValue;
	khtml::Document*    m_pArena           { nullptr };
	mutable char        m_chQuote          { 0     };
	bool                m_bIsEntityEncoded { false };

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

	/// Inject arena for in-situ Parse. Propagated to each parsed
	/// `KHTMLAttribute` during `Parse()`.
	void SetArena(khtml::Document* pArena) noexcept { m_pArena = pArena; }
	khtml::Document* GetArena() const noexcept { return m_pArena; }

//------
protected:
//------

	AttributeList    m_Attributes;
	khtml::Document* m_pArena { nullptr };

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

#if !defined(__cpp_inheriting_constructors) || __cpp_inheriting_constructors < 201511L || defined(DEKAF2_IS_MSC)
	// older GCCs need a default constructor here as they do not honor the using directive below
	KHTMLTag() = default;
#endif

	using KHTMLObject::KHTMLObject;
	using KHTMLObject::Parse;

	KHTMLTag(const KHTMLTag& other)
	: KHTMLObject(other)                       // explicit base init (gcc -Wextra)
	, m_sNameOwned(other.m_sName)              // deep-copy view bytes into owned
	, m_sName(m_sNameOwned.ToView())
	, m_Attributes(other.m_Attributes)
	, m_TagType(other.m_TagType)
	{
	}

	KHTMLTag(KHTMLTag&& other) noexcept
	: KHTMLObject(std::move(other))
	, m_Attributes(std::move(other.m_Attributes))
	, m_TagType(other.m_TagType)
	{
		bool bWasOwned = (other.m_sName.data() == other.m_sNameOwned.data())
		                  && !other.m_sNameOwned.empty();
		if (other.m_sName.empty()) { bWasOwned = true; }
		m_sNameOwned = std::move(other.m_sNameOwned);
		// if/else (not ternary) — KString::ToView() returns KStringViewZ,
		// which derives PRIVATELY from KStringView; gcc rejects the
		// common-type deduction in a ?:-expression.
		if (bWasOwned) m_sName = m_sNameOwned.ToView();
		else           m_sName = other.m_sName;
	}

	KHTMLTag& operator=(const KHTMLTag& other)
	{
		if (this != &other)
		{
			m_sNameOwned = KString(other.m_sName);
			m_sName      = m_sNameOwned.ToView();
			m_Attributes = other.m_Attributes;
			m_TagType    = other.m_TagType;
		}
		return *this;
	}

	KHTMLTag& operator=(KHTMLTag&& other) noexcept
	{
		if (this != &other)
		{
			bool bWasOwned = (other.m_sName.data() == other.m_sNameOwned.data())
			                  && !other.m_sNameOwned.empty();
			if (other.m_sName.empty()) { bWasOwned = true; }
			m_sNameOwned = std::move(other.m_sNameOwned);
			if (bWasOwned) m_sName = m_sNameOwned.ToView();
			else           m_sName = other.m_sName;
			m_Attributes = std::move(other.m_Attributes);
			m_TagType    = other.m_TagType;
		}
		return *this;
	}

	/// parse this tag from a stream
	virtual bool Parse(KBufferedReader& InStream, KStringView sOpening = KStringView{}, bool bDecodeEntities = false) override;
	/// serialize this tag to a stream
	virtual void Serialize(KOutStream& OutStream) const override;
	/// serialize this tag to a string
	virtual void Serialize(KStringRef& sOut) const override;

	/// override to also propagate the arena into the contained attribute set
	void SetArena(khtml::Document* pArena) noexcept override
	{
		KHTMLObject::m_pArena = pArena;
		m_Attributes.SetArena(pArena);
	}

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
	KStringView            Name      () const { return m_sName;        }
	/// @returns the tag attributes
	const KHTMLAttributes& Attributes() const { return m_Attributes;   }
	/// @returns the tag type (Open/Close/Standalone)
	TagType                GetTagType   () const { return m_TagType;      }

	/// @returns true if this tag has an attribute with the given name
	bool        HasAttribute (KStringView sAttributeName) const { return Attributes().Has(sAttributeName); }
	/// @returns the value of an attribute with the given name (or the empty string if not found)
	KStringView Attribute (KStringView sAttributeName) const { return Attributes().Get(sAttributeName); }

//------
protected:
//------

	// owning fallback storage; non-empty when self-owning
	KString         m_sNameOwned;
	// current name - points either into m_sNameOwned or into an external arena
	KStringView     m_sName;
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
/// Streaming, callback-driven (SAX-style) HTML parser. It tokenises the
/// input into the `KHTMLObject`-family of value types (`KHTMLTag`,
/// `KHTMLComment`, `KHTMLCData`, `KHTMLDocumentType`,
/// `KHTMLProcessingInstruction`) and individual content characters, and
/// invokes overridable hooks on each event. Consumers either subclass and
/// override the hooks (`KHTML` for full DOM, `KHTMLContentBlocks` for
/// block aggregation, application-specific subclasses for cherry-picking
/// the data they need) or instantiate the parser directly to validate
/// input.
///
/// ### Input pathways
///
/// Three entry points cover the realistic input sources:
///  - `Parse(KInStream&)` — read from any stream (file, socket, pipe).
///    Wraps the stream in a `KBufferedStreamReader`.
///  - `Parse(KBufferedReader&)` — the actual streaming loop. Used
///    internally; called directly when the consumer already has a
///    buffered reader (e.g. wrapping a custom source).
///  - `Parse(T&& sInput)` — a constrained template that takes anything
///    `KStringView` is constructible from (`KString`, `KStringView`,
///    `KStringViewZ`, `const char*`, `std::string`, `std::string_view`, ...). Routes
///    through the virtual `ParseImpl(KStringView)` hook which derived
///    classes (notably `KHTML::ParseImpl`) can override to insert
///    buffer-ownership / stable-region semantics before delegating
///    back to the streaming loop.
///
/// The template is SFINAE-constrained on `is_constructible<KStringView,T>`
/// so it doesn't capture stream types, and it loses overload resolution
/// to any concrete `Parse(KString)` overload in a derived class — that's
/// how `KHTML::Parse(KString)` cleanly intercepts the owning path
/// without forcing a `using`-declaration tug-of-war.
///
/// ### Callback hooks (override in subclasses)
///
/// All hooks are virtual member functions called from the streaming loop.
/// Default implementations are no-ops; subclasses pick the ones they need.
///
///  - `Object(KHTMLObject&)`  — a complete tag / comment / CData /
///    DocumentType / ProcessingInstruction was recognised. The argument
///    is a **temporary** living only for the duration of the call;
///    persistent storage of its bytes requires the consumer to copy.
///  - `Content(char)`         — one byte of character content (whitespace
///    is already collapsed in the parser's default behaviour).
///  - `Script(char)`          — one byte of `<script>` body content
///    (untouched by entity decoding or whitespace collapsing).
///  - `Invalid(char)`         — one byte that the parser couldn't fit
///    into the grammar (e.g. malformed tag fragment, unclosed comment).
///  - `Finished()`            — end-of-input reached; consumers should
///    flush any pending in-flight state from earlier `Content()` etc.
///
/// ### Arena injection (for in-situ-friendly storage)
///
/// `KHTMLObject`-derived classes store their bytes in a two-member
/// pattern: a `KStringView` (the public view) plus an optional owning
/// `KString` fallback (`m_*Owned`). The view can point into one of
/// three places:
///
///  1. The KHTMLObject's own KString fallback — the SAX default when
///     no arena is injected.
///  2. The injected arena (via `SetArena()`), populated through a
///     `KArenaStringBuilder` that the per-class `Parse()` methods use
///     for char-by-char accumulation. The arena's lifetime governs
///     view validity.
///  3. A stable region the caller has registered with the arena (see
///     `khtml::Document::RegisterStableRegion`). Unmodified
///     substrings are sliced straight out of the caller's source
///     buffer — zero arena bytes consumed for those strings. This is
///     "source-slicing" mode; it's transparent to the SAX consumer
///     because the public view types don't change.
///
/// Lowering / entity-decoding / whitespace-normalisation always force
/// promotion to the arena builder (slicing mode would lose
/// correctness). All decisions happen inside `khtml::ParseAccumulator`,
/// per accumulated string, transparently to the parser loop.
///
/// ### Hooks for slicing-aware consumers
///
/// Consumers that override `Content()` / `Script()` / `Invalid()` and
/// want to participate in source-slicing can query:
///
///  - `ActiveReader()` — the buffered reader currently being parsed
///    (`nullptr` outside of a parse). Their `CurrentPos()` gives the
///    source position one past the just-read byte.
///  - `IsDecodingEntity()` — true while the parser is iterating the
///    bytes of a decoded entity. Those bytes do not correspond to a
///    source position and must therefore be treated as
///    "transformed" (i.e. cannot extend a source slice).
///
/// `KHTML::Content()` etc. use both hooks to drive its
/// `ParseAccumulator` correctly.
///
/// ### Tag-internal accumulators
///
/// `KHTMLTag::Parse` / `KHTMLAttribute::Parse` /
/// `KHTMLStringObject::Parse` each instantiate one or two local
/// `khtml::ParseAccumulator` objects per accumulated string (tag name,
/// attribute name, attribute value, comment/CData payload, ...) — the
/// same tri-mode mechanism as `KHTML::Content`. With an arena set, all
/// these strings land in arena or as source-slices instead of
/// heap-allocated `KString`s.
///
/// ### Subclasses that ship with dekaf2
///
///  - `KHTML` — builds an arena-backed POD tree (`khtml::NodePOD`) and
///    exposes it via `Root()` for traversal / mutation.
///  - `KHTMLContentBlocks` — aggregates text blocks.
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

	/// Parse from an istream. Wraps the stream in a buffered reader and
	/// delegates to `Parse(KBufferedReader&)`. No source-slicing — the
	/// fill buffer is internal and transient.
	virtual bool Parse(KInStream& InStream);

	/// Streaming-parse loop entry. The buffered reader must outlive the
	/// call. Emits callbacks (`Object`, `Content`, `Script`, `Invalid`,
	/// `Finished`) as it tokenises. If an arena is set (`SetArena`) and
	/// the reader is over a stable source region, accumulators inside
	/// `KHTMLObject`-derived `Parse` methods plus the subclass
	/// `Content`-style callbacks can slice views straight out of the
	/// source without copying.
	virtual bool Parse(KBufferedReader& InStream);

	/// Memory-parse entry as a constrained template. SFINAE on
	/// `is_constructible<KStringView, T>` accepts everything string-y
	/// (`KString`, `KStringView`, `KStringViewZ`, `const char*`,
	/// `std::string`,`std::string_view`, ...). Being a template, this entry *loses* to any
	/// concrete `Parse(KString)` overload in a derived class (notably
	/// `KHTML`) under overload resolution — non-template beats template
	/// at equal match quality. That cleanly resolves the `const char*`
	/// ambiguity that would otherwise force a
	/// `-Woverloaded-virtual` workaround.
	///
	/// The actual virtual hook is `ParseImpl(KStringView)` below;
	/// derived classes override it to intercept the memory-parse path
	/// (e.g. for owning-buffer or stable-region semantics) without
	/// touching the public template.
	template<typename T,
	         typename std::enable_if<
	             std::is_constructible<KStringView, T&&>::value
	             && !std::is_base_of<KInStream, typename std::decay<T>::type>::value
	             && !std::is_base_of<KBufferedReader, typename std::decay<T>::type>::value,
	         int>::type = 0>
	bool Parse(T&& sInput)
	{
		return ParseImpl(KStringView(std::forward<T>(sInput)));
	}

	/// Switch entity-decoding to UTF-8 output mode. When enabled, any
	/// `&entity;` token encountered in character content is decoded
	/// during the parse and the decoded UTF-8 bytes are pushed through
	/// `Content(char)` one byte at a time (with `IsDecodingEntity()`
	/// returning true during that window). When disabled (the default),
	/// the entity bytes pass through verbatim.
	KHTMLParser& EmitEntitiesAsUTF8() { m_bEmitEntitiesAsUTF8 = true; return *this; }

//------
protected:
//------

	/// Memory-parse virtual entry. Derived classes (notably `KHTML`)
	/// override this to route through their owning buffer before the
	/// streaming pass runs. Default impl: wrap the view in a buffered
	/// string reader and delegate to `Parse(KBufferedReader&)`.
	virtual bool ParseImpl(KStringView sInput);

	/// Called once per complete `KHTMLObject` (tag, comment, CData,
	/// DocumentType, processing instruction) that the parser recognised.
	/// The `Object` reference points at a stack-local instance whose
	/// content views are valid only for the duration of this call —
	/// copy out (e.g. via `Clone()` or by extracting `Name()` /
	/// `Value()` into your own storage) if you need to keep bytes
	/// beyond the call.
	virtual void Object(KHTMLObject& Object);

	/// One byte of character content (whitespace-collapsed in the
	/// non-preformatted default). If the parser is in
	/// `IsDecodingEntity()` mode, `ch` is one byte of a decoded entity
	/// (UTF-8) and does not correspond to a source position.
	virtual void Content(char ch);

	/// One byte of `<script>` body content (passes through verbatim —
	/// no entity decoding, no whitespace collapsing). Followed by a
	/// closing `Object(KHTMLTag)` callback for `</script>` once the
	/// body ends.
	virtual void Script(char ch);

	/// One byte that the parser could not fit into the grammar (e.g.
	/// malformed tag fragment, unclosed comment). Subclasses can
	/// recover, log, or pass through verbatim.
	virtual void Invalid(char ch);

	/// End-of-input reached. Override to flush any in-flight state
	/// accumulated from `Content()`/`Script()`/`Invalid()`.
	virtual void Finished();

	/// Inject an arena into the parser. When non-null, parsed object strings
	/// (tag names, attribute names/values, text content) will be promoted into
	/// the arena so that the views in KHTMLObject derivatives remain valid
	/// for the arena's lifetime. When null, parsed object strings remain in
	/// their owning KString fallback (the default).
	void SetArena(khtml::Document* pArena) noexcept { m_pArena = pArena; }
	/// @returns the currently injected arena, or nullptr if none.
	khtml::Document* GetArena() const noexcept { return m_pArena; }

	//-----------------------------------------------------------------------------
	/// @returns the reader currently being parsed, or `nullptr` outside of
	/// a parse run. Set by `Parse(KBufferedReader&)` for the duration of
	/// its loop. Used by derived `Content()`/`Script()` overrides to
	/// query `CurrentPos()` for source-slicing-aware text accumulation.
	const KBufferedReader* ActiveReader() const noexcept { return m_pActiveReader; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @returns true while the parser is inside an entity-decode loop,
	/// i.e. each `Content(char)` callback during that window is emitting
	/// a *decoded* byte that does not correspond 1:1 to a source byte at
	/// `ActiveReader()->CurrentPos()`. Slicing-aware consumers must
	/// treat those bytes as "transformed".
	bool IsDecodingEntity() const noexcept { return m_bDecodingEntity; }
	//-----------------------------------------------------------------------------

//------
private:
//------

	DEKAF2_PRIVATE void Script(KStringView sScript);
	DEKAF2_PRIVATE void Invalid(KStringView sInvalid);
	DEKAF2_PRIVATE void Invalid(const KHTMLStringObject& Object);
	DEKAF2_PRIVATE void Invalid(const KHTMLTag& Tag);
	DEKAF2_PRIVATE void SkipScript(KBufferedReader& InStream);
	DEKAF2_PRIVATE void SkipInvalid(KBufferedReader& InStream);

	khtml::Document*       m_pArena             { nullptr };
	const KBufferedReader* m_pActiveReader      { nullptr };
	bool                   m_bDecodingEntity    { false };
	bool                   m_bEmitEntitiesAsUTF8 { false };

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
		DEKAF2_FULL_CONSTEXPR_20
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

/// @}

DEKAF2_NAMESPACE_END
#endif
