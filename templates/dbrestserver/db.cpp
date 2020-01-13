
#include "db.h"
#include "__LowerProjectName__.h"
#include <dekaf2/khttperror.h>
#include <mutex>

//-----------------------------------------------------------------------------
void DB::EnsureConnected ()
//-----------------------------------------------------------------------------
{
	if (IsConnectionOpen())
	{
		kDebug (2, "already connected to: {}", ConnectSummary());
		return; // already connected
	}

	kDebug (1, "looks like we need to connect...");

	//   1. environment vars        -- overrides all others
	//   2. -dbc on command line    -- handled by SetDBCFile() from option parsing
	//   3. /etc/__LowerProjectName__.dbc
	//   4. /etc/__LowerProjectName__.ini

	const auto& INI = __ProjectName__::GetIniParms ();

	if (!s_bDBCIsLoaded)
	{
		static std::mutex s_Mutex;
		std::lock_guard Lock(s_Mutex);

		if (s_sDBCFileContent.empty())
		{
			kDebug (1, "3. looking for: {}", "/etc/__LowerProjectName__.dbc");
			if (kFileExists ("/etc/__LowerProjectName__.dbc"))
			{
				kDebug (1, "loading: {}", "/etc/__LowerProjectName__.dbc");
				SetDBCFile ("/etc/__LowerProjectName__.dbc");
			}
		}

		if (s_sDBCFileContent.empty())
		{
			kDebug (1, "4. looking for: {}", __ProjectName__::__UpperProjectName___INI);
			if (!INI.empty())
			{
				kDebug (1, "using settings from ini file");
			}
		}

		// set the load flag, do not try again if not successful
		s_bDBCIsLoaded = true;
	}

	if (s_bDBCIsLoaded && !s_sDBCFileContent.empty())
	{
		kDebug (1, "loading: {}", s_sDBCFile);
		if (!SetConnect (s_sDBCFile, s_sDBCFileContent))
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, GetLastError() };
		}
	}

	KString sInitialConfig = ConnectSummary();

	kDebug (1, "5. checking for environment overrides (piecemeal acceptable)");
	SetDBType (kFirstNonEmpty<KStringView>(kGetEnv (__UpperProjectName___DBTYPE), INI.Get (__UpperProjectName___DBTYPE), TxDBType(GetDBType())));
	SetDBUser (kFirstNonEmpty<KStringView>(kGetEnv (__UpperProjectName___DBUSER), INI.Get (__UpperProjectName___DBUSER), GetDBUser()));
	SetDBPass (kFirstNonEmpty<KStringView>(kGetEnv (__UpperProjectName___DBPASS), INI.Get (__UpperProjectName___DBPASS), GetDBPass()));
	SetDBHost (kFirstNonEmpty<KStringView>(kGetEnv (__UpperProjectName___DBHOST), INI.Get (__UpperProjectName___DBHOST), GetDBHost()));
	SetDBName (kFirstNonEmpty<KStringView>(kGetEnv (__UpperProjectName___DBNAME), INI.Get (__UpperProjectName___DBNAME), GetDBName()));
	SetDBPort (kFirstNonEmpty<KStringView>(kGetEnv (__UpperProjectName___DBPORT), INI.Get (__UpperProjectName___DBPORT), KString::to_string(GetDBPort())).UInt32());

	if (kWouldLog(1))
	{
		KString sChanged = ConnectSummary();
		if (sInitialConfig != sChanged)
		{
			kDebug (1, "db configuration changed through ini file or env vars\n  from: {}\n    to: {}",
					sInitialConfig, sChanged);
		}
	}

	kDebug (1, "attempting to connect ...");
	
	if (!OpenConnection())
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, GetLastError() };
	}

	kDebug (1, "connected to: {}", ConnectSummary());

	// we have an open database connection

} // DB::EnsureConnected

//-----------------------------------------------------------------------------
void DB::EnsureSchema (bool bForce/*=false*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "force={} ...", bForce ? "true" : "false");

	const auto& INI = __ProjectName__::GetIniParms ();

	KString sSchemaDir = INI.Get (__UpperProjectName___SCHEMA_DIR);

	if (sSchemaDir.empty())
	{
		sSchemaDir = "/usr/local/share/__LowerProjectName__/schema";
	}

	if (!kGetEnv(__UpperProjectName___SCHEMA_DIR).empty())
	{
		// override schema from environment
		sSchemaDir = kGetEnv (__UpperProjectName___SCHEMA_DIR);
	}

	sSchemaDir += "/__LowerProjectName__-{}.ksql";

	if (!KSQL::EnsureSchema ("__UpperProjectName___SCHEMA", INITIAL_SCHEMA, CURRENT_SCHEMA, sSchemaDir, bForce))
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, GetLastError() };
	}

	// schema is up to date

} // EnsureSchema

//--------------------------------------------------------------------------------
void DB::clear()
//--------------------------------------------------------------------------------
{
	// insert code to clean up internal state for a new REST request

} // clear

//--------------------------------------------------------------------------------
bool DB::SetDBCFile (KString sDBCFile)
//--------------------------------------------------------------------------------
{

	s_sDBCFile = std::move(sDBCFile);
	s_sDBCFileContent = kReadAll (s_sDBCFile);

	return !s_sDBCFileContent.empty();

} // SetDBCFile

//--------------------------------------------------------------------------------
std::shared_ptr<DB> DB::Get()
//--------------------------------------------------------------------------------
{
	static std::vector<std::shared_ptr<DB>> s_DBStore;
	static std::shared_mutex s_Mutex;

	std::shared_ptr<DB> db;

	{
		std::shared_lock Lock(s_Mutex);
		for (auto& it : s_DBStore)
		{
			// due to our lock_guard it should be safe to assume that
			// use_count was increased before leaving the guarded section,
			// so we can rely on the connector being unused if use_count is 1.
			if (it.use_count() == 1)
			{
				// got an unused connector
				db = it;
				break;
			}
		}
	}

	// left the lock

	if (!db)
	{
		// did not find an unused connector, create one
		db = std::make_shared<DB>();

		// setup database connection
		db->EnsureConnected();

		// protect again by lock
		std::unique_lock Lock(s_Mutex);

		// is this the first connection?
		if (s_DBStore.empty())
		{
			// make sure we have the current schema
			db->EnsureSchema();
		}

		// push the new connector into the store (which increases use_count to 2..)
		s_DBStore.push_back(db);
	}
	else
	{
		db->clear();
		db->EnsureConnected();
	}

	return db;

} // Get


// static variables
KString DB::s_sDBCFile;
KString DB::s_sDBCFileContent;
bool    DB::s_bDBCIsLoaded { false };
