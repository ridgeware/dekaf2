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
namespace dekaf2 {
#endif

namespace KSQLite {

//=================================== Database ===================================

//--------------------------------------------------------------------------------
Database::Database(StringView sFilename, Mode iMode)
//--------------------------------------------------------------------------------
	: m_sFilename(sFilename)
{
	int iFlags;
	switch (iMode)
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
	sqlite3* db = m_DB;
	auto ret = sqlite3_open_v2(m_sFilename.c_str(), &db, iFlags, nullptr);
	if (ret != SQLITE_OK)
	{
		SetError();
		sqlite3_close(db);
		db = nullptr;
	}

} // ctor

//--------------------------------------------------------------------------------
Database::~Database()
//--------------------------------------------------------------------------------
{
	if (m_DB)
	{
		auto ret = sqlite3_close(m_DB);
		if (ret)
		{
			SetError("database is locked");
		}
	}

} // dtor

//--------------------------------------------------------------------------------
bool Database::ExecuteVoid(StringViewZ sQuery)
//--------------------------------------------------------------------------------
{
	auto ret = sqlite3_exec(m_DB, sQuery.c_str(), nullptr, nullptr, nullptr);
	return Check(ret);

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
	auto ret = sqlite3_exec(m_DB, sQuery.c_str(), ResultCallback, &ResultSet, nullptr);
	Check(ret);
	return ResultSet;

} // Execute

//--------------------------------------------------------------------------------
Database::size_type Database::AffectedRows()
//--------------------------------------------------------------------------------
{
	return sqlite3_changes(m_DB);
}

//--------------------------------------------------------------------------------
Database::size_type Database::LastInsertID() const noexcept
//--------------------------------------------------------------------------------
{
	return sqlite3_last_insert_rowid(m_DB);
}

//--------------------------------------------------------------------------------
bool Database::Key(StringView sKey)
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_HAS_CODEC
	if (!sKey.empty())
	{
		return Check(sqlite3_key(m_DB, sKey.data(), sKey.size()));
	}
	else
	{
		return SetError("empty key");
	}
#else
	return SetError("this version of SQLite does not support encryption");
#endif

} // Key

//--------------------------------------------------------------------------------
bool Database::Rekey(StringView sKey)
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_HAS_CODEC
	return Check(sqlite3_rekey(m_DB, sKey.data(), sKey.size()));
#else
	return SetError("this version of SQLite does not support encryption");
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

//--------------------------------------------------------------------------------
bool Database::Check(int iReturn)
//--------------------------------------------------------------------------------
{
	if (iReturn == SQLITE_OK)
	{
		SetError(StringView{});
		return true;
	}
	else
	{
		return SetError(sqlite3_errstr(iReturn));
	}

} // Check

//--------------------------------------------------------------------------------
bool Database::SetError(StringView sError) const
//--------------------------------------------------------------------------------
{
	m_sError = sError;
	return false;

} // SetError

//--------------------------------------------------------------------------------
bool Database::SetError() const
//--------------------------------------------------------------------------------
{
	if (m_DB)
	{
		return SetError(sqlite3_errmsg(m_DB));
	}
	else
	{
		return SetError("no database handle");
	}

} // SetError

//=================================== RowBase ====================================

//--------------------------------------------------------------------------------
detail::RowBase::RowBase(Database& database, StringView sQuery)
//--------------------------------------------------------------------------------
	: m_DB(database)
	, m_sQuery(sQuery)
{
	sqlite3_prepare_v2(m_DB, m_sQuery.data(), m_sQuery.size(), &m_Statement, nullptr);
	m_iColumnCount = sqlite3_column_count(m_Statement);

} // ctor RowBase

//--------------------------------------------------------------------------------
detail::RowBase::~RowBase()
//--------------------------------------------------------------------------------
{
	if (m_Statement)
	{
		sqlite3_reset(m_Statement);
		sqlite3_clear_bindings(m_Statement);
	}

} // dtor RowBase

//===================================== Row ======================================

//--------------------------------------------------------------------------------
Row::Row(Database& database, StringView sQuery)
//--------------------------------------------------------------------------------
: m_Row(std::make_shared<detail::RowBase>(database, sQuery))
{
}

//--------------------------------------------------------------------------------
Column Row::GetColumn(ColIndex iZeroBasedIndex)
//--------------------------------------------------------------------------------
{
	if (iZeroBasedIndex < m_Row->m_iColumnCount)
	{
		return Column(*this, iZeroBasedIndex);
	}
	else
	{
		// return an empty column
		return {};
	}
}

//--------------------------------------------------------------------------------
Column Row::GetColumn(StringView sColName)
//--------------------------------------------------------------------------------
{
	return GetColumn(GetColIndex(sColName));
}

//--------------------------------------------------------------------------------
Row::ColIndex Row::GetColIndex(StringView sColName)
//--------------------------------------------------------------------------------
{
	if (m_Row->m_NameMap.empty())
	{
		for (int iCount = 0; iCount < size(); ++iCount)
		{
			m_Row->m_NameMap.insert({sqlite3_column_name(m_Row->m_Statement, iCount), iCount});
		}
	}

	auto it = m_Row->m_NameMap.find(sColName);
	if (it != m_Row->m_NameMap.end())
	{
		return it->second;
	}

	return INT_MAX;

} // GetColIndex

//--------------------------------------------------------------------------------
Column Row::operator[](ColIndex iZeroBasedIndex)
//--------------------------------------------------------------------------------
{
	return GetColumn(iZeroBasedIndex);
}

//--------------------------------------------------------------------------------
Column Row::operator[](StringView sColName)
//--------------------------------------------------------------------------------
{
	return GetColumn(sColName);
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
	return sqlite3_bind_int64(m_Row.Statement(), iOneBasedIndex, iValue) == SQLITE_OK;
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex, double iValue)
//--------------------------------------------------------------------------------
{
	return sqlite3_bind_double(m_Row.Statement(), iOneBasedIndex, iValue) == SQLITE_OK;
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex, StringView sValue)
//--------------------------------------------------------------------------------
{
	return sqlite3_bind_text(m_Row.Statement(), iOneBasedIndex, sValue.data(), sValue.size(), SQLITE_STATIC) == SQLITE_OK;
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex, void* pValue, size_type iSize)
//--------------------------------------------------------------------------------
{
	return sqlite3_bind_blob(m_Row.Statement(), iOneBasedIndex, pValue, iSize, SQLITE_STATIC) == SQLITE_OK;
}

//--------------------------------------------------------------------------------
bool Statement::Bind(ParIndex iOneBasedIndex)
//--------------------------------------------------------------------------------
{
	return sqlite3_bind_null(m_Row.Statement(), iOneBasedIndex) == SQLITE_OK;
}

//--------------------------------------------------------------------------------
bool Statement::NextRow()
//--------------------------------------------------------------------------------
{
	auto ret = sqlite3_step(m_Row.Statement());

	if (ret == SQLITE_ROW)
	{
		m_Row.SetIsValid(true);
		return true;
	}
	else
	{
		m_Row.SetIsValid(false);
		return false;
	}

} // Next

//--------------------------------------------------------------------------------
Statement::ParIndex Statement::GetParIndex(StringViewZ sParName)
//--------------------------------------------------------------------------------
{
	return sqlite3_bind_parameter_index(m_Row.Statement(), sParName.c_str());
}

//=================================== Column =====================================

//--------------------------------------------------------------------------------
KStringViewZ Column::GetName()
//--------------------------------------------------------------------------------
{
#ifdef SQLITE_ENABLE_COLUMN_METADATA
	return sqlite3_column_origin_name(m_Row.Statement(), m_Index);
#else
	return {};
#endif

} // GetName

//--------------------------------------------------------------------------------
KStringViewZ Column::GetNameAs()
//--------------------------------------------------------------------------------
{
	return sqlite3_column_name(m_Row.Statement(), m_Index);

} // GetNameAs

//--------------------------------------------------------------------------------
int32_t Column::GetInt32()
//--------------------------------------------------------------------------------
{
	return sqlite3_column_int(m_Row.Statement(), m_Index);
}

//--------------------------------------------------------------------------------
uint32_t Column::GetUInt32()
//--------------------------------------------------------------------------------
{
	return static_cast<uint32_t>(GetInt64());
}

//--------------------------------------------------------------------------------
int64_t Column::GetInt64()
//--------------------------------------------------------------------------------
{
	return sqlite3_column_int64(m_Row.Statement(), m_Index);
}

//--------------------------------------------------------------------------------
uint64_t Column::GetUInt64()
//--------------------------------------------------------------------------------
{
	return static_cast<uint64_t>(GetInt64());
}

//--------------------------------------------------------------------------------
double Column::GetDouble()
//--------------------------------------------------------------------------------
{
	return sqlite3_column_double(m_Row.Statement(), m_Index);
}

//--------------------------------------------------------------------------------
KStringView Column::GetText()
//--------------------------------------------------------------------------------
{
	auto p = reinterpret_cast<const char*>(sqlite3_column_text(m_Row.Statement(), m_Index));
	return KStringView(p, sqlite3_column_bytes(m_Row.Statement(), m_Index));

} // GetText

//--------------------------------------------------------------------------------
KStringView Column::GetBLOB()
//--------------------------------------------------------------------------------
{
	auto p = reinterpret_cast<const char*>(sqlite3_column_blob(m_Row.Statement(), m_Index));
	return KStringView(p, sqlite3_column_bytes(m_Row.Statement(), m_Index));

} // GetBLOB

//--------------------------------------------------------------------------------
Column::size_type Column::size()
//--------------------------------------------------------------------------------
{
	return sqlite3_column_bytes(m_Row.Statement(), m_Index);
}

//--------------------------------------------------------------------------------
Column::Type Column::GetType()
//--------------------------------------------------------------------------------
{
	switch (sqlite3_column_type(m_Row.Statement(), m_Index))
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

