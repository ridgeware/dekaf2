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
#include "kutf8.h"
#include "khtmlentities.h"
#include <cctype>


namespace dekaf2 {

namespace detail {
namespace splitting_parser {

//-----------------------------------------------------------------------------
KStringViewPair SimpleText::NextPair()
//-----------------------------------------------------------------------------
{
	size_t iSizeSkel { 0 };
	size_t iSizeWord { 0 };

	Unicode::FromUTF8(m_sInput, [&](uint32_t ch)
	{
		if (!std::iswalnum(ch))
		{
			if (iSizeWord)
			{
				// abort scanning here, this is the trailing skeleton
				return false;
			}
			iSizeSkel += Unicode::UTF8Bytes(ch);
		}
		else
		{
			iSizeWord += Unicode::UTF8Bytes(ch);
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

	Unicode::FromUTF8(m_sInput, [&](uint32_t ch)
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
				if (std::tolower(ch) == *pScriptEndTag)
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
			iSizeSkel += Unicode::UTF8Bytes(ch);
		}
		else if (bOpenTag)
		{
			if (!bOpenTagHadSpace)
			{
				if (std::iswspace(ch))
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
					sTagName += std::tolower(static_cast<KString::value_type>(ch));
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
			iSizeSkel += Unicode::UTF8Bytes(ch);
		}
		else if (bOpenEntity)
		{
			if (std::iswalnum(ch))
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
						Unicode::ToUTF8(ch, sPair.first);
						iSizeWord += Unicode::UTF8Bytes(ch);
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
			else if (!std::iswalnum(ch))
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
				iSizeSkel += Unicode::UTF8Bytes(ch);
			}
			else
			{
				Unicode::ToUTF8(ch, sPair.first);
				iSizeWord += Unicode::UTF8Bytes(ch);
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

	Unicode::FromUTF8(m_sInput, [&](uint32_t ch)
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
				if (std::tolower(ch) == *pScriptEndTag)
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
			Unicode::ToUTF8(ch, sPair.second);
			iSizeSkel += Unicode::UTF8Bytes(ch);
		}
		else if (bOpenTag)
		{
			if (!bOpenTagHadSpace)
			{
				if (std::iswspace(ch))
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
					sTagName += std::tolower(static_cast<KString::value_type>(ch));
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
			Unicode::ToUTF8(ch, sPair.second);
			iSizeSkel += Unicode::UTF8Bytes(ch);
		}
		else
		{
			if (!std::iswalnum(ch))
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
				else if (std::iswspace(ch))
				{
					if (!bLastWasSpace)
					{
						sPair.second += ' ';
						bLastWasSpace = true;
					}
				}
				else
				{
					Unicode::ToUTF8(ch, sPair.second);
					bLastWasSpace = false;
				}
				iSizeSkel += Unicode::UTF8Bytes(ch);
			}
			else
			{
				iSizeWord += Unicode::UTF8Bytes(ch);
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


} // end of namespace dekaf2
