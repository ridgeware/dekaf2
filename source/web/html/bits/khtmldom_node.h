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
/// part of the public API — only consumed by KHTML / KHTMLNode and the
/// associated unit tests.
///
/// Design constraints:
///   * trivially-destructible PODs only — they live in an arena that never
///     calls destructors
///   * small (NodePOD = 80 byte, AttrPOD = 48 byte) and tightly packed
///   * doubly-linked siblings for O(1) insert / erase at any position
///   * tail-pointer for first_child/last_child gives O(1) push_back
///
/// Strings are stored as KStringView referencing arena-owned bytes (or
/// straight into the data segment when the literal is recognised as such by
/// KArenaAllocator).

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/web/html/khtmlparser.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <unordered_map>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

namespace khtml {

/// Discriminator for the kind of node stored in a NodePOD.
enum class NodeKind : std::uint8_t
{
	Document,               ///< the document node - name is empty
	Element,                ///< <name attr="val">...</name> — Name = tag name
	Text,                   ///< character data, escaped or raw (see NodeFlag::TextDoNotEscape) — Name = content
	Comment,                ///< <!-- ... --> — Name = comment body
	CData,                  ///< <![CDATA[ ... ]]> — Name = cdata content
	ProcessingInstruction,  ///< <?xml ... ?> — Name = entire PI body opaque (target + body)
	DocumentType            ///< <!DOCTYPE ...> — Name = entire DOCTYPE body opaque
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

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// One HTML attribute as stored in the arena. Linked into NodePOD via the
/// AttrPOD::Next chain. 48 byte, trivially destructible. The owning node is
/// always known to the caller (you reach the attribute via the node), so
/// AttrPOD intentionally carries no parent pointer.
class AttrPOD
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	AttrPOD() = default;
	AttrPOD(KStringView sName,
	        KStringView sValue    = KStringView{},
	        char        chQuote   = 0,
	        bool        bDoEscape = true)
	: m_sName    (sName)
	, m_sValue   (sValue)
	, m_chQuote  (chQuote)
	, m_bDoEscape(bDoEscape)
	{
	}

	KStringView Name    () const { return m_sName;     }
	KStringView Value   () const { return m_sValue;    }
	char        Quote   () const { return m_chQuote;   }
	bool        DoEscape() const { return m_bDoEscape; }

	AttrPOD*    Next    () const { return m_Next;      }

	/// Replace the name view; caller is responsible for ensuring the bytes
	/// referenced live at least as long as this AttrPOD (use the arena).
	void Name (KStringView sName ) { m_sName  = sName;  }
	/// Replace the value view; caller-managed lifetime as above.
	void Value(KStringView sValue) { m_sValue = sValue; }

	void AppendAttr(AttrPOD* attr) { attr->m_Next = m_Next; m_Next = attr; }

	/// Replace the singly-linked-list 'Next' pointer. Used by container-side
	/// surgery in NodePOD (Remove etc.); not normally needed by user code.
	void SetNext(AttrPOD* pNext) noexcept { m_Next = pNext; }

//------
private:
//------

	KStringView m_sName;
	KStringView m_sValue;
	AttrPOD*    m_Next      { nullptr }; ///< next attribute in the singly-linked attr list
	char        m_chQuote   {       0 }; ///< original quote char from parser (' or "), 0 = decide on serialize
	bool        m_bDoEscape {    true }; ///< if false, the value is already entity-encoded

}; // AttrPOD

static_assert(std::is_trivially_destructible<AttrPOD>::value,
	"AttrPOD must be trivially destructible to live in KArenaAllocator");

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generic DOM node. The only string payload is Name — its interpretation
/// depends on Kind:
///
///   Kind                    Name
///   ----                    ----
///   Document                (empty)
///   Element                 tag name
///   Text                    text content
///   Comment                 comment body
///   CData                   cdata content
///   ProcessingInstruction   entire PI body opaque (target + body)
///   DocumentType            entire DOCTYPE body opaque
///
/// Parent / sibling / child pointers form a doubly-linked tree. first_attr
/// and last_attr together give O(1) AppendAttr. 80 byte, trivially
/// destructible.
class NodePOD
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	NodePOD() = default;
	NodePOD(NodePOD* Parent, NodeKind kind)
	: m_Parent(Parent)
	, m_Kind  (kind)
	{
	}

	NodeKind                 Kind    () const { return m_Kind;     }
	NodeFlag                 Flags   () const { return m_Flags;    }
	KHTMLObject::TagProperty TagProps() const { return m_TagProps; }

	KStringView Name() const { return m_sName; }

	NodePOD* Parent     () const { return m_Parent;       }
	AttrPOD* FirstAttr  () const { return m_FirstAttr;    }
	AttrPOD* LastAttr   () const { return m_LastAttr;     }
	NodePOD* FirstChild () const { return m_FirstChild;   }
	NodePOD* LastChild  () const { return m_LastChild;    }
	NodePOD* NextSibling() const { return m_NextSibling;  }
	NodePOD* PrevSibling() const { return m_PrevSibling;  }

	NodePOD* Flags(NodeFlag flags) { m_Flags = flags; return this; }

	/// Replace the name view; caller is responsible for ensuring the bytes
	/// referenced live at least as long as this NodePOD (use the arena
	/// reachable via Document()).
	void Name(KStringView sName)
	{
		m_sName = sName;

		if (m_Kind == NodeKind::Element)
		{
			m_TagProps = KHTMLObject::GetTagProperty(sName);
		}
	}

	/// Append a new child NodePOD allocated in the document's arena. The
	/// child gets its name and kind set; its TagProps is precomputed for
	/// Element kinds. Returns nullptr if no document is reachable from this
	/// node (orphaned subtree).
	NodePOD* AddNode(NodeKind kind, KStringView sName = KStringView{});

	/// Insert a new child NodePOD immediately before pBefore. If pBefore is
	/// null, equivalent to AddNode(kind, sName). Returns nullptr on
	/// orphaned-subtree.
	NodePOD* AddNode(NodePOD* pBefore, NodeKind kind, KStringView sName = KStringView{});

	/// Append a text child. The text bytes are copied into the arena (or
	/// reused in place if the bytes are inside the data segment — see
	/// KArenaAllocator::AllocateString).
	NodePOD* AddText(KStringView sText, bool bDoNotEscape = false);

	/// Append a text child WITHOUT copying the bytes. The caller asserts
	/// that `sText` is already in stable storage (typically the
	/// document's own arena, e.g. via `KArenaStringBuilder`). Use this
	/// when the bytes were just freshly written into the arena and there
	/// is no need to go through `AllocateString`'s data-segment /
	/// stable-region check.
	NodePOD* AddTextAdopt(KStringView sText, bool bDoNotEscape = false);

	/// Append a new AttrPOD allocated in the document's arena.
	AttrPOD* AddAttribute(KStringView sName,
	                      KStringView sValue,
	                      char        chQuote   = '\0',
	                      bool        bDoEscape = true);

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

	/// const overload of Attribute().
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

	/// Remove the attribute with the given name from this node's chain. The
	/// AttrPOD memory stays in the arena (no individual free); only the
	/// linked-list pointers are updated. @returns true if an attribute was
	/// removed, false if no match was found. O(n).
	bool RemoveAttribute(KStringView sName) noexcept
	{
		AttrPOD* pPrev = nullptr;

		for (AttrPOD* a = m_FirstAttr; a != nullptr; pPrev = a, a = a->Next())
		{
			if (a->Name() == sName)
			{
				AttrPOD* pNext = a->Next();

				if (pPrev != nullptr)
				{
					pPrev->SetNext(pNext);
				}
				else
				{
					m_FirstAttr = pNext;
				}

				if (pNext == nullptr)
				{
					m_LastAttr = pPrev;
				}

				return true;
			}
		}

		return false;
	}

	void clear(NodeKind kind = NodeKind::Element)
	{
		*this  = NodePOD();
		m_Kind = kind;
	}

	/// Walk to the root of this tree (which may be the Document, or any
	/// orphan subtree's top node).
	NodePOD* Root() noexcept
	{
		NodePOD* p = this;
		while (p->m_Parent) p = p->m_Parent;
		return p;
	}

	/// Walk to the Document root of this tree (or nullptr if this subtree
	/// is not anchored under a Document).
	class Document* Document() noexcept;

//------
private:
//------

	NodePOD*      m_Parent      { nullptr };
	KStringView   m_sName;                                          ///< see Kind/Name table above
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
/// Side-map binding for interactive nodes (input, select, radio, checkbox).
/// Lives in `Document::m_Bindings`, keyed by the NodePOD pointer of the
/// input/select. The binding is the type-erased connection between an
/// arena-allocated NodePOD and an App-Variable that the input synchronises
/// with.
///
/// pfnSync is invoked by `Synchronize()` for each binding whose `name`
/// attribute matches a query-parameter key. pfnReset is invoked before
/// Sync to clear the App-Variable (so unchecked checkboxes / unselected
/// radios get the right default).
struct InteractiveBinding
{
	void* pResult            { nullptr };   ///< raw pointer to App-Variable
	void (*pfnSync)  (void* pResult, NodePOD* pInput, KStringView sValue) { nullptr };
	void (*pfnReset) (void* pResult, NodePOD* pInput)                     { nullptr };
};
static_assert(std::is_trivially_destructible<InteractiveBinding>::value,
	"InteractiveBinding must be trivially destructible");

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The Document is the synthetic root of the POD tree AND the owner of the
/// arena. Inheriting both ways keeps the tree self-contained: any NodePOD
/// can walk up to its Document and call AllocateString() / Construct<T>()
/// on it.
///
/// Each Document carries an inline 4-KiB buffer that the allocator uses as
/// its first backing region. Small documents (admin pages, JSON-embedded
/// snippets) fit entirely inside this buffer and pay **zero** heap
/// allocations. Larger documents grow into heap blocks once the inline
/// buffer is exhausted.
///
/// Note on move semantics: moving a populated Document is **UB**. The
/// inline buffer lives at a stable per-object address, but pointers stored
/// in arena-allocated nodes/attrs reference that address. NRVO typically
/// elides the move for return-by-value, so the common case is safe. Do not
/// `std::move(populated_doc)`. (This mirrors rapidxml's xml_document.)
class Document : public NodePOD, public KArenaAllocator
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// Size of the inline backing buffer carried inside every Document.
	/// Sized to fit a typical admin-page / small-snippet HTML build in a
	/// single allocator region (no heap call).
	static constexpr std::size_t kInlineBufferSize = 4096;

	Document(std::size_t iArenaSize = KArenaAllocator::DefaultBlockSize)
	: NodePOD(nullptr, NodeKind::Document)
	, KArenaAllocator(iArenaSize)
	{
		// Adopt our inline buffer post-base-init — at the init-list
		// point, the base class would access the still-uninitialised
		// std::array member (technically UB; the compiler warns).
		AdoptInlineBuffer(m_InitialBuffer.data(), m_InitialBuffer.size());
	}

	// Non-copyable (KArenaAllocator is non-copyable).
	Document(const Document&)            = delete;
	Document& operator=(const Document&) = delete;

	// Movable, but with the UB caveat above.
	Document(Document&& other) noexcept
	: NodePOD         (std::move(other))
	, KArenaAllocator (std::move(other))
	, m_Bindings      (std::move(other.m_Bindings))
	, m_Classes       (std::move(other.m_Classes))
	{
		// Copy the inline buffer bytes from other to us, then re-point
		// the allocator's m_pInline to our own buffer (it still points
		// at other's after the base-class move).
		m_InitialBuffer = other.m_InitialBuffer;
		AdoptInlineBuffer(m_InitialBuffer.data(), m_InitialBuffer.size());
	}

	Document& operator=(Document&&) = delete;

	void reset()
	{
		NodePOD::clear(NodeKind::Document);
		KArenaAllocator::reset();
		m_Bindings.clear();
		m_Classes.clear();
	}

	void clear()
	{
		NodePOD::clear(NodeKind::Document);
		KArenaAllocator::clear();
		m_Bindings.clear();
		m_Classes.clear();
	}

	/// Register a binding for an interactive node. Overwrites any previous
	/// binding for the same NodePOD.
	void Bind(NodePOD* pInput, const InteractiveBinding& binding)
	{
		m_Bindings[pInput] = binding;
	}

	/// @returns the binding for `pInput`, or nullptr if no binding exists.
	const InteractiveBinding* FindBinding(NodePOD* pInput) const
	{
		auto it = m_Bindings.find(pInput);
		return (it == m_Bindings.end()) ? nullptr : &it->second;
	}

	using BindingMap = std::unordered_map<NodePOD*, InteractiveBinding>;
	const BindingMap& Bindings() const { return m_Bindings; }
	BindingMap&       Bindings()       { return m_Bindings; }

	/// Aggregated CSS class definitions for emission into a `<style>` block.
	/// Keys and values are arena-allocated views (interned via AllocateString).
	using ClassMap = std::unordered_map<KStringView, KStringView>;
	const ClassMap& Classes() const { return m_Classes; }
	ClassMap&       Classes()       { return m_Classes; }

//------
private:
//------

	BindingMap m_Bindings;
	ClassMap   m_Classes;

	alignas(8) std::array<char, kInlineBufferSize> m_InitialBuffer {}; ///< inline arena backing

}; // Document

//-----------------------------------------------------------------------------
inline Document* NodePOD::Document() noexcept
//-----------------------------------------------------------------------------
{
	auto* pRoot = Root();

	if (pRoot == nullptr || pRoot->Kind() != NodeKind::Document)
	{
		return nullptr;
	}

	return static_cast<class Document*>(pRoot);
}

//-----------------------------------------------------------------------------
inline NodePOD* NodePOD::AddNode(NodeKind kind, KStringView sName)
//-----------------------------------------------------------------------------
{
	auto* pDoc = Document();

	if (pDoc == nullptr)
	{
		// orphaned subtree — cannot allocate
		return nullptr;
	}

	auto* pNode = pDoc->Construct<NodePOD>(this, kind);

	if (!sName.empty())
	{
		pNode->Name(pDoc->AllocateString(sName));
	}

	AppendChild(pNode);

	return pNode;
}

//-----------------------------------------------------------------------------
inline NodePOD* NodePOD::AddNode(NodePOD* pBefore, NodeKind kind, KStringView sName)
//-----------------------------------------------------------------------------
{
	auto* pDoc = Document();

	if (pDoc == nullptr)
	{
		return nullptr;
	}

	auto* pNode = pDoc->Construct<NodePOD>(this, kind);

	if (!sName.empty())
	{
		pNode->Name(pDoc->AllocateString(sName));
	}

	InsertChildBefore(pBefore, pNode);

	return pNode;
}

//-----------------------------------------------------------------------------
inline NodePOD* NodePOD::AddText(KStringView sText, bool bDoNotEscape)
//-----------------------------------------------------------------------------
{
	auto* pDoc = Document();

	if (pDoc == nullptr)
	{
		return nullptr;
	}

	auto* pNode = pDoc->Construct<NodePOD>(this, NodeKind::Text);

	if (!sText.empty())
	{
		pNode->m_sName = pDoc->AllocateString(sText);
	}

	if (bDoNotEscape)
	{
		pNode->m_Flags = pNode->m_Flags | NodeFlag::TextDoNotEscape;
	}

	AppendChild(pNode);

	return pNode;
}

//-----------------------------------------------------------------------------
inline NodePOD* NodePOD::AddTextAdopt(KStringView sText, bool bDoNotEscape)
//-----------------------------------------------------------------------------
{
	auto* pDoc = Document();

	if (pDoc == nullptr)
	{
		return nullptr;
	}

	auto* pNode = pDoc->Construct<NodePOD>(this, NodeKind::Text);

	// adopt the view as-is — caller asserts it points into stable storage
	pNode->m_sName = sText;

	if (bDoNotEscape)
	{
		pNode->m_Flags = pNode->m_Flags | NodeFlag::TextDoNotEscape;
	}

	AppendChild(pNode);

	return pNode;
}

//-----------------------------------------------------------------------------
inline AttrPOD* NodePOD::AddAttribute(KStringView sName, KStringView sValue, char chQuote, bool bDoEscape)
//-----------------------------------------------------------------------------
{
	auto* pDoc = Document();

	if (pDoc == nullptr)
	{
		return nullptr;
	}

	auto* pAttr = pDoc->Construct<AttrPOD>
	(
		pDoc->AllocateString(sName),
		pDoc->AllocateString(sValue),
		chQuote,
		bDoEscape
	);

	AppendAttr(pAttr);

	return pAttr;
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
