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

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KCSV::WriteColumn(KOutStream& Out, KStringView sColumn, bool bIsFirst)
//-----------------------------------------------------------------------------
{
	if (!bIsFirst)
	{
		Out += m_Limiters[ColumnLimiter];
	}

	if (m_LimiterSet.find_first_in(sColumn) == KString::npos)
	{
		// the fast path - no escapes needed
		Out += sColumn;
	}
	else
	{
		auto chFieldLimiter = m_Limiters[FieldLimiter];

		Out += chFieldLimiter;

		for (auto ch : sColumn)
		{
			if (ch == chFieldLimiter)
			{
				Out += ch;
			}
			Out += ch;
		}

		Out += chFieldLimiter;
	}

	return Out.Good();

} // WriteColumn

//-----------------------------------------------------------------------------
KCSV::STATE KCSV::ReadColumn(KInStream& In, KStringRef& sColumn)
//-----------------------------------------------------------------------------
{
	bool bIsStartofColumn     { true  };
	bool bUseFieldLimiter     { false };
	bool bLastWasFieldLimiter { false };

	std::istream::int_type ch;

	while (DEKAF2_LIKELY((ch = In.Read()) != std::istream::traits_type::eof()))
	{
		if (DEKAF2_UNLIKELY(ch == m_Limiters[RecordLimiter] && (!bUseFieldLimiter || bLastWasFieldLimiter)))
		{
			// return with success
			return STATE::EndOfRecord;
		}
		else if (DEKAF2_UNLIKELY(ch == '\r' && m_Limiters[RecordLimiter] == '\n' && (!bUseFieldLimiter || bLastWasFieldLimiter)))
		{
			// skip the CR when LF is the record limiter
			continue;
		}
		else if (DEKAF2_UNLIKELY(ch == m_Limiters[FieldLimiter]))
		{
			if (bIsStartofColumn)
			{
				bUseFieldLimiter = true;
				bIsStartofColumn = false;
			}
			else if (bLastWasFieldLimiter)
			{
				// output one field limiter
				sColumn += m_Limiters[FieldLimiter];
				bLastWasFieldLimiter = false;
			}
			else
			{
				bLastWasFieldLimiter = true;
			}
		}
		else if (ch == m_Limiters[ColumnLimiter] && (!bUseFieldLimiter || bLastWasFieldLimiter))
		{
// state would change to:
//			bLastWasFieldLimiter = false;
//			bUseFieldLimiter = false;
//			bIsStartofColumn = true;
// but as we return here it would never be read
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

DEKAF2_NAMESPACE_END
