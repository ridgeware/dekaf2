/*
 //-----------------------------------------------------------------------------//
 //
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

#include "kmpsearch.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KMPSearch::KMPSearch(KStringView sPattern)
//-----------------------------------------------------------------------------
: m_sPattern { sPattern }
{
	BuildFSA();
}

//-----------------------------------------------------------------------------
void KMPSearch::BuildFSA()
//-----------------------------------------------------------------------------
{
	FSA.clear();
	FSA.resize(m_sPattern.size() + 1);

	FSA[0] = -1;

	for (size_t i = 0; i < m_sPattern.size(); ++i)
	{
		FSA[i + 1] = FSA[i] + 1;

		while (FSA[i + 1] > 0 && m_sPattern[i] != m_sPattern[FSA[i + 1] - 1])
		{
			FSA[i + 1] = FSA[FSA[i + 1] - 1] + 1;
		}
	}

} // BuildFSA

//-----------------------------------------------------------------------------
bool KMPSearch::Match(char ch) const
//-----------------------------------------------------------------------------
{
	while (ch != m_sPattern[index])
	{
		if (index == 0)
		{
			return false;
		}
		index = FSA[index];
	}

	if (++index < m_sPattern.size())
	{
		return false;
	}
	else
	{
		index = FSA[index];
		return true;
	}

} // Match

//-----------------------------------------------------------------------------
KStringView::size_type KMPSearch::Match(KStringView sHaystack) const
//-----------------------------------------------------------------------------
{
	index = 0;

	for (KStringView::size_type iPos = 0; iPos < sHaystack.size(); ++iPos)
	{
		if (Match(sHaystack[iPos]))
		{
			return iPos - (m_sPattern.size() - 1);
		}
	}

	return KStringView::npos;

} // Match

//-----------------------------------------------------------------------------
bool KMPSearch::Match(KInStream& InStream) const
//-----------------------------------------------------------------------------
{
	index = 0;
	std::istream::int_type ch;

	while ((ch = InStream.Read()) != std::istream::traits_type::eof())
	{
		if (Match(ch))
		{
			return true;
		}
	}

	return false;

} // Match

DEKAF2_NAMESPACE_END
