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
		KString sKey = m_sLeadIn;
		sKey += sSearch;
		sKey += m_sLeadOut;

#if (BOOST_VERSION < 106600)
		auto p = m_RepMap.insert({std::move(sKey), sReplace});
#else
		auto p = m_RepMap.emplace(std::move(sKey), sReplace);
#endif
		if (p.second == false)
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
	if (m_sLeadIn == other.m_sLeadIn && m_sLeadOut == other.m_sLeadOut)
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
bool KReplacer::erase(KStringView sSearch)
//-----------------------------------------------------------------------------
{
	if (!sSearch.empty())
	{
		KString sKey = m_sLeadIn;
		sKey += sSearch;
		sKey += m_sLeadOut;

		m_RepMap.erase(sKey);

		return true;
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

	if (m_sLeadIn.empty())
	{
		// brute force, try to replace all variables across the whole content
		sOut = sIn;

		for (const auto& it : m_RepMap)
		{
			sOut.Replace(it.first, it.second);
		}
	}
	else
	{
		// search for LeadIn sequences, and if found check for all
		// variables
		for (;;)
		{
			auto pos = sIn.find(m_sLeadIn);
			if (pos == KStringView::npos)
			{
				// no more lead in found
				sOut += sIn;
				break;
			}
			sOut += sIn.substr(0, pos);
			sIn.remove_prefix(pos);

			bool bFound { false };

			for (const auto& it : m_RepMap)
			{
				// variable found?
				if (sIn.starts_with(it.first))
				{
					// replace with this value
					sOut += it.second;
					sIn.remove_prefix(it.first.size());
					bFound = true;
					break;
				}
			}

			if (!bFound)
			{
				if (m_bRemoveAllVariables)
				{
					pos = sIn.find(m_sLeadOut);
					if (pos != KStringView::npos)
					{
						sIn.remove_prefix(pos + m_sLeadOut.size());
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
void KReplacer::ReplaceInPlace(KString& sIn) const
//-----------------------------------------------------------------------------
{
	KString sOut = Replace(sIn);
	sIn.swap(sOut);

} // clear


} // of namespace dekaf2

