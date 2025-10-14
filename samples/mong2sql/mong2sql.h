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

//------
private:
//------
	struct Config
	{
		KString sInputFile;
		KString sMongoConnectionString;
		KString sCollectionName;
		KString sDBC;
		KString sTablePrefix;
		bool    m_bCreateTables { true };
		bool    bOutputToStdout { true };
		bool    bHasDBC { false };
		bool    bVerbose { false };
		KString sContinueField;
		bool    bContinueMode { false };
		bool    bNoData { false };
		bool    bFirstSynch { false };
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

	struct TableSchema
	{
		KString sTableName;
		KString sPrimaryKey;
		std::map<KString, ColumnInfo> Columns;
		std::vector<KString>          ColumnOrder;
		KString                       sParentTable;
		KString                       sParentKeyColumn;
		bool                          bHasObjectId { false };
	};

	void        ProcessCollection ();
	bool        ProcessFromFile (std::vector<KJSON>& documents);
	bool        ProcessFromMongoDB (std::vector<KJSON>& documents);
	bool        ProcessFromMongoDBDelta (std::vector<KJSON>& documents);
	std::vector<KString> GetAllCollectionsFromMongoDB ();
	void        ListAllCollectionsWithSizes ();
	void        ProcessDocuments (const std::vector<KJSON>& documents, KStringView sCollectionName);
	KString     ConvertCollectionNameToTableName (KStringView sCollectionName) const;
	KString     ConvertFieldNameToColumnName (KStringView sMongoField) const;
	KString     NormalizeTablePrefix (KStringView sPrefix) const;
	KString     BreakupCompoundWords (KStringView sInput) const;
	KString     ApplySingularizationToTableName (KStringView sTableName) const;
	void        InitializeMongoDB ();
	bool        ConnectToMongoDB ();
	KString     SanitizeMongoJSON (KStringView sJsonDoc) const;
	bool        TableExistsInMySQL (KStringView sTableName);
	KString     GenerateTablePKEY (KStringView sTableName);
	KString     ConvertMongoFieldToMySQLColumn (KStringView sContinueField);
	void        GenerateCreateTableSQL (const TableSchema& table);
	bool        InsertOrUpdateOneRow (const TableSchema& table, const std::map<KString, KString>& rowValues, const KString& sPrimaryKey, std::size_t iSequence);
	void        ShowVersion ();
	TableSchema& EnsureTableSchema (KStringView sTableName, KStringView sParentTable = KStringView{}, KStringView sParentKeyColumn = KStringView{});
	void        CollectSchemaForDocument (const KJSON& document, TableSchema& table, const KString& sPrefix);
	void        CollectArraySchema (const KJSON& array, TableSchema& parentTable, const KString& sKey);
	void        AddColumn (TableSchema& table, const KString& sColumnName, SqlType type, std::size_t iLength = 0, bool isNullable = true);
	SqlType     InferSqlType (const KJSON& value) const;
	SqlType     MergeSqlTypes (SqlType existing, SqlType candidate) const;
	KString     SqlTypeToString (const ColumnInfo& info) const;
	KString     SanitizeColumnName (KStringView sName) const;  // CRITICAL: Must be deterministic for schema/INSERT consistency
	static bool IsHex24 (KStringView sValue);
	bool        IsReservedWord (KStringView sValue) const;
	void        EmitDocumentInsert (const KJSON& document, KStringView sTableName, std::size_t iSequence);
	void        EmitArrayInserts (const KJSON& array, TableSchema& parentTable, const KString& sKey, const KString& sParentPKValue);
	void        ProcessNestedArrays (const KJSON& node, TableSchema& currentTable, const KString& sPrefix, const KString& sCurrentPKValue);
	void        FlattenDocument (const KJSON& document, const KString& sPrefix, std::map<KString, KString>& rowValues);
	KString     BuildColumnName (const KString& sPrefix, KStringView sKey) const;
	KString     BuildChildTableName (KStringView sParentTable, KStringView sKey) const;
	KString     ConvertPluralToSingular (KStringView sPlural) const;
	KString     ExtractLeafColumnName (KStringView sQualified) const;
	KString     ToSqlLiteral (const KJSON& value) const;
	static KString EscapeSqlString (KStringView sValue);
	KString     ExtractPrimaryKeyFromDocument (const KJSON& document) const;
	KString     GetLastModifiedFromMySQL (KStringView sTableName, KStringView sMySQLColumn);
	
	// Unified verbose output and debug logging
	void        VerboseImpl (int iLevel, const KString& sMessage) const;

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

	Config m_Config;
	KSQL    m_SQL;
	std::unique_ptr<mongocxx::client>  m_MongoClient;
	using SchemaMap = std::map<KString, TableSchema>;
	SchemaMap                          m_TableSchemas;
	std::vector<KString>               m_TableOrder;
	std::map<KString, std::size_t>     m_RowCounters;
	std::map<KString, std::int64_t>    m_CollectionSizes;

}; // Mong2SQL

// Macro for unified verbose output and debug logging
#define Verbose(iLevel, sFormat, ...) VerboseImpl(iLevel, kFormat(sFormat, ##__VA_ARGS__))
