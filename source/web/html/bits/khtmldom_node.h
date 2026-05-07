/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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

/// @file khtmldom_node.h
/// Internal POD node and attribute types for the arena-based KHTMLDOM. Not
/// part of the public API — only consumed by KHTML / KHTMLElement and the
/// associated unit tests.
///
/// Design constraints:
///   * trivially-destructible PODs only — they live in an arena that never
///     calls destructors
///   * small (currently 96 bytes) and tightly packed
///   * doubly-linked siblings for O(1) insert / erase at any position
///   * tail-pointer for first_child/last_child gives O(1) push_back
///
/// Strings are stored as KStringView referencing arena-owned bytes.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/web/html/khtmlparser.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

DEKAF2_NAMESPACE_BEGIN

namespace khtml {

/// Discriminator for the kind of node stored in a NodePOD.
enum class NodeKind : std::uint8_t
{
	Document,               ///< the document node - name and value are empty
	Element,                ///< <name attr="val">...</name>
	Text,                   ///< character data, escaped or raw (see NodeFlag::TextDoNotEscape)
	Comment,                ///< <!-- ... -->
	CData,                  ///< <![CDATA[ ... ]]>
	ProcessingInstruction,  ///< <?xml ... ?>
	DocumentType            ///< <!DOCTYPE ...>
};

/// Bit flags that decorate a NodePOD beyond its kind. Combined via the
/// usual bitwise operators (provided by DEKAF2_ENUM_IS_FLAG below).
enum NodeFlag : std::uint8_t
{
	None            = 0,
	/// Text node: bytes in NodePOD::Name are already entity-encoded (or must
	/// not be encoded — like inside <script>) and must be emitted verbatim.
	TextDoNotEscape = 1u << 0
};

DEKAF2_ENUM_IS_FLAG(NodeFlag)

class NodePOD;
class Document;

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class NodeBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	NodeBase() = default;
	NodeBase(NodePOD*    Parent,
	         KStringView sName  = KStringView{},
	         KStringView sValue = KStringView{})
	: m_Parent (Parent)
	, m_sName  (CreateString(sName) )
	, m_sValue (CreateString(sValue))
	{
	}

	KStringView Name     () const { return m_sName;  }
	KStringView Value    () const { return m_sValue; }

	NodePOD*    Parent   () const { return m_Parent; };

	/// Set new name - sName will be stored in the arena
	void        Name     (KStringView sName ) { Name (Document(), sName ); }
	/// Set new value - sValue will be stored in the arena
	void        Value    (KStringView sValue) { Value(Document(), sValue); }

//------
protected:
//------

	/// Returns root element of tree
	NodePOD* Root () noexcept;
	/// Returns document root of tree if any, else nullptr
	class Document* Document () noexcept;

	/// places a string in the arena of document and returns a view on it
	static KStringView CreateString (class Document* document, KStringView sStr);
	/// places a string in the arena of this node's document and returns a view on it - you still have to put the view somewhere
	KStringView CreateString (KStringView sStr);
	/// places a name in the arena of document and sets it in this node
	void Name (class Document* document, KStringView sName );
	/// places a value in the arena of document and sets it in this node
	void Value(class Document* document, KStringView sValue);

	NodePOD*    m_Parent { nullptr };
	KStringView m_sName;  ///< primary payload (see table above)
	KStringView m_sValue; ///< secondary payload (see table above)

}; // NodeBase

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// One HTML attribute as stored in the arena. Linked into NodePOD via the
/// AttrPOD::Next chain.
class AttrPOD : public detail::NodeBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	AttrPOD() = default;
	AttrPOD(NodePOD*    Parent,
	        KStringView sName,
	        KStringView sValue = KStringView{},
	        char        chQuote   = 0,
	        bool        bDoEscape = true)
	: NodeBase(Parent, sName, sValue)
	, m_chQuote(chQuote)
	, m_bDoEscape(bDoEscape)
	{
	}

	std::size_t size() const
	{
		std::size_t iCount = 1;
		AttrPOD* a = m_Next;
		while (a)
		{
			++iCount;
			a = a->m_Next;
		}
		return iCount;
	}

	bool empty() const { return false; }

	char     Quote   () const { return m_chQuote;   }
	bool     DoEscape() const { return m_bDoEscape; }

	NodePOD* Parent  () const { return m_Parent;    }
	AttrPOD* Next    () const { return m_Next;      }

	void     AppendAttr(AttrPOD* attr) { attr->m_Next = m_Next; m_Next = attr; }

//------
private:
//------

	AttrPOD* m_Next      { nullptr }; ///< next attribute in the singly-linked attr list
	char     m_chQuote   {       0 }; ///< original quote char from parser (' or "), 0 = decide on serialize
	bool     m_bDoEscape {    true }; ///< if false, the value is already entity-encoded

}; // AttrPOD

static_assert(std::is_trivially_destructible<AttrPOD>::value,
	"AttrPOD must be trivially destructible to live in KArenaAllocator");

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generic DOM node. The exact interpretation of Name/Value depends on Kind:
///
///   Kind                    Name                    Value
///   ----                    ----                    -----
///   Element                 tag name                (unused)
///   Text                    text content            (unused)
///   Comment                 comment text            (unused)
///   CData                   cdata content           (unused)
///   ProcessingInstruction   target ("xml")          instruction body
///   DocumentType            doctype declaration     (unused)
///
/// Parent / sibling / child pointers form a doubly-linked tree. first_attr
/// and last_attr together give O(1) AppendAttr.
class NodePOD : public detail::NodeBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	NodePOD() = default;
	NodePOD(NodePOD* Parent, NodeKind kind)
	: NodeBase(Parent)
	, m_Kind(kind)
	{
	}

	NodeKind Kind () const { return m_Kind;  }
	NodeFlag Flags() const { return m_Flags; }
	KHTMLObject::TagProperty TagProps() const { return m_TagProps; }

	AttrPOD* FirstAttr() const { return m_FirstAttr; };
	AttrPOD* LastAttr () const { return m_LastAttr;  };

	NodePOD* FirstChild () const { return m_FirstChild;  };
	NodePOD* LastChild  () const { return m_LastChild;   };
	NodePOD* NextSibling() const { return m_NextSibling; };
	NodePOD* PrevSibling() const { return m_PrevSibling; };

	NodePOD* Flags(NodeFlag flags) { m_Flags = flags; return this; }

	NodePOD* AddNode(NodeKind Kind,
	                 KStringView sName  = KStringView{},
	                 KStringView sValue = KStringView{});

	NodePOD* AddNode(NodePOD* Before,
	                 NodeKind Kind,
	                 KStringView sName  = KStringView{},
	                 KStringView sValue = KStringView{});

	NodePOD* AddText(KStringView sText, bool bDoNotEscape = false);

	AttrPOD* AddAttribute(KStringView sName,
	                      KStringView sValue,
	                      char chQuote   = '\0',
	                      bool bDoEscape = true);

	/// Append 'pAttr' at the end of this node's attribute chain. O(1).
	void AppendAttr(AttrPOD* pAttr) noexcept
	{
		if (this->m_LastAttr != nullptr)
		{
			this->m_LastAttr->AppendAttr(pAttr);
		}
		else
		{
			kAssert(pAttr->Next() == nullptr, "pAttr->Next() must be nullptr");
			this->m_FirstAttr = pAttr;
		}

		this->m_LastAttr = pAttr;
	}

	/// Append 'pChild' as the last child of this node. O(1).
	void AppendChild(NodePOD* pChild) noexcept
	{
		pChild->m_Parent      = this;
		pChild->m_NextSibling = nullptr;
		pChild->m_PrevSibling = this->m_LastChild;

		if (this->m_LastChild != nullptr)
		{
			this->m_LastChild->m_NextSibling = pChild;
		}
		else
		{
			this->m_FirstChild = pChild;
		}

		this->m_LastChild = pChild;
	}

	/// Insert 'pNew' immediately before 'pBefore'. If 'pBefore'
	/// is null, behaves like AppendChild. O(1).
	void InsertChildBefore(NodePOD* pBefore, NodePOD* pNew) noexcept
	{
		if (pBefore == nullptr)
		{
			AppendChild(pNew);
			return;
		}

		pNew->m_Parent      = this;
		pNew->m_NextSibling = pBefore;
		pNew->m_PrevSibling = pBefore->m_PrevSibling;

		if (pBefore->m_PrevSibling != nullptr)
		{
			pBefore->m_PrevSibling->m_NextSibling = pNew;
		}
		else
		{
			this->m_FirstChild = pNew;
		}

		pBefore->m_PrevSibling = pNew;
	}

	/// Unlink from parent and siblings. The arena keeps owning the
	/// memory — this node simply becomes unreachable from the tree. O(1).
	void Detach() noexcept
	{
		NodePOD* pParent = this->m_Parent;

		if (pParent == nullptr)
		{
			return;
		}

		if (this->m_PrevSibling != nullptr)
		{
			this->m_PrevSibling->m_NextSibling = this->m_NextSibling;
		}
		else
		{
			pParent->m_FirstChild = this->m_NextSibling;
		}

		if (this->m_NextSibling != nullptr)
		{
			this->m_NextSibling->m_PrevSibling = this->m_PrevSibling;
		}
		else
		{
			pParent->m_LastChild = this->m_PrevSibling;
		}

		this->m_Parent      = nullptr;
		this->m_NextSibling = nullptr;
		this->m_PrevSibling = nullptr;
	}

	/// @returns the number of children of this node. O(n).
	std::size_t CountChildren() const noexcept
	{
		std::size_t i = 0;

		for (const NodePOD* c = this->m_FirstChild; c != nullptr; c = c->m_NextSibling)
		{
			++i;
		}

		return i;
	}

	/// @returns the number of attributes on 'pNode'. O(n).
	std::size_t CountAttrs() const noexcept
	{
		std::size_t i = 0;

		for (const AttrPOD* a = this->m_FirstAttr; a != nullptr; a = a->Next())
		{
			++i;
		}

		return i;
	}

	/// Find an attribute on 'pNode' by name. Linear scan; HTML elements rarely
	/// have more than a handful of attributes, so this is intentionally simple.
	/// @returns the AttrPOD* if found, nullptr otherwise.
	AttrPOD* Attribute(KStringView sName) noexcept
	{
		for (AttrPOD* a = this->m_FirstAttr; a != nullptr; a = a->Next())
		{
			if (a->Name() == sName)
			{
				return a;
			}
		}

		return nullptr;
	}

	/// const overload of FindAttr().
	const AttrPOD* Attribute(KStringView sName) const noexcept
	{
		for (const AttrPOD* a = this->m_FirstAttr; a != nullptr; a = a->Next())
		{
			if (a->Name() == sName)
			{
				return a;
			}
		}

		return nullptr;
	}

	void clear(NodeKind kind = NodeKind::Element)
	{
		*this  = NodePOD();
		m_Kind = kind;
	}

//------
private:
//------

	NodeKind      m_Kind        { NodeKind::Element };
	NodeFlag      m_Flags       { NodeFlag::None    };              ///< OR-combination of NodeFlag bits
	KHTMLObject::TagProperty
	              m_TagProps    { KHTMLObject::TagProperty::None }; ///< cached HTML tag property bits

	AttrPOD*      m_FirstAttr   { nullptr };
	AttrPOD*      m_LastAttr    { nullptr };

	NodePOD*      m_FirstChild  { nullptr };
	NodePOD*      m_LastChild   { nullptr };
	NodePOD*      m_NextSibling { nullptr };
	NodePOD*      m_PrevSibling { nullptr };

}; // NodePOD

static_assert(std::is_trivially_destructible<NodePOD>::value,
	"NodePOD must be trivially destructible to live in KArenaAllocator");

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Document : public NodePOD, public KArenaAllocator
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	Document(std::size_t iArenaSize = KArenaAllocator::DefaultBlockSize)
	: NodePOD(nullptr, NodeKind::Document)
	, KArenaAllocator(iArenaSize)
	{
	}

	void reset()
	{
		NodePOD::clear(NodeKind::Document);
		KArenaAllocator::reset();
	}

	void clear()
	{
		NodePOD::clear(NodeKind::Document);
		KArenaAllocator::clear();
	}

}; // Document

//-----------------------------------------------------------------------------
	/// Returns root element of tree
inline NodePOD* detail::NodeBase::Root() noexcept
//-----------------------------------------------------------------------------
{
	if (!m_Parent)
	{
		return static_cast<class NodePOD*>(this);
	}

	auto* pNode = m_Parent;

	if (pNode)
	{
		while (pNode->m_Parent)
		{
			pNode = pNode->m_Parent;
		}
	}

	return pNode;
}

//-----------------------------------------------------------------------------
/// Returns document root of tree if any, else nullptr
inline class Document* detail::NodeBase::Document() noexcept
//-----------------------------------------------------------------------------
{
	auto* pNode = Root();
	if (!pNode || pNode->Kind() != NodeKind::Document) return nullptr;
	return static_cast<class Document*>(pNode);
}

//-----------------------------------------------------------------------------
inline KStringView detail::NodeBase::CreateString(class Document* document, KStringView sStr)
//-----------------------------------------------------------------------------
{
	if (!document || sStr.empty()) return KStringView{};
	return document->AllocateString(sStr);
}

//-----------------------------------------------------------------------------
inline KStringView detail::NodeBase::CreateString(KStringView sStr)
//-----------------------------------------------------------------------------
{
	return CreateString(Document(), sStr);
}

//-----------------------------------------------------------------------------
inline void detail::NodeBase::Name (class Document* document, KStringView sName )
//-----------------------------------------------------------------------------
{
	if (!document || sName.empty()) return;
	m_sName = CreateString(document, sName);
}

//-----------------------------------------------------------------------------
inline void detail::NodeBase::Value(class Document* document, KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (!document || sValue.empty()) return;
	m_sValue = CreateString(document, sValue);
}

//-----------------------------------------------------------------------------
// Helpers — every helper below operates on arena-owned NodePOD/AttrPOD only
// and never allocates outside of the arena.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Allocate a new NodePOD of the given kind in 'arena'.
inline NodePOD* CreateNode(KArenaAllocator& arena, NodePOD* Parent, NodeKind kind)
//-----------------------------------------------------------------------------
{
	return arena.Construct<NodePOD>(Parent, kind);
}

//-----------------------------------------------------------------------------
/// Allocate a new AttrPOD with arena-copied name/value.
/// @param chQuote original quote char from parser (' or "), 0 = let serializer decide
inline AttrPOD* CreateAttr(KArenaAllocator& arena,
                           NodePOD*         Parent,
                           KStringView      sName,
                           KStringView      sValue,
                           char             chQuote   = 0,
                           bool             bDoEscape = true)
//-----------------------------------------------------------------------------
{
	return arena.Construct<AttrPOD>
	(
		Parent,
		sName,
		sValue,
		chQuote,
		bDoEscape
	);
}

//-----------------------------------------------------------------------------
inline NodePOD* NodePOD::AddNode(NodeKind kind, KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	auto* document = Document();

	if (!document)
	{
		// TODO log
		return nullptr;
	}

	auto* pNode = CreateNode(*document, this, kind);

	if (!sName.empty())
	{
		pNode->Name(sName);

		if (kind == NodeKind::Element)
		{
			// get the tag props right here
			pNode->m_TagProps = KHTMLObject::GetTagProperty(sName);
		}
	}

	if (!sValue.empty())
	{
		pNode->Value(sName);
	}

	AppendChild(pNode);

	return pNode;
}

//-----------------------------------------------------------------------------
inline NodePOD* NodePOD::AddNode(NodePOD* Before, NodeKind kind, KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	auto* document = Document();

	if (!document)
	{
		// TODO log
		return nullptr;
	}

	auto* pNode = CreateNode(*document, this, kind);

	if (!sName.empty())
	{
		pNode->Name(sName);
	}

	if (!sValue.empty())
	{
		pNode->Value(sName);
	}

	InsertChildBefore(Before, pNode);

	return pNode;
}

//-----------------------------------------------------------------------------
inline NodePOD* NodePOD::AddText(KStringView sText, bool bDoNotEscape)
//-----------------------------------------------------------------------------
{
	auto* document = Document();

	if (!document)
	{
		// TODO log
		return nullptr;
	}

	auto* pNode = CreateNode(*document, this, NodeKind::Text);

	if (!sText.empty())
	{
		pNode->Name(sText);
	}

	if (bDoNotEscape)
	{
		pNode->m_Flags |= khtml::NodeFlag::TextDoNotEscape;
	}

	AppendChild(pNode);

	return pNode;
}

//-----------------------------------------------------------------------------
inline AttrPOD* NodePOD::AddAttribute(KStringView sName, KStringView sValue, char chQuote , bool bDoEscape)
//-----------------------------------------------------------------------------
{
	auto* document = Document();

	if (!document)
	{
		// TODO log
		return nullptr;
	}

	auto* attr = CreateAttr(*document, this, sName, sValue, chQuote, bDoEscape);

	AppendAttr(attr);

	return attr;
}

//-----------------------------------------------------------------------------
// Free functions defined in khtmldom.cpp — operate on POD-trees only.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Serialize the POD-tree rooted at 'pNode' to 'OutStream'. Output is
/// byte-equivalent to KHTMLElement::Print() for any tree built by KHTML's
/// parser. 'pNode' may be the synthetic root (Element node with empty Name)
/// or any element / leaf node.
DEKAF2_PUBLIC
void SerializeNode(KOutStream& OutStream, const NodePOD* pNode, char chIndent = '\t');
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Walk the POD-tree from the last child of 'pNode' backwards and remove
/// trailing whitespace, mirroring KHTMLElement::RemoveTrailingWhitespace().
/// Skips comment / pi / doctype nodes; trims trailing whitespace from text
/// nodes; recurses into non-script inline children. Returns true if traversal
/// stopped before the (reverse) end (non-whitespace text or block element
/// encountered), false if the entire range was empty / removable.
DEKAF2_PUBLIC
bool PodRemoveTrailingWhitespace(NodePOD* pNode, bool bStopAtBlockElement = true);
//-----------------------------------------------------------------------------

} // namespace khtml

DEKAF2_NAMESPACE_END
