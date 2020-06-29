
#include "db.h"
#include "build_config.h"
#include "__LowerProjectName__.h"
#include <dekaf2/khttperror.h>
#include <mutex>

//-----------------------------------------------------------------------------
void DB::EnsureConnected ()
//-----------------------------------------------------------------------------
{
	if (!KSQL::EnsureConnected("__ProjectName__", m_sDBCFile, GetIni()))
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, GetLastError() };
	}

	// we have an open database connection

} // EnsureConnected

//-----------------------------------------------------------------------------
void DB::EnsureSchema (bool bForce/*=false*/)
//-----------------------------------------------------------------------------
{
	kDebug (2, "force={} ...", bForce ? "true" : "false");

	if (bForce && m_bLiveDB)
	{
		m_sLastError.Format ("action -schemaforce is prohibited on live databases");
		throw KHTTPError { KHTTPError::H5xx_ERROR, GetLastError() };
	}

	if (!KSQL::EnsureSchema("__UpperProjectName__", BUILD_CONFIG::SCHEMA_DIR, "__LowerProjectName__-", CURRENT_SCHEMA, INITIAL_SCHEMA, bForce))
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
std::shared_ptr<DB> DB::Get (KStringViewZ sDBCFilename)
//--------------------------------------------------------------------------------
{
	static std::vector<std::shared_ptr<DB>> s_DBStore;
	static std::shared_mutex s_Mutex;

	std::shared_ptr<DB> db;
	bool bIsDefaultDBC = sDBCFilename.empty() || sDBCFilename == s_sDBCFilename;

	// only check in connector cache if this is our default DBC file
	if (bIsDefaultDBC)
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

		// only add to the cache if it is the default DBC file
		if (bIsDefaultDBC)
		{
			db->SetDBC (s_sDBCFilename);

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
			db->SetDBC (sDBCFilename);

			// setup database connection
			db->EnsureConnected();
		}
	}
	else
	{
		db->clear();
		db->EnsureConnected();
	}

	return db;

} // Get

//-----------------------------------------------------------------------------
const KSQL::IniParms& DB::GetIni(KStringViewZ sIniFilename)
//-----------------------------------------------------------------------------
{
	if (!s_bIniLoaded && !sIniFilename.empty())
	{
		static std::mutex Mutex;
		std::lock_guard Lock(Mutex);

		if (!s_bIniLoaded)
		{
			s_INI.Load(sIniFilename);
			s_bIniLoaded = true;
		}
	}
	return s_INI;

} // GetIni

//-----------------------------------------------------------------------------
const KString& DB::IniValue (KStringView sParm)
//-----------------------------------------------------------------------------
{
	static KString s_sEmpty;

	// this check is necessary to avoid any race on a KProps struct that is
	// loaded in another thread
	if (!s_bIniLoaded)
	{
		return s_sEmpty;
	}
	return s_INI[sParm];

} // IniValue

//static variables
KString        DB::s_sDBCFilename;
KSQL::IniParms DB::s_INI;
bool           DB::s_bIniLoaded { false };
