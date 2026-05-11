/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
//
// SPDX-License-Identifier: MIT
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

/// @file khtmldom_cursor.h
/// Lightweight handle (`khtml::NodeCursor` / `KHTMLNode`) over the
/// arena-backed POD tree of `KHTML`. The handle is the size of a single
/// pointer (8 byte) and trivially copyable; pass it by value.
///
/// Both read and write access go through the same handle type. The mutator
/// methods (SetAttribute, AddText, Delete, ...) allocate from the arena
/// reachable via the node's parent chain — see `khtml::NodePOD::Document()`.
///
/// A handle stays valid as long as the underlying KHTML instance lives and
/// is not `clear()`ed, reparsed, or moved. After `clear()` / `Parse()` /
/// move, every previously obtained handle must be considered invalidated.
///
/// `KHTMLNode` (in the public `dekaf2` namespace) is an alias for
/// `khtml::NodeCursor` and is the preferred name in user code. The
/// `khtml::NodeCursor` spelling stays as the implementation type and as the
/// historical name; both refer to the same class.
///
/// Typical use (read):
///
///   KHTML doc;
///   doc.Parse(sInput);
///
///   for (auto child : doc.Root().Children())
///   {
///       if (child.IsElement() && child.Name() == "html") { ... }
///   }
///
/// Typical use (write):
///
///   KHTML doc;
///   doc.Parse(sInput);
///
///   for (auto child : doc.Root().Children())
///   {
///       if (child.IsElement() && child.Name() == "img")
///       {
///           child.SetAttribute("loading", "lazy");
///       }
///   }
///
///   doc.Serialize(kOut);

#include "bits/khtmldom_node.h"
#include "khtmlparser.h"

#include <cstddef>
#include <iterator>

DEKAF2_NAMESPACE_BEGIN

namespace khtml {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Pointer-sized handle over a single attribute of a `NodePOD`. Both
/// read-access (Name, Value, ...) and write-access (SetValue, ...) go
/// through this class.
///
/// Dereferencing accessors on a default-constructed (= "end") cursor is
/// undefined behaviour; test the cursor with `if (c)` first.
class DEKAF2_PUBLIC AttrCursor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	AttrCursor() = default;
	explicit AttrCursor(AttrPOD* p) noexcept : m_p(p) {}

	explicit operator bool() const noexcept { return m_p != nullptr; }

	/// @returns raw arena pointer (or nullptr). Mainly for diagnostics
	///          and equivalence checks; prefer the named accessors.
	AttrPOD* Raw() const noexcept { return m_p; }

	KStringView Name    () const noexcept { return m_p->Name();     }
	KStringView Value   () const noexcept { return m_p->Value();    }
	/// @returns the original quote char as seen by the parser (' or "),
	///          or 0 if the parser saw no quote (the serializer will then
	///          pick a default).
	char        Quote   () const noexcept { return m_p->Quote();    }
	/// @returns true if the value still needs HTML-escaping on serialize,
	///          false if the value is already entity-encoded as-is.
	bool        DoEscape() const noexcept { return m_p->DoEscape(); }

	/// @returns the next attribute on the same node (or an empty cursor).
	AttrCursor  Next    () const noexcept { return AttrCursor{ m_p->Next() }; }

	/// Replace the attribute value. The new value is interned in the
	/// document's arena (or kept as a view if the bytes live in the data
	/// segment — see `KArenaAllocator::AllocateString`). The owning
	/// `NodePOD` must be reachable from a `Document` (otherwise this is
	/// a no-op).
	AttrCursor& SetValue(KStringView sValue);

	bool operator==(const AttrCursor& other) const noexcept { return m_p == other.m_p; }
	bool operator!=(const AttrCursor& other) const noexcept { return m_p != other.m_p; }

//------
private:
//------

	AttrPOD* m_p { nullptr };

}; // AttrCursor

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Pointer-sized handle over a single node of a `NodePOD` tree. Both
/// read-access (Kind, Name, FirstChild, ...) and write-access
/// (SetAttribute, AddText, Delete, ...) go through this class.
///
/// Dereferencing accessors (Kind(), Name(), FirstChild(), ...) on a
/// default-constructed cursor is undefined behaviour; test the cursor with
/// `if (c)` first.
///
/// Iteration-invalidation rule: when iterating `Children()` and removing
/// the current child, cache its `NextSibling()` *before* calling
/// `Delete()`. This handle is intentionally not an STL iterator; mutation
/// during traversal is the caller's responsibility.
class DEKAF2_PUBLIC NodeCursor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	NodeCursor() = default;
	explicit NodeCursor(NodePOD* p) noexcept : m_p(p) {}

	explicit operator bool() const noexcept { return m_p != nullptr; }

	/// @returns raw arena pointer (or nullptr). Mainly for diagnostics
	///          and equivalence checks; prefer the named accessors.
	NodePOD* Raw() const noexcept { return m_p; }

	NodeKind                 Kind    () const noexcept { return m_p->Kind();     }
	NodeFlag                 Flags   () const noexcept { return m_p->Flags();    }
	/// Cached HTML tag-property bits. Only meaningful for Element nodes.
	KHTMLObject::TagProperty TagProps() const noexcept { return m_p->TagProps(); }

	/// Primary (and only) payload — interpretation depends on Kind:
	///   Element                 : tag name
	///   Text / Comment / CData  : text content
	///   ProcessingInstruction   : entire PI body (target + body, opaque)
	///   DocumentType            : entire DOCTYPE body (opaque)
	KStringView Name() const noexcept { return m_p->Name(); }

	bool IsElement()               const noexcept { return m_p->Kind() == NodeKind::Element;               }
	bool IsText()                  const noexcept { return m_p->Kind() == NodeKind::Text;                  }
	bool IsComment()               const noexcept { return m_p->Kind() == NodeKind::Comment;               }
	bool IsCData()                 const noexcept { return m_p->Kind() == NodeKind::CData;                 }
	bool IsProcessingInstruction() const noexcept { return m_p->Kind() == NodeKind::ProcessingInstruction; }
	bool IsDocumentType()          const noexcept { return m_p->Kind() == NodeKind::DocumentType;          }

	/// True if this is a Text node whose payload must be emitted verbatim
	/// (e.g. inside <script> / <style>). Equivalent to checking
	/// `Flags() & NodeFlag::TextDoNotEscape`.
	bool IsTextRaw() const noexcept
	{
		return (m_p->Flags() & NodeFlag::TextDoNotEscape) != 0;
	}

	NodeCursor Parent()      const noexcept { return NodeCursor{ m_p->Parent()      }; }
	NodeCursor FirstChild()  const noexcept { return NodeCursor{ m_p->FirstChild()  }; }
	NodeCursor LastChild()   const noexcept { return NodeCursor{ m_p->LastChild()   }; }
	NodeCursor NextSibling() const noexcept { return NodeCursor{ m_p->NextSibling() }; }
	NodeCursor PrevSibling() const noexcept { return NodeCursor{ m_p->PrevSibling() }; }

	AttrCursor FirstAttr()   const noexcept { return AttrCursor{ m_p->FirstAttr()   }; }
	AttrCursor LastAttr()    const noexcept { return AttrCursor{ m_p->LastAttr()    }; }

	/// Find the first attribute whose Name() equals 'sName'. Linear search
	/// over the attribute chain (O(n) in number of attrs). Returns an empty
	/// cursor if no such attribute exists.
	AttrCursor FindAttr(KStringView sName) const noexcept
	{
		for (auto a = FirstAttr(); a; a = a.Next())
		{
			if (a.Name() == sName) return a;
		}
		return AttrCursor{};
	}

	/// Convenience: returns the value of the named attribute, or an empty
	/// view if no such attribute exists. Equivalent to
	/// `FindAttr(sName) ? FindAttr(sName).Value() : KStringView{}`.
	KStringView GetAttribute(KStringView sName) const noexcept
	{
		auto a = FindAttr(sName);
		return a ? a.Value() : KStringView{};
	}

	/// @returns true iff an attribute with the given name exists.
	bool HasAttribute(KStringView sName) const noexcept
	{
		return FindAttr(sName) ? true : false;
	}

	std::size_t CountChildren() const noexcept { return m_p->CountChildren(); }
	std::size_t CountAttrs()    const noexcept { return m_p->CountAttrs   (); }

	// -- Mutators -----------------------------------------------------------

	/// Replace the node name. For Element nodes this also recomputes the
	/// cached `TagProps`. The new name is interned in the document's arena
	/// (or kept as a view if the bytes live in the data segment).
	NodeCursor& SetName(KStringView sName);

	/// Set (or overwrite) an attribute on this element. Allocates the
	/// AttrPOD in the document's arena on first call; subsequent calls for
	/// the same name update the existing entry's value in place.
	/// @param sName     attribute name
	/// @param sValue    attribute value
	/// @param chQuote   quote char to use on emit (' or "), 0 = decide on
	///                  serialize. Builder default is `"` to match the
	///                  pre-Phase-4 heap-DOM output.
	/// @param bDoEscape false = sValue is already entity-encoded
	NodeCursor& SetAttribute(KStringView sName,
	                         KStringView sValue,
	                         char        chQuote   = '"',
	                         bool        bDoEscape = true);

	/// Remove the attribute with the given name. No-op if none exists.
	NodeCursor& RemoveAttribute(KStringView sName);

	/// Append a Text child to this node. Returns a cursor to the new child.
	/// @param sText       text content (copied into arena unless it is in
	///                    the data segment)
	/// @param bDoNotEscape if true, the content is already entity-encoded
	///                    (or must remain unescaped — like inside script)
	NodeCursor AddText(KStringView sText, bool bDoNotEscape = false);

	/// Append a Text child whose content is already entity-encoded (or
	/// must not be entity-encoded — like inside `<script>`/`<style>`).
	NodeCursor AddRawText(KStringView sText)
	{
		return AddText(sText, /*bDoNotEscape=*/true);
	}

	/// Append a new Element child with the given tag name. Returns a
	/// cursor to the new child.
	NodeCursor AddElement(KStringView sTagName)
	{
		return NodeCursor{ m_p->AddNode(NodeKind::Element, sTagName) };
	}

	/// Unlink this node from its parent and siblings. The arena keeps
	/// owning the memory — this node simply becomes unreachable from the
	/// tree. O(1). After Delete(), this handle is "orphaned": further
	/// mutator calls that need the arena are no-ops (Document() is no
	/// longer reachable).
	void Delete() noexcept { m_p->Detach(); }

	/// Construct a new child of type T as child of this node. T's
	/// constructor must take `KHTMLNode parent` as its first argument; the
	/// remaining `args...` are forwarded.
	///
	///   auto div = body.Add<html::Div>("FormDiv");
	///   form.Add<html::Input>(html::Input::TEXT, "user");
	///
	/// Returns the constructed `T` by value (T is a thin handle, usually
	/// 8 byte).
	template<class T, class... Args>
	T Add(Args&&... args)
	{
		return T(*this, std::forward<Args>(args)...);
	}

	bool operator==(const NodeCursor& other) const noexcept { return m_p == other.m_p; }
	bool operator!=(const NodeCursor& other) const noexcept { return m_p != other.m_p; }

	/// Range adapter for `for (auto child : node.Children())` iteration.
	class ChildRange;
	ChildRange Children() const noexcept;

	/// Range adapter for `for (auto attr : node.Attributes())` iteration.
	class AttrRange;
	AttrRange  Attributes() const noexcept;

//------
private:
//------

	NodePOD* m_p { nullptr };

}; // NodeCursor

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Forward-iterable range over the children of a NodeCursor. Walks
/// FirstChild...NextSibling and stops at nullptr.
class DEKAF2_PUBLIC NodeCursor::ChildRange
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	class iterator
	{
	public:

		using value_type        = NodeCursor;
		using difference_type   = std::ptrdiff_t;
		using iterator_category = std::forward_iterator_tag;
		using pointer           = const NodeCursor*;
		using reference         = NodeCursor;

		iterator() = default;
		explicit iterator(NodeCursor cur) noexcept : m_cur(cur) {}

		NodeCursor operator*()  const noexcept { return m_cur; }

		iterator& operator++()    noexcept { m_cur = m_cur.NextSibling(); return *this; }
		iterator  operator++(int) noexcept { auto t = *this; ++*this;     return t;     }

		bool operator==(const iterator& o) const noexcept { return m_cur.Raw() == o.m_cur.Raw(); }
		bool operator!=(const iterator& o) const noexcept { return m_cur.Raw() != o.m_cur.Raw(); }

	private:

		NodeCursor m_cur;
	};

	explicit ChildRange(NodeCursor first) noexcept : m_first(first) {}

	iterator begin() const noexcept { return iterator{ m_first       }; }
	iterator end()   const noexcept { return iterator{ NodeCursor{}  }; }

	bool empty() const noexcept { return !m_first; }

private:

	NodeCursor m_first;

}; // NodeCursor::ChildRange

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Forward-iterable range over the attributes of a NodeCursor. Walks
/// FirstAttr...Next and stops at nullptr.
class DEKAF2_PUBLIC NodeCursor::AttrRange
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	class iterator
	{
	public:
		using value_type        = AttrCursor;
		using difference_type   = std::ptrdiff_t;
		using iterator_category = std::forward_iterator_tag;
		using pointer           = const AttrCursor*;
		using reference         = AttrCursor;

		iterator() = default;
		explicit iterator(AttrCursor cur) noexcept : m_cur(cur) {}

		AttrCursor operator*()  const noexcept { return m_cur; }

		iterator& operator++()    noexcept { m_cur = m_cur.Next(); return *this; }
		iterator  operator++(int) noexcept { auto t = *this; ++*this; return t;  }

		bool operator==(const iterator& o) const noexcept { return m_cur.Raw() == o.m_cur.Raw(); }
		bool operator!=(const iterator& o) const noexcept { return m_cur.Raw() != o.m_cur.Raw(); }

	private:
		AttrCursor m_cur;
	};

	explicit AttrRange(AttrCursor first) noexcept : m_first(first) {}

	iterator begin() const noexcept { return iterator{ m_first       }; }
	iterator end()   const noexcept { return iterator{ AttrCursor{}  }; }

	bool empty() const noexcept { return !m_first; }

private:

	AttrCursor m_first;

}; // NodeCursor::AttrRange

inline NodeCursor::ChildRange NodeCursor::Children() const noexcept
{
	return ChildRange{ FirstChild() };
}

inline NodeCursor::AttrRange NodeCursor::Attributes() const noexcept
{
	return AttrRange{ FirstAttr() };
}

//-----------------------------------------------------------------------------
// Mutator implementations — inline because they only do small POD
// arithmetic plus delegate into NodePOD/AttrPOD/Document.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
inline AttrCursor& AttrCursor::SetValue(KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (m_p != nullptr)
	{
		// AttrPOD itself has no parent pointer; arena allocation is the
		// caller's job. We just install the view. Stable inputs (data
		// segment literals, slices of an already-arena-owned string,
		// or a value the caller is about to keep alive) work directly.
		// For arena-managed copies prefer NodeCursor::SetAttribute(name,
		// value) which knows the owning node and goes through the
		// Document's AllocateString.
		m_p->Value(sValue);
	}

	return *this;
}

//-----------------------------------------------------------------------------
inline NodeCursor& NodeCursor::SetName(KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_p == nullptr) return *this;

	auto* pDoc = m_p->Document();

	if (pDoc != nullptr)
	{
		m_p->Name(pDoc->AllocateString(sName));
	}
	else
	{
		// orphan subtree — caller-managed lifetime
		m_p->Name(sName);
	}

	return *this;
}

//-----------------------------------------------------------------------------
inline NodeCursor& NodeCursor::SetAttribute(KStringView sName,
                                            KStringView sValue,
                                            char        chQuote,
                                            bool        bDoEscape)
//-----------------------------------------------------------------------------
{
	if (m_p == nullptr) return *this;

	auto* pExisting = m_p->Attribute(sName);

	if (pExisting != nullptr)
	{
		auto* pDoc = m_p->Document();

		if (pDoc != nullptr)
		{
			pExisting->Value(pDoc->AllocateString(sValue));
		}
		else
		{
			pExisting->Value(sValue);
		}

		// quote / escape only updated on create, not on update — caller
		// generally expects in-place value mutation to keep formatting
	}
	else
	{
		m_p->AddAttribute(sName, sValue, chQuote, bDoEscape);
	}

	return *this;
}

//-----------------------------------------------------------------------------
inline NodeCursor& NodeCursor::RemoveAttribute(KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_p != nullptr) m_p->RemoveAttribute(sName);
	return *this;
}

//-----------------------------------------------------------------------------
inline NodeCursor NodeCursor::AddText(KStringView sText, bool bDoNotEscape)
//-----------------------------------------------------------------------------
{
	if (m_p == nullptr) return NodeCursor{};

	return NodeCursor{ m_p->AddText(sText, bDoNotEscape) };
}

} // namespace khtml

//-----------------------------------------------------------------------------
/// Public alias for the schreibfähige Cursor-Klasse. Use `KHTMLNode` in
/// new code; `khtml::NodeCursor` remains as the implementation name.
using KHTMLNode = khtml::NodeCursor;
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END
