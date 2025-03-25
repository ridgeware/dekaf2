/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

#include "kwords.h"
#include "kutf.h"
#include "khtmlentities.h"
#include "kctype.h"
#include "kcompatibility.h"

DEKAF2_NAMESPACE_BEGIN

namespace detail {
namespace splitting_parser {

static DEKAF2_CONSTEXPR_14 KStringViewPair s_Pair_Empty { "", "" };
static DEKAF2_CONSTEXPR_14 KStringViewPair s_Pair_Word { "a", "" };

//-----------------------------------------------------------------------------
KStringViewPair CountSpacedText::NextPair()
//-----------------------------------------------------------------------------
{
	size_t iSizeConsumed { 0 };
	bool bIsEmpty { true };

	for (auto ch : m_sInput)
	{
		if (KASCII::kIsSpace(ch))
		{
			if (!bIsEmpty)
			{
				// abort scanning here, this is the trailing skeleton
				break;
			}
		}
		else
		{
			bIsEmpty = false;
		}

		++iSizeConsumed;
	}

	m_sInput.remove_prefix(iSizeConsumed);

	return bIsEmpty ? s_Pair_Empty : s_Pair_Word;

} // CountSpacedText::NextPair

//-----------------------------------------------------------------------------
KStringViewPair SimpleSpacedText::NextPair()
//-----------------------------------------------------------------------------
{
	size_t iSizeSkel { 0 };
	size_t iSizeWord { 0 };

	for (auto ch : m_sInput)
	{
		if (KASCII::kIsSpace(ch))
		{
			if (iSizeWord)
			{
				// abort scanning here, this is the trailing skeleton
				break;
			}
			++iSizeSkel;
		}
		else
		{
			++iSizeWord;
		}
	}

	KStringViewPair sPair;
	sPair.second.assign(m_sInput.data(), iSizeSkel);
	m_sInput.remove_prefix(iSizeSkel);
	sPair.first.assign(m_sInput.data(), iSizeWord);
	m_sInput.remove_prefix(iSizeWord);

	return sPair;

} // SimpleSpacedText::NextPair

//-----------------------------------------------------------------------------
KStringViewPair CountText::NextPair()
//-----------------------------------------------------------------------------
{
	size_t iSizeConsumed { 0 };
	bool bIsEmpty { true };

	kutf::ForEach(m_sInput, [&iSizeConsumed, &bIsEmpty](uint32_t ch)
	{
		if (!kIsAlNum(ch))
		{
			if (!bIsEmpty)
			{
				// abort scanning here, this is the trailing skeleton
				return false;
			}
			iSizeConsumed += kutf::UTFChars(ch);
		}
		else
		{
			iSizeConsumed += kutf::UTFChars(ch);
			bIsEmpty = false;
		}
		return true;
	});

	m_sInput.remove_prefix(iSizeConsumed);

	return bIsEmpty ? s_Pair_Empty : s_Pair_Word;

} // CountText::NextPair

//-----------------------------------------------------------------------------
KStringViewPair SimpleText::NextPair()
//-----------------------------------------------------------------------------
{
	size_t iSizeSkel { 0 };
	size_t iSizeWord { 0 };

	kutf::ForEach(m_sInput, [&iSizeSkel, &iSizeWord](uint32_t ch)
	{
		if (!kIsAlNum(ch))
		{
			if (iSizeWord)
			{
				// abort scanning here, this is the trailing skeleton
				return false;
			}
			iSizeSkel += kutf::UTFChars(ch);
		}
		else
		{
			iSizeWord += kutf::UTFChars(ch);
		}
		return true;
	});

	KStringViewPair sPair;
	sPair.second.assign(m_sInput.data(), iSizeSkel);
	m_sInput.remove_prefix(iSizeSkel);
	sPair.first.assign(m_sInput.data(), iSizeWord);
	m_sInput.remove_prefix(iSizeWord);

	return sPair;

} // SimpleText::NextPair

//-----------------------------------------------------------------------------
KStringViewPair CountHTML::NextPair()
//-----------------------------------------------------------------------------
{
	size_t iSizeConsumed { 0 };
	size_t iStartEntity { 0 };
	KString sTagName;
	static const char ScriptEndTag[] = "/script>";
	const char* pScriptEndTag = nullptr;
	bool bOpenTag { false };
	bool bOpenTagHadSpace { false };
	bool bOpenScript { false };
	bool bOpenEntity { false };
	bool bIsEmpty { true };

	kutf::ForEach(m_sInput, [&](uint32_t ch)
	{
		if (bOpenScript)
		{
			// push everything into the skeleton until we had </script>
			if (ch == '<')
			{
				pScriptEndTag = ScriptEndTag;
			}
			else if (pScriptEndTag != nullptr)
			{
				if (KASCII::kToLower(ch) == static_cast<uint32_t>(*pScriptEndTag))
				{
					++pScriptEndTag;
					if (!*pScriptEndTag)
					{
						bOpenScript = false;
					}
				}
				else
				{
					if (ch == '<')
					{
						pScriptEndTag = ScriptEndTag;
					}
					else
					{
						pScriptEndTag = nullptr;
					}
				}
			}
			iSizeConsumed += kutf::UTFChars(ch);
		}
		else if (bOpenTag)
		{
			if (!bOpenTagHadSpace)
			{
				if (kIsSpace(ch))
				{
					bOpenTagHadSpace = true;
					if (sTagName == "script")
					{
						bOpenScript = true;
					}
				}
				else if (ch != '>')
				{
					// tag names may be ASCII only
					sTagName += kToLower(static_cast<KString::value_type>(ch));
				}
			}
			if (ch == '>')
			{
				bOpenTag = false;
				if (sTagName == "script")
				{
					bOpenScript = true;
				}
			}
			iSizeConsumed += kutf::UTFChars(ch);
		}
		else if (bOpenEntity)
		{
			if (kIsAlNum(ch))
			{
				++iSizeConsumed;
			}
			else
			{
				bOpenEntity = false;
				if (ch == ';')
				{
					++iSizeConsumed;
				}
				bIsEmpty = false;
				if (ch != ';')
				{
					if (ch == '&')
					{
						bOpenEntity = true;
						iStartEntity = iSizeConsumed;
						++iSizeConsumed;
					}
					else
					{
						bIsEmpty = false;
						iSizeConsumed += kutf::UTFChars(ch);
					}
				}
			}
		}
		else
		{
			if (ch == '&')
			{
				// start of entity
				bOpenEntity = true;
				iStartEntity = iSizeConsumed;
				++iSizeConsumed;
			}
			else if (!kIsAlNum(ch))
			{
				if (!bIsEmpty)
				{
					// abort scanning here, this is the trailing skeleton
					return false;
				}
				if (ch == '<')
				{
					bOpenTag = true;
					bOpenTagHadSpace = false;
					sTagName.clear();
				}
				iSizeConsumed += kutf::UTFChars(ch);
			}
			else
			{
				bIsEmpty = false;
				iSizeConsumed += kutf::UTFChars(ch);
			}
		}
		return true;
	});

	if (bOpenEntity)
	{
		bIsEmpty = false;
	}

	m_sInput.remove_prefix(iSizeConsumed);

	return bIsEmpty ? s_Pair_Empty : s_Pair_Word;

} // CountHTML::NextPair

//-----------------------------------------------------------------------------
std::pair<KString, KStringView> SimpleHTML::NextPair()
//-----------------------------------------------------------------------------
{
	size_t iSizeSkel { 0 };
	size_t iSizeWord { 0 };
	size_t iStartEntity { 0 };
	std::pair<KString, KStringView> sPair;
	KString sTagName;
	static const char ScriptEndTag[] = "/script>";
	const char* pScriptEndTag = nullptr;
	bool bOpenTag { false };
	bool bOpenTagHadSpace { false };
	bool bOpenScript { false };
	bool bOpenEntity { false };

	kutf::ForEach(m_sInput, [&](uint32_t ch)
	{
		if (bOpenScript)
		{
			// push everything into the skeleton until we had </script>
			if (ch == '<')
			{
				pScriptEndTag = ScriptEndTag;
			}
			else if (pScriptEndTag != nullptr)
			{
				if (KASCII::kToLower(ch) == static_cast<uint32_t>(*pScriptEndTag))
				{
					++pScriptEndTag;
					if (!*pScriptEndTag)
					{
						bOpenScript = false;
					}
				}
				else
				{
					if (ch == '<')
					{
						pScriptEndTag = ScriptEndTag;
					}
					else
					{
						pScriptEndTag = nullptr;
					}
				}
			}
			iSizeSkel += kutf::UTFChars(ch);
		}
		else if (bOpenTag)
		{
			if (!bOpenTagHadSpace)
			{
				if (kIsSpace(ch))
				{
					bOpenTagHadSpace = true;
					if (sTagName == "script")
					{
						bOpenScript = true;
					}
				}
				else if (ch != '>')
				{
					// tag names may be ASCII only
					sTagName += kToLower(static_cast<KString::value_type>(ch));
				}
			}
			if (ch == '>')
			{
				bOpenTag = false;
				if (sTagName == "script")
				{
					bOpenScript = true;
				}
			}
			iSizeSkel += kutf::UTFChars(ch);
		}
		else if (bOpenEntity)
		{
			if (kIsAlNum(ch))
			{
				++iSizeWord;
			}
			else
			{
				bOpenEntity = false;
				if (ch == ';')
				{
					++iSizeWord;
				}
				sPair.first += KHTMLEntity::DecodeOne(m_sInput.substr(iStartEntity, iSizeSkel + iSizeWord - iStartEntity));
				if (ch != ';')
				{
					if (ch == '&')
					{
						bOpenEntity = true;
						iStartEntity = iSizeSkel + iSizeWord;
						++iSizeWord;
					}
					else
					{
						kutf::ToUTF(ch, sPair.first);
						iSizeWord += kutf::UTFChars(ch);
					}
				}
			}
		}
		else
		{
			if (ch == '&')
			{
				// start of entity
				bOpenEntity = true;
				iStartEntity = iSizeSkel + iSizeWord;
				++iSizeWord;
			}
			else if (!kIsAlNum(ch))
			{
				if (!sPair.first.empty())
				{
					// abort scanning here, this is the trailing skeleton
					return false;
				}
				if (ch == '<')
				{
					bOpenTag = true;
					bOpenTagHadSpace = false;
					sTagName.clear();
				}
				iSizeSkel += kutf::UTFChars(ch);
			}
			else
			{
				kutf::ToUTF(ch, sPair.first);
				iSizeWord += kutf::UTFChars(ch);
			}
		}
		return true;
	});

	if (bOpenEntity)
	{
		sPair.first += KHTMLEntity::DecodeOne(m_sInput.substr(iStartEntity, iSizeSkel + iSizeWord - iStartEntity));
	}

	sPair.second.assign(m_sInput.data(), iSizeSkel);
	m_sInput.remove_prefix(iSizeSkel + iSizeWord);

	return sPair;

} // SimpleHTML::NextPair

//-----------------------------------------------------------------------------
std::pair<KString, KString> NormalizingHTML::NextPair()
//-----------------------------------------------------------------------------
{
	// the following whitespace chars are equivalent:
	//  CR, LF, TAB, SP
	//
	// examples:
	//   in: "hello  there"
	//  out: "hello there"
	//
	//   in: "hello     <span>there</span>    ."
	//  out: "hello <span>there</span> ."
	//
	//   in: "hello    <span>      there      </span>    ."
	//  out: "hello <span> there </span> ."
	//
	//   in: "hello    <span>   <standalone/>   there      </span>    ."
	//  out: "hello <span> <standalone/> there </span> ."
	//
	//   in: "hello \t \n  <span>      there      </span>    ."
	//  out: "hello <span> there </span> ."
	//

	size_t iSizeSkel { 0 };
	size_t iSizeWord { 0 };
	KString sTagName;
	static const char ScriptEndTag[] = "/script>";
	const char* pScriptEndTag = nullptr;
	bool bOpenTag { false };
	bool bOpenTagHadSpace { false };
	bool bOpenScript { false };
	bool bLastWasSpace { false };

	std::pair<KStringView, KString> sPair;

	kutf::ForEach(m_sInput, [&](uint32_t ch)
	{
		if (bOpenScript)
		{
			// push everything into the skeleton until we had </script>
			if (ch == '<')
			{
				pScriptEndTag = ScriptEndTag;
			}
			else if (pScriptEndTag != nullptr)
			{
				if (KASCII::kToLower(ch) == static_cast<uint32_t>(*pScriptEndTag))
				{
					++pScriptEndTag;
					if (!*pScriptEndTag)
					{
						bOpenScript = false;
					}
				}
				else
				{
					if (ch == '<')
					{
						pScriptEndTag = ScriptEndTag;
					}
					else
					{
						pScriptEndTag = nullptr;
					}
				}
			}
			kutf::ToUTF(ch, sPair.second);
			iSizeSkel += kutf::UTFChars(ch);
		}
		else if (bOpenTag)
		{
			if (!bOpenTagHadSpace)
			{
				if (kIsSpace(ch))
				{
					bOpenTagHadSpace = true;
					if (sTagName == "script")
					{
						bOpenScript = true;
					}
				}
				else if (ch != '>')
				{
					// tag names may be ASCII only
					sTagName += kToLower(static_cast<KString::value_type>(ch));
				}
			}
			if (ch == '>')
			{
				bOpenTag = false;
				bLastWasSpace = false;
				if (sTagName == "script")
				{
					bOpenScript = true;
				}
			}
			kutf::ToUTF(ch, sPair.second);
			iSizeSkel += kutf::UTFChars(ch);
		}
		else
		{
			if (!kIsAlNum(ch))
			{
				if (iSizeWord)
				{
					// abort scanning here, this is the trailing skeleton
					return false;
				}
				if (ch == '<')
				{
					sPair.second += '<';
					bOpenTag = true;
					bOpenTagHadSpace = false;
					sTagName.clear();
					bLastWasSpace = false;
				}
				else if (kIsSpace(ch))
				{
					if (!bLastWasSpace)
					{
						sPair.second += ' ';
						bLastWasSpace = true;
					}
				}
				else
				{
					kutf::ToUTF(ch, sPair.second);
					bLastWasSpace = false;
				}
				iSizeSkel += kutf::UTFChars(ch);
			}
			else
			{
				iSizeWord += kutf::UTFChars(ch);
			}
		}
		return true;
	});

	m_sInput.remove_prefix(iSizeSkel);
	sPair.first.assign(m_sInput.data(), iSizeWord);
	m_sInput.remove_prefix(iSizeWord);

	return sPair;

} // NormalizingHTML::NextPair

} // of namespace splitting_parser
} // of namespace detail

DEKAF2_NAMESPACE_END
