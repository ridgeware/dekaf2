/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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
//
*/

#pragma once

/// @file kdiff.h
/// provides text differencing (diff) between two strings, with output in
/// unified diff, plain text markup, or HTML markup format. Based on the
/// diff-match-patch algorithm by Neil Fraser (Google).

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "bits/kunique_deleter.h"
#include <list>

#define DEKAF2_KDIFF_USE_WSTRING
#ifndef DEKAF2_KDIFF_USE_WSTRING
// #define DEKAF2_KDIFF_USE_KSTRING
#endif
#ifndef DEKAF2_KDIFF_USE_KSTRING
  #include <string>
#endif

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup util_text
/// @{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Computes and formats the difference between two strings.
///
/// Internally uses the diff-match-patch algorithm (Neil Fraser / Google) to
/// find the minimal set of insertions, deletions, and equal runs, then
/// exposes the result in several output formats:
/// - unified diff (patch compatible)
/// - plain text markup with `[+inserted]` / `[-deleted]` brackets
/// - HTML markup with configurable `<ins>` / `<del>` tags
///
/// Supports character-level, line-level, and combined line-then-character
/// differencing modes. Post-processing ("sanitation") cleans up the raw diff
/// for human readability or patch efficiency.
///
/// UTF-8 input is fully supported; internally the diff operates on wide
/// strings to correctly handle multi-byte codepoints.
///
/// Usage:
/// @code
/// KDiff differ("old text", "new text");
/// KString html = differ.GetHTMLDiff();        // single merged output
/// KString text = differ.GetTextDiff();         // [+...] / [-...] markup
/// KString patch = differ.GetUnifiedDiff();     // unified diff / patch format
/// auto dist = differ.GetLevenshteinDistance(); // edit distance
/// @endcode
class DEKAF2_PUBLIC KDiff
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

#if defined(DEKAF2_KDIFF_USE_WSTRING)
	using string_t     = std::wstring;
	using stringview_t = sv::wstring_view;
#elif defined(DEKAF2_KDIFF_USE_KSTRING)
	using string_t     = KString;
	using stringview_t = KStringView;
#else
	using string_t     = std::string;
	using stringview_t = sv::string_view;
#endif

	/// Controls the granularity of the diff comparison
	enum DiffMode
	{
		Character         = 0, ///< compare individual characters (most precise, slowest for large texts)
		Word              = 1, ///< compare whole words (not yet implemented)
		LineThenCharacter = 2, ///< first diff by lines, then refine changed lines by characters (good speed/quality tradeoff)
		Line              = 3  ///< compare whole lines only, like GNU diff (not yet functional)
	};

	/// Controls post-processing cleanup of the raw diff result
	enum Sanitation
	{
		Semantic,   ///< merge adjacent diffs at word boundaries for best human readability
		Lossless,   ///< like Semantic but never changes the meaning of a diff - more fragmented, still readable
		Efficiency  ///< minimize the number of edit regions for compact patches - not well readable by humans
	};

	/// The type of change represented by a single Diff element
	enum Operation : uint16_t
	{
		Equal  = 0, ///< text is unchanged between old and new
		Insert = 1, ///< text was added in the new version
		Delete = 2  ///< text was removed from the old version
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// A single diff element: a text fragment together with its Operation
	/// (Equal, Insert, or Delete). A complete diff result is a sequence of
	/// these elements that, when concatenated, reproduce either the old or
	/// the new text depending on which operations are applied.
	struct Diff
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// construct from a wide string and an operation
		Diff(std::wstring sText, Operation op);
		/// construct from a std::string and an operation
		Diff(std::string sText, Operation op);
		/// construct from a KString and an operation
		Diff(KString sText, Operation op);
		/// returns the text fragment of this diff element as a KString
#if defined(DEKAF2_KDIFF_USE_WSTRING)
		KString            GetText() const;
#else
		const string_t&    GetText() const { return m_sText; }
#endif
		/// replaces the text fragment of this diff element
#if defined(DEKAF2_KDIFF_USE_WSTRING)
		void SetText(const KString& sText);
#elif defined(DEKAF2_KDIFF_USE_KSTRING)
		void SetText(KString sText)        { m_sText = std::move(sText);    }
#else
		void SetText(const KString& sText) { m_sText = sText.ToStdString(); }
#endif
		/// returns the Operation type of this diff element (Equal, Insert, or Delete)
		Operation GetOperation()     const { return m_operation;            }

		/// implicit conversion to the text fragment (KString)
#if defined(DEKAF2_KDIFF_USE_WSTRING)
		operator KString()           const { return GetText();              }
#else
		operator const string_t&()   const { return GetText();              }
#endif
		/// implicit conversion to the Operation type
		operator Operation()         const { return GetOperation();         }

	//----------
	private:
	//----------

		Operation    m_operation;
		string_t     m_sText;

	}; // Diff

	/// ordered list of Diff elements that together describe the transformation from old to new text
	using Diffs = std::list<Diff>;

	/// default ctor
	KDiff() = default;

	/// construct with source and target strings, and mode settings
	/// @param sOldText the original string
	/// @param sNewText the modified string
	/// @param Mode the diff compare mode: Character, Word, or Line, defaults to Character
	/// @param San the sanitation mode: Semantic, Lossless, or Efficiency, defaults to Semantic
	KDiff(KStringView sOldText,
	      KStringView sNewText,
	      DiffMode Mode = DiffMode::Character,
	      Sanitation San = Sanitation::Semantic)
	{
		CreateDiff(sOldText, sNewText, Mode, San);
	}

	/// create the difference between two strings
	/// @param sOldText the original string
	/// @param sNewText the modified string
	/// @param Mode the diff compare mode: Character, Word, or Line, defaults to Character
	/// @param San the sanitation mode: Semantic, Lossless, or Efficiency, defaults to Semantic
	void CreateDiff(KStringView sOldText,
	                KStringView sNewText,
	                DiffMode Mode = DiffMode::Character,
	                Sanitation San = Sanitation::Semantic);

	/// returns a reference to the list of all Diff elements computed by CreateDiff().
	/// Each element contains a text fragment and its associated Operation (Equal, Insert, Delete).
	/// Returns an empty list if no diff has been computed yet.
	Diffs& GetDiffs();

	/// returns the diff in unified diff format (compatible with GNU patch).
	/// The output uses URL-encoded newlines (%0A) in the header context.
	/// @return the unified diff as a KString, or an empty string if no diff was computed
	KString GetUnifiedDiff();

	/// returns the diff as a single merged string with plain text markup.
	/// Inserted fragments are wrapped as `[+text]`, deleted fragments as `[-text]`,
	/// and equal fragments appear unmodified.
	/// @return the marked-up text, or an empty string if no diff was computed
	KString GetTextDiff();

	/// returns the diff as two separate strings with plain text markup,
	/// one showing only deletions, the other only insertions.
	/// Deleted fragments are wrapped as `[-text]` in sOldText,
	/// inserted fragments as `[+text]` in sNewText.
	/// @param sOldText receives the original text with `[-deleted]` markup
	/// @param sNewText receives the modified text with `[+inserted]` markup
	/// @return count of diff fragments (Insert + Delete), 0 if texts are equal
	std::size_t GetTextDiff (KStringRef& sOldText, KStringRef& sNewText);

	/// returns the diff as a single merged HTML string.
	/// Inserted text is wrapped in `<ins>...</ins>`, deleted text in `<del>...</del>`,
	/// equal text appears unmodified. The tag names are configurable.
	/// @param sInsertTag HTML tag name for inserted fragments (default = "ins")
	/// @param sDeleteTag HTML tag name for deleted fragments (default = "del")
	/// @return the HTML-marked-up text, or an empty string if no diff was computed
	KString GetHTMLDiff(KStringView sInsertTag = "ins", KStringView sDeleteTag = "del");

	/// returns the diff as two separate HTML strings,
	/// one showing only deletions, the other only insertions.
	/// @param sOldText receives the original text with `<del>` markup around deleted fragments
	/// @param sNewText receives the modified text with `<ins>` markup around inserted fragments
	/// @param sInsertTag HTML tag name for inserted fragments (default = "ins")
	/// @param sDeleteTag HTML tag name for deleted fragments (default = "del")
	/// @return count of diff fragments (Insert + Delete), 0 if texts are equal
	std::size_t GetHTMLDiff (KStringRef& sOldText, KStringRef& sNewText,
	                         KStringView sInsertTag = "ins", KStringView sDeleteTag = "del");

	/// returns the Levenshtein edit distance derived from the diff.
	/// This is the total count of characters that were inserted or deleted
	/// (equal fragments do not count). Useful as a numeric similarity measure.
	/// @return edit distance in characters, 0 if texts are identical
	std::size_t GetLevenshteinDistance();

	/// returns the Levenshtein distance as a percentage of the longer input string.
	/// 0 means the texts are identical, 100 means completely different.
	/// @return percentage distance (0..100)
	uint16_t GetPercentDistance();

//----------
private:
//----------

	KUniqueVoidPtr m_Diffs;
	string_t::size_type m_iMaxSize { 0 };

}; // KDiff

/// convenience function: compute a diff and return it as a single merged HTML string.
/// Equivalent to constructing a KDiff and calling GetHTMLDiff().
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param sInsertTag HTML tag name for inserted fragments (default = "ins")
/// @param sDeleteTag HTML tag name for deleted fragments (default = "del")
/// @param Mode the diff granularity: Character, Word, LineThenCharacter, or Line
/// @param San the cleanup mode: Semantic, Lossless, or Efficiency
/// @return a single string with `<ins>` and `<del>` markup around changed fragments
DEKAF2_PUBLIC
KString KDiffToHTML (KStringView sOldText, KStringView sNewText,
                     KStringView sInsertTag="ins", KStringView sDeleteTag="del",
                     KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
                     KDiff::Sanitation San = KDiff::Sanitation::Semantic);

/// convenience function: compute a diff and write HTML markup into both strings in place.
/// sOldText receives `<del>` markup around deleted fragments,
/// sNewText receives `<ins>` markup around inserted fragments.
/// @param sOldText the original string (modified in place with deletion markup)
/// @param sNewText the modified string (modified in place with insertion markup)
/// @param sInsertTag HTML tag name for inserted fragments (default = "ins")
/// @param sDeleteTag HTML tag name for deleted fragments (default = "del")
/// @param Mode the diff granularity: Character, Word, LineThenCharacter, or Line
/// @param San the cleanup mode: Semantic, Lossless, or Efficiency
/// @return count of diff fragments (Insert + Delete), 0 if texts are equal
DEKAF2_PUBLIC
std::size_t KDiffToHTML2 (KStringRef& sOldText, KStringRef& sNewText,
                          KStringView sInsertTag="ins", KStringView sDeleteTag="del",
                          KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
                          KDiff::Sanitation San = KDiff::Sanitation::Semantic);

/// convenience function: compute a diff and return it as a single merged string
/// with plain text markup (`[+inserted]` / `[-deleted]`).
/// Equivalent to constructing a KDiff and calling GetTextDiff().
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param Mode the diff granularity: Character, Word, LineThenCharacter, or Line
/// @param San the cleanup mode: Semantic, Lossless, or Efficiency
/// @return a single string with `[+...]` and `[-...]` markup around changed fragments
DEKAF2_PUBLIC
KString KDiffToASCII (KStringView sOldText, KStringView sNewText,
                      KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
                      KDiff::Sanitation San = KDiff::Sanitation::Semantic);

/// convenience function: compute a diff and write plain text markup into both strings in place.
/// sOldText receives `[-deleted]` markup, sNewText receives `[+inserted]` markup.
/// @param sOldText the original string (modified in place with deletion markup)
/// @param sNewText the modified string (modified in place with insertion markup)
/// @param Mode the diff granularity: Character, Word, LineThenCharacter, or Line
/// @param San the cleanup mode: Semantic, Lossless, or Efficiency
/// @return count of diff fragments (Insert + Delete), 0 if texts are equal
DEKAF2_PUBLIC
std::size_t KDiffToASCII2 (KStringRef& sOldText, KStringRef& sNewText,
                           KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
                           KDiff::Sanitation San = KDiff::Sanitation::Semantic);


/// @}

DEKAF2_NAMESPACE_END
