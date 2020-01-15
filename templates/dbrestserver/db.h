
#pragma once

#include <dekaf2/ksql.h>
#include <dekaf2/kprops.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <memory>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DB : public KSQL
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum
	{
		INITIAL_SCHEMA    = INT32_C(100),  // <-- never alter this
		CURRENT_SCHEMA    = INT32_C(100),  // <-- increment to match "schema/__LowerProjectName__-NNN.ksql" files
	};

	/// Check for valid DB connection, throws on error
	void EnsureConnected();

	/// Check for updates of the DB schema
	void EnsureSchema (bool bForce = false);

	/// Get a db connector from the connector cache or create a new one.
	/// To use the caching pool leave sDBCFilename empty, it will then
	/// fall back to the DBC file set with SetDBCFilename(), the
	/// /etc/__LowerProjectName__.dbc file, or environment variables.
	/// When sDBCFilename is set, this will create a non-cached connector
	/// based on this DBC file.
	static std::shared_ptr<DB> Get(KStringViewZ sDBCFilename = KStringViewZ{});

	/// Get const ref on ini, load from sIniFilename if not yet loaded
	static const KSQL::IniParms& GetIni(KStringViewZ sIniFilename = KStringViewZ{});

	/// Lookup of ini value
	static const KString& IniValue (KStringView sParm);

	/// Set the static DBC filename that is used for all subsequent DB::Get() requests
	/// without explicit DBC file
	static void SetDBCFilename (KString sDBCFilename) { s_sDBCFilename = std::move(sDBCFilename); }

//----------
protected:
//----------

	static KString        s_sDBCFilename;
	static KSQL::IniParms s_INI;
	static bool           s_bIniLoaded;

	/// Clear all internal state
	void clear();

}; // DB

