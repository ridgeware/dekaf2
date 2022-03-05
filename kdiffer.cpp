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

#include "kdiffer.h"
#include "diff_match_patch.h"
#include "klog.h"
#include "kctype.h"

namespace dekaf2 {

namespace detail {

struct KString_traits : diff_match_patch_utf32_direct<char, uint32_t>
{
	static bool        is_alnum(char c)            { return KASCII::kIsAlNum(c);           }
	static bool        is_digit(char c)            { return KASCII::kIsDigit(c);           }
	static bool        is_space(char c)            { return KASCII::kIsSpace(c);           }
	static int         to_int(const char* s)       { return std::atoi(s);                  }
	static char        from_wchar(wchar_t c)       { return static_cast<char>(c);          }
	static wchar_t     to_wchar(char c)            { return static_cast<wchar_t>(c);       }
	static std::string cs(const wchar_t* s)        { return std::string(s, s + wcslen(s)); }
	static const char  eol = '\n';
	static const char  tab = '\t';
};

} // of namespace detail

using KDiffMatchPatch = diff_match_patch<KString, KStringView, detail::KString_traits>;

//-----------------------------------------------------------------------------
auto DiffDeleter = [](void* data)
//-----------------------------------------------------------------------------
{
	delete static_cast<KDiffMatchPatch::Diffs*>(data);
};

//-----------------------------------------------------------------------------
inline const KDiffMatchPatch::Diffs* dget(const KUniqueVoidPtr& p)
//-----------------------------------------------------------------------------
{
	return static_cast<KDiffMatchPatch::Diffs*>(p.get());
}

//-----------------------------------------------------------------------------
void KDiff::Diff(KStringView sOldText,
				   KStringView sNewText,
				   DiffMode Mode,
				   Sanitation San)
//-----------------------------------------------------------------------------
{
	m_Diffs.reset();

	try
	{
		auto Diffs = KDiffMatchPatch::diff_main(sOldText, sNewText, (Mode == DiffMode::Line), 2.0f);

		switch (San)
		{
			case Sanitation::Semantic:
				KDiffMatchPatch::diff_cleanupSemantic(Diffs);
				break;

			case Sanitation::Lossless:
				KDiffMatchPatch::diff_cleanupSemanticLossless(Diffs);
				break;

			case Sanitation::Efficiency:
				KDiffMatchPatch::diff_cleanupEfficiency(Diffs);
				break;
		}

		m_Diffs = KUniqueVoidPtr(new KDiffMatchPatch::Diffs(std::move(Diffs)), DiffDeleter);
	}
	catch (const KDiffMatchPatch::string_t& sEx)
	{
		kException(sEx);
	}

} // Diff

//-----------------------------------------------------------------------------
KString KDiff::GetUnifiedDiff()
//-----------------------------------------------------------------------------
{
	KString sDiff;

	if (m_Diffs)
	{
		try
		{
			auto Patches = KDiffMatchPatch().patch_make(*dget(m_Diffs));
			sDiff = KDiffMatchPatch::patch_toText(Patches);
		}
		catch (const KDiffMatchPatch::string_t& sEx)
		{
			kException(sEx);
		}
	}

	return sDiff;

} // GetUnifiedDiff

//-----------------------------------------------------------------------------
KString KDiff::GetTextDiff()
//-----------------------------------------------------------------------------
{
	KString sDiff;

	if (m_Diffs)
	{
		// iterate over diffs:
		for (const auto& diff : *dget(m_Diffs))
		{
			const auto& sText = diff.text;

			switch (diff.operation)
			{
			case KDiffMatchPatch::INSERT:
				sDiff += kFormat ("[+{}]", sText);
				break;
			case KDiffMatchPatch::DELETE:
				sDiff += kFormat ("[-{}]", sText);
				break;
			case KDiffMatchPatch::EQUAL:
			default:
				sDiff += sText;
				break;
			}
		}
	}

	return sDiff;

} // GetTextDiff

//-----------------------------------------------------------------------------
std::size_t KDiff::GetTextDiff2 (KString& sOld, KString& sNew)
//-----------------------------------------------------------------------------
{
	sOld.clear();
	sNew.clear();
	std::size_t iDiffs { 0 };

	if (m_Diffs)
	{
		// iterate over diffs:
		for (const auto& diff : *dget(m_Diffs))
		{
			const auto& sText = diff.text;

			switch (diff.operation)
			{
			case KDiffMatchPatch::INSERT:
				sNew += kFormat ("[+{}]", sText);
				++iDiffs;
				break;
			case KDiffMatchPatch::DELETE:
				sOld += kFormat ("[-{}]", sText);
				++iDiffs;
				break;
			case KDiffMatchPatch::EQUAL:
			default:
				sOld += sText;
				sNew += sText;
				break;
			}
		}
	}

	// sOld and sNew now contain markup
	return iDiffs;

} // GetTextDiff2

//-----------------------------------------------------------------------------
KString KDiff::GetHTMLDiff(KStringView sInsertTag, KStringView sDeleteTag)
//-----------------------------------------------------------------------------
{
	KString sDiff;

	if (m_Diffs)
	{
		// iterate over diffs:
		for (const auto& diff : *dget(m_Diffs))
		{
			const auto& sText = diff.text;

			switch (diff.operation)
			{
			case KDiffMatchPatch::INSERT:
				sDiff += kFormat ("<{}>{}</{}>", sInsertTag, sText, sInsertTag);
				break;
			case KDiffMatchPatch::DELETE:
				sDiff += kFormat ("<{}>{}</{}>", sDeleteTag, sText, sDeleteTag);
				break;
			case KDiffMatchPatch::EQUAL:
			default:
				sDiff += sText;
				break;
			}
		}
	}

	return sDiff;

} // GetHTMLDiff

//-----------------------------------------------------------------------------
std::size_t KDiff::GetHTMLDiff2 (KString& sOld, KString& sNew, KStringView sInsertTag/*="ins"*/, KStringView sDeleteTag/*="del"*/)
//-----------------------------------------------------------------------------
{
	sOld.clear();
	sNew.clear();
	std::size_t iDiffs { 0 };

	if (m_Diffs)
	{
		// iterate over diffs:
		for (const auto& diff : *dget(m_Diffs))
		{
			const auto& sText = diff.text;

			switch (diff.operation)
			{
			case KDiffMatchPatch::INSERT:
				sNew += kFormat ("<{}>{}</{}>", sInsertTag, sText, sInsertTag);
				++iDiffs;
				break;
			case KDiffMatchPatch::DELETE:
				sOld += kFormat ("<{}>{}</{}>", sDeleteTag, sText, sDeleteTag);
				++iDiffs;
				break;
			case KDiffMatchPatch::EQUAL:
			default:
				sOld += sText;
				sNew += sText;
				break;
			}
		}
	}

	// sOld and sNew now contain markup
	return iDiffs;

} // GetHTMLDiff2

//-----------------------------------------------------------------------------
uint32_t KDiff::GetLevenshteinDistance()
//-----------------------------------------------------------------------------
{
	if (m_Diffs)
	{
		try
		{
			return KDiffMatchPatch::diff_levenshtein(*dget(m_Diffs));
		}
		catch (const KDiffMatchPatch::string_t& sEx)
		{
			kException(sEx);
		}
	}

	return 0;

} // GetLevenshteinDistance

//-----------------------------------------------------------------------------
KString KDiffToHTML (KStringView sOldText, KStringView sNewText, KStringView sInsertTag/*="ins"*/, KStringView sDeleteTag/*="kDebug"*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ(sOldText, sNewText);
	auto sDiff = Differ.GetHTMLDiff(sInsertTag, sDeleteTag);

	kDebug (2, "diffs: {}", sDiff);
	return sDiff;

} // KDiffToHTML

//-----------------------------------------------------------------------------
std::size_t KDiffToHTML2 (KString& sOldText, KString& sNewText, KStringView sInsertTag/*="ins"*/, KStringView sDeleteTag/*="del"*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ (sOldText, sNewText);
	auto iDiffs = Differ.GetHTMLDiff2 (sOldText, sNewText, sInsertTag, sDeleteTag);

	kDebug (2, "old diffs: {}", sOldText);
	kDebug (2, "new diffs: {}", sNewText);

	return iDiffs;

} // KDiffToHTML2

//-----------------------------------------------------------------------------
KString KDiffToASCII (KStringView sOldText, KStringView sNewText)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ(sOldText, sNewText);
	auto sDiff = Differ.GetTextDiff();

	kDebug (2, "diffs: {}", sDiff);
	return sDiff;

} // KDiffToASCII

//-----------------------------------------------------------------------------
std::size_t KDiffToASCII2 (KString& sOldText, KString& sNewText)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ (sOldText, sNewText);
	auto iDiffs = Differ.GetTextDiff2 (sOldText, sNewText);

	kDebug (2, "old diffs: {}", sOldText);
	kDebug (2, "new diffs: {}", sNewText);

	return iDiffs;

} // KDiffToASCII2

} // end of namespace dekaf2
