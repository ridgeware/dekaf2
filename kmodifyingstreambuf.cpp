/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2023, Ridgeware, Inc.
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

#include "kmodifyingstreambuf.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KModifyingOutputStreamBuf::~KModifyingOutputStreamBuf()
//-----------------------------------------------------------------------------
{
	if (m_iFound > 0)
	{
		// flush remaining output
		Write(m_sSearch.Left(m_iFound));
	}

} // dtor

//-----------------------------------------------------------------------------
void KModifyingOutputStreamBuf::Replace(KStringView sSearch, KStringView sReplace)
//-----------------------------------------------------------------------------
{
	m_sSearch  = sSearch;
	m_sReplace = sReplace;
	m_iFound   = 0;

} // Replace

//-----------------------------------------------------------------------------
bool KModifyingOutputStreamBuf::OutputSingleChar(char ch)
//-----------------------------------------------------------------------------
{
	if (ch == m_sSearch[m_iFound])
	{
		++m_iFound;

		if (m_iFound == m_sSearch.size())
		{
			m_iFound = 0;
			return Write(m_sReplace) == m_sReplace.size();
		}

		// we do not care for start of line if m_sSearch is not empty
//		m_bAtStartOfLine = ch == '\n';

		return true;
	}

	if (m_iFound > 0)
	{
		Write(m_sSearch.Left(m_iFound));
		// if we are eof here we will also be in the next call
		// to write below, therefore we do not have to check
		m_iFound = 0;
	}

	return !traits_type::eq_int_type(traits_type::eof(), Write(ch));

} // OutputSingleChar

//-----------------------------------------------------------------------------
bool KModifyingOutputStreamBuf::Output(char ch)
//-----------------------------------------------------------------------------
{
	if (!m_sSearch.empty())
	{
		return OutputSingleChar(ch);
	}
	else 
	{
		if (m_bAtStartOfLine) // && m_sSearch == empty
		{
			Write(m_sReplace);
		}

		m_bAtStartOfLine = ch == '\n';
	}

	return !traits_type::eq_int_type(traits_type::eof(), Write(ch));

} // Output

//-----------------------------------------------------------------------------
bool KModifyingOutputStreamBuf::Output(KStringView sOut)
//-----------------------------------------------------------------------------
{
	if (m_sSearch.empty())
	{
		bool bGood { true };

		for (; bGood && !sOut.empty(); )
		{
			if (m_bAtStartOfLine)
			{
				bGood = Write(m_sReplace) == m_sReplace.size();
			}
			// search for next lf
			auto iNext = sOut.find('\n');

			if (iNext != KStringView::npos)
			{
				++iNext; // write the LF as well
				bGood = Write(sOut.Left(iNext)) == iNext;
				sOut.remove_prefix(iNext);
				m_bAtStartOfLine = true;
			}
			else
			{
				bGood = Write(sOut) == sOut.size();
				sOut.clear();
				m_bAtStartOfLine = false;
			}
		}

		return bGood;
	}
	else
	{
		for (; !sOut.empty(); )
		{
			if (m_iFound == 0)
			{
				auto iNext = sOut.find(m_sSearch.front());

				if (iNext != KStringView::npos)
				{
					if (Write(sOut.Left(iNext)) != iNext)
					{
						return false;
					}

					sOut.remove_prefix(iNext);
				}
			}

			KStringView::size_type iWrote { 0 };

			for (auto ch : sOut)
			{
				if (!OutputSingleChar(ch))
				{
					return false;
				}
				++iWrote;

				if (!m_iFound)
				{
					break;
				}
			}

			sOut.remove_prefix(iWrote);
		}
	}

	return true;

} // Output

DEKAF2_NAMESPACE_END
