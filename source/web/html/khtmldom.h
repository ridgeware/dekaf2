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
/// KHTML: an HTML document, parsed into a node tree. Access the tree
/// through `KHTMLNode` handles starting at `KHTML::Root()`; build new
/// content with `parent.Add<T>(...)` (see
/// `notes/kwebobjects-migration-guide.md`).

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/web/html/khtmlparser.h>
#include <dekaf2/web/html/bits/khtmldom_node.h>
#include <dekaf2/web/html/bits/khtmlparse_accumulator.h>
#include <dekaf2/web/html/khtmldom_cursor.h>
#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/errors/kerror.h>
#include <memory>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup web_html
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KHTML parses HTML into a document tree which can be traversed,
/// modified, and serialized back to HTML.
///
/// @code
/// KHTML Doc;
/// Doc.Parse(KInFile("index.html"));        // or any string type
///
/// for (auto Node : Doc.Root().Children())  // walk the top-level nodes
/// {
///     if (Node.IsElement() && Node.Name() == "html") { /* ... */ }
/// }
///
/// Doc.Add<html::Paragraph>();              // add content (see kwebobjects.h)
/// Doc.Serialize(KOut);                     // render back to HTML
/// @endcode
///
/// While parsing, tag and attribute names are lowercased, entities are
/// decoded, and whitespace in non-preformatted text is collapsed.
/// Parse problems (e.g. unbalanced tags) do not stop the parse — they
/// are recovered from and reported through Issues().
///
/// ### Choosing a Parse method
///
/// All overloads build the same tree. They differ in who owns the input
/// bytes afterwards:
///
/// | Call                | The input is    | The caller must                              |
/// |---------------------|-----------------|----------------------------------------------|
/// | `Parse(KString)`    | moved/copied in | nothing — the document owns it               |
/// | `Parse(KInStream&)` | streamed        | nothing                                      |
/// | `ParseStable(view)` | referenced      | keep the bytes alive as long as the document |
///
/// `Parse(std::move(sHtml))` is the fastest way to parse a string you no
/// longer need. `ParseStable(sView)` is equally fast and leaves
/// ownership with the caller. Passing a `KStringView` / `const char*`
/// to `Parse()` is always safe: those bytes are copied while parsing.
///
/// ### How strings are stored (implementation note)
///
/// Nodes and strings live in a single arena owned by the document and
/// are released all at once (or recycled, see clear()). When the source
/// buffer is known to stay alive — the `Parse(KString)` and
/// `ParseStable()` paths — most strings are not even copied into the
/// arena: a tag name, attribute name/value, or text run that is
/// byte-identical to the source is stored as a view into the source
/// buffer. Only a string that has to differ from its source spelling
/// gets copied into the arena.
///
/// This decision falls per string, and every string starts out as a
/// view. Example — parsing `<p title="a&amp;b">Hello world</p>`:
///  - `p`, `title` and `Hello world` match their source bytes and stay
///    views into the source — no memory consumed
///  - `a&b` differs from its source spelling `a&amp;b`, so this one
///    string is copied into the arena; the strings before and after it
///    are unaffected and remain views
///
/// The same holds for a tag name that had to be lowercased or a text
/// run with collapsed whitespace: only the affected string is copied.
/// With `Parse(KInStream&)` or a plain view there is no lifetime
/// guarantee on the source, so every string goes into the arena.
///
/// ### Copy / move
///
/// KHTML is non-copyable. Moving a *populated* document is undefined
/// behaviour (see khtml::Document); returning a local by value is safe
/// (NRVO):
/// @code
/// KHTML Make() { KHTML Doc; Doc.Parse("<p>hi</p>"); return Doc; }
/// @endcode
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
	/// Construct and immediately parse from a source string. Same
	/// semantics as `Parse(KString)`: `KHTML(std::move(sBuffer))` hands the
	/// buffer to the document without copying, `KHTML(sBuffer)` copies it.
	KHTML(KString sSource);
	//-----------------------------------------------------------------------------

	/// if the HTML is unbalanced (forgotten close tag), for how many levels
	/// down should the resynchronisation be tried (default = 2)
	KHTML& SetMaxAutoCloseLevels(std::size_t iMaxLevels) { m_iMaxAutoCloseLevels = iMaxLevels; return *this; }

	// SetThrowOnError is in base class
	// parsing is in base class

	//-----------------------------------------------------------------------------
	/// Parse a source string that the document takes ownership of:
	/// `Parse(std::move(sBuffer))` moves the buffer in without copying,
	/// `Parse(sBuffer)` copies it. The document keeps the buffer alive so
	/// that parsed strings can be stored as views into it (see the class
	/// description). Clears any previous content first.
	/// @returns true on success
	bool Parse(KString sSource);

	/// Parse from an input stream (file, socket, ...). Clears any
	/// previous content first.
	/// @returns true on success
	bool Parse(KInStream& InStream) override;

	// Bring in the base class string template. Overload resolution:
	// KString arguments pick the concrete Parse(KString) above, while
	// KStringView / const char* arguments pick the base template (exact
	// reference binding beats the user-defined conversion to KString)
	// and are therefore copied during the parse, not into the document.
	using KHTMLParser::Parse;

	//-----------------------------------------------------------------------------
	/// Parse a view whose bytes the caller **guarantees** stay alive and
	/// unmoved at least as long as this document — typically a view into
	/// a `KString` the caller already holds. Parsed strings can then be
	/// stored as views into the caller's bytes instead of being copied
	/// (see the class description).
	///
	/// If the underlying bytes go away or move before the document does,
	/// the document's strings dangle. When in doubt use
	/// `Parse(KString{sView})` — it copies, but is always safe.
	/// @returns true on success
	bool ParseStable(KStringView sView);
	//-----------------------------------------------------------------------------

	/// Render the POD tree back to HTML into a stream / string.
	/// `chIndent` is the per-level indent character (default tab); pass
	/// `'\0'` for unindented compact output. Output is byte-for-byte
	/// deterministic regardless of which Parse path produced the tree.
	void    Serialize(KOutStream& Stream, char chIndent = '\t') const;
	void    Serialize(KStringRef& sOut, char chIndent = '\t') const;
	KString Serialize(char chIndent = '\t') const;

	/// Clear all content. Arena blocks return to the internal free list
	/// (not released to the heap) so the next `Parse()` recycles them
	/// without further `malloc()` calls. Drops any registered stable
	/// regions and the `m_SourceBuffer`.
	void clear();

	/// No content?
	bool empty() const
	{
		auto* root = PodRoot();
		return root == nullptr || root->FirstChild() == nullptr;
	}

	/// true if we have content
	operator bool() const { return !empty(); }

	/// Per-parse diagnostics (unbalanced tags, recovered standalone
	/// tags, etc.). The list is appended to during the parse; valid for
	/// the document's lifetime, cleared on `clear()` / re-parse.
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

	/// Construct a child of the synthetic document root. Equivalent to
	/// `Root().Add<T>(args...)`. `T` must accept a `KHTMLNode` (the
	/// parent handle) as its first ctor argument; the rest is
	/// forwarded. See `khtml::NodeCursor::Add` for the CTAD-friendly
	/// `Add<TemplateName>(args...)` shorthand.
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

	// Owned copy of the parser input — held for the document's lifetime
	// so that the source-slicing accumulator (see
	// `khtml::ParseAccumulator`) can hand out `KStringView`s into it.
	// Registered with `m_Document` as a stable region on each
	// `Parse(KString)`; freed by `clear()` and on re-parse.
	KString                       m_SourceBuffer;

	// Text-content accumulator (tri-mode: SourceSlice when the parser
	// is reading from a stable buffer with no transformation needed,
	// ArenaBuilder otherwise, HeapKString when no arena is set).
	// Re-armed on the first Content()/Script() call after a tag
	// boundary; finalised on every FlushText().
	KString                       m_sContentOwned;
	KStringView                   m_sContent;
	khtml::ParseAccumulator       m_ContentAcc;
	bool                          m_bContentArmed { false };

	std::vector<KString>          m_Issues;
	std::size_t                   m_iMaxAutoCloseLevels { 2 };
	bool                          m_bLastWasSpace { true  };
	bool                          m_bDoNotEscape  { false };
	uint8_t                       m_bPreformatted { 0     };

}; // KHTML


/// @}

DEKAF2_NAMESPACE_END
