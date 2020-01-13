
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

	// these constants are used both by INI file and ENV VARs:
	static constexpr KStringViewZ __UpperProjectName___DBUSER     = "__UpperProjectName___DBUSER";
	static constexpr KStringViewZ __UpperProjectName___DBPASS     = "__UpperProjectName___DBPASS";
	static constexpr KStringViewZ __UpperProjectName___DBNAME     = "__UpperProjectName___DBNAME";
	static constexpr KStringViewZ __UpperProjectName___DBTYPE     = "__UpperProjectName___DBTYPE";
	static constexpr KStringViewZ __UpperProjectName___DBHOST     = "__UpperProjectName___DBHOST";
	static constexpr KStringViewZ __UpperProjectName___DBPORT     = "__UpperProjectName___DBPORT";
	static constexpr KStringViewZ __UpperProjectName___SCHEMA_DIR = "__UpperProjectName___SCHEMA_DIR";

	/// check for valid DB connection, throws on error
	void EnsureConnected();

	/// check for updates of the DB schema
	void EnsureSchema (bool bForce = false);

	/// get a db connector from the connector cache or create a new one
	static std::shared_ptr<DB> Get();

	/// set dbc file and load contents, returns false if no content or no file
	static bool SetDBCFile (KString sDBCFile);

//----------
private:
//----------

	/// clear all internal state
	void clear();

	static KString s_sDBCFile;
	static KString s_sDBCFileContent;
	static bool    s_bDBCIsLoaded;

}; // DB

