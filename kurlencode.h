
#pragma once

#include "kstringview.h"
#include "kstring.h"
#include "kprops.h"
#include "bits/ktemplate.h"
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
		size_t which = std::min(static_cast<size_t>(part), TABLECOUNT - 1UL);
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
	int iValue{0};
	switch (pszGoop[0])
	{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			iValue += ((pszGoop[0] - '0') << 4);
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			iValue += ((pszGoop[0] - 'A' + 10) << 4);
			break;
	}
	switch (pszGoop[1])
	{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			iValue += ((pszGoop[1] - '0'));
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			iValue += ((pszGoop[1] - 'A' + 10));
			break;
	}

	return static_cast<Ch>(iValue);

} // kx2c

} // detail until here

//-----------------------------------------------------------------------------
/// decodes string in place
template<class String>
void kUrlDecode (String& sDecode, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
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

} // kUrlDecode

//-----------------------------------------------------------------------------
/// decodes string on a copy
template<class String>
void kUrlDecode (KStringView sSource, String& sTarget, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
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

} // kUrlDecode copy

//-----------------------------------------------------------------------------
template<class String>
String kUrlDecode (KStringView sSource, bool bPlusAsSpace = false)
//-----------------------------------------------------------------------------
{
	String sRet;
	kUrlDecode(sSource, sRet, bPlusAsSpace);
	return sRet;
}

//-----------------------------------------------------------------------------
template<class String>
void kUrlEncode (KStringView sSource, String& sTarget, const bool excludeTable[256], bool bSpaceAsPlus = false)
//-----------------------------------------------------------------------------
{
	static const unsigned char* sxDigit{
		reinterpret_cast<const unsigned char*>("0123456789ABCDEF")};
	size_t iSize = sSource.size();
	// Pre-allocate to prevent potential multiple re-allocations.
	sTarget.reserve (sTarget.size () + sSource.size ());
	for (size_t iIndex = 0; iIndex < iSize; ++iIndex)
	{
		unsigned char ch = static_cast<unsigned char>(sSource[iIndex]);
		// Do not encode either alnum or encoding excluded characters.
		if (isalnum (ch) || excludeTable[ch])
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
	KURLEncoded(URIPart encoding)
	//-------------------------------------------------------------------------
	    : m_Encoding(encoding)
	{
	}

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
	// the non-Key-Value decoding
	template<bool X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	KStringView getDecoded() const
	//-------------------------------------------------------------------------
	{
		if ((m_eState & (VALID | DECODED)) == VALID)
		{
			// have to decode..
			kUrlDecode(m_sEncoded, m_sDecoded);
			m_eState |= DECODED;
		}
		return m_sDecoded;
	}

	//-------------------------------------------------------------------------
	// the non-Key-Value encoding
	template<bool X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	KStringView getEncoded() const
	//-------------------------------------------------------------------------
	{
		if ((m_eState & MODIFIED) == MODIFIED)
		{
			m_sEncoded.clear();
			kUrlEncode (m_sDecoded, m_sEncoded, m_Encoding);
			m_eState |= ENCODED;
			m_eState &= ~MODIFIED;
		}

		return m_sEncoded;
	}

	//-------------------------------------------------------------------------
	// the Key-Value decoding
	template<bool X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	value_type& getDecoded()
	//-------------------------------------------------------------------------
	{
		if ((m_eState & (VALID | DECODED)) == VALID)
		{
			decode();
		}
		// need to set modified flag, as we handed out a non-const ref
		m_eState |= MODIFIED;
		return m_sDecoded;
	}

	//-------------------------------------------------------------------------
	// the Key-Value decoding
	template<bool X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	const value_type& getDecoded() const
	//-------------------------------------------------------------------------
	{
		if ((m_eState & (VALID | DECODED)) == VALID)
		{
			decode();
		}
		return m_sDecoded;
	}

	//-------------------------------------------------------------------------
	// the Key-Value encoding
	template<bool X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	KStringView getEncoded() const
	//-------------------------------------------------------------------------
	{
		if ((m_eState & MODIFIED) == MODIFIED)
		{
			m_sEncoded.clear();

			bool bSeparator = false;
			for (const auto& it : m_sDecoded)
			{
				if (bSeparator)
				{
					m_sEncoded += chPairSep;
				}
				else
				{
					bSeparator = true;
				}
				kUrlEncode (it.first, m_sEncoded, m_Encoding);
				m_sEncoded += chKeyValSep;
				kUrlEncode (it.second, m_sEncoded, m_Encoding);
			}

			m_eState |= ENCODED;
			m_eState &= ~MODIFIED;
		}

		return m_sEncoded;
	}

	//-------------------------------------------------------------------------
	template<bool X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void getDecoded(KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		sTarget += getDecoded();
	}

	//-------------------------------------------------------------------------
	void getEncoded(KString& sTarget) const
	//-------------------------------------------------------------------------
	{
		sTarget += getEncoded();
	}

	//-------------------------------------------------------------------------
	void setEncoded(KStringView sv)
	//-------------------------------------------------------------------------
	{
		// store original encoding
		if (sv.empty())
		{
			clear();
		}
		else
		{
			m_sEncoded = sv;
			m_eState = VALID;
		}
	}

	//-------------------------------------------------------------------------
	template<bool X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void setDecoded(KStringView sv)
	//-------------------------------------------------------------------------
	{
		if (sv != m_sDecoded || sv.empty())
		{
			if (sv.empty())
			{
				clear();
			}
			else
			{
				m_sDecoded = sv;
				m_eState = VALID | MODIFIED | DECODED;
			}
		}
	}

	//-------------------------------------------------------------------------
	bool empty() const
	//-------------------------------------------------------------------------
	{
		return m_eState == EMPTY;
	}

	//-------------------------------------------------------------------------
	void clear()
	//-------------------------------------------------------------------------
	{
		m_sDecoded.clear ();
		m_sEncoded.clear ();
		m_eState = EMPTY;
	}

	//-------------------------------------------------------------------------
	KURLEncoded& operator=(KStringView sv)
	//-------------------------------------------------------------------------
	{
		setDecoded(sv);
		return *this;
	}

	//-------------------------------------------------------------------------
	template<bool X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	friend bool operator==(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.getDecoded() == right.getDecoded();
	}

	//-------------------------------------------------------------------------
	template<bool X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	friend bool operator==(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		// this should rather be a comparison on the decoded version
		return left.getEncoded() == right.getEncoded();
	}

	//-------------------------------------------------------------------------
	friend bool operator!=(const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return !operator==(left, right);
	}

	//-------------------------------------------------------------------------
	template<bool X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	friend bool operator< (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return left.getDecoded() < right.getDecoded();
	}

	//-------------------------------------------------------------------------
	template<bool X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	friend bool operator< (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		// this should rather be a comparison on the decoded version
		return left.getEncoded() < right.getEncoded();
	}

	//-------------------------------------------------------------------------
	friend bool operator> (const self_type& left, const self_type& right)
	//-------------------------------------------------------------------------
	{
		return operator>(right, left);
	}

//------
protected:
//------

	//-------------------------------------------------------------------------
	// the Key-Value decoding
	template<bool X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	void decode() const
	//-------------------------------------------------------------------------
	{
		KStringView svQuery(m_sEncoded);

		while (!svQuery.empty())
		{
			// Get bounds of query pair
			auto iEnd = svQuery.find (chPairSep); // Find separator
			if (iEnd == KString::npos)
			{
				iEnd = svQuery.size();
			}

			KStringView svEncoded{svQuery.substr (0, iEnd)};

			auto iEquals = svEncoded.find (chKeyValSep);
			if (iEquals > iEnd)
			{
				iEquals = iEnd;
			}

			KStringView svKeyEncoded (svEncoded.substr (0          , iEquals));
			KStringView svValEncoded (svEncoded.substr (iEquals + 1         ));

			// we can have empty values
			if (svKeyEncoded.size () /* && svValEncoded.size () */ )
			{
				// decoding may only happen AFTER '=' '&' detections
				KString sKey, sVal;
				kUrlDecode (svKeyEncoded, sKey, m_Encoding);
				kUrlDecode (svValEncoded, sVal, m_Encoding);
				m_sDecoded.Add (std::move (sKey), std::move (sVal));
			}

			svQuery.remove_prefix(iEnd + 1);
		}

		m_eState |= DECODED;
	}

	enum eState : uint8_t
	{
		EMPTY    = 0,
		VALID    = 1 << 0,
		MODIFIED = 1 << 1,
		DECODED  = 1 << 2,
		ENCODED  = 1 << 3
	};

	mutable Decoded m_sDecoded {};
	mutable KString m_sEncoded {};
	mutable uint8_t m_eState { EMPTY };
	URIPart m_Encoding;

}; // KURLEncoded

using URLEncodedString    = KURLEncoded<KString>;
using URLEncodedQuery     = KURLEncoded<KProps<KString, KString>, '&', '='>;
using URLEncodedPathParam = KURLEncoded<KProps<KString, KString>, ';', '='>;

} // end of namespace dekaf2
