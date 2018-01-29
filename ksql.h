/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
//
*/

#pragma once

//
// Note:
//  To use the KSQL class for ORACLE and MYSQL you do *NOT* need to
//  define any special compile-time constants.  These constants
//  are provided in the outside chance that you have the correct
//  header files and want access to extra database features which
//  the KSQL class does not provide.
//
//  By putting "#define ORACLECOMPILE" before the "#include ksql.h"
//  you will need to satisfy the compiler for the oracle header files 
//  below (they are in various directories under ORACLE_HOME).
//
//  By putting "#define MYSQLCOMPILE" before the "#include ksql.h"
//  you will need to satisfy the compiler for the mysql header files 
//  below (they come with libmysqlclient.a).
//

#ifdef ORACLECOMPILE
  #if 0 // for gnu attempts, not msdev
  #ifdef WIN32
    #undef _WIN64
    #ifndef _int64
      #ifdef _INT64
        #define _int64 _INT64
      #else
        #define _int64 int64_t
      #endif
    #endif
  #endif
  #endif

  #ifdef max
    #undef max
  #endif
  #ifdef min
    #undef min
  #endif

  extern "C"
  {
    #include <oratypes.h>   // OCI6
    #include <ociapr.h>     // OCI6
    #include <oci.h>        // OCI8
    #ifndef NOSQLCA
      #include <sqlca.h>      // for global sqlca struct
    #endif
  }

  #ifdef boolean
    #undef boolean
  #endif
#endif

enum {
	PARSE_NO_DEFER   = 0,     // 
	PARSE_V7_LNG     = 2,     // 
	VAR_NOT_IN_LIST  = 1007,  // oracle constants: placed here so that we can compile w/out oracle
	STRING_TYPE      = 5,     // 
	NO_DATA_FOUND    = 1403   // 
};

#include <dekaf2/dekaf2all.h>         // <-- KSQL requires DEKAF2 framework

#ifdef ODBC
  #include <sqlext.h>         // <-- Microsoft ODBC API's
#endif

#ifdef MYSQLCOMPILE
  #ifdef WIN32
    #define NO_CLIENT_LONG_LONG // <-- for mysql header file
  #endif
  #ifdef cygwin
    #undef STDCALL
  #endif
  #include <mysql.h>          // mysql top include
  //nclude <mysql_com.h>      // included by mysql.h
  //nclude <mysql_version.h>  // included by mysql.h
#endif

// to get DBLIB member vars to compile:
struct tds_dblib_dbprocess;  typedef struct tds_dblib_dbprocess DBPROCESS;

// to get CTLIB member vars to compile:
struct _cs_config;           typedef struct _cs_config CS_CONFIG;
struct _cs_context;          typedef struct _cs_context CS_CONTEXT;
struct _cs_connection;       typedef struct _cs_connection CS_CONNECTION;
struct _cs_locale;           typedef struct _cs_locale CS_LOCALE;
struct _cs_command;          typedef struct _cs_command CS_COMMAND;
struct _cs_blk_row;          typedef struct _cs_blk_row CS_BLK_ROW;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// KSQL: DATABASE CONNECTION CLASS
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

namespace dekaf2
{

enum { MAXCOLNAME = 100};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
typedef struct
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	char*   dszValue;
	char    szColName[MAXCOLNAME+1];
	int     iDataType;
	int     iMaxDataLen;
	short   indp;   // oratypes.h:typedef   signed short    sb2;

} COLINFO;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Normalizing date/time columns into format like "20010723211321"...
//
//   MySQL:  no conversion necessary
//   ORACLE: select to_char(colname,'YYYYMMDDHH24MISS')
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#define ESCAPE_MYSQL "\'\\"
#define ESCAPE_MSSQL "\'"

/* DBC file format structures:
   These structures, once established, CAN NOT CHANGE.
   Size and offset of each field must be retained as-is in order to maintain compatibility with existing DBC files.
   The first field, "szLeader[]", is used to identify which format a particular file follows
*/
struct DBCFILEv1
{
	enum
	{
		// DBCFILEv1 used 51 bytes for each authentication field
		// however, older versions of KSQL will truncate the fields when reading them if they are over 49 characters long.
		// Therefore, for compatibility, MAXLEN_CONNECPARM is 49 characters even though sizeof(szUsername) is still 51 bytes.
		MAXLEN_CONNECTPARM = 49
	};

	char           szLeader[10];     // <-- length of leader can never change as struct gets rev'ed
	int            iDBType;
	int            iAPISet;
	unsigned char  szUsername[MAXLEN_CONNECTPARM+2]; // order: UPDH
	unsigned char  szPassword[MAXLEN_CONNECTPARM+2];
	unsigned char  szDatabase[MAXLEN_CONNECTPARM+2];
	unsigned char  szHostname[MAXLEN_CONNECTPARM+2];
	int            iDBPortNum;
};

struct DBCFILEv2
{
	enum
	{
		// DBCFILEv2 used 51 bytes for each authentication field
		// however, older versions of KSQL will truncate the fields when reading them if they are over 49 characters long.
		// Therefore, for compatibility, MAXLEN_CONNECPARM is 49 characters even though sizeof(szUsername) is still 51 bytes.
		MAXLEN_CONNECTPARM = 49
	};

	char          szLeader[10];     // <-- length of leader can never change as struct gets rev'ed
	uint8_t       iDBType[4];
	uint8_t       iAPISet[4];
	unsigned char szUsername[MAXLEN_CONNECTPARM+2]; // order: UPDH
	unsigned char szPassword[MAXLEN_CONNECTPARM+2];
	unsigned char szDatabase[MAXLEN_CONNECTPARM+2];
	unsigned char szHostname[MAXLEN_CONNECTPARM+2];
	uint8_t       iDBPortNum[4];
};

/* DBCFilev3 was introduced to increase the maximum length of the connection parameters */
struct DBCFILEv3
{
	enum
	{
		MAXLEN_CONNECTPARM = 127
	};

	char          szLeader[10];     // <-- length of leader can never change as struct gets rev'ed
	uint8_t       iDBType[4];
	uint8_t       iAPISet[4];
	unsigned char szUsername[MAXLEN_CONNECTPARM+1]; // order: UPDH
	unsigned char szPassword[MAXLEN_CONNECTPARM+1];
	unsigned char szDatabase[MAXLEN_CONNECTPARM+1];
	unsigned char szHostname[MAXLEN_CONNECTPARM+1];
	uint8_t       iDBPortNum[4];
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KCOL
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//----------
public:
//----------
	KCOL () = default;

	KCOL (KStringView _sValue, uint64_t _iFlags=0, uint32_t _iMaxLen=0)
	{
		sValue  = _sValue;
		iFlags  = _iFlags;
		iMaxLen = _iMaxLen;
	}

	KCOL (KCOL&&)                  = default;
	KCOL (const KCOL&)             = default;
	KCOL& operator = (KCOL&&)      = default;
	KCOL& operator = (const KCOL&) = default;

	KString  sValue;  // aka "second"
	uint64_t iFlags;
	uint32_t iMaxLen;
};

typedef KProps <KString, KCOL,    /*order-matters=*/true, /*unique-keys*/true> KCOLS;
typedef KProps <KString, KString, /*order-matters=*/true, /*unique-keys*/true> KPROPS;

//-----------------------------------------------------------------------------
class KROW : public KCOLS
//-----------------------------------------------------------------------------
{
//----------
public:
//----------
	template<class... Args>
	KROW(Args&&... args)
	    : KCOLS(std::forward<Args>(args)...)
	{
	}

	KROW (KROW&&)                  = default;
	KROW (const KROW&)             = default;
	KROW& operator = (KROW&&)      = default;
	KROW& operator = (const KROW&) = default;

	KROW (KStringView sTablename) {
		m_sTablename =  sTablename;
	}

	void SetTablename (KStringView sTablename) {
		m_sTablename =  sTablename;
	}

	bool AddCol (KStringView sColName, KStringView sValue=nullptr, uint64_t iFlags=0, uint32_t iMaxLen=0) {
		KCOL col (sValue, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, col) != KCOLS::end());
	}

	bool AddCol (KStringView sColName, int64_t iValue, uint64_t iFlags=0, uint32_t iMaxLen=0) {
		KString sValue; sValue.Format ("{}", iValue);
		KCOL col (sValue, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, col) != KCOLS::end());
	}

	bool AddCol (KStringView sColName, uint64_t iValue, uint64_t iFlags=0, uint32_t iMaxLen=0) {
		KString sValue; sValue.Format ("{}", iValue);
		KCOL col (sValue, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, col) != KCOLS::end());
	}

	bool AddCol (KStringView sColName, double nValue, uint64_t iFlags=0, uint32_t iMaxLen=0)
	{
		KString sValue; sValue.Format ("{}", nValue);
		KCOL col (sValue, iFlags, iMaxLen);
		return (KCOLS::Add (sColName, col) != KCOLS::end());
	}

	bool SetValue (KStringView sColName, KStringView sValue)
	{
		auto it = KCOLS::find (sColName);
		if (it == KCOLS::KeyIndex().end()) {
			KCOL col(sValue);
			return (KCOLS::Add (sColName, col) != KCOLS::end());
		}
		else {
			it->second.sValue = sValue;
			return (true);
		}
	}

	bool SetValue (KStringView sColName, int64_t iValue)
	{
		KString sValue; sValue.Format ("{}", iValue);
		auto it = KCOLS::find (sColName);
		if (it == KCOLS::KeyIndex().end()) {
			KCOL col(sValue);
			return (KCOLS::Add (sColName, col) != KCOLS::end());
		}
		else {
			it->second.sValue = sValue;
			return (true);
		}
	}

	/// Returns the Nth column's name (note: column index starts at 0).
	KStringView GetName (size_t iZeroBasedIndex)
	{
		return (at (iZeroBasedIndex).first);
	}

	/// Returns the Nth column's value as a string (note: column index starts at 0).  Note that you can map this to literally any data type by using KStringView member functions like .Int32().
	KStringView GetValue (size_t iZeroBasedIndex)
	{
		return (at (iZeroBasedIndex).second.sValue);
	}

	/// Returns the Nth column's flags (note: column index starts at 0).
	bool GetFlags (size_t iZeroBasedIndex)
	{
		return (at (iZeroBasedIndex).second.iFlags);
	}

	/// Returns whether or not a particular flag is set on the Nth column (note: column index starts at 0).
	bool IsFlag (size_t iZeroBasedIndex, uint64_t iFlag)
	{
		return (GetFlags(iZeroBasedIndex) & iFlag);
	}

	/// Returns the maximum character length of the Nth column if it was set (note: column index starts at 0).
	uint32_t MaxLength (uint32_t iZeroBasedIndex) {
		return (at (iZeroBasedIndex).second.iMaxLen);
	}

	/// Formats the proper RDBMS DDL statement for inserting one row into the database for the given table and column structure.
	bool FormInsert (KString& sSQL, int iDBType, bool fUnicode=false, bool fIdentityInsert=false);

	/// Formats the proper RDBMS DDL statement for updating one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormUpdate (KString& sSQL, int iDBType, bool fUnicode=false);

	/// Formats the proper RDBMS DDL statement for deleting one row in the database for the given table and column structure.
	/// Note that at least one column must have the PKEY flag set (so that the framework knows what to put in the WHERE clause).
	bool FormDelete (KString& sSQL, int iDBType, bool fUnicode=false);

	/// Returns the last RDBMS error message.
	KStringView GetLastError() { return (m_sLastError); }

	/// Returns the tablename of this KROW object (if set).
	KStringView GetTablename() { return (m_sTablename); }

	enum
	{
		PKEY             = 0x0000001,   ///< Indicates given column is part of the primary key.  At least one column must have the PKEY flag to use KROW to do UPDATE and DELETE.
		NONCOLUMN        = 0x0000010,   ///< Indicates given column is not a column and should be included in DDL statements.
		EXPRESSION       = 0x0000100,   ///< Indicates given column is not a column and should be included in DDL statements.
		INSERTONLY       = 0x0001000,   ///< Indicates given column is only to be used in INSERT statements (not UPDATE or DELETE).
		NUMERIC          = 0x0010000,   ///< Indicates given column should not be quoted when forming DDL statements.
		NULL_IS_NOT_NIL  = 0x0100000    ///< Indicates given column is ???
	};

	// - - - - - - - - - - - - - - - -
	// helper functions:
	// - - - - - - - - - - - - - - - -
	static void EscapeChars (KStringView sString, KString& sEscaped, int iDBType);
	static void EscapeChars (KStringView sString, KString& sEscaped, const char* szCharsToEscape, int iEscapeChar=0);

//----------
private:
//----------
	void SmartClip (KStringView sColName, KString& sValue, size_t iMaxLen);

	KString m_sTablename;
	KString m_sLastError;
};

//-----------------------------------------------------------------------------
class KSQL
//-----------------------------------------------------------------------------
{
//----------
public:
//----------
	enum
	{
		// the DBT constant is used to govern certain SQL statments:
		DBT_MYSQL             = 100,        // assume we're connecting to MySQL
		DBT_ORACLE6           = 206,        // assume we're connecting to an Oracle6 rdbms
		DBT_ORACLE7           = 207,        // assume we're connecting to an Oracle7 rdbms
		DBT_ORACLE8           = 208,        // assume we're connecting to an Oracle8 rdbms
		DBT_ORACLE            = 200,        // use the latest assumptions about Oracle
		DBT_SQLSERVER         = 300,
		DBT_SYBASE            = 400,
		DBT_INFORMIX          = 500,

		// the API constant determines how to connect to the database
		API_MYSQL             = 10000,      // connect to MYSQL using their custom APIs
		API_OCI6              = 26000,      // connect to Oracle using the v6 OCI (older set)
		API_OCI8              = 28000,      // connect to Oracle using the v8 OCI (the default)
		API_DBLIB             = 40000,      // connect to SQLServer or Sybase using DBLIB
		API_CTLIB             = 50000,      // connect to SQLServer or Sybase using CTLIB
		API_INFORMIX          = 80000,      // connect to Informix using their API
		API_ODBC              = 90000,      // connect to something using ODBC APIs

		// blob encoding schemes:
		BT_ASCII              = 'A',        // ASCII:  only replace newlines and single quotes
		BT_BINARY             = 'B',        // BINARY: encode every char into base 256 (2 digit hex)
		MAXLEN_CONNECTPARM    = DBCFILEv3::MAXLEN_CONNECTPARM,
		MAX_BLOBCHUNKSIZE     =  2000,      // LCD between Oracle, Sybase, MySQL, and Informix
		MAXLEN_CURSORNAME     =    50,
		NUM_RETRIES           =     5,      // <-- when db connection is lost
		MAX_CHARS_CTLIB       = 8000,       //varchar columns hold at most 8000 characters.

		F_IgnoreSQLErrors     = 0x00000001,  // <-- only effects the WarningLog
		F_BufferResults       = 0x00000010,  // <-- for use with ResetBuffer()
		F_NoAutoCommit        = 0x00000100,  // <-- only used by Oracle
		F_NoTranslations      = 0x00001000,  // <-- turn off {{token}} translations in SQL
		F_IgnoreSelectKeyword = 0x00010000,  // <-- override check in ExecQuery() for "select..."
		F_NoKlogDebug         = 0x00100000,  // <-- quietly: do not output the customary klog debug statements
		F_AutoReset           = 0x01000000,  // <-- For ctlib, refresh the connection to the server for each query

		FORM_ASCII            = 'a',         // <-- for OutputQuery() method
		FORM_CSV              = 'c',
		FORM_HTML             = 'h'
	};

	const char* BAR = "--------------------------------------------------------------------------------"; // for printf() so keep this const char*

	KSQL (uint32_t iFlags=0, int iDebugID=0, int iDBType=DBT_MYSQL, KStringView sUsername=NULL, KStringView sPassword=NULL, KStringView sDatabase=NULL, KStringView sHostname=NULL, uint32_t iDBPortNum=0);
	KSQL (KSQL& Another,  int iDebugID=0);
	KSQL (KSQL* pAnother, int iDebugID=0);

	virtual ~KSQL ();

	bool   CopyConnection (KSQL* pAnother);
	bool   SetConnect (int iDBType, KStringView sUsername, KStringView sPassword, KStringView sDatabase, KStringView sHostname=NULL, uint32_t iDBPortNum=0);
	bool   SetDBType (int iDBType);
	bool   SetDBUser (KStringView sUsername);
	bool   SetDBPass (KStringView sPassword);
	bool   SetDBHost (KStringView sHostname);
	bool   SetDBName (KStringView sDatabase);
	bool   SetDBPort (int iDBPortNum);

	virtual bool   LoadConnect      (KString sDBCFile);
	bool           SaveConnect      (KString sDBCFile);
	int            GetAPISet        ()      { return (m_iAPISet); }
	bool           SetAPISet        (int iAPISet);
	virtual bool   OpenConnection   ();
	virtual bool   OpenConnection   (KStringView sListOfHosts, KStringView sDelimeter = ",");
	void           CloseConnection  ();
	bool           IsConnectionOpen ()      { return (m_bConnectionIsOpen); }

	virtual bool   Insert         (KROW& Row, bool fUnicode=false);
	virtual bool   Update         (KROW& Row, bool fUnicode=false);
	bool           Delete         (KROW& Row, bool fUnicode=false);
	bool           InsertOrUpdate (KROW& Row, bool* pbInserted=NULL, bool fUnicode=false);

	bool   FormInsert     (KROW& Row, KString& sSQL, bool fUnicode=false, bool fIdentityInsert=false)
			{ bool fOK = Row.FormInsert (m_sLastSQL, m_iDBType, fUnicode, fIdentityInsert); sSQL=m_sLastSQL; return (fOK); }
	bool   FormUpdate     (KROW& Row, KString& sSQL, bool fUnicode=false)
			{ bool fOK = Row.FormUpdate (m_sLastSQL, m_iDBType, fUnicode); sSQL=m_sLastSQL; return (fOK); }
	bool   FormDelete     (KROW& Row, KString& sSQL, bool fUnicode=false)
			{ bool fOK = Row.FormDelete (m_sLastSQL, m_iDBType, fUnicode); sSQL=m_sLastSQL; return (fOK); }

	void   SetErrorPrefix   (KStringView sPrefix, uint32_t iLineNum=0);
	void   ClearErrorPrefix ()        { m_sErrorPrefix = "KSQL: "; }

	void   SetWarningThreshold  (uint32_t iWarnIfOverNumSeconds, FILE* fpAlternativeToKlog=NULL)
					{ m_iWarnIfOverNumSeconds = iWarnIfOverNumSeconds;
					  m_bpWarnIfOverNumSeconds = fpAlternativeToKlog; }

	static void     SetDebugLevel (uint32_t iNewLevel) { m_iDebugLevel=iNewLevel; }
	static int32_t  GetDebugLevel () {return m_iDebugLevel;}

	/// After establishing a database connection, this is how you sent DDL (create table, etc.) statements to the RDBMS.
	template<class... Args>
	bool ExecSQL (Args&&... args);

	bool           ExecRawSQL     (KStringView sSQL, uint64_t iFlags=0, KStringView sAPI="ExecRawSQL");
	bool           ExecSQLFile    (KString sFilename);

	/// After establishing a database connection, this is how you issue a SQL query and get results.
	template<class... Args>
	bool ExecQuery (Args&&... args);

	/// This is a special type of version of ExecQuery() in which only one row is fetched and cast to a integer.
	template<class... Args>
	int64_t SingleIntQuery (Args&&... args);

	int64_t        SingleIntRawQuery (KStringView sSQL, uint64_t iFlags=0, KStringView sAPI="ExecRawQuery");

	bool           ExecRawQuery   (KStringView sSQL, uint64_t iFlags=0, KStringView sAPI="ExecRawQuery");
	uint32_t       GetNumCols     ();
	uint32_t       GetNumColumns  ()         { return (GetNumCols());       }
	bool           NextRow        ();
	bool           NextRow        (KROW& Row, bool fTrimRight=true);
	uint64_t       GetNumBuffered ()         { return (m_iNumRowsBuffered); }
	bool           ResetBuffer    ();

	#ifdef ORACLECOMPILE
	// Oracle/OCI variable binding support:
	bool   ParseSQL        (KStringView sFormat, ...);
	bool   ParseRawSQL     (KStringView sSQL, uint64_t iFlags=0, KStringView sAPI="ParseRawSQL");
	bool   ParseQuery      (KStringView sFormat, ...);
	bool   ParseRawQuery   (KStringView sSQL, uint64_t iFlags=0, KStringView sAPI="ParseRawQuery");

	bool   BindByName      (KStringView sPlaceholder, KStringView sValue) { return (BindByName (pszPlaceholder, (char*)pszValue)); };
	bool   BindByName      (KStringView sPlaceholder, char*   pszValue);
	bool   BindByName      (KStringView sPlaceholder, int*    piValue);
	bool   BindByName      (KStringView sPlaceholder, uint32_t*   piValue);
	bool   BindByName      (KStringView sPlaceholder, long*   piValue);
	bool   BindByName      (KStringView sPlaceholder, uint64_t*  piValue);
	bool   BindByName      (KPROPS* pList);

	bool   BindByPos       (uint32_t iPosition, char** ppszValue);
	bool   BindByPos       (uint32_t iPosition, int*   piValue);
	bool   BindByPos       (uint32_t iPosition, uint32_t*  piValue);
	bool   BindByPos       (uint32_t iPosition, long*  piValue);
	bool   BindByPos       (uint32_t iPosition, uint64_t* piValue);

	//OL   ArrayBindByName (KStringView sPlaceholder, char** StringArray, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByName (KStringView sPlaceholder, int*   Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByName (KStringView sPlaceholder, uint32_t*  Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByName (KStringView sPlaceholder, long*  Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByName (KStringView sPlaceholder, uint64_t* Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByName (KStringView sPlaceholder, int64_t* Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByName (KStringView sPlaceholder, uint64_t* Array, uint32_t iNumArray); -- TODO

	//OL   ArrayBindByPos  (uint32_t iPosition, char** StringArray, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByPos  (uint32_t iPosition, int*   Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByPos  (uint32_t iPosition, uint32_t*  Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByPos  (uint32_t iPosition, long*  Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByPos  (uint32_t iPosition, uint64_t* Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByPos  (uint32_t iPosition, int64_t* Array, uint32_t iNumArray); -- TODO
	//OL   ArrayBindByPos  (uint32_t iPosition, uint64_t* Array, uint32_t iNumArray); -- TODO

	bool   ExecParsedSQL   ();
	bool   ExecParsedQuery ();
	#endif

	/// After starting a query, this is the canonical method for fetching results based on column number in the SQL query.
	KStringView  Get         (uint32_t iOneBasedColNum, bool fTrimRight=true);

	COLINFO*     GetColProps (uint32_t iOneBasedColNum);

	/// After starting a query, this converts data in given column to a UNIX time (time_t).  Handles dates like "1965-03-31 12:00:00" and "20010302213436".
	time_t       GetUnixTime (uint32_t iOneBasedColNum);

	#if 0 // example usage
	KSQL sql;
	while (sql.NextRow())
	{
		int         iID   = sql.Get(1).Int32();
		KStringView sName = sql.Col(2).String();
	}

	KSQL sql;
	KROW row;
	while (sql.NextRow(row))
	{
		int         iID   = row.Get("id").Int32();
		KStringView sName = row.Get("name");
	}
	#endif

	int         GetDBType ()             { return (m_iDBType);         }
	KStringView GetDBUser ()             { return (m_sUsername);       }
	KStringView GetDBPass ()             { return (m_sPassword);       }
	KStringView GetDBHost ()             { return (m_sHostname);       }
	KStringView GetDBName ()             { return (m_sDatabase);       }
	uint32_t    GetDBPort ()             { return (m_iDBPortNum);      }
	KStringView ConnectSummary ()        { return (m_sConnectSummary); }
	KStringView GetTempDir()             { return (m_sTempDir);        }

	KStringView GetLastError ()          { return (m_sLastError);      }
	int         GetLastErrorNum ()       { return (m_iErrorNum);       }
	int         GetLastOCIError ()       { return (GetLastErrorNum()); }
	KStringView GetLastSQL ()            { return (m_sLastSQL);        }
	bool        SetFlags (uint64_t iFlags);
	uint64_t     GetFlags ()             { return (m_iFlags);          }
	bool        IsFlag (uint64_t iBit)   { return ((m_iFlags & iBit) == iBit); }
	uint64_t    GetNumRowsAffected ()    { return (m_iNumRowsAffected); }
	uint64_t    GetLastInsertID ();
	KStringView GetLastInfo ();

	void        SetTempDir (KStringView sTempDir)  { m_sTempDir = sTempDir; }

	void        BuildTranslationList (KPROPS* pList, int iDBType=0);
	void        DoTranslations (KString& sSQL, int iDBType=0);
	const char* TxDBType (int iDBType) const;
	const char* TxAPISet (int iAPISet) const;

	#if 0
	// blob and long ascii support:
	unsigned char* EncodeData (unsigned char* pszBlobData, int iBlobType, uint64_t iBlobDataLen=0, bool fInPlace=false);
	unsigned char* DecodeData (unsigned char* pszBlobData, int iBlobType, uint64_t iEncodedLen=0, bool fInPlace=false);
	bool           PutBlob    (KStringView sBlobTable, KStringView sBlobKey, unsigned char* pszBlobData, int iBlobType, uint64_t iBlobDataLen=0);
	unsigned char* GetBlob    (KStringView sBlobTable, KStringView sBlobKey, uint64_t* piBlobDataLen=NULL);
	#endif

	// canned queries:
	bool   ListTables (KStringView sLike="%", bool fIncludeViews=false, bool fRestrictToMine=true);
	                                                  // ORACLE rtns: TableName, 'TABLE'|'VIEW', Owner
	                                                  // MYSQL  rtns: TableName, etc. (see "show table status")
	bool   ListProcedures (KStringView sLike="%", bool fRestrictToMine=true);
	                                                  // MYSQL  rtns: ProcedureName, etc. (see "show procedure status")
	bool   DescribeTable (KStringView sTablename);  // rtns: ColName, Datatype, Null, Key, Default, Extras..

	bool   QueryStarted ()         { return (m_bQueryStarted); }
	void   EndQuery ();

	size_t  OutputQuery     (KStringView sSQL, KStringView sFormat, FILE* fpout=stdout);
	size_t  OutputQuery     (KStringView sSQL, int iFormat=FORM_ASCII, FILE* fpout=stdout);

	void   DisableRetries();
	void   EnableRetries();

	KPROPS  m_TxList;

//----------
private:
//----------
	void   _init (int iDebugID);
	void   ExecSQLFileGo (KStringView sFilename, uint32_t& iLineNumStart, uint32_t& iStatement, uint64_t& iTotalRowsAffected, bool& fOK, bool& fDone, bool& fDropStatement /*, m_sLastSQL*/);
	void   ResetErrorStatus ();

//----------
protected:
//----------
	int64_t    m_iDebugID;                // useful for tracing multiple instances of KSQL in debug log
	uint64_t   m_iFlags;                  // set by calling SetFlags()
	int        m_iErrorNum;               // db error number (e.g. ORA code)
	int        m_iDBType;
	int        m_iAPISet;
	uint32_t   m_iDBPortNum;
	KString    m_sUsername;
	KString    m_sPassword;
	KString    m_sHostname;
	KString    m_sDatabase;
	KString    m_sConnectSummary;
	KString    m_sCursorName;

	static     uint32_t  m_iDebugLevel;

	void*      m_dMYSQL;                   // MYSQL      m_mysql;
	void*      m_MYSQLRow;                 // MYSQL_ROW  m_row;
	void*      m_dMYSQLResult;             // MYSQL_RES* m_presult;

	void*      m_dOCI6LoginDataArea;       // Oracle6&7: (Lda_Def*), kmalloc
	KString    m_sOCI6HostDataArea;        // Oracle6&7: char[256]
	void*      m_dOCI6ConnectionDataArea;  // Oracle6&7: (Cda_Def*), kmalloc

	void*      m_dOCI8EnvHandle;           // Oracle8: OCIEnv*
	void*      m_dOCI8ErrorHandle;         // Oracle8: OCI6Error*
	void*      m_dOCI8ServerHandle;        // Oracle8: OCIServer*
	void*      m_dOCI8ServerContext;       // Oracle8: OCISvcCtx*
	void*      m_dOCI8Session;             // Oracle8: OCISession*
	void*      m_dOCI8Statement;           // Oracle8: OCIStmt*
	int        m_iOCI8FirstRowStat;        // Oracle8: my status var for implied fetch after exec

	uint32_t       m_iOraSessionMode;          // mode arg for OCI8: OCISessionBegin()/OCISessionEnd()
	uint32_t       m_iOraServerMode;           // mode arg for OCI8: OCIServerAttach()/OCIServerDetach()

	DBPROCESS*     m_pDBPROC;              // DBLIB
	CS_CONTEXT*    m_pCtContext;           // CTLIB
	CS_CONNECTION* m_pCtConnection;        // CTLIB
	CS_COMMAND*    m_pCtCommand;           // CTLIB

	// my own struct for column attributes and data coming back from queries:
	COLINFO*   m_dColInfo;                 // kmalloc

	FILE*      m_bpBufferedResults;
	bool       m_bConnectionIsOpen;
	bool       m_bFileIsOpen;
	bool       m_bQueryStarted;
	KString    m_sDBCFile;
	KString    m_sLastSQL;
	KString    m_sLastError;
	KString    m_sTmpResultsFile;
	uint64_t   m_iRowNum;
	uint32_t   m_iNumColumns;
	uint64_t   m_iNumRowsBuffered;
	uint64_t   m_iNumRowsAffected;
	uint64_t   m_iLastInsertID;
	char**     m_dBufferedColArray;
	uint32_t   m_iCursor; // CTLIB
	KString    m_sErrorPrefix;
	int        m_bDisableRetries;
	uint32_t   m_iWarnIfOverNumSeconds;
	FILE*      m_bpWarnIfOverNumSeconds;
	KString    m_sTempDir;

	#ifdef ORACLECOMPILE
	enum       {MAXBINDVARS = 100};
	bool       m_bStatementParsed;
	uint32_t   m_iMaxBindVars;
	uint32_t   m_idxBindVar;
	OCIBind*   m_OCIBindArray[MAXBINDVARS+1];
	#endif

	#ifdef ODBC
	enum       {MAX_ODBCSTR = 300};
	HENV       m_Environment;
	HDBC       m_hdbc;
	HSTMT      m_hstmt;
	KString    m_sConnectString;
	KString    m_sConnectOutput;

	bool  CheckODBC (RETCODE iRetCode);
	#endif

	bool  SQLError (bool fForceError=false);
	bool  WasOCICallOK (KStringView sContext);
	bool  BufferResults ();
	void  FreeAll ();
	void  FreeBufferedColArray (bool fValuesOnly=false);
	void  FormatConnectSummary ();
	bool  PreparedToRetry ();

	#ifdef ORACLECOMPILE
	bool _BindByName      (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType);
	bool _BindByPos       (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType);
	//OL _ArrayBindByName (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType); - TODO
	//OL _ArrayBindByPos  (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType); - TODO
	#endif

	bool ctlib_login           ();
	bool ctlib_logout          ();
	bool ctlib_execsql         (KStringView sSQL);
	bool ctlib_nextrow         ();
	bool ctlib_api_error       (KStringView sContext);
	uint32_t ctlib_check_errors    ();
	bool ctlib_clear_errors    ();
	bool ctlib_prepare_results ();
	void ctlib_flush_results   ();

	bool DecodeDBCData(unsigned char *pszBuffer, long iNumRead, KStringView sDBCFile);
	bool EncodeDBCData(DBCFILEv2& dbc);
	bool EncodeDBCData(DBCFILEv3& dbc);

	unsigned char* encrypt (unsigned char* string) const;
	unsigned char* decrypt (unsigned char* string) const;

}; // KSQL

} // namespace dekaf2
