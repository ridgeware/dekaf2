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

#include "mong2sql.h"

#include <dekaf2/kfilesystem.h>
#include <dekaf2/klog.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kreader.h>
#include <dekaf2/khash.h>
#include <dekaf2/kbar.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include <bsoncxx/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>


//-----------------------------------------------------------------------------
int Mong2SQL::Main(int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	KInit(false);
	SetThrowOnError(true);

	KOptions Options(true, KLog::STDOUT, /*bThrow*/true);
	Options.SetBriefDescription("convert a MongoDB collection to one or more tables in a MySQL db");
	Options.SetAdditionalHelp(
		"mong2sql : convert a MongoDB collection to one or more tables in a MySQL db\n"
		"\n"
		"usage: mong2sql [<options>] <collection>\n"
		"\n"
		"where <options> are:\n"
		"  -in <file>       :: use input file a full dump of the collection\n"
		"  -mongo \"...\"     :: specify the MongoDB connection string\n"
		"  -dbc <dbc>       :: specify the MySQL database connection\n"
		"  -continue <field> :: enable delta sync mode using MongoDB field for incremental updates\n"
		"  -v, -verbose     :: enable verbose output with progress information\n"
		"  -q, -quiet       :: disable verbose output\n"
		"\n"
		"notes:\n"
		"  * one of -in or -mongo needs to be specified\n"
		"  * if no connection parms are specified for MySQL, then SQL is just\n"
		"    generated to stdout\n"
		"  * -continue mode requires both -mongo and -dbc connections\n"
		"  * verbosity is automatically enabled when using -dbc\n"
	);
	Options.SetAdditionalArgDescription("<collection>");

	Options.Option("in <file>")
		.Help("use input file a full dump of the collection")
		.Set(m_Config.sInputFile);

	Options.Option("mongo <connection_string>")
		.Help("specify the MongoDB connection string")
		.Set(m_Config.sMongoConnectionString);

	Options.Option("dbc <file>")
		.Help("specify the MySQL database connection (DBC file)")
		.Set(m_Config.sDBC);

	Options.Option("continue <lastmod_key>")
		.Help("enable delta sync mode using specified MongoDB field for incremental updates")
		.Set(m_Config.sContinueField)
		.Final()
		([&]()
		{
			m_Config.bContinueMode = true;
			m_Config.m_bCreateTables = false;  // Don't overwrite existing tables in continue mode
		});

	Options.Option("v,verbose")
		.Help("enable verbose output with progress information")
		.Final()
		([&]()
		{
			m_Config.bVerbose = true;
		});

	Options.Option("q,quiet")
		.Help("disable verbose output")
		.Final()
		([&]()
		{
			m_Config.bVerbose = false;
		});

	Options.Option("V,version")
		.Help("show version information")
		.Final()
		([&]()
		{
			ShowVersion();
		});

	Options.UnknownCommand([&](KOptions::ArgList& Args)
	{
		if (!Args.empty())
		{
			m_Config.sCollectionName = Args.pop();
		}
	});

	Options.Parse(argc, argv);

	if (m_Config.sCollectionName.empty())
	{
		SetError("collection name is required");
		return 1;
	}

	if (m_Config.sInputFile.empty() && m_Config.sMongoConnectionString.empty())
	{
		SetError("one of -in or -mongo needs to be specified");
		return 1;
	}

	if (!m_Config.sInputFile.empty() && !m_Config.sMongoConnectionString.empty())
	{
		SetError("cannot specify both -in and -mongo options");
		return 1;
	}

	// Validate continue mode requirements
	if (m_Config.bContinueMode)
	{
		if (m_Config.sMongoConnectionString.empty())
		{
			SetError("-continue mode requires -mongo connection string");
			return 1;
		}
		if (m_Config.sDBC.empty())
		{
			SetError("-continue mode requires -dbc connection for MySQL");
			return 1;
		}
		if (m_Config.sContinueField.empty())
		{
			SetError("-continue mode requires lastmod_key field name");
			return 1;
		}
		kDebug(1, "continue mode enabled: field='{}', mongo='{}', dbc='{}'", 
			m_Config.sContinueField, m_Config.sMongoConnectionString, m_Config.sDBC);
	}

	if (!LoadDBC())
	{
		return 1;
	}

	kDebug(1, "starting collection processing");
	ProcessCollection();
	kDebug(1, "collection processing complete");
	return 0;

} // Mong2SQL::Main

//-----------------------------------------------------------------------------
bool Mong2SQL::LoadDBC()
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	if (m_Config.sDBC.empty())
	{
		m_Config.bOutputToStdout = true;
		m_Config.bHasDBC = false;
		return true;
	}

	kDebug(2, "validating DBC: {}", m_Config.sDBC);
	if (!KSQL::LooksLikeValidDBC(m_Config.sDBC))
	{
		SetError(kFormat("invalid dbc: {}", m_Config.sDBC));
		return false;
	}

	if (!m_SQL.LoadConnect(m_Config.sDBC) || !m_SQL.OpenConnection())
	{
		SetError(kFormat("invalid dbc: {}, {}", m_Config.sDBC, m_SQL.GetLastError()));
		return false;
	}

	m_Config.bOutputToStdout = false;
	m_Config.bHasDBC = true;
	m_Config.bVerbose = true;  // Auto-enable verbosity when using database connection
    
	if (m_Config.bVerbose)
	{
		KOut.FormatLine(":: Connected to MySQL: {}", m_SQL.ConnectSummary());
	}
	kDebug(1, "Connected using DBC: {}", m_Config.sDBC);
	return true;

} // LoadDBC

//-----------------------------------------------------------------------------
void Mong2SQL::ShowVersion()
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);
	KOut.FormatLine(":: {}", Dekaf::GetVersionInformation());

} // ShowVersion
//-----------------------------------------------------------------------------
void Mong2SQL::ProcessCollection()
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	std::vector<KJSON> documents;
	bool bSuccess { false };

	if (!m_Config.sInputFile.empty())
	{
		bSuccess = ProcessFromFile(documents);
	}
	else if (m_Config.bContinueMode)
	{
		bSuccess = ProcessFromMongoDBDelta(documents);
	}
	else
	{
		bSuccess = ProcessFromMongoDB(documents);
	}

	if (!bSuccess)
	{
		return;
	}

	if (documents.empty())
	{
		KErr.FormatLine (">> no documents found for collection '{}'", m_Config.sCollectionName);
		return;
	}

	kDebug(1, "processing {} documents for collection '{}'", documents.size(), m_Config.sCollectionName);
	ProcessDocuments(documents, m_Config.sCollectionName);

} // ProcessCollection

//-----------------------------------------------------------------------------
bool Mong2SQL::ProcessFromFile(std::vector<KJSON>& documents)
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	if (m_Config.bVerbose)
	{
		KOut.FormatLine(":: Reading from file: {}", m_Config.sInputFile);
	}
	kDebug(2, "reading from file: {}", m_Config.sInputFile);
	if (!kFileExists(m_Config.sInputFile))
	{
		SetError(kFormat("Input file does not exist: {}", m_Config.sInputFile));
		return false;
	}

	KInFile InFile(m_Config.sInputFile);
	if (!InFile.is_open())
	{
		SetError(kFormat("Cannot open input file: {}", m_Config.sInputFile));
		return false;
	}

	kDebug(1, "Processing collection '{}' from file: {}", m_Config.sCollectionName, m_Config.sInputFile);

	KString sLine;
	std::size_t iLine { 0 };

	while (InFile.ReadLine(sLine))
	{
		++iLine;
		sLine.Trim();
		if (sLine.empty() || sLine.starts_with('#'))
		{
			continue; // Skip empty lines and comments
		}
        
		if (iLine % 1000 == 0)
		{
			kDebug(2, "processed {} lines", iLine);
		}

		KJSON document;
		KString sError;
		if (!kjson::Parse(document, sLine, sError))
		{
			KErr.FormatLine (">> failed to parse JSON line {}: {} - Error: {}", iLine, sLine, sError);
			continue;
		}

		documents.emplace_back(std::move(document));
	}

	kDebug(1, "loaded {} documents from {} lines", documents.size(), iLine);
	return true;

} // ProcessFromFile

//-----------------------------------------------------------------------------
bool Mong2SQL::ProcessFromMongoDB(std::vector<KJSON>& documents)
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	if (m_Config.bVerbose)
	{
		KOut.FormatLine(":: Connecting to MongoDB: {}", m_Config.sMongoConnectionString);
	}
	kDebug(2, "connecting to MongoDB: {}", m_Config.sMongoConnectionString);
	try
	{
		static mongocxx::instance Instance{};

		mongocxx::uri uri{m_Config.sMongoConnectionString.c_str()};
		mongocxx::client client{uri};

		KString sDatabaseName = uri.database();
		if (sDatabaseName.empty())
		{
			sDatabaseName = "test"; // MongoDB default database
		}

		auto database   = client[sDatabaseName.c_str()];
		auto collection = database[m_Config.sCollectionName.c_str()];

		kDebug(1, "Processing collection '{}' from MongoDB: {}", m_Config.sCollectionName, m_Config.sMongoConnectionString);

		kDebug(2, "querying collection '{}' in database '{}'", m_Config.sCollectionName, sDatabaseName);
		auto cursor = collection.find({});

		std::size_t docCount = 0;
		for (auto&& doc : cursor)
		{
			KString sJsonDoc = bsoncxx::to_json(doc);

			KJSON document;
			KString sError;
			if (!kjson::Parse(document, sJsonDoc, sError))
			{
				KErr.FormatLine (">> failed to parse BSON document: {} - Error: {}", sJsonDoc, sError);
				continue;
			}

			documents.emplace_back(std::move(document));
			++docCount;
			if (docCount % 1000 == 0)
			{
				kDebug(2, "loaded {} documents from MongoDB", docCount);
			}
		}
		kDebug(1, "loaded {} documents from MongoDB", docCount);
	}
	catch (const std::exception& ex)
	{
		SetError(kFormat("MongoDB error: {}", ex.what()));
		return false;
	}

	return true;

} // ProcessFromMongoDB

//-----------------------------------------------------------------------------
bool Mong2SQL::ProcessFromMongoDBDelta(std::vector<KJSON>& documents)
//-----------------------------------------------------------------------------
{
	kDebug(1, "processing MongoDB delta sync for field '{}'", m_Config.sContinueField);
    
	// Convert MongoDB field name to MySQL column name using existing rules
	KString sTableName = ConvertCollectionNameToTableName(m_Config.sCollectionName);
	KString sMySQLColumn = ConvertMongoFieldToMySQLColumn(m_Config.sContinueField);
    
	// Get the last modified timestamp from MySQL
	KString sLastModified = GetLastModifiedFromMySQL(sTableName, sMySQLColumn);
	kDebug(1, "last modified in MySQL table '{}' column '{}': '{}'", sTableName, sMySQLColumn, sLastModified);
    
	// Connect to MongoDB with filter
	kDebug(2, "connecting to MongoDB with delta filter: {}", m_Config.sMongoConnectionString);
    
	// TODO: Implement MongoDB connection with filter
	// For now, return the basic MongoDB processing as placeholder
	// This would need to use mongocxx to apply the filter: { lastmod_key: { $gte: sLastModified } }
    
	KErr.FormatLine(">> delta sync not fully implemented yet - falling back to full sync");
	return ProcessFromMongoDB(documents);
}

//-----------------------------------------------------------------------------
KString Mong2SQL::GetLastModifiedFromMySQL(KStringView sTableName, KStringView sColumnName)
//-----------------------------------------------------------------------------
{
	kDebug(1, "querying max({}) from table '{}'", sColumnName, sTableName);
    
	if (!m_Config.bHasDBC)
	{
		KErr.FormatLine(">> no MySQL connection available for delta sync");
		return KString{};
	}
    
	return m_SQL.SingleStringQuery ("select max({}) from {}", sColumnName, sTableName);
}

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertMongoFieldToMySQLColumn(KStringView sMongoField)
//-----------------------------------------------------------------------------
{
	kDebug(1, "converting MongoDB field '{}' to MySQL column", sMongoField);
    
	// Use the same field name conversion logic as for regular fields
	KString sColumnName = ConvertFieldNameToColumnName(sMongoField);
	KString sSanitized = SanitizeColumnName(sColumnName);
    
	kDebug(2, "MongoDB field '{}' -> MySQL column '{}'", sMongoField, sSanitized);
	return sSanitized;
}

//-----------------------------------------------------------------------------
void Mong2SQL::ProcessDocuments(const std::vector<KJSON>& documents, KStringView sCollectionName)
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	kDebug(2, "processing {} documents for collection '{}'", documents.size(), sCollectionName);
	m_TableSchemas.clear();
	m_TableOrder.clear();
	m_RowCounters.clear();

	KString sTableName = ConvertCollectionNameToTableName(sCollectionName);
	TableSchema& rootTable = EnsureTableSchema(sTableName);

	if (m_Config.m_bCreateTables)
	{
		kDebug(1, "collecting schema information from {} documents", documents.size());
		std::size_t docCount = 0;
		for (const auto& document : documents)
		{
			CollectSchemaForDocument(document, rootTable, KString{});
			++docCount;
			if (docCount % 1000 == 0)
			{
				kDebug(2, "analyzed {} documents for schema", docCount);
			}
		}

		if (m_Config.bVerbose)
		{
			KOut.FormatLine(":: Generating DDL for {} tables", m_TableOrder.size());
		}
		kDebug(1, "generating DDL for {} tables", m_TableOrder.size());
		for (const auto& tableName : m_TableOrder)
		{
			kDebug(2, "generating DDL for table: {}", tableName);
			GenerateCreateTableSQL(m_TableSchemas.at(tableName));
		}
	}

	// Second pass: Generate INSERT statements for all documents
	if (m_Config.bVerbose)
	{
		KOut.FormatLine(":: inserting {} rows into {} ...", kFormNumber(documents.size()), sTableName);
	}
    
	std::size_t iSequence { 0 };
	KBAR pbar;
	if (m_Config.bVerbose && documents.size() > 1)
	{
		pbar.Start(documents.size());
	}
    
	for (const auto& document : documents)
	{
		++iSequence;
		try
		{
			EmitDocumentInsert(document, sTableName, iSequence);
			if (m_Config.bVerbose && documents.size() > 1)
			{
				pbar.Move();
			}
		}
		catch (const std::exception& ex)
		{
			// Break the progress bar if active
			if (m_Config.bVerbose && documents.size() > 1)
			{
				pbar.Break();
			}
            
			// Get the primary key value for error reporting
			KString sPrimaryKey = ExtractPrimaryKeyFromDocument(document);
			KErr.FormatLine(">> insert into {}, row {}: $oid={}, failed: {}", sTableName, iSequence, sPrimaryKey, ex.what());
			if (m_Config.bHasDBC && !m_SQL.GetLastError().empty())
			{
				KErr.FormatLine(">> MySQL error: {}", m_SQL.GetLastError());
			}
            
			// Crash and do not continue
			std::exit(1);
		}
	}
    
	if (m_Config.bVerbose && documents.size() > 1)
	{
		pbar.Finish();
	}

} // ProcessDocuments

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertCollectionNameToTableName(KStringView sCollectionName)
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	KString sTableName = sCollectionName;
	sTableName = sTableName.ToUpper();

	if (sTableName.ends_with('S') && sTableName.size() > 1)
	{
		sTableName.remove_suffix(1);
	}

	return sTableName;

} // ConvertCollectionNameToTableName

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertFieldNameToColumnName(KStringView sFieldName)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");
	KString sColumnName;

	for (auto ch : sFieldName)
	{
		if (std::isupper(static_cast<unsigned char>(ch)) && !sColumnName.empty())
		{
			sColumnName += '_';
		}
		sColumnName += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	}

	return sColumnName;

} // ConvertFieldNameToColumnName

//-----------------------------------------------------------------------------
KString Mong2SQL::GenerateTableHash(KStringView sTableName)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	KString sLowerTableName = sTableName;
	sLowerTableName = sLowerTableName.ToLower();
	return kFormat("{}_hash", sLowerTableName);

} // GenerateTableHash

//-----------------------------------------------------------------------------
Mong2SQL::TableSchema& Mong2SQL::EnsureTableSchema(KStringView sTableName, KStringView sParentTable, KStringView sParentKeyColumn)
//-----------------------------------------------------------------------------
{
	kDebug(2, "ensuring schema for table: {}", sTableName);
	KString sKey = sTableName;
	auto it = m_TableSchemas.find(sKey);

	if (it == m_TableSchemas.end())
	{
		TableSchema schema;
		schema.sTableName = sKey;
		schema.sPrimaryKey = GenerateTableHash(schema.sTableName);
		schema.sParentTable = sParentTable;
		schema.sParentKeyColumn = sParentKeyColumn;

		auto result = m_TableSchemas.emplace(schema.sTableName, std::move(schema));
		it = result.first;
		m_TableOrder.push_back(it->first);
		m_RowCounters[it->first] = 0;
		kDebug(2, "created new table schema: {} (total tables: {})", schema.sTableName, m_TableOrder.size());

		if (!sParentTable.empty())
		{
			std::size_t iParentLength { 0 };
			auto parentIt = m_TableSchemas.find(KString(sParentTable));
			if (parentIt != m_TableSchemas.end())
			{
				auto parentColIt = parentIt->second.Columns.find(KString(sParentKeyColumn));
				if (parentColIt != parentIt->second.Columns.end())
				{
					iParentLength = parentColIt->second.iMaxLength;
				}
			}
			if (iParentLength == 0)
			{
				iParentLength = 16;
			}
			AddColumn(it->second, KString(sParentKeyColumn), SqlType::Text, iParentLength, /*isNullable*/false);
		}
	}
	else if (!sParentTable.empty())
	{
		it->second.sParentTable = sParentTable;
		it->second.sParentKeyColumn = sParentKeyColumn;
		std::size_t iParentLength { 0 };
		auto parentIt = m_TableSchemas.find(KString(sParentTable));
		if (parentIt != m_TableSchemas.end())
		{
			auto parentColIt = parentIt->second.Columns.find(KString(sParentKeyColumn));
			if (parentColIt != parentIt->second.Columns.end())
			{
				iParentLength = parentColIt->second.iMaxLength;
			}
		}
		if (iParentLength == 0)
		{
			iParentLength = 16;
		}
		AddColumn(it->second, KString(sParentKeyColumn), SqlType::Text, iParentLength, /*isNullable*/false);
	}

	return it->second;

} // EnsureTableSchema

//-----------------------------------------------------------------------------
void Mong2SQL::CollectSchemaForDocument(const KJSON& document, TableSchema& table, const KString& sPrefix)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	if (!document.is_object())
	{
		return;
	}
    
	kDebug(3, "collecting schema for document with {} fields, prefix: '{}'", document.size(), sPrefix);

	for (auto it = document.begin(); it != document.end(); ++it)
	{
		// Check for MongoDB _id field and mark table as having ObjectId
		if (it.key() == "_id" && it->is_object() && it->contains("$oid"))
		{
			table.bHasObjectId = true;
			std::size_t iLength = std::max<std::size_t>(24, 16); // MongoDB ObjectId is 24 chars
			AddColumn(table, table.sPrimaryKey, SqlType::Text, iLength, /*isNullable*/false);
			continue; // Skip further processing of _id field
		}
		
		KString sKey = ConvertFieldNameToColumnName(it.key());
		KString sRawColumnName = BuildColumnName(sPrefix, sKey);
		KString sSanitizedColumnName = SanitizeColumnName(sRawColumnName);
		const auto& value = *it;
        
		kDebug(3, "processing field '{}' -> '{}' -> '{}', type: {}", it.key(), sRawColumnName, sSanitizedColumnName, 
			value.is_object() ? "object" : value.is_array() ? "array" : value.is_string() ? "string" : 
			value.is_number() ? "number" : value.is_boolean() ? "boolean" : value.is_null() ? "null" : "unknown");

		if (value.is_object())
		{
			kDebug(3, "recursing into object field '{}'", sSanitizedColumnName);
			CollectSchemaForDocument(value, table, sSanitizedColumnName);
			kDebug(3, "returned from object field '{}'", sSanitizedColumnName);
		}
		else if (value.is_array())
		{
			kDebug(3, "processing array field '{}' with {} elements", sSanitizedColumnName, value.size());
			CollectArraySchema(value, table, sSanitizedColumnName);
			kDebug(3, "completed array field '{}'", sSanitizedColumnName);
		}
		else
		{
			std::size_t iLength { 0 };
			if (value.is_string())
			{
				iLength = value.String().size();
			}

			AddColumn(table, sSanitizedColumnName, InferSqlType(value), iLength);
		}
	}

} // CollectSchemaForDocument

//-----------------------------------------------------------------------------
void Mong2SQL::CollectArraySchema(const KJSON& array, TableSchema& parentTable, const KString& sKey)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	if (!array.is_array())
	{
		return;
	}
    
	kDebug(3, "collecting array schema for key '{}' with {} elements", sKey, array.size());
    
	// Skip empty arrays - no point creating a table with only a foreign key
	if (array.empty())
	{
		kDebug(3, "skipping empty array for key '{}' (no data to process)", sKey);
		return;
	}

	KString sChildTableName = SanitizeColumnName(BuildChildTableName(parentTable.sTableName, sKey));
	auto& childTable = EnsureTableSchema(sChildTableName, parentTable.sTableName, parentTable.sPrimaryKey);

	std::size_t iElementIndex = 0;
	for (const auto& element : array)
	{
		kDebug(3, "processing array element {} of {}", iElementIndex + 1, array.size());
		if (element.is_object())
		{
			kDebug(3, "array element {} is object with {} fields", iElementIndex + 1, element.size());
			CollectSchemaForDocument(element, childTable, KString{});
			kDebug(3, "completed array element {} object processing", iElementIndex + 1);
		}
		else if (element.is_array())
		{
			kDebug(3, "array element {} is nested array with {} elements", iElementIndex + 1, element.size());
			AddColumn(childTable, SanitizeColumnName(ExtractLeafColumnName(sKey)), SqlType::Text, 0);
		}
		else
		{
			KString sValueColumn = SanitizeColumnName(ExtractLeafColumnName(sKey));
			std::size_t iLength { 0 };
			if (element.is_string())
			{
				iLength = element.String().size();
			}
			if (sValueColumn == "_id_oid")
			{
				childTable.bHasObjectId = true;
				iLength = std::max<std::size_t>(iLength, 16);
				AddColumn(childTable, childTable.sPrimaryKey, SqlType::Text, iLength, /*isNullable*/false);
				continue;
			}

			AddColumn(childTable, sValueColumn, InferSqlType(element), iLength);
		}
		++iElementIndex;
	}
	kDebug(3, "completed processing {} array elements for key '{}'", array.size(), sKey);

} // CollectArraySchema

//-----------------------------------------------------------------------------
void Mong2SQL::AddColumn(TableSchema& table, const KString& sColumnName, SqlType type, std::size_t iLength, bool isNullable)
//-----------------------------------------------------------------------------
{
	kDebug(3, "adding column '{}' to table '{}', type: {}, length: {}, nullable: {}", 
		sColumnName, table.sTableName, static_cast<int>(type), iLength, isNullable);
	auto result = table.Columns.emplace(sColumnName, ColumnInfo{});
	auto& info = result.first->second;

	if (result.second)
	{
		table.ColumnOrder.push_back(sColumnName);
		info.Type = type;
		info.bNullable = isNullable;
	}
	else
	{
		info.Type = MergeSqlTypes(info.Type, type);
		if (!isNullable)
		{
			info.bNullable = false;
		}
	}

	if (info.Type == SqlType::Text)
	{
		if (iLength > info.iMaxLength)
		{
			info.iMaxLength = iLength;
		}
	}
	else
	{
		info.iMaxLength = 0;
	}

} // AddColumn

//-----------------------------------------------------------------------------
Mong2SQL::SqlType Mong2SQL::InferSqlType(const KJSON& value) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	if (value.is_null())
	{
		return SqlType::Text;
	}
	if (value.is_boolean())
	{
		return SqlType::Boolean;
	}
	if (value.is_number_integer() || value.is_number_unsigned())
	{
		return SqlType::Integer;
	}
	if (value.is_number_float())
	{
		return SqlType::Float;
	}
	if (value.is_string())
	{
		return SqlType::Text;
	}

	return SqlType::Text;

} // InferSqlType

//-----------------------------------------------------------------------------
Mong2SQL::SqlType Mong2SQL::MergeSqlTypes(SqlType existing, SqlType candidate) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	if (existing == candidate)
	{
		return existing;
	}

	if (existing == SqlType::Unknown)
	{
		return candidate;
	}
	if (candidate == SqlType::Unknown)
	{
		return existing;
	}
	if (existing == SqlType::Text || candidate == SqlType::Text)
	{
		return SqlType::Text;
	}
	if (existing == SqlType::Float || candidate == SqlType::Float)
	{
		return SqlType::Float;
	}
	if (existing == SqlType::Integer || candidate == SqlType::Integer)
	{
		return SqlType::Integer;
	}

	return SqlType::Text;

} // MergeSqlTypes

//-----------------------------------------------------------------------------
KString Mong2SQL::SqlTypeToString(const ColumnInfo& info) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	switch (info.Type)
	{
		case SqlType::Boolean: return "boolean";
		case SqlType::Integer: return "bigint";
		case SqlType::Float:   return "double";
		case SqlType::Text:
		case SqlType::Unknown:
		default:
		{
			if (info.iMaxLength == 0 || info.iMaxLength > 255)
			{
				return "text";
			}

			std::size_t iRounded = ((info.iMaxLength + 9) / 10) * 10;
			if (iRounded == 0)
			{
				iRounded = 10;
			}
			if (iRounded > 255)
			{
				iRounded = 255;
			}

			return kFormat("varchar({})", iRounded);
		}
	}

} // SqlTypeToString

//-----------------------------------------------------------------------------
KString Mong2SQL::SanitizeColumnName(KStringView sName) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "sName={} ...", sName);
	KString sResult = sName;
    
	// Remove $ signs
	sResult.Replace("$", "");
    
	// Replace dashes with underscores
	sResult.Replace("-", "_");
    
	// Remove hex hashes (24-character hex strings like MongoDB ObjectIds)
	std::size_t iPos = 0;
	while ((iPos = sResult.find('_', iPos)) != KString::npos)
	{
		if (iPos + 25 <= sResult.size())
		{
			KStringView sSegment = KStringView(sResult).Mid(iPos + 1, 24);
			if (IsHex24(sSegment))
			{
				// Found a 24-char hex string after underscore, remove it and the underscore
				sResult.erase(iPos, 25);
				continue; // Don't increment iPos, check the same position again
			}
		}
		iPos++; // Move to next position if no hex hash found
	}
    
	// Remove leading underscores and other punctuation
	while (!sResult.empty() && !std::isalnum(static_cast<unsigned char>(sResult[0])))
	{
		sResult.erase(0, 1);
	}
    
	// Collapse multiple consecutive underscores to single underscore
	std::size_t iDoublePos = 0;
	while ((iDoublePos = sResult.find("__", iDoublePos)) != KString::npos)
	{
		sResult.replace(iDoublePos, 2, "_");
		// Don't increment iDoublePos, check the same position again in case there were 3+ underscores
	}
    
	// If the result is empty after cleaning, use a default name
	if (sResult.empty())
	{
		sResult = "col";
	}
    
	// Check for reserved words (case-insensitive)
	if (IsReservedWord(sResult))
	{
		sResult += "_";
	}
    
	kDebug(3, "sanitized '{}' -> '{}'", sName, sResult);
	return sResult;

} // SanitizeColumnName

//-----------------------------------------------------------------------------
bool Mong2SQL::IsHex24(KStringView sValue)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	if (sValue.size() != 24)
	{
		return false;
	}
    
	for (auto ch : sValue)
	{
		if (!std::isxdigit(static_cast<unsigned char>(ch)))
		{
			return false;
		}
	}
    
	return true;

} // IsHex24

//-----------------------------------------------------------------------------
bool Mong2SQL::IsReservedWord(KStringView sValue) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	KString sUpper = sValue;
	sUpper = sUpper.ToUpper();
    
	return kStrIn(sUpper, MYSQL_RESERVED_WORDS, ',');

} // IsReservedWord

//-----------------------------------------------------------------------------
void Mong2SQL::EmitDocumentInsert(const KJSON& document, KStringView sTableName, std::size_t iSequence)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	auto& table = EnsureTableSchema(sTableName);
	std::size_t& iCounter = m_RowCounters[table.sTableName];
	++iCounter;

	KString sPrimaryValue = ExtractPrimaryKeyFromDocument(document);

	std::map<KString, KString> rowValues;
	rowValues[table.sPrimaryKey] = sPrimaryValue;

	FlattenDocument(document, KString{}, rowValues);

	GenerateInsertSQL(table, rowValues);

	ProcessNestedArrays(document, table, KString{}, sPrimaryValue);

} // EmitDocumentInsert

//-----------------------------------------------------------------------------
void Mong2SQL::ProcessNestedArrays(const KJSON& node, TableSchema& currentTable, const KString& sPrefix, const KString& sCurrentPKValue)
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	if (!node.is_object())
	{
		return;
	}

	for (auto it = node.begin(); it != node.end(); ++it)
	{
		KString sKey = ConvertFieldNameToColumnName(it.key());
		KString sColumnName = BuildColumnName(sPrefix, sKey);
		const auto& value = *it;

		if (value.is_array())
		{
			EmitArrayInserts(value, currentTable, sColumnName, sCurrentPKValue);
		}
		else if (value.is_object())
		{
			ProcessNestedArrays(value, currentTable, sColumnName, sCurrentPKValue);
		}
	}

} // ProcessNestedArrays

//-----------------------------------------------------------------------------
void Mong2SQL::EmitArrayInserts(const KJSON& array, TableSchema& parentTable, const KString& sKey, const KString& sParentPKValue)
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	if (!array.is_array())
	{
		return;
	}
    
	// Skip empty arrays - consistent with schema collection
	if (array.empty())
	{
		kDebug(2, "skipping empty array insert for key '{}'", sKey);
		return;
	}

	KString sChildTableName = BuildChildTableName(parentTable.sTableName, sKey);
	auto& childTable = EnsureTableSchema(sChildTableName, parentTable.sTableName, parentTable.sPrimaryKey);
	std::size_t& iCounter = m_RowCounters[childTable.sTableName];

	for (const auto& element : array)
	{
		++iCounter;
		// For array elements, generate a hash-based primary key since they don't have _id
		KString sInput = kFormat("{}:{}", childTable.sTableName, element.dump());
		auto iHashValue = kHash(sInput.c_str());
		KString sChildPrimary = kFormat("{:016x}", static_cast<unsigned long long>(iHashValue));

		std::map<KString, KString> rowValues;
		rowValues[childTable.sPrimaryKey] = sChildPrimary;
		if (!childTable.sParentKeyColumn.empty())
		{
			rowValues[childTable.sParentKeyColumn] = sParentPKValue;
		}

		if (element.is_object())
		{
			FlattenDocument(element, KString{}, rowValues);
			ProcessNestedArrays(element, childTable, KString{}, sChildPrimary);
		}
		else if (element.is_array())
		{
			rowValues[ExtractLeafColumnName(sKey)] = element.dump();
		}
		else
		{
			KString sValueColumn = ExtractLeafColumnName(sKey);
			rowValues[sValueColumn] = ToSqlLiteral(element);
		}

		GenerateInsertSQL(childTable, rowValues);
	}

} // EmitArrayInserts

//-----------------------------------------------------------------------------
void Mong2SQL::FlattenDocument(const KJSON& document, const KString& sPrefix, std::map<KString, KString>& rowValues)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	if (!document.is_object())
	{
		return;
	}

	for (auto it = document.begin(); it != document.end(); ++it)
	{
		KString sKey = ConvertFieldNameToColumnName(it.key());
		
		// Skip _id field - it's handled as primary key separately
		if (it.key() == "_id")
		{
			continue;
		}
		
		KString sColumnName = BuildColumnName(sPrefix, sKey);
		const auto& value = *it;

		if (value.is_object())
		{
			FlattenDocument(value, sColumnName, rowValues);
		}
		else if (value.is_array())
		{
			continue; // handled separately
		}
		else
		{
			rowValues[sColumnName] = ToSqlLiteral(value);
		}
	}

} // FlattenDocument

//-----------------------------------------------------------------------------
KString Mong2SQL::BuildColumnName(const KString& sPrefix, KStringView sKey) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");
	KString sResult;
	if (sPrefix.empty())
	{
		sResult = KString{sKey};
	}
	else
	{
		sResult = kFormat("{}_{}", sPrefix, sKey);
	}
    
	// Collapse multiple consecutive underscores to single underscore
	// This ensures consistency between schema collection and INSERT generation
	std::size_t iDoublePos = 0;
	while ((iDoublePos = sResult.find("__", iDoublePos)) != KString::npos)
	{
		sResult.replace(iDoublePos, 2, "_");
		// Don't increment iDoublePos, check the same position again in case there were 3+ underscores
	}
    
	kDebug(3, "built column name: prefix='{}' + key='{}' = '{}'", sPrefix, sKey, sResult);
	return sResult;

} // BuildColumnName

//-----------------------------------------------------------------------------
KString Mong2SQL::BuildChildTableName(KStringView sParentTable, KStringView sKey) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	KString sSuffix = sKey;
	KString sOriginalSuffix = sSuffix;
	sSuffix = sSuffix.ToUpper();
    
	// Better pluralization: remove "es" before "s" for words like "aliases" -> "alias"
	if (sSuffix.ends_with("ES") && sSuffix.size() > 2)
	{
		sSuffix.remove_suffix(2);
	}
	else if (sSuffix.ends_with('S') && sSuffix.size() > 1)
	{
		sSuffix.remove_suffix(1);
	}

	KString sResult;
	if (sSuffix.empty())
	{
		sResult = KString{sParentTable};
	}
	else
	{
		sResult = kFormat("{}_{}", sParentTable, sSuffix);
	}
    
	// Collapse multiple consecutive underscores to single underscore in table names too
	std::size_t iDoublePos = 0;
	while ((iDoublePos = sResult.find("__", iDoublePos)) != KString::npos)
	{
		sResult.replace(iDoublePos, 2, "_");
		// Don't increment iDoublePos, check the same position again in case there were 3+ underscores
	}
    
	kDebug(3, "built child table name: parent='{}' + key='{}' -> suffix='{}' = '{}'", 
		sParentTable, sOriginalSuffix, sSuffix, sResult);
	return sResult;

} // BuildChildTableName

//-----------------------------------------------------------------------------
KString Mong2SQL::ExtractLeafColumnName(KStringView sQualified) const
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");
	KString sColumn = sQualified;
	auto iPos = sColumn.rfind('_');
	if (iPos != KString::npos)
	{
		sColumn.erase(0, iPos + 1);
	}
	// Better pluralization: remove "es" before "s" for words like "aliases" -> "alias"
	if (sColumn.ends_with("es") && sColumn.size() > 2)
	{
		sColumn.remove_suffix(2);
	}
	else if (sColumn.ends_with('s') && sColumn.size() > 1)
	{
		sColumn.remove_suffix(1);
	}
	return sColumn;

} // ExtractLeafColumnName

//-----------------------------------------------------------------------------
KString Mong2SQL::ToSqlLiteral(const KJSON& value) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	if (value.is_null())
	{
		return "NULL";
	}
	else if (value.is_boolean())
	{
		return value.Bool() ? "1" : "0";
	}
	else if (value.is_number_integer())
	{
		return kFormat("{}", value.Int64());
	}
	else if (value.is_number_unsigned())
	{
		return kFormat("{}", value.UInt64());
	}
	else if (value.is_number_float())
	{
		return kFormat("{}", value.Float());
	}
	else if (value.is_string())
	{
		return value.String();
	}

	return value.dump();

} // ToSqlLiteral

//-----------------------------------------------------------------------------
KString Mong2SQL::EscapeSqlString(KStringView sValue)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	KString sEscaped;
	sEscaped.reserve(sValue.size());

	for (auto ch : sValue)
	{
		if (ch == '\\')
		{
			sEscaped += "\\\\";
		}
		else if (ch == '\'')
		{
			sEscaped += "''";
		}
		else
		{
			sEscaped += ch;
		}
	}

	return sEscaped;

} // EscapeSqlString

//-----------------------------------------------------------------------------
KString Mong2SQL::ExtractPrimaryKeyFromDocument(const KJSON& document) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	
	// Look for MongoDB _id field
	if (document.contains("_id") && document["_id"].is_object())
	{
		const auto& idObj = document["_id"];
		if (idObj.contains("$oid") && idObj["$oid"].is_string())
		{
			return idObj["$oid"].String();
		}
	}
	
	// Fallback: if no _id.$oid found, generate a hash (shouldn't happen with real MongoDB data)
	KString sInput = document.dump();
	auto iHashValue = kHash(sInput.c_str());
	return kFormat("{:016x}", static_cast<unsigned long long>(iHashValue));

} // ExtractPrimaryKeyFromDocument

//-----------------------------------------------------------------------------
void Mong2SQL::GenerateCreateTableSQL(const TableSchema& table)
//-----------------------------------------------------------------------------
{
	kDebug(1, "{}: generating DDL with {} columns", table.sTableName, table.ColumnOrder.size());
	if (table.ColumnOrder.empty())
	{
		kDebug(1, "{}: no columns [1]", table.sTableName);
		return;
	}

	std::vector<KString> columns;
	columns.reserve(table.ColumnOrder.size());
	for (const auto& column : table.ColumnOrder)
	{
		if (!table.bHasObjectId && column == table.sPrimaryKey)
		{
			continue; // suppress generated hash column when no _id present
		}
		columns.push_back(column);
	}

	if (columns.empty())
	{
		kDebug(1, "{}: no columns [2]", table.sTableName);
		return;
	}

	auto rank = [&](const KString& column) -> int
	{
		if (table.bHasObjectId && column == table.sPrimaryKey)
		{
			return 0;
		}
		if (column.ends_with("_hash"))
		{
			return 1;
		}
		return 2;
	};

	std::stable_sort(columns.begin(), columns.end(), [&](const KString& a, const KString& b)
	{
		int ra = rank(a);
		int rb = rank(b);
		if (ra != rb)
		{
			return ra < rb;
		}
		return a < b;
	});

	std::size_t iNameWidth { 0 };
	std::size_t iTypeWidth { 0 };
	std::vector<KString> types;
	types.reserve(columns.size());

	kDebug(1, "{}: calculating column sizes...", table.sTableName);
	for (const auto& column : columns)
	{
		const auto& info = table.Columns.at(column);
		KString sTypeString = SqlTypeToString(info);
		types.emplace_back(sTypeString);
		iNameWidth = std::max(iNameWidth, column.size());
		iTypeWidth = std::max(iTypeWidth, sTypeString.size());
	}

	kDebug(1, "{}: forming column definitions...", table.sTableName);
	auto formatColumn = [&](const KString& sColumn, const KString& sTypeString, const ColumnInfo& info, bool bLast)
	{
		const bool bIsPrimaryColumn = (table.bHasObjectId && sColumn == table.sPrimaryKey);

		KString sLine;
		sLine += "    ";
		sLine += kFormat("{:<{}}", sColumn, static_cast<int>(iNameWidth));
		sLine += "  ";
		sLine += kFormat("{:<{}}", sTypeString, static_cast<int>(iTypeWidth));
		sLine += "  ";
		sLine += (info.bNullable && !bIsPrimaryColumn) ? "null" : "not null";
		if (bIsPrimaryColumn)
		{
			sLine += " primary key";
		}
		if (!bLast)
		{
			sLine += ",";
		}
		return sLine;
	};

	std::vector<KString> columnLines;
	columnLines.reserve(columns.size());
	for (std::size_t i = 0; i < columns.size(); ++i)
	{
		const auto& sColumn = columns[i];
		const auto& info   = table.Columns.at(sColumn);
		columnLines.push_back(formatColumn(sColumn, types[i], info, i + 1 == columns.size()));
	}

	KString sDropSQL   = kFormat("drop table if exists {};", table.sTableName);
	KString sCreateSQL = kFormat("create table {} (\n", table.sTableName);
	for (const auto& sLine : columnLines)
	{
		sCreateSQL += sLine;
		sCreateSQL += "\n";
	}
	sCreateSQL += ");";

	kDebug(1, "{}: issuing create table...", table.sTableName);
	if (m_Config.bOutputToStdout)
	{
		KOut.WriteLine(sDropSQL);
		KOut.WriteLine("# ----------------------------------------------------------------------");
		KOut.FormatLine("create table {}", table.sTableName);
		KOut.WriteLine("# ----------------------------------------------------------------------");
		KOut.WriteLine("(");
		for (const auto& sLine : columnLines)
		{
			KOut.WriteLine(sLine);
		}
		KOut.WriteLine(");");
		KOut.WriteLine();
	}
	else
	{
		if (!m_SQL.ExecSQL("{}", sDropSQL))
		{
			KErr.FormatLine(">> failed to execute SQL: {}", sDropSQL);
		}
		if (!m_SQL.ExecSQL("{}", sCreateSQL))
		{
			KErr.FormatLine(">> failed to execute SQL: {}", sCreateSQL);
		}
	}

	kDebug(1, "{}: building indexes...", table.sTableName);
	std::size_t iIndexCounter { 0 };
	for (const auto& sColumn : table.ColumnOrder)
	{
		if (sColumn == table.sPrimaryKey && table.bHasObjectId)
		{
			continue;
		}
		if (!sColumn.ends_with("_hash"))
		{
			continue;
		}

		++iIndexCounter;
		KString sIndexName = kFormat("IDX{:02d}", static_cast<int>(iIndexCounter));
		KString sIndexSQL  = kFormat("create index {} on {} ({});", sIndexName, table.sTableName, sColumn);

		if (m_Config.bOutputToStdout)
		{
			KOut.WriteLine(sIndexSQL);
			KOut.WriteLine();
		}
		else
		{
			if (!m_SQL.ExecSQL("{}", sIndexSQL))
			{
				KErr.FormatLine(">> failed to execute SQL: {}", sIndexSQL);
			}
		}
	}

	kDebug(1, "{}: done.", table.sTableName);

} // GenerateCreateTableSQL

//-----------------------------------------------------------------------------
void Mong2SQL::GenerateInsertSQL(const TableSchema& table, const std::map<KString, KString>& rowValues)
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	
	// Create KROW and populate with column values
	KROW row(table.sTableName);
	
	for (const auto& sColumn : table.ColumnOrder)
	{
		auto it = rowValues.find(sColumn);
		
		// Get column metadata from schema
		auto colIt = table.Columns.find(sColumn);
		if (colIt == table.Columns.end())
		{
			continue; // Skip columns not in schema
		}
		
		const auto& columnInfo = colIt->second;
		KCOL::Flags flags = KCOL::NOFLAG;
		
		// Set type-specific flags
		if (columnInfo.Type == SqlType::Boolean)
		{
			flags |= KCOL::BOOLEAN;
		}
		else if (columnInfo.Type == SqlType::Integer || columnInfo.Type == SqlType::Float)
		{
			flags |= KCOL::NUMERIC;
		}
		
		// Check if this is the primary key column
		if (table.bHasObjectId && sColumn == table.sPrimaryKey)
		{
			flags |= KCOL::PKEY;
		}
		
		if (it != rowValues.end())
		{
			// Add column with type info and max length
			row.AddCol(sColumn, it->second, flags, columnInfo.iMaxLength);
		}
		else
		{
			// Add NULL value for missing columns
			row.AddCol(sColumn, KString{}, flags, columnInfo.iMaxLength);
		}
	}
	
	// Generate INSERT/UPDATE statement using KSQL
	if (m_Config.bOutputToStdout)
	{
		KOut.FormatLine("{}", row.FormInsert(m_SQL.GetDBType(), /*bIdentityInsert=*/false, /*bIgnore=*/false));
	}
	else
	{
		auto bOK = m_SQL.Insert (row);

		if (!bOK && m_SQL.WasDuplicateError())
		{
			bOK = m_SQL.Update (row);
		}

		if (!bOK)
		{
			throw std::runtime_error(m_SQL.GetLastError());
		}
	}

} // GenerateInsertSQL

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return Mong2SQL().Main(argc, argv);
	}
	catch (const std::exception& ex)
	{
		kPrintLine(KErr, ">> {}: {}", Mong2SQL::s_sProjectName, ex.what());
	}

	return 1;

} // main
