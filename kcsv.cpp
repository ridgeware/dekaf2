/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

#include "kcsv.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KCSV::KCSV(char chRecordLimiter,
           char chColumnLimiter,
           char chFieldLimiter)
//-----------------------------------------------------------------------------
: m_chRecordLimiter ( chRecordLimiter )
, m_chColumnLimiter ( chColumnLimiter )
, m_chFieldLimiter  ( chFieldLimiter  )
{
	m_sLimiters  = chRecordLimiter;
	m_sLimiters += chColumnLimiter;
	m_sLimiters += chFieldLimiter;
}

//-----------------------------------------------------------------------------
bool KCSV::WriteColumn(KOutStream& Out, KStringView sColumn, bool bIsFirst)
//-----------------------------------------------------------------------------
{
	if (!bIsFirst)
	{
		Out += m_chColumnLimiter;
	}

	bool bUseFieldLimiter = sColumn.find_first_of(m_sLimiters) != KString::npos;

	if (bUseFieldLimiter)
	{
		Out += m_chFieldLimiter;
	}

	for (auto ch : sColumn)
	{
		if (ch == m_chFieldLimiter && bUseFieldLimiter)
		{
			Out += ch;
		}
		Out += ch;
	}

	if (bUseFieldLimiter)
	{
		Out += m_chFieldLimiter;
	}

	return Out.Good();

} // WriteColumn

//-----------------------------------------------------------------------------
KCSV::STATE KCSV::ReadColumn(KInStream& In, KString& sColumn)
//-----------------------------------------------------------------------------
{
	bool bIsStartofColumn     { true  };
	bool bUseFieldLimiter     { false };
	bool bLastWasFieldLimiter { false };

	std::istream::int_type ch;

	while (DEKAF2_LIKELY((ch = In.Read()) != std::istream::traits_type::eof()))
	{
		if (DEKAF2_UNLIKELY(ch == m_chRecordLimiter && (!bUseFieldLimiter || bLastWasFieldLimiter)))
		{
			// return with success
			return STATE::EndOfRecord;
		}
		else if (ch == '\r' && m_chRecordLimiter == '\n' && (!bUseFieldLimiter || bLastWasFieldLimiter))
		{
			// skip the CR when LF is the record limiter
			continue;
		}
		else if (ch == m_chFieldLimiter)
		{
			if (bIsStartofColumn)
			{
				bUseFieldLimiter = true;
				bIsStartofColumn = false;
			}
			else if (bLastWasFieldLimiter)
			{
				// output one field limiter
				sColumn += m_chFieldLimiter;
				bLastWasFieldLimiter = false;
			}
			else
			{
				bLastWasFieldLimiter = true;
			}
		}
		else if (ch == m_chColumnLimiter && (!bUseFieldLimiter || bLastWasFieldLimiter))
		{
			bLastWasFieldLimiter = false;
			bUseFieldLimiter = false;
			bIsStartofColumn = true;
			// return with success
			return STATE::EndOfColumn;
		}
		else
		{
			bIsStartofColumn = false;
			sColumn += ch;
		}
	}

	// return with success if the current column is not empty, otherwise it was the eof
	return (!sColumn.empty()) ? STATE::EndOfRecord : STATE::EndOfFile;

} // ReadColumn

} // of namespace dekaf2
