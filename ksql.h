/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file ksql.h
/// dekaf2's main SQL abstraction KSQL

#include <cinttypes>
#include <vector>
#include "kstring.h"
#include "kstringview.h"
#include "krow.h"
#include "kjson.h"

//
// Note:
//  To use the KSQL class for ORACLE and MYSQL you do *NOT* need to
//  define any special compile-time constants.  These constants
//  are provided in the outside chance that you have the correct
//  header files and want access to extra database features which
//  the KSQL class does not provide.
//
//  By putting "#define DEKAF2_HAS_ORACLE" before the "#include ksql.h"
//  you will need to satisfy the compiler for the oracle header files 
//  below (they are in various directories under ORACLE_HOME).
//
//  By putting "#define DEKAF2_HAS_MYSQL" before the "#include ksql.h"
//  you will need to satisfy the compiler for the mysql header files 
//  below (they come with libmysqlclient.a).
//

#ifdef DEKAF2_HAS_ORACLE
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

  enum {
	PARSE_NO_DEFER   = 0,     // 
	PARSE_V7_LNG     = 2,     // 
	VAR_NOT_IN_LIST  = 1007,  // oracle constants: placed here so that we can compile w/out oracle
	STRING_TYPE      = 5,     // 
	NO_DATA_FOUND    = 1403   // 
  };
#endif

#ifdef DEKAF2_HAS_ODBC
  #include <sqlext.h>         // <-- Microsoft ODBC API's
#endif

#ifdef DEKAF2_HAS_DBLIB
// to get DBLIB member vars to compile:
struct tds_dblib_dbprocess;  typedef struct tds_dblib_dbprocess DBPROCESS;
#endif

#ifdef DEKAF2_HAS_CTLIB
// to get CTLIB member vars to compile:
struct _cs_config;           typedef struct _cs_config CS_CONFIG;
struct _cs_context;          typedef struct _cs_context CS_CONTEXT;
struct _cs_connection;       typedef struct _cs_connection CS_CONNECTION;
struct _cs_locale;           typedef struct _cs_locale CS_LOCALE;
struct _cs_command;          typedef struct _cs_command CS_COMMAND;
struct _cs_blk_row;          typedef struct _cs_blk_row CS_BLK_ROW;
#endif

#ifdef DEKAF2_HAS_MYSQL
// forward declare MYSQL types
typedef struct st_mysql MYSQL;
typedef char** MYSQL_ROW;
typedef struct st_mysql_res MYSQL_RES;
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// KSQL: DATABASE CONNECTION CLASS
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

namespace dekaf2 {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Normalizing date/time columns into format like "20010723211321"...
//
//   MySQL:  no conversion necessary
//   ORACLE: select to_char(colname,'YYYYMMDDHH24MISS')
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSQL : public detail::KCommonSQLBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// KSQL processing flags
	using Flags  = uint16_t;
	/// value translations
	using TXList = KProps <KString, KString, /*order-matters=*/true, /*unique-keys*/true>;

	enum OutputFormat
	{
		FORM_ASCII            = 'a',         // <-- for OutputQuery() method
		FORM_CSV              = 'c',
		FORM_HTML             = 'h'
	};

	enum BlobType
	{
		// blob encoding schemes:
		BT_ASCII              = 'A',        // ASCII:  only replace newlines and single quotes
		BT_BINARY             = 'B',        // BINARY: encode every char into base 256 (2 digit hex)
	};

	enum
	{
		MAX_BLOBCHUNKSIZE     =  2000,      // LCD between Oracle, Sybase, MySQL, and Informix
//		MAXLEN_CURSORNAME     =    50,
		NUM_RETRIES           =     5,      // <-- when db connection is lost
		MAX_CHARS_CTLIB       = 8000,       //varchar columns hold at most 8000 characters.

		F_IgnoreSQLErrors     = 1 << 0,      // <-- only effects the WarningLog
		F_BufferResults       = 1 << 1,      // <-- for use with ResetBuffer()
		F_NoAutoCommit        = 1 << 2,      // <-- only used by Oracle
		F_NoTranslations      = 1 << 3,      // <-- turn off {{token}} translations in SQL
		F_IgnoreSelectKeyword = 1 << 4,      // <-- override check in ExecQuery() for "select..."
		F_NoKlogDebug         = 1 << 5,      // <-- quietly: do not output the customary klog debug statements
		F_AutoReset           = 1 << 6,      // <-- For ctlib, refresh the connection to the server for each query

		FAC_NORMAL            = 1 << 0,      /// FAC_NORMAL: handles empty string, single string and comma-delimed strings
		FAC_NUMERIC           = 1 << 1,      /// FAC_NUMERIC: handles empty string, single number and comma-delimed numbers
		FAC_SUBSELECT         = 1 << 2,      /// FAC_SUBSELECT: se code examples
		FAC_BETWEEN           = 1 << 3,      /// FAC_BETWEEN: handles empty string, single number and number range with a dash
		FAC_LIKE              = 1 << 4       /// FAC_LIKE: use LIKE operator instead of EQUALS
	};

	const char* BAR = "--------------------------------------------------------------------------------"; // for printf() so keep this const char*

	KSQL (DBT iDBType = DBT::MYSQL, KStringView sUsername = {}, KStringView sPassword = {}, KStringView sDatabase = {}, KStringView sHostname = {}, uint16_t iDBPortNum = 0);
	KSQL (KSQL& other);

	~KSQL ();

	bool   SetConnect (DBT iDBType, KStringView sUsername, KStringView sPassword, KStringView sDatabase, KStringView sHostname = {}, uint16_t iDBPortNum = 0);
	bool   SetDBType (DBT iDBType);
	bool   SetDBType (KStringView sDBType);
	bool   SetDBUser (KStringView sUsername);
	bool   SetDBPass (KStringView sPassword);
	bool   SetDBHost (KStringView sHostname);
	bool   SetDBName (KStringView sDatabase);
	bool   SetDBPort (int iDBPortNum);

	bool   LoadConnect      (KString sDBCFile);
	bool   SaveConnect      (KString sDBCFile);
	API    GetAPISet        ()      { return (m_iAPISet); }
	bool   SetAPISet        (API iAPISet);
	bool   OpenConnection   ();
	bool   OpenConnection   (KStringView sListOfHosts, KStringView sDelimiter = ",");
	void   CloseConnection  (bool bDestructor=false);
	bool   IsConnectionOpen ()      { return (m_bConnectionIsOpen); }

	bool   Insert         (KROW& Row);
	bool   Update         (KROW& Row);
	bool   Delete         (KROW& Row);
	bool   UpdateOrInsert (KROW& Row, KROW& AdditionalInsertCols, bool* pbInserted = nullptr);

	bool   FormInsert     (KROW& Row, KString& sSQL, bool fIdentityInsert=false)
			{ bool fOK = Row.FormInsert (m_sLastSQL, m_iDBType, fIdentityInsert); sSQL = m_sLastSQL; return (fOK); }
	bool   FormUpdate     (KROW& Row, KString& sSQL)
			{ bool fOK = Row.FormUpdate (m_sLastSQL, m_iDBType); sSQL = m_sLastSQL; return (fOK); }
	bool   FormDelete     (KROW& Row, KString& sSQL)
			{ bool fOK = Row.FormDelete (m_sLastSQL, m_iDBType); sSQL = m_sLastSQL; return (fOK); }

	void   SetErrorPrefix   (KStringView sPrefix, uint32_t iLineNum = 0);
	void   ClearErrorPrefix ()        { m_sErrorPrefix = "KSQL: "; }

	void   SetWarningThreshold  (time_t iWarnIfOverNumSeconds, FILE* fpAlternativeToKlog=nullptr)
					{ m_iWarnIfOverNumSeconds = iWarnIfOverNumSeconds;
					  m_bpWarnIfOverNumSeconds = fpAlternativeToKlog; }

	/// After establishing a database connection, this is how you sent DDL (create table, etc.) statements to the RDBMS.
	template<class... Args>
	bool ExecSQL (Args&&... args)
	{
		m_sLastSQL = kPrintf(std::forward<Args>(args)...);

		if (!IsFlag(F_NoTranslations)) {
			DoTranslations (m_sLastSQL, m_iDBType);
		}

		bool bOK = ExecRawSQL (m_sLastSQL, 0, "ExecSQL");
		kDebugLog (GetDebugLevel(), "{} rows affected.", m_iNumRowsAffected);
		return (bOK);

	} // KSQL::ExecSQL

	bool ExecRawSQL  (KStringView sSQL, Flags iFlags = 0, KStringView sAPI="ExecRawSQL");
	bool ExecSQLFile (KStringViewZ sFilename);

	/// After establishing a database connection, this is how you issue a SQL query and get results.
	template<class... Args>
	bool ExecQuery (Args&&... args)
	{
		kDebugLog (3, "KSQL::ExecQuery()...");

		EndQuery ();
		if (!OpenConnection())
			return (false);

		m_sLastSQL = kPrintf(std::forward<Args>(args)...);

		if (!IsFlag(F_NoTranslations)) {
			DoTranslations (m_sLastSQL, m_iDBType);
		}

		if (!IsFlag(F_IgnoreSelectKeyword) && !m_sLastSQL.starts_with ("select") && !m_sLastSQL.starts_with("SELECT"))
		{
			m_sLastError.Format ("{}ExecQuery: query does not start with keyword 'select' [see F_IgnoreSelectKeyword]", m_sErrorPrefix);
			return (SQLError());
		}

		return (ExecRawQuery (m_sLastSQL, 0, "ExecQuery"));

	} // ExecQuery


	/// This is a special type of version of ExecQuery() in which only one row is fetched and cast to a integer.
	template<class... Args>
	int64_t SingleIntQuery (Args&&... args)
	{
		kDebugLog (3, "KSQL::SingleIntQuery()...");

		m_sLastSQL = kPrintf(std::forward<Args>(args)...);

		if (!IsFlag(F_NoTranslations))
		{
			DoTranslations (m_sLastSQL, m_iDBType);
		}

		return (SingleIntRawQuery (m_sLastSQL, 0, "SingleIntQuery"));

	} // KSQL::SingleIntQuery

	int64_t        SingleIntRawQuery (KStringView sSQL, Flags iFlags=0, KStringView sAPI = "ExecRawQuery");

	bool           ExecRawQuery   (KStringView sSQL, Flags iFlags=0, KStringView sAPI = "ExecRawQuery");
	KROW::Index    GetNumCols     ();
	KROW::Index    GetNumColumns  ()         { return (GetNumCols());       }
	bool           NextRow        ();
	bool           NextRow        (KROW& Row, bool fTrimRight=true);
	uint64_t       GetNumBuffered ()         { return (m_iNumRowsBuffered); }
	bool           ResetBuffer    ();

    #ifdef DEKAF2_HAS_ORACLE
	// Oracle/OCI variable binding support:
	bool   ParseSQL        (KStringView sFormat, ...);
	bool   ParseRawSQL     (KStringView sSQL, uint64_t iFlags=0, KStringView sAPI = "ParseRawSQL");
	bool   ParseQuery      (KStringView sFormat, ...);
	bool   ParseRawQuery   (KStringView sSQL, uint64_t iFlags=0, KStringView sAPI = "ParseRawQuery");

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
	KStringView  Get         (KROW::Index iOneBasedColNum, bool fTrimRight=true);

	/// After starting a query, this converts data in given column to a UNIX time (time_t).  Handles dates like "1965-03-31 12:00:00" and "20010302213436".
	time_t       GetUnixTime (KROW::Index iOneBasedColNum);

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

	DBT         GetDBType ()             { return (m_iDBType);         }
	const KString& GetDBUser ()          { return (m_sUsername);       }
	const KString& GetDBPass ()          { return (m_sPassword);       }
	const KString& GetDBHost ()          { return (m_sHostname);       }
	const KString& GetDBName ()          { return (m_sDatabase);       }
	uint16_t    GetDBPort ()             { return (m_iDBPortNum);      }
	const KString& ConnectSummary ()     { return (m_sConnectSummary); }
	const KString& GetTempDir()          { return (m_sTempDir);        }

	const KString& GetLastError ()       { return (m_sLastError);      }
	int         GetLastErrorNum ()       { return (m_iErrorNum);       }
	bool        WasDuplicateError()      { return (GetLastErrorNum() == 1062); /*TODO:MySQL only*/ }
	int         GetLastOCIError ()       { return (GetLastErrorNum()); }
	const KString& GetLastSQL ()         { return (m_sLastSQL);        }
	Flags       SetFlags (Flags iFlags);
	Flags       GetFlags ()              { return (m_iFlags);          }
	bool        IsFlag (Flags iBit)      { return ((m_iFlags & iBit) == iBit); }
	uint64_t    GetNumRowsAffected ()    { return (m_iNumRowsAffected); }
	uint64_t    GetLastInsertID ();
	KString     GetLastInfo ();

	void        SetTempDir (KStringView sTempDir)  { m_sTempDir = sTempDir; }

	void        BuildTranslationList (TXList& pList, DBT iDBType = DBT::NONE);
	void        DoTranslations (KString& sSQL, DBT iDBType = DBT::NONE);
	KStringView TxDBType (DBT iDBType) const;
	KStringView TxAPISet (API iAPISet) const;

	#if 0
	// blob and long ascii support:
	unsigned char* EncodeData (unsigned char* pszBlobData, BlobType iBlobType, uint64_t iBlobDataLen = 0, bool fInPlace = false);
	unsigned char* DecodeData (unsigned char* pszBlobData, BlobType iBlobType, uint64_t iEncodedLen = 0, bool fInPlace = false);
	bool           PutBlob    (KStringView sBlobTable, KStringView sBlobKey, unsigned char* pszBlobData, BlobType iBlobType, uint64_t iBlobDataLen = 0);
	unsigned char* GetBlob    (KStringView sBlobTable, KStringView sBlobKey, uint64_t* piBlobDataLen = nullptr);
	#endif

	// canned queries:
	bool   ListTables (KStringView sLike = "%", bool fIncludeViews = false, bool fRestrictToMine = true);
	                                                  // ORACLE rtns: TableName, 'TABLE'|'VIEW', Owner
	                                                  // MYSQL  rtns: TableName, etc. (see "show table status")
	bool   ListProcedures (KStringView sLike = "%", bool fRestrictToMine = true);
	                                                  // MYSQL  rtns: ProcedureName, etc. (see "show procedure status")
	/// search data dictionary and describe a table definition.
	/// returns an open query with: ColName, Datatype, Null, Key, Default, Extras..
	bool   DescribeTable (KStringView sTablename);

	/// search data dictionary and find tables with the given column_name (or columns LIKE the given column_name if you include percent signs).
	/// returns a JSON array: table_name, column_name, column_key, column_comment, is_nullable, column_default
	/// empty array means column not found
	KJSON  FindColumn    (KStringView sColLike);

	bool   QueryStarted ()         { return (m_bQueryStarted); }
	void   EndQuery (bool bDestructor=false);

	size_t  OutputQuery     (KStringView sSQL, KStringView sFormat, FILE* fpout = stdout);
	size_t  OutputQuery     (KStringView sSQL, OutputFormat iFormat = FORM_ASCII, FILE* fpout = stdout);

	void   DisableRetries() { m_bDisableRetries = true;  }
	void   EnableRetries()  { m_bDisableRetries = false; }

	bool   BeginTransaction (KStringView sOptions="");
	bool   CommitTransaction (KStringView sOptions="");

	/// helper method to form AND clauses for dynamic SQL.
	KString FormAndClause (KStringView sDbCol, KString/*copy*/ sQueryParm, uint64_t iFlags=FAC_NORMAL);

	TXList  m_TxList;

//----------
private:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// keeps column information
	class KColInfo
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		KColInfo() = default;

		void SetColumnType(DBT iDBType, int _iNativeDataType, KCOL::Len _iMaxDataLen);

#if defined(DEKAF2_HAS_ORACLE) || defined(DEKAF2_HAS_CTLIB) || defined(DEKAF2_HAS_DBLIB)
		std::unique_ptr<char> dszValue;
#endif
		KString  sColName;
		KCOL::Flags iKSQLDataType   { 0 };
		KCOL::Len   iMaxDataLen     { 0 };
#if defined(DEKAF2_HAS_ORACLE) || defined(DEKAF2_HAS_CTLIB)
		int16_t     indp            { 0 };   // oratypes.h:typedef   signed short    sb2;
#endif

	}; // KColInfo

	using KColInfos = std::vector<KColInfo>;

	struct SQLFileParms
	{
		bool      fDone              { false };
		bool      fOK                { true  };
		bool      fDropStatement     { false };
		uint64_t  iTotalRowsAffected { 0     };
		uint32_t  iLineNum           { 0     };
		uint32_t  iLineNumStart      { 0     };
		uint32_t  iStatement         { 0     };

	}; // SQLFileParms

	void   ExecSQLFileGo (KStringView sFilename, SQLFileParms& Parms);
	void   ResetErrorStatus ();
	const  KColInfo& GetColProps (KROW::Index iOneBasedColNum);

	// my own struct for column attributes and data coming back from queries:
	KColInfos  m_dColInfo;

//----------
protected:
//----------

	Flags      m_iFlags { 0 };                  // set by calling SetFlags()
	int        m_iErrorNum { 0 };               // db error number (e.g. ORA code)
#if defined(DEKAF2_HAS_MYSQL)
	DBT        m_iDBType { DBT::MYSQL };
	API        m_iAPISet { API::MYSQL };
#elif defined(DEKAF2_HAS_SQLITE3)
	DBT        m_iDBType { DBT::SQLITE3 };
	API        m_iAPISet { API::SQLITE3 };
#else
	DBT        m_iDBType { DBT::NONE };
	API        m_iAPISet { API::NONE };
#endif
	uint16_t   m_iDBPortNum { 0 };
	KString    m_sUsername;
	KString    m_sPassword;
	KString    m_sHostname;
	KString    m_sDatabase;
	KString    m_sConnectSummary;

	static     uint16_t  m_iDebugLevel;

#ifdef DEKAF2_HAS_MYSQL
	MYSQL*     m_dMYSQL { nullptr };                   // MYSQL      m_mysql;
	MYSQL_ROW  m_MYSQLRow { nullptr };                 // MYSQL_ROW  m_row;
	MYSQL_RES* m_dMYSQLResult { nullptr };             // MYSQL_RES* m_presult;
#endif

#ifdef DEKAF2_HAS_ORACLE
	void*      m_dOCI6LoginDataArea { nullptr };       // Oracle6&7: (Lda_Def*), kmalloc
	KString    m_sOCI6HostDataArea;        // Oracle6&7: char[256]
	void*      m_dOCI6ConnectionDataArea { nullptr };  // Oracle6&7: (Cda_Def*), kmalloc

	void*      m_dOCI8EnvHandle { nullptr };           // Oracle8: OCIEnv*
	void*      m_dOCI8ErrorHandle { nullptr };         // Oracle8: OCI6Error*
	void*      m_dOCI8ServerHandle { nullptr };        // Oracle8: OCIServer*
	void*      m_dOCI8ServerContext { nullptr };       // Oracle8: OCISvcCtx*
	void*      m_dOCI8Session { nullptr };             // Oracle8: OCISession*
	void*      m_dOCI8Statemen { nullptr }t;           // Oracle8: OCIStmt*
	int        m_iOCI8FirstRowStat { 0 };        // Oracle8: my status var for implied fetch after exec

	uint32_t   m_iOraSessionMode { 0 };          // mode arg for OCI8: OCISessionBegin()/OCISessionEnd()
	uint32_t   m_iOraServerMode { 0 };           // mode arg for OCI8: OCIServerAttach()/OCIServerDetach()

	enum       { MAXBINDVARS = 100 };
	bool       m_bStatementParsed { false };
	uint32_t   m_iMaxBindVars { 0 };
	uint32_t   m_idxBindVar { 0 };
	OCIBind*   m_OCIBindArray[MAXBINDVARS+1];
#endif

#ifdef DEKAF2_HAS_DBLIB
	DBPROCESS*     m_pDBPROC { nullptr };              // DBLIB
#endif

#ifdef DEKAF2_HAS_CTLIB
	CS_CONTEXT*    m_pCtContext { nullptr };           // CTLIB
	CS_CONNECTION* m_pCtConnection { nullptr };        // CTLIB
	CS_COMMAND*    m_pCtCommand { nullptr };           // CTLIB
	KString        m_sCursorName;
	uint32_t       m_iCursor { 0 }; // CTLIB
#endif

#ifdef DEKAF2_HAS_ODBC
	enum       { MAX_ODBCSTR = 300 };
	HENV       m_Environment { nullptr };
	HDBC       m_hdbc { SQL_NULL_HSTMT };
	HSTMT      m_hstmt { nullptr };
	KString    m_sConnectString;
	KString    m_sConnectOutput;

	bool  CheckODBC (RETCODE iRetCode);
#endif

	FILE*      m_bpBufferedResults { nullptr };
	bool       m_bConnectionIsOpen { false };
	bool       m_bFileIsOpen { false };
	bool       m_bQueryStarted { false };
	KString    m_sDBCFile;
	KString    m_sLastSQL;
	KString    m_sLastError;
	KString    m_sTmpResultsFile;
	uint64_t   m_iRowNum { 0 };
	KROW::Index m_iNumColumns { 0 };
	uint64_t   m_iNumRowsBuffered { 0 };
	uint64_t   m_iNumRowsAffected { 0 };
	uint64_t   m_iLastInsertID { 0 };
	char**     m_dBufferedColArray { nullptr };
	KString    m_sErrorPrefix { "KSQL: " };
	bool       m_bDisableRetries { false };
	time_t     m_iWarnIfOverNumSeconds { 0 };
	FILE*      m_bpWarnIfOverNumSeconds { nullptr };
	KString    m_sTempDir { "/tmp" };

	bool  SQLError (bool fForceError=false);
	bool  WasOCICallOK (KStringView sContext);
	bool  BufferResults ();
	void  FreeAll (bool bDestructor=false);
	void  FreeBufferedColArray (bool fValuesOnly=false);
	void  FormatConnectSummary ();
	bool  PreparedToRetry ();

    #ifdef DEKAF2_HAS_ORACLE
	bool _BindByName      (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType);
	bool _BindByPos       (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType);
	//OL _ArrayBindByName (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType); - TODO
	//OL _ArrayBindByPos  (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType); - TODO
	#endif

	#ifdef DEKAF2_HAS_CTLIB
	bool ctlib_login           ();
	bool ctlib_logout          ();
	bool ctlib_execsql         (KStringView sSQL);
	bool ctlib_nextrow         ();
	bool ctlib_api_error       (KStringView sContext);
	uint32_t ctlib_check_errors ();
	bool ctlib_clear_errors    ();
	bool ctlib_prepare_results ();
	void ctlib_flush_results   ();
	#endif

	bool DecodeDBCData(KStringView sBuffer, KStringView sDBCFile);

}; // KSQL

} // namespace dekaf2
