/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

#include "kversion.h"
#include <algorithm>

namespace dekaf2 {

//-----------------------------------------------------------------------------
KVersion::value_type KVersion::operator[](std::size_t iPos) const
//-----------------------------------------------------------------------------
{
	if (DEKAF2_LIKELY(iPos < size()))
	{
		return m_Version[iPos];
	}

	return 0;

} // operator[] const

//-----------------------------------------------------------------------------
KVersion::value_type& KVersion::operator[](std::size_t iPos)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(iPos >= size()))
	{
		m_Version.resize(iPos + 1);
	}

	return m_Version[iPos];

} // operator[]

//-----------------------------------------------------------------------------
KVersion& KVersion::Trim()
//-----------------------------------------------------------------------------
{
	auto it = std::find_if(m_Version.rbegin(), m_Version.rend(), [](const value_type v) { return v != 0; });
	m_Version.resize(it.base() - m_Version.begin());
	return *this;

} // Parse

//-----------------------------------------------------------------------------
bool KVersion::Parse(KStringView sVersion, KStringView sSeparators)
//-----------------------------------------------------------------------------
{
	clear();

	auto Parts = sVersion.Split(sSeparators);

	m_Version.reserve(Parts.size());

	for (const auto Part : Parts)
	{
		m_Version.push_back(Part.UInt32());
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
KString KVersion::Serialize(KString::value_type chSeparator) const
//-----------------------------------------------------------------------------
{
	KString sVersion;

	for (auto iVersion : m_Version)
	{
		if (!sVersion.empty())
		{
			sVersion += chSeparator;
		}

		sVersion += KString::to_string(iVersion);
	}

	return sVersion;

} // Serialize

//-----------------------------------------------------------------------------
bool operator==(const KVersion& left, const KVersion& right)
//-----------------------------------------------------------------------------
{
	auto Pair = std::mismatch(left.begin(), left.end(), right.begin(), right.end());

	if (Pair.first == left.end())
	{
		// check if all remaining values in right are 0
		for (; Pair.second != right.end(); ++Pair.second)
		{
			if (*Pair.second != 0)
			{
				return false;
			}
		}
		return true;
	}
	else if (Pair.second == right.end())
	{
		// check if all remaining values in left are 0
		for (; Pair.first != left.end(); ++Pair.first)
		{
			if (*Pair.first != 0)
			{
				return false;
			}
		}
		return true;
	}

	// comparison aborted in the middle of both sequences
	return false;

} // operator==()

//-----------------------------------------------------------------------------
bool operator<(const KVersion& left, const KVersion& right)
//-----------------------------------------------------------------------------
{
	auto Pair = std::mismatch(left.begin(), left.end(), right.begin(), right.end());

	if (Pair.first == left.end())
	{
		// check if all remaining values in right are 0
		for (; Pair.second != right.end(); ++Pair.second)
		{
			if (*Pair.second != 0)
			{
				return true;
			}
		}
		return false;
	}
	else if (Pair.second == right.end())
	{
		return false;
	}

	// comparison aborted in the middle of both sequences
	return *Pair.first < *Pair.second;

} // operator<()

} // end of namespace dekaf2

