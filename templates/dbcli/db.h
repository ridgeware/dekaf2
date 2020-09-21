
#pragma once

#include <dekaf2/ksql.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kpool.h>
#include <memory>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// database connector class
class DB : public KSQL
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum
	{
		INITIAL_SCHEMA    = INT32_C(100),  // <-- never alter this
		CURRENT_SCHEMA    = INT32_C(100),  // <-- increment to match highest "schema/__LowerProjectName__-NNN.ksql" file
	};

	/// Check for valid DB connection, throws on error
	void EnsureConnected();

	/// Check for updates of the DB schema
	void EnsureSchema (bool bForce = false);

	/// Clear all internal state
	void clear();

	/// Get a DB connector from the connector pool or create a new one.
	/// This will use the DBC file set with SetDBCFilename(), the
	/// /etc/__LowerProjectName__.dbc file, or environment variables to
	/// configure the DB connection.
	static KSharedPool<DB>::unique_ptr Get();

	/// Get const ref on ini, load from sIniFilename if not yet loaded
	static const KSQL::IniParms& GetIni(KStringViewZ sIniFilename = KStringViewZ{});

	/// Lookup of ini value
	static const KString& IniValue (KStringView sParm);

	/// Set the static DBC filename that is used for all subsequent DB::Get() requests
	static void SetDBCFilename (KString sDBCFilename) { s_sDBCFilename = std::move(sDBCFilename); }

//----------
protected:
//----------

	static KString        s_sDBCFilename;
	static KSQL::IniParms s_INI;
	static bool           s_bIniLoaded;

}; // DB

