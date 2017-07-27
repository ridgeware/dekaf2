/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2015, Ridgeware, Inc.
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

#include <stdio.h>
#include <assert.h>
#include "kstringutils.h"
#include "kstring.h"
#include "klog.h"
#include "kregex.h"

namespace dekaf2
{

const KString::size_type KString::npos;

//------------------------------------------------------------------------------
KString& KString::append(const string_type& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.append(str, pos, n);
	} catch (std::exception& e) {
		KLog().exception(e, "append", "KString");
	} catch (...) {
		KLog().exception("append", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::assign(const string_type& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.assign(str, pos, n);
	} catch (std::exception& e) {
		KLog().exception(e, "assign", "KString");
	} catch (...) {
		KLog().exception("assign", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n, const string_type& str)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n, str);
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos1, n1, str, pos2, n2);
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, const value_type* s, size_type n2)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n1, s ? s : "", n2);
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, const value_type* s)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n1, s ? s : "");
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, size_type n2, value_type c)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n1, n2, c);
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, const string_type& str)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, str);
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, s ? s : "", n);
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, il);
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n, KStringView sv)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n, sv.data(), sv.size());
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::replace(iterator i1, iterator i2, KStringView sv)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(i1, i2, sv.data(), sv.size());
	} catch (std::exception& e) {
		KLog().exception(e, "replace", "KString");
	} catch (...) {
		KLog().exception("replace", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString KString::substr(size_type pos, size_type n) const
//------------------------------------------------------------------------------
{
	try {
		return m_rep.substr(pos, n);
	} catch (std::exception& e) {
		KLog().exception(e, "substr", "KString");
	} catch (...) {
		KLog().exception("substr", "KString");
	}
	return KString();
}

//------------------------------------------------------------------------------
KString::size_type KString::copy(value_type* s, size_type n, size_type pos) const
//------------------------------------------------------------------------------
{
	try {
		return m_rep.copy(s, n, pos);
	} catch (std::exception& e) {
		KLog().exception(e, "copy", "KString");
	} catch (...) {
		KLog().exception("copy", "KString");
	}
	return 0;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const string_type& str)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, str);
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos1, const string_type& str, size_type pos2, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos1, str, pos2, n);
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, s ? s : "", n);
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const value_type* s)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, s ? s : "");
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, size_type n, value_type c)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, n, c);
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::insert(iterator it, value_type c)
//------------------------------------------------------------------------------
{
	try {
		return m_rep.insert(it, c);
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return end();
}

//------------------------------------------------------------------------------
KString& KString::insert (iterator it, std::initializer_list<value_type> il)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(it, il);
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, KStringView sv)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, sv.data(), sv.size());
	} catch (std::exception& e) {
		KLog().exception(e, "insert", "KString");
	} catch (...) {
		KLog().exception("insert", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString& KString::erase(size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.erase(pos, n);
	} catch (std::exception& e) {
		KLog().exception(e, "erase", "KString");
	} catch (...) {
		KLog().exception("erase", "KString");
	}
	return *this;
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator position)
//------------------------------------------------------------------------------
{
	try {
		return m_rep.erase(position);
	} catch (std::exception& e) {
		KLog().exception(e, "erase", "KString");
	} catch (...) {
		KLog().exception("erase", "KString");
	}
	return end();
}

//------------------------------------------------------------------------------
KString::iterator KString::erase(iterator first, iterator last)
//------------------------------------------------------------------------------
{
	try {
		return m_rep.erase(first, last);
	} catch (std::exception& e) {
		KLog().exception(e, "erase", "KString");
	} catch (...) {
		KLog().exception("erase", "KString");
	}
	return end();
}

//----------------------------------------------------------------------
int KString::compare(size_type pos, size_type n, const string_type& str) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos, n, str);
	} catch (std::exception& e) {
		KLog().exception(e, "compare", "KString");
	} catch (...) {
		KLog().exception("compare", "KString");
	}
	return 1;
}

//----------------------------------------------------------------------
int KString::compare(size_type pos1, size_type n1, const string_type& str,  size_type pos2, size_type n2) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos1, n1, str, pos2, n2);
	} catch (std::exception& e) {
		KLog().exception(e, "compare", "KString");
	} catch (...) {
		KLog().exception("compare", "KString");
	}
	return 1;
}

//----------------------------------------------------------------------
int KString::compare(size_type pos, size_type n1, const value_type* s) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos, n1, s ? s : "");
	} catch (std::exception& e) {
		KLog().exception(e, "compare", "KString");
	} catch (...) {
		KLog().exception("compare", "KString");
	}
	return 1;
}

//----------------------------------------------------------------------
int KString::compare(size_type pos, size_type n1, const value_type* s, size_type n2) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos, n1, s ? s : "", n2);
	} catch (std::exception& e) {
		KLog().exception(e, "compare", "KString");
	} catch (...) {
		KLog().exception("compare", "KString");
	}
	return 1;
}

//----------------------------------------------------------------------
int KString::compare(size_type pos, size_type n1, KStringView sv) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos, n1, sv.data(), sv.size());
	} catch (std::exception& e) {
		KLog().exception(e, "compare", "KString");
	} catch (...) {
		KLog().exception("compare", "KString");
	}
	return 1;
}

//------------------------------------------------------------------------------
KString::size_type KString::Replace (const value_type* pszSearch, size_type iSearchLen, const value_type* pszReplaceWith, size_type iReplaceWithLen, bool bReplaceAll)
//------------------------------------------------------------------------------
{
	return dekaf2::Replace(m_rep, pszSearch, iSearchLen, pszReplaceWith, iReplaceWithLen, bReplaceAll);
}

//----------------------------------------------------------------------
KString::size_type KString::ReplaceRegex(const KStringView& sRegEx, const KStringView& sReplaceWith, bool bReplaceAll)
//----------------------------------------------------------------------
{
	return dekaf2::KRegex::Replace(m_rep, sRegEx, sReplaceWith, bReplaceAll);
}

//----------------------------------------------------------------------
KString::size_type KString::InnerReplace(size_type iIdxStartMatch, size_type iIdxEndMatch, const char* pszReplaceWith)
//----------------------------------------------------------------------
{
	size_type iRepalaceLen = strlen(pszReplaceWith);
	replace(iIdxStartMatch, (iIdxEndMatch - iIdxStartMatch), pszReplaceWith, iRepalaceLen);

	return iIdxStartMatch + iRepalaceLen; // Index after a last replaced character
} // InnerReplace

//----------------------------------------------------------------------
// static
bool KString::StartsWith(KStringView sInput, KStringView sPattern)
//----------------------------------------------------------------------
{
	if (sInput.size() < sPattern.size())
	{
		return false;
	}
	
	return !memcmp(sInput.data(), sPattern.data(), sPattern.size());

} // StartsWith

//----------------------------------------------------------------------
// static
bool KString::EndsWith(KStringView sInput, KStringView sPattern)
//----------------------------------------------------------------------
{
	if (sInput.size() < sPattern.size())
	{
		return false;
	}
	
	return !memcmp(sInput.data() + sInput.size() - sPattern.size(), sPattern.data(), sPattern.size());

} // EndsWith

//----------------------------------------------------------------------
bool KString::IsEmail() const
//----------------------------------------------------------------------
{
	return false;
//	return kIsEmail(m_rep.c_str());
} // IsEmail

//----------------------------------------------------------------------
bool KString::IsURL() const
//----------------------------------------------------------------------
{
	return false;
//	return kIsURL(m_rep.c_str());
} // IsURL

//----------------------------------------------------------------------
bool KString::IsFilePath() const
//----------------------------------------------------------------------
{
	return false;
//	return kIsFilePath(m_rep.c_str());
} // IsFilePath

//----------------------------------------------------------------------
KString KString::ToLower() const
//----------------------------------------------------------------------
{
	KString sLower;
	sLower.reserve(m_rep.size());

	for (const auto& it : m_rep)
	{
		sLower += static_cast<value_type>(std::tolower(static_cast<unsigned char>(it)));
	}

	return sLower;
} // ToUpper

//----------------------------------------------------------------------
KString KString::ToUpper() const
//----------------------------------------------------------------------
{
	KString sUpper;
	sUpper.reserve(m_rep.size());

	for (const auto& it : m_rep)
	{
		sUpper += static_cast<value_type>(std::toupper(static_cast<unsigned char>(it)));
	}

	return sUpper;
} // ToUpper

//----------------------------------------------------------------------
KString& KString::MakeLower()
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = static_cast<value_type>(std::tolower(static_cast<unsigned char>(it)));
	}
	return *this;
} // MakeLower

//----------------------------------------------------------------------
KString& KString::MakeUpper()
//----------------------------------------------------------------------
{
	for (auto& it : m_rep)
	{
		it = static_cast<value_type>(std::toupper(static_cast<unsigned char>(it)));
	}
	return *this;
} // MakeUpper

//----------------------------------------------------------------------
KString KString::Left(size_type iCount)
//----------------------------------------------------------------------
{
	if (iCount >= size())
	{
		return *this;
	}
	return substr(0, iCount);
} // Left

//----------------------------------------------------------------------
KString KString::Right(size_type iCount)
//----------------------------------------------------------------------
{
	if (iCount >= size())
	{
		return *this;
	}
	return substr(size() - iCount, iCount);
} // Right

//----------------------------------------------------------------------
KString& KString::TrimLeft()
//----------------------------------------------------------------------
{
	dekaf2::TrimLeft(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(const value_type chTarget)
//----------------------------------------------------------------------
{
	dekaf2::TrimLeft(m_rep, [chTarget](value_type ch){ return ch == chTarget; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(const value_type* pszTarget)
//----------------------------------------------------------------------
{
	dekaf2::TrimLeft(m_rep, [pszTarget](value_type ch){ return strchr(pszTarget, ch) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight()
//----------------------------------------------------------------------
{
	dekaf2::TrimRight(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(const value_type chTarget)
//----------------------------------------------------------------------
{
	dekaf2::TrimRight(m_rep, [chTarget](value_type ch){ return ch == chTarget; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(const value_type* pszTarget)
//----------------------------------------------------------------------
{
	dekaf2::TrimRight(m_rep, [pszTarget](value_type ch){ return strchr(pszTarget, ch) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim()
//----------------------------------------------------------------------
{
	dekaf2::Trim(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
}

//----------------------------------------------------------------------
KString& KString::Trim(const value_type chTarget)
//----------------------------------------------------------------------
{
	dekaf2::Trim(m_rep, [chTarget](value_type ch){ return ch == chTarget; } );
}

//----------------------------------------------------------------------
KString& KString::Trim(const value_type* pszTarget)
//----------------------------------------------------------------------
{
	dekaf2::Trim(m_rep, [pszTarget](value_type ch){ return strchr(pszTarget, ch) != nullptr; } );
}

//----------------------------------------------------------------------
KString& KString::ClipAt(const value_type* pszClipAt)
//----------------------------------------------------------------------
{
	size_type pos = m_rep.find(pszClipAt);
	if (pos != npos)
	{
		m_rep.erase(pos);
	}
	return *this;
} // ClipAt

//----------------------------------------------------------------------
KString& KString::ClipAtReverse(const value_type* pszClipAtReverse)
//----------------------------------------------------------------------
{
	size_type pos = m_rep.find(pszClipAtReverse);
	if (pos != npos)
	{
		m_rep.erase(0, pos);
	}
	return *this;
} // ClipAtReverse

//----------------------------------------------------------------------
class KStringBuffer::SimpleReplacer
//----------------------------------------------------------------------
{
public:
	explicit SimpleReplacer(const KString& replacer)
		: m_pReplacer(&replacer)
	{
	}

	CB_NEXT_OP operator()(const KString&, std::size_t, std::size_t, KString& sReplacer)
	{
		sReplacer = *m_pReplacer;
		return RET_REPLACE;
	}

private:
	const KString* m_pReplacer;
}; // KStringBuffer::SimpleReplacer

//----------------------------------------------------------------------
KString::size_type KStringBuffer::Replace(const KString& sSearchPattern, const SearchHandler& foundHandler)
//----------------------------------------------------------------------
{
	KString::size_type iNumReplacement = 0;
	KString::size_type iOffset = 0;
	KString::size_type iFoundIndex = m_sWorkBuffer.npos;

	KString sReplaceWith;

	for(;(iFoundIndex = m_sWorkBuffer.find(sSearchPattern, iOffset)) != KString::npos;)
	{
		CB_NEXT_OP next_op = foundHandler(
			m_sWorkBuffer, iFoundIndex, sSearchPattern.length(), sReplaceWith);

		if (next_op==RET_REPLACE)
		{
			iOffset = m_sWorkBuffer.InnerReplace(iFoundIndex, iFoundIndex + sSearchPattern.length(), sReplaceWith.c_str());
			++iNumReplacement;
		}
		else
		{
			iOffset = iFoundIndex + sSearchPattern.length();
		}
	}

	return iNumReplacement;
} // Replace

//----------------------------------------------------------------------
KString::size_type KStringBuffer::Replace(const KString& sSearchPattern, const KString& sWith)
//----------------------------------------------------------------------
{
	return Replace(sSearchPattern, SimpleReplacer(sWith));
} // Replace

//----------------------------------------------------------------------
void KString::RemoveIllegalChars(const KString& sIllegalChars)
//----------------------------------------------------------------------
{
	if (sIllegalChars.empty())
	{
		erase();
	}
	else
	{
		size_type pos;
		for(size_type lastpos = size();
			(pos = find_last_of(sIllegalChars, lastpos)) != npos;
			)
		{
			erase(pos, 1);
			lastpos = pos;
		}
	}
}

//-----------------------------------------------------------------------------
bool KString::In (const KString& sHaystack, value_type iDelim/*=','*/)
//-----------------------------------------------------------------------------
{
	auto& sNeedle{m_rep};

	size_t iNeedle = 0, iHaystack = 0; // Beginning indices
	size_t iNsize = sNeedle.size (), iHsize = sHaystack.size (); // Ending

	while (iHaystack < iHsize)
	{
		iNeedle = 0;

		// Search for matching tokens
		while ( (iNeedle < iNsize) &&
			(sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ((iNeedle >= iNsize) &&
			((sHaystack[iHaystack] == iDelim) || iHaystack >= iHsize))
		{
			return true;
		}

		// Advance to next delimiter
		while (iHaystack < iHsize && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// Pass by the delimiter if it exists
		iHaystack += (iHaystack < iHsize);
	}
	return false;

} // kstrin

//-----------------------------------------------------------------------------
bool KString::kstrin (const char* sNeedle, const char* sHaystack, char iDelim/*=','*/)
//-----------------------------------------------------------------------------
{
	if (!sHaystack || !sNeedle)
	{
		return false;
	}

	size_t iNeedle = 0, iHaystack = 0; // Beginning indices

	while (sHaystack[iHaystack])
	{
		iNeedle = 0;

		// Search for matching tokens
		while (  sNeedle[iNeedle] &&
			(sNeedle[iNeedle] == sHaystack[iHaystack]))
		{
			++iNeedle;
			++iHaystack;
		}

		// If end of needle or haystack at delimiter or end of haystack
		if ( !sNeedle[iNeedle] &&
			((sHaystack[iHaystack] == iDelim) || !sHaystack[iHaystack]))
		{
			return true;
		}

		// Advance to next delimiter
		while (sHaystack[iHaystack] && sHaystack[iHaystack] != iDelim)
		{
			++iHaystack;
		}

		// Pass by the delimiter if it exists
		iHaystack += (!!sHaystack[iHaystack]);
	}
	return false;

} // kstrin

} // end of namespace dekaf2


//----------------------------------------------------------------------
std::istream& std::getline(std::istream& stream, dekaf2::KString& str)
//----------------------------------------------------------------------
{
	return std::getline(stream, str.m_rep);
}

