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

#pragma once

/// @file kcsv.h
/// CSV reader/writer

#include <vector>
#include "kreader.h"
#include "kwriter.h"
#include "kstring.h"
#include "kstringview.h"
#include "kinstringstream.h"
#include "koutstringstream.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// read and write CSV files
class KCSV
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// construct a CSV reader/writer with record, column and field delimiters (defaulted)
	KCSV(char chRecordLimiter = '\n',
		 char chColumnLimiter = ',',
		 char chFieldLimiter  = '"');

	//-----------------------------------------------------------------------------
	/// write any iterable type with elements that are convertible into a string view with correct escaping into an output stream
	template<class Columns>
	bool Write(KOutStream& Out, const Columns& Record)
	//-----------------------------------------------------------------------------
	{
		if (Record.empty())
		{
			return false;
		}

		bool bFirst { true };

		for (const auto& Column : Record)
		{
			WriteColumn(Out, Column, bFirst);
			bFirst = false;
		}

		if (m_chRecordLimiter == '\n')
		{
			// the csv file format uses the canonical linefeed of http
			Out += '\r';
		}

		Out += m_chRecordLimiter;

		return Out.Good();
	}

	//-----------------------------------------------------------------------------
	/// write any iterable type with elements that are convertible into a string view with correct escaping into an output string
	template<class Columns>
	KString Write(const Columns& Record)
	//-----------------------------------------------------------------------------
	{
		KString sRecord;
		KOutStringStream Oss(sRecord);
		Write(Oss, Record);
		return sRecord;
	}

	//-----------------------------------------------------------------------------
	/// read a vector of strings with correct escaping from an input stream, appending to Record
	template<class Columns = std::vector<KString>>
	bool Read(KInStream& In, Columns& Record)
	//-----------------------------------------------------------------------------
	{
		KString sColumn;

		for (;;)
		{
			switch (ReadColumn(In, sColumn))
			{
				case STATE::EndOfColumn:
					Record.push_back(std::move(sColumn));
					break;

				case STATE::EndOfRecord:
					Record.push_back(std::move(sColumn));
					return true;

				case STATE::EndOfFile:
					return !Record.empty();
			}
		}
	}

	//-----------------------------------------------------------------------------
	/// read a vector of strings with correct escaping from an input stream
	template<class Columns = std::vector<KString>>
	Columns Read(KInStream& In)
	//-----------------------------------------------------------------------------
	{
		Columns Record;
		Read(In, Record);
		return Record;
	}

	//-----------------------------------------------------------------------------
	/// read a vector of strings with correct escaping from an input string
	template<class Columns = std::vector<KString>>
	Columns Read(KStringView In)
	//-----------------------------------------------------------------------------
	{
		KInStringStream Iss(In);
		return Read<Columns>(Iss);
	}

//------
protected:
//------

	enum class STATE { EndOfRecord, EndOfColumn, EndOfFile };

	bool WriteColumn(KOutStream& Out, KStringView sColumn, bool bIsFirst);
	STATE ReadColumn(KInStream& In, KString& sColumn);

//------
private:
//------

	KString m_sLimiters;
	char    m_chRecordLimiter;
	char    m_chColumnLimiter;
	char    m_chFieldLimiter;

}; // KCSV

} // end of namespace dekaf2
