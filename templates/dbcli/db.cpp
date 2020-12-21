
#include "db.h"
#include "build_config.h"
#include "__LowerProjectName__.h"
#include <dekaf2/khttperror.h>
#include <dekaf2/kpool.h>
#include <mutex>

//-----------------------------------------------------------------------------
void DB::EnsureConnected ()
//-----------------------------------------------------------------------------
{
	if (!KSQL::EnsureConnected("__ProjectName__", GetDBC(), GetIni()))
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

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Pool control class that customizes initialization of a newly popped connector
class DBPoolControl : public KPoolControl<DB>
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//--------------------------------------------------------------------------------
	DBPoolControl(KStringViewZ sDBCFilename)
	//--------------------------------------------------------------------------------
	: m_sDBCFilename(sDBCFilename)
	{
	}

	//--------------------------------------------------------------------------------
	void Popped(DB* db, bool bIsNew)
	//--------------------------------------------------------------------------------
	{
		// do anything here that may be useful when a connector is taken from
		// the pool (or newly created)

		if (bIsNew)
		{
			// setup database connection
			db->SetDBC(m_sDBCFilename);
		}
		else
		{
			// make sure reused connector is cleared from previous use
			db->clear();
		}

		db->EnsureConnected();
	}

	//--------------------------------------------------------------------------------
	void Pushed(DB* db)
	//--------------------------------------------------------------------------------
	{
		// do anything here that may be useful on return of a connector to the pool
	}

//----------
private:
//----------

	KStringViewZ m_sDBCFilename;

}; // DBPoolControl

//--------------------------------------------------------------------------------
KSharedPool<DB>::unique_ptr DB::Get ()
//--------------------------------------------------------------------------------
{
	static DBPoolControl Control(s_sDBCFilename);
	static KSharedPool<DB> Pool(Control);

	return Pool.get();

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
