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
/// Read-only cursor API over the arena-backed POD shadow tree of KHTML.
/// Allows callers to walk the parsed HTML structure without ever touching
/// the heap-DOM (KHTMLElement / KHTMLText / ...) and without allocating
/// their own copies of names, values, or children.
///
/// A cursor is the size of a single pointer and is cheap to copy. It stays
/// valid as long as the underlying KHTML instance lives and is not
/// clear()ed or reparsed. After clear() / Parse() / move, every previously
/// obtained cursor must be considered invalidated.
///
/// Typical use (mirrors xapis/html.cpp::HTMLProcessor::TraverseHTML):
///
///   KHTML doc;
///   doc.Parse(sInput);
///
///   for (auto child : doc.Root().Children())
///   {
///       if (child.IsElement() && child.Name() == "html") { ... }
///   }
///
/// The API intentionally exposes only read access. Mutation of the parsed
/// tree still goes through the heap-DOM via KHTML::DOM(), which lazily
/// materializes the heap subtree on demand.

#include "bits/khtmldom_node.h"
#include "khtmlparser.h"

#include <cstddef>
#include <iterator>

DEKAF2_NAMESPACE_BEGIN

namespace khtml {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Read-only cursor over a single attribute in the POD shadow tree.
/// Pointer-sized; copy is trivial. Dereferencing accessors (Name(), Value(),
/// ...) on a default-constructed (= "end") cursor is undefined behaviour;
/// always test the cursor with `if (c)` first.
class DEKAF2_PUBLIC AttrCursor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	AttrCursor() = default;
	explicit AttrCursor(const AttrPOD* p) noexcept : m_p(p) {}

	explicit operator bool() const noexcept { return m_p != nullptr; }

	/// @returns raw arena pointer (or nullptr). Mainly for diagnostics
	///          and equivalence checks; prefer the named accessors.
	const AttrPOD* Raw     () const noexcept { return m_p;           }

	KStringView    Name    () const noexcept { return m_p->Name;     }
	KStringView    Value   () const noexcept { return m_p->Value;    }
	/// @returns the original quote char as seen by the parser (' or "),
	///          or 0 if the parser saw no quote (the serializer will then
	///          pick a default).
	char           Quote   () const noexcept { return m_p->Quote;    }
	/// @returns true if the value still needs HTML-escaping on serialize,
	///          false if the value is already entity-encoded as-is.
	bool           DoEscape() const noexcept { return m_p->DoEscape; }

	/// @returns the next attribute on the same node (or an empty cursor).
	AttrCursor     Next    () const noexcept { return AttrCursor{ m_p->Next }; }

	bool operator==(const AttrCursor& other) const noexcept { return m_p == other.m_p; }
	bool operator!=(const AttrCursor& other) const noexcept { return m_p != other.m_p; }

private:

	const AttrPOD* m_p { nullptr };

}; // AttrCursor

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Read-only cursor over a single node in the POD shadow tree.
/// Pointer-sized; copy is trivial. Dereferencing accessors (Kind(), Name(),
/// FirstChild(), ...) on a default-constructed cursor is undefined
/// behaviour; always test the cursor with `if (c)` first.
class DEKAF2_PUBLIC NodeCursor
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
public:

	NodeCursor() = default;
	explicit NodeCursor(const NodePOD* p) noexcept : m_p(p) {}

	explicit operator bool() const noexcept { return m_p != nullptr; }

	/// @returns raw arena pointer (or nullptr). Mainly for diagnostics
	///          and equivalence checks; prefer the named accessors.
	const NodePOD* Raw() const noexcept { return m_p; }

	NodeKind                 Kind    () const noexcept { return m_p->Kind;     }
	NodeFlag                 Flags   () const noexcept { return m_p->Flags;    }
	/// Cached HTML tag-property bits. Only meaningful for Element nodes.
	KHTMLObject::TagProperty TagProps() const noexcept { return m_p->TagProps; }

	/// Primary payload — interpretation depends on Kind:
	///   Element                 : tag name
	///   Text / Comment / CData  : text content
	///   ProcessingInstruction   : target ("xml")
	///   DocumentType            : doctype declaration
	KStringView Name () const noexcept { return m_p->Name;  }
	/// Secondary payload — currently only set for ProcessingInstruction
	/// (instruction body); empty for every other Kind.
	KStringView Value() const noexcept { return m_p->Value; }

	bool IsElement()               const noexcept { return m_p->Kind == NodeKind::Element;               }
	bool IsText()                  const noexcept { return m_p->Kind == NodeKind::Text;                  }
	bool IsComment()               const noexcept { return m_p->Kind == NodeKind::Comment;               }
	bool IsCData()                 const noexcept { return m_p->Kind == NodeKind::CData;                 }
	bool IsProcessingInstruction() const noexcept { return m_p->Kind == NodeKind::ProcessingInstruction; }
	bool IsDocumentType()          const noexcept { return m_p->Kind == NodeKind::DocumentType;          }

	/// True if this is a Text node whose payload must be emitted verbatim
	/// (e.g. inside <script> / <style>). Equivalent to checking
	/// `Flags() & NodeFlag::TextDoNotEscape`.
	bool IsTextRaw() const noexcept
	{
		return (m_p->Flags & NodeFlag::TextDoNotEscape) != 0;
	}

	NodeCursor Parent()      const noexcept { return NodeCursor{ m_p->Parent      }; }
	NodeCursor FirstChild()  const noexcept { return NodeCursor{ m_p->FirstChild  }; }
	NodeCursor LastChild()   const noexcept { return NodeCursor{ m_p->LastChild   }; }
	NodeCursor NextSibling() const noexcept { return NodeCursor{ m_p->NextSibling }; }
	NodeCursor PrevSibling() const noexcept { return NodeCursor{ m_p->PrevSibling }; }

	AttrCursor FirstAttr()   const noexcept { return AttrCursor{ m_p->FirstAttr   }; }
	AttrCursor LastAttr()    const noexcept { return AttrCursor{ m_p->LastAttr    }; }

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

	std::size_t CountChildren() const noexcept { return ::dekaf2::khtml::CountChildren(m_p); }
	std::size_t CountAttrs()    const noexcept { return ::dekaf2::khtml::CountAttrs   (m_p); }

	/// Range adapter for `for (auto child : node.Children())` iteration.
	class ChildRange;
	ChildRange Children() const noexcept;

	/// Range adapter for `for (auto attr : node.Attributes())` iteration.
	class AttrRange;
	AttrRange  Attributes() const noexcept;

	bool operator==(const NodeCursor& other) const noexcept { return m_p == other.m_p; }
	bool operator!=(const NodeCursor& other) const noexcept { return m_p != other.m_p; }

private:

	const NodePOD* m_p { nullptr };

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

} // namespace khtml

DEKAF2_NAMESPACE_END
