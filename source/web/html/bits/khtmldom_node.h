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

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Discriminator for the kind of node stored in a NodePOD.
enum class NodeKind : std::uint8_t
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	Element,                ///< <name attr="val">...</name>
	Text,                   ///< character data, escaped or raw (see NodeFlag::TextDoNotEscape)
	Comment,                ///< <!-- ... -->
	CData,                  ///< <![CDATA[ ... ]]>
	ProcessingInstruction,  ///< <?xml ... ?>
	DocumentType            ///< <!DOCTYPE ...>
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Bit flags that decorate a NodePOD beyond its kind. Combined via the
/// usual bitwise operators (provided by DEKAF2_ENUM_IS_FLAG below).
enum NodeFlag : std::uint8_t
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	None            = 0,
	/// Text node: bytes in NodePOD::Name are already entity-encoded (or must
	/// not be encoded — like inside <script>) and must be emitted verbatim.
	TextDoNotEscape = 1u << 0
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// One HTML attribute as stored in the arena. Linked into NodePOD via the
/// AttrPOD::Next chain.
struct AttrPOD
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	KStringView Name;                ///< attribute name (arena-owned bytes)
	KStringView Value;               ///< attribute value (arena-owned bytes)
	AttrPOD*    Next     { nullptr };///< next attribute in the singly-linked attr list
	char        Quote    { 0 };      ///< original quote char from parser (' or "), 0 = decide on serialize
	bool        DoEscape { true    };///< if false, the value is already entity-encoded
};

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
struct NodePOD
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	NodeKind      Kind        { NodeKind::Element };
	NodeFlag      Flags       { NodeFlag::None };                         ///< OR-combination of NodeFlag bits
	KHTMLObject::TagProperty TagProps { KHTMLObject::TagProperty::None }; ///< cached HTML tag property bits

	KStringView   Name;       ///< primary payload (see table above)
	KStringView   Value;      ///< secondary payload (see table above)

	AttrPOD*      FirstAttr   { nullptr };
	AttrPOD*      LastAttr    { nullptr };

	NodePOD*      FirstChild  { nullptr };
	NodePOD*      LastChild   { nullptr };
	NodePOD*      NextSibling { nullptr };
	NodePOD*      PrevSibling { nullptr };
	NodePOD*      Parent      { nullptr };
};

static_assert(std::is_trivially_destructible<NodePOD>::value,
	"NodePOD must be trivially destructible to live in KArenaAllocator");

//-----------------------------------------------------------------------------
// Helpers — every helper below operates on arena-owned NodePOD/AttrPOD only
// and never allocates outside of the arena.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Allocate a new NodePOD of the given kind in 'arena'.
inline NodePOD* CreateNode(KArenaAllocator& arena, NodeKind kind)
//-----------------------------------------------------------------------------
{
	auto* node = arena.Construct<NodePOD>();
	node->Kind = kind;
	return node;
}

//-----------------------------------------------------------------------------
/// Allocate a new AttrPOD with arena-copied name/value.
/// @param chQuote original quote char from parser (' or "), 0 = let serializer decide
inline AttrPOD* CreateAttr(KArenaAllocator& arena,
                           KStringView      sName,
                           KStringView      sValue,
                           char             chQuote   = 0,
                           bool             bDoEscape = true)
//-----------------------------------------------------------------------------
{
	auto* attr      = arena.Construct<AttrPOD>();
	attr->Name      = arena.AllocateString(sName);
	attr->Value     = arena.AllocateString(sValue);
	attr->Quote     = chQuote;
	attr->DoEscape  = bDoEscape;
	return attr;
}

//-----------------------------------------------------------------------------
/// Append 'pAttr' at the end of 'pNode's attribute chain. O(1).
inline void AppendAttr(NodePOD* pNode, AttrPOD* pAttr) noexcept
//-----------------------------------------------------------------------------
{
	pAttr->Next = nullptr;

	if (pNode->LastAttr != nullptr)
	{
		pNode->LastAttr->Next = pAttr;
	}
	else
	{
		pNode->FirstAttr = pAttr;
	}

	pNode->LastAttr = pAttr;
}

//-----------------------------------------------------------------------------
/// Append 'pChild' as the last child of 'pParent'. O(1).
inline void AppendChild(NodePOD* pParent, NodePOD* pChild) noexcept
//-----------------------------------------------------------------------------
{
	pChild->Parent      = pParent;
	pChild->NextSibling = nullptr;
	pChild->PrevSibling = pParent->LastChild;

	if (pParent->LastChild != nullptr)
	{
		pParent->LastChild->NextSibling = pChild;
	}
	else
	{
		pParent->FirstChild = pChild;
	}

	pParent->LastChild = pChild;
}

//-----------------------------------------------------------------------------
/// Insert 'pNew' immediately before 'pBefore' inside 'pParent'. If 'pBefore'
/// is null, behaves like AppendChild. O(1).
inline void InsertChildBefore(NodePOD* pParent, NodePOD* pBefore, NodePOD* pNew) noexcept
//-----------------------------------------------------------------------------
{
	if (pBefore == nullptr)
	{
		AppendChild(pParent, pNew);
		return;
	}

	pNew->Parent      = pParent;
	pNew->NextSibling = pBefore;
	pNew->PrevSibling = pBefore->PrevSibling;

	if (pBefore->PrevSibling != nullptr)
	{
		pBefore->PrevSibling->NextSibling = pNew;
	}
	else
	{
		pParent->FirstChild = pNew;
	}

	pBefore->PrevSibling = pNew;
}

//-----------------------------------------------------------------------------
/// Unlink 'pNode' from its parent and siblings. The arena keeps owning the
/// memory — the node simply becomes unreachable from the tree. O(1).
inline void DetachChild(NodePOD* pNode) noexcept
//-----------------------------------------------------------------------------
{
	NodePOD* pParent = pNode->Parent;

	if (pParent == nullptr)
	{
		return;
	}

	if (pNode->PrevSibling != nullptr)
	{
		pNode->PrevSibling->NextSibling = pNode->NextSibling;
	}
	else
	{
		pParent->FirstChild = pNode->NextSibling;
	}

	if (pNode->NextSibling != nullptr)
	{
		pNode->NextSibling->PrevSibling = pNode->PrevSibling;
	}
	else
	{
		pParent->LastChild = pNode->PrevSibling;
	}

	pNode->Parent      = nullptr;
	pNode->NextSibling = nullptr;
	pNode->PrevSibling = nullptr;
}

//-----------------------------------------------------------------------------
/// @returns the number of children of 'pNode'. O(n).
inline std::size_t CountChildren(const NodePOD* pNode) noexcept
//-----------------------------------------------------------------------------
{
	std::size_t i = 0;

	for (const NodePOD* c = pNode->FirstChild; c != nullptr; c = c->NextSibling)
	{
		++i;
	}

	return i;
}

//-----------------------------------------------------------------------------
/// @returns the number of attributes on 'pNode'. O(n).
inline std::size_t CountAttrs(const NodePOD* pNode) noexcept
//-----------------------------------------------------------------------------
{
	std::size_t i = 0;

	for (const AttrPOD* a = pNode->FirstAttr; a != nullptr; a = a->Next)
	{
		++i;
	}

	return i;
}

//-----------------------------------------------------------------------------
/// Find an attribute on 'pNode' by name. Linear scan; HTML elements rarely
/// have more than a handful of attributes, so this is intentionally simple.
/// @returns the AttrPOD* if found, nullptr otherwise.
inline AttrPOD* FindAttr(NodePOD* pNode, KStringView sName) noexcept
//-----------------------------------------------------------------------------
{
	for (AttrPOD* a = pNode->FirstAttr; a != nullptr; a = a->Next)
	{
		if (a->Name == sName)
		{
			return a;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
/// const overload of FindAttr().
inline const AttrPOD* FindAttr(const NodePOD* pNode, KStringView sName) noexcept
//-----------------------------------------------------------------------------
{
	for (const AttrPOD* a = pNode->FirstAttr; a != nullptr; a = a->Next)
	{
		if (a->Name == sName)
		{
			return a;
		}
	}

	return nullptr;
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

DEKAF2_ENUM_IS_FLAG(khtml::NodeFlag)

DEKAF2_NAMESPACE_END
