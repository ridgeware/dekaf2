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

	/// if the HTML is unbalanced (forgotten close tag), for how many levels
	/// down should the resynchronisation be tried (default = 2)
	KHTML& SetMaxAutoCloseLevels(std::size_t iMaxLevels) { m_iMaxAutoCloseLevels = iMaxLevels; return *this; }

	// SetThrowOnError is in base class
	// parsing is in base class

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

	KString                       m_sContent;
	std::vector<KString>          m_Issues;
	std::size_t                   m_iMaxAutoCloseLevels { 2 };
	bool                          m_bLastWasSpace { true  };
	bool                          m_bDoNotEscape  { false };
	uint8_t                       m_bPreformatted { 0     };

}; // KHTML


/// @}

DEKAF2_NAMESPACE_END
