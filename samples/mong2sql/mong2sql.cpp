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
#include <dekaf2/kparallel.h>
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
	
	// Initialize new KJSON collections array
	m_Collections = KJSON::array();

	KOptions Options(true, KLog::STDOUT, /*bThrow*/true);
	Options.SetBriefDescription("convert a MongoDB collection to one or more tables in a MySQL db");
	Options.SetAdditionalHelp(
		"notes:\n"
		"  * one of -in or -mongo needs to be specified\n"
		"  * if no connection parms are specified for MySQL, then SQL is just\n"
		"    generated to stdout\n"
		"  * -continue mode requires both -mongo and -dbc connections\n"
		"  * -compare mode requires both -mongo and -dbc connections\n"
		"  * verbosity is automatically enabled when using -dbc\n"
		"\n"
		"special actions:\n"
		"  all       :: processes all collections in the MongoDB database (requires -mongo)\n"
		"  list      :: lists all collections with sizes, sorted smallest to largest (requires -mongo)\n"
	);
	Options.SetAdditionalArgDescription("all|<collection> [...]");

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
			m_Config.sMode = "CONTINUE";
			m_Config.bCreateTables = false;  // Don't overwrite existing tables in continue mode
		});

	Options.Option("t,threads <N>")
		.Help("adjust the number of threads (default is 12)")
		.Set(m_Config.iThreads)
		([&]()
		{
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

	Options.Option("f,force")
		.Help("ignore mongo collection cache in /var/tmp/{dbname}")
		([&]()
		{
			m_Config.bForce = true;
		});

	Options.Option("nodata")
		.Help("generate DDL only, skip data inserts")
		([&]()
		{
			m_Config.bNoData = true;
			m_Config.iThreads = 1;
		});

	Options.Option("first")
		.Help("first sync mode: skip collections if target MySQL table already exists")
		([&]()
		{
			m_Config.bFirstSynch = true;
		});

	Options.Option("compare")
		.Help("compare MongoDB collections with MySQL tables and show sync status")
		([&]()
		{
			m_Config.sMode = "COMPARE";
		});

	Options.Option("dump")
		.Help("dump the m_Collections KJSON structure and exit (for debugging)")
		([&]()
		{
			m_Config.sMode = "DUMP";
			m_Config.bNoData = true;  // Disable inserts when dumping
			m_Config.iThreads = 1;
		});

	Options.Option("list")
		.Help("list the given collections with sizes (fast, no queries)")
		([&]()
		{
			m_Config.sMode = "LIST";
			m_Config.bNoData = true;  // Disable inserts
			m_Config.iThreads = 1;
		});

	Options.UnknownCommand([&](KOptions::ArgList& Args)
	{
		kDebug (3, "unknown arg lambda");
		while (!Args.empty())
		{
			KString sCollection = Args.pop();
			if (m_Config.sCollectionNames)
			{
				m_Config.sCollectionNames += ",";
			}
			m_Config.sCollectionNames += sCollection;
			kDebug (3, "collection added: {}", sCollection);
		}
	});

	Options.Parse(argc, argv);

	if (m_Config.sCollectionNames.empty())
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
	if (m_Config.sMode.Hash() == "CONTINUE"_hash)
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

	if (!m_Config.sDBC && (m_Config.sMode.ToUpper().Hash() == "COMPARE"_hash))
	{
		SetError ("compare requires that -dbc is specified on the cli");
		return 1;
	}

	if (!LoadDBC())
	{
		return 1;
	}

	if (kStrIn ("all", m_Config.sCollectionNames))
	{
		m_Config.sCollectionNames.clear(); // no restriction on collection names
	}

	if (m_Config.sInputFile)
	{
		if (!GetCollectionFromFile())
		{
			return 1;
		}
	}
	else
	{
		if (!GetCollectionsFromMongoDB())
		{
			return 1;
		}
	}

	// all the actions except LIST require scanning the collections:
	if (m_Config.sMode.ToUpper().Hash() != "LIST"_hash)
	{
		kParallelForEach (m_Collections, [&](KJSON& cc)
		{
			// replace this array element (cc) with the expanded collection:
			{
				std::unique_lock Lock(m_Mutex);
				cc = DigestCollection (cc);
			}
		}, m_Config.iThreads);
	}
	
	// now perform the action:
	switch (m_Config.sMode.ToUpper().Hash())
	{
	case "DUMP"_hash:
		KOut.WriteLine (m_Collections.dump(1,'\t'));
		break;
	case "LIST"_hash:
		return ListCollections();
		break;
	case "COMPARE"_hash:
		return CompareCollectionsToMySQL();
		break;
	case "COPY"_hash:
		return CopyCollections();
		break;
	default:
		KErr.FormatLine (">> unknown mode: {}", m_Config.sMode);
		return 1;
	}

	return 0;

} // Mong2SQL::Main

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
bool Mong2SQL::GetCollectionsFromMongoDB ()
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");
	
	// Initialize MongoDB driver and establish connection
	InitializeMongoDB();
	if (!ConnectToMongoDB())
	{
		return false;
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
		
		m_Collections = KJSON::array();

		auto WantNames = m_Config.sCollectionNames.Split(",");

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
				if (sCollectionName.starts_with("system."))
				{
					kDebug (1, "skipping system collection: {}", sCollectionName);
					continue;
				}

				kDebug(2, "found collection: {}", sCollectionName);
					
				if (m_Config.sCollectionNames)
				{
					kDebug(2, "making sure this collection is part of the cli spec: {}", m_Config.sCollectionNames);

					auto FoundIt = std::find (WantNames.begin(), WantNames.end(), sCollectionName);
					if (FoundIt == WantNames.end())
					{
						kDebug (1, "collection '{}' does not match: {}", sCollectionName, m_Config.sCollectionNames);
						continue;
					}
					kDebug (1, "collection '{}' matches cli spec: {}", sCollectionName, m_Config.sCollectionNames);
					WantNames.erase (FoundIt);
				}

				// Get collection size using totalSize()
				uint64_t iSizeBytes = 0;
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
							iSizeBytes = sizeElement.get_int32().value;
						}
						else
						{
							iSizeBytes = sizeElement.get_int64().value;
						}
					}
						
					kDebug(2, "collection '{}' size: {} bytes", sCollectionName, iSizeBytes);
				}
				catch (const std::exception& ex)
				{
					kDebug(1, "warning: could not get size for collection '{}': {}", sCollectionName, ex.what());
					iSizeBytes = 0; // Default to 0 if size query fails
				}
					
				m_Collections += KJSON {
					{ "collection_name", sCollectionName },
					{ "size_bytes",      iSizeBytes      },
					{ "num_documents",   -1              }
				};
			}

		} // for each collection found in the mongo db
		
		// Sort collections by size_bytes ascending (smallest first)
		SortCollectionsBySize();

		if (WantNames.size() == 1)
		{
			SetError (kFormat("did not find mongo collection: {}", kJoined (WantNames)));
			return false;
		}
		else if (WantNames.size() > 1)
		{
			SetError (kFormat("did not find mongo collections: {}", kJoined (WantNames)));
			return false;
		}

		Verbose(1, "MongoDB: found {} collections, sorted by size (smallest to largest)", m_Collections.size());
	}
	catch (const std::exception& ex)
	{
		SetError(kFormat("MongoDB error while listing collections: {}", ex.what()));
		return false;
	}
	
	return true;

} // GetCollectionsFromMongoDB

//-----------------------------------------------------------------------------
void Mong2SQL::SortCollectionsBySize()
//-----------------------------------------------------------------------------
{
	kDebug(1, "sorting {} collections by size_bytes", m_Collections.size());
	
	#if 0

	std::sort (m_Collections.begin(), m_Collections.end(), [](const KJSON& left, const KJSON& right) { return left("size_bytes").UInt64() > right("size_bytes").UInt64(); } );

	#else

	if (m_Collections.size() <= 1)
	{
		return; // Nothing to sort
	}
	
	// Create a vector of indices
	std::vector<std::size_t> indices;
	indices.reserve(m_Collections.size());
	for (std::size_t ii = 0; ii < m_Collections.size(); ++ii)
	{
		indices.push_back(ii);
	}
	
	// Sort indices based on size_bytes
	std::sort(indices.begin(), indices.end(), [this](std::size_t a, std::size_t b)
	{
		return m_Collections[a]["size_bytes"].UInt64() < m_Collections[b]["size_bytes"].UInt64();
	});
	
	// Build new sorted array
	KJSON sortedCollections = KJSON::array();
	for (std::size_t idx : indices)
	{
		sortedCollections += m_Collections[idx];
	}
	
	// Replace with sorted version
	m_Collections = std::move(sortedCollections);
	
	#endif

	kDebug(1, "collections sorted");

} // SortCollectionsBySize

//-----------------------------------------------------------------------------
bool Mong2SQL::GetCollectionFromFile()
//-----------------------------------------------------------------------------
{
	kDebug(1, "...");

	if (m_Config.sCollectionNames.Contains(","))
	{
		KErr.FormatLine (">> when reading from an infile, you can only specify one collection, not: {}", m_Config.sCollectionNames);
		return false;
	}

	KInFile InFile(m_Config.sInputFile);
	if (!InFile.is_open())
	{
		KErr.FormatLine (">> failed to read collection from: {}", m_Config.sInputFile);
		return false;
	}

	m_Collections    = KJSON::array();
	auto oCollection = KJSON::array();

	KString  sLine;
	uint32_t iLines { 0 };
	uint64_t iBytes { 0 };

	while (InFile.ReadLine(sLine))
	{
		sLine.Trim();
		++iLines;
		iBytes += sLine.size();
		if (sLine.empty() || sLine.starts_with('#'))
		{
			continue; // Skip empty lines and comments
		}

		if (iLines % 1000 == 0)
		{
			kDebug(2, "processed {} lines", iLines);
		}

		KJSON   document;
		KString sError;
		if (!kjson::Parse(document, sLine, sError))
		{
			KErr.FormatLine ("{}:{}: failed to parse line: {}\n>> {}", m_Config.sInputFile, iLines, sLine, sError);
			continue;
		}

		oCollection += document;
	}

	m_Collections += KJSON {
		{ "collection_name", m_Config.sCollectionNames/*sic*/ },
		{ "size_bytes",      iBytes                           },
		{ "num_documents",   iLines                           }
	};

	return true;

} // GetCollectionFromFile()

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
int Mong2SQL::ListCollections ()
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	KOut.FormatLine ("+-{:<50.50}+-{:>10.10}-+-{:>10.10}-+", KLog::BAR, KLog::BAR, KLog::BAR);
	KOut.FormatLine ("| {:<50.50}| {:>10.10} | {:>10.10} |", "MONGO-COLLECTION", "MONGO-SIZE", "DOCUMENTS");
	KOut.FormatLine ("+-{:<50.50}+-{:>10.10}-+-{:>10.10}-+", KLog::BAR, KLog::BAR, KLog::BAR);

	for (const auto& cc : m_Collections)
	{
		auto iCount = cc["num_collection"].Int64();
		KOut.FormatLine ("| {:<50.50}| {:>10.10} | {:>10.10} |",
			cc["collection_name"],
			kFormBytes(cc["size_bytes"].UInt64()),
			(iCount >= 0) ? kFormNumber(iCount) : "");
	}

	KOut.FormatLine ("+-{:<50.50}+-{:>10.10}-+-{:>10.10}-+", KLog::BAR, KLog::BAR, KLog::BAR);

	return 0;

} // ListCollections

//-----------------------------------------------------------------------------
KJSON Mong2SQL::DigestCollection (const KJSON& oCollection) const
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	mongocxx::uri uri{m_Config.sMongoConnectionString.c_str()};
	KString       sDatabaseName   = uri.database();
	KString       sCollectionName = oCollection["collection_name"];
	KString       sCacheFile      = kFormat ("/var/tmp/{}/{}.json", sDatabaseName, sCollectionName);
	KString       sContents;
	
	// cached in /var/tmp
	if (!m_Config.bForce && kFileExists (sCacheFile) && kReadFile (sCacheFile, sContents) && sContents)
	{
		KString sError;
		KJSON   oLoaded;
		if (kjson::Parse (oLoaded, sContents, sError))
		{
			kDebug (1, "{}: collection loaded from cache: {}", sCollectionName, sCacheFile);
			return oLoaded;
		}
	}

	KJSON oBuilt;
	oBuilt["collection_name"] = sCollectionName;
	oBuilt["size_bytes"]      = oCollection["size_bytes"].UInt64();
	oBuilt["tables"]          = KJSON::array();

	// Create local MongoDB connection (thread-safe, cannot share connections across threads)
	try
	{
		mongocxx::uri localUri{m_Config.sMongoConnectionString.c_str()};
		mongocxx::client localClient{localUri};
		KString sDbName = localUri.database();
		if (sDbName.empty())
		{
			sDbName = "test";
		}
		
		auto database   = localClient[sDbName.c_str()];
		auto collection = database[sCollectionName.c_str()];
		
		kDebug (1, "{}: querying collection for schema analysis...", sCollectionName);
		
		// Fetch all documents from MongoDB collection
		auto cursor = collection.find({});
		std::vector<KJSON> documents;
		uint64_t iDocCount = 0;
		
		for (const auto& doc : cursor)
		{
			KString sJsonDoc = bsoncxx::to_json(doc);
			sJsonDoc = SanitizeMongoJSON(sJsonDoc);
			
			KJSON   document;
			KString sError;
			if (!kjson::Parse(document, sJsonDoc, sError))
			{
				kDebug (1, "{}: failed to parse document: {}", sCollectionName, sError);
				continue;
			}
			
			documents.emplace_back(std::move(document));
			++iDocCount;
			
			if (iDocCount % 1000 == 0)
			{
				kDebug (2, "{}: loaded {} documents", sCollectionName, iDocCount);
			}
		}
		
		kDebug (1, "{}: loaded {} documents, analyzing schema...", sCollectionName, iDocCount);
		oBuilt["num_documents"] = iDocCount;
		
		// Local schema tracking structures (thread-safe, no member variables)
		KJSON localTables = KJSON::object();      // tables keyed by name
		KJSON localTableOrder = KJSON::array();   // ordered list of table names
		
		// Deduce MySQL tables from documents
		if (!documents.empty())
		{
			KString sTableName = ConvertCollectionNameToTableName(sCollectionName);
			
			// Initialize root table with collection path
			KJSON& rootTable = localTables[sTableName];
			rootTable["table_name"] = sTableName;
			rootTable["primary_key"] = "";
			rootTable["parent_table"] = "";
			rootTable["parent_key_column"] = "";
			rootTable["has_object_id"] = false;
			rootTable["collection_path"] = sCollectionName;  // Root path is just the collection name
			rootTable["num_documents"] = 0;  // Will be set after processing documents
			rootTable["columns"] = KJSON::object();
			rootTable["column_order"] = KJSON::array();
			localTableOrder += sTableName;
			
			// Set primary key for root table
			KString sPrimaryKey = GenerateTablePKEY(sTableName);
			rootTable["primary_key"] = sPrimaryKey;
			
			// Collect schema from all documents
			for (const auto& document : documents)
			{
				CollectSchemaForDocumentLocal(document, sTableName, KString{}, localTables, localTableOrder);
			}
			
			// Set document count for root table
			localTables[sTableName]["num_documents"] = iDocCount;
			
			kDebug (1, "{}: discovered {} table(s)", sCollectionName, localTables.size());
			
			// Compute max column name width for all tables
			std::size_t iMaxColumnWidth = 0;
			for (auto itTable = localTables.begin(); itTable != localTables.end(); ++itTable)
			{
				const auto& tableData = *itTable;
				const auto& columns = tableData["columns"];
				for (auto itCol = columns.begin(); itCol != columns.end(); ++itCol)
				{
					iMaxColumnWidth = std::max(iMaxColumnWidth, itCol.key().size());
				}
			}
			oBuilt["max_column_width"] = iMaxColumnWidth;
			
			// Generate DDL for all tables and store metadata
			for (const auto& tableName : localTableOrder)
			{
				const auto& tableData = localTables[tableName.String()];
				
				KJSON oTable;
				oTable["tablename"] = tableData["table_name"];
				oTable["collection_path"] = tableData["collection_path"];
				oTable["num_documents"] = tableData["num_documents"];
				oTable["num_columns"] = tableData["columns"].size();
				oTable["has_object_id"] = tableData["has_object_id"];
				oTable["primary_key"] = tableData["primary_key"];
				oTable["parent_table"] = tableData["parent_table"];
				oTable["parent_key_column"] = tableData["parent_key_column"];
				
				// Store column metadata
				KJSON oColumns = KJSON::array();
				const auto& columnOrder = tableData["column_order"];
				for (const auto& columnName : columnOrder)
				{
					KString sColName = columnName.String();
					const auto& columnInfo = tableData["columns"][sColName];
					KJSON oColumn;
					oColumn["name"] = sColName;
					oColumn["type"] = columnInfo["sql_type"];
					oColumn["nullable"] = columnInfo["nullable"];
					oColumn["max_length"] = columnInfo["max_length"];
					oColumn["has_non_null"] = columnInfo["has_non_null"].Bool();
					oColumn["has_non_zero"] = columnInfo["has_non_zero"].Bool();
					oColumns += oColumn;
				}
				oTable["columns"] = oColumns;
				
				oBuilt["tables"] += oTable;
			}
		}
		else
		{
			kDebug (1, "{}: no documents found in collection", sCollectionName);
		}
	}
	catch (const std::exception& ex)
	{
		kDebug (1, "{}: MongoDB error during digestion: {}", sCollectionName, ex.what());
		oBuilt["error"] = ex.what();
	}

	// Store in cache
	kMakeDir (KString{kDirname (sCacheFile)});
	kRemoveFile (sCacheFile);
	if (!kWriteFile (sCacheFile, oBuilt.dump(1,'\t')))
	{
		kDebug (1, "{}: could not write cache file: {}", sCollectionName, sCacheFile);
	}
	else
	{
		kDebug (1, "{}: wrote cache file: {}", sCollectionName, sCacheFile);
	}

	kDebug (1, "{}: collection built and stored to cache: {}", sCollectionName, sCacheFile);

	return oBuilt;

} // DigestCollection

//-----------------------------------------------------------------------------
int Mong2SQL::CompareCollectionsToMySQL ()
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	if (!m_Config.sDBC)
	{
		SetError ("compare requires that -dbc is specified on the cli");
		return 1;
	}

	KOut.FormatLine ("+-{:<50.50}+-{:>10.10}-+-{:>10.10}-+-{:<50.50}+-{:>10.10}-+", KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);
	KOut.FormatLine ("| {:<50.50}| {:>10.10} | {:>10.10} | {:<50.50}| {:>10.10} |", "MONGO-COLLECTION", "MONGO-SIZE", "DOCUMENTS", "SQL-TABLE", "#ROWS");
	KOut.FormatLine ("+-{:<50.50}+-{:>10.10}-+-{:>10.10}-+-{:<50.50}+-{:>10.10}-+", KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);

	kParallelForEach (m_Collections, [&](KJSON& cc)
	{
		KString     sSection;
		auto        iCount = cc["num_documents"].Int64();
		const auto& Tables = cc["tables"];
		uint16_t    iTab{0};
		KSQL        db;

		if (!db.LoadConnect(m_Config.sDBC) || !db.OpenConnection())
		{
			SetError (db.GetLastError());
			return;
		}

		for (const auto& tt : Tables)
		{
			++iTab;
			KString sTablename = tt["tablename"];
			        iCount     = tt["num_documents"].UInt64();
			auto    iRows      = db.SingleIntQuery ("select count(*) from {}", sTablename);
			KString sRows      = (iRows >= 0) ? kFormNumber(iRows) : "";
			KString sComment;

			if (iRows < 0)
			{
				sComment = "<-- not created yet";
			}
			else if (iRows != iCount)
			{
				sComment = "<-- needs to be [re]synched";
			}

			sSection += kFormat ("| {:<50.50}| {:>10.10} | {:>10.10} | {:<50.50}| {:>10.10} | {}\n",
				(iTab == 1)   ? cc["collection_name"]                 : tt["collection_path"],
				(iTab == 1)   ? kFormBytes(cc["size_bytes"].UInt64()) : "",
				(iCount >= 0) ? kFormNumber(iCount)                   : "",
				sTablename,
				sRows,
				sComment);
		}

		if (!iTab)
		{
			sSection += kFormat ("| {:<50.50}| {:>10.10} | {:>10.10} | {:<50.50}| {:>10.10} | {}\n",
				cc["collection_name"],
				kFormBytes(cc["size_bytes"].UInt64()),
				kFormNumber(iCount),
				"", // tablename
				"", // rows
				"<-- empty collection");
		}

		sSection += kFormat ("| {:<50.50}| {:>10.10} | {:>10.10} | {:<50.50}| {:>10.10} |\n", "", "", "", "", "");

		{
			std::unique_lock Lock(m_Mutex);
			KOut.Write (sSection); // <-- flush entire selection at once so that collections are not mingled
		}

	}, m_Config.iThreads);

	KOut.FormatLine ("+-{:<50.50}+-{:>10.10}-+-{:>10.10}-+-{:<50.50}+-{:>10.10}-+", KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR, KLog::BAR);

	return 0;

} // CompareCollectionsToMySQL

//-----------------------------------------------------------------------------
int Mong2SQL::CopyCollections ()
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	kParallelForEach (m_Collections, [&](const KJSON& oCollection)
	{
		CopyCollection (oCollection);

	}, m_Config.iThreads);

	return 0;

} // CopyCollections

//-----------------------------------------------------------------------------
void Mong2SQL::ShowBar (KBAR& bar, KStringView sCollectionName, uint64_t iInsertCount/*=0*/, uint64_t iUpdateCount/*=0*/)
//-----------------------------------------------------------------------------
{
	bool bShowBar  = (!m_Config.sDBC.empty()); // show progress bar if SQL is not going to stdout
	if (!bShowBar)
	{
		return;
	}

	std::unique_lock Lock(m_Mutex);
	auto sLine = kFormat (":: {:%a %T} [{:30.30}] collection {:<50} {:>12} out of {:12}",
		kNow(), bar.GetBar(30,'_'), sCollectionName, kFormNumber(bar.GetSoFar()), kFormNumber(bar.GetExpected()));

	if (iInsertCount)
	{
		sLine += kFormat (" {} inserts", kFormNumber(iInsertCount));
	}

	if (iUpdateCount)
	{
		sLine += kFormat (" {} updates", kFormNumber(iUpdateCount));
	}

	KOut.FormatLine ("{}", sLine);

} // ShowBar

//-----------------------------------------------------------------------------
void Mong2SQL::CopyCollection (const KJSON& oCollection)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	// Note: this method assumes that m_Collections has been filled out by calling DigestCollections()

	KString sCollectionName = oCollection.Select("collection_name");
	auto    iExpected       = oCollection.Select("num_documents").UInt64();
	auto    iTenPercent     = (iExpected / 10);
	size_t  iInsertCount {0};
	KBAR    bar (iExpected, /*width=*/30, KBAR::STATIC);

	kDebug (1, "copying collection: {}", sCollectionName);
	ShowBar (bar, sCollectionName);

	// Ensure collection is digested (has table metadata)
	if (!oCollection.contains("tables") || oCollection["tables"].empty())
	{
		kDebug (1, "no tables defined for this collection: {}", sCollectionName);
		return;
	}
	
	const auto& tables = oCollection["tables"];
	
	// Thread-safe: Create local database connection
	KSQL db;
	bool bHasDB = false;

	if (!m_Config.sDBC.empty())
	{
		if (!db.LoadConnect(m_Config.sDBC) || !db.OpenConnection())
		{
			KErr.FormatLine(">> {}: failed to connect to MySQL: {}", sCollectionName, db.GetLastError());
			return;
		}
		bHasDB = true;
		kDebug(2, "{}: connected to MySQL", sCollectionName);
	}
	
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Phase 1: Generate DDL (CREATE TABLE statements) if requested
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_Config.bCreateTables)
	{
		for (const auto& table : tables)
		{
			KString sTableName = table["tablename"].String();
			
			// Check if table exists (for -first mode)
			if (m_Config.bFirstSynch && bHasDB)
			{
				auto iExists = db.SingleIntQuery("select count(*) from INFORMATION_SCHEMA.TABLES where table_schema = database() and table_name = '{}'", sTableName);
				if (iExists > 0)
				{
					kDebug(1, "{}: table {} already exists, skipping DDL (-first mode)", sCollectionName, sTableName);
					continue;
				}
			}
			
			// Generate DROP TABLE statement
			KString sDropSQL = kFormat("drop table if exists {};", sTableName);
			
			// Generate CREATE TABLE statement
			KString sCreateSQL = GenerateCreateTableDDL(table);
			
			// Execute or output DDL
			if (bHasDB)
			{
				if (!m_Config.bFirstSynch) // Don't drop in first synch mode
				{
					if (!db.ExecSQL("{}", sDropSQL))
					{
						KErr.FormatLine(">> {}: failed to drop table {}: {}", sCollectionName, sTableName, db.GetLastError());
					}
				}
				
				if (!db.ExecSQL("{}", sCreateSQL))
				{
					KErr.FormatLine(">> {}: failed to create table {}: {}", sCollectionName, sTableName, db.GetLastError());
					return;
				}
				kDebug(1, "{}: created table {}", sCollectionName, sTableName);
			}
			else
			{
				if (!m_Config.bFirstSynch)
				{
					KOut.WriteLine(sDropSQL);
				}
				KOut.WriteLine(sCreateSQL);
				KOut.WriteLine();
			}
		}
	}
	
	if (m_Config.bNoData)
	{
		kDebug(1, "{}: skipping data inserts (-nodata)", sCollectionName);
		return;
	}
	
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// Phase 2: Insert data (if not disabled)
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	// Create local MongoDB connection (thread-safe)
	try
	{
		mongocxx::uri mongoUri{m_Config.sMongoConnectionString.c_str()};
		mongocxx::client mongoClient{mongoUri};
		KString sDbName = mongoUri.database();
		if (sDbName.empty())
		{
			sDbName = "test";
		}
		
		auto database = mongoClient[sDbName.c_str()];
		auto collection = database[sCollectionName.c_str()];
		
		kDebug(1, "{}: querying MongoDB for documents...", sCollectionName);
		
		// Handle continue mode (partial collection processing)
		if (m_Config.bContinueMode)
		{
			// TODO: Implement partial collection processing
			// This will require:
			// 1. Query MongoDB with a filter based on m_Config.sContinueField
			// 2. Only process documents where field > last_processed_value
			KErr.FormatLine(">> {}: continue mode not yet implemented", sCollectionName);
			return;
		}

		// Fetch all documents from MongoDB
		auto cursor = collection.find({});

		std::size_t iDocCount {0};
		
		for (const auto& doc : cursor)
		{
			++iDocCount;
			
			// Convert BSON to JSON
			KString sJsonDoc  = bsoncxx::to_json(doc);
			sJsonDoc = SanitizeMongoJSON(sJsonDoc);
			
			KJSON document;
			KString sError;
			if (!kjson::Parse(document, sJsonDoc, sError))
			{
				KErr.FormatLine (">> {}:{} failed to parse document: {}", sCollectionName, iDocCount, sError);
				continue;
			}
			
			// Extract primary key from document
			KString sPrimaryKey = ExtractPrimaryKeyFromDocument(document);
			
			// Find root table schema
			const KJSON* pRootTable = nullptr;
			for (const auto& table : tables)
			{
				if (table["parent_table"].String().empty())
				{
					pRootTable = &table;
					break;
				}
			}
			
			if (!pRootTable)
			{
				KErr.FormatLine (">> {}:{}: no root table found, skipping inserts", sCollectionName, iDocCount);
				break;
			}
			
			// Flatten document into row values
			std::map<KString, KString> rowValues;
			KString sTablename = pRootTable->Select("tablename");
			KString sTablePK   = GenerateTablePKEY (sTablename);
			if (!sTablePK.empty() && !sPrimaryKey.empty())
			{
				rowValues[sTablePK] = sPrimaryKey; // Add primary key using actual column name from schema
			}
			else
			{
				KErr.FormatLine (">> {}:{}: no pkey value, sTablePK={}, sPrimaryKey={}", sCollectionName, iDocCount, sTablePK, sPrimaryKey);
				break;
			}
			FlattenDocumentToRow(document, KString{}, rowValues);
			
			// Insert the root document row
			if (InsertDocumentRow(db, *pRootTable, rowValues, sPrimaryKey))
			{
				++iInsertCount;
			}
			
			// Process nested arrays (child tables)
			ProcessDocumentArrays(db, document, tables, sTablename, KString{}, sPrimaryKey);
			
			bar.Move ();

			if (iTenPercent && !(iDocCount % iTenPercent))
			{
				ShowBar (bar, sCollectionName, iInsertCount);
			}
		}
		
		kDebug(1, "{}: inserted {} of {} documents", sCollectionName, iInsertCount, iDocCount);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: MongoDB error during insert: {}", sCollectionName, ex.what());
	}
	
	ShowBar (bar, sCollectionName, iInsertCount);

} // CopyCollection

//-----------------------------------------------------------------------------
KString Mong2SQL::GenerateCreateTableDDL (const KJSON& table) const
//-----------------------------------------------------------------------------
{
	KString sTableName = table["tablename"].String();
	const auto& columns = table["columns"];
	
	if (columns.empty())
	{
		kDebug(1, "{}: no columns found", sTableName);
		return KString{};
	}
	
	// Determine primary key from schema metadata
	KString sPrimaryKey = table["primary_key"].String();
	bool bHasObjectId = table["has_object_id"].Bool();
	
	// Build CREATE TABLE statement
	KString sSQL = kFormat(
		"# ----------------------------------------------------------------------\n"
		"create table {}\n"
		"# ----------------------------------------------------------------------\n"
		"(\n",
			sTableName);
	
	// Filter out all-null and all-zero columns and calculate max widths for alignment
	std::vector<KJSON> activeColumns;
	std::size_t iMaxNameWidth = 0;
	std::size_t iMaxTypeWidth = 0;
	for (const auto& col : columns)
	{
		bool bHasNonNull = col["has_non_null"].Bool();
		bool bHasNonZero = col["has_non_zero"].Bool();
		KString sType = col["type"].String();
		
		// Skip columns that have only null values across the entire collection
		if (!bHasNonNull)
		{
			kDebug(2, "{}: skipping all-null column: {}", sTableName, col["name"].String());
			continue;
		}
		
		// Skip numeric columns that have only zero values (treat like null)
		if ((sType.starts_with("int") || sType.starts_with("bigint") || 
		     sType.starts_with("float") || sType.starts_with("double")) && !bHasNonZero)
		{
			kDebug(2, "{}: skipping all-zero column: {}", sTableName, col["name"].String());
			continue;
		}
		
		activeColumns.push_back(col);
		iMaxNameWidth = std::max(iMaxNameWidth, col["name"].String().size());
		iMaxTypeWidth = std::max(iMaxTypeWidth, col["type"].String().size());
	}
	
	// If no columns remain after filtering, return empty
	if (activeColumns.empty())
	{
		kDebug(1, "{}: no non-null columns found, skipping table creation", sTableName);
		return KString{};
	}
	
	// Generate column definitions with aligned columns
	for (std::size_t ii = 0; ii < activeColumns.size(); ++ii)
	{
		const auto& col = activeColumns[ii];
		KString sColName = col["name"].String();
		KString sType = col["type"].String();
		bool bNullable = col["nullable"].Bool();
		bool bIsPK = (bHasObjectId && sColName == sPrimaryKey);
		
		// Build modifiers string
		KString sModifiers = (bNullable && !bIsPK) ? "null" : "not null";
		if (bIsPK)
		{
			sModifiers += " primary key";
		}
		
		// Format with proper alignment: column_name  data_type  modifiers
		sSQL += kFormat("    {:<{}}  {:<{}}  {}", 
			sColName, iMaxNameWidth,
			sType, iMaxTypeWidth,
			sModifiers);
		
		if ((ii + 1) < activeColumns.size())
		{
			sSQL += ",";
		}
		sSQL += "\n";
	}
	
	sSQL += ") engine=InnoDB default charset=utf8mb4;\n";
	
	return sSQL;
	
} // GenerateCreateTableDDL

//-----------------------------------------------------------------------------
KString Mong2SQL::ExtractPrimaryKeyFromDocument (const KJSON& document) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "extracting primary key from document");
	
	// Look for MongoDB _id field
	if (document.contains("_id") && document["_id"].is_object())
	{
		const auto& idObj = document["_id"];
		if (idObj.contains("$oid") && idObj["$oid"].is_string())
		{
			return idObj["$oid"].String();
		}
	}
	
	// Fallback: if no _id.$oid found, generate a hash
	KString sInput = document.dump();
	auto iHashValue = kHash(sInput.c_str());
	return kFormat("{:016x}", static_cast<unsigned long long>(iHashValue));
	
} // ExtractPrimaryKeyFromDocument

//-----------------------------------------------------------------------------
void Mong2SQL::FlattenDocumentToRow (const KJSON& document, const KString& sPrefix, std::map<KString, KString>& rowValues) const
//-----------------------------------------------------------------------------
{
	kDebug(2, "{}", document.dump(1,'\t'));
	
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
			// Recursively flatten nested objects
			FlattenDocumentToRow(value, sColumnName, rowValues);
		}
		else if (value.is_array())
		{
			// Skip arrays - handled separately in ProcessDocumentArrays
			continue;
		}
		else
		{
			// Convert value to SQL literal
			rowValues[sColumnName] = ToSqlLiteral(value);
		}
	}
	
} // FlattenDocumentToRow

//-----------------------------------------------------------------------------
KString Mong2SQL::ToSqlLiteral (const KJSON& value) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "converting value to SQL literal");
	
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
	else if (value.is_number_float())
	{
		return kFormat("{}", value.String().Double());
	}
	else if (value.is_string())
	{
		return value.String();
	}
	
	// Fallback: dump as JSON string
	return value.dump();
	
} // ToSqlLiteral

//-----------------------------------------------------------------------------
bool Mong2SQL::InsertDocumentRow (KSQL& db, const KJSON& tableSchema, const std::map<KString, KString>& rowValues, KStringView sPrimaryKey) const
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	KString sTableName = tableSchema["tablename"].String();
	const auto& columns = tableSchema["columns"];
	
	kDebug(3, "inserting row into table: {}", sTableName);
	
	// Create KROW and populate with column values
	KROW row(sTableName);
	
	for (const auto& col : columns)
	{
		KString sColName = col["name"].String();
		KString sColType = col["type"].String();
		bool bNullable = col["nullable"].Bool();
		bool bHasNonNull = col["has_non_null"].Bool();
		bool bHasNonZero = col["has_non_zero"].Bool();
		
		// Skip columns that have only null values across the entire collection
		if (!bHasNonNull)
		{
			continue;
		}
		
		// Skip numeric columns that have only zero values (treat like null)
		if ((sColType.starts_with("int") || sColType.starts_with("bigint") || 
		     sColType.starts_with("float") || sColType.starts_with("double")) && !bHasNonZero)
		{
			continue;
		}
		
		auto it = rowValues.find(sColName);
		
		KCOL::Flags flags = KCOL::NOFLAG;
		
		// Set type-specific flags based on SQL type
		if (sColType.starts_with("int") || sColType.starts_with("bigint"))
		{
			flags |= KCOL::NUMERIC;
		}
		else if (sColType.starts_with("tinyint(1)"))
		{
			flags |= KCOL::BOOLEAN;
		}
		else if (sColType.starts_with("float") || sColType.starts_with("double"))
		{
			flags |= KCOL::NUMERIC;
		}
		
		// Check if this is the primary key column
		// Use the actual primary key from the schema, not just any _oid column
		KString sTablePK = tableSchema["primary_key"].String();
		if (!sTablePK.empty() && sColName == sTablePK)
		{
			flags |= KCOL::PKEY;
		}
		
		if (it != rowValues.end())
		{
			// Add column with value
			std::size_t iMaxLen = col["max_length"].UInt64();
			row.AddCol(sColName, it->second, flags, iMaxLen);
		}
		else if (bNullable)
		{
			// Add NULL value for missing nullable columns
			row.AddCol(sColName, KString{}, flags, 0);
		}
	}
	
	// Generate INSERT statement
	if (m_Config.sDBC.empty())
	{
		// Output to stdout
		// Use bIdentityInsert=true because MongoDB _oid columns have explicit values (not auto-increment)
		KOut.FormatLine("{};", row.FormInsert(KSQL::DBT::MYSQL, /*bIdentityInsert=*/true, /*bIgnore=*/false));
		return true;
	}
	else
	{
		// Execute on database
		auto bOK = db.Insert(row);
		
		if (!bOK && db.WasDuplicateError())
		{
			// Try update if duplicate key
			bOK = db.Update(row);
		}
		
		if (!bOK)
		{
			// Log error
			KErr.FormatLine(">> insert into {}, $oid={}: {}", sTableName, sPrimaryKey, db.GetLastError());
			return false;
		}
		
		return true;
	}
	
} // InsertDocumentRow

//-----------------------------------------------------------------------------
void Mong2SQL::ProcessDocumentArrays (KSQL& db, const KJSON& document, const KJSON& allTables, KStringView sTableName, const KString& sPrefix, KStringView sParentPK) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "processing arrays in document");
	
	if (!document.is_object())
	{
		return;
	}
	
	for (auto it = document.begin(); it != document.end(); ++it)
	{
		KString sKey = ConvertFieldNameToColumnName(it.key());
		KString sColumnName = BuildColumnName(sPrefix, sKey);
		const auto& value = *it;
		
		if (value.is_array() && !value.empty())
		{
			// Find child table for this array
			KString sChildTableName = BuildChildTableName(sTableName, sColumnName);
			
			// Find the table schema
			const KJSON* pChildTable = nullptr;
			for (const auto& table : allTables)
			{
				if (table["tablename"].String() == sChildTableName)
				{
					pChildTable = &table;
					break;
				}
			}
			
			if (!pChildTable)
			{
				kDebug(2, "no schema found for child table: {}", sChildTableName);
				continue;
			}
			
			// Process each array element
			for (const auto& element : value)
			{
				std::map<KString, KString> childRowValues;
				
				// Add parent foreign key (use the stored parent_key_column from schema)
				KString sParentKeyCol = pChildTable->Select("parent_key_column").String();
				if (!sParentKeyCol.empty())
				{
					childRowValues[sParentKeyCol] = KString{sParentPK};
				}
				
				if (element.is_object())
				{
					// Extract child primary key if exists
					KString sChildPK = ExtractPrimaryKeyFromDocument(element);
					KString sChildTablePK = pChildTable->Select("primary_key").String();
					if (!sChildPK.empty() && !sChildTablePK.empty())
					{
						childRowValues[sChildTablePK] = sChildPK; // Use actual PK column name from schema
					}
					
					// Flatten child document
					FlattenDocumentToRow(element, KString{}, childRowValues);
					
					// Insert child row
					InsertDocumentRow(db, *pChildTable, childRowValues, sChildPK);
					
					// Recursively process nested arrays
					ProcessDocumentArrays(db, element, allTables, sChildTableName, KString{}, sChildPK);
				}
				else if (element.is_array())
				{
					// Nested array - recursive processing
					// This is rare but can happen
					kDebug(2, "nested array found in {}, skipping", sChildTableName);
				}
				else
				{
					// Scalar value in array
					KString sValueCol = ExtractLeafColumnName(sColumnName);
					childRowValues[sValueCol] = ToSqlLiteral(element);
					
					// Insert child row
					InsertDocumentRow(db, *pChildTable, childRowValues, sParentPK);
				}
			}
		}
		else if (value.is_object())
		{
			// Nested object (not array) - process its arrays recursively
			ProcessDocumentArrays(db, value, allTables, sTableName, sColumnName, sParentPK);
		}
	}
	
} // ProcessDocumentArrays

//-----------------------------------------------------------------------------
Mong2SQL::TableSchema& Mong2SQL::EnsureTableSchema (KStringView sTableName, KStringView sParentTable, KStringView sParentKeyColumn)
//-----------------------------------------------------------------------------
{
	kDebug(2, "ensuring schema for table: {}", sTableName);
	KString sKey = sTableName;
	auto it = m_TableSchemas.find(sKey);

	if (it == m_TableSchemas.end())
	{
		TableSchema schema;
		schema.sTableName = sKey;
		schema.sPrimaryKey = KString{};
		schema.sParentTable = sParentTable;
		schema.sParentKeyColumn = sParentKeyColumn;

		auto result = m_TableSchemas.emplace(schema.sTableName, std::move(schema));
		it = result.first;
		m_TableOrder.push_back(it->first);
		m_RowCounters[it->first] = 0;
		kDebug(2, "created new table schema: {}", sKey);

		if (!sParentTable.empty())
		{
			std::size_t iParentLength = 16;
			auto parentIt = m_TableSchemas.find(KString(sParentTable));
			if (parentIt != m_TableSchemas.end())
			{
				auto parentColIt = parentIt->second.Columns.find(KString(sParentKeyColumn));
				if (parentColIt != parentIt->second.Columns.end())
				{
					iParentLength = parentColIt->second.iMaxLength;
				}
			}
			AddColumn(it->second, KString(sParentKeyColumn), SqlType::Text, iParentLength, false);
		}
	}

	return it->second;

} // EnsureTableSchema

//-----------------------------------------------------------------------------
void Mong2SQL::CollectSchemaForDocument (const KJSON& document, TableSchema& table, const KString& sPrefix)
//-----------------------------------------------------------------------------
{
	kDebug(3, "collecting schema for document, prefix: '{}'", sPrefix);
	if (!document.is_object())
	{
		return;
	}
	
	if (sPrefix.empty())
	{
		m_RowCounters[table.sTableName]++;
	}

	for (auto it = document.begin(); it != document.end(); ++it)
	{
		if (it.key() == "_id" && it->is_object() && it->contains("$oid"))
		{
			table.bHasObjectId = true;
			if (table.sPrimaryKey.empty())
			{
				table.sPrimaryKey = GenerateTablePKEY(table.sTableName);
			}
			std::size_t iLength = std::max<std::size_t>(24, 16);
			AddColumn(table, table.sPrimaryKey, SqlType::Text, iLength, false);
			continue;
		}
		
		KString sKey = ConvertFieldNameToColumnName(it.key());
		KString sRawColumnName = BuildColumnName(sPrefix, sKey);
		KString sSanitizedColumnName = SanitizeColumnName(sRawColumnName);
		const auto& value = *it;

		if (value.is_object())
		{
			CollectSchemaForDocument(value, table, sSanitizedColumnName);
		}
		else if (value.is_array())
		{
			CollectArraySchema(value, table, sSanitizedColumnName);
		}
		else
		{
			std::size_t iLength = 0;
			if (value.is_string())
			{
				iLength = value.String().size();
			}
			AddColumn(table, sSanitizedColumnName, InferSqlType(value), iLength);
		}
	}

} // CollectSchemaForDocument

//-----------------------------------------------------------------------------
void Mong2SQL::CollectArraySchema (const KJSON& array, TableSchema& parentTable, const KString& sKey)
//-----------------------------------------------------------------------------
{
	kDebug(3, "collecting array schema for key '{}' with {} elements", sKey, array.size());
	if (!array.is_array() || array.empty())
	{
		return;
	}

	KString sChildTableName = SanitizeColumnName(BuildChildTableName(parentTable.sTableName, sKey));
	auto& childTable = EnsureTableSchema(sChildTableName, parentTable.sTableName, parentTable.sPrimaryKey);
	
	if (childTable.sPrimaryKey.empty())
	{
		childTable.sPrimaryKey = GenerateTablePKEY(childTable.sTableName);
	}

	for (const auto& element : array)
	{
		m_RowCounters[childTable.sTableName]++;
		
		if (element.is_object())
		{
			CollectSchemaForDocument(element, childTable, KString{});
		}
		else if (element.is_array())
		{
			KString sNestedKey = SanitizeColumnName(ExtractLeafColumnName(sKey));
			CollectArraySchema(element, childTable, sNestedKey);
		}
		else
		{
			KString sValueColumn = SanitizeColumnName(ExtractLeafColumnName(sKey));
			std::size_t iLength = 0;
			if (element.is_string())
			{
				iLength = element.String().size();
			}
			AddColumn(childTable, sValueColumn, InferSqlType(element), iLength);
		}
	}

} // CollectArraySchema

//-----------------------------------------------------------------------------
void Mong2SQL::AddColumn (TableSchema& table, const KString& sColumnName, SqlType type, std::size_t iLength, bool isNullable)
//-----------------------------------------------------------------------------
{
	kDebug(3, "adding column '{}' to table '{}'", sColumnName, table.sTableName);
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
Mong2SQL::SqlType Mong2SQL::InferSqlType (const KJSON& value) const
//-----------------------------------------------------------------------------
{
	if (value.is_null())        return SqlType::Text;
	if (value.is_boolean())     return SqlType::Boolean;
	if (value.is_number_integer() || value.is_number_unsigned()) return SqlType::Integer;
	if (value.is_number_float()) return SqlType::Float;
	if (value.is_string())      return SqlType::Text;
	return SqlType::Text;

} // InferSqlType

//-----------------------------------------------------------------------------
Mong2SQL::SqlType Mong2SQL::MergeSqlTypes (SqlType existing, SqlType candidate) const
//-----------------------------------------------------------------------------
{
	if (existing == candidate) return existing;
	if (existing == SqlType::Unknown) return candidate;
	if (candidate == SqlType::Unknown) return existing;
	if (existing == SqlType::Text || candidate == SqlType::Text) return SqlType::Text;
	if (existing == SqlType::Float || candidate == SqlType::Float) return SqlType::Float;
	if (existing == SqlType::Integer || candidate == SqlType::Integer) return SqlType::Integer;
	return SqlType::Text;

} // MergeSqlTypes

//-----------------------------------------------------------------------------
KString Mong2SQL::SqlTypeToString (const ColumnInfo& info) const
//-----------------------------------------------------------------------------
{
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
				if (info.iMaxLength > 60000)
				{
					return "longtext";
				}
				return "text";
			}
			std::size_t iRounded = ((info.iMaxLength + 9) / 10) * 10;
			if (iRounded == 0) iRounded = 10;
			if (iRounded > 255) iRounded = 255;
			return kFormat("varchar({})", iRounded);
		}
	}

} // SqlTypeToString

//-----------------------------------------------------------------------------
void Mong2SQL::GenerateCreateTableSQL (const TableSchema& table)
//-----------------------------------------------------------------------------
{
	kDebug(1, "{}: generating DDL with {} columns", table.sTableName, table.ColumnOrder.size());
	if (table.ColumnOrder.empty())
	{
		return;
	}

	std::vector<KString> columns;
	for (const auto& column : table.ColumnOrder)
	{
		if (!table.bHasObjectId && column == table.sPrimaryKey)
		{
			continue;
		}
		columns.push_back(column);
	}

	if (columns.empty())
	{
		return;
	}

	std::stable_sort(columns.begin(), columns.end(), [&](const KString& a, const KString& b)
	{
		bool aIsPK = (table.bHasObjectId && a == table.sPrimaryKey);
		bool bIsPK = (table.bHasObjectId && b == table.sPrimaryKey);
		if (aIsPK != bIsPK) return aIsPK;
		return a < b;
	});

	KString sDropSQL = kFormat("drop table if exists {};", table.sTableName);
	KString sCreateSQL = kFormat("create table {} (\n", table.sTableName);
	
	for (std::size_t i = 0; i < columns.size(); ++i)
	{
		const auto& sColumn = columns[i];
		const auto& info = table.Columns.at(sColumn);
		bool bIsPK = (table.bHasObjectId && sColumn == table.sPrimaryKey);
		
		sCreateSQL += "    ";
		sCreateSQL += sColumn;
		sCreateSQL += "  ";
		sCreateSQL += SqlTypeToString(info);
		sCreateSQL += "  ";
		sCreateSQL += (info.bNullable && !bIsPK) ? "null" : "not null";
		if (bIsPK)
		{
			sCreateSQL += " primary key";
		}
		if (i + 1 < columns.size())
		{
			sCreateSQL += ",";
		}
		sCreateSQL += "\n";
	}
	
	sCreateSQL += ") engine=InnoDB default charset=utf8mb4;";

	if (m_Config.bOutputToStdout)
	{
		KOut.WriteLine(sDropSQL);
		KOut.WriteLine(sCreateSQL);
		KOut.WriteLine();
	}
	else if (m_Config.bHasDBC)
	{
		if (!m_SQL.ExecSQL("{}", sDropSQL))
		{
			KErr.FormatLine(">> failed to drop table: {}", m_SQL.GetLastError());
		}
		if (!m_SQL.ExecSQL("{}", sCreateSQL))
		{
			KErr.FormatLine(">> failed to create table: {}", m_SQL.GetLastError());
		}
	}

} // GenerateCreateTableSQL

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertCollectionNameToTableName (KStringView sCollectionName) const
//-----------------------------------------------------------------------------
{
	KString sTableName = KString{sCollectionName};
	
	while (!sTableName.empty() && !std::isalnum(sTableName.front()))
	{
		sTableName.erase(0, 1);
	}
	while (!sTableName.empty() && !std::isalnum(sTableName.back()))
	{
		sTableName.pop_back();
	}
	
	sTableName = sTableName.ToUpper();
	sTableName = ApplySingularizationToTableName(sTableName);
	
	while (!sTableName.empty() && sTableName.back() == '_')
	{
		sTableName.pop_back();
	}

	KString sPrefix = NormalizeTablePrefix(m_Config.sTablePrefix);
	if (!sPrefix.empty())
	{
		sTableName = sPrefix + sTableName;
	}

	return sTableName;

} // ConvertCollectionNameToTableName

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertFieldNameToColumnName (KStringView sFieldName) const
//-----------------------------------------------------------------------------
{
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
KString Mong2SQL::SanitizeColumnName (KStringView sName) const
//-----------------------------------------------------------------------------
{
	KString sResult = sName;
	sResult.Replace("$", "");
	sResult.Replace("-", "_");
	
	while (!sResult.empty() && !std::isalnum(static_cast<unsigned char>(sResult[0])))
	{
		sResult.erase(0, 1);
	}
	
	std::size_t iPos = 0;
	while ((iPos = sResult.find("__", iPos)) != KString::npos)
	{
		sResult.replace(iPos, 2, "_");
	}
	
	if (sResult.empty())
	{
		sResult = "col";
	}
	
	if (sResult.size() > 64)
	{
		sResult = sResult.Left(64);
	}
	
	// Check for reserved word collision and escape with trailing underscore
	// Reserved words are uppercase, so convert to upper for comparison
	if (kStrIn(sResult.ToUpper(), MYSQL_RESERVED_WORDS))
	{
		sResult += "_";
		kDebug(2, "escaped reserved word column: {} -> {}", sName, sResult);
	}
	
	return sResult;

} // SanitizeColumnName

//-----------------------------------------------------------------------------
KString Mong2SQL::BuildColumnName (const KString& sPrefix, KStringView sKey) const
//-----------------------------------------------------------------------------
{
	KString sResult;
	if (sPrefix.empty())
	{
		sResult = KString{sKey};
	}
	else
	{
		sResult = kFormat("{}_{}", sPrefix, sKey);
	}
	
	std::size_t iPos = 0;
	while ((iPos = sResult.find("__", iPos)) != KString::npos)
	{
		sResult.replace(iPos, 2, "_");
	}
	
	return sResult;

} // BuildColumnName

//-----------------------------------------------------------------------------
KString Mong2SQL::BuildChildTableName (KStringView sParentTable, KStringView sKey) const
//-----------------------------------------------------------------------------
{
	KString sOriginalSuffix = ExtractLeafColumnName(sKey);
	KString sSuffix = ConvertPluralToSingular(sOriginalSuffix.ToLower()).ToUpper();
	KString sResult = kFormat("{}_{}", sParentTable, sSuffix);
	return sResult;

} // BuildChildTableName

//-----------------------------------------------------------------------------
KString Mong2SQL::ExtractLeafColumnName (KStringView sQualified) const
//-----------------------------------------------------------------------------
{
	auto iPos = sQualified.find_last_of('_');
	if (iPos != KStringView::npos)
	{
		return KString{sQualified.substr(iPos + 1)};
	}
	return KString{sQualified};

} // ExtractLeafColumnName

//-----------------------------------------------------------------------------
KString Mong2SQL::GenerateTablePKEY (KStringView sTableName) const
//-----------------------------------------------------------------------------
{
	return kFormat("{}_oid", sTableName.ToLower());

} // GenerateTablePKEY

//-----------------------------------------------------------------------------
KString Mong2SQL::NormalizeTablePrefix (KStringView sPrefix) const
//-----------------------------------------------------------------------------
{
	if (sPrefix.empty())
	{
		return KString{};
	}
	
	KString sResult = KString{sPrefix}.ToUpper();
	while (!sResult.empty() && sResult.back() == '_')
	{
		sResult.pop_back();
	}
	if (!sResult.empty())
	{
		sResult += '_';
	}
	return sResult;

} // NormalizeTablePrefix

//-----------------------------------------------------------------------------
KString Mong2SQL::ApplySingularizationToTableName (KStringView sTableName) const
//-----------------------------------------------------------------------------
{
	if (sTableName.empty())
	{
		return KString{};
	}
	
	KString sResult = sTableName;
	std::size_t iLastUnderscore = sResult.find_last_of('_');
	
	if (iLastUnderscore != KString::npos && iLastUnderscore < sResult.size() - 1)
	{
		KString sPrefix = sResult.substr(0, iLastUnderscore + 1);
		KString sLastWord = sResult.substr(iLastUnderscore + 1);
		KString sLastWordLower = sLastWord.ToLower();
		
		if (sLastWordLower == "s")
		{
			sResult = sPrefix.substr(0, sPrefix.size() - 1);
		}
		else
		{
			KString sSingular = ConvertPluralToSingular(sLastWordLower);
			sResult = sPrefix + sSingular.ToUpper();
		}
	}
	else
	{
		KString sLower = sResult.ToLower();
		sResult = ConvertPluralToSingular(sLower).ToUpper();
	}
	
	return sResult;

} // ApplySingularizationToTableName

//-----------------------------------------------------------------------------
KString Mong2SQL::ConvertPluralToSingular (KStringView sPlural) const
//-----------------------------------------------------------------------------
{
	if (sPlural.empty())
	{
		return KString{sPlural};
	}
	
	KString sLower = KString{sPlural}.ToLower();
	KString sResult = sLower;
	
	// Simple rules (expanded ruleset in reference code, abbreviated here for conciseness)
	if (sResult.ends_with("ies") && sResult.size() > 3)
	{
		sResult.remove_suffix(3);
		sResult += 'y';
	}
	else if (sResult.ends_with("oes") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	else if (sResult.ends_with("ses") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	else if (sResult.ends_with("ches") && sResult.size() > 4)
	{
		sResult.remove_suffix(2);
	}
	else if (sResult.ends_with("shes") && sResult.size() > 4)
	{
		sResult.remove_suffix(2);
	}
	else if (sResult.ends_with("xes") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	else if (sResult.ends_with("zes") && sResult.size() > 3)
	{
		sResult.remove_suffix(2);
	}
	else if (sResult.ends_with("s") && sResult.size() > 1)
	{
		sResult.remove_suffix(1);
	}
	
	return sResult;

} // ConvertPluralToSingular

//-----------------------------------------------------------------------------
KString Mong2SQL::SanitizeMongoJSON (KStringView sJsonDoc) const
//-----------------------------------------------------------------------------
{
	KString sResult = sJsonDoc;
	sResult.Replace(" : nan", " : null");
	sResult.Replace(": nan", ": null");
	sResult.Replace(" : inf", " : null");
	sResult.Replace(": inf", ": null");
	sResult.Replace(" : -inf", " : null");
	sResult.Replace(": -inf", ": null");
	return sResult;

} // SanitizeMongoJSON

//-----------------------------------------------------------------------------
bool Mong2SQL::TableExistsInMySQL (KStringView sTableName)
//-----------------------------------------------------------------------------
{
	if (!m_Config.bHasDBC)
	{
		return false;
	}
	
	auto sResult = m_SQL.SingleStringQuery("select count(*) from INFORMATION_SCHEMA.TABLES where table_schema = database() and table_name = '{}'", sTableName);
	
	if (!sResult.empty())
	{
		auto count = sResult.Int64();
		return (count > 0);
	}
	return false;

} // TableExistsInMySQL

//-----------------------------------------------------------------------------
// Thread-safe helper methods using local KJSON structures
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
KJSON& Mong2SQL::EnsureTableSchemaLocal (KStringView sTableName, KJSON& localTables, KJSON& localTableOrder, KStringView sCollectionPath, KStringView sParentTable, KStringView sParentKeyColumn) const
//-----------------------------------------------------------------------------
{
	kDebug(2, "ensuring local schema for table: {}, path: {}", sTableName, sCollectionPath);
	
	if (!localTables.contains(sTableName))
	{
		KJSON& newTable = localTables[KString{sTableName}];
		newTable["table_name"] = sTableName;
		newTable["primary_key"] = "";
		newTable["parent_table"] = sParentTable;
		newTable["parent_key_column"] = sParentKeyColumn;
		newTable["has_object_id"] = false;
		newTable["collection_path"] = sCollectionPath;
		newTable["num_documents"] = 0;  // Initialize counter
		newTable["columns"] = KJSON::object();
		newTable["column_order"] = KJSON::array();
		
		localTableOrder += KString{sTableName};
		kDebug(2, "created new local table schema: {}", sTableName);
		
		if (!sParentKeyColumn.empty())
		{
			// Create a non-null string value for parent key column
			KJSON nonNullValue = "";
			AddColumnLocal(newTable, sParentKeyColumn, SqlType::Text, 16, false, nonNullValue);
		}
	}
	
	return localTables[KString{sTableName}];

} // EnsureTableSchemaLocal

//-----------------------------------------------------------------------------
void Mong2SQL::CollectSchemaForDocumentLocal (const KJSON& document, KStringView sTableName, const KString& sPrefix, KJSON& localTables, KJSON& localTableOrder) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "collecting local schema for document, table: '{}', prefix: '{}'", sTableName, sPrefix);
	if (!document.is_object())
	{
		return;
	}
	
	KJSON& tableData = localTables[KString{sTableName}];
	
	for (auto it = document.begin(); it != document.end(); ++it)
	{
		if (it.key() == "_id" && it->is_object() && it->contains("$oid"))
		{
			tableData["has_object_id"] = true;
			if (tableData["primary_key"].String().empty())
			{
				KString sPK = GenerateTablePKEY(sTableName);
				tableData["primary_key"] = sPK;
			}
			std::size_t iLength = std::max<std::size_t>(24, 16);
			// Create a non-null string value for primary key column
			KJSON nonNullValue = "";
			AddColumnLocal(tableData, tableData["primary_key"].String(), SqlType::Text, iLength, false, nonNullValue);
			continue;
		}
		
		KString sKey = ConvertFieldNameToColumnName(it.key());
		KString sRawColumnName = BuildColumnName(sPrefix, sKey);
		KString sSanitizedColumnName = SanitizeColumnName(sRawColumnName);
		const auto& value = *it;

		if (value.is_object())
		{
			CollectSchemaForDocumentLocal(value, sTableName, sSanitizedColumnName, localTables, localTableOrder);
		}
		else if (value.is_array())
		{
			CollectArraySchemaLocal(value, sTableName, sSanitizedColumnName, localTables, localTableOrder);
		}
		else
		{
			std::size_t iLength = 0;
			if (value.is_string())
			{
				iLength = value.String().size();
			}
			AddColumnLocal(tableData, sSanitizedColumnName, InferSqlType(value), iLength, true, value);
		}
	}

} // CollectSchemaForDocumentLocal

//-----------------------------------------------------------------------------
void Mong2SQL::CollectArraySchemaLocal (const KJSON& array, KStringView sParentTableName, const KString& sKey, KJSON& localTables, KJSON& localTableOrder) const
//-----------------------------------------------------------------------------
{
	kDebug(3, "collecting local array schema for key '{}' with {} elements", sKey, array.size());
	if (!array.is_array() || array.empty())
	{
		return;
	}

	// Get parent's collection path and build child path
	KString sParentPath = localTables[KString{sParentTableName}]["collection_path"].String();
	KString sLeafField = ExtractLeafColumnName(sKey);  // Extract the field name portion
	KString sChildPath = kFormat("{}/{}", sParentPath, sLeafField);

	KString sParentPK = localTables[KString{sParentTableName}]["primary_key"].String();
	KString sChildTableName = SanitizeColumnName(BuildChildTableName(sParentTableName, sKey));
	KJSON& childTable = EnsureTableSchemaLocal(sChildTableName, localTables, localTableOrder, sChildPath, sParentTableName, sParentPK);
	
	// Don't automatically generate primary key for child tables
	// Only set if the child documents have their own _id field

	for (const auto& element : array)
	{
		// Increment document count for each array element
		childTable["num_documents"] = childTable["num_documents"].Int64() + 1;
		
		if (element.is_object())
		{
			CollectSchemaForDocumentLocal(element, sChildTableName, KString{}, localTables, localTableOrder);
		}
		else if (element.is_array())
		{
			KString sNestedKey = SanitizeColumnName(ExtractLeafColumnName(sKey));
			CollectArraySchemaLocal(element, sChildTableName, sNestedKey, localTables, localTableOrder);
		}
		else
		{
			KString sValueColumn = SanitizeColumnName(ExtractLeafColumnName(sKey));
			std::size_t iLength = 0;
			if (element.is_string())
			{
				iLength = element.String().size();
			}
			AddColumnLocal(childTable, sValueColumn, InferSqlType(element), iLength, true, element);
		}
	}

} // CollectArraySchemaLocal

//-----------------------------------------------------------------------------
void Mong2SQL::AddColumnLocal (KJSON& tableData, KStringView sColumnName, SqlType type, std::size_t iLength, bool isNullable, const KJSON& value) const
//-----------------------------------------------------------------------------
{
	bool bIsNull = value.is_null();
	bool bIsZero = false;
	
	// Check if value is numeric zero
	if ((type == SqlType::Integer || type == SqlType::Float) && !bIsNull)
	{
		if (value.is_number_integer() || value.is_number_unsigned())
		{
			bIsZero = (value.Int64() == 0);
		}
		else if (value.is_number_float())
		{
			bIsZero = (value.Float() == 0.0);
		}
	}
	
	kDebug(3, "adding local column '{}' to table '{}', isNull={}, isZero={}", sColumnName, tableData["table_name"].String(), bIsNull, bIsZero);
	
	KJSON& columns = tableData["columns"];
	KString sColKey{sColumnName};
	
	if (!columns.contains(sColKey))
	{
		// New column
		tableData["column_order"] += sColKey;
		
		KJSON& columnInfo = columns[sColKey];
		columnInfo["type"] = static_cast<int>(type);
		columnInfo["nullable"] = isNullable;
		columnInfo["max_length"] = iLength;
		columnInfo["has_non_null"] = !bIsNull;  // Track if we've seen non-null data
		columnInfo["has_non_zero"] = !bIsZero;  // Track if we've seen non-zero data (for numeric types)
		
		// Compute SQL type string
		ColumnInfo tempInfo;
		tempInfo.Type = type;
		tempInfo.iMaxLength = iLength;
		tempInfo.bNullable = isNullable;
		columnInfo["sql_type"] = SqlTypeToString(tempInfo);
	}
	else
	{
		// Existing column - merge types
		KJSON& columnInfo = columns[sColKey];
		SqlType existingType = static_cast<SqlType>(columnInfo["type"].Int64());
		SqlType mergedType = MergeSqlTypes(existingType, type);
		columnInfo["type"] = static_cast<int>(mergedType);
		
		if (!isNullable)
		{
			columnInfo["nullable"] = false;
		}
		
		// Update has_non_null flag: once true, stays true
		if (!bIsNull)
		{
			columnInfo["has_non_null"] = true;
		}
		
		// Update has_non_zero flag: once true, stays true
		if (!bIsZero)
		{
			columnInfo["has_non_zero"] = true;
		}
		
		if (mergedType == SqlType::Text)
		{
			std::size_t currentMaxLength = columnInfo["max_length"].UInt64();
			if (iLength > currentMaxLength)
			{
				columnInfo["max_length"] = iLength;
			}
		}
		else
		{
			columnInfo["max_length"] = 0;
		}
		
		// Recompute SQL type string
		ColumnInfo tempInfo;
		tempInfo.Type = mergedType;
		tempInfo.iMaxLength = columnInfo["max_length"].UInt64();
		tempInfo.bNullable = columnInfo["nullable"].Bool();
		columnInfo["sql_type"] = SqlTypeToString(tempInfo);
	}

} // AddColumnLocal

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
