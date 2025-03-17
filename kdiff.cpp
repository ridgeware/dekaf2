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

#include "kdiff.h"
#include "diff_match_patch.h"
#include "klog.h"
#include "kctype.h"
#include "kutf8.h"
#include "kstringutils.h"
#include <string>
#include <type_traits>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

template<class string_t, class stringview_t>
struct DMP_String_helpers
{
	static inline string_t to_string(int n)
	{
		return kSignedToString<string_t>(n);
	}

	static inline stringview_t mid(const stringview_t &str,
								   typename stringview_t::size_type pos,
								   typename stringview_t::size_type len = stringview_t::npos)
	{
		return (pos >= str.length()) ? stringview_t() : str.substr(pos, len);
	}

	static inline stringview_t right(const stringview_t& str, typename stringview_t::size_type n)
	{
		return n > str.size() ? stringview_t() : str.substr(str.size() - n);
	}
};

template<>
struct DMP_String_helpers<KString, KStringView>
{
	static inline KString to_string(int n)
	{
		return kSignedToString<KString>(n);
	}

	static inline KStringView mid(const KStringView &str,
								  KStringView::size_type pos,
								  KStringView::size_type len = KStringView::npos)
	{
		return str.Mid(pos, len);
	}

	static inline KStringView right(const KStringView& str, KStringView::size_type n)
	{
		return str.Right(n);
	}
};

template <class char_t, class utf32_type = unsigned>
struct DMP_utf32_helpers
{
	using utf32_t = utf32_type;

	template <class iterator>
	static iterator to_utf32(iterator i, iterator end, utf32_t& u)
	{
		// to_utf32 is not called for UTF8 strings in diff_match_patch,
		// therefore do not decode those here
		if (sizeof(char_t) == 1)
		{
			u = Unicode::CodepointCast(*i++);
		}
		else
		{
			u = Unicode::Codepoint(i, end);
		}

		return i;
	}

	template <class iterator>
	static iterator from_utf32(utf32_t u, iterator o)
	{
		if (sizeof(char_t) == 2)
		{
			if (Unicode::NeedsSurrogates(u))
			{
				Unicode::SurrogatePair sp(u);
				*o++ = static_cast<char_t>(sp.low);
				*o++ = static_cast<char_t>(sp.high);
			}
			else
			{
				*o++ = static_cast<char_t>(u);
			}
		}
		else
		{
			*o++ = static_cast<char_t>(u);
		}

		return o;
	}
};

template <typename string_t, typename stringview_t>
struct DMP_narrow_traits
: DMP_utf32_helpers<char, uint32_t>
, DMP_String_helpers<string_t, stringview_t>
{
	static bool        is_alnum  (char c)             { return KASCII::kIsAlNum(c);             }
	static bool        is_digit  (char c)             { return KASCII::kIsDigit(c);             }
	static bool        is_space  (char c)             { return KASCII::kIsSpace(c);             }
	static bool        is_control(char c)             { return (c == '\n' || c == '\r');        }
	static uint32_t    to_uint32(const string_t& str) { return kToInt<uint32_t>(str);           }
	static char        from_wchar(wchar_t c)          { return static_cast<char>(c);            }
	static wchar_t     to_wchar(char c)               { return static_cast<wchar_t>(c);         }
	static string_t    cs(const wchar_t* s)           { return string_t(s, s + std::wcslen(s)); }
	static const char  eol     = '\n';
	static const char  tab     = '\t';
	constexpr static const char* pilcrow = "\xC2\xB6";
};

template <typename string_t, typename stringview_t>
struct DMP_wide_traits
: DMP_utf32_helpers<wchar_t, uint32_t>
, DMP_String_helpers<string_t, stringview_t>
{
	static bool        is_alnum  (wchar_t c)          { return kIsAlNum(c);                   }
	static bool        is_digit  (wchar_t c)          { return kIsDigit(c);                   }
	static bool        is_space  (wchar_t c)          { return kIsSpace(c);                   }
	static bool        is_control(wchar_t c)          { return (c == L'\n' || c == L'\r');    }
	static uint32_t    to_uint32(const string_t& str) { return kToInt<uint32_t>(str);         }
	static wchar_t     from_wchar(wchar_t c)          { return c;                             }
	static wchar_t     to_wchar(wchar_t c)            { return c;                             }
	static const wchar_t* cs(const wchar_t* s)        { return s;                             }
	static const wchar_t eol = L'\n';
	static const wchar_t tab = L'\t';
	static const wchar_t pilcrow = L'\u00b6';
};

} // of namespace detail

#if defined(DEKAF2_KDIFF_USE_WSTRING)
using KDiffMatchPatch = diff_match_patch<KDiff::string_t, KDiff::stringview_t, detail::DMP_wide_traits<KDiff::string_t, KDiff::stringview_t>>;
#elif defined(DEKAF2_KDIFF_USE_KSTRING)
using KDiffMatchPatch = diff_match_patch<KDiff::string_t, KDiff::stringview_t, detail::DMP_narrow_traits<KDiff::string_t, KDiff::stringview_t>>;
#else
using KDiffMatchPatch = diff_match_patch<KDiff::string_t, KDiff::stringview_t, detail::DMP_narrow_traits<KDiff::string_t, KDiff::stringview_t>>;
#endif

namespace {

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
void PrintException(const KDiffMatchPatch::string_t& sEx)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_KDIFF_USE_WSTRING
	kException(Unicode::ToUTF<KString>(sEx).c_str());
#else
	kException(sEx);
#endif
}

} // end of anonymous namespace

#if defined(DEKAF2_KDIFF_USE_WSTRING)
//-----------------------------------------------------------------------------
KString KDiff::Diff::GetText() const
//-----------------------------------------------------------------------------
{
	return Unicode::ToUTF<KString>(m_sText);

} // Diff::GetText

//-----------------------------------------------------------------------------
void KDiff::Diff::SetText(const KString& sText)
//-----------------------------------------------------------------------------
{
	Unicode::ConvertUTF(sText, m_sText);

} // Diff::SetText
#endif

//-----------------------------------------------------------------------------
void KDiff::CreateDiff(KStringView sOldText,
					   KStringView sNewText,
					   DiffMode Mode,
					   Sanitation San)
//-----------------------------------------------------------------------------
{
	m_Diffs.reset();

	try
	{
#ifdef DEKAF2_KDIFF_USE_WSTRING
		auto sFrom = Unicode::ConvertUTF<KDiffMatchPatch::string_t>(sOldText);
		auto sTo   = Unicode::ConvertUTF<KDiffMatchPatch::string_t>(sNewText);
		m_iMaxSize = std::max(sFrom.size(), sTo.size());
		auto Diffs = KDiffMatchPatch::diff_main(sFrom, sTo, static_cast<KDiffMatchPatch::Mode>(Mode), 2.0f);
#else
		m_iMaxSize = std::max(sOldText.size(), sNewText.size());
		auto Diffs = KDiffMatchPatch::diff_main(sOldText, sNewText, static_cast<KDiffMatchPatch::Mode>(Mode), 2.0f);
#endif

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
		PrintException(sEx);
	}

} // CreateDiff

//-----------------------------------------------------------------------------
KDiff::Diffs& KDiff::GetDiffs()
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_HAS_CPP_20) && defined(__cpp_lib_is_layout_compatible) && !defined(_MSC_VER)
	static_assert(std::is_layout_compatible<KDiff::Diff, KDiffMatchPatch::Diff>::value, "different layout for Diff types");
#else
	static_assert(sizeof(KDiff::Diff) == sizeof(KDiffMatchPatch::Diff), "different size for Diff types");
	static_assert(std::is_same<std::underlying_type<KDiff::Operation>::type, std::underlying_type<KDiffMatchPatch::Operation>::type>::value, "different type for Operation types");
	static_assert(std::is_same<KDiff::string_t, decltype(KDiffMatchPatch::Diff::text)>::value, "different type for string types");
#endif

	static Diffs s_EmptyDiffs;

	if (!m_Diffs)
	{
		return s_EmptyDiffs;
	}

	return *static_cast<KDiff::Diffs*>(m_Diffs.get());

} // GetDiffs

//-----------------------------------------------------------------------------
KString KDiff::GetUnifiedDiff()
//-----------------------------------------------------------------------------
{
	if (m_Diffs)
	{
		try
		{
			auto Patches = KDiffMatchPatch().patch_make(*dget(m_Diffs));
#ifdef DEKAF2_KDIFF_USE_WSTRING
			return Unicode::ToUTF<KString>(KDiffMatchPatch::patch_toText(Patches));
#else
			return KDiffMatchPatch::patch_toText(Patches);
#endif
		}
		catch (const KDiffMatchPatch::string_t& sEx)
		{
			PrintException(sEx);
		}
	}

	return KString{};

} // GetUnifiedDiff

//-----------------------------------------------------------------------------
KString KDiff::GetTextDiff()
//-----------------------------------------------------------------------------
{
	KString sDiff;

	// iterate over diffs:
	for (const auto& diff : GetDiffs())
	{
		switch (diff.GetOperation())
		{
			case Operation::Insert:
				sDiff += "[+";
				sDiff += diff.GetText();
				sDiff += ']';
				break;
				
			case Operation::Delete:
				sDiff += "[-";
				sDiff += diff.GetText();
				sDiff += ']';
				break;

			case Operation::Equal:
			default:
				sDiff += diff.GetText();
				break;
		}
	}

	return sDiff;

} // GetTextDiff

//-----------------------------------------------------------------------------
std::size_t KDiff::GetTextDiff (KStringRef& sOldText, KStringRef& sNewText)
//-----------------------------------------------------------------------------
{
	sOldText.clear();
	sNewText.clear();
	std::size_t iDiffs { 0 };

	// iterate over diffs:
	for (const auto& diff : GetDiffs())
	{
		switch (diff.GetOperation())
		{
			case Operation::Insert:
				sNewText += "[+";
				sNewText += diff.GetText();
				sNewText += ']';
				++iDiffs;
				break;

			case Operation::Delete:
				sOldText += "[-";
				sOldText += diff.GetText();
				sOldText += ']';
				++iDiffs;
				break;

			case Operation::Equal:
			default:
#if defined(DEKAF2_KDIFF_USE_WSTRING)
				auto sText = diff.GetText();
#else
				auto& sText = diff.GetText();
#endif
				sOldText += sText;
				sNewText += sText;
				break;
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

	// iterate over diffs:
	for (const auto& diff : GetDiffs())
	{
		switch (diff.GetOperation())
		{
			case Operation::Insert:
				sDiff += '<';
				sDiff += sInsertTag;
				sDiff += '>';
				sDiff += diff.GetText();
				sDiff += "</";
				sDiff += sInsertTag;
				sDiff += '>';
				break;

			case Operation::Delete:
				sDiff += '<';
				sDiff += sDeleteTag;
				sDiff += '>';
				sDiff += diff.GetText();
				sDiff += "</";
				sDiff += sDeleteTag;
				sDiff += '>';
				break;

			case Operation::Equal:
			default:
				sDiff += diff.GetText();
				break;
		}
	}

	return sDiff;

} // GetHTMLDiff

//-----------------------------------------------------------------------------
std::size_t KDiff::GetHTMLDiff (KStringRef& sOldText, KStringRef& sNewText, KStringView sInsertTag/*="ins"*/, KStringView sDeleteTag/*="del"*/)
//-----------------------------------------------------------------------------
{
	sOldText.clear();
	sNewText.clear();
	std::size_t iDiffs { 0 };

	// iterate over diffs:
	for (const auto& diff : GetDiffs())
	{
		switch (diff.GetOperation())
		{
			case Operation::Insert:
				sNewText += '<';
				sNewText += sInsertTag;
				sNewText += '>';
				sNewText += diff.GetText();
				sNewText += "</";
				sNewText += sInsertTag;
				sNewText += '>';
				++iDiffs;
				break;

			case Operation::Delete:
				sOldText += '<';
				sOldText += sDeleteTag;
				sOldText += '>';
				sOldText += diff.GetText();
				sOldText += "</";
				sOldText += sDeleteTag;
				sOldText += '>';
				++iDiffs;
				break;

			case Operation::Equal:
			default:
#if defined(DEKAF2_KDIFF_USE_WSTRING)
				auto sText = diff.GetText();
#else
				auto& sText = diff.GetText();
#endif
				sOldText += sText;
				sNewText += sText;
				break;
		}
	}

	// sOld and sNew now contain markup
	return iDiffs;

} // GetHTMLDiff2

//-----------------------------------------------------------------------------
std::size_t KDiff::GetLevenshteinDistance()
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
			PrintException(sEx);
		}
	}

	return 0;

} // GetLevenshteinDistance

//-----------------------------------------------------------------------------
uint16_t KDiff::GetPercentDistance()
//-----------------------------------------------------------------------------
{
	return m_iMaxSize ? GetLevenshteinDistance() * 100 / m_iMaxSize : 0;

} // GetPercentDistance

//-----------------------------------------------------------------------------
KString KDiffToHTML (KStringView sOldText, KStringView sNewText,
					 KStringView sInsertTag/*="ins"*/, KStringView sDeleteTag/*="kDebug"*/,
					 KDiff::DiffMode Mode  /*= KDiff::DiffMode::Character*/,
					 KDiff::Sanitation San /*= KDiff::Sanitation::Semantic*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ(sOldText, sNewText, Mode, San);
	auto sDiff = Differ.GetHTMLDiff(sInsertTag, sDeleteTag);

	kDebug (2, "diffs: {}", sDiff);
	return sDiff;

} // KDiffToHTML

//-----------------------------------------------------------------------------
std::size_t KDiffToHTML2 (KStringRef& sOldText, KStringRef& sNewText,
						  KStringView sInsertTag/*="ins"*/, KStringView sDeleteTag/*="del"*/,
						  KDiff::DiffMode Mode  /*= KDiff::DiffMode::Character*/,
						  KDiff::Sanitation San /*= KDiff::Sanitation::Semantic*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ (sOldText, sNewText, Mode, San);
	auto iDiffs = Differ.GetHTMLDiff (sOldText, sNewText, sInsertTag, sDeleteTag);

	kDebug (2, "old diffs: {}", sOldText);
	kDebug (2, "new diffs: {}", sNewText);

	return iDiffs;

} // KDiffToHTML2

//-----------------------------------------------------------------------------
KString KDiffToASCII (KStringView sOldText, KStringView sNewText,
					  KDiff::DiffMode Mode  /*= KDiff::DiffMode::Character*/,
					  KDiff::Sanitation San /*= KDiff::Sanitation::Semantic*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ(sOldText, sNewText, Mode, San);
	auto sDiff = Differ.GetTextDiff();

	kDebug (2, "diffs: {}", sDiff);
	return sDiff;

} // KDiffToASCII

//-----------------------------------------------------------------------------
std::size_t KDiffToASCII2 (KStringRef& sOldText, KStringRef& sNewText,
						   KDiff::DiffMode Mode  /*= KDiff::DiffMode::Character*/,
						   KDiff::Sanitation San /*= KDiff::Sanitation::Semantic*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "old text: {}", sOldText);
	kDebug (2, "new text: {}", sNewText);

	KDiff Differ (sOldText, sNewText, Mode, San);
	auto iDiffs = Differ.GetTextDiff (sOldText, sNewText);

	kDebug (2, "old diffs: {}", sOldText);
	kDebug (2, "new diffs: {}", sNewText);

	return iDiffs;

} // KDiffToASCII2

DEKAF2_NAMESPACE_END
