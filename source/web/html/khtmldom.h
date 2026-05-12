/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2021, Ridgeware, Inc.
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


/// @file khtmldom.h
/// KHTML: arena-backed HTML document. Parser writes directly into the POD
/// shadow tree; builder access is through `KHTMLNode` handles obtained
/// from `KHTML::Root()` and via the parent-first `parent.Add<T>(...)`
/// idiom (see `notes/kwebobjects-migration-guide.md`).

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/web/html/khtmlparser.h>
#include <dekaf2/web/html/bits/khtmldom_node.h>
#include <dekaf2/web/html/khtmldom_cursor.h>
#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/errors/kerror.h>
#include <memory>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup web_html
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Parses HTML into an arena-backed POD tree and owns it. The public
/// surface is `Parse(...)`, `Root()`, `Serialize(...)`.
class DEKAF2_PUBLIC KHTML : public KErrorBase, public KHTMLParser
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	KHTML();
	// non-copyable: KHTML owns a KArenaAllocator (which is non-copyable)
	// and cloning a populated arena would be expensive and rarely useful.
	//
	// Move: implicitly defined (since Document has a user-defined move
	// ctor and a deleted move-assign). The move ctor on a populated KHTML
	// is UB — see Document's class-level comment. NRVO covers the
	// return-by-value pattern, which is the safe common case.
	KHTML(const KHTML&) = delete;
	KHTML& operator=(const KHTML&) = delete;
	KHTML(KHTML&&) = default;
	// no move-assign: Document has none either
	virtual ~KHTML();

	//-----------------------------------------------------------------------------
	/// Construct and immediately parse from a source string. Takes the
	/// source by value — the caller chooses ownership: `KHTML(std::move(buf))`
	/// hands the bytes to the document (zero copies; the buffer becomes
	/// `m_SourceBuffer` and is registered with the arena as a stable
	/// region for views to point into). `KHTML(buf)` copies at the
	/// call-site, leaving the caller's buffer untouched.
	KHTML(KString sSource);
	//-----------------------------------------------------------------------------

	/// if the HTML is unbalanced (forgotten close tag), for how many levels
	/// down should the resynchronisation be tried (default = 2)
	KHTML& SetMaxAutoCloseLevels(std::size_t iMaxLevels) { m_iMaxAutoCloseLevels = iMaxLevels; return *this; }

	// SetThrowOnError is in base class
	// parsing is in base class

	//-----------------------------------------------------------------------------
	/// Parse a source string with the same ownership semantics as the
	/// `KHTML(KString)` constructor: takes the source by value, so
	/// `Parse(std::move(buf))` moves the buffer into the document (zero
	/// copies); `Parse(buf)` copies. The buffer is held as
	/// `m_SourceBuffer` and registered as a stable arena region.
	///
	/// Anything implicitly convertible to `KString` (`KStringView`,
	/// `KStringViewZ`, `const char*`, ...) reaches this overload — there
	/// is no separate `Parse(KStringView)` entry. We intentionally do
	/// not pull in `KHTMLParser::Parse` via a using-declaration, since
	/// that would re-expose the base's `Parse(KStringView)` as an exact
	/// match and bypass `m_SourceBuffer` ownership. The diagnostic
	/// guard below silences `-Woverloaded-virtual`, which fires on the
	/// intentional hiding.
	///
	/// Clears any previous parse result first. Returns true on success.
#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif
	bool Parse(KString sSource);

	// Stream input clears m_SourceBuffer and runs the streaming path
	// unchanged.
	bool Parse(KInStream& InStream) override;
#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic pop
#endif
	//-----------------------------------------------------------------------------

	void    Serialize(KOutStream& Stream, char chIndent = '\t') const;
	void    Serialize(KStringRef& sOut, char chIndent = '\t') const;
	KString Serialize(char chIndent = '\t') const;

	/// Clear all content (returns blocks to internal free list; the next
	/// Parse() reuses them).
	void clear();

	/// No content?
	bool empty() const
	{
		auto* root = PodRoot();
		return root == nullptr || root->FirstChild() == nullptr;
	}

	/// true if we have content
	operator bool() const { return !empty(); }

	const std::vector<KString>& Issues() const { return m_Issues; }

	/// Read/write handle at the synthetic root of the POD tree. The handle
	/// itself wraps a `NodeKind::Document` root; iterate `Children()` to
	/// walk the top-level nodes.
	KHTMLNode Root() noexcept
	{
		return KHTMLNode(PodRoot());
	}

	/// const overload (cast-away-const cursor; matches the rapidxml / KXML
	/// convention — the caller is responsible for not mutating through it).
	KHTMLNode Root() const noexcept
	{
		return KHTMLNode(const_cast<khtml::NodePOD*>(PodRoot()));
	}

	/// Add an element to the document root. Equivalent to
	/// `Root().Add<T>(args...)`.
	template<class T, class... Args>
	T Add(Args&&... args)
	{
		return Root().Add<T>(std::forward<Args>(args)...);
	}

//------
protected:
//------

	virtual void Object(KHTMLObject& Object) override;
	virtual void Content(char ch) override;
	virtual void Script(char ch) override;
	virtual void Invalid(char ch) override;
	virtual void Finished() override;

	void FlushText();
	void SetIssue(KString sIssue);

	// POD shadow-tree mirror helpers. Each helper allocates POD
	// nodes from m_Arena and appends them under m_PodHierarchy.back().
	// PodResetTree() is called from the ctor and clear() to (re-)initialise
	// the tree.
	void PodResetTree();
	khtml::NodePOD* PodAddElement(const KHTMLTag& Tag);
	void PodAddStringNode(khtml::NodeKind kind, const KHTMLStringObject& Object);
	void PodAddTextLeaf(KStringView sContent, bool bDoNotEscape);

//------
public:
//------

	/// @returns the root of the arena-backed POD tree, or nullptr if no
	/// parsing has happened yet.
	khtml::NodePOD*       PodRoot()       { return m_Document.FirstChild(); }
	const khtml::NodePOD* PodRoot() const { return m_Document.FirstChild(); }

	/// @returns the arena/document.
	const khtml::Document& Arena() const { return m_Document; }
	khtml::Document&       Arena()       { return m_Document; }

//------
private:
//------

	// arena-backed POD tree (the ground truth)
	khtml::Document               m_Document;
	std::vector<khtml::NodePOD*>  m_PodHierarchy;

	// Owned copy of the parser input. Held for the lifetime of this
	// document so that arena-allocated strings (Phase 5 in-situ) can
	// safely point into it. Registered with `m_Document` as a stable
	// region on each Parse(); freed by clear() and on re-parse.
	KString                       m_SourceBuffer;

	// Step-4: text-content accumulator now writes straight into the
	// document arena via KArenaStringBuilder. Held as a value member —
	// default-constructed inactive. `Start()`-ed on the first
	// Content()/Script() call after a tag boundary; `Finalize()`-d on
	// every FlushText() (which is invoked at every tag boundary).
	KArenaStringBuilder           m_ContentBuilder;

	std::vector<KString>          m_Issues;
	std::size_t                   m_iMaxAutoCloseLevels { 2 };
	bool                          m_bLastWasSpace { true  };
	bool                          m_bDoNotEscape  { false };
	uint8_t                       m_bPreformatted { 0     };

}; // KHTML


/// @}

DEKAF2_NAMESPACE_END
