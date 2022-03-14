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
#include <memory>
#include "kreader.h"
#include "kwriter.h"
#include "kstring.h"
#include "kstringview.h"
#include "kinstringstream.h"
#include "koutstringstream.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// read and write CSV files
class DEKAF2_PUBLIC KCSV
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// construct a CSV reader/writer with record, column and field delimiters (defaulted)
	KCSV(char chRecordLimiter = '\n',
		 char chColumnLimiter = ',',
		 char chFieldLimiter  = '"');
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write any iterable type with elements that are convertible into a string view with correct escaping into an output stream
	template<class Columns = std::vector<KString>>
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
	template<class Columns = std::vector<KString>>
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
		for (;;)
		{
			KString sColumn;

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
	STATE ReadColumn(KInStream& In, KStringRef& sColumn);

//------
private:
//------

	KString m_sLimiters;
	char    m_chRecordLimiter;
	char    m_chColumnLimiter;
	char    m_chFieldLimiter;

}; // KCSV

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class Record = std::vector<KString>>
class DEKAF2_PUBLIC KInCSV : protected KCSV
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

    //-----------------------------------------------------------------------------
    /// construct a CSV reader with stream, record, column and field delimiters (defaulted)
    KInCSV(KInStream& In,
            char chRecordLimiter = '\n',
            char chColumnLimiter = ',',
            char chFieldLimiter  = '"')
    //-----------------------------------------------------------------------------
    :    KCSV(chRecordLimiter, chColumnLimiter, chFieldLimiter)
    ,    m_In(In)
    {
    }

    //-----------------------------------------------------------------------------
    /// construct a CSV reader with string_view, record, column and field delimiters (defaulted)
    KInCSV(KStringView sIn,
            char chRecordLimiter = '\n',
            char chColumnLimiter = ',',
            char chFieldLimiter  = '"')
    //-----------------------------------------------------------------------------
    :   KCSV(chRecordLimiter, chColumnLimiter, chFieldLimiter)
    ,   m_InStringStream(std::make_unique<KInStringStream>(sIn))
    ,   m_In(*m_InStringStream)
    {
    }

    //:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    /// iterator over records, use it for auto range for loops like
    /// for (const auto& record : csv)
    class DEKAF2_PUBLIC iterator
    //:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
    {
    public:

        iterator(KInCSV* csv = nullptr)
        : m_csv(csv)
        {}

        Record* operator->() { return &m_record; }
        Record& operator*()  { return m_record;  }

        iterator& operator++()
        {
            if (m_csv)
            {
                m_record = m_csv->Read();

                if (m_record.empty())
                {
                    m_csv = nullptr;
                }
            }
            else
            {
                m_record = Record{};
            }

            return *this;
        }

        bool operator==(const iterator& other) const { return m_csv == other.m_csv; }
        bool operator!=(const iterator& other) const { return !operator==(other);   }

    private:

        KInCSV* m_csv { nullptr };
        Record m_record;
    };

    /// return begin of records
    iterator begin() { return ++iterator(this); }
    /// return end of records
    iterator end()   { return iterator();       }

	//-----------------------------------------------------------------------------
	/// read a vector of strings with correct escaping from an input stream
    Record Read()
	//-----------------------------------------------------------------------------
	{
		return KCSV::Read<Record>(m_In);
	}

//------
protected:
//------

    std::unique_ptr<KInStringStream> m_InStringStream;
    KInStream& m_In;

}; // KInCSV

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KOutCSV : protected KCSV
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// construct a CSV writer with stream, record, column and field delimiters (defaulted)
	KOutCSV(KOutStream& Out,
			char chRecordLimiter = '\n',
			char chColumnLimiter = ',',
			char chFieldLimiter  = '"')
	//-----------------------------------------------------------------------------
	:	KCSV(chRecordLimiter, chColumnLimiter, chFieldLimiter)
	,	m_Out(Out)
	{
	}

	//-----------------------------------------------------------------------------
	/// write any iterable type with elements that are convertible into a string view with correct escaping into the output stream
	template<class Columns = std::vector<KString>>
	bool Write(const Columns& Record)
	//-----------------------------------------------------------------------------
	{
		return KCSV::Write(m_Out, Record);
	}

//------
protected:
//------

	KOutStream& m_Out;

}; // KOutCSV

} // end of namespace dekaf2
