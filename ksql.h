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

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "krow.h"
#include "kjson.h"
#include "kcache.h"
#include "kexception.h"
#include "ksystem.h"
#include "kthreadsafe.h"
#include "kreference_proxy.h"
#include "kassociative.h"
#include "kscopeguard.h"
#include "kduration.h"
#include "ktemplate.h"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <cinttypes>
#include <vector>
#include <tuple>
#include <type_traits>
#include <set>
#include <thread>

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
#ifdef DEKAF2_MYSQL_IS_MARIADB
typedef struct st_mysql MYSQL;
typedef struct st_mysql_res MYSQL_RES;
#else
typedef struct MYSQL MYSQL;
typedef struct MYSQL_RES MYSQL_RES;
#endif
typedef char** MYSQL_ROW;
typedef unsigned long* MYSQL_ROW_LENS; // this one is not a MYSQL type, but it is returned from mysql_fetch_lengths()
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// KSQL: DATABASE CONNECTION CLASS
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

DEKAF2_NAMESPACE_BEGIN

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Normalizing date/time columns into format like "20010723211321"...
//
//   MySQL:  no conversion necessary
//   ORACLE: select to_char(colname,'YYYYMMDDHH24MISS')
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Dekaf2's general purpose database generalization. Supports MySQL/MariaDB
/// and MS SQLServer / Sybase
class DEKAF2_PUBLIC KSQL : public detail::KCommonSQLBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	// for mysql the ConnectionID is actually only an unsigned long, but we
	// do not know about other DBTypes
	using ConnectionID = std::size_t;

	/// property sheet containing configuration parameters
	using IniParms = KProps<KString, KString, false, true>;

	/// KSQL exception class
	struct Exception : public KException
	{
		using KException::KException;
	};

	/// value translations
	using TXList = KProps <KString, KString, /*order-matters=*/false, /*unique-keys*/true>;

	enum OutputFormat
	{
		FORM_ASCII            = 'a',        ///< for OutputQuery() method
		FORM_CSV              = 'c',
		FORM_HTML             = 'h'
	};

	enum BlobType
	{
		// blob encoding schemes:
		BT_ASCII              = 'A',        ///< ASCII:  only replace newlines and single quotes
		BT_BINARY             = 'B',        ///< BINARY: encode every char into base 256 (2 digit hex)
	};

	enum
	{
		MAX_BLOBCHUNKSIZE     =  2000,       ///< LCD between Oracle, Sybase, MySQL, and Informix
//		MAXLEN_CURSORNAME     =    50,
		NUM_RETRIES           =     5,       ///< when db connection is lost
		MAX_CHARS_CTLIB       =  8000,       ///< varchar columns hold at most 8000 characters.
	};

	/// KSQL processing flags
	enum Flags : uint8_t // should better be an enum class Flags, with removed prefix on the values
	{
		F_None                = 0,           ///< no flags
		F_IgnoreSQLErrors     = 1 << 0,      ///< only effects the WarningLog
		F_BufferResults       = 1 << 1,      ///< for use with ResetBuffer()
		F_NoAutoCommit        = 1 << 2,      ///< only used by Oracle
		F_NoTranslations      = 1 << 3,      ///< turn off {{token}} translations in SQL
		F_IgnoreSelectKeyword = 1 << 4,      ///< override check in ExecQuery() for "select..."
		F_NoKlogDebug         = 1 << 5,      ///< quietly: do not output the customary klog debug statements
		F_AutoReset           = 1 << 6,      ///< For ctlib: refresh the connection to the server for each query
		F_ReadOnlyMode        = 1 << 7       ///< If connection should disallow INSERT, UPDATE, DELETE and only allow SELECT
	};

	enum FAC : uint16_t // should better be an enum class FAC, with removed prefix on the values
	{
		FAC_NONE              = 0,           ///< FAC_NONE: no flags
		FAC_NORMAL            = 1 << 0,      ///< FAC_NORMAL: handles empty string, single string and comma-delimed strings
		FAC_NUMERIC           = 1 << 1,      ///< FAC_NUMERIC: handles empty string, single number and comma-delimed numbers
		FAC_SUBSELECT         = 1 << 2,      ///< FAC_SUBSELECT: se code examples
		FAC_BETWEEN           = 1 << 3,      ///< FAC_BETWEEN: handles empty string, single number and number range with a dash
		FAC_LIKE              = 1 << 4,      ///< FAC_LIKE: use LIKE operator instead of EQUALS
		FAC_TEXT_CONTAINS     = 1 << 5,      ///< FAC_TEXT_CONTAINS: full-text search using SQL tolower and like operator
		FAC_TIME_PERIODS      = 1 << 6,      ///< FAC_TIME_PERIODS: time intervals >= 'hour', 'day', 'week', 'month' or 'year'
		FAC_DECIMAL           = 1 << 7,      ///< FAC_DECIMAL: like FAC_NUMERIC but expects float/double
		FAC_SIGNED            = 1 << 8       ///< FAC_SIGNED: like FAC_NUMERIC but retains +/- sign
	};

	/// default constructor
	KSQL ();
	/// constructor to configure connection details
	/// @param iDBType the DBType
	/// @param sUsername the user name
	/// @param sPassword the password
	/// @param sDatabase the database to use
	/// @param sHostname the database server hostname
	/// @param iDBPortNum the database server port number
	KSQL (DBT iDBType, KStringView sUsername = {}, KStringView sPassword = {}, KStringView sDatabase = {}, KStringView sHostname = {}, uint16_t iDBPortNum = 0);
	/// construct using DBC file.
	/// This connector immediately also opens the connection.
	/// @param sDBCFile dbc file to be used
	KSQL (KStringView sDBCFile);
	/// construct using DBC file, env variables, INI parms with connection details.
	/// This connector is the only one that immediately also opens the connection.
	/// @param sIdentifierList name used as prefix for env vars to
	/// load the configuration from.
	/// @param sDBCFile dbc file to be used. Can be empty.
	/// @param INI a property sheet with ini parameters which
	/// will be searched as last resort to find connection parameters
	KSQL (KStringView sIdentifierList,
	      KStringView sDBCFile,
	      const IniParms& INI = IniParms{});
	/// copy constructor, only copies connection details from other instance, not internal state!
	KSQL (const KSQL& other);
	/// move constructor, moves all internal state
	KSQL (KSQL&&) = default;

	~KSQL ();

	bool operator== (const KSQL& other) const { return (GetHash() == other.GetHash()); }
	bool operator!= (const KSQL& other) const { return (GetHash() != other.GetHash()); }

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// KSQL iterator over result rows, use it for auto range for loops like
	/// for (auto& row : ksql) after issuing a query statement
	class iterator
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:

		iterator(KSQL* pdb = nullptr)
		: m_pdb(pdb)
		{}

		KROW* operator->() { return &m_row; }
		KROW& operator*()  { return m_row;  }

		iterator& operator++() { if (m_pdb && !m_pdb->NextRow(m_row)) m_pdb = nullptr; return *this; }
		bool operator==(const iterator& other) const { return m_pdb == other.m_pdb; }
		bool operator!=(const iterator& other) const { return !operator==(other);   }

	private:

		KSQL* m_pdb { nullptr };
		KROW  m_row;

	}; // iterator

	/// return begin of result rows
	iterator begin() { return ++iterator(this); }
	/// return end of result rows
	iterator end()   { return iterator();       }

	/// set all connection parameters
	/// @param iDBType the DBType
	/// @param sUsername the user name
	/// @param sPassword the password
	/// @param sDatabase the database to use
	/// @param sHostname the database server hostname
	/// @param iDBPortNum the database server port number
	/// @return always returns true
	bool   SetConnect (DBT iDBType, KStringView sUsername, KStringView sPassword, KStringView sDatabase, KStringView sHostname = {}, uint16_t iDBPortNum = 0);
	/// copy connection parameters from another KSQL instance
	/// @return always returns true
	bool   SetConnect (KSQL& Other);

	/// configure connection database server type by numeric DBT value
	bool   SetDBType (DBT iDBType);
	/// configure connection database server type by string (name)
	bool   SetDBType (KStringView sDBType);
	/// configure connection user name
	bool   SetDBUser (KStringView sUsername);
	/// configure connection password
	bool   SetDBPass (KStringView sPassword);
	/// configure server hostname to connect to
	bool   SetDBHost (KStringView sHostname);
	/// configure database name to use
	bool   SetDBName (KStringView sDatabase);
	/// configure server port to connect to
	bool   SetDBPort (int iDBPortNum);

	/// load connection parameters from a DBC file
	bool   LoadConnect      (KStringViewZ sDBCFile);
	/// save connection parameters into a DBC file
	bool   SaveConnect      (KStringViewZ sDBCFile);
	bool   SetConnect       (KStringViewZ sDBCFile, KStringView sDBCFileContent);
	API    GetAPISet        ()      { return (m_iAPISet); }
	bool   SetAPISet        (API iAPISet);
	/// Open a server connection with the configured connection parameters
	bool   OpenConnection   (uint16_t iConnectTimeoutSecs = 0);
	/// Open a server connection to the first responding host in a (comma separated) list.
	/// Reuses all other configured connection parameters for each host.
	/// @return true on success
	bool   OpenConnection   (KStringView sListOfHosts, KStringView sDelimiter = ",", uint16_t iConnectTimeoutSecs = 0);
	/// Close an open database server connection
	void   CloseConnection  (bool bDestructor=false);
	/// returns true on an open server connection
	bool   IsConnectionOpen ()      { return (m_bConnectionIsOpen); }

	/// Load one KROW object (primary key selects which, can be left out or empty)
	bool   Load            (KROW& Row, bool bSelectAllColumns = false);
	/// Insert one KROW object
	bool   Insert          (const KROW& Row, bool bIgnoreDupes=false);
	/// Insert a vector of KROW objects. It is permitted to add KROW objects for different tables, the
	/// bulk insert automatically ends one insert and starts another. Only the first in a series of KROW
	/// objects needs to have the table name set.
	bool   Insert          (const std::vector<KROW>& Rows, bool bIgnoreDupes=false);
	/// Update with one KROW object (primary key selects which)
	bool   Update          (const KROW& Row);
	/// Delete with one KROW object (primary key selects which)
	bool   Delete          (const KROW& Row);
	/// Update or insert one KROW object (primary key selects which)
	bool   UpdateOrInsert  (const KROW& Row, bool* pbInserted = nullptr);
	/// Update or insert one KROW object (primary key selects which)
	bool   UpdateOrInsert  (KROW& Row, const KROW& AdditionalInsertCols, bool* pbInserted = nullptr);

	/// cascading delete of a given key from the entire database (uses data dictionary tables). returns true/false and populates all changes made in given json array. expects a single string or number (without quotes)
	bool   PurgeKey        (KStringView sSchemaName, KStringView sPKEY_colname, KStringView sValue, KJSON& ChangesMade, KStringView sIgnoreRegex="");

	/// cascading delete of a given key with other composite key[s] from the entire database (uses data dictionary tables). returns true/false and populates all changes made in given json array. expects a single string or number (without quotes)
	bool   PurgeKey        (KStringView sSchemaName, const KROW& OtherKeys, KStringView sPKEY_colname, KStringView sValue, KJSON& ChangesMade, KStringView sIgnoreRegex/*=""*/);

	/// cascading delete of a given key from the entire database (uses data dictionary tables). returns true/false and populates all changes made in given json array. expects IN clause (without the parens)
	bool   PurgeKeyList    (KStringView sSchemaName, KStringView sPKEY_colname, const KSQLString& sInClause, KJSON& ChangesMade, KStringView sIgnoreRegex="", bool bDryRun=false, int64_t* piNumAffected=NULL);

	/// bulk copy a table (or portion) from another database to this one
	/// notes:
	/// @param OtherDB the db to copy from
	/// @param sTablename the table to copy from
	/// @param sWhereClause an optional where clause
	/// @param iFlushRows adjust iFlushRows up or down depending on memory concerns and row size
	/// @param iPbarThreshold if the count(*) is less than this, no pbar will be shown;
	/// use iPbarThreshold=-1 to turn off progress bar entirely and to eliminate the initial count(*) query
	/// after successful operation, GetNumRowsAffected() will be adjusted to the cumulative total
	bool   BulkCopy        (KSQL& OtherDB, KStringView sTablename, const KSQLString& sWhereClause="", uint16_t iFlushRows=1024, int32_t iPbarThreshold=500);

	KSQLString FormInsert (KROW& Row, bool fIdentityInsert=false)
	        { return Row.FormInsert (m_iDBType, fIdentityInsert); }
	KSQLString FormUpdate (KROW& Row)
	        { return Row.FormUpdate (m_iDBType); }
	KSQLString FormDelete (KROW& Row)
	        { return Row.FormDelete (m_iDBType); }
	KSQLString FormSelect (KROW& Row, bool bSelectAllColumns = false)
	        { return Row.FormSelect (m_iDBType, bSelectAllColumns); }

	void   SetErrorPrefix   (KStringView sPrefix, uint32_t iLineNum = 0);
	void   ClearErrorPrefix ()        { m_sErrorPrefix.clear(); }

	/// issue a klog warning everytime a query or sql statement exceeds the given number of seconds
	void   SetWarningThreshold  (KDuration iWarnIfOverMilliseconds, FILE* fpAlternativeToKlog=nullptr)
	{
		m_iWarnIfOverMilliseconds = iWarnIfOverMilliseconds;
		m_fpPerformanceLog        = fpAlternativeToKlog;
	}

	/// call back everytime a query or sql statement exceeds the given duration
	void   SetWarningThreshold  (KDuration iWarnIfOverMilliseconds,
	                             std::function<void(const KSQL&, KDuration, const KString&)> TimingCallback = nullptr)
	{
		m_iWarnIfOverMilliseconds = iWarnIfOverMilliseconds;
		m_TimingCallback = TimingCallback;
	}

	enum class QueryType : uint16_t
	{
		None        = 0,
		Select      = 1 <<  0,   // select, table, values
		Insert      = 1 <<  1,   // insert, import, load
		Update      = 1 <<  2,   // update
		Delete      = 1 <<  3,   // delete, truncate
		Create      = 1 <<  4,   // create
		Alter       = 1 <<  5,   // alter, rename
		Drop        = 1 <<  6,   // drop
		Transaction = 1 <<  7,   // start, begin, commit, rollback
		Execute     = 1 <<  8,   // exec, call, do
		Action      = 1 <<  9,   // kill, use, set, grant, lock, unlock
		Maintenance = 1 << 10,   // analyze, optimize, check, repair
		Info        = 1 << 11,   // desc, explain, show
		Other       = 1 << 12,
		Any         = Select|Insert|Update|Delete|Create|Alter|Drop|Transaction|Execute|Action|Maintenance|Info|Other
	};

	/// returns QueryType of a query string - if bAllowAny == true, the string "any" will select all query types
	static QueryType GetQueryType(KStringView sQuery, bool bAllowAny = false);

	/// returns a string with the name of the single selected query type
	static KStringViewZ SerializeOneQueryType(QueryType OneQueryType);

	/// returns a string with the names of the selected query types
	static std::vector<KStringViewZ> SerializeQueryTypes(QueryType QueryType);

	/// set a timeout and type of query to abort after the timeout expired
	void SetQueryTimeout(std::chrono::milliseconds Timeout, QueryType Queries = QueryType::Any);

	/// set the timeout and type of query to abort after the timeout expired to the default values set with SetDefaultQueryTimeout()
	void ResetQueryTimeout();

	/// set a default timeout and type of query to abort after the timeout expired, will be used by all new instances of KSQL as the initial setting
	static void SetDefaultQueryTimeout(std::chrono::milliseconds Timeout, QueryType Queries = QueryType::Any);

	/// disable an existing query timeout
	void DisableQueryTimeout() { m_bEnableQueryTimeout = false; }

	/// enable an existing query timeout (needs timeout value > 0 and query type != None to be effective)
	void EnableQueryTimeout()  { m_bEnableQueryTimeout = true;  }

//----------
private:
//----------

	bool ExecLastRawSQL (Flags iFlags=Flags::F_None, KStringView sAPI = "ExecLastRawSQL");
	bool ExecLastRawQuery (Flags iFlags=Flags::F_None, KStringView sAPI = "ExecLastRawQuery");
	bool ExecLastRawInsert(bool bIgnoreDupes=false);

	inline
	bool ExecRawSQL  (KSQLString sSQL, Flags iFlags = Flags::F_None, KStringView sAPI="ExecRawSQL")
	{
		m_sLastSQL = std::move(sSQL);
		return ExecLastRawSQL(iFlags, sAPI);
	}

	inline
	bool ExecRawQuery   (KSQLString sSQL, Flags iFlags=Flags::F_None, KStringView sAPI = "ExecRawQuery")
	{
		m_sLastSQL = std::move(sSQL);
		return ExecLastRawQuery(iFlags, sAPI);
	}

	/// Executes a verbatim SQL statement that returns one single integer value or -1 on failure
	/// @param sSQL  a SQL statement that returns one integer column
	/// @param iFlags additional processing flags, default none
	/// @param sAPI  the method name used for logging, default "SingleIntRawQuery"
	int64_t        SingleIntRawQuery (KSQLString sSQL, Flags iFlags=Flags::F_None, KStringView sAPI = "SingleIntRawQuery");

	/// Executes a verbatim SQL statement that returns one single string value or "" on failure
	/// @param sSQL  a SQL statement that returns one string column
	/// @param iFlags additional processing flags, default none
	/// @param sAPI  the method name used for logging, default "SingleStringRawQuery"
	KString        SingleStringRawQuery (KSQLString sSQL, Flags iFlags=Flags::F_None, KStringView sAPI = "SingleStringRawQuery");

	/// Executes a verbatim SQL statement that returns one single KROW
	/// @param sSQL  a SQL statement
	/// @param iFlags additional processing flags, default none
	/// @param sAPI  the method name used for logging, default "SingleRawQuery"
	KROW           SingleRawQuery (KSQLString sSQL, Flags iFlags=Flags::F_None, KStringView sAPI = "SingleRawQuery");

//----------
public:
//----------

	/// After establishing a database connection, this is how you send DDL (create table, etc.) statements to the RDBMS.
	template<class... Args>
	bool ExecSQL (Args&&... args)
	{
		m_sLastSQL = FormatSQL (std::forward<Args>(args)...);
		bool bOK   = ExecLastRawSQL (Flags::F_None, "ExecSQL");
		return (bOK);

	} // KSQL::ExecSQL

	bool ExecSQLFile (KStringViewZ sFilename);

	/// After establishing a database connection, this is how you issue a SQL query and get results.
	template<class... Args>
	bool ExecQuery (Args&&... args)
	{
		m_sLastSQL = FormatSQL (std::forward<Args>(args)...);
		return (ExecLastRawQuery (Flags::F_None, "ExecQuery"));

	} // ExecQuery

	/// Executes an SQL statement with format arguments that returns one KROW
	template<class... Args>
	KROW SingleQuery (Args&&... args)
	{
		return (SingleRawQuery (FormatSQL (std::forward<Args>(args)...), Flags::F_None, "SingleQuery"));

	} // KSQL::SingleQuery

	/// Executes an SQL statement with format arguments that returns one single integer value or -1 on failure
	template<class... Args>
	int64_t SingleIntQuery (Args&&... args)
	{
		return (SingleIntRawQuery (FormatSQL (std::forward<Args>(args)...), Flags::F_None, "SingleIntQuery"));

	} // KSQL::SingleIntQuery

	/// Executes an SQL statement with format arguments that returns one single string value or "" on failure
	template<class... Args>
	KString SingleStringQuery (Args&&... args)
	{
		return (SingleStringRawQuery (FormatSQL (std::forward<Args>(args)...), Flags::F_None, "SingleStringQuery"));

	} // KSQL::SingleStringQuery

	KROW::Index    GetNumCols     ();
	KROW::Index    GetNumColumns  ()         { return (GetNumCols());       }
	bool           NextRow        ();
	bool           NextRow        (KROW& Row, bool fTrimRight=true);
	/// Loads column layout into Row
	/// @param Row a KROW that will receive the column definitions. Existing content in Row will not be erased (but updated).
	/// @param sColumns comma separated list of columns to load, or empty for all columns
	bool           LoadColumnLayout(KROW& Row, KStringView sColumns = KStringView{});
	uint64_t       GetNumBuffered ()         { return (m_iNumRowsBuffered); }
	bool           ResetBuffer    ();

    #ifdef DEKAF2_HAS_ORACLE
	// Oracle/OCI variable binding support:
	bool   ParseSQL        (KStringView sFormat, ...);
	bool   ParseRawSQL     (KStringView sSQL, Flags iFlags=Flags::F_None, KStringView sAPI = "ParseRawSQL");
	bool   ParseQuery      (KStringView sFormat, ...);
	bool   ParseRawQuery   (KStringView sSQL, Flags iFlags=Flags::F_None, KStringView sAPI = "ParseRawQuery");

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

	static constexpr KStringViewZ KSQL_CONNECTION_TEST_ONLY { "KSQL_CONNECTION_TEST_ONLY" };

	static bool IsConnectionTestOnly ();

	/// After starting a query, this is the canonical method for fetching results based on column number in the SQL query.
	KStringView  Get         (KROW::Index iOneBasedColNum, bool fTrimRight=true);

	/// After starting a query, this converts data in given column to a UNIX time (time_t).  Handles dates like "1965-03-31 12:00:00" and "20010302213436".
	KUnixTime    GetUnixTime (KROW::Index iOneBasedColNum);

	/// convert various formats to something that the db can handle
	static KString ConvertTimestamp (KStringView sTimestamp);

	/// helper to see if something is a SELECT statement:
	static bool IsSelect (KStringView sSQL);

	/// helper to see if we are about to attempt a transaction on a read-only connection:
	bool IsReadOnlyViolation (QueryType QueryType);

	/// helper to see if we are about to attempt a transaction on a read-only connection:
	bool IsReadOnlyViolation (KStringView sSQL)
	{
		return IsReadOnlyViolation(GetQueryType(sSQL));
	}

	/// produce a summary of all [matching] tables with row counts (to KOut)
	bool ShowCounts (KStringView sRegex="");

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

	// or easier:

	KSQL sql;
	for (auto& row : sql)
	{
		int         iID   = row["id"].Int32();
		KStringView sName = row["name"];
	}
	#endif

	/// returns configured database type
	DBT            GetDBType ()    const { return (m_iDBType);         }
	/// returns configured connection user
	const KString& GetDBUser ()    const { return (m_sUsername);       }
	/// returns configured connection password
	const KString& GetDBPass ()    const { return (m_sPassword);       }
	/// returns configured DB server host name
	const KString& GetDBHost ()    const { return (m_sHostname);       }
	/// returns configured database name
	const KString& GetDBName ()    const { return (m_sDatabase);       }
	/// returns configured connect to port number
	uint16_t       GetDBPort ()    const { return (m_iDBPortNum);      }
	/// returns hash value over connection details
	uint64_t       GetHash ()      const;
	/// returns connection details as string
	const KString& ConnectSummary () const;
	const KString& GetTempDir()    const { return (m_sTempDir);        }

	/// this tmp file is used to hold buffered results (if flag F_BufferResults is set). Always only
	/// access its name through these functions!
	const KString& GetTempResultsFile();
	void ClearTempResultsFile() { m_sNeverReadMeDirectlyTmpResultsFile.clear(); }
	void RemoveTempResultsFile();

	/// returns last error string
	const KString& GetLastError () const { return (m_sLastErrorSetOnlyWithSetError);      }
	/// returns last error number
	uint32_t    GetLastErrorNum () const { return (m_iErrorSetOnlyWithSetError);       }
	/// returns true for MySQL duplicate index error
	bool        WasDuplicateError() const { return (GetLastErrorNum() == 1062); /*TODO:this is hardcoded for MySQL only*/ }
	int         GetLastOCIError () const { return (GetLastErrorNum()); }
	/// returns last issued SQL statement
	const KString& GetLastSQL ()   const { return (m_sLastSQL.str());        }

	// helper struct to enable ScopedFlags with C++11 already
	struct ResetFlagsCallable
	{
		ResetFlagsCallable(KSQL* db, Flags iFlags) : m_db(db), m_iFlags(iFlags) {}
		void operator()() const noexcept { m_db->SetFlags(m_iFlags); }
		KSQL* m_db; Flags m_iFlags;
	};

	/// set configuration/processing flags, and reset original flags at end of scope
	KScopeGuard<ResetFlagsCallable> ScopedFlags (Flags iFlags, bool bAdditive = true)
	{
		auto iOrigFlags = bAdditive ? SetFlag(iFlags) : SetFlags(iFlags);
		return KScopeGuard<ResetFlagsCallable>(ResetFlagsCallable(this, iOrigFlags));
	}
	/// set configuration/processing flags, returns old flags
	Flags       SetFlags (Flags iFlags);
	/// add new flag(s) to existing configuration/processing flags (logical OR)
	Flags       SetFlag (Flags iFlag);
	/// ensure that the given flag is not set (does not affect any other flags)
	Flags       ClearFlag (Flags iFlag);
	/// clear all flags (put KSQL back to base state)
	Flags       ClearFlags ()  { return SetFlags (Flags::F_None); }
	/// return configuration/processing flags
	Flags       GetFlags ()         const { return (m_iFlags);          }
	/// returns true if given configuration/processing flag(s) are set
	bool        IsFlag (Flags iBit) const { return ((m_iFlags & iBit) == iBit); }
	/// returns number of table rows changed/deleted/added by last SQL statement
	uint64_t    GetNumRowsAffected () const { return (m_iNumRowsAffected); }
	/// returns last inserted ID from autoincrement/identity column
	uint64_t    GetLastInsertID ();
	/// returns status text after insert/update statements (only for MySQL)
	KString     GetLastInfo ();

	/// set temp directory
	void        SetTempDir (KString sTempDir) { m_sTempDir = std::move(sTempDir); }

	void        BuildTranslationList (TXList& pList, DBT iDBType = DBT::NONE);
	void        DoTranslations (KSQLString& sSQL);
	/// returns iDBType as string @param iDBType a DBType
	KStringView TxDBType (DBT iDBType) const;
	/// returns iAPISet as string @param iAPISet an APISet
	KStringView TxAPISet (API iAPISet) const;

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
	/// optional sTablenamePrefix will restrict the results to tables/views matching that LIKE pattern
	KJSON  FindColumn    (KStringView sColLike, KStringView sSchemaName=""/*current dbname*/, KStringView sTableNameLike="%");


	bool   QueryStarted ()         { return (m_bQueryStarted); }
	void   EndQuery (bool bDestructor=false);

	std::size_t OutputQuery  (KStringView sSQL, KStringView sFormat, FILE* fpout = stdout);
	std::size_t OutputQuery  (KStringView sSQL, OutputFormat iFormat=FORM_ASCII, FILE* fpout = stdout);
	KString     QueryAllRows (const KSQLString& sSQL, OutputFormat iFormat=FORM_ASCII, std::size_t* piNumRows=NULL);

	void   DisableRetries() { m_bDisableRetries = true;  }
	void   EnableRetries()  { m_bDisableRetries = false; }

	bool   BeginTransaction (KStringView sOptions="");
	bool   CommitTransaction (KStringView sOptions="");
	bool   RollbackTransaction (KStringView sOptions="");

	/// helper method to form AND clauses for dynamic SQL.
	KSQLString FormAndClause (const KSQLString& sDbCol, KStringView sQueryParm, FAC iFlags=FAC::FAC_NORMAL, KStringView sSplitBy=",");

	/// general purpose helper to create "group by 1,2,3,4..."
	static KSQLString FormGroupBy (uint8_t iNumCols);

	/// static helper for parsing and formatting dates
	static KString ToYYYYMMDD (KString/*copy*/ sDate);

	/// static helper to bracket an entire day used by FormAndClause
	static KString FullDay (KStringView sDate);

	/// General purpose helper method to create "order by X.column1 desc, Y.column2, ..."
	/// Designed to support (among other things) ANT table sorting.
	///
	/// @param sCommaDelimedSort The first argument is your UI's
	///   query parameter (which ANT calls 'sort').  The value is a comma-delimed list of table
	///   columns to sort the results by (which ANT or your UI framework builds as web users click on
	///   UI elements to affect the sorting).
	///
	/// @param sOrderBy The 2nd argument is the return value (SQL "order by" clause).  Note that you can
	///   either pass in an empty string or you can pass in an initial "order by" and allow this
	///   method to ammend it.  The method returns sOrderBy untouched if sCommaDelimedSort is empty
	///   or if there is otherwise nothing to sort by.
	///
	/// @param Config The last argument is an key/value object of mappings from column
	///   names that the UI understands to physical database column expressions that are in your query.
	///   e.g. Let's say you API has a column that the UI knows as "Status".  In the SQL query, the
	///   status column is actually called "status_code" and is pulled from a table with the alias "C".
	///   Your resulting Config (at compile time) should look something like this:
	///
	///   @code
	///   {
	///      { "status",   "C.status_code" },
	///      ...
	///   }
	///   @endcode
	///
	/// Note: all query parms are case-insentive, so "Status" and "status" are the same.
	/// In the event of an error, the method returns false and KSQL's GetLastError() will explain it.
	/// The most common error would be an attempt to sort by a column that is not in your Config spec.
	bool FormOrderBy (KStringView sCommaDelimedSort, KSQLString& sOrderBy, const KJSON& Config);

	/// shortcut to KROW::EscapeChars that automatically adds the DBType
	KSQLString EscapeString (KStringView sCol)
	{
		return KROW::EscapeChars(sCol, m_iDBType);
	}

	/// shortcut to KROW::EscapeChars that automatically adds the DBType
	bool NeedsEscape (KStringView sCol)
	{
		return KROW::NeedsEscape(sCol, m_iDBType);
	}

	/// returns the list of chars that are escaped for the current DBType
	KStringView EscapedCharacters() const
	{
		return KROW::EscapedCharacters(m_iDBType);
	}

	/// converts a list of (single) quoted and comma separated values into an injection safe string with the same format
	KSQLString EscapeFromQuotedList(KStringView sList);

	/// Allow KSQL to throw in case of SQL errors. Returns previous throw status
	bool SetThrow(bool bYesNo) { std::swap(m_bMayThrow, bYesNo); return bYesNo; }
	/// returns true if KSQL is allowed to throw
	bool GetThrow() const { return m_bMayThrow; }

	//::::::::::::::::::::::
	class DIFF
	//::::::::::::::::::::::
	{
		public:
		static constexpr KStringViewZ diff_prefix{"diff_prefix"};
		static constexpr KStringViewZ left_schema{"left_schema"};
		static constexpr KStringViewZ left_prefix{"left_prefix"};
		static constexpr KStringViewZ right_schema{"right_schema"};
		static constexpr KStringViewZ right_prefix{"right_prefix"};
		static constexpr KStringViewZ include_columns_on_missing_tables{"include_columns_on_missing_tables"};
		static constexpr KStringViewZ ignore_collate{"ignore_collate"};
		static constexpr KStringViewZ show_meta_info{"show_meta_info"};

		struct OneDiff
		{
			OneDiff() = default;
			OneDiff(KString sComment,
			        KSQLString sActionLeft,
			        KSQLString sActionRight)
			: sComment(std::move(sComment))
			, sActionLeft(std::move(sActionLeft))
			, sActionRight(std::move(sActionRight))
			{}

			KString                 sComment;
			KSQLString sActionLeft;
			KSQLString sActionRight;

		}; // One

		using Diffs = std::vector<OneDiff>;

	}; // DIFF

	/// Load entire data dictionary into a JSON structure for the given schema (or current connection).
	/// Optionally restrict by tablename prefix (case insensitive).
	KJSON LoadSchema (KStringView sDBName="", KStringView sStartsWith="", const KJSON& options={});

	/// Diff two data dictionaries, returned by prior calls to LoadSchema().
	/// Produces two summaries of the diffs: one structured (DIFF::Diffs) and the other serialized (ASCII "diff" output).
	/// Returns the number of diffs (or 0 if no diffs)
	std::size_t DiffSchemas (const KJSON& LeftSchema,
	                         const KJSON& RightSchema,
	                         DIFF::Diffs& Diffs,
	                         KStringRef&  sSummary,
	                         const KJSON& options={
	                            {DIFF::left_schema,"left schema"},
	                            {DIFF::left_prefix,"<"},
	                            {DIFF::right_schema,"right schema"},
	                            {DIFF::right_prefix,">"}
	                         });

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// helper object to proxy access to KSQL and reset the Throw/NoThrow state after use
	template<class SQL>
	class ThrowingSQL : public KReferenceProxy<SQL>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		using base = KReferenceProxy<SQL>;

		ThrowingSQL(SQL& sql, bool bYesNo)
		: base(sql)
		, m_bResetTo(sql.SetThrow(bYesNo))
		{
		}

		~ThrowingSQL()
		{
			// reset initial state
			base::get().SetThrow(m_bResetTo);
		}

	//----------
	private:
	//----------

		bool  m_bResetTo;

	}; // ThrowingSQL

	/// returns helper object to access to KSQL in throwing mode, and reset mode after use
	/// @code use like:
	/// ksql.Throw()->ExecSQL("...");
	/// or
	/// auto sql = ksql.Throw();
	/// sql->ExecSQL("...");
	/// @endcode
	auto Throw()   { return ThrowingSQL<KSQL>(*this, true ); }
	/// returns helper object to access to KSQL in non-throwing mode, and reset mode after use
	/// @code use like:
	/// ksql.NoThrow()->ExecSQL("...");
	/// or
	/// auto sql = ksql.NoThrow();
	/// sql->ExecSQL("...");
	/// @endcode
	auto NoThrow() { return ThrowingSQL<KSQL>(*this, false); }

	/// acquire a named lock, which is automatically released when the connection fails
	/// (on mysql, on other platforms this defers to GetPersistentLock())
	bool GetLock (KStringView sName, chrono::seconds iTimeoutSeconds = chrono::seconds(-1));
	/// release a named lock (on mysql, on other platforms this defers to ReleasePersistentLock())
	bool ReleaseLock (KStringView sName);
	/// check a named lock (on mysql, on other platforms this defers to IsPersistentlyLocked())
	bool IsLocked (KStringView sName);

	/// acquire a lock that is persistent through database connections
	bool GetPersistentLock (KStringView sName, chrono::seconds iTimeoutSeconds = chrono::seconds(-1));
	/// release a lock that is persistent through database connections
	bool ReleasePersistentLock (KStringView sName);
	/// check a lock that is persistent through database connections
	bool IsPersistentlyLocked (KStringView sName);

	using SchemaCallback = std::function<bool(uint16_t iFrom, uint16_t iTo)>;

	/// check for updates of the DB schema, try to setup params automatically
	bool EnsureSchema (KStringView sSchemaVersionTable,
	                   KStringView sSchemaFolder,
	                   KStringView sFilenamePrefix,
	                   uint16_t iCurrentSchema,
	                   uint16_t iInitialSchema=100,
	                   bool bForce = false,
	                   const SchemaCallback& Callback = nullptr);

	/// get current schema version
	uint16_t GetSchemaVersion (KStringView sTablename);
	uint16_t GetSchema (KStringView sTablename) { return GetSchemaVersion (sTablename); } // DEPRECATED: for backward compatability only

	/// Check for valid DB connection - if none, try to open one. Search
	/// the connection parameters in
	/// 1. an explicit dbc file given
	/// 2. a dbc file given from environment or IniParms
	/// 3. single environment vars prefixed with "sIdentifier_"
	///    where iIdentifer is one of the comma-delimed values in sIdentifierList
	///    which are applied LEFT to RIGHT, with LAST ONE winning
	/// 4. if dbc file name was empty, construct one from "/etc/sIdentifier.dbc",
	///    and if existing will be loaded
	/// 5. single IniParms with connection parameters
	///
	/// All separate connection parameters are selected by the above logic in descending
	/// precedence.
	///
	/// @param sIdentifierList name used as prefix for env vars to
	/// load the configuration from.
	/// @param sDBCFile dbc file to be used. Can be empty.
	/// @param INI a property sheet with ini parameters which
	/// @param iConnectTimeoutSecs can be used to override MySQL default (which is huge/long)
	/// will be searched as last resort to find connection parameters
	bool EnsureConnected (KStringView sIdentifierList, KString sDBCFile, const IniParms& INI = IniParms{}, uint16_t iConnectTimeoutSecs = 0);

	/// conditionally open the db connection, assuming all connection parms are already set
	/// @param iConnectTimeoutSecs can be used to override MySQL default (which is huge/long)
	bool EnsureConnected (uint16_t iConnectTimeoutSecs = 0);

	void SetDBC (KStringView sFile) { m_sDBCFile = sFile; }
	const KString& GetDBC () const { return m_sDBCFile;  }

	/// returns the connection ID for the current connection, or 0
	ConnectionID GetConnectionID(bool bQueryIfUnknown = true);

	/// kills connection with ID iConnectionID
	bool KillConnection(ConnectionID iConnectionID);

	/// kills running query of connection with ID iConnectionID
	bool KillQuery(ConnectionID iConnectionID);

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// keeping current query state of a connection
	struct ConnectionInfo
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		ConnectionID ID       { 0 }; ///< the connection ID
		KString      sHost;          ///< the database host
		KString      sDB;            ///< the current database
		KString      sState;         ///< the current state
		KString      sQuery;         ///< the running query
		std::size_t  iSeconds { 0 }; ///< the time in seconds the query is running

	}; // ConnectionID

	/// returns a ConnectionInfo struct for the requested connection ID
	/// @param iConnectionID the connection ID to query for
	ConnectionInfo GetConnectionInfo(ConnectionID iConnectionID);

	TXList  m_TxList;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class KSQLStatementStats
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		void      clear();
		void      Enable(bool bYesNo);
		uint64_t  Total() const;
		KString   Print() const;

		void      Collect(KStringView sLastSQL, QueryType QueryType = QueryType::None)
		{
			if DEKAF2_UNLIKELY(bEnabled)
			{
				Increment(sLastSQL, QueryType);
			}
		}

		uint64_t  iSelect   { 0 };
		uint64_t  iInsert   { 0 };
		uint64_t  iUpdate   { 0 };
		uint64_t  iDelete   { 0 };
		uint64_t  iCreate   { 0 };
		uint64_t  iAlter    { 0 };
		uint64_t  iDrop     { 0 };
		uint64_t  iTransact { 0 };
		uint64_t  iExec     { 0 };
		uint64_t  iAction   { 0 };
		uint64_t  iTblMaint { 0 };
		uint64_t  iInfo     { 0 };
		uint64_t  iOther    { 0 };
		bool      bEnabled  { false };

	//----------
	private:
	//----------

		void      Increment(KStringView sLastSQL, QueryType QueryType);

	}; // SQLStmtStats

	KSQLStatementStats& GetSQLStmtStats()            { return m_SQLStmtStats;         }

//----------
private:
//----------

	void BulkCopyFlush (std::vector<KROW>& BulkRows, bool bLast=true);

	void LogPerformance (KDuration iMilliseconds, bool bIsQuery);

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct DBCLoader
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KString operator()(KStringViewZ s)
		{
			return kReadAll(s);
		}
	};

	using DBCCache = KSharedCache<KString, KString, DBCLoader>;

	static DBCCache s_DBCCache;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// keep a list of connection IDs, thread safe
	class ConnectionIDs
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		void Add(ConnectionID iConnectionID);
		bool Has(ConnectionID iConnectionID) const;
		bool HasAndRemove(ConnectionID iConnectionID);

	//----------
	private:
	//----------

		KThreadSafe<std::set<ConnectionID>> m_Connections;

	}; // ConnectionIDs

	static ConnectionIDs s_CanceledConnections;

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// keep a list of timed queries and their connection IDs, thread safe
	class TimedConnectionIDs
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		TimedConnectionIDs();
		~TimedConnectionIDs();

		/// add a connection ID of a query with its timeout to the list of timed queries
		void Add(KSQL& Instance);
		/// add a connection ID of a query with its timeout to the list of timed queries
		void Add(ConnectionID iConnectionID, std::chrono::milliseconds Timeout, const KSQL& Instance);
		/// remove a timed query from the watchlist
		bool Remove(ConnectionID iConnectionID, uint64_t iServerHash);
		/// remove a timed query from the watchlist
		bool Remove(KSQL& Instance);

	//----------
	private:
	//----------

		using Clock = std::chrono::steady_clock;

		void Watcher();

		struct IndexByID{};
		struct IndexByTime{};

		struct Row
		{
			ConnectionID      ID;
			uint64_t          iServerHash;
			Clock::time_point Expires;
		};

		using TimedConnectionsMap =
			boost::multi_index::multi_index_container<
				Row,
				boost::multi_index::indexed_by<
					boost::multi_index::hashed_unique<
						boost::multi_index::tag<IndexByID>,
						boost::multi_index::composite_key<
							Row,
							boost::multi_index::member<Row, ConnectionID, &Row::ID         >,
							boost::multi_index::member<Row, uint64_t,     &Row::iServerHash>
						>
					>,
					boost::multi_index::ordered_non_unique<
						boost::multi_index::tag<IndexByTime>,
						boost::multi_index::member<Row, Clock::time_point, &Row::Expires>
					>
				>
			>;

		KThreadSafe<TimedConnectionsMap>                   m_Connections;
		KThreadSafe<KMap<uint64_t, std::unique_ptr<KSQL>>> m_DBs;
		bool                                               m_bQuit;
		// lazy initialized worker, to not create a thread in the static
		// initialization phase before any signal masks are set
		std::unique_ptr<std::thread>                       m_Watcher;
		std::once_flag                                     m_Once;

	}; // TimedConnectionIDs

	static TimedConnectionIDs s_TimedConnections;

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
		/// set the internal data buffer to a 0 length string
		void ClearContent()
		{
			if (dszValue)
			{
				dszValue.get()[0] = 0;
			}
		}

		std::unique_ptr<char[]> dszValue;
#endif
		KString     sColName;
		KCOL::Flags iKSQLDataType   { KCOL::Flags::NOFLAG };
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
	const  KColInfo& GetColProps (KROW::Index iOneBasedColNum);
	void   ResetConnectionID () { m_iConnectionID = 0; }

	// my own struct for column attributes and data coming back from queries:
	KColInfos  m_dColInfo;

//----------
protected:
//----------

	/// sets m_sLastError, then either throws a KSQL exception if allowed to do, or returns false
	/// @param sError the error string to set
	/// @param iErrorNum the db specific error number, if available, default -1
	/// @return always false
	bool SetError(KString sError, uint32_t iErrorNum = -1, bool bNoThrow = false);
	/// sets m_sLastError, and returns false (never throws, even if KSQL is configured to do so)
	/// @param sError the error string to set
	/// @param iErrorNum the db specific error number, if available, default -1
	/// @return always false
	bool SetErrorNoThrow(KString sError, uint32_t iErrorNum = -1)
	{
		return SetError(std::move(sError), iErrorNum, true);
	}
	/// Reset error string and error status
	void ClearError();
	/// Reset last sql command string - only needed in multi-tenant environments for client isolation
	void ClearLastSQL() { m_sLastSQL.clear(); }

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	static KSQLString EscapeType(DBT iDBType, const char* value);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	template<typename T,
	         typename std::enable_if<!std::is_constructible<KStringView, T>::value
	                               || std::is_same<KSQLString, typename std::decay<T>::type>::value, int>::type = 0
	>
	static auto EscapeType(DBT iDBType, T&& value)
	//-----------------------------------------------------------------------------
	{
		return std::forward<T>(value);
	}

	//-----------------------------------------------------------------------------
	template<typename T,
	         typename Decayed = typename std::decay<T>::type,
	         typename std::enable_if<std::is_constructible<KStringView, T>::value
	                             && !std::is_same<KSQLString, Decayed>::value
	                             && !std::is_same<KString,                 Decayed>::value
	                             && !std::is_same<std::string,             Decayed>::value, int>::type = 0
	>
	static KSQLString EscapeType(DBT iDBType, T&& value)
	//-----------------------------------------------------------------------------
	{
		// this is a string_view like parameter - escape it if it does not come from
		// constant storage
		if (!kIsInsideDataSegment(value.data()))
		{
			return KROW::EscapeChars (std::forward<T>(value), iDBType);
		}
		else
		{
			return std::forward<T>(value);
		}
	}

	//-----------------------------------------------------------------------------
	template<typename T,
	         typename Decayed = typename std::decay<T>::type,
	         typename std::enable_if<std::is_same<KString,                 Decayed>::value
	                             ||  std::is_same<std::string,             Decayed>::value, int>::type = 0
	>
	static KSQLString EscapeType(DBT iDBType, T&& value)
	//-----------------------------------------------------------------------------
	{
		// this is a string like parameter - escape it
		return KROW::EscapeChars (std::forward<T>(value), iDBType);
	}

	//-----------------------------------------------------------------------------
	// this one is needed to appease gcc, which otherwise would claim that iDBType is
	// unused (in the methods with std::apply)
	template<class FormatString>
	static KSQLString FormatEscaped(DBT iDBType, FormatString&& sFormat)
	//-----------------------------------------------------------------------------
	{
		return std::forward<FormatString>(sFormat);

	} // FormatEscaped

	//-----------------------------------------------------------------------------
	/// static - escapes all string arguments and leaves the rest alone
	template<class FormatString,
	         class... Args,
	         typename std::enable_if<std::is_same<KSQLString, typename std::decay<FormatString>::type>::value &&
	                                 sizeof...(Args) != 0, int>::type = 0>
	static KSQLString FormatEscaped(DBT iDBType, FormatString&& sFormat, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		KSQLString sRet;

		sRet.ref() = std::apply([sFormat](auto&&... args)
		{
			return kFormat(KRuntimeFormat(sFormat.str()), std::forward<decltype(args)>(args)...);
		},
		std::make_tuple(EscapeType(iDBType, args)...));

		return sRet;

	} // FormatEscaped

	//-----------------------------------------------------------------------------
	/// static - escapes all string arguments and leaves the rest alone
	template<class FormatString,
	         class... Args,
	         typename std::enable_if<!std::is_same<KSQLString, typename std::decay<FormatString>::type>::value &&
	                                 sizeof...(Args) != 0, int>::type = 0>
	static KSQLString FormatEscaped(DBT iDBType, FormatString&& sFormat, Args&&... args)
	//-----------------------------------------------------------------------------
	{
		KSQLString sRet;

		sRet.ref() = std::apply([sFormat](auto&&... args)
		{
			return kFormat(KRuntimeFormat(sFormat), std::forward<decltype(args)>(args)...);
		},
		std::make_tuple(EscapeType(iDBType, args)...));

		return sRet;

	} // FormatEscaped

//----------
public:
//----------

	/// add a temp table to list that gets cleaned up on descruction
	void AddTempTable (KStringView sTablename);

	/// remove a temp table to list that gets cleaned up on descruction
	void RemoveTempTable (KStringView sTablename);

	/// drop all temp table in the list formed by AddTempTable();
	void PurgeTempTables ();

	//----------------------------------------------------------------------
	/// helper function to detect dynamic (format) strings which could be the source of SQL injections
	static bool IsDynamicString(KStringView sStr,  bool bThrowIfDynamic = false);
	static bool IsDynamicString(const char* sAddr, bool bThrowIfDynamic = false);
	//----------------------------------------------------------------------

	// hide this FormatSQL from general use, as we want users to prefer the one
	// with automatic DBT deduction
	struct format_detail
	{
		//----------------------------------------------------------------------
		/// static - format an SQL query with python syntax and automatically escape all string parameters
		// version for KSQLString as the format string, in which case we
		// do not test for const data
		template<class FormatString,
		         class... Args,
		         typename std::enable_if<std::is_same<KSQLString, typename std::decay<FormatString>::type>::value, int>::type = 0>
		KSQLString FormatSQL (DBT iDBType, const FormatString& sFormat, Args&&... args)
		//----------------------------------------------------------------------
		{
			return FormatEscaped(iDBType, std::forward<FormatString>(sFormat), std::forward<Args>(args)...);

		} // FormatSQL

		//-----------------------------------------------------------------------------
		/// static - escapes all string arguments and leaves the rest alone
		// prevent FormatString from being KString, as that could mean that it is the
		// output of a non-escaping kFormat() call, or any other manually assembled, non-escaped string
		template<class FormatString,
		         class... Args,
		         typename Decayed = typename std::decay<FormatString>::type,
		         typename std::enable_if<!std::is_same<KString,     Decayed>::value &&
		                                 !std::is_same<std::string, Decayed>::value &&
		                                 !std::is_same<KSQLString,  Decayed>::value, int>::type = 0
		>
		static KSQLString FormatSQL(DBT iDBType, FormatString&& sFormat, Args&&... args)
		//-----------------------------------------------------------------------------
		{
			// although we already forbide strings in this template, we also check if
			// the string comes from constant storage, to avoid uses like from KString::c_str()
			IsDynamicString(sFormat, true); // throws if is dynamic
			return FormatEscaped(iDBType, std::forward<FormatString>(sFormat), std::forward<Args>(args)...);
		}
	};

	//----------------------------------------------------------------------
	/// format an SQL query with python syntax and automatically escape all string parameters
	// version for KSQLString as the format string, in which case we
	// do not test for const data
	template<class FormatString,
	         class... Args,
	         typename std::enable_if<std::is_same<KSQLString, typename std::decay<FormatString>::type>::value, int>::type = 0
	>
	KSQLString FormatSQL (const FormatString& sFormat, Args&&... args)
	//----------------------------------------------------------------------
	{
		if (IsFlag(F_NoTranslations) || kFind(sFormat.str(), "{{") == KStringView::npos)
		{
			return FormatEscaped(m_iDBType, sFormat, std::forward<Args>(args)...);
		}
		else
		{
			KSQLString sSQL = sFormat;
			DoTranslations (sSQL);
			return FormatEscaped(m_iDBType, sSQL, std::forward<Args>(args)...);
		}

	} // FormatSQL

	//----------------------------------------------------------------------
	/// format an SQL query with python syntax and automatically escape all string parameters
	// prevent FormatString from being KString, as that could mean that it is the
	// output of a non-escaping kFormat() call, or any other manually assembled, non-escaped string
	template<class FormatString,
	         class... Args
	       , typename Decayed = typename std::decay<FormatString>::type
	       , typename std::enable_if<!std::is_same<KString,     Decayed>::value &&
	                                 !std::is_same<std::string, Decayed>::value &&
	                                 !std::is_same<KSQLString,  Decayed>::value, int>::type = 0
	>
	KSQLString FormatSQL (FormatString&& sFormat, Args&&... args)
	//----------------------------------------------------------------------
	{
		// although we already forbide strings in this template, we also check if
		// the string comes from constant storage, to avoid uses like from KString::c_str()
		if (IsFlag(F_NoTranslations) || kFind(sFormat, "{{") == KStringView::npos)
		{
			IsDynamicString(sFormat, true); // throws if non-const data
			return FormatEscaped(m_iDBType, sFormat, std::forward<Args>(args)...);
		}
		else
		{
			KSQLString sSQL(std::forward<FormatString>(sFormat)); // throws if non-const data
			DoTranslations (sSQL);
			return FormatEscaped(m_iDBType, sSQL, std::forward<Args>(args)...);
		}

	} // FormatSQL

//----------
private:
//----------

	KSQLString m_sLastSQL;
	KString    m_sLastErrorSetOnlyWithSetError;   // error string. Never set directly, only via SetError()
	uint32_t   m_iErrorSetOnlyWithSetError { 0 }; // db error number (e.g. ORA code). Never set directly, only via SetError()
	Flags      m_iFlags { Flags::F_None };        // set by calling SetFlags()
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
	uint16_t   m_iConnectTimeoutSecs { 0 };
	KString    m_sUsername;
	KString    m_sPassword;
	KString    m_sHostname;
	KString    m_sDatabase;
	mutable KString m_sConnectSummary;

	/// vector of temp tables to drop on descruction of class
	std::set<KString> m_TempTables;

#ifdef DEKAF2_HAS_MYSQL
	MYSQL*         m_dMYSQL       { nullptr };     // MYSQL      m_mysql;
	MYSQL_ROW      m_MYSQLRow     { nullptr };     // MYSQL_ROW  m_row;
	MYSQL_ROW_LENS m_MYSQLRowLens { nullptr };	   // stores the result of mysql_fetch_lengths() for one row
	MYSQL_RES*     m_dMYSQLResult { nullptr };     // MYSQL_RES* m_presult;
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
	bool       m_bMayThrow { false };
	KString    m_sDBCFile;
	KString    m_sNeverReadMeDirectlyTmpResultsFile;
	uint64_t   m_iRowNum { 0 };
	KROW::Index m_iNumColumns { 0 };
	uint64_t   m_iNumRowsBuffered { 0 };
	uint64_t   m_iNumRowsAffected { 0 };
	uint64_t   m_iLastInsertID { 0 };
	mutable uint64_t m_iConnectHash { 0 };
	ConnectionID m_iConnectionID { 0 };
	char**     m_dBufferedColArray { nullptr };
	KString    m_sErrorPrefix;
	bool       m_bDisableRetries { false };
	bool       m_bEnableQueryTimeout { false };
	static std::atomic<std::chrono::milliseconds> s_QueryTimeout;
	static std::atomic<QueryType> s_QueryTypeForTimeout;
	std::chrono::milliseconds m_QueryTimeout { 0 };
	QueryType  m_QueryTypeForTimeout { QueryType::None };
	KDuration  m_iWarnIfOverMilliseconds { 0 };
	FILE*      m_fpPerformanceLog { nullptr };
	KString    m_sTempDir { "/tmp" };
	KSQLStatementStats m_SQLStmtStats;
	std::function<void(const KSQL&, KDuration, const KString&)> m_TimingCallback;

	bool  BufferResults ();
	void  FreeAll (bool bDestructor=false);
	void  FreeBufferedColArray (bool fValuesOnly=false);
	void  InvalidateConnectHash () const { m_iConnectHash = 0; }
	void  FormatConnectSummary () const;
	void  InvalidateConnectSummary () const { m_sConnectSummary.clear(); InvalidateConnectHash(); }
	/// modifies sError if connection was lost due to a connection kill request, else retains old message
	bool  PreparedToRetry (uint32_t iErrorNum, KString* sError = nullptr);

    #ifdef DEKAF2_HAS_ORACLE
	bool  WasOCICallOK    (KStringView sContext, uint32_t iErrorNum, KStringRef& sError);
	bool _BindByName      (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType);
	bool _BindByPos       (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType);
	//OL _ArrayBindByName (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType); - TODO
	//OL _ArrayBindByPos  (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType); - TODO
	#endif

	#ifdef DEKAF2_HAS_CTLIB
	uint64_t ctlib_get_rows_affected ();
	bool     ctlib_is_initialized    ();
	bool     ctlib_login             ();
	bool     ctlib_logout            ();
	bool     ctlib_execsql           (KStringView sSQL);
	bool     ctlib_nextrow           ();
	bool     ctlib_api_error         (KStringView sContext);
	uint32_t ctlib_check_errors      ();
	bool     ctlib_clear_errors      ();
	bool     ctlib_prepare_results   ();
	void     ctlib_flush_results     ();
	KString  m_sCtLibLastError;
	uint32_t m_iCtLibErrorNum { 0 };
	#endif

	bool DecodeDBCData(KStringView sBuffer, KStringViewZ sDBCFile);

}; // KSQL

// declare the operators for KSQL's flag enums
DEKAF2_ENUM_IS_FLAG(KSQL::Flags)
DEKAF2_ENUM_IS_FLAG(KSQL::FAC)
DEKAF2_ENUM_IS_FLAG(KSQL::QueryType)

//----------------------------------------------------------------------
/// format an SQL query with python syntax and automatically escape all string parameters
/// @param sFormat the format string
/// @param iDBType the SQL database type to select the escape characters for
template<class FormatString, class... Args>
KSQLString kFormatSQL (KSQL::DBT iDBType, FormatString&& sFormat, Args&&... args)
//----------------------------------------------------------------------
{
	return KSQL::format_detail::FormatSQL(iDBType,
	                                      std::forward<FormatString>(sFormat),
	                                      std::forward<Args>(args)...);
}

//----------------------------------------------------------------------
/// format an SQL query with python syntax and automatically escape all string parameters with the
/// maximum of escape characters (=MySQL)
/// @param sFormat the format string
template<class FormatString, class... Args>
KSQLString kFormatSQL (FormatString&& sFormat, Args&&... args)
//----------------------------------------------------------------------
{
	return KSQL::format_detail::FormatSQL(KSQL::DBT::MYSQL,
	                                      std::forward<FormatString>(sFormat),
	                                      std::forward<Args>(args)...);
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DbSemaphore
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	DbSemaphore (KSQL& db, KString sAction, bool bThrow=true, bool bWait=false, chrono::seconds iTimeout=chrono::seconds::zero(), bool bVerbose=false);
	~DbSemaphore () { Clear(); }

	bool  Create (chrono::seconds iTimeout = chrono::seconds::zero());
	bool  Clear ();
	bool  IsCreated () const { return m_bIsSet; }
	const KString& GetLastError () const { return m_sLastError; }

//----------
private:
//----------

	KSQL&    m_db;
	KString  m_sAction;
	KString  m_sLastError;
	bool     m_bThrow { false };
	bool     m_bIsSet { false };
	bool     m_bVerbose { false };

}; // DbSemaphore

DEKAF2_NAMESPACE_END
