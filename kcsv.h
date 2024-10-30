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

#include "kdefinitions.h"
#include "kreader.h"
#include "kwriter.h"
#include "kstring.h"
#include "kstringview.h"
#include "kinstringstream.h"
#include "koutstringstream.h"
#include "kjson.h"
#include <vector>
#include <memory>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// read and write CSV files
class DEKAF2_PUBLIC KCSV
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	enum { RecordLimiter = 0, ColumnLimiter = 1, FieldLimiter = 2 };

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// construct a CSV reader/writer with record, column and field delimiters (defaulted)
	DEKAF2_CONSTEXPR_14
	KCSV(char chRecordLimiter = '\n',
		 char chColumnLimiter = ',',
		 char chFieldLimiter  = '"')
	//-----------------------------------------------------------------------------
	: m_Limiters   { chRecordLimiter, chColumnLimiter, chFieldLimiter }
	, m_LimiterSet ( KStringView (&m_Limiters[0], 3) )
	{
	}

	//-----------------------------------------------------------------------------
	/// Explicitly write the ByteOrderMark in UTF8 encoding. This is deprecated, but most Microsoft CSV applications
	/// require this to display non-ASCII characters correctly. Write the BOM only as the first output, before
	/// any other output, and particularly avoid this if the target could be Unix applications.
	bool WriteBOM(KOutStream& Out);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Explicitly write the ByteOrderMark in UTF8 encoding. This is deprecated, but most Microsoft CSV applications
	/// require this to display non-ASCII characters correctly. Write the BOM only as the first output, before
	/// any other output, and particularly avoid this if the target could be Unix applications.
	DEKAF2_NODISCARD
	KStringView WriteBOM() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write a single column into an output stream. To terminate a record (or, row), call WriteEndOfRecord()
	bool WriteColumn(KOutStream& Out, KStringView sColumn);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// terminate a record, if single colums were written with WriteColumn(), prepare for the next (if any)
	bool WriteEndOfRecord(KOutStream& Out);
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

		for (const auto& Column : Record)
		{
			WriteColumn(Out, Column);
		}

		return WriteEndOfRecord(Out);
	}

	//-----------------------------------------------------------------------------
	/// write any iterable type with elements that are convertible into a string view with correct escaping into an output string
	template<class Columns = std::vector<KString>>
	DEKAF2_NODISCARD
	KString Write(const Columns& Record)
	//-----------------------------------------------------------------------------
	{
		KString sRecord;
		KOutStringStream Oss(sRecord);
		Write(Oss, Record);
		return sRecord;
	}

	//-----------------------------------------------------------------------------
	/// write a single column into an output string. To terminate a record (or, row), call WriteEndOfRecord()
	DEKAF2_NODISCARD
	KString WriteColumn(KStringView sColumn);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// skip UTF8 BOM if existing at start of stream - Microsoft applications use to write this at the start of files
	KInStream& SkipBOM(KInStream& In);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// skip UTF8 BOM if existing at start of input - Microsoft applications use to write this at the start of files
	DEKAF2_NODISCARD
	KStringView SkipBOM(KStringView sIn);
	//-----------------------------------------------------------------------------

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

	STATE ReadColumn(KInStream& In, KStringRef& sColumn);

//------
private:
//------

	// using a char array instead of std::array<> because the latter is not
	// constexpr before C++20
	char                m_Limiters[3];
	bool                m_bFirst { true };
	KFindSetOfChars     m_LimiterSet;

}; // KCSV

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<class Record = std::vector<KString>>
class DEKAF2_PUBLIC KInCSV : protected KCSV
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using StringVector = std::vector<KString>;

	//-----------------------------------------------------------------------------
	/// construct a CSV reader with stream, record, column and field delimiters (defaulted)
	DEKAF2_CONSTEXPR_14
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
	DEKAF2_NODISCARD
	iterator begin() { return ++iterator(this); }
	/// return end of records
	DEKAF2_NODISCARD
	iterator end()   { return iterator();       }

	//-----------------------------------------------------------------------------
	KInCSV& SkipBOM()
	//-----------------------------------------------------------------------------
	{
		KCSV::SkipBOM(m_In);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// read a vector of strings with correct escaping from an input stream
	Record Read()
	//-----------------------------------------------------------------------------
	{
		return KCSV::Read<Record>(m_In);
	}

	//-----------------------------------------------------------------------------
	/// returns the headers, if not yet set, reads next (probably first) line of input. Can be called repeatedly, will
	/// always return the initial value until SetHeaders() is used to change the headers to a new set
	const StringVector& GetHeaders()
	//-----------------------------------------------------------------------------
	{
		if (!m_Headers)
		{
			SetHeaders(KCSV::Read<StringVector>(m_In));
		}

		return *m_Headers;
	}

	//-----------------------------------------------------------------------------
	/// sets the headers, use this if they are not contained in first line of input and you want to convert into
	/// json objects
	KInCSV& SetHeaders(StringVector Headers)
	//-----------------------------------------------------------------------------
	{
		m_Headers = std::make_unique<StringVector>(std::move(Headers));
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// returns a json struct as either an array of arrays of strings, or an array of objects (with the headers as keys),
	DEKAF2_NODISCARD
	KJSON to_json(bool bAsObject = true)
	//-----------------------------------------------------------------------------
	{
		KJSON json = KJSON::array();

		if (bAsObject)
		{
			if (!m_Headers)
			{
				// read first / next line as headers
				SetHeaders(KCSV::Read<StringVector>(m_In));
			}

			for (auto& Row : *this)
			{
				KJSON jObject = KJSON::object();

				for (std::size_t iCol = 0, iMax = std::min(Row.size(), m_Headers->size()); iCol < iMax; ++iCol)
				{
					jObject.emplace((*m_Headers)[iCol], std::move(Row[iCol]));
				}

				json.push_back(std::move(jObject));
			}
		}
		else
		{
			for (auto& Row : *this)
			{
				KJSON jArr = KJSON::array();

				for (auto& Col : Row)
				{
					jArr.push_back(std::move(Col));
				}

				json.push_back(std::move(jArr));
			}
		}

		return json;
	}

	//-----------------------------------------------------------------------------
	/// returns a json array of objects (with the headers as keys),
	operator KJSON()
	//-----------------------------------------------------------------------------
	{
		return to_json();
	}

//------
protected:
//------

	std::unique_ptr<KInStringStream> m_InStringStream;
	std::unique_ptr<StringVector>    m_Headers;
	KInStream&                       m_In;

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
	DEKAF2_CONSTEXPR_14
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
	/// construct a CSV writer with string, record, column and field delimiters (defaulted)
	KOutCSV(KStringRef& sOut,
			char chRecordLimiter = '\n',
			char chColumnLimiter = ',',
			char chFieldLimiter  = '"')
	//-----------------------------------------------------------------------------
	:	KCSV(chRecordLimiter, chColumnLimiter, chFieldLimiter)
	,   m_OutStringStream(std::make_unique<KOutStringStream>(sOut))
	,   m_Out(*m_OutStringStream)
	{
	}

	//-----------------------------------------------------------------------------
	/// Explicitly write the ByteOrderMark in UTF8 encoding. This is deprecated, but most Microsoft applications
	/// require this to display non-ASCII characters correctly. Write the BOM only as the first output, before
	/// any other output
	bool WriteBOM()
	//-----------------------------------------------------------------------------
	{
		return KCSV::WriteBOM(m_Out);
	}

	//-----------------------------------------------------------------------------
	/// write any iterable type with elements that are convertible into a string view with correct escaping into the output stream
	template<class Columns = std::vector<KString>>
	bool Write(const Columns& Record)
	//-----------------------------------------------------------------------------
	{
		return KCSV::Write(m_Out, Record);
	}

	//-----------------------------------------------------------------------------
	/// write a single column into the output stream. To terminate a record (or, row), call WriteEndOfRecord()
	bool WriteColumn(KStringView sColumn)
	//-----------------------------------------------------------------------------
	{
		return KCSV::WriteColumn(m_Out, sColumn);
	}

	//-----------------------------------------------------------------------------
	/// terminate a record, if single colums were written with WriteColumn(), prepare for the next (if any)
	bool WriteEndOfRecord()
	//-----------------------------------------------------------------------------
	{
		return KCSV::WriteEndOfRecord(m_Out);
	}

//------
protected:
//------

	std::unique_ptr<KOutStringStream> m_OutStringStream;
	KOutStream&                       m_Out;

}; // KOutCSV

DEKAF2_NAMESPACE_END
