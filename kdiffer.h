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

#include "kstringview.h"
#include "bits/kunique_deleter.h"

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Generate diffs in various formats from two input strings
class KDiffer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum DiffMode
	{
		Character,  ///< diff characters
		Word,       ///< diff words (not yet implemented)
		Line        ///< diff lines
	};

	enum Sanitation
	{
		Semantic,	///< best for human eyes
		Lossless,   ///< more fragmented, but still good for human eyes
		Efficiency  ///< for efficient patches, not well readable
	};

	/// default ctor
	KDiffer() = default;

	/// construct with source and target strings, and mode settings
	/// @param sSource the original string
	/// @param sTarget the modified string
	/// @param Mode the diff compare mode: Character, Word, or Line
	/// @param San the sanitation mode: Semantic, Lossless, or Efficiency
	KDiffer(KStringView sSource,
			KStringView sTarget,
			DiffMode Mode = DiffMode::Character,
			Sanitation San = Sanitation::Semantic)
	{
		Diff(sSource, sTarget);
	}

	/// create the difference between two strings
	/// @param sSource the original string
	/// @param sTarget the modified string
	/// @param Mode the diff compare mode: Character, Word, or Line
	/// @param San the sanitation mode: Semantic, Lossless, or Efficiency
	void Diff(KStringView sSource,
			  KStringView sTarget,
			  DiffMode Mode = DiffMode::Character,
			  Sanitation San = Sanitation::Semantic);

	/// return difference in unified diff format (only good for line mode)
	KString GetUnifiedDiff();

	/// return difference in text format
	KString GetTextDiff();

	/// return difference in text format, old and new
	void GetTextDiff2 (KString sOld, KString sNew);

	/// return difference in HTML markup style
	/// @param sInsertTag tag used for inserted text fragments (default = "ins")
	/// @param sDeleteTag tag used for deleted text fragments (default = "del")
	KString GetHTMLDiff(KStringView sInsertTag = "ins", KStringView sDeleteTag = "del");

	/// return difference in HTML markup style, old and new
	/// @param sInsertTag tag used for inserted text fragments (default = "ins")
	/// @param sDeleteTag tag used for deleted text fragments (default = "del")
	void GetHTMLDiff2 (KString& sOld, KString& sNew, KStringView sInsertTag = "ins", KStringView sDeleteTag = "del");

	/// return Levenshtein distance for the diff (number of inserted, deleted or substituted characters)
	uint32_t GetLevenshteinDistance();

//----------
private:
//----------

	KUniqueVoidPtr m_Diffs;

}; // KDiffer

/// shortcut to create a diff with HTML markups
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param sInsertTag tag used for inserted text fragments (default = "ins")
/// @param sDeleteTag tag used for deleted text fragments (default = "del")
KString KDiffToHTML (KStringView sOldText, KStringView sNewText, KStringView sInsertTag="ins", KStringView sDeleteTag="del");

/// shortcut to create a diff with HTML markups by modifying both strings in place
/// @param sOldText the original string
/// @param sNewText the modified string
/// @param sInsertTag tag used for inserted text fragments (default = "ins")
/// @param sDeleteTag tag used for deleted text fragments (default = "del")
void KDiffToHTML2 (KString& sOldText, KString& sNewText, KStringView sInsertTag="ins", KStringView sDeleteTag="del");

/// shortcut to create a diff in text format
/// @param sOldText the original string
/// @param sNewText the modified string
KString KDiffToASCII (KStringView sOldText, KStringView sNewText);

/// shortcut to create a diff in text format by modifying both strings in place
/// @param sOldText the original string
/// @param sNewText the modified string
void KDiffToASCII2 (KString& sOldText, KString& sNewText);

} // end of namespace dekaf2

