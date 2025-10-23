/*
//
// mong2sql: Convert MongoDB collections to MySQL tables
//
// Copyright (c) 2025
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
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |/|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
*/

#pragma once

#include <dekaf2/dekaf2.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kjson.h>
#include <dekaf2/ksql.h>
#include <dekaf2/kbar.h>

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>

#include <map>
#include <memory>
#include <set>
#include <vector>

using namespace dekaf2;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Mong2SQL : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	static constexpr KStringView s_sProjectName    = "mong2sql";
	static constexpr KStringView s_sProjectVersion = "1.0.0";
    
	int Main (int argc, char* argv[]);

	template<class... Args>
	void Verbose (int iLevel, KFormatString<Args...> sFormat, Args&&... args) const
	{
		VerboseImpl (iLevel, kFormat (sFormat, std::forward<Args>(args)...));
	}

//------
private:
//------
	struct Config
	{
		KString sInputFile;
		KString sMongoConnectionString;
		KString sCollectionNames;
		KString sDBC;
		KString sTablePrefix;
		KString sMode {"COPY"};
		bool    bCreateTables { true };
		bool    bOutputToStdout { true };
		bool    bHasDBC { false };
		bool    bVerbose { false };
		bool    bForce { false };
		bool    bContinueMode { false };
		KString sContinueField;
		bool    bNoData { false };
		bool    bFirstSynch { false };
		uint8_t iThreads { 12 };
	};

	enum class SqlType
	{
		Unknown,
		Boolean,
		Integer,
		Float,
		Text
	};

	struct ColumnInfo
	{
		SqlType      Type { SqlType::Unknown };
		bool         bNullable { true };
		std::size_t  iMaxLength { 0 };
	};

	bool        IsMode(KStringView sMode)
	{
		return (m_Config.sMode.ToUpper() == sMode.ToUpper());
	}

	void        InitializeMongoDB ();
	bool        ConnectToMongoDB ();
	void        ShowVersion ();
	bool        GetCollectionFromFile();
	bool        GetCollectionsFromMongoDB ();
	void        SortCollectionsBySize ();

	int         ListCollections ();
	KJSON       LoadMetaDataFromCache (KStringView sCollectionName) const;
	KString     DeduceCacheFile (KStringView sCollectionName) const;
	KJSON       DigestCollection (const KJSON& oCollection) const;
	int         DumpCollections () const;
	int         CompareCollectionsToMySQL();
	int         CopyCollections();
	void        CopyCollection (const KJSON& oCollection);
	bool        CopyMongoDocument (const KJSON& document, const KJSON& oTables, KStringView sCollectionName, KSQL& db, KBAR& bar, KString& sCreatedTables, uint64_t& iInserts, uint64_t& iUpdates);
	KString     GenerateCreateTableDDL (const KJSON& table) const;
	
	// Document processing helpers
	void        ShowBar (KBAR& bar, KStringView sCollectionName, KStringView sCreatedTables="", uint64_t iInserts=0, uint64_t iUpdates=0);
	KString     ExtractPrimaryKeyFromDocument (const KJSON& document) const;
	void        FlattenDocumentToRow (const KJSON& document, const KString& sPrefix, std::map<KString, KString>& rowValues) const;
	KString     ToSqlLiteral (const KJSON& value) const;
	bool        InsertDocumentRow (KSQL& db, const KJSON& tableSchema, const std::map<KString, KString>& rowValues, KStringView sPrimaryKey, KString& sCreatedTables, uint64_t& iInserts, uint64_t& iUpdates) const;
	void        ProcessDocumentArrays (KSQL& db, const KJSON& document, const KJSON& allTables, KStringView sTableName, const KString& sPrefix, KStringView sParentPK, KString& sCreatedTables, uint64_t& iInserts, uint64_t& iUpdates) const;

	// Unified verbose output and debug logging
	void        VerboseImpl (int iLevel, const KString& sMessage) const;

	static constexpr KStringViewZ collection_name { "collection_name" };
	static constexpr KStringViewZ collection_path { "collection_path" };
	static constexpr KStringViewZ column_order { "column_order" };
	static constexpr KStringViewZ columns { "columns" };
	static constexpr KStringViewZ documents { "documents" };
	static constexpr KStringViewZ has_non_null { "has_non_null" };
	static constexpr KStringViewZ has_non_zero { "has_non_zero" };
	static constexpr KStringViewZ has_object_id { "has_object_id" };
	static constexpr KStringViewZ max_column_width { "max_column_width" };
	static constexpr KStringViewZ max_length { "max_length" };
	static constexpr KStringViewZ column_name { "column_name" };
	static constexpr KStringViewZ nullable { "nullable" };
	static constexpr KStringViewZ num_columns { "num_columns" };
	static constexpr KStringViewZ num_documents { "num_documents" };
	static constexpr KStringViewZ parent_key_column { "parent_key_column" };
	static constexpr KStringViewZ parent_table { "parent_table" };
	static constexpr KStringViewZ primary_key { "primary_key" };
	static constexpr KStringViewZ size_bytes { "size_bytes" };
	static constexpr KStringViewZ sql_type { "sql_type" };
	static constexpr KStringViewZ sql_type_enum { "sql_type_enum" };
	static constexpr KStringViewZ table_name { "table_name" };
	static constexpr KStringViewZ tables { "tables" };

	// MySQL reserved words from https://dev.mysql.com/doc/refman/8.4/en/keywords.html
	static constexpr KStringViewZ MYSQL_RESERVED_WORDS {
		"ACCESSIBLE,ADD,ALL,ALTER,ANALYZE,AND,AS,ASC,ASENSITIVE"
		",BEFORE,BETWEEN,BIGINT,BINARY,BLOB,BOTH,BY"
		",CALL,CASCADE,CASE,CHANGE,CHAR,CHARACTER,CHECK,COLLATE,COLUMN,CONDITION"
		",CONSTRAINT,CONTINUE,CONVERT,CREATE,CROSS,CUBE,CUME_DIST"
		",CURRENT_DATE,CURRENT_TIME,CURRENT_TIMESTAMP,CURRENT_USER,CURSOR"
		",DATABASE,DATABASES,DAY_HOUR,DAY_MICROSECOND,DAY_MINUTE,DAY_SECOND"
		",DEC,DECIMAL,DECLARE,DEFAULT,DELAYED,DELETE,DENSE_RANK,DESC"
		",DESCRIBE,DETERMINISTIC,DISTINCT,DISTINCTROW,DIV,DOUBLE,DROP,DUAL"
		",EACH,ELSE,ELSEIF,EMPTY,ENCLOSED,ESCAPED,EXCEPT,EXISTS,EXIT,EXPLAIN"
		",FALSE,FETCH,FIRST_VALUE,FLOAT,FLOAT4,FLOAT8,FOR,FORCE,FOREIGN,FROM,FULLTEXT"
		",FUNCTION,GENERATED,GET,GRANT,GROUP,GROUPING,GROUPS"
		",HAVING,HIGH_PRIORITY,HOUR_MICROSECOND,HOUR_MINUTE,HOUR_SECOND"
		",IF,IGNORE,IN,INDEX,INFILE,INNER,INOUT,INSENSITIVE,INSERT,INT,INT1,INT2,INT3,INT4,INT8"
		",INTEGER,INTERVAL,INTO,IO_AFTER_GTIDS,IO_BEFORE_GTIDS,IS,ITERATE"
		",JOIN,JSON_TABLE,KEY,KEYS,KILL"
		",LAG,LAST_VALUE,LATERAL,LEAD,LEADING,LEAVE,LEFT,LIKE,LIMIT,LINEAR,LINES,LOAD"
		",LOCALTIME,LOCALTIMESTAMP,LOCK,LONG,LONGBLOB,LONGTEXT,LOOP,LOW_PRIORITY"
		",MASTER_BIND,MASTER_SSL_VERIFY_SERVER_CERT,MATCH,MAXVALUE,MEDIUMBLOB,MEDIUMINT,MEDIUMTEXT,MIDDLEINT"
		",MINUTE_MICROSECOND,MINUTE_SECOND,MOD,MODIFIES,NATURAL,NOT,NO_WRITE_TO_BINLOG,NTH_VALUE,NTILE"
		",NULL,NUMERIC"
		",OF,ON,OPTIMIZE,OPTIMIZER_COSTS,OPTION,OPTIONALLY,OR,ORDER,OUT,OUTER,OUTFILE,OVER"
		",PARTITION,PERCENT_RANK,PERSIST,PERSIST_ONLY,PORTION,PRECISION,PRIMARY,PROCEDURE,PURGE"
		",RANGE,RANK,READ,READS,READ_WRITE,REAL,RECURSIVE,REFERENCES,REGEXP,RELEASE"
		",RENAME,REPEAT,REPLACE,REQUIRE,RESIGNAL,RESTRICT,RETURN,REVOKE,RIGHT,RLIKE,ROW,ROWS,ROW_NUMBER"
		",SCHEMA,SCHEMAS,SELECT,SENSITIVE,SEPARATOR,SET,SHOW,SIGNAL,SMALLINT,SPATIAL,SPECIFIC,SQL"
		",SQLEXCEPTION,SQLSTATE,SQLWARNING,SQL_BIG_RESULT,SQL_CALC_FOUND_ROWS,SQL_SMALL_RESULT,SSL"
		",STARTING,STORED,STRAIGHT_JOIN,SYSTEM"
		",TABLE,TERMINATED,THEN,TINYBLOB,TINYINT,TINYTEXT,TO,TRAILING,TRIGGER,TRUE"
		",UNDO,UNION,UNIQUE,UNLOCK,UNSIGNED,UPDATE,USAGE,USE,USING"
		",UTC_DATE,UTC_TIME,UTC_TIMESTAMP"
		",VALUES,VARBINARY,VARCHAR,VARCHARACTER,VARYING,VIRTUAL"
		",WHEN,WHERE,WHILE,WINDOW,WITH,WRITE,XOR"
		",YEAR_MONTH,ZEROFILL"
	};

	bool LoadDBC ();
	
	// Helper methods for schema collection and DDL generation
	SqlType     InferSqlType (const KJSON& value) const;
	SqlType     MergeSqlTypes (SqlType existing, SqlType candidate) const;
	KString     SqlTypeToString (const ColumnInfo& info) const;
	
	// Thread-safe helper methods (use local KJSON structures)
	void        TrackKeyCardinality (const KJSON& document, const KString& sPrefix, std::map<KString, std::set<KString>>& keyCardinality, std::map<KString, std::size_t>& documentCounts) const;
	std::set<KString> AnalyzeKeyCardinality (const std::map<KString, std::set<KString>>& keyCardinality, const std::map<KString, std::size_t>& documentCounts, KStringView sCollectionName) const;
	void        CollectSchemaForDocumentLocal (const KJSON& document, KStringView sTableName, const KString& sPrefix, KJSON& localTables, KJSON& localTableOrder, const std::set<KString>& pivotFields = std::set<KString>{}) const;
	void        CollectArraySchemaLocal (const KJSON& array, KStringView sParentTableName, const KString& sKey, KJSON& localTables, KJSON& localTableOrder, const std::set<KString>& pivotFields = std::set<KString>{}) const;
	void        AddColumnLocal (KJSON& tableData, KStringView sColumnName, SqlType type, std::size_t iLength, bool isNullable, const KJSON& value) const;
	KJSON&      EnsureTableSchemaLocal (KStringView sTableName, KJSON& localTables, KJSON& localTableOrder, KStringView sCollectionPath, KStringView sParentTable = KStringView{}, KStringView sParentKeyColumn = KStringView{}) const;
	
	// Naming and conversion helpers
	KString     ConvertCollectionNameToTableName (KStringView sCollectionName) const;
	KString     ConvertFieldNameToColumnName (KStringView sFieldName) const;
	KString     SanitizeColumnName (KStringView sName) const;
	KString     BuildColumnName (const KString& sPrefix, KStringView sKey) const;
	KString     BuildChildTableName (KStringView sParentTable, KStringView sKey) const;
	KString     ExtractLeafColumnName (KStringView sQualified) const;
	KString     GenerateTablePKEY (KStringView sTableName) const;
	KString     NormalizeTablePrefix (KStringView sPrefix) const;
	KString     ApplySingularizationToTableName (KStringView sTableName) const;
	KString     ConvertPluralToSingular (KStringView sPlural) const;
	KString     SanitizeMongoJSON (KStringView sJsonDoc) const;
	bool        TableExistsInMySQL (KStringView sTableName);

	Config m_Config;
	KSQL    m_SQL;
	std::unique_ptr<mongocxx::client>  m_MongoClient;
	
	KJSON   m_Collections;  // Array of collection objects with metadata, tables, columns, etc.
	
	std::mutex m_Mutex;

}; // Mong2SQL

// Macro for unified verbose output and debug logging
//#define Verbose(iLevel, sFormat, ...) VerboseImpl(iLevel, kFormat(sFormat, __VA_ARGS__))

