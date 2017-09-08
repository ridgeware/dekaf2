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
#include "kstring.h"
#include "kstringutils.h"
#include "klog.h"
#include "kregex.h"

namespace dekaf2
{

const KString::size_type KString::npos;

//------------------------------------------------------------------------------
void KString::log_exception(std::exception& e, KStringView sWhere)
//------------------------------------------------------------------------------
{
	kException(e);
}

//------------------------------------------------------------------------------
KString& KString::append(const string_type& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.append(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::append(const std::string& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.append(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}
#endif

//------------------------------------------------------------------------------
KString& KString::assign(const string_type& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.assign(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::assign(const std::string& str, size_type pos, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.assign(str, pos, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}
#endif

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n, const string_type& str)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n, str);
	} catch (std::exception& e) {
		kException(e);
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
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::replace(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2)
//------------------------------------------------------------------------------
{
	try {
		// avoid segfaults
		if (pos2 > str.size())
		{
			kWarning("pos2 ({}) exceeds size ({})", pos2, str.size());
			pos2 = str.size();
		}
		if (pos2 + n2 > str.size())
		{
			kWarning("pos2 ({}) + n ({}) exceeds size ({})", pos2, n2, str.size());
			n2 = str.size() - pos2;
		}
		m_rep.replace(pos1, n1, str.data()+pos2, n2);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}

#endif

//------------------------------------------------------------------------------
KString& KString::replace(size_type pos, size_type n1, const value_type* s, size_type n2)
//------------------------------------------------------------------------------
{
	try {
		m_rep.replace(pos, n1, s ? s : "", n2);
	} catch (std::exception& e) {
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
	}
	return *this;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//------------------------------------------------------------------------------
KString& KString::insert(size_type pos1, const std::string& str, size_type pos2, size_type n)
//------------------------------------------------------------------------------
{
	try {
		// avoid segfaults
		if (pos2 > str.size())
		{
			kWarning("pos2 ({}) exceeds size ({})", pos2, str.size());
			pos2 = str.size();
		}
		if (pos2 + n > str.size())
		{
			kWarning("pos2 ({}) + n ({}) exceeds size ({})", pos2, n, str.size());
			n = str.size() - pos2;
		}
		m_rep.insert(pos1, str.data()+pos2, n);
	} catch (std::exception& e) {
		kException(e);
	}
	return *this;
}
#endif

//------------------------------------------------------------------------------
KString& KString::insert(size_type pos, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	try {
		m_rep.insert(pos, s ? s : "", n);
	} catch (std::exception& e) {
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
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
		kException(e);
	}
	return 1;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//----------------------------------------------------------------------
int KString::compare(size_type pos, size_type n, const std::string& str) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos, n, str);
	} catch (std::exception& e) {
		kException(e);
	}
	return 1;
}
#endif

//----------------------------------------------------------------------
int KString::compare(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos1, n1, str, pos2, n2);
	} catch (std::exception& e) {
		kException(e);
	}
	return 1;
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
//----------------------------------------------------------------------
int KString::compare(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos1, n1, str, pos2, n2);
	} catch (std::exception& e) {
		kException(e);
	}
	return 1;
}
#endif

//----------------------------------------------------------------------
int KString::compare(size_type pos, size_type n1, const value_type* s) const
//----------------------------------------------------------------------
{
	try {
		return m_rep.compare(pos, n1, s ? s : "");
	} catch (std::exception& e) {
		kException(e);
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
		kException(e);
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
		kException(e);
	}
	return 1;
}

//------------------------------------------------------------------------------
KString::size_type KString::find(const value_type* s, size_type pos, size_type n) const
//------------------------------------------------------------------------------
{
#if (DEKAF2_GCC_VERSION >= 40805) && (DEKAF2_USE_GLIBC_FOR_KSTRING_FIND)
	// GLIBC has a performant Boyer-Moore implementation for ::memmem, it
	// outperforms fbstring's simplyfied Boyer-Moore by one magnitude
	// (which means facebook uses it internally as well, as they mention a
	// search performance improvement by a factor of 30, but their code
	// in reality only improves search performance by a factor of 2
	// compared to std::string::find() - it is ::memmem() which brings it
	// to 30)
	if (DEKAF2_UNLIKELY(pos >= size()))
	{
		return npos;
	}
	auto found = static_cast<const value_type*>(::memmem(data() + pos, size() - pos, s, n));
	if (DEKAF2_UNLIKELY(!found))
	{
		return npos;
	}
	else
	{
		return static_cast<size_type>(found - data());
	}
#else
	return m_rep.find(s, pos, n);
#endif
}

//------------------------------------------------------------------------------
KString::size_type KString::Replace (KStringView sSearch, KStringView sReplace, bool bReplaceAll)
//------------------------------------------------------------------------------
{
	return dekaf2::kReplace(*this, sSearch, sReplace, bReplaceAll);
}

//----------------------------------------------------------------------
KString::size_type KString::ReplaceRegex(KStringView sRegEx, KStringView sReplaceWith, bool bReplaceAll)
//----------------------------------------------------------------------
{
	return dekaf2::KRegex::Replace(*this, sRegEx, sReplaceWith, bReplaceAll);
}

//----------------------------------------------------------------------
KStringView KString::ToView(size_type pos, size_type n) const
//----------------------------------------------------------------------
{
	if (pos > size())
	{
		kWarning("pos ({}) exceeds size ({})", pos, size());
		pos = size();
	}
	if (n == npos)
	{
		n = size() - pos;
	}
	else if (pos + n > size())
	{
		kWarning("pos ({}) + n ({}) exceeds size ({})", pos, n, size());
		n = size() - pos;
	}
	return KStringView(data() + pos, n);
}

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
KStringView KString::Left(size_type iCount)
//----------------------------------------------------------------------
{
	if (iCount >= size())
	{
		return *this;
	}
	return KStringView(m_rep.data(), iCount);
} // Left

//----------------------------------------------------------------------
KStringView KString::Right(size_type iCount)
//----------------------------------------------------------------------
{
	if (iCount >= size())
	{
		return *this;
	}
	else if (!iCount || !size())
	{
		// return an empty string
		return KStringView("");
	}
	else
	{
		return KStringView(m_rep.data() + size() - iCount, iCount);
	}
} // Right

//----------------------------------------------------------------------
KString& KString::PadLeft(size_t iWidth, value_type chPad)
//----------------------------------------------------------------------
{
	dekaf2::kPadLeft(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::PadRight(size_t iWidth, value_type chPad)
//----------------------------------------------------------------------
{
	dekaf2::kPadRight(m_rep, iWidth, chPad);
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft()
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrimLeft(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimLeft(KStringView sTrim)
//----------------------------------------------------------------------
{
	if (sTrim.size() == 1)
	{
		return TrimLeft(sTrim[0]);
	}
	dekaf2::kTrimLeft(m_rep, [sTrim](value_type ch){ return memchr(sTrim.data(), ch, sTrim.size()) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight()
//----------------------------------------------------------------------
{
	dekaf2::kTrimRight(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrimRight(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::TrimRight(KStringView sTrim)
//----------------------------------------------------------------------
{
	if (sTrim.size() == 1)
	{
		return TrimRight(sTrim[0]);
	}
	dekaf2::kTrimRight(m_rep, [sTrim](value_type ch){ return memchr(sTrim.data(), ch, sTrim.size()) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim()
//----------------------------------------------------------------------
{
	dekaf2::kTrim(m_rep, [](value_type ch){ return std::isspace(ch) != 0; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(value_type chTrim)
//----------------------------------------------------------------------
{
	dekaf2::kTrim(m_rep, [chTrim](value_type ch){ return ch == chTrim; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::Trim(KStringView sTrim)
//----------------------------------------------------------------------
{
	if (sTrim.size() == 1)
	{
		return Trim(sTrim[0]);
	}
	dekaf2::kTrim(m_rep, [sTrim](value_type ch){ return memchr(sTrim.data(), ch, sTrim.size()) != nullptr; } );
	return *this;
}

//----------------------------------------------------------------------
KString& KString::ClipAt(KStringView sClipAt)
//----------------------------------------------------------------------
{
	size_type pos = find(sClipAt);
	if (pos != npos)
	{
		erase(pos);
	}
	return *this;
} // ClipAt

//----------------------------------------------------------------------
KString& KString::ClipAtReverse(KStringView sClipAtReverse)
//----------------------------------------------------------------------
{
	size_type pos = find(sClipAtReverse);
	if (pos != npos)
	{
		erase(0, pos);
	}
	return *this;
} // ClipAtReverse

//----------------------------------------------------------------------
void KString::RemoveIllegalChars(KStringView sIllegalChars)
//----------------------------------------------------------------------
{
	size_type pos;
	for (size_type lastpos = size(); (pos = find_last_of(sIllegalChars, lastpos)) != npos; )
	{
		erase(pos, 1);
		lastpos = pos;
	}
}

//----------------------------------------------------------------------
bool kStartsWith(KStringView sInput, KStringView sPattern)
//----------------------------------------------------------------------
{
	if (sInput.size() < sPattern.size())
	{
		return false;
	}

	return !memcmp(sInput.data(), sPattern.data(), sPattern.size());

} // kStartsWith

//----------------------------------------------------------------------
bool kEndsWith(KStringView sInput, KStringView sPattern)
//----------------------------------------------------------------------
{
	if (sInput.size() < sPattern.size())
	{
		return false;
	}

	return !memcmp(sInput.data() + sInput.size() - sPattern.size(), sPattern.data(), sPattern.size());

} // kEndsWith

//-----------------------------------------------------------------------------
bool KString::In (KStringView sHaystack, value_type iDelim/*=','*/)
//-----------------------------------------------------------------------------
{
	// gcc 4.8.5 needs the non-brace initialization here..
	auto& sNeedle(m_rep);

	size_t iNeedle = 0, iHaystack = 0; // Beginning indices
	size_t iNsize = sNeedle.size ();
	size_t iHsize = sHaystack.size (); // Ending

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
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim/*=','*/)
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
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	dekaf2::KString::string_type& sref = str.str();
	return getline(stream, sref);
#else
	return std::getline(stream, str.str());
#endif
}

//----------------------------------------------------------------------
std::istream& std::getline(std::istream& stream, dekaf2::KString& str, dekaf2::KString::value_type delimiter)
//----------------------------------------------------------------------
{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	dekaf2::KString::string_type& sref = str.str();
	return getline(stream, sref, delimiter);
#else
	return std::getline(stream, str.str(), delimiter);
#endif
}

