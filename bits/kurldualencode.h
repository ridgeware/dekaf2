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

/// @file kurldualencode.h
/// A class to maintain both decoded and encoded versions of a URL string
/// in one storage place. Rather a design study, not for general usage.


#include "../kurlencode.h"
#include "../kstringutils.h"
#include "../kstringview.h"
#include "../kstring.h"
#include "../kprops.h"
#include "../ktemplate.h"
#include "../kwriter.h"
#include <cinttypes>


DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename Decoded, const char chPairSep = '\0', const char chKeyValSep = '\0'>
class KURLDualEncoded
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self_type  = KURLDualEncoded<Decoded, chPairSep, chKeyValSep>;
	using value_type = Decoded;

	//-------------------------------------------------------------------------
	KURLDualEncoded() = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLDualEncoded(const KURLDualEncoded&) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLDualEncoded(KURLDualEncoded&& other) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLDualEncoded& operator=(const KURLDualEncoded&) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	KURLDualEncoded& operator=(KURLDualEncoded&& other) = default;
	//-------------------------------------------------------------------------

	//-------------------------------------------------------------------------
	// the non-Key-Value decoding
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	const KString& get() const
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
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	const KString& Serialize(URIPart Component) const
	//-------------------------------------------------------------------------
	{
		m_Component = Component;
		if ((m_eState & MODIFIED) == MODIFIED)
		{
			m_sEncoded.clear();
			kUrlEncode (m_sDecoded, m_sEncoded, m_Component);
			m_eState |= ENCODED;
			m_eState &= ~MODIFIED;
		}

		return m_sEncoded;
	}

	//-------------------------------------------------------------------------
	// the Key-Value decoding
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	value_type& get()
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
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	const value_type& get() const
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
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
	const KString& Serialize(URIPart Component) const
	//-------------------------------------------------------------------------
	{
		m_Component = Component;
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
				kUrlEncode (it.first, m_sEncoded, m_Component);
				m_sEncoded += chKeyValSep;
				kUrlEncode (it.second, m_sEncoded, m_Component);
			}

			m_eState |= ENCODED;
			m_eState &= ~MODIFIED;
		}

		return m_sEncoded;
	}

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void get(KStringRef& sTarget) const
	//-------------------------------------------------------------------------
	{
		sTarget += get();
	}

	//-------------------------------------------------------------------------
	void Serialize(KStringRef& sTarget, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		sTarget += Serialize(Component);
	}

	//-------------------------------------------------------------------------
	void Serialize(KOutStream& sTarget, URIPart Component) const
	//-------------------------------------------------------------------------
	{
		sTarget += Serialize(Component);
	}

	//-------------------------------------------------------------------------
	void Parse(KStringView sv, URIPart Component)
	//-------------------------------------------------------------------------
	{
		m_Component = Component;
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
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	void set(KStringView sv)
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
		m_Component = URIPart::Path;
	}

	//-------------------------------------------------------------------------
	template<const char X = chPairSep, typename std::enable_if<X == '\0', int>::type = 0>
	KURLDualEncoded& operator=(KStringView sv)
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

	//-------------------------------------------------------------------------
	// the Key-Value decoding
	template<const char X = chPairSep, typename std::enable_if<X != '\0', int>::type = 0>
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
				kUrlDecode (svKeyEncoded, sKey, m_Component);
				kUrlDecode (svValEncoded, sVal, m_Component);
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
	mutable URIPart m_Component { URIPart::Path };

}; // KURLDualEncoded

using URLDualEncodedString = KURLDualEncoded<KString>;
using URLDualEncodedQuery  = KURLDualEncoded<KProps<KString, KString>, '&', '='>;

DEKAF2_NAMESPACE_END
