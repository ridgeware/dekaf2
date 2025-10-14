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
#include <random>
#include <thread>
#include <chrono>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

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
		"notes:\n"
		"  * one of -in or -mongo needs to be specified\n"
		"  * if no connection parms are specified for MySQL, then SQL is just\n"
		"    generated to stdout\n"
		"  * -continue mode requires both -mongo and -dbc connections\n"
		"  * verbosity is automatically enabled when using -dbc\n"
		"\n"
		"special actions:\n"
		"  all       :: processes all collections in the MongoDB database (requires -mongo)\n"
		"  list      :: lists all collections with sizes, sorted smallest to largest (requires -mongo)\n"
	);
	Options.SetAdditionalArgDescription("<collection>|list|all");

	Options.Option("in <file>")
		.Help("use input file a full dump of the collection")
		.Set(m_Config.sInputFile);

	Options.Option("mongo <connection_string>")
		.Help("specify the MongoDB connection string")
		.Set(m_Config.sMongoConnectionString);

	Options.Option("prefix <table_prefix>")
		.Help("specify a prefix for all MySQL table names (will be uppercased and end with _)")
		.Set(m_Config.sTablePrefix);

	Options.Option("dbc <file>")
		.Help("specify the MySQL database connection (DBC file)")
		.Set(m_Config.sDBC);

	Options.Option("continue <lastmod_key>")
		.Help("enable delta sync mode using specified MongoDB field for incremental updates")
		.Set(m_Config.sContinueField)
		([&]()
		{
			m_Config.bContinueMode = true;
			m_Config.m_bCreateTables = false;  // Don't overwrite existing tables in continue mode
		});

	Options.Option("v,verbose")
		.Help("enable verbose output with progress information")
		([&]()
		{
			m_Config.bVerbose = true;
		});

	Options.Option("q,quiet")
		.Help("disable verbose output")
		([&]()
		{
			m_Config.bVerbose = false;
		});

	Options.Option("V,version")
		.Help("show version information")
		([&]()
		{
			ShowVersion();
		});

	Options.Option("nodata")
		.Help("generate DDL only, skip data inserts")
		([&]()
		{
			m_Config.bNoData = true;
		});

	Options.Option("first")
		.Help("first sync mode: skip collections if target MySQL table already exists")
		([&]()
		{
			m_Config.bFirstSynch = true;
		});


	Options.UnknownCommand([&](KOptions::ArgList& Args)
	{
		kDebug (3, "unknown arg lambda");
		if (!Args.empty())
		{
			m_Config.sCollectionName = Args.pop();
			kDebug (3, "collection set to: {}", m_Config.sCollectionName);
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
    
	Verbose(1, "  MySQL: connecting: {} ...", m_SQL.ConnectSummary());
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
	
	// Check for "all" collections special case
	if (m_Config.sCollectionName.ToLower() == "all")
	{
		if (!m_Config.sInputFile.empty())
		{
			KErr.FormatLine(">> error: collection 'all' cannot be used with file input (-in flag)");
			return;
		}
		
		// Get all collections from MongoDB
		std::vector<KString> collections = GetAllCollectionsFromMongoDB();
		if (collections.empty())
		{
			KErr.FormatLine(">> no collections found in MongoDB");
			return;
		}
		
		// Process each collection
		for (const auto& collection : collections)
		{
			m_Config.sCollectionName = collection;
			ProcessCollection();
		}
		
		return;
	}
	
	// Check for "list" collections special case
	if (m_Config.sCollectionName.ToLower() == "list")
	{
		if (!m_Config.sInputFile.empty())
		{
			KErr.FormatLine(">> error: collection 'list' cannot be used with file input (-in flag)");
			return;
		}
		
		if (m_Config.sMongoConnectionString.empty())
		{
			KErr.FormatLine(">> error: collection 'list' requires MongoDB connection (-mongo flag)");
			return;
		}
		
		// List all collections with sizes
		ListAllCollectionsWithSizes();
		return;
	}
	
	// Normal single collection processing
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
	Verbose(2, "Reading from file: {}", m_Config.sInputFile);
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
	
	// Initialize MongoDB driver and establish connection
	InitializeMongoDB();
	if (!ConnectToMongoDB())
	{
		return false;
	}
	
	const int MAX_RETRIES = 3;
	int iRetryCount = 0;
	
	while (iRetryCount <= MAX_RETRIES)
	{
		try
		{
			mongocxx::uri uri{m_Config.sMongoConnectionString.c_str()};
			KString sDatabaseName = uri.database();
			if (sDatabaseName.empty())
			{
				sDatabaseName = "test"; // MongoDB default database
			}

			auto database   = (*m_MongoClient)[sDatabaseName.c_str()];
			auto collection = database[m_Config.sCollectionName.c_str()];

			// Look up collection size if available
			auto sizeIt = m_CollectionSizes.find(m_Config.sCollectionName);
			if (sizeIt != m_CollectionSizes.end())
			{
				Verbose(1, "MongoDB: {}: querying collection ({})...", m_Config.sCollectionName, kFormBytes(sizeIt->second));
			}
			else
			{
				Verbose(1, "MongoDB: {}: querying collection...", m_Config.sCollectionName);
			}
			auto cursor = collection.find({});

			std::size_t iDocCount = 0;
			for (const auto& doc : cursor)
			{
				KString sJsonDoc = bsoncxx::to_json(doc);
				
				// Sanitize MongoDB JSON to handle BSON values that aren't valid JSON (nan, inf, etc.)
				sJsonDoc = SanitizeMongoJSON(sJsonDoc);
				
				KJSON document;
				KString sError;
				if (!kjson::Parse(document, sJsonDoc, sError))
				{
					KErr.FormatLine(">> FATAL: failed to parse MongoDB document: {} - Error: {}", sJsonDoc, sError);
					KErr.FormatLine(">> This indicates invalid JSON from MongoDB. Terminating.");
					std::exit(1);
				}

				documents.emplace_back(std::move(document));
				++iDocCount;
				if (iDocCount % 1000 == 0)
				{
					kDebug(2, "loaded {} documents from MongoDB", iDocCount);
				}
			}
			Verbose (1, "MongoDB: {}: loaded {} documents off collection", m_Config.sCollectionName, kFormNumber(iDocCount));
			
			// Success - break out of retry loop
			break;
		}
		catch (const std::exception& ex)
		{
			KString sErrorMsg = ex.what();
			bool bIsTimeoutError = sErrorMsg.contains("timeout") || 
								   sErrorMsg.contains("socket error") || 
								   sErrorMsg.contains("getMore") ||
								   sErrorMsg.contains("network") ||
								   sErrorMsg.contains("connection");
			
			if (bIsTimeoutError && iRetryCount < MAX_RETRIES)
			{
				// Generate random sleep time between 5-30 seconds
				std::random_device rd;
				std::mt19937 gen(rd());
				std::uniform_int_distribution<> dis(5, 30);
				int iSleepSeconds = dis(gen);
				
				Verbose(1, "MongoDB: {}: timeout/network error (attempt {} of {}), retrying in {} seconds...", 
						m_Config.sCollectionName, iRetryCount + 1, MAX_RETRIES + 1, iSleepSeconds);
				kDebug(1, "MongoDB error details: {}", sErrorMsg);
				
				std::this_thread::sleep_for(std::chrono::seconds(iSleepSeconds));
				
				// Clear documents for retry
				documents.clear();
				
				// Reconnect to MongoDB
				m_MongoClient.reset();
				if (!ConnectToMongoDB())
				{
					SetError(kFormat("MongoDB reconnection failed for collection '{}' after timeout", m_Config.sCollectionName));
					return false;
				}
				
				iRetryCount++;
			}
			else
			{
				// Non-timeout error or max retries exceeded
				if (bIsTimeoutError)
				{
					SetError(kFormat("MongoDB timeout error while processing collection '{}' (failed after {} retries): {}", 
									m_Config.sCollectionName, MAX_RETRIES + 1, sErrorMsg));
				}
				else
				{
					SetError(kFormat("MongoDB error while processing collection '{}': {}", m_Config.sCollectionName, sErrorMsg));
				}
				return false;
			}
		}
	}

	return true;

} // ProcessFromMongoDB

//-----------------------------------------------------------------------------
bool Mong2SQL::ProcessFromMongoDBDelta(std::vector<KJSON>& documents)
//-----------------------------------------------------------------------------
{
	Verbose (1, "MongoDB: synching {} off {}", m_Config.sCollectionName, m_Config.sContinueField);
    
	// Convert MongoDB field name to MySQL column name using existing rules
	KString sTableName = ConvertCollectionNameToTableName(m_Config.sCollectionName);
	KString sMySQLColumn = ConvertMongoFieldToMySQLColumn(m_Config.sContinueField);
    
	// Get the last modified timestamp from MySQL
	KString sLastModified = GetLastModifiedFromMySQL(sTableName, sMySQLColumn);
	Verbose (1, "  MySQL: max({}.{}) = {}", sTableName, sMySQLColumn, sLastModified);
    
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
std::vector<KString> Mong2SQL::GetAllCollectionsFromMongoDB()
//-----------------------------------------------------------------------------
{
	kDebug(1, "getting all collections from MongoDB");
	std::vector<KString> collections;
	
	if (m_Config.sMongoConnectionString.empty())
	{
		SetError("MongoDB connection string is required for 'all' collections");
		return collections;
	}

	// Initialize MongoDB driver and establish connection
	InitializeMongoDB();
	if (!ConnectToMongoDB())
	{
		return {};
	}

	try
	{
		mongocxx::uri uri{m_Config.sMongoConnectionString.c_str()};
		
		// Extract database name from connection string or use default
		KString sDatabaseName = uri.database();
		if (sDatabaseName.empty())
		{
			sDatabaseName = "test"; // MongoDB default database
		}
		
		auto database = (*m_MongoClient)[sDatabaseName.c_str()];
		
		Verbose(1, "MongoDB: listing (and sizing) collections ...");
		
		// Structure to hold collection name and size for sorting
		struct CollectionInfo {
			KString name;
			std::int64_t size;
		};
		std::vector<CollectionInfo> collectionInfos;
		
		// Get list of collections
		auto cursor = database.list_collections();
		for (const auto& doc : cursor)
		{
			auto nameElement = doc["name"];
			if (nameElement && nameElement.type() == bsoncxx::type::k_string)
			{
				auto stringView = nameElement.get_string().value;
				KString sCollectionName{stringView.data(), stringView.size()};
				
				// Skip system collections
				if (!sCollectionName.starts_with("system."))
				{
					kDebug(2, "found collection: {}", sCollectionName);
					
					// Get collection size using totalSize()
					std::int64_t collectionSize = 0;
					try
					{
						auto collection = database[sCollectionName.c_str()];
						
						// Run db.collection.totalSize() command
						auto command = bsoncxx::builder::stream::document{} 
							<< "collStats" << sCollectionName.c_str()
							<< bsoncxx::builder::stream::finalize;
						
						auto result = database.run_command(command.view());
						auto sizeElement = result.view()["totalSize"];
						
						if (sizeElement && (sizeElement.type() == bsoncxx::type::k_int32 || 
											sizeElement.type() == bsoncxx::type::k_int64))
						{
							if (sizeElement.type() == bsoncxx::type::k_int32)
							{
								collectionSize = sizeElement.get_int32().value;
							}
							else
							{
								collectionSize = sizeElement.get_int64().value;
							}
						}
						
						kDebug(2, "collection '{}' size: {} bytes", sCollectionName, collectionSize);
					}
					catch (const std::exception& ex)
					{
						kDebug(1, "warning: could not get size for collection '{}': {}", sCollectionName, ex.what());
						collectionSize = 0; // Default to 0 if size query fails
					}
					
					collectionInfos.push_back({sCollectionName, collectionSize});
				}
			}
		}
		
		// Sort collections by size (smallest to largest)
		std::sort(collectionInfos.begin(), collectionInfos.end(), 
			[](const CollectionInfo& a, const CollectionInfo& b) {
				return a.size < b.size;
			});
		
		// Extract sorted collection names and store sizes
		collections.reserve(collectionInfos.size());
		m_CollectionSizes.clear(); // Clear any previous data
		for (const auto& info : collectionInfos)
		{
			collections.push_back(info.name);
			m_CollectionSizes[info.name] = info.size;
		}
		
		Verbose(1, "MongoDB: found {} collections, sorted by size (smallest to largest)", collections.size());
	}
	catch (const std::exception& ex)
	{
		SetError(kFormat("MongoDB error while listing collections: {}", ex.what()));
		return collections;
	}
	
	return collections;
}

//-----------------------------------------------------------------------------
void Mong2SQL::ListAllCollectionsWithSizes()
//-----------------------------------------------------------------------------
{
	kDebug(1, "listing all collections with sizes from MongoDB");
	
	// Initialize MongoDB driver and establish connection
	InitializeMongoDB();
	if (!ConnectToMongoDB())
	{
		return;
	}
	
	try
	{
		mongocxx::uri uri{m_Config.sMongoConnectionString.c_str()};
		
		// Extract database name from connection string or use default
		KString sDatabaseName = uri.database();
		if (sDatabaseName.empty())
		{
			sDatabaseName = "test"; // MongoDB default database
		}
		
		auto database = (*m_MongoClient)[sDatabaseName.c_str()];
		
		// Structure to hold collection name and size for sorting
		struct CollectionInfo {
			KString name;
			std::int64_t size;
		};
		std::vector<CollectionInfo> collectionInfos;
		
		// Get list of collections
		auto cursor = database.list_collections();
		for (const auto& doc : cursor)
		{
			auto nameElement = doc["name"];
			if (nameElement && nameElement.type() == bsoncxx::type::k_string)
			{
				auto stringView = nameElement.get_string().value;
				KString sCollectionName{stringView.data(), stringView.size()};
				
				// Skip system collections
				if (!sCollectionName.starts_with("system."))
				{
					// Get collection size using totalSize()
					std::int64_t collectionSize = 0;
					try
					{
						auto collection = database[sCollectionName.c_str()];
						
						// Run db.collection.totalSize() command
						auto command = bsoncxx::builder::stream::document{} 
							<< "collStats" << sCollectionName.c_str()
							<< bsoncxx::builder::stream::finalize;
						
						auto result = database.run_command(command.view());
						auto sizeElement = result.view()["totalSize"];
						
						if (sizeElement && (sizeElement.type() == bsoncxx::type::k_int32 || 
											sizeElement.type() == bsoncxx::type::k_int64))
						{
							if (sizeElement.type() == bsoncxx::type::k_int32)
							{
								collectionSize = sizeElement.get_int32().value;
							}
							else
							{
								collectionSize = sizeElement.get_int64().value;
							}
						}
					}
					catch (const std::exception& ex)
					{
						kDebug(2, "failed to get size for collection '{}': {}", sCollectionName, ex.what());
						collectionSize = 0; // Default to 0 if size query fails
					}
					
					collectionInfos.push_back({sCollectionName, collectionSize});
				}
			}
		}
		
		// Sort collections by size (smallest to largest)
		std::sort(collectionInfos.begin(), collectionInfos.end(), 
			[](const CollectionInfo& a, const CollectionInfo& b) {
				return a.size < b.size;
			});
		
		// Output formatted list
		for (const auto& info : collectionInfos)
		{
			KOut.FormatLine("{:>8}  {}", kFormBytes(info.size), info.name);
		}
	}
	catch (const std::exception& ex)
	{
		SetError(kFormat("failed to list collections: {}", ex.what()));
	}
}

//-----------------------------------------------------------------------------
void Mong2SQL::VerboseImpl(int iLevel, const KString& sMessage) const
//-----------------------------------------------------------------------------
{
	if (m_Config.bVerbose)
	{
		KOut.FormatLine(":: {:%a %T}: {}", kNow(), sMessage);
	}
	else
	{
		kDebug(iLevel, "{}", sMessage);
	}
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
	
	// Check for first sync mode - skip if table already exists
	if (m_Config.bFirstSynch && TableExistsInMySQL(sTableName))
	{
		Verbose(1, "  MySQL: table {} already exists, skipping (-first mode)", sTableName);
		return;
	}
	
	TableSchema& rootTable = EnsureTableSchema(sTableName);
	
	// Root table (main collection) should always have a primary key
	if (rootTable.sPrimaryKey.empty())
	{
		rootTable.sPrimaryKey = GenerateTablePKEY(rootTable.sTableName);
	}

	if (m_Config.m_bCreateTables)
	{
		kDebug(1, "collecting schema information from {} documents", documents.size());
		std::size_t iDocCount = 0;
		for (const auto& document : documents)
		{
			CollectSchemaForDocument(document, rootTable, KString{});
			++iDocCount;
			if (iDocCount % 1000 == 0)
			{
				kDebug(2, "analyzed {} documents for schema", iDocCount);
			}
		}

		Verbose(1, "  MySQL: generating DDL for {} table{}", m_TableOrder.size(), (m_TableOrder.size() == 1) ? "" : "s");
		for (const auto& tableName : m_TableOrder)
		{
			const auto& tableSchema = m_TableSchemas.at(tableName);
			Verbose(1, "  MySQL: creating table {} with {} columns ...", tableName, tableSchema.Columns.size());
			GenerateCreateTableSQL(tableSchema);
		}
	}

	// Second pass: Generate INSERT statements for all documents (unless -nodata is specified)
	if (!m_Config.bNoData)
	{
		Verbose(1, "  MySQL: inserting {} rows into {} ...", kFormNumber(documents.size()), sTableName);
		
		std::size_t iSequence { 0 };
		KBAR pbar;
		if (m_Config.bVerbose && documents.size() > 1)
		{
			pbar.Start(documents.size());
		}
		
		for (const auto& document : documents)
		{
			++iSequence;
			
			// EmitDocumentInsert now handles all error reporting internally
			// It will exit(1) if a critical error occurs
			EmitDocumentInsert(document, sTableName, iSequence);
			
			if (m_Config.bVerbose && documents.size() > 1)
			{
				pbar.Move();
			}
		}
		
		if (m_Config.bVerbose && documents.size() > 1)
		{
			pbar.Finish();
		}
	}
	else
	{
		Verbose(1, "  MySQL: skipping data inserts (-nodata specified)");
	}

} // ProcessDocuments

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertCollectionNameToTableName(KStringView sCollectionName) const
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	
	// Don't break up compound words - just use the collection name as-is
	KString sTableName = KString{sCollectionName};
	
	// Strip leading and trailing punctuation (underscores, dashes, dots, etc.)
	while (!sTableName.empty() && !std::isalnum(sTableName.front()))
	{
		sTableName.erase(0, 1);
	}
	while (!sTableName.empty() && !std::isalnum(sTableName.back()))
	{
		sTableName.pop_back();
	}
	
	// Convert to uppercase
	sTableName = sTableName.ToUpper();

	// Apply proper singularization to the last word (after word breaking)
	sTableName = ApplySingularizationToTableName(sTableName);
	
	// Remove any trailing underscores that might have been left from word breaking
	while (!sTableName.empty() && sTableName.back() == '_')
	{
		sTableName.pop_back();
	}

	// Apply table prefix if specified
	KString sPrefix = NormalizeTablePrefix(m_Config.sTablePrefix);
	if (!sPrefix.empty())
	{
		sTableName = sPrefix + sTableName;
	}

	return sTableName;

} // ConvertCollectionNameToTableName

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertFieldNameToColumnName(KStringView sFieldName) const
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
KString Mong2SQL::NormalizeTablePrefix(KStringView sPrefix) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "normalizing table prefix: '{}'", sPrefix);
	
	if (sPrefix.empty())
	{
		return KString{};
	}
	
	KString sResult = KString{sPrefix}.ToUpper();
	
	// Remove any trailing underscores first
	while (!sResult.empty() && sResult.back() == '_')
	{
		sResult.pop_back();
	}
	
	// Add exactly one underscore at the end
	if (!sResult.empty())
	{
		sResult += '_';
	}
	
	kDebug(3, "normalized table prefix: '{}' -> '{}'", sPrefix, sResult);
	return sResult;
	
} // NormalizeTablePrefix

//-----------------------------------------------------------------------------
KString Mong2SQL::BreakupCompoundWords(KStringView sInput) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "breaking up compound words: '{}'", sInput);
	
	if (sInput.empty())
	{
		return KString{};
	}
	
	// Common English word patterns ordered by length (longest first) for proper matching
	static const std::vector<KString> commonWords = {
		// Longest words first (12+ chars)
		"translationprojects", "translatedcontents", "websiteqielements", "videotutorials",
		"notification", "translation", "translationproject", "websiteqi", "analytics", 
		"scheduler", "processor", "controller", "permission", "secondary", "tertiary",
		
		// 10-11 chars
		"translated", "translate", "internet", "explorer", "download", "advanced", 
		"standard", "internal", "external", "pipeline", "tutorial", "sitemap",
		
		// 8-9 chars  
		"document", "provider", "element", "service", "project", "content", "website",
		"desktop", "android", "windows", "firefox", "browser", "message", "warning",
		"success", "monitor", "counter", "manager", "handler", "template", "revision",
		"inactive", "disabled", "enabled", "premium", "primary", "unbabel",
		
		// 6-7 chars
		"report", "server", "client", "search", "filter", "button", "result", 
		"status", "member", "access", "session", "config", "setting", "option",
		"backup", "archive", "history", "version", "active", "public", "private",
		"remote", "global", "default", "custom", "special", "safari", "chrome",
		"metric", "worker", "layout", "style",
		
		// 5 chars
		"super", "inter", "trans", "multi", "under", "image", "video", "audio", 
		"upload", "login", "token", "value", "param", "cache", "draft", "final",
		"local", "basic", "macro", "extra", "queue", "theme", "alert", "error",
		"debug", "trace", "timer", "model", "linux",
		
		// 4 chars  
		"post", "semi", "anti", "over", "site", "page", "user", "admin", "link",
		"file", "sort", "list", "item", "menu", "form", "field", "input", "output",
		"type", "kind", "group", "team", "role", "auth", "temp", "lite", "mini",
		"micro", "meta", "main", "lead", "mail", "chat", "info", "stats", "view",
		"html", "json", "babel", "tier",
		
		// 4-6 chars (including plurals and common words)  
		"events", "event", "presses", "press", "plans", "plan",
		"elements", "projects", "contents", "services", "documents", "tutorials", "websites",
		"reports", "servers", "clients", "buttons", "results", "members", "sessions", 
		"configs", "settings", "options", "backups", "archives", "versions", "themes",
		"alerts", "errors", "timers", "models", "sites", "pages", "users", "admins", 
		"links", "files", "sorts", "lists", "items", "menus", "forms", "fields", 
		"inputs", "outputs", "types", "kinds", "groups", "teams", "roles", "mails", 
		"chats", "infos", "stats", "views", "logs", "jobs", "apps", "docs", "urls", 
		"keys", "apis", "nets", "zips",
		
		// 3 chars
		"web", "job", "log", "data", "api", "url", "key", "tmp", "pro", "net", 
		"app", "ios", "mac", "css", "xml", "csv", "pdf", "doc", "txt", "zip", 
		"tar", "sql",
		
		// 2 chars
		"un", "re", "up", "in", "on", "js", "db", "qi", "gz"
	};
	
	KString sResult;
	KString sLower = KString{sInput}.ToLower();
	std::size_t iPos = 0;
	
	while (iPos < sLower.size())
	{
		bool bFoundWord = false;
		std::size_t iBestMatch = 0;
		
		// Try to find the longest matching word starting at current position
		for (const auto& word : commonWords)
		{
			if (iPos + word.size() <= sLower.size() && 
				sLower.substr(iPos, word.size()) == word &&
				word.size() > iBestMatch)
			{
				// Make sure we're at a word boundary (not in the middle of a longer word)
				// Check that the next character (if exists) suggests end of word
				bool bValidBoundary = true;
				if (iPos + word.size() < sLower.size())
				{
					char nextChar = sLower[iPos + word.size()];
					// If next char is lowercase and current word doesn't end with common suffixes,
					// it might be part of a longer word
					if (std::islower(nextChar) && 
						!word.ends_with("ed") && !word.ends_with("er") && !word.ends_with("ing") &&
						!word.ends_with("ly") && !word.ends_with("tion") && !word.ends_with("sion") &&
						!word.ends_with("ness") && !word.ends_with("ment") && !word.ends_with("able") &&
						!word.ends_with("ible") && !word.ends_with("ful") && !word.ends_with("less"))
					{
						// Check if this might be a compound - look for vowel patterns
						if (word.size() >= 3) // Only for substantial words
						{
							bValidBoundary = true; // Accept it as a word boundary
						}
						else
						{
							bValidBoundary = false;
						}
					}
				}
				
				if (bValidBoundary)
				{
					iBestMatch = word.size();
					bFoundWord = true;
				}
			}
		}
		
		if (bFoundWord && iBestMatch > 0)
		{
			// Add the word with proper casing
			if (!sResult.empty())
			{
				sResult += "_";
			}
			
			KString sWord = sInput.substr(iPos, iBestMatch);
			sResult += sWord.ToUpper();
			iPos += iBestMatch;
		}
		else
		{
			// No word found - try to find a reasonable break point or add multiple characters
			std::size_t iNextPos = iPos + 1;
			
			// Look ahead to find a vowel pattern or reasonable break point
			// Don't break single characters unless absolutely necessary
			while (iNextPos < sLower.size() && iNextPos - iPos < 6)
			{
				// Look for vowel-consonant or consonant-vowel boundaries
				char currentChar = sLower[iNextPos - 1];
				char nextChar = sLower[iNextPos];
				
				bool isCurrentVowel = (currentChar == 'a' || currentChar == 'e' || currentChar == 'i' || 
									   currentChar == 'o' || currentChar == 'u');
				bool isNextVowel = (nextChar == 'a' || nextChar == 'e' || nextChar == 'i' || 
								   nextChar == 'o' || nextChar == 'u');
				
				// Break at vowel-consonant or consonant-vowel boundaries, but prefer longer segments
				if ((isCurrentVowel && !isNextVowel) || (!isCurrentVowel && isNextVowel))
				{
					if (iNextPos - iPos >= 3) // Minimum 3 characters
					{
						break;
					}
				}
				iNextPos++;
			}
			
			// Add the segment (minimum 1 character, but prefer longer segments)
			if (!sResult.empty() && sResult.back() != '_')
			{
				sResult += "_";
			}
			
			KString sSegment = sInput.substr(iPos, iNextPos - iPos);
			sResult += sSegment.ToUpper();
			iPos = iNextPos;
		}
	}
	
	// Clean up any double underscores
	std::size_t iDoublePos = 0;
	while ((iDoublePos = sResult.find("__", iDoublePos)) != KString::npos)
	{
		sResult.replace(iDoublePos, 2, "_");
	}
	
	// Remove leading/trailing underscores
	while (!sResult.empty() && sResult.front() == '_')
	{
		sResult.erase(0, 1);
	}
	while (!sResult.empty() && sResult.back() == '_')
	{
		sResult.pop_back();
	}
	
	kDebug(3, "compound words: '{}' -> '{}'", sInput, sResult);
	return sResult;
	
} // BreakupCompoundWords



//-----------------------------------------------------------------------------
KString Mong2SQL::ApplySingularizationToTableName(KStringView sTableName) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "applying singularization to table name: '{}'", sTableName);
	
	if (sTableName.empty())
	{
		return KString{};
	}
	
	KString sResult = sTableName;
	
	// Find the last word (after the last underscore)
	std::size_t iLastUnderscore = sResult.find_last_of('_');
	
	if (iLastUnderscore != KString::npos && iLastUnderscore < sResult.size() - 1)
	{
		// There's an underscore and it's not at the end
		KString sPrefix = sResult.substr(0, iLastUnderscore + 1); // Include the underscore
		KString sLastWord = sResult.substr(iLastUnderscore + 1);
		
		// Apply singularization to the last word only
		KString sLastWordLower = sLastWord.ToLower();
		
		// Special case: if the last word is just "S", remove it entirely (it's a plural marker)
		if (sLastWordLower == "s")
		{
			sResult = sPrefix.substr(0, sPrefix.size() - 1); // Remove the trailing underscore too
		}
		else
		{
			KString sSingularWord = ConvertPluralToSingular(sLastWordLower);
			sResult = sPrefix + sSingularWord.ToUpper();
		}
	}
	else
	{
		// No underscore, singularize the entire string
		KString sLower = sResult.ToLower();
		sResult = ConvertPluralToSingular(sLower).ToUpper();
	}
	
	kDebug(3, "singularized table name: '{}' -> '{}'", sTableName, sResult);
	return sResult;
	
} // ApplySingularizationToTableName

//-----------------------------------------------------------------------------
void Mong2SQL::InitializeMongoDB()
//-----------------------------------------------------------------------------
{
	kDebug(3, "initializing MongoDB driver");
	
	// Initialize MongoDB C++ driver instance (only once per application)
	static mongocxx::instance instance{};
	static bool initialized = false;
	
	if (!initialized)
	{
		kDebug(2, "MongoDB driver initialized");
		initialized = true;
	}
}

//-----------------------------------------------------------------------------
bool Mong2SQL::ConnectToMongoDB()
//-----------------------------------------------------------------------------
{
	kDebug(2, "establishing MongoDB connection");
	
	if (m_MongoClient)
	{
		kDebug(3, "MongoDB client already connected");
		return true;
	}
	
	try
	{
		mongocxx::uri uri{m_Config.sMongoConnectionString.c_str()};
		m_MongoClient = std::make_unique<mongocxx::client>(uri);
		
		Verbose(1, "MongoDB: connecting: {}", m_Config.sMongoConnectionString);
		kDebug(2, "MongoDB client connected successfully");
		return true;
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> failed to connect to MongoDB: {}", ex.what());
		m_MongoClient.reset();
		return false;
	}
}

//-----------------------------------------------------------------------------
KString Mong2SQL::SanitizeMongoJSON(KStringView sJsonDoc) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "sanitizing MongoDB JSON");
	
	KString sResult = sJsonDoc;
	
	// Replace MongoDB-specific invalid JSON values with valid JSON equivalents
	// MongoDB BSON supports these values, but they're not valid JSON
	
	// Handle 'nan' (Not a Number) - replace with null
	sResult.Replace(" : nan", " : null");
	sResult.Replace(": nan", ": null");
	sResult.Replace(" :nan", " :null");
	sResult.Replace(":nan", ":null");
	
	// Handle 'inf' and '-inf' (Infinity) - replace with null
	sResult.Replace(" : inf", " : null");
	sResult.Replace(": inf", ": null");
	sResult.Replace(" :inf", " :null");
	sResult.Replace(":inf", ":null");
	sResult.Replace(" : -inf", " : null");
	sResult.Replace(": -inf", ": null");
	sResult.Replace(" :-inf", " :null");
	sResult.Replace(":-inf", ":null");
	
	// Handle other potential MongoDB-specific values that aren't valid JSON
	// Note: MongoDB dates and ObjectIds are already handled properly by bsoncxx::to_json()
	
	return sResult;
}

//-----------------------------------------------------------------------------
bool Mong2SQL::TableExistsInMySQL(KStringView sTableName)
//-----------------------------------------------------------------------------
{
	kDebug(2, "checking if table '{}' exists in MySQL", sTableName);
	
	if (!m_Config.bHasDBC)
	{
		// If no database connection, we can't check - assume it doesn't exist
		kDebug(3, "no database connection, assuming table does not exist");
		return false;
	}
	
	// Query INFORMATION_SCHEMA to check if table exists
	auto sResult = m_SQL.SingleStringQuery("SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '{}'", sTableName);
	
	if (!sResult.empty())
	{
		auto count = sResult.Int64();
		bool bExists = (count > 0);
		kDebug(2, "table '{}' exists: {}", sTableName, bExists ? "yes" : "no");
		return bExists;
	}
	else
	{
		kDebug(1, "failed to check table existence: {}", m_SQL.GetLastError());
		return false; // Assume it doesn't exist if query fails
	}
}

//-----------------------------------------------------------------------------
KString Mong2SQL::GenerateTablePKEY(KStringView sTableName)
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	KString sLowerTableName = sTableName;
	sLowerTableName = sLowerTableName.ToLower();
	return kFormat("{}_oid", sLowerTableName);

} // GenerateTablePKEY

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
		// Don't automatically generate primary key - only set when _id field is found
		schema.sPrimaryKey = KString{}; // Empty until _id is discovered
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
			// Set the primary key name now that we found an _id field
			if (table.sPrimaryKey.empty())
			{
				table.sPrimaryKey = GenerateTablePKEY(table.sTableName);
			}
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
	
	// Ensure child table has a primary key for potential grandchild relationships
	if (childTable.sPrimaryKey.empty())
	{
		childTable.sPrimaryKey = GenerateTablePKEY(childTable.sTableName);
	}

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
			// Recursively handle nested arrays - create proper child table with foreign keys
			KString sNestedKey = SanitizeColumnName(ExtractLeafColumnName(sKey));
			CollectArraySchema(element, childTable, sNestedKey);
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
				// Set the primary key name now that we found an _id field
				if (childTable.sPrimaryKey.empty())
				{
					childTable.sPrimaryKey = GenerateTablePKEY(childTable.sTableName);
				}
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
				// TEXT can store up to 65,535 bytes (64KB)
				// If we're getting close to that limit, use LONGTEXT instead
				if (info.iMaxLength > 60000) // Leave some safety margin below 65KB
				{
					return "longtext";
				}
				else
				{
					return "text";
				}
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
	
	// MySQL column name limit is 64 characters - truncate if necessary
	if (sResult.size() > 64)
	{
		Verbose (1, "truncating column name '{}' from {} to 64 characters", sResult, sResult.size());
		sResult = sResult.Left(64);
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

	// Insert the row - if it fails critically, the method will handle error reporting
	if (!InsertOrUpdateOneRow(table, rowValues, sPrimaryValue, iSequence))
	{
		// Critical error occurred - stop processing
		std::exit(1);
	}

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
		std::map<KString, KString> rowValues;
		
		// Add parent foreign key if needed
		if (!childTable.sParentKeyColumn.empty())
		{
			rowValues[childTable.sParentKeyColumn] = sParentPKValue;
		}

		KString sChildPrimary; // Will remain empty if no _id exists
		
		if (element.is_object())
		{
			// Extract primary key from _id field if it exists
			sChildPrimary = ExtractPrimaryKeyFromDocument(element);
			if (!sChildPrimary.empty() && !childTable.sPrimaryKey.empty())
			{
				rowValues[childTable.sPrimaryKey] = sChildPrimary;
			}
			
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

		// Insert the array element row - if it fails critically, the method will handle error reporting
		KString sPrimaryKeyForError = sChildPrimary.empty() ? sParentPKValue : sChildPrimary;
		if (!InsertOrUpdateOneRow(childTable, rowValues, sPrimaryKeyForError, iCounter))
		{
			// Critical error occurred - stop processing
			std::exit(1);
		}
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
KString Mong2SQL::ConvertPluralToSingular(KStringView sPlural) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "converting plural '{}' to singular", sPlural);
	
	if (sPlural.empty())
	{
		return KString{sPlural};
	}
	
	KString sLower = KString{sPlural}.ToLower();
	
	// Handle irregular plurals first (most important exceptions)
	static const std::map<KStringView, KStringView> irregulars = {
		// Common irregular plurals
		{"children", "child"},
		{"people", "person"},
		{"men", "man"},
		{"women", "woman"},
		{"feet", "foot"},
		{"teeth", "tooth"},
		{"geese", "goose"},
		{"mice", "mouse"},
		{"oxen", "ox"},
		
		// Words ending in 'f' or 'fe' that become 'ves'
		{"lives", "life"},
		{"wives", "wife"},
		{"knives", "knife"},
		{"leaves", "leaf"},
		{"shelves", "shelf"},
		{"wolves", "wolf"},
		{"halves", "half"},
		{"calves", "calf"},
		
		// Latin/Greek plurals
		{"data", "datum"},
		{"criteria", "criterion"},
		{"phenomena", "phenomenon"},
		{"alumni", "alumnus"},
		{"cacti", "cactus"},
		{"fungi", "fungus"},
		{"nuclei", "nucleus"},
		{"syllabi", "syllabus"},
		{"analyses", "analysis"},
		{"bases", "basis"},
		{"crises", "crisis"},
		{"diagnoses", "diagnosis"},
		{"hypotheses", "hypothesis"},
		{"oases", "oasis"},
		{"parentheses", "parenthesis"},
		{"synopses", "synopsis"},
		{"theses", "thesis"},
		
		// Words that don't change
		{"sheep", "sheep"},
		{"deer", "deer"},
		{"fish", "fish"},
		{"species", "species"},
		{"series", "series"},
		{"means", "means"},
		{"aircraft", "aircraft"},
		{"spacecraft", "spacecraft"}
	};
	
	auto it = irregulars.find(sLower);
	if (it != irregulars.end())
	{
		kDebug(3, "found irregular plural '{}' -> '{}'", sPlural, it->second);
		return KString{it->second};
	}
	
	// Handle regular pluralization rules
	KString sResult = sLower;
	
	// Words ending in 'ies' -> 'y' (e.g., "cities" -> "city", "countries" -> "country")
	if (sResult.ends_with("ies") && sResult.size() > 3)
	{
		sResult.remove_suffix(3);
		sResult += 'y';
	}
	// Words ending in 'oes' -> 'o' (e.g., "heroes" -> "hero", "potatoes" -> "potato")
	else if (sResult.ends_with("oes") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	// Words ending in 'ves' -> 'f' (e.g., "thieves" -> "thief")
	else if (sResult.ends_with("ves") && sResult.size() > 3)
	{
		sResult.remove_suffix(3);
		sResult += 'f';
	}
	// Words ending in 'ses' -> 's' (e.g., "classes" -> "class", "glasses" -> "glass")
	else if (sResult.ends_with("ses") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	// Words ending in 'ches' -> 'ch' (e.g., "churches" -> "church", "beaches" -> "beach")
	else if (sResult.ends_with("ches") && sResult.size() > 4)
	{
		sResult.remove_suffix(2);
	}
	// Words ending in 'shes' -> 'sh' (e.g., "dishes" -> "dish", "brushes" -> "brush")
	else if (sResult.ends_with("shes") && sResult.size() > 4)
	{
		sResult.remove_suffix(2);
	}
	// Words ending in 'xes' -> 'x' (e.g., "boxes" -> "box", "taxes" -> "tax")
	else if (sResult.ends_with("xes") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	// Words ending in 'zes' -> 'z' (e.g., "quizzes" -> "quiz")
	else if (sResult.ends_with("zes") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	// Special case: words ending in 'es' where 'e' is part of the root
	// This handles "locales" -> "locale", "templates" -> "template", etc.
	else if (sResult.ends_with("es") && sResult.size() > 2)
	{
		// Check if removing just 's' makes sense (keep the 'e')
		KString sCandidate = sResult;
		sCandidate.remove_suffix(1); // Remove just the 's', keep the 'e'
		
		// List of words where we should keep the 'e'
		static const std::set<KStringView> keepE = {
			"locale", "template", "attribute", "module", "profile", "schedule",
			"console", "service", "resource", "response", "message", "package",
			"interface", "database", "cache", "image", "file", "table", "node",
			"route", "source", "device", "engine", "frame", "scene", "theme",
			"scheme", "volume", "phrase", "clause", "course", "house", "mouse",
			"horse", "nurse", "purse", "verse", "case", "base", "phase", "chase",
			"site", "page", "type", "name", "time", "line", "code", "mode", "role",
			"rule", "style", "title", "value", "score", "store", "state", "stage",
			"office", "notice", "choice", "voice", "price", "slice", "advice",
			"device", "service", "practice", "justice", "police", "finance",
			"affiliate", "associate", "candidate", "delegate", "estimate", "template",
			"pipeline", "baseline", "outline", "online", "offline", "antine"
		};
		
		// Check if the candidate word itself is in keepE, or if it ends with a word in keepE
		bool bKeepE = false;
		if (keepE.find(sCandidate) != keepE.end())
		{
			bKeepE = true;
		}
		else
		{
			// Check if the word ends with any word in keepE (for compound words)
			for (const auto& sKeepWord : keepE)
			{
				if (sCandidate.ends_with(sKeepWord))
				{
					bKeepE = true;
					break;
				}
			}
		}
		
		if (bKeepE)
		{
			sResult = sCandidate;
		}
		else
		{
			// Default: remove 'es' entirely
			sResult.remove_suffix(2);
		}
	}
	// Simple plural: just remove 's' (e.g., "users" -> "user", "items" -> "item")
	else if (sResult.ends_with('s') && sResult.size() > 1)
	{
		sResult.remove_suffix(1);
	}
	
	kDebug(3, "converted plural '{}' -> singular '{}'", sPlural, sResult);
	return sResult;
}

//-----------------------------------------------------------------------------
KString Mong2SQL::BuildChildTableName(KStringView sParentTable, KStringView sKey) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "...");
	KString sSuffix = sKey;
	KString sOriginalSuffix = sSuffix;
	
	// Convert to singular using English pluralization rules
	sSuffix = ConvertPluralToSingular(sSuffix);
	sSuffix = sSuffix.ToUpper();

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
	
	// Add FOREIGN KEY constraint if this is a child table
	// NOTE: Commented out due to dependency issues in DDL execution
	/*
	if (!table.sParentTable.empty() && !table.sParentKeyColumn.empty())
	{
		// Find the foreign key column name (should be parent table name + "_oid")
		KString sForeignKeyColumn = table.sParentKeyColumn;
		sCreateSQL += kFormat("    ,constraint fk_{}_{} foreign key ({}) references {} ({})\n", 
			table.sTableName.ToLower(), table.sParentTable.ToLower(),
			sForeignKeyColumn, table.sParentTable, sForeignKeyColumn);
	}
	*/
	
	sCreateSQL += ") engine=InnoDB default charset=utf8mb4;";

	kDebug(1, "{}: issuing create table...", table.sTableName);
	if (m_Config.bOutputToStdout)
	{
		KOut.FormatLine("{};", sDropSQL);
		KOut.WriteLine("# ----------------------------------------------------------------------");
		KOut.FormatLine("create table {}", table.sTableName);
		KOut.WriteLine("# ----------------------------------------------------------------------");
		KOut.WriteLine("(");
		for (const auto& sLine : columnLines)
		{
			KOut.WriteLine(sLine);
		}
		
		// Add FOREIGN KEY constraint if this is a child table
		// NOTE: Commented out due to dependency issues in DDL execution
		/*
		if (!table.sParentTable.empty() && !table.sParentKeyColumn.empty())
		{
			KString sForeignKeyColumn = table.sParentKeyColumn;
			KOut.FormatLine("    ,constraint fk_{}_{} foreign key ({}) references {} ({})", 
				table.sTableName.ToLower(), table.sParentTable.ToLower(),
				sForeignKeyColumn, table.sParentTable, sForeignKeyColumn);
		}
		*/
		
		KOut.WriteLine(") engine=InnoDB default charset=utf8mb4;");
		KOut.WriteLine();
	}
	else
	{
		if (!m_SQL.ExecSQL("{}", sDropSQL))
		{
			KErr.FormatLine(">> failed to execute SQL: {}\n{}", m_SQL.GetLastError(), sDropSQL);
		}
		if (!m_SQL.ExecSQL("{}", sCreateSQL))
		{
			KErr.FormatLine(">> failed to execute SQL: {}\n{}", m_SQL.GetLastError(), sCreateSQL);
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
bool Mong2SQL::InsertOrUpdateOneRow(const TableSchema& table, const std::map<KString, KString>& rowValues, const KString& sPrimaryKey, std::size_t iSequence)
//-----------------------------------------------------------------------------
{
	kDebug(1, "inserting/updating row {} in table {}", iSequence, table.sTableName);
	
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
		KOut.FormatLine("{};", row.FormInsert(m_SQL.GetDBType(), /*bIdentityInsert=*/false, /*bIgnore=*/false));
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
			// Handle the error directly here instead of throwing
			KString sErrorMessage = m_SQL.GetLastError();
			
			// Check if this is MySQL error 1366 (Incorrect string value)
			// This is often due to character encoding issues and should be recoverable
			if (m_SQL.GetLastErrorNum() == 1366)
			{
				// Log error but indicate we can continue
				KErr.FormatLine(">> insert into {}, row {}: $oid={}, failed (continuing): {}\n{}", 
					table.sTableName, iSequence, sPrimaryKey, sErrorMessage, m_SQL.GetLastSQL());
				return true; // Continue processing
			}
			else
			{
				// Log error and indicate we should stop
				KErr.FormatLine(">> insert into {}, row {}: $oid={}, failed: {}\n{}", 
					table.sTableName, iSequence, sPrimaryKey, sErrorMessage, m_SQL.GetLastSQL());
				return false; // Stop processing
			}
		}
	}
	
	return true; // Success - continue processing

} // InsertOrUpdateOneRow

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
