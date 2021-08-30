/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

#include "ksqlite.h"
#include <fstream>
#include <sqlite3.h>

#ifdef DEKAF2
#include "klog.h"
namespace dekaf2 {
#endif

namespace KSQLite {

//--------------------------------------------------------------------------------
inline bool Success(int ec)
//--------------------------------------------------------------------------------
{
	return (ec == SQLITE_OK);
}

//--------------------------------------------------------------------------------
detail::DBConnector::DBConnector(StringViewZ sFilename, Mode iMode)
//--------------------------------------------------------------------------------
{
	Connect(sFilename, iMode);

} // ctor DBConnector

//--------------------------------------------------------------------------------
detail::DBConnector::DBConnector(const DBConnector& other)
//--------------------------------------------------------------------------------
{
	Connect(other.Filename(), other.m_iMode);

} // copy ctor DBConnector

//--------------------------------------------------------------------------------
detail::DBConnector::~DBConnector()
//--------------------------------------------------------------------------------
{
	sqlite3_close(m_DB);

} // dtor DBConnector

//--------------------------------------------------------------------------------
bool detail::DBConnector::Connect(StringViewZ sFilename, Mode iMode)
//--------------------------------------------------------------------------------
{
	if (m_DB == nullptr)
	{
		m_iMode = iMode;

		int iFlags;
		switch (m_iMode)
		{
			default:
			case READONLY:
				iFlags = SQLITE_OPEN_READONLY;
				break;

			case READWRITE:
				iFlags = SQLITE_OPEN_READWRITE;
				break;

			case READWRITECREATE:
				iFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
				break;
		}
		auto ec = sqlite3_open_v2(sFilename.c_str(), &m_DB, iFlags, nullptr);
		if (Success(ec))
		{
			return true;
		}
		else
		{
#ifdef DEKAF2
			kDebug(1, "error: {}", sqlite3_errstr(ec));
#endif
			sqlite3_close(m_DB);
			m_DB = nullptr;
		}
	}

	return false;

} // Connect

//--------------------------------------------------------------------------------
StringViewZ detail::DBConnector::Filename() const
//--------------------------------------------------------------------------------
{
	return sqlite3_db_filename(m_DB, "main");
}

//--------------------------------------------------------------------------------
detail::DBConnector::size_type detail::DBConnector::AffectedRows()
//--------------------------------------------------------------------------------
{
	return sqlite3_changes(m_DB);
}

//--------------------------------------------------------------------------------
detail::DBConnector::size_type detail::DBConnector::LastInsertID()
//--------------------------------------------------------------------------------
{
	return sqlite3_last_insert_rowid(m_DB);
}

//--------------------------------------------------------------------------------
StringViewZ detail::DBConnector::Error() const
//--------------------------------------------------------------------------------
{
	if (m_DB)
	{
		auto ec = IsError();
		if (ec)
		{
			return sqlite3_errstr(ec);
		}
		else
		{
			return {};
		}
	}
	return "no database connection";
}

//--------------------------------------------------------------------------------
int detail::DBConnector::IsError() const
//--------------------------------------------------------------------------------
{
	if (m_DB)
	{
		return sqlite3_extended_errcode(m_DB);
	}
	else
	{
		return SQLITE_CANTOPEN;
	}
}

//=================================== Database ===================================

//--------------------------------------------------------------------------------
Database::Database(StringViewZ sFilename, Mode iMode)
//--------------------------------------------------------------------------------
	: m_Connector(std::make_shared<detail::DBConnector>(sFilename, iMode))
{
} // ctor

//--------------------------------------------------------------------------------
Database::Database(const Database& other)
//--------------------------------------------------------------------------------
	: m_Connector(std::make_shared<detail::DBConnector>(*other.Connector()))
{
} // copy ctor

//--------------------------------------------------------------------------------
Database& Database::operator=(const Database& other)
//--------------------------------------------------------------------------------
{
	m_Connector = std::make_shared<detail::DBConnector>(*other.Connector());
	return *this;

} // operator=()

//--------------------------------------------------------------------------------
bool Database::Connect(StringViewZ sFilename, Mode iMode)
//--------------------------------------------------------------------------------
{
	m_Connector = std::make_shared<detail::DBConnector>();
	return Connector()->Connect(sFilename, iMode);

} // Connect

//--------------------------------------------------------------------------------
Statement Database::Prepare(StringView sQuery)
//--------------------------------------------------------------------------------
{
	return Statement(*this, sQuery);
}

//--------------------------------------------------------------------------------
bool Database::ExecuteVoid(StringViewZ sQuery)
//--------------------------------------------------------------------------------
{
	auto ec = sqlite3_exec(*Connector(), sQuery.c_str(), nullptr, nullptr, nullptr);
	if (!Success(ec))
	{
#ifdef DEKAF2
		kDebug(1, "error: {}", sqlite3_errstr(ec));
		kDebug(1, sQuery);
		return false;
#endif
	}
	return true;

} // Execute

//--------------------------------------------------------------------------------
int ResultCallback(void* pContainer, int iCols, char** sColums, char** sColNames)
//--------------------------------------------------------------------------------
{
	if (pContainer)
	{
		auto& Container(*static_cast<Database::result_type*>(pContainer));
		Database::result_type_row Row;
		for (int iCount = 0; iCount < iCols; ++iCount)
		{
			Row.insert({sColNames[iCount], sColums[iCount]});
		}
		Container.push_back(std::move(Row));
		return 0;
	}
	else
	{
		return 1; // abort
	}

} // ResultCallback

//--------------------------------------------------------------------------------
Database::result_type Database::Execute(StringViewZ sQuery)
//--------------------------------------------------------------------------------
{
	result_type ResultSet;
	auto ec = sqlite3_exec(*Connector(), sQuery.c_str(), ResultCallback, &ResultSet, nullptr);
	if (!Success(ec))
	{
#ifdef DEKAF2
		kDebug(1, "error: {}", sqlite3_errstr(ec));
		kDebug(1, sQuery);
#endif
	}
	return ResultSet;

} // Execute

//--------------------------------------------------------------------------------
bool Database::Key(StringView sKey)
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_HAS_CODEC
	return Success(sqlite3_key(*Connector(), sKey.data(), sKey.size()));
#else
	return false;
#endif

} // Key

//--------------------------------------------------------------------------------
bool Database::Rekey(StringView sKey)
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_HAS_CODEC
	return Success(sqlite3_rekey(*Connector(), sKey.data(), sKey.size()));
#else
	return false;
#endif

} // Rekey

//--------------------------------------------------------------------------------
bool Database::IsEncrypted(StringViewZ sFilename)
//--------------------------------------------------------------------------------
{
	if (!sFilename.empty())
	{
		std::ifstream File(sFilename.c_str(), std::ios::in | std::ios::binary);
		char header[16];
		if (File.is_open())
		{
			File.seekg(0, std::ios::beg);
			File.getline(header, 16);
			return strncmp(header, "SQLite format 3\000", 16) != 0;
		}
	}
	return false;

} // IsEncrypted

//=================================== RowBase ====================================

//--------------------------------------------------------------------------------
Row::RowBase::RowBase(Database& database, StringView sQuery)
//--------------------------------------------------------------------------------
	: m_Connector(database)
{
	auto ec = sqlite3_prepare_v2(*m_Connector, sQuery.data(), sQuery.size(), &m_Statement, nullptr);

	if (!Success(ec))
	{
#ifdef DEKAF2
		kDebug(1, "error: {}", sqlite3_errstr(ec));
		kDebug(1, sQuery);
#endif
	}

	if (m_Statement)
	{
		m_iColumnCount = sqlite3_column_count(m_Statement);
	}

} // ctor RowBase

//--------------------------------------------------------------------------------
Row::RowBase::~RowBase()
//--------------------------------------------------------------------------------
{
	if (m_Statement)
	{
		sqlite3_finalize(m_Statement);
	}

} // dtor RowBase

//===================================== Row ======================================

//--------------------------------------------------------------------------------
Row::Row()
//--------------------------------------------------------------------------------
	: m_Row(std::make_shared<RowBase>())
{
}

//--------------------------------------------------------------------------------
Row::Row(Database& database, StringView sQuery)
//--------------------------------------------------------------------------------
	: m_Row(std::make_shared<RowBase>(database, sQuery))
{
}

//--------------------------------------------------------------------------------
Column Row::Col(ColIndex iOneBasedIndex)
//--------------------------------------------------------------------------------
{
	if (iOneBasedIndex && iOneBasedIndex <= m_Row->m_iColumnCount)
	{
		return Column(*this, iOneBasedIndex);
	}
	else
	{
		// return an empty column
		return {};
	}
}

//--------------------------------------------------------------------------------
Column Row::Col(StringView sColName)
//--------------------------------------------------------------------------------
{
	return Col(GetColIndex(sColName));
}

//--------------------------------------------------------------------------------
Row::ColIndex Row::GetColIndex(StringView sColName)
//--------------------------------------------------------------------------------
{
	if (m_Row->m_NameMap.empty())
	{
		if (Statement())
		{
			// build the column to index map
			for (int iCount = 0; iCount < size(); ++iCount)
			{
				m_Row->m_NameMap.insert( { sqlite3_column_name(Statement(), iCount), iCount+1 } );
			}
		}
	}

	auto it = m_Row->m_NameMap.find(sColName);

	if (it != m_Row->m_NameMap.end())
	{
		return it->second;
	}

	return 0;

} // GetColIndex

//--------------------------------------------------------------------------------
Column Row::operator[](ColIndex iOneBasedIndex)
//--------------------------------------------------------------------------------
{
	return Col(iOneBasedIndex);
}

//--------------------------------------------------------------------------------
Column Row::operator[](StringView sColName)
//--------------------------------------------------------------------------------
{
	return Col(sColName);
}

//--------------------------------------------------------------------------------
StringViewZ Row::GetQuery() const
//--------------------------------------------------------------------------------
{
	return Statement() ? sqlite3_sql(*m_Row) : "";
}

//--------------------------------------------------------------------------------
bool Row::Reset(bool bClearBindings) noexcept
//--------------------------------------------------------------------------------
{
	if (!Statement())
	{
		return false;
	}
	auto ec = sqlite3_reset(*m_Row);
	if (ec == SQLITE_OK && bClearBindings)
	{
		ec = sqlite3_clear_bindings(*m_Row);
	}
	m_Row->m_bIsDone = false;
	m_Row->m_bIsValid = false;

	return Success(ec);

}

//--------------------------------------------------------------------------------
bool Row::Next()
//--------------------------------------------------------------------------------
{
	if (m_Row->m_bIsDone)
	{
		SetIsValid(false);
	}
	else if (Statement())
	{
		auto ec = sqlite3_step(*m_Row);

		if (ec == SQLITE_ROW)
		{
			SetIsValid(true);
			return true;
		}
		else
		{
			SetIsDone(ec == SQLITE_DONE);
			SetIsValid(false);
		}
	}
	return false;

}

//--------------------------------------------------------------------------------
Row& Row::operator++()
//--------------------------------------------------------------------------------
{
	Next();
	return *this;
}

//================================== Statement ===================================

//--------------------------------------------------------------------------------
Statement::Statement(Database& database, StringView sQuery)
//--------------------------------------------------------------------------------
	: m_Row(database, sQuery)
{
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex, int64_t iValue)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_int64(m_Row, iOneBasedIndex, iValue));
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex, double iValue)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_double(m_Row, iOneBasedIndex, iValue));
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex, StringView sValue, bool bCopy)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_text(m_Row, iOneBasedIndex, sValue.data(), sValue.size(), bCopy ? SQLITE_TRANSIENT : SQLITE_STATIC));
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex, void* pValue, size_type iSize, bool bCopy)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_blob(m_Row, iOneBasedIndex, pValue, iSize, bCopy ? SQLITE_TRANSIENT : SQLITE_STATIC));
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex)
//--------------------------------------------------------------------------------
{
	return Success(sqlite3_bind_null(m_Row, iOneBasedIndex));
}

//--------------------------------------------------------------------------------
bool Statement::Reset(bool bClearBindings)
//--------------------------------------------------------------------------------
{
	return m_Row.Reset(bClearBindings);

} // Reset

//--------------------------------------------------------------------------------
bool Statement::NextRow()
//--------------------------------------------------------------------------------
{
	return m_Row.Next();

} // Next

//--------------------------------------------------------------------------------
bool Statement::Execute()
//--------------------------------------------------------------------------------
{
	bool bRet = m_Row.Next();
	if (m_Row.Done())
	{
		bRet = true;
	}
	return bRet;

} // Execute

//--------------------------------------------------------------------------------
Statement::iterator::iterator(class Row& row, bool bToEnd)
//--------------------------------------------------------------------------------
: m_pRow(bToEnd ? nullptr : &row)
{
	if (m_pRow && m_pRow->empty())
	{
		m_pRow = nullptr;
	}

} // ctor iterator

//--------------------------------------------------------------------------------
Statement::iterator& Statement::iterator::operator++()
//--------------------------------------------------------------------------------
{
	if (m_pRow)
	{
		if (!m_pRow->Next())
		{
			m_pRow = nullptr;
		}
	}
	return *this;
	
} // operator++

//--------------------------------------------------------------------------------
Statement::ParIndex Statement::GetParIndex(StringViewZ sParName)
//--------------------------------------------------------------------------------
{
	return sqlite3_bind_parameter_index(m_Row, sParName.c_str());
}

//=================================== Column =====================================

//--------------------------------------------------------------------------------
StringViewZ Column::GetName()
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_ENABLE_COLUMN_METADATA
	return m_Row.Statement() ? sqlite3_column_origin_name(m_Row, m_Index) : "";
#else
	return {};
#endif

} // GetName

//--------------------------------------------------------------------------------
StringViewZ Column::GetNameAs()
//--------------------------------------------------------------------------------
{
	return m_Row.Statement() ? sqlite3_column_name(m_Row, m_Index) : "";

} // GetNameAs

//--------------------------------------------------------------------------------
int32_t Column::Int32()
//--------------------------------------------------------------------------------
{
	return m_Row.Statement() ? sqlite3_column_int(m_Row, m_Index) : 0;
}

//--------------------------------------------------------------------------------
uint32_t Column::UInt32()
//--------------------------------------------------------------------------------
{
	return static_cast<uint32_t>(Int64());
}

//--------------------------------------------------------------------------------
int64_t Column::Int64()
//--------------------------------------------------------------------------------
{
	return m_Row.Statement() ? sqlite3_column_int64(m_Row, m_Index) : 0;
}

//--------------------------------------------------------------------------------
uint64_t Column::UInt64()
//--------------------------------------------------------------------------------
{
	return static_cast<uint64_t>(Int64());
}

//--------------------------------------------------------------------------------
double Column::Double()
//--------------------------------------------------------------------------------
{
	return m_Row.Statement() ? sqlite3_column_double(m_Row, m_Index) : 0.0;
}

//--------------------------------------------------------------------------------
StringView Column::String()
//--------------------------------------------------------------------------------
{
	auto p = m_Row.Statement() ? reinterpret_cast<const char*>(sqlite3_column_text(m_Row, m_Index)) : "";
	return StringView(p, size());

} // String

//--------------------------------------------------------------------------------
Column::size_type Column::size()
//--------------------------------------------------------------------------------
{
	return m_Row.Statement() ? sqlite3_column_bytes(m_Row, m_Index) : 0;
}

//--------------------------------------------------------------------------------
Column::ColType Column::Type()
//--------------------------------------------------------------------------------
{
	switch (m_Row.Statement() ? sqlite3_column_type(m_Row, m_Index) : SQLITE_NULL)
	{
		case SQLITE_INTEGER:
			return Integer;
		case SQLITE_FLOAT:
			return Float;
		case SQLITE_TEXT:
			return Text;
		case SQLITE_BLOB:
			return BLOB;
		default:
		case SQLITE_NULL:
			return Null;
	}

} // GetType

} // of namespace KSQLite

#ifdef DEKAF2
} // of namespace dekaf2
#endif

