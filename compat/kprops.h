/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

#include "kconfiguration.h"

#ifdef DEKAF1_INCLUDE_PATH

#include "../kprops.h"
#include "../ksplit.h"
#include "../kurl.h"

#include DEKAF2_stringify(DEKAF1_INCLUDE_PATH/dekaf.h)

namespace dekaf2 {
namespace compat {


// following are all adaptors for KProps that are needed to match the old KPROPS

template<class Key, class Value, bool Sequential=true, bool Unique=true>
class KProps : public dekaf2::KProps<Key, Value, Sequential, Unique>
{
public:
	using base_type = dekaf2::KProps<Key, Value, Sequential, Unique>;

	inline size_t GetNumPairs() const
	{
		return base_type::size();
	}
	inline void TruncateList()
	{
		base_type::clear();
	}
	inline const typename Key::value_type* Find(const typename Key::value_type* key) const
	{
		return base_type::Get(key).c_str();
	}
	inline const Value& Find(const Value& key) const
	{
		return base_type::Get(key);
	}
	inline bool Exists(const Value& key) const
	{
		return base_type::Count(key) > 0;
	}
	long long Increment(const Key& sName, long long iDelta=1)
	{
		long long iValue = std::strtoll(base_type::Get(sName).c_str(), nullptr, 10);
		iValue += iDelta;
		Add(sName, iValue);
		return iValue;
	}
	inline long long Decrement(const Key& sName, long long iDelta=-1)
	{
		return Increment(iDelta);
	}
	bool MemoryLoad(const char* pszBuffer, int chDelim = '=')
	{
		size_t      iErrors{0};
		const char* pszLineBegin{pszBuffer};
		const char* pszLineEnd{pszLineBegin};
		size_t      iLineNo{0};
		Value       sLine;

		while (pszLineBegin && ((pszLineEnd = strchr(pszLineBegin, '\n'))
			|| (strlen(pszLineBegin) > 0 && (pszLineEnd = pszLineBegin + strlen(pszLineBegin)))))
		{
			++iLineNo;
			sLine.assign(pszLineBegin, pszLineEnd - pszLineBegin);
			pszLineBegin = *pszLineEnd == '\0' ? nullptr : pszLineEnd + 1;

			sLine.TrimRight("\r");
			sLine.TrimLeft(" \t");

			if (sLine.empty())
			{
				continue;
			}

			if (sLine[0] == '#')
			{
				continue;
			}

			const char* szLine   = sLine.c_str();
			const char* pszDelim = strchr (szLine, chDelim);
			if (!pszDelim)
			{
				continue;
			}

			const char* pszName  = szLine;
			const char* pszValue = pszDelim + 1;

			Key sKeyDecoded(pszName, static_cast<dekaf2::compat::KString::size_type>(pszDelim - pszName));
			dekaf2::kUrlDecode(sKeyDecoded);

			if (!Add(std::move(sKeyDecoded), pszValue))
			{
				++iErrors;
			}
		}

		return (iErrors == 0);
	}
	/**
	 * Produces all data in the kprops structure into one string
	 * @param pszDelim1 The first delimiter that is used, the default value is '&'
	 * @param pszDelim2 The second delimiter that is used, the default value is '='
	 * @param pszPrefix The value is placed at the beginning of the string.
	 * @param pszSuffix The value is palced at the end of the string.
	 * @return The string will contain the full glob
	 */
	Key GlobToString(const char* pszDelim1="&", const char* pszDelim2="=", const char* pszPrefix="", const char* pszSuffix="") const
	{
		Key sString(pszPrefix);
		for (const auto& it : *this)
		{
			if (!sString.empty())
			{
				sString.append(pszDelim1);
			}
			sString.append(it.first);
			sString.append(pszDelim2);
			sString.append(it.second);
		}
		sString.append(pszSuffix);
		return sString;
	}
	bool Add(const Key& key, long long iValue)
	{
		return Add(key, std::to_string(iValue));
	}
	inline bool Add(const KProps& other)
	{
		base_type::operator+=(other);
		return true;
	}
	inline bool Add(const KProps* other)
	{
		base_type::operator+=(*other);
		return true;
	}
	bool Add(const KPROPS& other)
	{
		for (size_t ii=0; ii < other.GetNumPairs(); ++ii)
		{
			if (!Add(other.GetName(ii), other.GetValue(ii)))
			{
				return false;
			}
		}
		return true;
	}
	inline bool Add(const Key& key)
	{
		base_type::Add(key, Value{});
		return true;
	}
	inline bool Add(const Key& key, const Value& value)
	{
		base_type::Add(key, value);
		return true;
	}
	template<class K, class V>
	inline bool UniqueAddFast(K&& key, V&& value)
	{
		base_type::Add(std::forward<K>(key), std::forward<V>(value));
		return true;
	}
};

} // end of namespace compat
} // end of namespace dekaf2

//-----------------------------------------------------------------------------
template <class String, class Props>
 unsigned int kParseDelimedList(const String& sBuffer, Props& PropsList, int chDelim=',', bool bTrim=TRUE, bool bCombineDelimiters=FALSE, int chEscape='\0')
//inline unsigned int kParseDelimedList(const MyKString& sBuffer, MyKPROPS& PropsList, int chDelim=',', bool bTrim=TRUE, bool bCombineDelimiters=FALSE, int chEscape='\0')
//-----------------------------------------------------------------------------
{
	char delim(chDelim);
	return dekaf2::kSplit(PropsList,
	                      sBuffer,
	                      dekaf2::KStringView(&delim, 1),
	                      bTrim ? " \t\r\n" : "",
	                      chEscape);
} // kParseDelimedList

#endif // of DEKAF1_INCLUDE_PATH
