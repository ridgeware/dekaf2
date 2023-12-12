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
/// Generate diffs in various formats from two input strings

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

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generate diffs in various formats from two input strings
class KDiff
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

	enum DiffMode
	{
		Character         = 0, ///< diff characters
		Word              = 1, ///< diff words (not yet implemented)
		LineThenCharacter = 2, ///< diff lines, then characters (speedup for character mode)
		Line              = 3  ///< diff lines (like gnu diff, not yet functional)
	};

	enum Sanitation
	{
		Semantic,   ///< best for human eyes
		Lossless,   ///< more fragmented, but still good for human eyes
		Efficiency  ///< for efficient patches, not well readable
	};

	enum Operation : uint16_t
	{
		Equal  = 0,
		Insert = 1,
		Delete = 2
	};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// one diff with its associated operation
	struct Diff
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		Diff(std::wstring sText, Operation op);
		Diff(std::string sText, Operation op);
		Diff(KString sText, Operation op);
#if defined(DEKAF2_KDIFF_USE_WSTRING)
		KString            GetText() const;
#else
		const string_t&    GetText() const { return m_sText; }
#endif
#if defined(DEKAF2_KDIFF_USE_WSTRING)
		void SetText(const KString& sText);
#elif defined(DEKAF2_KDIFF_USE_KSTRING)
		void SetText(KString sText)        { m_sText = std::move(sText);    }
#else
		void SetText(const KString& sText) { m_sText = sText.ToStdString(); }
#endif
		Operation GetOperation()     const { return m_operation;            }

#if defined(DEKAF2_KDIFF_USE_WSTRING)
		operator KString()           const { return GetText();              }
#else
		operator const string_t&()   const { return GetText();              }
#endif
		operator Operation()         const { return GetOperation();         }

	//----------
	private:
	//----------

		Operation    m_operation;
		string_t     m_sText;

	}; // Diff

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

	/// get a vector with all diffs
	Diffs& GetDiffs();

	/// return difference in unified diff format
	KString GetUnifiedDiff();

	/// return difference in text format
	KString GetTextDiff();

	/// return difference in text format, old and new
	/// @param sOldText returns the original string including text markup for all deleted fragments
	/// @param sNewText returns the modified string including text markup for all inserted fragments
	/// @return count of diffs, 0 if equal
	std::size_t GetTextDiff (KStringRef& sOldText, KStringRef& sNewText);

	/// return difference in HTML markup style
	/// @param sInsertTag tag used for inserted text fragments (default = "ins")
	/// @param sDeleteTag tag used for deleted text fragments (default = "del")
	KString GetHTMLDiff(KStringView sInsertTag = "ins", KStringView sDeleteTag = "del");

	/// return difference in HTML markup style, old and new
	/// @param sOldText returns the original string including markup for all deleted fragments
	/// @param sNewText returns the modified string including markup for all inserted fragments
	/// @param sInsertTag tag used for inserted text fragments (default = "ins")
	/// @param sDeleteTag tag used for deleted text fragments (default = "del")
	/// @return count of diffs, 0 if equal
	std::size_t GetHTMLDiff (KStringRef& sOldText, KStringRef& sNewText,
							 KStringView sInsertTag = "ins", KStringView sDeleteTag = "del");

	/// return Levenshtein distance for the diff (number of inserted, deleted or substituted characters)
	std::size_t GetLevenshteinDistance();

	/// return Levenshtein distance expressed as percentage
	uint16_t GetPercentDistance();

//----------
private:
//----------

	KUniqueVoidPtr m_Diffs;
	string_t::size_type m_iMaxSize { 0 };

}; // KDiff

/// shortcut to create a diff with HTML markups
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param sInsertTag tag used for inserted text fragments (default = "ins")
/// @param sDeleteTag tag used for deleted text fragments (default = "del")
/// @param Mode the diff compare mode: Character, Word, or Line, defaults to Character
/// @param San the sanitation mode: Semantic, Lossless, or Efficiency, defaults to Semantic
KString KDiffToHTML (KStringView sOldText, KStringView sNewText,
					 KStringView sInsertTag="ins", KStringView sDeleteTag="del",
					 KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
					 KDiff::Sanitation San = KDiff::Sanitation::Semantic);

/// shortcut to create a diff with HTML markups by modifying both strings in place
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param sInsertTag tag used for inserted text fragments (default = "ins")
/// @param sDeleteTag tag used for deleted text fragments (default = "del")
/// @param Mode the diff compare mode: Character, Word, or Line, defaults to Character
/// @param San the sanitation mode: Semantic, Lossless, or Efficiency, defaults to Semantic
/// @return count of diffs, 0 if equal
std::size_t KDiffToHTML2 (KStringRef& sOldText, KStringRef& sNewText,
						  KStringView sInsertTag="ins", KStringView sDeleteTag="del",
						  KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
						  KDiff::Sanitation San = KDiff::Sanitation::Semantic);

/// shortcut to create a diff in text format
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param Mode the diff compare mode: Character, Word, or Line, defaults to Character
/// @param San the sanitation mode: Semantic, Lossless, or Efficiency, defaults to Semantic
KString KDiffToASCII (KStringView sOldText, KStringView sNewText,
					  KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
					  KDiff::Sanitation San = KDiff::Sanitation::Semantic);

/// shortcut to create a diff in text format by modifying both strings in place
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param Mode the diff compare mode: Character, Word, or Line, defaults to Character
/// @param San the sanitation mode: Semantic, Lossless, or Efficiency, defaults to Semantic
/// @return count of diffs, 0 if equal
std::size_t KDiffToASCII2 (KStringRef& sOldText, KStringRef& sNewText,
						   KDiff::DiffMode Mode  = KDiff::DiffMode::Character,
						   KDiff::Sanitation San = KDiff::Sanitation::Semantic);

DEKAF2_NAMESPACE_END
