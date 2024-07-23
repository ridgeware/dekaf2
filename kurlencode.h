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

/// @file kurlencode.h
/// percent-encoding methods

#include "kstringview.h"
#include "kstring.h"
#include "kprops.h"
#include "ktemplate.h"
#include "kwriter.h"
#include <cinttypes>
#include "kctype.h"


DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the different URL components
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
/// the encoding tables used for the different URL components
class DEKAF2_PUBLIC KUrlEncodingTables
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KUrlEncodingTables() noexcept;
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

	static constexpr int INT_TABLECOUNT = 3;
	static constexpr int TABLECOUNT = 8;
	static KUrlEncodingTables MyInstance;
	static const char* s_sExcludes[];
	static bool* EncodingTable[TABLECOUNT];
	static bool Tables[INT_TABLECOUNT][256];
};

} // end of namespace detail


namespace detail {

//-----------------------------------------------------------------------------
int kx2c (int c1, int c2);
//-----------------------------------------------------------------------------

} // detail until here

//-----------------------------------------------------------------------------
/// percent-decodes string in place
template<class String>
void kUrlDecode (String& sDecode, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
{
	if (!sDecode.empty())
	{
		auto insert  = sDecode.begin();
		auto current = insert;
		auto end     = sDecode.end();

		while (current < end)
		{
			if (*current == '+')
			{
				*insert++   = bPlusAsSpace ? ' ' : *current;
				++current;
			}
			else if (*current == '%'
				&& end - current > 2
				&& KASCII::kIsXDigit(*(current + 1))
				&& KASCII::kIsXDigit(*(current + 2)))
			{
				auto iValue = detail::kx2c(*(current + 1), *(current + 2));
				*insert++   = static_cast<typename String::value_type>(iValue);
				current    += 3;
			}
			else
			{
				*insert++   = *current++;
			}
		}

		if (insert < end)
		{
			size_t nsz = insert - sDecode.begin();
			sDecode.erase(nsz);
		}
	}

} // kUrlDecode

//-----------------------------------------------------------------------------
/// percent-decodes string to a copy
template<class String, class StringView>
void kUrlDecode (const StringView& sSource, String& sTarget, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
{
	if (!sSource.empty())
	{
		sTarget.reserve (sTarget.size() + sSource.size());
		auto current = sSource.begin();
		auto end     = sSource.end();

		while (current < end)
		{
			if (*current == '+')
			{
				sTarget    += bPlusAsSpace ? ' ' : *current;
				++current;
			}
			else if (*current == '%'
				&& end - current > 2
				&& KASCII::kIsXDigit(*(current + 1))
				&& KASCII::kIsXDigit(*(current + 2)))
			{
				auto iValue = detail::kx2c(*(current + 1), *(current + 2));
				sTarget    += static_cast<typename String::value_type>(iValue);
				current    += 3;
			}
			else
			{
				sTarget    += *current++;
			}
		}
	}

} // kUrlDecode copy

//-----------------------------------------------------------------------------
/// percent-decodes string
/// @param sSource the encoded string
/// @param bPlusAsSpace if true, a + sign will be translated as space, default is false
/// @return the decoded string
template<class String = KString, class StringView = KStringView>
DEKAF2_NODISCARD
String kUrlDecode (const StringView& sSource, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
{
	String sRet;
	kUrlDecode(sSource, sRet, bPlusAsSpace);
	return sRet;
}

//-----------------------------------------------------------------------------
/// percent-encodes string
/// @param sSource the unencoded input string
/// @param sTarget the encoded output string
/// @param excludeTable pointer on a table with ASCII chars that are not to be percent encoded
/// @param bSpaceAsPlus if true, a space will be translated as + sign, default is false
template<class String, class StringView>
void kUrlEncode (const StringView& sSource, String& sTarget, const bool excludeTable[256], bool bSpaceAsPlus = false)
//-----------------------------------------------------------------------------
{
	static constexpr char sxDigit[] = "0123456789ABCDEF";

	size_t iSize = sSource.size();
	// Pre-allocate to prevent potential multiple re-allocations.
	sTarget.reserve (sTarget.size () + sSource.size ());
	for (size_t iIndex = 0; iIndex < iSize; ++iIndex)
	{
		auto ch = static_cast<unsigned char>(sSource[iIndex]);
		// Do not encode either alnum or encoding excluded characters.
		if (excludeTable[ch])
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
			sTarget += sxDigit[(ch >> 4) & 0x0f];
			sTarget += sxDigit[(ch     ) & 0x0f];
		}
	}
}

//-----------------------------------------------------------------------------
/// percent-decode in place
/// @param sTarget the input/output string
/// @param encoding the URIPart component, used to determine the applicable exclusion table for the conversion
template<class String>
void kUrlDecode (String& sTarget, URIPart encoding)
//-----------------------------------------------------------------------------
{
	kUrlDecode(sTarget, encoding == URIPart::Query);
}

//-----------------------------------------------------------------------------
/// percent-decode
/// @param sSource the input string
/// @param sTarget the output string
/// @param encoding the URIPart component, used to determine the applicable exclusion table for the conversion
template<class String>
void kUrlDecode (KStringView sSource, String& sTarget, URIPart encoding)
//-----------------------------------------------------------------------------
{
	kUrlDecode(sSource, sTarget, encoding == URIPart::Query);
}

//-----------------------------------------------------------------------------
/// percent-encode
/// @param sSource the input string
/// @param sTarget the output string
/// @param encoding the URIPart component, used to determine the applicable exclusion table for the conversion,
/// defaults to URIPart::Protocol and hence encodes most defensively per default
template<class String>
void kUrlEncode (KStringView sSource, String& sTarget, URIPart encoding = URIPart::Protocol)
//-----------------------------------------------------------------------------
{
	kUrlEncode(sSource, sTarget, detail::KUrlEncodingTables::getTable(encoding), encoding == URIPart::Query);
}

//-----------------------------------------------------------------------------
/// percent-encode
/// @param sSource the input string
/// @param encoding the URIPart component, used to determine the applicable exclusion table for the conversion,
/// defaults to URIPart::Protocol and hence encodes most defensively per default
/// @return the output string
template<class String = KString>
DEKAF2_NODISCARD
String kUrlEncode (KStringView sSource, URIPart encoding = URIPart::Protocol)
//-----------------------------------------------------------------------------
{
	String sTarget;
	kUrlEncode(sSource, sTarget, encoding);
	return sTarget;
}

/// checks if an IP address is a IPv6 address like '[a0:ef::c425:12]'
/// @param sAddress the string to test
/// @param bNeedsBraces if true, address has to be in square braces [ ], if false they must not be present
/// @return true if sAddress holds an IPv6 numerical address
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsIPv6Address(KStringView sAddress, bool bNeedsBraces);

/// checks if an IP address is a IPv4 address like '1.2.3.4'
/// @param sAddress the string to test
/// @return true if sAddress holds an IPv4 numerical address
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kIsIPv4Address(KStringView sAddress);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// wrapper for types that have a percent-decoded and percent-encoded form
template<
	typename Decoded,
	const char chPairSep = '\0',
	const char chKeyValSep = '\0',
	bool bIsPod = DEKAF2_PREFIX detail::is_pod<Decoded>::value
>
class KURLEncoded
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self_type      = KURLEncoded<Decoded, chPairSep, chKeyValSep>;
	using value_type     = Decoded;

	//-------------------------------------------------------------------------
	/// default ctor
	KURLEncoded() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	/// return into a string reference
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void get(KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		sTarget += m_sDecoded;
	}

	//-------------------------------------------------------------------------
	/// return the decoded value
	DEKAF2_NODISCARD
	value_type& get()
	//-------------------------------------------------------------------------
	{
		return m_sDecoded;
	}

	//-------------------------------------------------------------------------
	/// return the (const) decoded value
	DEKAF2_NODISCARD
	const value_type& get() const
	//-------------------------------------------------------------------------
	{
		return m_sDecoded;
	}

	//-------------------------------------------------------------------------
	// set the decoded value
	void set(value_type value)
	//-------------------------------------------------------------------------
	{
		m_sDecoded = std::move(value);
	}

	//-------------------------------------------------------------------------
	/// serialize into a string
	// the non-Key-Value POD encoding
	template<const char X = chPairSep, bool P = bIsPod, typename std::enable_if<X == '\0' && P == true, int>::type = 0>
	void Serialize(KStringRef& sEncoded, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		sEncoded += KString::to_string(m_sDecoded);
	}

	//-------------------------------------------------------------------------
	/// serialize into a string
	// the non-Key-Value non-POD encoding
	template<const char X = chPairSep, bool P = bIsPod, typename std::enable_if<X == '\0' && P == false, int>::type = 0>
	void Serialize(KStringRef& sEncoded, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		if (Component == URIPart::Domain && kIsIPv6Address(m_sDecoded, true))
		{
			// an IPv6 address
			sEncoded = m_sDecoded;
		}
		else
		{
			kUrlEncode (m_sDecoded, sEncoded, Component);
		}
	}

	//-------------------------------------------------------------------------
	/// serialize into a string
	// the Key-Value encoding
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	void Serialize(KStringRef& sEncoded, URIPart Component) const
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
	/// serialize into a string
	DEKAF2_NODISCARD
	KString Serialize(URIPart Component) const
	//-------------------------------------------------------------------------
	{
		KString sEncoded;
		Serialize(sEncoded, Component);
		return sEncoded;
	}

	//-------------------------------------------------------------------------
	/// serialize into a stream
	void Serialize(KOutStream& sTarget, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		sTarget += Serialize(Component);
	}

	//-------------------------------------------------------------------------
	/// parse from a string
	template<const char X = chPairSep, bool P = bIsPod, typename std::enable_if<X == '\0' && P == true, int>::type = 0>
	void Parse(KStringView sv, URIPart Component)
	//-------------------------------------------------------------------------
	{
		m_sDecoded = static_cast<Decoded>(sv.UInt64());
	}

	//-------------------------------------------------------------------------
	/// parse from a string
	template<const char X = chPairSep, bool P = bIsPod, typename std::enable_if<X == '\0' && P == false, int>::type = 0>
	void Parse(KStringView sv, URIPart Component)
	//-------------------------------------------------------------------------
	{
		kUrlDecode(sv, m_sDecoded);
	}

	//-------------------------------------------------------------------------
	// the Key-Value decoding
	/// parse from a string
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
			if (!svKeyEncoded.empty () /* && svValEncoded.size () */ )
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
	/// return the begin iterator
	template<bool X = bIsPod, typename std::enable_if<!X, int>::type = 0 >
	DEKAF2_NODISCARD
	auto begin()
	//-------------------------------------------------------------------------
	{
		return m_sDecoded.begin();
	}

	//-------------------------------------------------------------------------
	/// return the end iterator
	template<bool X = bIsPod, typename std::enable_if<!X, int>::type = 0 >
	DEKAF2_NODISCARD
	auto end()
	//-------------------------------------------------------------------------
	{
		return m_sDecoded.end();
	}

	//-------------------------------------------------------------------------
	/// return the begin iterator
	template<bool X = bIsPod, typename std::enable_if<!X, int>::type = 0 >
	DEKAF2_NODISCARD
	auto begin() const
	//-------------------------------------------------------------------------
	{
		return m_sDecoded.begin();
	}

	//-------------------------------------------------------------------------
	/// return the const iterator
	template<bool X = bIsPod, typename std::enable_if<!X, int>::type = 0 >
	DEKAF2_NODISCARD
	auto end() const
	//-------------------------------------------------------------------------
	{
		return m_sDecoded.end();
	}

	//-------------------------------------------------------------------------
	/// set the value from a string
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void set(KStringView sv)
	//-------------------------------------------------------------------------
	{
		m_sDecoded = sv;
	}

	//-------------------------------------------------------------------------
	/// is this object empty?
	template<bool X = bIsPod, typename std::enable_if<X == false, int>::type = 0>
	DEKAF2_NODISCARD
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return m_sDecoded.empty();
	}

	//-------------------------------------------------------------------------
	/// is this object empty?
	template<bool X = bIsPod, typename std::enable_if<X == true, int>::type = 0>
	DEKAF2_NODISCARD
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return m_sDecoded == Decoded{};
	}

	//-------------------------------------------------------------------------
	/// clear this object
	template<bool X = bIsPod, typename std::enable_if<X == false, int>::type = 0>
	void clear()
	//-------------------------------------------------------------------------
	{
		m_sDecoded.clear ();
	}

	//-------------------------------------------------------------------------
	/// clear this object
	template<bool X = bIsPod, typename std::enable_if<X == true, int>::type = 0>
	void clear()
	//-------------------------------------------------------------------------
	{
		m_sDecoded = Decoded{};
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
	friend bool operator< (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.get() < right.get();
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

#ifndef _MSC_VER
extern template void kUrlDecode(KStringRef& sDecode, bool pPlusAsSpace = false);
extern template void kUrlDecode(const KStringView& sSource, KStringRef& sTarget, bool bPlusAsSpace = false);
extern template KString kUrlDecode(const KStringView& sSource, bool bPlusAsSpace = false);
extern template void kUrlEncode (const KStringView& sSource, KStringRef& sTarget, const bool excludeTable[256], bool bSpaceAsPlus = false);

extern template class KURLEncoded<uint16_t>;
extern template class KURLEncoded<KString>;
extern template class KURLEncoded<KProps<KString, KString, true, false>, '&', '='>;
#endif // of _MSC_VER

using URLEncodedUInt   = KURLEncoded<uint16_t>;
using URLEncodedString = KURLEncoded<KString>;
using URLEncodedQuery  = KURLEncoded<KProps<KString, KString, /*Sequential=*/true, /*Unique=*/false>, '&', '='>;

DEKAF2_NAMESPACE_END
