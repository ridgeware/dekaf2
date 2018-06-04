/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kstringutils.h"
#include "kstringview.h"
#include "kstring.h"
#include "kprops.h"
#include "bits/ktemplate.h"
#include "kwriter.h"
#include <cinttypes>


namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
enum class URIPart : uint8_t
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	Protocol       = 0,
	User           = 1,
	Password       = 2,
	Domain         = 3,
	Port           = 4,
	Path           = 5,
	Query          = 6,
	Fragment       = 7
};

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KUrlEncodingTables
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KUrlEncodingTables();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static inline bool* getTable(URIPart part)
	//-----------------------------------------------------------------------------
	{
		return MyInstance.getTableInt(part);
	}

//------
protected:
//------

	//-----------------------------------------------------------------------------
	inline bool* getTableInt(URIPart part)
	//-----------------------------------------------------------------------------
	{
		std::size_t which = std::min(static_cast<std::size_t>(part), static_cast<std::size_t>(TABLECOUNT - 1));
		return EncodingTable[which];
	}

	enum { INT_TABLECOUNT = 3, TABLECOUNT = 8 };
	static KUrlEncodingTables MyInstance;
	static const char* s_sExcludes[];
	static bool* EncodingTable[TABLECOUNT];
	static bool Tables[INT_TABLECOUNT][256];
};

} // end of namespace detail


namespace detail // hide kx2c in an anonymous namespace
{

//-----------------------------------------------------------------------------
template<class Ch>
inline Ch kx2c (Ch* pszGoop)
//-----------------------------------------------------------------------------
{
	auto iValue = kFromHexChar(pszGoop[0]) << 4;
	iValue += kFromHexChar(pszGoop[1]);

	return static_cast<Ch>(iValue);

} // kx2c

} // detail until here

//-----------------------------------------------------------------------------
/// decodes string in place
template<class String>
void kUrlDecode (String& sDecode, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
{
	if (!sDecode.empty())
	{
		auto insert  = &sDecode[0];
		auto current = insert;
		auto end     = current + sDecode.size();
		while (current < end)
		{
			if (*current == '+')
			{
				if (bPlusAsSpace)
				{
					*insert++ = ' ';
					++current;
				}
				else
				{
					*insert++ = *current++;
				}
			}
			else if (*current == '%'
				&& end - current > 2
				&& std::isxdigit(*(current + 1))
				&& std::isxdigit(*(current + 2)))
			{
				*insert++ = detail::kx2c(current + 1);
				current += 3;
			}
			else
			{
				*insert++ = *current++;
			}
		}

		if (insert < end)
		{
			size_t nsz = insert - &sDecode[0];
			sDecode.erase(nsz);
		}
	}

} // kUrlDecode

extern template void kUrlDecode(KString& sDecode, bool pPlusAsSpace = false);

//-----------------------------------------------------------------------------
/// decodes string on a copy
template<class String>
void kUrlDecode (KStringView sSource, String& sTarget, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
{
	if (!sSource.empty())
	{
		sTarget.reserve (sTarget.size ()+sSource.size ());
		auto current = &sSource[0];
		auto end     = current + sSource.size();
		while (current < end)
		{
			if (*current == '+')
			{
				if (bPlusAsSpace)
				{
					sTarget += ' ';
					++current;
				}
				else
				{
					sTarget += *current++;
				}
			}
			else if (*current == '%'
				&& end - current > 2
				&& std::isxdigit(*(current + 1))
				&& std::isxdigit(*(current + 2)))
			{
				sTarget += detail::kx2c(current + 1);
				current += 3;
			}
			else
			{
				sTarget += *current++;
			}
		}
	}

} // kUrlDecode copy

extern template void kUrlDecode(KStringView sSource, KString& sTarget, bool bPlusAsSpace = false);

//-----------------------------------------------------------------------------
template<class String>
String kUrlDecode (KStringView sSource, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
{
	String sRet;
	kUrlDecode(sSource, sRet, bPlusAsSpace);
	return sRet;
}

extern template KString kUrlDecode(KStringView sSource, bool bPlusAsSpace = false);

//-----------------------------------------------------------------------------
template<class String>
void kUrlEncode (KStringView sSource, String& sTarget, const bool excludeTable[256], bool bSpaceAsPlus = false)
//-----------------------------------------------------------------------------
{
	static constexpr char sxDigit[] = "0123456789ABCDEF";

	size_t iSize = sSource.size();
	// Pre-allocate to prevent potential multiple re-allocations.
	sTarget.reserve (sTarget.size () + sSource.size ());
	for (size_t iIndex = 0; iIndex < iSize; ++iIndex)
	{
		unsigned char ch = static_cast<unsigned char>(sSource[iIndex]);
		// Do not encode either alnum or encoding excluded characters.
		if ((!(ch & ~0x7f) && std::isalnum (ch)) || excludeTable[ch])
		{
			sTarget += ch;
		}
		else if (ch == ' ' && bSpaceAsPlus)
		{
			// it is only in the query part of a URL that
			// spaces _may_ be represented as +, but they do not
			// have to..
			sTarget += '+';
		}
		else
		{
			sTarget += '%';
			sTarget += static_cast<char> (sxDigit[(ch>>4)&0xf]);
			sTarget += static_cast<char> (sxDigit[(ch   )&0xf]);
		}
	}
}

extern template void kUrlEncode (KStringView sSource, KString& sTarget, const bool excludeTable[256], bool bSpaceAsPlus = false);

//-----------------------------------------------------------------------------
template<class String>
void kUrlEncode (KStringView sSource, String& sTarget, KStringView svExclude=KStringView{}, bool bSpaceAsPlus = false)
//-----------------------------------------------------------------------------
{
	// to exclude encoding of special characters, make a table of exclusions.
	bool bExcludeTable[256] = {false};

	for (auto iC: svExclude)
	{
		bExcludeTable[static_cast<unsigned char>(iC)] = true;
	}

	kUrlEncode (sSource, sTarget, reinterpret_cast<const bool*>(&bExcludeTable), bSpaceAsPlus);
}

//-----------------------------------------------------------------------------
template<class String>
void kUrlDecode (String& sTarget, URIPart encoding)
//-----------------------------------------------------------------------------
{
	kUrlDecode(sTarget, encoding == URIPart::Query);
}

//-----------------------------------------------------------------------------
template<class String>
void kUrlDecode (KStringView sSource, String& sTarget, URIPart encoding)
//-----------------------------------------------------------------------------
{
	kUrlDecode(sSource, sTarget, encoding == URIPart::Query);
}

//-----------------------------------------------------------------------------
template<class String>
void kUrlEncode (KStringView sSource, String& sTarget, URIPart encoding)
//-----------------------------------------------------------------------------
{
	kUrlEncode(sSource, sTarget, detail::KUrlEncodingTables::getTable(encoding), encoding == URIPart::Query);
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Decoded, const char chPairSep = '\0', const char chKeyValSep = '\0'>
class KURLEncoded
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self_type  = KURLEncoded<Decoded, chPairSep, chKeyValSep>;
	using value_type = Decoded;

	//-------------------------------------------------------------------------
	KURLEncoded() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLEncoded(const KURLEncoded&) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLEncoded(KURLEncoded&& other) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLEncoded& operator=(const KURLEncoded&) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLEncoded& operator=(KURLEncoded&& other) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void get(KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		sTarget += m_sDecoded;
	}

	//-------------------------------------------------------------------------
	value_type& get()
	//-------------------------------------------------------------------------
	{
		return m_sDecoded;
	}

	//-------------------------------------------------------------------------
	const value_type& get() const
	//-------------------------------------------------------------------------
	{
		return m_sDecoded;
	}

	//-------------------------------------------------------------------------
	// the non-Key-Value encoding
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void Serialize(KString& sEncoded, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		kUrlEncode (m_sDecoded, sEncoded, Component);
	}

	//-------------------------------------------------------------------------
	// the Key-Value encoding
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	void Serialize(KString& sEncoded, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		bool bSeparator = false;
		for (const auto& it : m_sDecoded)
		{
			if (bSeparator)
			{
				sEncoded += chPairSep;
			}
			else
			{
				bSeparator = true;
			}
			kUrlEncode (it.first, sEncoded, Component);
			if (!it.second.empty())
			{
				sEncoded += chKeyValSep;
				kUrlEncode (it.second, sEncoded, Component);
			}
		}
	}

	//-------------------------------------------------------------------------
	KString Serialize(URIPart Component) const
	//-------------------------------------------------------------------------
	{
		KString sEncoded;
		Serialize(sEncoded, Component);
		return sEncoded;
	}

	//-------------------------------------------------------------------------
	void Serialize(KOutStream& sTarget, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		sTarget += Serialize(Component);
	}

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void Parse(KStringView sv, URIPart Component)
	//-------------------------------------------------------------------------
	{
		kUrlDecode(sv, m_sDecoded);
	}

	//-------------------------------------------------------------------------
	// the Key-Value decoding
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	void Parse(KStringView sv, URIPart Component)
	//-------------------------------------------------------------------------
	{
		while (!sv.empty())
		{
			KStringView svEncoded;

			// Get bounds of query pair
			auto iEnd = sv.find (chPairSep); // Find separator
			if (iEnd == KString::npos)
			{
				iEnd = sv.size();
				svEncoded = sv.substr(0, iEnd);
				sv.remove_prefix(iEnd);
			}
			else
			{
				svEncoded = sv.substr(0, iEnd);
				sv.remove_prefix(iEnd+1);
			}

			auto iEquals = svEncoded.find (chKeyValSep);
			if (iEquals > iEnd)
			{
				iEquals = iEnd;
			}

			KStringView svKeyEncoded (svEncoded.substr (0, iEquals));
			KStringView svValEncoded (iEquals < svEncoded.size() ? svEncoded.substr (iEquals + 1) : KStringView{});

			// we can have empty values
			if (svKeyEncoded.size () /* && svValEncoded.size () */ )
			{
				// decoding may only happen AFTER '=' '&' detections
				KString sKey, sVal;
				kUrlDecode (svKeyEncoded, sKey, Component);
				kUrlDecode (svValEncoded, sVal, Component);
				m_sDecoded.Add (std::move (sKey), std::move (sVal));
			}
		}
	}

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void set(KStringView sv)
	//-------------------------------------------------------------------------
	{
		m_sDecoded = sv;
	}

	//-------------------------------------------------------------------------
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return m_sDecoded.empty();
	}

	//-------------------------------------------------------------------------
	void clear()
	//-------------------------------------------------------------------------
	{
		m_sDecoded.clear ();
	}

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	KURLEncoded& operator=(KStringView sv)
	//-------------------------------------------------------------------------
	{
		set(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	friend bool operator==(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.get() == right.get();
	}

	//-------------------------------------------------------------------------
	friend bool operator!=(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return !operator==(left, right);
	}

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	friend bool operator< (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.get() < right.get();
	}

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	friend bool operator< (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		// this should rather be a comparison on the decoded version
		return left.Serialize() < right.Serialize();
	}

	//-------------------------------------------------------------------------
	friend bool operator> (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return operator<(right, left);
	}

//------
protected:
//------

	Decoded m_sDecoded {};

}; // KURLEncoded

#ifndef __clang__
extern template class KURLEncoded<KString>;
extern template class KURLEncoded<KProps<KString, KString>, '&', '='>;
#endif

using URLEncodedString = KURLEncoded<KString>;
using URLEncodedQuery  = KURLEncoded<KProps<KString, KString>, '&', '='>;

} // end of namespace dekaf2
