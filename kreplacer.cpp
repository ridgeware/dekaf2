/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/

#include "kreplacer.h"
#include "kstringutils.h" // for kFormNumber()
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KReplacer::empty() const
//-----------------------------------------------------------------------------
{
	return m_RepMap.empty();

} // empty

//-----------------------------------------------------------------------------
size_t KReplacer::size() const
//-----------------------------------------------------------------------------
{
	return m_RepMap.size();

} // size

//-----------------------------------------------------------------------------
void KReplacer::clear()
//-----------------------------------------------------------------------------
{
	m_RepMap.clear();

} // clear

//-----------------------------------------------------------------------------
bool KReplacer::insert(KStringView sSearch, KStringView sReplace)
//-----------------------------------------------------------------------------
{
	if (!sSearch.empty())
	{
		KString sKey = m_sTokenPrefix;
		sKey += sSearch;
		sKey += m_sTokenSuffix;

		auto p = m_RepMap.emplace(std::move(sKey), sReplace);

		if (!p.second)
		{
			// replace the existing value
			p.first->second = sReplace;
		}
		return true;
	}

	return false;
	
} // insert

//-----------------------------------------------------------------------------
void KReplacer::insert(const KReplacer& other)
//-----------------------------------------------------------------------------
{
	if (m_sTokenPrefix == other.m_sTokenPrefix && m_sTokenSuffix == other.m_sTokenSuffix)
	{
		for (const auto& it : other.m_RepMap)
		{
			m_RepMap.insert(it);
		}
	}
	else
	{
		kDebug(2, "lead in and lead out are not same");
	}

} // insert

//-----------------------------------------------------------------------------
uint16_t KReplacer::AddTokens (const KJSON& object, bool bFormNumbers/*=true*/)
//-----------------------------------------------------------------------------
{
	uint16_t iTokens{0};

	if (!object.is_object())
	{
		kDebug (1, "called with non-simple object: {:.20}...", object.dump());
	}
	else
	{
		for (auto& it : object.items())
		{
			try
			{
				auto    sKey   = it.key();
				auto    oValue = it.value();
				KString sValue;

				if (oValue.is_string())
				{
					sValue = oValue;
				}
				else if (oValue.is_number())
				{
					kDebug (3, "looks like a number: {} = {}", sKey, oValue.dump());
					sValue = oValue.dump();
					if (bFormNumbers)
					{
						sValue = kFormNumber (sValue.Int64());
					}
				}
				else if (oValue.is_boolean())
				{
					kDebug (3, "looks like a bool: {} = {}", sKey, oValue.dump());
					sValue = oValue.dump();
				}
				else
				{
					kDebug (3, "ignoring this key b/c not a simple key/value pair: {}", sKey);
					continue; // for
				}

				kDebug (2, "token {}{}{} = {}", m_sTokenPrefix, sKey, m_sTokenSuffix, sValue);

				insert (sKey, sValue);
				++iTokens;
			}
			catch (std::exception& e)
			{
				// ignore anything that is not a simple key/value pair
			}
		}
	}

	return iTokens;

} // AddTokens

//-----------------------------------------------------------------------------
bool KReplacer::erase(KStringView sSearch)
//-----------------------------------------------------------------------------
{
	if (!sSearch.empty())
	{
		KString sKey = m_sTokenPrefix;
		sKey += sSearch;
		sKey += m_sTokenSuffix;

		return m_RepMap.erase(sKey) > 0;
	}

	return false;

} // erase

//-----------------------------------------------------------------------------
KString KReplacer::Replace(KStringView sIn) const
//-----------------------------------------------------------------------------
{
	KString sOut;
	sOut.reserve(sIn.size());

	// TODO consider using Aho-Corasick here

	if (m_sTokenPrefix.empty())
	{
		if (m_sTokenSuffix.empty())
		{
			// neither lead in nor lead out -
			// brute force, try to replace all variables across the whole content
			sOut = sIn;

			// we search in descending order from the map,
			// as that would find the longer matches first
			for (auto it = m_RepMap.crbegin(), ie = m_RepMap.crend(); it != ie; ++it)
			{
				sOut.Replace(it->first, it->second);
			}
		}
		else
		{
			// we have a lead out, but no lead in -
			// search backwards from a found lead out
			for (;;)
			{
				auto pos = sIn.find(m_sTokenSuffix);
				if (pos == KStringView::npos)
				{
					// no more lead out found
					sOut += sIn;
					break;
				}

				KStringView sTemp = sIn.substr(0, pos + m_sTokenSuffix.size());

				// we search the full map and keep a record of
				// the longest match - alternatively we could sort
				// the map by size descending and return the first match
				auto lit = m_RepMap.cend();
				std::size_t iLongest { 0 };

				for (auto it = m_RepMap.cbegin(), ie = m_RepMap.cend(); it != ie; ++it)
				{
					// no need to check if value is shorter or equal
					// than longest found so far
					if (it->first.size() > iLongest)
					{
						// variable found?
						if (sTemp.ends_with(it->first))
						{
							lit = it;
							iLongest = it->first.size();
						}
					}
				}

				if (lit != m_RepMap.cend())
				{
					sTemp.remove_suffix(lit->first.size());
					sOut += sTemp;
					// replace with this value
					sOut += lit->second;
					sIn.remove_prefix(pos + m_sTokenSuffix.size());
				}
				else
				{
					sOut += sTemp;
					sIn.remove_prefix(pos + m_sTokenSuffix.size());
				}
			}
		}
	}
	else
	{
		// we have a lead in, and maybe a lead out -
		// search for LeadIn sequences, and if found
		// check for all variables
		for (;;)
		{
			auto pos = sIn.find(m_sTokenPrefix);
			if (pos == KStringView::npos)
			{
				// no more lead in found
				sOut += sIn;
				break;
			}
			sOut += sIn.substr(0, pos);
			sIn.remove_prefix(pos);

			bool bFound { false };

			// we search in descending order from the map,
			// as that would find the longer matches first
			// (if we have no lead out)
			for (auto it = m_RepMap.crbegin(), ie = m_RepMap.crend(); it != ie; ++it)
			{
				// variable found?
				if (sIn.starts_with(it->first))
				{
					// replace with this value
					sOut += it->second;
					sIn.remove_prefix(it->first.size());
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				if (m_bRemoveUnusedTokens)
				{
					pos = sIn.find(m_sTokenSuffix);
					if (pos != KStringView::npos)
					{
						sIn.remove_prefix(pos + m_sTokenSuffix.size());
						continue;
					}
					else
					{
						// we have no lead out anymore in the content, so we give up
						sOut += sIn;
						break;
					}
				}
				// advance one char
				sOut += sIn.front();
				sIn.remove_prefix(1);
			}
		}
	}

	return sOut;

} // clear

//-----------------------------------------------------------------------------
void KReplacer::ReplaceInPlace(KStringRef& sIn) const
//-----------------------------------------------------------------------------
{
	sIn = Replace(sIn).str();

} // clear

// if std::map is not yet supported by this lib to be nothrow, don't test dependant class
static_assert(!std::is_nothrow_move_constructible<std::map<int, int>>::value || std::is_nothrow_move_constructible<KReplacer>::value,
			  "KReplacer is intended to be nothrow move constructible, but is not!");

// if std::map is not yet supported by this lib to be nothrow, don't test dependant class
static_assert(!std::is_nothrow_move_constructible<std::map<int, int>>::value || std::is_nothrow_move_constructible<KVariables>::value,
			  "KVariables is intended to be nothrow move constructible, but is not!");

} // of namespace dekaf2

