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

// #define DEKAF2_HAS_ORACLE  <-- conditionally defined by makefile
// #define DEKAF2_HAS_MYSQL   <-- conditionally defined by makefile
// #define DEKAF2_HAS_DBLIB   <-- conditionally defined by makefile
// #define DEKAF2_HAS_CTLIB   <-- conditionally defined by makefile
// #define DEKAF2_HAS_ODBC    <-- conditionally defined by makefile
// #define DEKAF2_HAS_SQLITE3 <-- conditionally defined by makefile

#include "ksql.h"   // <-- public header (should have no dependent headers other than DEKAF header)
#include "bits/ksql_dbc.h"
#include "dekaf2.h"
#include "kcrashexit.h"
#include "ksystem.h"
#include "kregex.h"
#include "kstack.h"
#include "ksplit.h"
#include "kstringutils.h"
#include "kfilesystem.h"
#include "kwebclient.h"
#include "khash.h"
#include "kbar.h"
#include <cstdint>

#if defined(DEKAF2_IS_WINDOWS) || defined(DEKAF2_DO_NOT_HAVE_STRPTIME)
	#include <ctime>
	#include <iomanip>
	#include <sstream>

	extern "C" char* strptime(const char* s,
							  const char* f,
							  struct tm* tm)
	{
		std::istringstream input(s);
		input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
		input >> std::get_time(tm, f);
		if (input.fail())
		{
			return nullptr;
		}
		return (char*)(s + input.tellg());
	}
#endif

#ifndef DEKAF2_DO_NOT_HAVE_STRPTIME
#include <ctime>   // for strptime()
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif

#ifdef DEKAF2_HAS_MYSQL
  #ifdef WIN32
    #define NO_CLIENT_LONG_LONG // <-- for mysql header file
  #endif
  #ifdef cygwin
    #undef STDCALL
  #endif
  #include <mysql.h>          // mysql top include
  #include <mysql/errmsg.h>   // mysql error codes (start with CR_)
#endif

#ifdef DEKAF2_HAS_SQLITE3
  #include "ksqlite.h"
#endif

#ifdef DEKAF2_HAS_DBLIB
  #ifndef __STDC_VERSION__
	// FreeTDS depends on __STDC_VERSION__ to _not_ redefine standard types
	// C++ unfortunately does not require to set it, so we match it with the
	// value of __cplusplus
	#define __STDC_VERSION__ __cplusplus
  #endif
  // dependent headers when building DBLIB (these are *not* part of our distribution):
  #include <sqlfront.h>        // dblib top level include
  #include <sqldb.h>           // dblib top level include

  int dblib_msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line);
  int dblib_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
#endif

#ifdef DEKAF2_HAS_CTLIB
	#ifndef __STDC_VERSION__
	  // FreeTDS depends on __STDC_VERSION__ to _not_ redefine standard types
	  // C++ unfortunately does not require to set it, so we match it with the
	  // value of __cplusplus
	  #define __STDC_VERSION__ __cplusplus
	#endif
  #include <ctpublic.h>        // CTLIB, alternative to DBLIB for Sybase and MS SQL Server
  #include <sybdb.h>	       // "sybdb.h is the only other file you need" (FreeTDS doc) -- ctpublic.h should actually already include it
  #define KSQL2_CTDEBUG 3
#endif

// FYI: from ocidfn.h
//	 SQLT_CHR  1                        /* (ORANET TYPE) character string */
//	 SQLT_NUM  2                          /* (ORANET TYPE) oracle numeric */
//	 SQLT_INT  3                                 /* (ORANET TYPE) integer */
//	 SQLT_FLT  4                   /* (ORANET TYPE) Floating point number */
//	 SQLT_STR  5                                /* zero terminated string */
//	 SQLT_VNU  6                        /* NUM with preceding length byte */
//	 SQLT_PDN  7                  /* (ORANET TYPE) Packed Decimal Numeric */
//	 SQLT_LNG  8                                                  /* long */
//	 SQLT_VCS  9                             /* Variable character string */
//	 SQLT_NON  10                      /* Null/empty PCC Descriptor entry */
//	 SQLT_RID  11                                                /* rowid */
//	 SQLT_DAT  12                                /* date in oracle format */
//	 SQLT_VBI  15                                 /* binary in VCS format */
//	 SQLT_BIN  23                                  /* binary data(DTYBIN) */
//	 SQLT_LBI  24                                          /* long binary */
//	 SQLT_UIN  68                                     /* uint32_teger */
//	 SQLT_SLS  91                        /* Display sign leading separate */
//	 SQLT_LVC  94                                  /* Longer longs (char) */
//	 SQLT_LVB  95                                   /* Longer long binary */
//	 SQLT_AFC  96                                      /* Ansi fixed char */
//	 SQLT_AVC  97                                        /* Ansi Var char */
//	 SQLT_CUR  102                                        /* cursor  type */
//	 SQLT_RDD  104                                    /* rowid descriptor */
//	 SQLT_LAB  105                                          /* label type */
//	 SQLT_OSL  106                                        /* oslabel type */
//	 SQLT_NTY  108                                   /* named object type */
//	 SQLT_REF  110                                            /* ref type */
//	 SQLT_CLOB 112                                       /* character lob */
//	 SQLT_BLOB 113                                          /* binary lob */
//	 SQLT_BFILEE 114                                   /* binary file lob */
//	 SQLT_CFILEE 115                                /* character file lob */
//	 SQLT_RSET 116                                     /* result set type */
//	 SQLT_NCO  122      /* named collection type (varray or nested table) */
//	 SQLT_VST  155                                      /* OCIString type */
//	 SQLT_ODT  156                                        /* OCIDate type */
//	 SQLT_DATE                      184                      /* ANSI Date */
//	 SQLT_TIME                      185                           /* TIME */
//	 SQLT_TIME_TZ                   186            /* TIME WITH TIME ZONE */
//	 SQLT_TIMESTAMP                 187                      /* TIMESTAMP */
//	 SQLT_TIMESTAMP_TZ              188       /* TIMESTAMP WITH TIME ZONE */
//	 SQLT_INTERVAL_YM               189         /* INTERVAL YEAR TO MONTH */
//	 SQLT_INTERVAL_DS               190         /* INTERVAL DAY TO SECOND */
//	 SQLT_TIMESTAMP_LTZ             232        /* TIMESTAMP WITH LOCAL TZ */
//	 SQLT_PNTY   241              /* pl/sql representation of named types */

namespace dekaf2
{

struct SQLTX
{
	KStringView sOriginal;
	KStringView sMySQL;
	KStringView sSQLite3;
	KStringView sOraclePre8;
	KStringView sOracle;
	KStringView sSybase;
	KStringView sInformix;
};

constexpr SQLTX g_Translations[] = {
	// ---------------  ----------------  ----------------  ---------------  --------------   --------------  ----------------------------
	// ORIGINAL         MYSQL             SQLite3           ORACLEpre8       ORACLE8.x...     SQLSERVER       INFORMIX
	// ---------------  ----------------  ----------------  ---------------  --------------   --------------  ----------------------------
	{ "+",              "+",/*CONCAT()*/  "+",              "||",            "||",            "+",            "||"                     },
	{ "CAT",            "+",/*CONCAT()*/  "+",              "||",            "||",            "+",            "||"                     },
	{ "DATETIME",       "timestamp",      "timestamp",      "date",          "date",          "datetime",     "?datetime-col?"         }, // column type
	{ "NOW",            "now()",          "now()",          "sysdate",       "sysdate",       "getdate()",    "current year to second" }, // function
// DO NOT USE UTC! INSTEAD USE NOW. UTC IS AN ABERRATION. NOW _IS_ UTC, but is displayed in your time zone
	{ "UTC",            "utc_timestamp()","datetime('now')","sysdate",       "sysdate",       "getutcdate()", "current year to second" }, // function (oracle and informix are not correct right now)
	{ "MAXCHAR",        "text",           "text",           "varchar(2000)", "varchar(4000)", "text",         "char(2000)"             },
	{ "CHAR2000",       "text",           "text",           "varchar(2000)", "varchar(2000)", "text",         "char(2000)"             },
	{ "PCT",            "%",              "%",              "%",             "%",             "%",            "%"                      },
	{ "AUTO_INCREMENT", "auto_increment", "",               "",              "",              "identity",     ""                       }
	// ---------------  ----------------  ----------------  ---------------  --------------  ---------------  ----------------------------
};

//-----------------------------------------------------------------------------
static void*  kfree (void* dPointer, const char* sContext = nullptr )
//-----------------------------------------------------------------------------
{
	typedef enum {fubar=1} EX;

	if (dPointer)
	{
		DEKAF2_TRY {
			free (dPointer);
		}
		DEKAF2_CATCH (EX) {
			kWarning ("would have crashed on free() of 0x{} {}",
				dPointer,
				(sContext) ? " : "    : "",
				(sContext) ? sContext : "");
		}
	}

	return (nullptr);

} // kfree

//-----------------------------------------------------------------------------
void* kmalloc (size_t iNumBytes, const char* pszContext, bool bClearMemory = true)
//-----------------------------------------------------------------------------
{
	kDebug (3, "{}: dynamic allocation of {} bytes", pszContext, iNumBytes);

	char* pszRawMemory = (char*) malloc (iNumBytes);
	if (!pszRawMemory)
	{
		kWarning ("{}: server ran out of memory!  Could not grab {} bytes", pszContext, iNumBytes);
		kCrashExit (CRASHCODE_MEMORY);
	}
	else if (bClearMemory)
	{
		memset (pszRawMemory, 0, iNumBytes);
	}

	return ((void*)pszRawMemory);

} // kmalloc 

//-----------------------------------------------------------------------------
void KSQL::KColInfo::SetColumnType (DBT iDBType, int iNativeDataType, KCOL::Len _iMaxDataLen)
//-----------------------------------------------------------------------------
{
	iMaxDataLen = _iMaxDataLen;

	switch (iDBType)
	{
		case DBT::MYSQL:
		switch (iNativeDataType)
		{
#ifdef DEKAF2_HAS_MYSQL
				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
				// we only care about setting flags for non-string types:
				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_LONG:
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
				case MYSQL_TYPE_YEAR:
				case MYSQL_TYPE_BIT:
					iKSQLDataType = KROW::NUMERIC;
					break;

				case MYSQL_TYPE_DECIMAL:
				case 246: // MYSQL_TYPE_NEWDECIMAL:
				case 247: // MYSQL_TYPE_ENUM:
				case 255: // MYSQL_TYPE_GEOMETRY:
					iKSQLDataType = KROW::NUMERIC;
					if (iMaxDataLen >= 19)
					{
						// this is a large integer as well (see below)..
						iKSQLDataType |= KROW::INT64NUMERIC;
					}
					break;

				case MYSQL_TYPE_LONGLONG:
				case MYSQL_TYPE_INT24:
					// make sure we flag large integers - this is important when we want to
					// convert them into JSON integers, which have a limit of 53 bits
					// - values larger than that need to be represented as strings..
					iKSQLDataType = KROW::NUMERIC | KROW::INT64NUMERIC;
					break;

					// always the problem child:
				case MYSQL_TYPE_NULL:
					iKSQLDataType = KROW::NOFLAG;
					break;

				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
				// these we can leave with no KROW flags:
				// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
				case MYSQL_TYPE_TIMESTAMP:
				case MYSQL_TYPE_DATE:
				case MYSQL_TYPE_TIME:
				case MYSQL_TYPE_DATETIME:
				case MYSQL_TYPE_NEWDATE:
				case MYSQL_TYPE_VARCHAR:
	//			case MYSQL_TYPE_TIMESTAMP2: // we do not have
	//			case MYSQL_TYPE_DATETIME2:  // the integer value
	//			case MYSQL_TYPE_TIME2:      // for these three
				case 245: // MYSQL_TYPE_JSON:
				case 248: // MYSQL_TYPE_SET:
				case 249: // MYSQL_TYPE_TINY_BLOB:
				case 250: // MYSQL_TYPE_MEDIUM_BLOB:
				case 251: // MYSQL_TYPE_LONG_BLOB:
				case 252: // MYSQL_TYPE_BLOB:
				case 253: // MYSQL_TYPE_VAR_STRING:
				case 254: // MYSQL_TYPE_STRING:
#endif // DEKAF2_HAS_MYSQL
				default:
					iKSQLDataType = KROW::NOFLAG;
					break;
			}
			break;

		case DBT::SQLSERVER:
		case DBT::SYBASE:
			switch (iNativeDataType)
			{
#ifdef DEKAF2_HAS_CTLIB
				case CS_TINYINT_TYPE:
				case CS_SMALLINT_TYPE:
				case CS_USMALLINT_TYPE:
				case CS_USHORT_TYPE:
				case CS_INT_TYPE:
				case CS_UINT_TYPE:
				case CS_BIT_TYPE:
				case CS_FLOAT_TYPE:
				case CS_REAL_TYPE:
					iKSQLDataType = KROW::NUMERIC;
					break;

				case CS_DECIMAL_TYPE:
				case CS_NUMERIC_TYPE:
				case CS_LONG_TYPE:
					iKSQLDataType = KROW::NUMERIC;
					if (iMaxDataLen >= 19)
					{
						// this is a large integer as well..
						iKSQLDataType |= KROW::INT64NUMERIC;
					}
					break;

				case CS_BIGINT_TYPE:
				case CS_UBIGINT_TYPE:
					iKSQLDataType = KROW::NUMERIC | KROW::INT64NUMERIC;
					break;

#endif
				default:
					iKSQLDataType = KROW::NOFLAG;
					break;
			}
			break;

		case DBT::ORACLE6:
		case DBT::ORACLE7:
		case DBT::ORACLE8:
		case DBT::ORACLE:
		case DBT::INFORMIX:
		default:
			iKSQLDataType = KROW::NOFLAG;
			// TODO: get column meta data from query respose for other dbtypes
			break;
	}

} // KColInfo::SetColumnType

//-----------------------------------------------------------------------------
void KSQL::KSQLStatementStats::clear()
//-----------------------------------------------------------------------------
{
	*this = KSQLStatementStats{};
}

//-----------------------------------------------------------------------------
void KSQL::KSQLStatementStats::Enable(bool bValue)
//-----------------------------------------------------------------------------
{
	clear(); // We always reset the stats to 0 whenever they are turned on or off
	bEnabled = bValue;
}

//-----------------------------------------------------------------------------
uint64_t KSQL::KSQLStatementStats::Total() const
//-----------------------------------------------------------------------------
{
	return  iSelect   +
			iInsert   +
			iUpdate   +
			iDelete   +
			iCreate   +
			iAlter    +
			iDrop     +
			iTransact +
			iExec     +
			iAction   +
			iTblMaint +
			iInfo     +
			iOther;
}

//-----------------------------------------------------------------------------
KString KSQL::KSQLStatementStats::Print() const
//-----------------------------------------------------------------------------
{
	return kFormat ("TOTAL={}, SELECT={}, INSERT={}, UPDATE={}{}{}{}{}{}{}{}{}{}{}",
				Total(), iSelect, iInsert, iUpdate,
				(iDelete   ? kFormat(", DELETE={}",   iDelete)   : ""),
				(iCreate   ? kFormat(", CREATE={}",   iCreate)   : ""),
				(iAlter    ? kFormat(", ALTER={}",    iAlter)    : ""),
				(iDrop     ? kFormat(", DROP={}",     iDrop)     : ""),
				(iTransact ? kFormat(", TRANSACT={}", iTransact) : ""),
				(iExec     ? kFormat(", EXEC={}",     iExec)     : ""),
				(iAction   ? kFormat(", Action={}",   iAction)   : ""),
				(iTblMaint ? kFormat(", TblMaint={}", iTblMaint) : ""),
				(iInfo     ? kFormat(", Info={}",     iInfo)     : ""),
				(iOther    ? kFormat(", Other={}",    iOther)    : ""));
}

//-----------------------------------------------------------------------------
void KSQL::KSQLStatementStats::Increment(KStringView sLastSQL)
//-----------------------------------------------------------------------------
{
	auto sSQLStmt = sLastSQL.substr(0,20).TrimLeft().ToLowerASCII();

	if (sSQLStmt.StartsWith("select") || sSQLStmt.StartsWith("table") || sSQLStmt.StartsWith("values"))
	{
		++iSelect;
	}
	else if (sSQLStmt.StartsWith("insert") || sSQLStmt.StartsWith("import") || sSQLStmt.StartsWith("load"))
	{
		++iInsert;
	}
	else if (sSQLStmt.StartsWith("update"))
	{
		++iUpdate;
	}
	else if (sSQLStmt.StartsWith("delete") || sSQLStmt.StartsWith("truncate"))
	{
		++iDelete;
	}
	else if (sSQLStmt.StartsWith("create"))
	{
		++iCreate;
	}
	else if (sSQLStmt.StartsWith("alter") || sSQLStmt.StartsWith("rename"))
	{
		++iAlter;
	}
	else if (sSQLStmt.StartsWith("drop"))
	{
		++iDrop;
	}
	else if (sSQLStmt.StartsWith("start")  || sSQLStmt.StartsWith("begin") ||  // start transaction or begin transaction
			 sSQLStmt.StartsWith("commit") || sSQLStmt.StartsWith("rollback")) // commit or rollback transaction
	{
		++iTransact;
	}
	else if (sSQLStmt.StartsWith("exec") || sSQLStmt.StartsWith("call") || // stored procedure execution statements
			 sSQLStmt.StartsWith("do"))
	{
		++iExec;
	}
	else if (sSQLStmt.StartsWith("use")  || sSQLStmt.StartsWith("set") || sSQLStmt.StartsWith("grant") ||
			 sSQLStmt.StartsWith("lock") || sSQLStmt.StartsWith("unlock"))
	{
		++iAction;
	}
	else if (sSQLStmt.StartsWith("analyze") || sSQLStmt.StartsWith("optimize") ||
			 sSQLStmt.StartsWith("check")   || sSQLStmt.StartsWith("repair")) // table maintenance statements
	{
		++iTblMaint;
	}
	else if (sSQLStmt.StartsWith("desc") || sSQLStmt.StartsWith("explain") ||
			 sSQLStmt.StartsWith("show"))
	{
		++iInfo;
	}
	else
	{
		kDebug (1, "Other SQL={}", sLastSQL);
		++iOther;
	}
}

//-----------------------------------------------------------------------------
KSQL::KSQL (DBT iDBType/*=DBT::MYSQL*/, KStringView sUsername/*=nullptr*/, KStringView sPassword/*=nullptr*/, KStringView sDatabase/*=nullptr*/, KStringView sHostname/*=nullptr*/, uint16_t iDBPortNum/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// this tmp file is used to hold buffered results (if flag F_BufferResults is set):
	m_sTmpResultsFile.Format ("{}/ksql-{}-{}.res", GetTempDir(), kGetPid(), kGetTid());

	if (!sUsername.empty())
	{
		SetConnect (iDBType, sUsername, sPassword, sDatabase, sHostname, iDBPortNum);
		// already calls SetAPISet() and InvalidateConnectSummary()
	}

} // KSQL - default constructor

//-----------------------------------------------------------------------------
KSQL::KSQL (const KSQL& other)
//-----------------------------------------------------------------------------
: m_iFlags(other.m_iFlags)
, m_iWarnIfOverMilliseconds(other.m_iWarnIfOverMilliseconds)
, m_TimingCallback(other.m_TimingCallback)
{
	kDebug (3, "...");

	// this tmp file is used to hold buffered results (if flag F_BufferResults is set):
	m_sTmpResultsFile.Format ("{}/ksql-{}-{}.res", GetTempDir(), kGetPid(), kGetTid());

	if (!other.GetDBUser().empty())
	{
		m_sDBCFile = other.m_sDBCFile;
		SetConnect (other.GetDBType(), other.GetDBUser(), other.GetDBPass(), other.GetDBName(), other.GetDBHost(), other.GetDBPort());
	}

} // KSQL - copy constructor

//-----------------------------------------------------------------------------
KSQL::~KSQL ()
//-----------------------------------------------------------------------------
{
	EndQuery (/*bDestructor=*/true);
	CloseConnection (/*bDestructor=*/true);  // <-- this calls FreeAll()

} // destructor

//-----------------------------------------------------------------------------
void KSQL::FreeAll (bool bDestructor/*=false*/)
//-----------------------------------------------------------------------------
{
	if (kWouldLog(3) && !bDestructor)
	{
		kDebug (3, "FreeAll()...");
		kDebug (3, "  instance cleanup:");
		kDebug (3, "    m_bConnectionIsOpen        = {}", (m_bConnectionIsOpen) ? "true" : "false");
		kDebug (3, "    m_bFileIsOpen              = {}", (m_bFileIsOpen) ? "true" : "false");
		kDebug (3, "    m_bQueryStarted            = {}", (m_bQueryStarted) ? "true" : "false");

		kDebug (3, "  dynamic memory outlook:");
#ifdef DEKAF2_HAS_MYSQL
		kDebug (3, "    m_dMYSQL                   = {}{}", (void*)m_dMYSQL, (m_dMYSQL) ? " (needs to be freed)" : "");
		kDebug (3, "    m_MYSQLRow                 = {}{}", (void*)m_MYSQLRow, (m_MYSQLRow) ? " (needs to be freed)" : "");
		kDebug (3, "    m_dMYSQLResult             = {}{}", (void*)m_dMYSQLResult, (m_dMYSQLResult) ? " (needs to be freed)" : "");
#endif
#ifdef DEKAF2_HAS_ORACLE
		kDebug (3, "    m_dOCI6LoginDataArea       = {}{}", m_dOCI6LoginDataArea, (m_dOCI6LoginDataArea) ? " (needs to be freed)" : "");
		kDebug (3, "    m_dOCI6ConnectionDataArea  = {}{}", m_dOCI6ConnectionDataArea, (m_dOCI6ConnectionDataArea) ? " (needs to be freed)" : "");
		kDebug (3, "    m_dOCI8EnvHandle           = {}{}", m_dOCI8EnvHandle, (m_dOCI8EnvHandle) ? " (needs to be freed)" : "");
		kDebug (3, "    m_dOCI8ErrorHandle         = {}{}", m_dOCI8ErrorHandle, (m_dOCI8ErrorHandle) ? " (needs to be freed)" : "");
		kDebug (3, "    m_dOCI8ServerContext       = {}{}", m_dOCI8ServerContext, (m_dOCI8ServerContext) ? " (needs to be freed)" : "");
#if USE_SERVER_ATTACH
		kDebug (3, "    m_dOCI8ServerHandle        = {}{}", m_dOCI8ServerHandle, (m_dOCI8ServerHandle) ? " (needs to be freed)" : "");
		kDebug (3, "    m_dOCI8Session             = {}{}", m_dOCI8Session, (m_dOCI8Session) ? " (needs to be freed)" : "");
#endif
		kDebug (3, "    m_dOCI8Statement           = {}{}", m_dOCI8Statement, (m_dOCI8Statement) ? " (needs to be freed)" : "");
#endif
//		kDebug (3, "    m_dBufferedColArray        = {}{}", m_dBufferedColArray, (m_dBufferedColArray) ? " (needs to be freed)" : "");
	}

#ifdef DEKAF2_HAS_MYSQL
	if (m_dMYSQL)
	{
		//kfree (m_dMYSQL, "~KSQL:m_dMYSQL");  -- should have been freed by mysql_close()
		m_dMYSQL = nullptr;
	}
#endif

#ifdef DEKAF2_HAS_ORACLE
	if (m_dOCI6LoginDataArea)
	{
		kfree (m_dOCI6LoginDataArea, "~KSQL:m_dOCI6LoginDataArea");
		m_dOCI6LoginDataArea = nullptr;
	}

	if (m_dOCI6ConnectionDataArea)
	{
		kfree (m_dOCI6ConnectionDataArea, "~KSQL:m_dOCI6ConnectionDataArea");
		m_dOCI6ConnectionDataArea = nullptr;
	}

	//	An OCI application should perform the following three steps before it terminates:
	//	1. Delete the user session by calling OCISessionEnd() for each session.
	//	2. Delete access to the data source(s) by calling OCIServerDetach() for each source.
	//	3. Explicitly deallocate all handles by calling OCIHandleFree() for each handle
	//	4. Delete the environment handle, which deallocates all other handles associated with it.
	//	Note: When a parent OCI handle is freed, any child handles associated with it are freed automatically.

	// de-allocate these in reverse order of their allocation in OpenConnection():
	if (m_dOCI8Statement)
	{
		OCIHandleFree (m_dOCI8Statement,     OCI_HTYPE_STMT);
		m_dOCI8Statement = nullptr;
	}
	if (m_dOCI8Session)
	{
		OCISessionEnd ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle, (OCISession*)m_dOCI8Session, m_iOraSessionMode);
		OCIHandleFree (m_dOCI8Session,       OCI_HTYPE_SESSION);
		m_dOCI8Session = nullptr;
	}
	if (m_dOCI8ServerHandle)
	{
		OCIServerDetach ((OCIServer*)m_dOCI8ServerHandle, (OCIError*)m_dOCI8ErrorHandle, m_iOraServerMode);
		OCIHandleFree (m_dOCI8ServerHandle,  OCI_HTYPE_SERVER);
		m_dOCI8ServerHandle = nullptr;
	}
	if (m_dOCI8ServerContext)
	{
		OCIHandleFree (m_dOCI8ServerContext, OCI_HTYPE_SVCCTX);
		m_dOCI8ServerContext = nullptr;
	}
	if (m_dOCI8ErrorHandle)
	{
		OCIHandleFree (m_dOCI8ErrorHandle,   OCI_HTYPE_ERROR);
		m_dOCI8ErrorHandle = nullptr;
	}
	if (m_dOCI8EnvHandle)
	{
		OCIHandleFree (m_dOCI8EnvHandle,     OCI_HTYPE_ENV);
		m_dOCI8EnvHandle = nullptr;
	}

	//OCITerminate (OCI_DEFAULT); // <-- don't do it: it might affect other KSQL instances
#endif

} // FreeAll

#define NOT_IF_ALREADY_OPEN(FUNC) \
	kDebug (3, "..."); \
	if (IsConnectionOpen()) \
	{ \
		m_sLastError.Format ("{}{} cannot change database connection when it's already open", m_sErrorPrefix, FUNC); \
		return (false); \
	}


//-----------------------------------------------------------------------------
bool KSQL::SetConnect (DBT iDBType, KStringView sUsername, KStringView sPassword, KStringView sDatabase, KStringView sHostname/*=nullptr*/, uint16_t iDBPortNum/*=0*/)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN("SetConnect");

	m_iDBType    = iDBType;
	m_iDBPortNum = iDBPortNum;
	m_sUsername  = sUsername;
	m_sPassword  = sPassword;
	m_sDatabase  = sDatabase;
	m_sHostname  = sHostname;

	SetAPISet (API::NONE); // <-- pick the default APIs for this DBType
	InvalidateConnectSummary();
	kDebug (1, ConnectSummary());

	return (true);

} // SetConnect

//-----------------------------------------------------------------------------
bool KSQL::SetConnect (KSQL& Other)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN("SetConnect");

	m_iDBType    = Other.GetDBType();
	m_iDBPortNum = Other.GetDBPort();
	m_sUsername  = Other.GetDBUser();
	m_sPassword  = Other.GetDBPass();
	m_sDatabase  = Other.GetDBName();
	m_sHostname  = Other.GetDBHost();
	SetAPISet (Other.GetAPISet());

	InvalidateConnectSummary();
	kDebug (1, ConnectSummary());

	return (true);

} // SetConnect

//-----------------------------------------------------------------------------
bool KSQL::SetDBType (DBT iDBType)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBType");
	m_iDBType    = iDBType;
	SetAPISet (API::NONE); // <-- pick the default APIs for this DBType
	InvalidateConnectSummary();
	return (true);

} // SetDBType

//-----------------------------------------------------------------------------
bool KSQL::SetDBType (KStringView sDBType)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBType");

#ifdef DEKAF2_HAS_ORACLE
	if (sDBType.starts_with("oracle"))
	{
		return (SetDBType (DBT::ORACLE));
	}
#endif
#ifdef DEKAF2_HAS_MYSQL
	if (sDBType == "mysql")
	{
		return (SetDBType (DBT::MYSQL));
	}
#endif
#ifdef DEKAF2_HAS_SQLITE3
	if (sDBType.starts_with("sqlite"))
	{
		return (SetDBType (DBT::SQLITE3));
	}
#endif
#if defined(DEKAF2_HAS_DBLIB) || defined(DEKAF2_HAS_CTLIB)
	if (sDBType == "sqlserver")
	{
		return (SetDBType (DBT::SQLSERVER));
	}
	if (sDBType == "sybase")
	{
		return (SetDBType (DBT::SYBASE));
	}
#endif

	m_sLastError.Format ("unsupported dbtype: {}", sDBType);
	return (false);

} // SetDBType

//-----------------------------------------------------------------------------
bool KSQL::SetDBUser (KStringView sUsername)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBUser");
	m_sUsername = sUsername;
	InvalidateConnectSummary();
	return (true);

} // SetDBUser

//-----------------------------------------------------------------------------
bool KSQL::SetDBPass (KStringView sPassword)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBPass");
	m_sPassword = sPassword;
	InvalidateConnectSummary();
	return (true);

} // SetDBPass

//-----------------------------------------------------------------------------
bool KSQL::SetDBHost (KStringView sHostname)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBHost");
	m_sHostname = sHostname;
	InvalidateConnectSummary();
	return (true);

} // SetDBHost

//-----------------------------------------------------------------------------
bool KSQL::SetDBName (KStringView sDatabase)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBName");
	m_sDatabase = sDatabase;
	InvalidateConnectSummary();
	return (true);

} // SetDBName

//-----------------------------------------------------------------------------
bool KSQL::SetDBPort (int iDBPortNum)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBPort");
	m_iDBPortNum = static_cast<uint32_t>(iDBPortNum);
	InvalidateConnectSummary();
	return (true);

} // SetDBPort

//-----------------------------------------------------------------------------
bool KSQL::SaveConnect (const KString& sDBCFile)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	{
		DBCFILEv2 dbc_v2;
		dbc_v2.SetDBT(m_iDBType);
		dbc_v2.SetAPI(m_iAPISet);
		dbc_v2.SetDBPort(m_iDBPortNum);

		if (dbc_v2.SetUsername(m_sUsername)
			&& dbc_v2.SetPassword(m_sPassword)
			&& dbc_v2.SetDatabase(m_sDatabase)
			&& dbc_v2.SetHostname(m_sHostname))
		{
			// Store DBC as a version 2 file if we can
			if (!dbc_v2.Save(sDBCFile))
			{
				m_sLastError.Format ("{}SaveConnect(): could not write to '{}'.", m_sErrorPrefix, sDBCFile);
				return (false);
			}
			return true;
		}
	}

	{
		DBCFILEv3 dbc_v3;
		dbc_v3.SetDBT(m_iDBType);
		dbc_v3.SetAPI(m_iAPISet);
		dbc_v3.SetDBPort(m_iDBPortNum);

		if (dbc_v3.SetUsername(m_sUsername)
			&& dbc_v3.SetPassword(m_sPassword)
			&& dbc_v3.SetDatabase(m_sDatabase)
			&& dbc_v3.SetHostname(m_sHostname))
		{
			// Store DBC as a version 2 file if we can
			if (!dbc_v3.Save(sDBCFile))
			{
				m_sLastError.Format ("{}SaveConnect(): could not write to '{}'.", m_sErrorPrefix, sDBCFile);
				return (false);
			}
			return true;
		}
	}
	m_sLastError.Format ("{}SaveConnect(): could not encode all strings into '{}' - too long.", m_sErrorPrefix, sDBCFile);
	return false;

} // SaveConnect

//-----------------------------------------------------------------------------
bool KSQL::DecodeDBCData (KStringView sBuffer, KStringView sDBCFile)
//-----------------------------------------------------------------------------
{
	std::unique_ptr<DBCFileBase> dbc;

	if (sBuffer.starts_with("KSQLDBC1"))
	{
		#ifdef WIN32
		m_sLastError.Format ("old format (DBC1) doesn't work on win32");
		kDebug(GetDebugLevel(), m_sLastError);
		return (false);
		#else
		kDebug((GetDebugLevel() + 1), "old format (1)");
		dbc = std::make_unique<DBCFILEv1>();
		#endif
	}
	else if (sBuffer.starts_with("KSQLDBC2"))
	{
		kDebug((GetDebugLevel() + 1), "compact format (2)");
		dbc = std::make_unique<DBCFILEv2>();
	}
	else if (sBuffer.starts_with("KSQLDBC3"))
	{
		kDebug((GetDebugLevel() + 1), "current format (3)");
		dbc = std::make_unique<DBCFILEv3>();
	}
	else if (sBuffer.starts_with("KSQLDBC"))
	{
		/* It's an unrecognized header but it follows the same pattern as the others:
		   This version of the software can't process the data, but it may be a future version of DBC,
		   so provide a helpful error message.
		*/
		m_sLastError.Format("unrecognized DBC version in DBC file '{}'.", sDBCFile);
		kDebug(GetDebugLevel(), m_sLastError);
		return false;
	}
	else
	{
		m_sLastError.Format ("invalid header on DBC file '{}'.", sDBCFile);
		kDebug(GetDebugLevel(), m_sLastError);
		return (false);
	}

	if (!dbc->SetBuffer(sBuffer))
	{
		m_sLastError.Format ("corrupted DBC file '{}'.", sDBCFile);
		kDebug(GetDebugLevel(), m_sLastError);
		return (false);
	}

	m_iDBType    = dbc->GetDBT();
	m_iAPISet    = dbc->GetAPI();
	m_iDBPortNum = dbc->GetDBPort();
	m_sUsername  = dbc->GetUsername();
	m_sPassword  = dbc->GetPassword();
	m_sDatabase  = dbc->GetDatabase();
	m_sHostname  = dbc->GetHostname();

	return true;

} // DecodeDBCData

//-----------------------------------------------------------------------------
bool KSQL::SetConnect (const KString& sDBCFile, const KString& sDBCFileContent)
//-----------------------------------------------------------------------------
{
	kDebug (3, sDBCFile);

	if (IsConnectionOpen())
	{
		m_sLastError.Format ("can't call SetConnect on an OPEN DATABASE");
		kDebug (GetDebugLevel(), m_sLastError);
		return (false);
	}

	m_iDBType           = DBT::NONE;
	m_iAPISet           = API::NONE;
	m_iDBPortNum        = 0;
	m_sUsername.clear();
	m_sPassword.clear();
	m_sDatabase.clear();
	m_sHostname.clear();

	// take care that the new instance is not a ref on the old one
	if (sDBCFile.data() != m_sDBCFile.data())
	{
		m_sDBCFile.clear();
	}

	if (sDBCFileContent.empty())
	{
		m_sLastError.Format ("empty DBC file '{}'", sDBCFile);
		kDebug (GetDebugLevel(), m_sLastError);
		return (false);
	}

	if (!DecodeDBCData(sDBCFileContent, sDBCFile))
	{
		return false;
	}

	InvalidateConnectSummary();

	m_sDBCFile = sDBCFile;

	return (true);

} // SetConnect

//-----------------------------------------------------------------------------
bool KSQL::LoadConnect (const KString& sDBCFile)
//-----------------------------------------------------------------------------
{
	kDebug (3, sDBCFile);

	if (IsConnectionOpen())
	{
		m_sLastError.Format ("can't call LoadConnect on an OPEN DATABASE");
		kDebug (GetDebugLevel(), m_sLastError);
		return (false);
	}

	kDebug (GetDebugLevel(), "opening '{}'...", sDBCFile);

	auto sBuffer = kReadAll(sDBCFile);

	return SetConnect (sDBCFile, sBuffer);

} // LoadConnect

//-----------------------------------------------------------------------------
bool KSQL::OpenConnection (KStringView sListOfHosts, KStringView sDelimiter/* = ","*/)
//-----------------------------------------------------------------------------
{
	for (auto sDBHost : sListOfHosts.Split(sDelimiter))
	{
		SetDBHost (sDBHost);

		if (OpenConnection())
		{
			kDebug (2, "host {} is up", sDBHost);
			return true;
		}
		else
		{
			kDebug (2, "host {} is down", sDBHost);
		}
	}

	return false;

} // KSQL::OpenConnection

//-----------------------------------------------------------------------------
bool KSQL::OpenConnection ()
//-----------------------------------------------------------------------------
{
    #ifdef DEKAF2_HAS_ORACLE
	static bool s_fOCI8Initialized = false;
	#endif

	kDebug (3, "...");

	if (IsConnectionOpen())
	{
		return (true);
	}

	m_sLastError.clear();
	if (m_iAPISet == API::NONE)
	{
		SetAPISet (API::NONE); // <-- pick the default APIs for this DBType
	}

	InvalidateConnectSummary ();

	if (kWouldLog(GetDebugLevel() + 1))
	{
		kDebug (GetDebugLevel() + 1, "connect info:");
		kDebug (GetDebugLevel() + 1, "  Summary  = {}", ConnectSummary());
		kDebug (GetDebugLevel() + 1, "  DBType   = {}", TxDBType(m_iDBType));
		kDebug (GetDebugLevel() + 1, "  APISet   = {}", TxAPISet(m_iAPISet));
		kDebug (GetDebugLevel() + 1, "  DBUser   = {}", m_sUsername);
		kDebug (GetDebugLevel() + 1, "  DBHost   = {}", m_sHostname);
		kDebug (GetDebugLevel() + 1, "  DBPort   = {}", m_iDBPortNum);
		kDebug (GetDebugLevel() + 1, "  DBName   = {}", m_sDatabase);
		kDebug (GetDebugLevel() + 1, "  Flags    = {} ( {}{}{}{})",
			m_iFlags,
			IsFlag(F_IgnoreSQLErrors)  ? "IgnoreSQLErrors "  : "",
			IsFlag(F_BufferResults)    ? "BufferResults "    : "",
			IsFlag(F_NoAutoCommit)     ? "NoAutoCommit "     : "",
			IsFlag(F_NoTranslations)   ? "NoTranslations "   : "");
	}

	kDebug (GetDebugLevel(), "connecting to: {}...", ConnectSummary());

    #ifdef DEKAF2_HAS_ORACLE
	char*  sOraHome = kGetEnv("ORACLE_HOME","");
	#endif

    #ifdef DEKAF2_HAS_ODBC
	SWORD nResult;
	#endif

    #ifdef DEKAF2_HAS_DBLIB
	LOGINREC* pLogin = nullptr;
	#endif
	
    #ifdef DEKAF2_HAS_MYSQL
	int iPortNum = 0;
	iPortNum = GetDBPort();
	#endif

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case API::NONE:
	// - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}CANNOT CONNECT (no connect info!)", m_sErrorPrefix);
		return (SQLError ());

    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - -
	case API::MYSQL:
	// - - - - - - - - - - - - - - - - -
		if (m_sHostname.empty())
		{
			m_sHostname = "localhost";
		}

		static std::mutex s_OnceInitMutex;
		static bool s_fOnceInitFlag = false;
		if (!s_fOnceInitFlag)
		{
			std::lock_guard<std::mutex> Lock(s_OnceInitMutex);
			if (!s_fOnceInitFlag)
			{
				kDebug (3, "mysql_library_init()...");
				mysql_library_init(0, nullptr, nullptr);
				s_fOnceInitFlag = true;
			}
		}

		kDebug (3, "mysql_init()...");
		m_dMYSQL = mysql_init (nullptr);

		kDebug (3, "mysql_real_connect()...");

		if (!mysql_real_connect (m_dMYSQL, m_sHostname.c_str(), m_sUsername.c_str(), m_sPassword.c_str(), m_sDatabase.c_str(), /*port*/ iPortNum, /*sock*/nullptr,
			/*flag*/CLIENT_FOUND_ROWS)) // <-- this flag corrects the behavior of GetNumRowsAffected()
		{
			m_iErrorNum = mysql_errno (m_dMYSQL);
			m_sLastError.Format ("{}MSQL-{}: {}", m_sErrorPrefix, GetLastErrorNum(), mysql_error(m_dMYSQL));
			if (m_dMYSQL)
			{
				mysql_close(m_dMYSQL);
			}
			return (SQLError ());
		}

		mysql_set_character_set (m_dMYSQL, "utf8"); // by default
		break;
	#endif

    #ifdef DEKAF2_HAS_ODBC
	// - - - - - - - - - - - - - - - - -
	case API::ODBC:
	// - - - - - - - - - - - - - - - - -
		// ODBC initialization:
		if (SQLAllocEnv (&m_Environment) != SQL_SUCCESS)
		{
			m_sLastError.Format ("{}SQLAllocEnv() failed", m_sErrorPrefix);
			return (SQLError ());
		}

		if (SQLAllocConnect (m_Environment, &m_hdbc) != SQL_SUCCESS)
		{
			m_sLastError.Format ("{}SQLAllocConnect() failed", m_sErrorPrefix);
			SQLFreeEnv (m_Environment);
			return (SQLError ());
		}

		m_sConnectOutput.clear();

		snprintf ((char* )m_sConnectString, MAX_ODBCSTR, "DSN={};UID={};PWD={}", m_sDatabase, m_sUsername, m_sPassword);
		if (!m_sHostname.empty())
		{
			KString sAdd;
			sAdd.Format (";HOST={}", m_sHostname);
			m_sConnectString += sAdd;
		}

		// enum enumSQLDriverConnect {
		//   SQL_DRIVER_NOPROMPT,
		//   SQL_DRIVER_COMPLETE,
		//   SQL_DRIVER_PROMPT,
		//   SQL_DRIVER_COMPLETE_REQUIRED
		// }

		if (!CheckODBC (SQLDriverConnect ( // level 1 api
			m_hdbc,
			nullptr,    // hwnd
			m_sConnectString,
			SQL_NTS, // null terminated string
			m_sConnectOutput,
			sizeof(m_sConnectOutput),
			&nResult,
			SQL_DRIVER_NOPROMPT)))
		{
			return (false);
		}

		//if (nResult < 0)
		//	ErrorLog ("{}(warning): SQLDriverConnect() had problems with szConnectOutput\n", g_sProgName);

		if (strcmp ((char* )m_sConnectString, (char* )m_sConnectOutput))
		{
			kDebug (2, "(warning): connection string was tailored by odbc driver.");
			kDebug (2, "(warning): orig connect string: '{}'\n", m_sConnectString);
			kDebug (2, "(warning):  new connect string: '{}'\n", m_sConnectOutput);
		}
		break;
	#endif

    #ifdef DEKAF2_HAS_ORACLE
	// - - - - - - - - - - - - - - - - -
	case API::OCI8:
	// - - - - - - - - - - - - - - - - -
		kDebug (2, "ORACLE_HOME='{}'", sOraHome);
		if (!*sOraHome)
		{
			kDebug (2, "$ORACLE_HOME not set");
		}

		if (!s_fOCI8Initialized)
		{
			// see this excellent write up on Shared Data Mode:
			//   http://www.oradoc.com/ora816/appdev.816/a76975/oci02bas.htm#425685
			// This sounds perfect for DEKAF applications....
			OCIInitialize (OCI_DEFAULT, // OCI_SHARED | OCI_THREADED,
				nullptr,
				nullptr,  // extern "C" (dvoid * (*)(dvoid *, size_t))          nullptr,
				nullptr,  // extern "C" (dvoid * (*)(dvoid *, dvoid *, size_t)) nullptr,
				nullptr); // extern "C" (void (*)(dvoid *, dvoid *))            nullptr);
			s_fOCI8Initialized = true;
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// OCI8 Initialization sequence, extracted from cdemo81.c:
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		OCIEnvInit      ((OCIEnv **)&m_dOCI8EnvHandle, OCI_DEFAULT, 0, nullptr);
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8ErrorHandle,   OCI_HTYPE_ERROR,   0, nullptr);
	#if USE_SERVER_ATTACH
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8ServerContext, OCI_HTYPE_SVCCTX,  0, nullptr);
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8ServerHandle,  OCI_HTYPE_SERVER,  0, nullptr);
		OCIServerAttach ((OCIServer*)m_dOCI8ServerHandle, (OCIError*)m_dOCI8ErrorHandle, (text *)"", strlen(""), m_iOraServerMode);

		/* set attribute server context in the service context */
		OCIAttrSet (m_dOCI8ServerContext, OCI_HTYPE_SVCCTX, m_dOCI8ServerHandle,
		            0, OCI_ATTR_SERVER, (OCIError *) m_dOCI8ErrorHandle);

		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8Session,       OCI_HTYPE_SESSION, 0, nullptr);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// OCISessionBegin(), from ociap.h, Oracle 8.16:
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// To provide credentials for a call to OCISessionBegin(), one of two methods are 
		// supported. The first is to provide a valid username and password pair for 
		// database authentication in the user authentication handle passed to 
		// OCISessionBegin(). This involves using OCIAttrSet() to set the 
		// OCI_ATTR_USERNAME and OCI_ATTR_PASSWORD attributes on the 
		// authentication handle. Then OCISessionBegin() is called with 
		// OCI_CRED_RDBMS.
		// Note: When the authentication handle is terminated using 
		// OCISessionEnd(), the username and password attributes remain 
		// unchanged and thus can be re-used in a future call to OCISessionBegin(). 
		// Otherwise, they must be reset to new values before the next 
		// OCISessionBegin() call.
		// The second type of credentials supported are external credentials. No 
		// attributes need to be set on the authentication handle before calling 
		// OCISessionBegin(). The credential type is OCI_CRED_EXT. This is equivalent 
		// to the Oracle7 `connect /' syntax. If values have been set for 
		// OCI_ATTR_USERNAME and OCI_ATTR_PASSWORD, then these are 
		// ignored if OCI_CRED_EXT is used.

		OCIAttrSet (m_dOCI8Session, OCI_HTYPE_SESSION, m_sUsername, m_sUsername.length(), OCI_ATTR_USERNAME, (OCIError*)m_dOCI8ErrorHandle);
		OCIAttrSet (m_dOCI8Session, OCI_HTYPE_SESSION, m_sPassword, m_sPassword.length(), OCI_ATTR_PASSWORD, (OCIError*)m_dOCI8ErrorHandle);
		OCIAttrSet (m_dOCI8Session, OCI_HTYPE_SESSION, m_sDatabase, m_sDatabase.length(), ?????    -- cannot override ORACLE_SID ???

		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		m_iErrorNum = OCISessionBegin ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle, (OCISession*)m_dOCI8Session, 
			OCI_CRED_RDBMS, m_iOraSessionMode);
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		if (!WasOCICallOK("OpenConnection:OCISessionBegin"))
		{
			if (IsFlag(F_IgnoreSQLErrors))
			{
				kDebug (GetDebugLevel(), m_sLastError);
			}
			else
			{
				kWarning(m_sLastError);
				if (m_TimingCallback)
				{
					m_TimingCallback (*this, /*iMilliseconds=*/0, m_sLastError);
				}
			}
			return (false);
		}

		// associate the session with the server context handle:
		OCIAttrSet (m_dOCI8ServerContext, OCI_HTYPE_SVCCTX, m_dOCI8Session, 0, OCI_ATTR_SESSION, (OCIError*)m_dOCI8ErrorHandle);
	#else
		// This is a much simpler alternative (and gives us an override to ORACLE_SID):
		// OCILogon (OCIEnv *envhp, OCIError *errhp, OCISvcCtx **svchp, 
		//   CONST OraText *username, ub4 uname_len, 
		//   CONST OraText *password, ub4 passwd_len, 
		//   CONST OraText *dbname,   ub4 dbname_len);
		// ...
		// OCILogoff (OCISvcCtx *svchp, OCIError *errhp);

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		m_iErrorNum = OCILogon ((OCIEnv*)m_dOCI8EnvHandle, (OCIError*)m_dOCI8ErrorHandle, (OCISvcCtx**)(&m_dOCI8ServerContext),
			(text*) m_sUsername.c_str(), m_sUsername.length()),
			(text*) m_sPassword.c_str(), m_sPassword.length()),
			(text*) m_sHostname.c_str(), m_sHostname.length()));  // FYI: m_sHostname holds the ORACLE_SID
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		if (!WasOCICallOK("OpenConnection:OCILogon"))
		{
			return (SQLError());
		}
	#endif

		// now allocate the one and only one statement (1 per KSQL instance):
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8Statement,     OCI_HTYPE_STMT,    0, nullptr);

		// sanity checks:
		kAssert (m_dOCI8EnvHandle,     "m_dOCI8EnvHandle is still nullptr after OpenConnection() logic");
		kAssert (m_dOCI8ErrorHandle,   "m_dOCI8ErrorHandle is still nullptr after OpenConnection() logic");
		kAssert (m_dOCI8ServerContext, "m_dOCI8ServerContext is still nullptr after OpenConnection() logic");
		#ifdef USE_SERVER_ATTACH
		kAssert (m_dOCI8ServerHandle,  "m_dOCI8ServerHandle is still nullptr after OpenConnection() logic");
		kAssert (m_dOCI8Session,       "m_dOCI8Session is still nullptr after OpenConnection() logic");
		#endif
		kAssert (m_dOCI8Statement,     "m_dOCI8Statement is still nullptr after OpenConnection() logic");
		break;

	// - - - - - - - - - - - - - - - - -
	case API::OCI6:
	// - - - - - - - - - - - - - - - - -
		kDebug (GetDebugLevel(), "ORACLE_HOME='{}'", sOraHome);
		if (!*sOraHome)
		{
			kDebug (GetDebugLevel(), "$ORACLE_HOME not set");
		}

		m_dOCI6LoginDataArea      = (Lda_Def*) kmalloc (sizeof(Lda_Def), "KSQL:m_dOCI6LoginDataArea");  // <-- oracle login data area
		m_dOCI6ConnectionDataArea = (Cda_Def*) kmalloc (sizeof(Cda_Def), "KSQL:m_dOCI6ConnectionDataArea");  // <-- oracle connection data area

		// OCI Logon Modes for olog call:
		// #define OCI_LM_DEF 0                           /* default login */
		// #define OCI_LM_NBL 1                      /* non-blocking logon */

		// Attach to ORACLE as user/pass = DBName/Password
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		m_iErrorNum = olog ((Lda_Def*)m_dOCI6LoginDataArea, (ub1*)m_sOCI6HostDataArea, 
				(text *)m_sUsername, strlen(m_sUsername),
		        (text *)m_sPassword, strlen(m_sPassword),
				(text *)m_sHostname, strlen(m_sHostname), OCI_LM_DEF);  // FYI: m_sHostname holds the ORACLE_SID
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		if (!WasOCICallOK("OpenConnection:olog"))
		{
			return (SQLError());
		}

		kDebug (GetDebugLevel()+1, "OCI6: connect through olog() succeeded.\n");
	
		// Only one cursor is associated with each KSQL instance, so open it now:
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		m_iErrorNum = oopen ((Cda_Def*)m_dOCI6ConnectionDataArea, (Lda_Def*)m_dOCI6LoginDataArea, nullptr, -1, -1, nullptr, -1);
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		if (!WasOCICallOK("OpenConnection:oopen"))
		{
			return (SQLError());
		}

		break;
	#endif

    #ifdef DEKAF2_HAS_DBLIB
	// - - - - - - - - - - - - - - - - -
	case API::DBLIB:
	// - - - - - - - - - - - - - - - - -
		dbinit();
		dberrhandle (nullptr); // TODO: DBLIB BLOCKED until I figure out how to bind a member function here
		dbmsghandle (nullptr);

		pLogin = dblogin();
		DBSETLUSER (pLogin, GetDBUser());
		DBSETLPWD  (pLogin, GetDBPass());
		DBSETLAPP  (pLogin, "KSQL");

		m_pDBPROC = dbopen(pLogin, GetDBHost());
		if (!m_pDBPROC)
		{
			dbloginfree(pLogin);
			pLogin = nullptr;
			if (m_sLastError.empty())
			{
				m_sLastError.Format ("{}DBLIB: connection failed.", m_sErrorPrefix);
			}
			return (false); // connection failed	
		}

		if (!GetDBName().empty())
		{
			kDebug (GetDebugLevel()+1, "use {}", GetDBName());
			dbuse(m_pDBPROC, GetDBName());
		}
		break;
	#endif

	#ifdef DEKAF2_HAS_CTLIB
	// - - - - - - - - - - - - - - - - -
	case API::CTLIB:
	// - - - - - - - - - - - - - - - - -
		if (!ctlib_login())
		{
			if (m_sLastError.empty())
			{
				m_sLastError.Format ("{}CTLIB: connection failed.", m_sErrorPrefix);
			}
			return (SQLError ());
		}
		break;
	#endif

	// - - - - - - - - - - - - - - - - -
	case API::INFORMIX:
	default:
	// - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}API Set not coded yet ({})", m_sErrorPrefix, TxAPISet(GetAPISet()));
		return (SQLError ());
	}

	m_bConnectionIsOpen = true;
		
	kDebug (3, "connection is now open...");

	if (!IsFlag(F_NoTranslations))
	{
		BuildTranslationList (m_TxList);
	}

    #ifdef DEKAF2_HAS_DBLIB
	if (pLogin)
	{
		dbloginfree (pLogin);
		pLogin = nullptr;
	}
	#endif

	return (IsConnectionOpen());

} // OpenConnection

//-----------------------------------------------------------------------------
void KSQL::CloseConnection (bool bDestructor/*=false*/)
//-----------------------------------------------------------------------------
{
	if (!bDestructor)
	{
		kDebug (3, "...");
	}

	m_sLastError.clear();

	if (IsConnectionOpen())
	{
		if (!bDestructor)
		{
			kDebug (GetDebugLevel(), "disconnecting from {}...", ConnectSummary());
		}

		ResetErrorStatus ();

		switch (m_iAPISet)
		{
        #ifdef DEKAF2_HAS_MYSQL
		// - - - - - - - - - - - - - - - - -
		case API::MYSQL:
		// - - - - - - - - - - - - - - - - -
			if (!bDestructor)
			{
				kDebug (3, "mysql_close()...");
			}
			mysql_close (m_dMYSQL);
			break;
		#endif

        #ifdef DEKAF2_HAS_ORACLE
		// - - - - - - - - - - - - - - - - -
		case API::OCI8:
		// - - - - - - - - - - - - - - - - -
			#ifdef USE_SERVER_ATTACH
			OCISessionEnd   ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle, (OCISession*)m_dOCI8Session, OCI_DEFAULT);
			OCIServerDetach ((OCIServer*)m_dOCI8ServerHandle,  (OCIError*)m_dOCI8ErrorHandle, OCI_DEFAULT);
			#else
			OCILogoff ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle);
			#endif
			break;

		// - - - - - - - - - - - - - - - - -
		case API::OCI6:
		// - - - - - - - - - - - - - - - - -
			oclose ((Cda_Def*)m_dOCI6ConnectionDataArea);   // <-- close the cursor
			ologof ((Lda_Def*)m_dOCI6LoginDataArea);   // <-- close the connection      -- this sometimes causes a BUS ERROR -- why??
			break;
		#endif

        #ifdef DEKAF2_HAS_DBLIB
		// - - - - - - - - - - - - - - - - -
		case API::DBLIB:
		// - - - - - - - - - - - - - - - - -
			dbexit ();
			break;
		#endif

		#ifdef DEKAF2_HAS_CTLIB
		// - - - - - - - - - - - - - - - - -
		case API::CTLIB:
		// - - - - - - - - - - - - - - - - -
			ctlib_logout ();
			break;
		#endif

		// - - - - - - - - - - - - - - - - -
		case API::INFORMIX:
		case API::ODBC:
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("unsupported API Set ({})", TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
		}
	}

	FreeAll (bDestructor);

	m_bConnectionIsOpen = false;

} // CloseConnection

//-----------------------------------------------------------------------------
void KSQL::SetErrorPrefix (KStringView sPrefix, uint32_t iLineNum/*=0*/)
//-----------------------------------------------------------------------------
{
	if (iLineNum && !sPrefix.empty())
	{
		m_sErrorPrefix.Format ("{}:{}: ", sPrefix, iLineNum);
	}
	else if (!sPrefix.empty())
	{
		m_sErrorPrefix.Format ("{}: ", sPrefix);
	}
	else
	{
		ClearErrorPrefix();
	}

} // SetErrorPrefix

enum ViewInString
{
	Unrelated,
	Same,
	SameStart,
	Substring
};

//-----------------------------------------------------------------------------
ViewInString kSameBuffer(const KString& sStr, KStringView svView)
//-----------------------------------------------------------------------------
{
	if (svView.data() >= sStr.data() + sStr.size())
	{
		return Unrelated;
	}
	if (svView.data() + svView.size() < sStr.data())
	{
		return Unrelated;
	}
	if (sStr.data() == svView.data())
	{
		if (sStr.size() == svView.size())
		{
			return Same;
		}
		else
		{
			return SameStart;
		}
	}
	else
	{
		return Substring;
	}
}

//-----------------------------------------------------------------------------
inline
bool kSameBufferStart(const KString& sStr, KStringView svView)
//-----------------------------------------------------------------------------
{
	return (sStr.data() == svView.data());
}

//-----------------------------------------------------------------------------
inline
void CopyIfNotSame(KString& sTarget, KStringView svView)
//-----------------------------------------------------------------------------
{
	if (!kSameBufferStart(sTarget, svView))
	{
		sTarget = svView;
	}
}

//-----------------------------------------------------------------------------
bool KSQL::ExecLastRawSQL (Flags iFlags/*=0*/, KStringView sAPI/*="ExecLastRawSQL"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}\n", sAPI, m_sLastSQL);
	}

	if (IsFlag(F_ReadOnlyMode) && ! IsSelect(m_sLastSQL))
	{
		m_sLastError.Format ("attempt to perform a non-query on a READ ONLY db connection:\n{}", m_sLastSQL);
		return false;
	}

	m_iNumRowsAffected  = 0;
	EndQuery();

	bool   bOK          = false;
	int    iRetriesLeft = NUM_RETRIES;
	int    iSleepFor    = 0;

	KStopTime Timer;

	m_SQLStmtStats.Collect(m_sLastSQL);

	while (!bOK && iRetriesLeft && !m_bDisableRetries)
	{
		ResetErrorStatus ();

		switch (m_iAPISet)
		{
			#ifdef DEKAF2_HAS_MYSQL
			// - - - - - - - - - - - - - - - - -
			case API::MYSQL:
			// - - - - - - - - - - - - - - - - -
				{
					if (!m_dMYSQL)
					{
						kDebug (1, "lost m_dMYSQL pointer.  Reopening connection ...");

						CloseConnection();
						OpenConnection();

						if (!m_dMYSQL)
						{
							kDebug (1, "failed.  aborting query or SQL:\n{}", m_sLastSQL);
							break;
						}
					}

					kDebug (3, "mysql_query(): m_dMYSQL is {}, SQL is {} bytes long", m_dMYSQL ? "not null" : "nullptr", m_sLastSQL.size());
					if (mysql_query (m_dMYSQL, m_sLastSQL.c_str()))
					{
						m_iErrorNum = mysql_errno (m_dMYSQL);
						m_sLastError.Format ("{}MSQL-{}: {}", m_sErrorPrefix, GetLastErrorNum(), mysql_error(m_dMYSQL));
						break;
					}

					m_iNumRowsAffected = 0;
					kDebug (3, "mysql_affected_rows()...");
					my_ulonglong iNumRows = mysql_affected_rows (m_dMYSQL);
					if ((uint64_t)iNumRows != (uint64_t)(-1))
					{
						m_iNumRowsAffected = (uint64_t) iNumRows;
					}

					kDebug (3, "mysql_insert_id()...");
					my_ulonglong iNewID = mysql_insert_id (m_dMYSQL);
					m_iLastInsertID = (uint64_t) iNewID;

					if (m_iLastInsertID)
					{
						kDebug (GetDebugLevel(), "last insert ID = {}", m_iLastInsertID);
					}
					else
					{
						kDebug (3, "no insert ID.");
					}

					bOK = true;
				}
				break;
			#endif

			#ifdef DEKAF2_HAS_ORACLE
			// - - - - - - - - - - - - - - - - -
			case API::OCI8:
			// - - - - - - - - - - - - - - - - -
				kDebug (3, "OCIStmtPrepare...");
				m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
											 (text*)m_sLastSQL.data(), m_sLastSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);

				if (!WasOCICallOK("ExecSQL:OCIStmtPrepare"))
				{
					break;
				}

				// Note: Using the OCI_COMMIT_ON_SUCCESS mode of the OCIExecute() call, the
				// application can selectively commit transactions at the end of each
				// statement execution, saving an extra roundtrip by calling OCITransCommit().
				kDebug (3, "OCIStmtExecute...");
				m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
								 (CONST OCISnapshot *) nullptr, (OCISnapshot *) nullptr, IsFlag(F_NoAutoCommit) ? 0 : OCI_COMMIT_ON_SUCCESS);

				if (!WasOCICallOK("ExecSQL:OCIStmtExecute"))
				{
					break;
				}

				m_iNumRowsAffected = 0;
				m_iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumRowsAffected,
					0, OCI_ATTR_ROW_COUNT, (OCIError*)m_dOCI8ErrorHandle);

				bOK = true;
				break;

			// - - - - - - - - - - - - - - - - -
			case API::OCI6:
			// - - - - - - - - - - - - - - - - -
				// let the RDBMS parse the SQL expression:
				kDebug (3, "oparse...");
				m_iErrorNum = oparse ((Cda_Def*)m_dOCI6ConnectionDataArea, (text *)m_sLastSQL.c_str(), (sb4) -1, (sword) PARSE_NO_DEFER, (ub4) PARSE_V7_LNG);
				if (!WasOCICallOK("ExecSQL:oparse"))
				{
					break;
				}

				// execute it:
				kDebug (3, "oexec...");
				m_iErrorNum = oexec ((Cda_Def*)m_dOCI6ConnectionDataArea);
				if (!WasOCICallOK("ExecSQL:oexec"))
				{
					break;
				}

				// now issue a "commit" or it will be rolled back automatically:
				if (!IsFlag(F_NoAutoCommit))
				{
					m_iErrorNum = ocom ((Lda_Def*)m_dOCI6LoginDataArea);
					if (!WasOCICallOK("ExecSQL:ocom"))
					{
						m_sLastError += "\non auto commit.";
						break;
					}
				}

				// ORACLE TODO: deal with SEQUENCE values
				m_iNumRowsAffected = ((Cda_Def*)m_dOCI6ConnectionDataArea)->rpc; // ocidfn.h: rows processed count

				ocan ((Cda_Def*)m_dOCI6ConnectionDataArea);  // <-- let cursor free it's resources

				bOK = true;
				break;
			#endif

			#ifdef DEKAF2_HAS_CTLIB
			// - - - - - - - - - - - - - - - - -
			case API::CTLIB:
			// - - - - - - - - - - - - - - - - -
				if (!ctlib_is_initialized())
				{
					kDebug (1, "lost CTLIB connection.  Reopening connection ...");

					CloseConnection();
					OpenConnection();

					if (!ctlib_is_initialized())
					{
						kDebug (1, "failed.  aborting query or SQL:\n{}", m_sLastSQL);
						break; // once
					}
				}

				bOK = ctlib_execsql (m_sLastSQL);

				if (!bOK)
				{
					kDebug (3, "ctlib_execsql returned false");
				}
				break;
			#endif

			// - - - - - - - - - - - - - - - - -
			case API::DBLIB:
			case API::INFORMIX:
			case API::ODBC:
			default:
			// - - - - - - - - - - - - - - - - -
				kWarning ("unsupported API Set ({})", TxAPISet(m_iAPISet));
				kCrashExit (CRASHCODE_DEKAFUSAGE);

		} // switch

		if (!bOK)
		{
			if (!(iRetriesLeft--) || !PreparedToRetry())
			{
				iRetriesLeft = 0; // stop looping
			}
			else
			{
				// the first retry should have no delay:
				if (iSleepFor)
				{
					kDebug (GetDebugLevel(), "sleeping {} seconds...", iSleepFor);
					std::this_thread::sleep_for(std::chrono::seconds(iSleepFor));
				}

				// increase iSleepFor in case we end up here again:
				if (!iSleepFor)
				{
					iSleepFor = 1;  // second retry should have a delay of one second
				}
				else
				{
					iSleepFor *= 2; // double it each time after that
				}
			}
		}

	} // retry loop

	ClearErrorPrefix();

	if (!bOK) {
		return (SQLError());
	}

	LogPerformance (Timer.elapsed<std::chrono::milliseconds>().count(), /*bIsQuery=*/false);

	return (bOK);

} // ExecLastRawSQL

//-----------------------------------------------------------------------------
void KSQL::LogPerformance (uint64_t iMilliseconds, bool bIsQuery)
//-----------------------------------------------------------------------------
{
	if (!(GetFlags() & F_NoKlogDebug))
	{
		KString sThreshold;
		if (m_iWarnIfOverMilliseconds)
		{
			sThreshold += kFormat (", warning set to {} ms", kFormNumber(m_iWarnIfOverMilliseconds));
			kDebug (GetDebugLevel(), "took: {} ms{}", kFormNumber(iMilliseconds), sThreshold);
		}
	}

	if (m_iWarnIfOverMilliseconds && (iMilliseconds > m_iWarnIfOverMilliseconds))
	{
		KString sWarning;
		sWarning.Format (
				"KSQL: warning: statement took longer than {} ms (took {}) against db {}:\n"
				"{}\n",
					kFormNumber(m_iWarnIfOverMilliseconds),
					kFormNumber(iMilliseconds),
					ConnectSummary(),
					m_sLastSQL);

		if (!bIsQuery)
		{
			sWarning += kFormat (
			"KSQL: {} rows affected.\n",
				kFormNumber(m_iNumRowsAffected));
		}

		if (m_TimingCallback)
		{
			m_TimingCallback(*this, iMilliseconds, sWarning);
		}
		else if (m_fpPerformanceLog)
		{
			kDebugLog (1, "writing warning to special log file:\n{}", sWarning);
			fprintf (m_fpPerformanceLog, "%s", sWarning.c_str());
			fflush (m_fpPerformanceLog);
		}
		else
		{
			kWarningLog (sWarning);
		}
	}

} // LogPerformance

//-----------------------------------------------------------------------------
bool KSQL::PreparedToRetry ()
//-----------------------------------------------------------------------------
{
	// Note: this is called every time m_sLastError and m_iLastErrorNum have
	// been set by an RDBMS error.

	bool bConnectionLost { false };
	bool bDisplayWarning { false };

	switch (m_iDBType)
	{
#ifdef DEKAF2_HAS_MYSQL
		case DBT::MYSQL:
			switch (m_iErrorNum)
			{
#ifdef CR_SERVER_GONE_ERROR
				case CR_SERVER_GONE_ERROR:
					bConnectionLost = true;
					break;

				case CR_SERVER_LOST:
					bDisplayWarning = true;
					bConnectionLost = true;
					break;
#else
				case 2006:
					bConnectionLost = true;
					break;

				case 2013:
					bDisplayWarning = true;
					bConnectionLost = true;
					break;
#endif
				}
				break;
#endif

#if defined(DEKAF2_HAS_DBLIB) || defined(DEKAF2_HAS_CTLIB)
			case DBT::SQLSERVER:
			case DBT::SYBASE:
				switch (m_iErrorNum)
				{
					// SQL-01205: State 45, Severity 13, Line 1, Transaction (Process ID 95) was deadlocked on lock resources with
					//  another process and has been chosen as the deadlock victim. Rerun the transaction.
					case  1205:
					// CTLIB-20006: Write to SQL Server failed.
					case 20006:
						bConnectionLost = true;
						break;

					case 20004:
					// CTLIB-20004: Read from SQL server failed.
						bDisplayWarning = true;
						bConnectionLost = true;
						break;

					case 0: // spurious error
						kDebug(1, "CTLIB has a clogged queue, check where it was not emptied!");
						ctlib_flush_results();
						ctlib_clear_errors();
						return true; // simply repeat, without reconnect
				}
				break;
#endif

			default:
				break;
	}

	if (bConnectionLost)
	{
		if (IsFlag(F_IgnoreSQLErrors) || !bDisplayWarning)
		{
			kDebug (GetDebugLevel(), GetLastError());
			kDebug (GetDebugLevel(), "automatic retry now in progress...");
		}
		else
		{
			kWarning (GetLastError());
			kWarning ("automatic retry now in progress...");

			if (m_TimingCallback)
			{
				m_TimingCallback (*this, /*iMilliseconds=*/0, kFormat ("{}\n{}", GetLastError(), "automatic retry now in progress..."));
			}
		}

		CloseConnection ();
		OpenConnection ();

		if (IsConnectionOpen())
		{
			kDebug (GetDebugLevel(), "new connection looks good.");
		}
		else if (IsFlag(F_IgnoreSQLErrors)) {
			kDebug (GetDebugLevel(), "NEW CONNECTION FAILED.");
		}
		else
		{
			kWarning ("NEW CONNECTION FAILED.");
			if (m_TimingCallback)
			{
				m_TimingCallback (*this, /*iMilliseconds=*/0, "NEW CONNECTION FAILED.");
			}
		}

		return (true); // <-- we are now prepare for automatic retry
	}
	else
	{
		kDebug (3, "FYI: will not retry: {}: {}", GetLastErrorNum(), GetLastError());
		return (false); // <-- no not retry
	}

} // PreparedToRetry

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ParseSQL (KStringView sFormat, ...)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	m_iLastInsertID = 0;
	EndQuery ();
	if (!OpenConnection())
	{
		return (false);
	}

	kDebug (3, "format={}", sFormat);

	va_list vaArgList;
	va_start (vaArgList, sFormat);
	m_sLastSQL.FormatV(sFormat, vaArgList);
	va_end (vaArgList);

	if (!IsFlag(F_NoTranslations))
	{
		DoTranslations (m_sLastSQL, m_iDBType);
	}

	return (ParseRawSQL (m_sLastSQL, 0, "ParseSQL"));

} // ParseSQL

//-----------------------------------------------------------------------------
bool KSQL::ParseRawSQL (KStringView sSQL, int64_t iFlags/*=0*/, KStringView sAPI/*="ParseRawSQL"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}{}\n", sAPI, (sSQL.Contains("\n")) ? "\n" : "", sSQL);
	}

	if (IsFlag(F_ReadOnlyMode) && ! IsSelect(m_sLastSQL))
	{
		m_sLastError.Format ("attempt to perform a non-query on a READ ONLY db connection:\n{}", m_sLastSQL);
		return false;
	}

	CopyIfNotSame(m_sLastSQL, sSQL);
	ResetErrorStatus ();

	switch (m_iAPISet)
	{
		// - - - - - - - - - - - - - - - - -
		case API::OCI8:
		// - - - - - - - - - - - - - - - - -
			m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
										 (text*)sSQL.data(), sSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);
			if (!WasOCICallOK("ParseSQL:OCIStmtPrepare"))
			{
				return (SQLError (/*fForceError=*/false));
			}

			m_bStatementParsed = true;
			m_iMaxBindVars     = 5;
			m_idxBindVar       = 0;
			break;

		// - - - - - - - - - - - - - - - - -
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("unsupported API Set ({}={})", m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	return (true);

} // ParseRawSQL
#endif

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ExecParsedSQL ()
//-----------------------------------------------------------------------------
{
	if (!m_bStatementParsed)
	{
		kWarning ("ParseSQL() or ParseQuery() was not called yet.");
		kCrashExit (CRASHCODE_DEKAFUSAGE);
		return (false);
	}

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
		// - - - - - - - - - - - - - - - - -
		case API::OCI8:
		// - - - - - - - - - - - - - - - - -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
							 (CONST OCISnapshot *) nullptr, (OCISnapshot *) nullptr, IsFlag(F_NoAutoCommit) ? 0 : OCI_COMMIT_ON_SUCCESS);
			if (!WasOCICallOK("ParseSQL:OCIStmtExecute"))
			{
				return (SQLError (/*fForceError=*/false));
			}

			m_iNumRowsAffected = 0;
			m_iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumRowsAffected,
				0, OCI_ATTR_ROW_COUNT, (OCIError*)m_dOCI8ErrorHandle);

			break;

		// - - - - - - - - - - - - - - - - -
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("unsupported API Set ({}={})", m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	m_idxBindVar = 0; // <-- reset index into m_OCIBindArray[]

	ClearErrorPrefix();

	return (true);

} // ExecParsedSQL
#endif

//-----------------------------------------------------------------------------
bool KSQL::ExecSQLFile (KStringViewZ sFilename)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	EndQuery ();
	if (!IsConnectionOpen() && !OpenConnection())
	{
		return (false);
	}

	kDebug (GetDebugLevel(), "{}\n", sFilename);

	if (!kFileExists (sFilename))
	{
		m_sLastError.Format ("{}:ExecSQLFile(): file '{}' does not exist.", m_sErrorPrefix, sFilename);
		return (SQLError());
	}

	KInFile File(sFilename);
	if (!File.is_open())
	{
		m_sLastError.Format ("{}ExecSQLFile(): {} could not read from file '{}'", m_sErrorPrefix, kGetWhoAmI(), sFilename);
		SQLError();
		return (false);
	}

	if (IsFlag(F_ReadOnlyMode))
	{
		m_sLastError.Format ("attempt to execute a SQL file on a READ ONLY db connection: {}", sFilename);
		return false;
	}

	// trim all white space at end of lines (we keep the leading white space here to
	// allow for better log statement alignment)
	File.SetReaderRightTrim("\r\n\t ");

	m_sLastSQL.clear();

	SQLFileParms Parms;

	// SPECIAL DIRECTIVES:
	//   //DROP|                      -- drop statement (set ignore db errors flag for that statement)
	//   //define {{FOO}} definition  -- simple macro
	//   //MYS|                       -- line applies to MySQL only
	//   //ORA|                       -- line applies to Oracle only
	//   //SYB|                       -- line applies to Sybase only
	//   //MSS|                       -- line applies to (MS)SQLServer only
	KString sLeader = (m_iDBType == DBT::SQLSERVER) ? "MSS" : TxDBType(m_iDBType).ToUpper();
	if (sLeader.size() > 3)
	{
		sLeader.erase(3);
	}
	sLeader += '|';

	// Special strings for supporting the "delimiter" statement
	constexpr KStringView sDefaultDelimiter = ";";
	KString sDelimiter = sDefaultDelimiter;
	KString sLine;

	// Loop through and process every line of the SQL file
	while (!Parms.fDone && File.ReadLine(sLine))
	{
		bool fFoundSpecialLeader = false;
		++Parms.iLineNum;

		kDebug (GetDebugLevel()+1, "[{}] {}", Parms.iLineNum, sLine);

		KStringView sStart(sLine);
		// remove all leading white space
		sStart.TrimLeft();

		if (sStart.starts_with("#include"_ksv))
		{
			auto Parts = sStart.Split("\""_ksv, ""_ksv);
			if (Parts.size() < 2)
			{
				m_sLastError.Format ("{}:{}: malformed include directive (file should be enclosed in double quotes).", sFilename, Parms.iLineNum);
				return (SQLError());
			}

			// Allow for ${myvar} variable expansion on include filenames
			KString sIncludedFile = Parts[1];

			static constexpr KStringView s_sOpen  = "${";
			static constexpr KStringView s_sClose = "}";

			kReplaceVariables(sIncludedFile, s_sOpen, s_sClose, true /* bQueryEnvironment */);

			// TODO check if test for escaped ${ is needed ( "\${" )

			if (!kFileExists (sIncludedFile))
			{
				m_sLastError.Format ("{}:{}: missing include file: {}", sFilename, Parms.iLineNum, sIncludedFile);
				return (SQLError());
			}
			kDebug (GetDebugLevel(), "{}, line {}: recursing to include file: {}", sFilename, Parms.iLineNum, sIncludedFile);
			if (!ExecSQLFile (sIncludedFile))
			{
				// m_sLastError is already set
				return (SQLError());
			}
			continue; // done with #include
		}

		// look for special directives:
		while (sStart.starts_with("//"_ksv))
		{
			kDebug (3, " line {}: seeing if this is a special directive or just a comment..", Parms.iLineNum);

			if (sStart.starts_with(sLeader))
			{
				kDebug (GetDebugLevel()+1, " line {}: contains {}-specific code (included)", Parms.iLineNum, TxDBType(m_iDBType));

				sStart.remove_prefix(sLeader.size());
				sStart.TrimLeft();
				fFoundSpecialLeader = true;
			}
			else if (sStart.starts_with("//DROP|"_ksv))
			{
				kDebug (GetDebugLevel()+1, " line {}: is a drop statement", Parms.iLineNum);
				sStart.remove_prefix(strlen("//DROP|"));
				sStart.TrimLeft();
				fFoundSpecialLeader = true;
				Parms.fDropStatement = true;
			}
			else if (sStart.starts_with("//define"_ksv))
			{
				kDebug (GetDebugLevel()+1, " line {}: is a macro definition", Parms.iLineNum);

				sStart.remove_prefix(strlen("//define"));
				sStart.TrimLeft();
				if (!sStart.starts_with("{{"_ksv))
				{
					m_sLastError.Format ("{}:{}: malformed token: {}", sFilename, Parms.iLineNum, sStart);
					return (false);
				}
				auto iEnd = sStart.find("}}"_ksv);
				if (iEnd == KStringView::npos)
				{
					m_sLastError.Format ("{}:{}: malformed {{token}}: {}", sFilename, Parms.iLineNum, sStart);
					return (false);
				}
				KStringView sValue = sStart.substr(iEnd+strlen("}}")).Trim();
				m_TxList.Add (sStart.substr(strlen("{{")), sValue);
				sStart.clear();
				fFoundSpecialLeader = true;
			}
			else
			{
				kDebug (3, " line {}: done processing directive[s]", Parms.iLineNum);
				break; // quit processing directives
			}
		}

		if (sStart.empty())
		{
			continue;
		}

		// allow full-line comments (end of line comments are a pain because of quoted inserts and updates):
		if (sStart.starts_with("//")      // C++
		 || sStart.starts_with("#")       // MySQL
		 || sStart.starts_with("--")      // Oracle
		 || sStart.starts_with("rem ")    // Oracle
		 || sStart.starts_with("REM ")    // Oracle
		)
		{
			continue; // <-- skip full line comment lines
		}

		// the "delimiter" is currently only used for stored procedure files
		if (sStart.starts_with("delimiter"))
		{
			sDelimiter = sStart.substr(strlen("delimiter"));
			sDelimiter.Trim();
			if (sDelimiter.empty())
			{
				sDelimiter = sDefaultDelimiter;
			}
			kDebug (GetDebugLevel(), "line {} setting delimiter={}\n", Parms.iLineNum, sDelimiter);
			continue; // <-- skip delimiter statement after extracting the delimiter
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// trimming must be done very carefully:
		//  * we could be in the middle of a quoted string
		//  * don't trim LEFT b/c it affects SQL statement readability
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		bool fFoundDelimiter = false;

		while ((sStart.size()) >= sDelimiter.size() &&
		    (  (sStart.back() <= ' ')
		    || (sStart.ends_with(sDelimiter))))
		{
			if (sStart.ends_with(sDelimiter))
			{
				fFoundDelimiter = true;
				sStart.remove_suffix(sDelimiter.size());
				sLine.remove_suffix(sDelimiter.size());
			}
			else
			{
				sStart.remove_suffix(1);
				sLine.remove_suffix(1);
			}
		}

		if (!Parms.iLineNumStart || m_sLastSQL.empty())
		{
			Parms.iLineNumStart = Parms.iLineNum;
		}

		if (fFoundSpecialLeader)
		{
			m_sLastSQL += sStart;  // <-- add the line to the SQL statement buffer
		}
		else
		{
			m_sLastSQL += sLine;    // <-- add the line to the SQL statement buffer [with leading whitespace]
		}

		m_sLastSQL += "\n";  // <-- maintain readability by replacing the newline

		// execute the SQL statement everytime we encounter a semicolon at the end:
		if (fFoundDelimiter)
		{
			ExecSQLFileGo (sFilename, Parms); // assumes m_sLastSQL contains the statement
		}

	} // while getting lines

	// execute the last SQL statement (in case they forgot the semicolon):
	if (!Parms.fDone && !m_sLastSQL.empty())
	{
		ExecSQLFileGo (sFilename, Parms);
	}

	kDebug (GetDebugLevel(), "{}: TotalRowsAffected = {}", sFilename, Parms.iTotalRowsAffected);
	m_iNumRowsAffected = Parms.iTotalRowsAffected;

	ClearErrorPrefix ();

	return (Parms.fOK);

} // KSQL::ExecSQLFile

//-----------------------------------------------------------------------------
void KSQL::ExecSQLFileGo (KStringView sFilename, SQLFileParms& Parms)
//-----------------------------------------------------------------------------
{
	// assumes m_sLastSQL contains the statement to be executed

	++Parms.iStatement;

	if (Parms.fDropStatement)
	{
		kDebug (GetDebugLevel(), "setting F_IgnoreSQLErrors");
		SetFlags (F_IgnoreSQLErrors);
	}

	SetErrorPrefix (sFilename, Parms.iLineNumStart);

	kDebug (GetDebugLevel()+1, "{}: statement # {}:\n{}\n", sFilename, Parms.iStatement, m_sLastSQL);

	if (m_sLastSQL.empty())
	{
		kDebug (GetDebugLevel()+1, "{}: statement # {} is EMPTY.", sFilename, Parms.iStatement);
	}
	else if (m_sLastSQL == "exit\n"   || m_sLastSQL == "quit\n")
	{
		kDebug (GetDebugLevel()+1, "{}: statement # {} is '{}' (stopping).", sFilename, Parms.iStatement, m_sLastSQL);
		Parms.fOK = Parms.fDone = true;
	}
	else if (IsSelect(m_sLastSQL))
	{
		kDebug (3, "{}: statement # {} is a QUERY...", sFilename, Parms.iStatement);

		if (!IsFlag(F_NoTranslations))
		{
			DoTranslations (m_sLastSQL);
		}
	
		if (!ExecLastRawQuery (0, "ExecSQLFile") && !Parms.fDropStatement)
		{
			Parms.fOK   = false;
			Parms.fDone = true;
		}
	}
	else if (m_sLastSQL.starts_with("analyze") || m_sLastSQL.starts_with("ANALYZE"))
	{
		// "analyze table" is the MYSQL command for updating statistics on a table.
		kDebug (3, "{}: statement # {} is ANALYZE...", sFilename, Parms.iStatement);

		if (!IsFlag(F_NoTranslations))
		{
			DoTranslations (m_sLastSQL);
		}

		// analyze returns row output like a select statement, so we need to execute it as a
		// query, but we need to set the F_IgnoreSelectKeyword flag. row output will look 
		// something like this:
		// +--------------------------+---------+----------+----------+
		// | Table                    | Op      | Msg_type | Msg_text |
		// +--------------------------+---------+----------+----------+
		// | schema_name.table_name1  | analyze | status   | OK       |
		// | schema_name.table_name2  | analyze | status   | OK       |
		// +--------------------------+---------+----------+----------+
		if (ExecLastRawQuery (F_IgnoreSelectKeyword, "ExecSQLFile"))
		{
			while (NextRow())
			{
				KStringView sTable   { Get (1, /*fTrimRight=*/false) };
				KStringView sOp      { Get (2, /*fTrimRight=*/false) };
				KStringView sMsgType { Get (3, /*fTrimRight=*/false) };
				KStringView sMsgText { Get (4, /*fTrimRight=*/false) };

				if ((sMsgType == "status") && (sMsgText == "OK"))
				{
					kDebug (2, "{} table {} {}={}", sOp, sTable, sMsgType, sMsgText);
				}
				else
				{
					kWarning ("UNEXPECTED ANALYZE RESULT: {} table {} {}={}", sOp, sTable, sMsgType, sMsgText);
				}
			}
		}
		else
		{
			Parms.fOK   = false;
			Parms.fDone = true;
		}
	}
	else
	{
		kDebug (3, "{}: statement # {} is not a query...", sFilename, Parms.iStatement);

		if (!IsFlag(F_NoTranslations))
		{
			DoTranslations (m_sLastSQL);
		}

		if (!ExecLastRawSQL (0, "ExecSQLFile") && !Parms.fDropStatement)
		{
			Parms.fOK   = false;
			Parms.fDone = true;
		}
		else
		{
			Parms.iTotalRowsAffected += GetNumRowsAffected();
		}
	}

	if (Parms.fDropStatement)
	{
		SetFlags (0);
	}

	m_sLastSQL.clear();           // reset buffer
	Parms.iLineNumStart  = 0;     // reset line num
	Parms.fDropStatement = false; // reset flags

} // ExecSQLFileGo

//-----------------------------------------------------------------------------
bool KSQL::ExecLastRawQuery (Flags iFlags/*=0*/, KStringView sAPI/*="ExecLastRawQuery"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}{}\n", sAPI, (m_sLastSQL.Contains("\n")) ? "\n" : "", m_sLastSQL);
	}

	EndQuery();

	if (!IsConnectionOpen() && !OpenConnection())
	{
		return (false);
	}

	if (!(iFlags & F_IgnoreSelectKeyword) && !IsFlag(F_IgnoreSelectKeyword) && ! IsSelect (m_sLastSQL))
	{
		m_sLastError.Format ("{}ExecQuery: query does not start with keyword 'select' [see F_IgnoreSelectKeyword]", m_sErrorPrefix);
		return (SQLError());
	}

	KStopTime Timer;

	#if defined(DEKAF2_HAS_CTLIB)
	uint32_t iRetriesLeft = NUM_RETRIES;
	#endif
	
	ResetErrorStatus ();

	switch (m_iAPISet)
	{
    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - -
	case API::MYSQL:
	// - - - - - - - - - - - - - - - - -
		{
			// with mysql, I can get away with calling my ExecSQL() function for the
			// start of queries...
			if (!ExecLastRawSQL (F_NoKlogDebug, "ExecRawQuery"))
			{
				// note: retry logic for MySQL is inside ExecLastRawSQL()
				return (false);
			}

			kDebug (3, "mysql_use_result()...");
			m_dMYSQLResult = mysql_use_result (m_dMYSQL);
			if (!m_dMYSQLResult)
			{
				kDebug(1, "expected query results but got none. Did you intend to use ExecSQL() instead of ExecQuery() ?");
				m_iErrorNum = mysql_errno (m_dMYSQL);
				if (m_iErrorNum == 0)
				{
					m_sLastError = "KSQL: expected query results but got none. Did you intend to use ExecSQL() instead of ExecQuery() ?";
				}
				else
				{
					m_sLastError.Format ("{}MSQL-{}: {}", m_sErrorPrefix, GetLastErrorNum(), mysql_error(m_dMYSQL));
				}
				return (SQLError());
			}

			kDebug (3, "getting col info from mysql...\nmysql_field_count()...");

			m_iNumColumns = mysql_field_count (m_dMYSQL);

			kDebug (3, "num columns: {}", m_iNumColumns);

			m_dColInfo.clear();
			m_dColInfo.reserve(m_iNumColumns);

			kDebug (3, "mysql_fetch_field() X N ...");

			MYSQL_FIELD* pField;

			for (;(pField = mysql_fetch_field(m_dMYSQLResult));)
			{
				KColInfo ColInfo;

				ColInfo.sColName = pField->name;
				ColInfo.SetColumnType (DBT::MYSQL, pField->type, pField->length);
				kDebug (3, "col {:35} mysql_datatype: {:4} => ksql_flags: 0x{:08x} = {}",
					ColInfo.sColName, pField->type, ColInfo.iKSQLDataType, KROW::FlagsToString(ColInfo.iKSQLDataType));

				m_dColInfo.push_back(std::move(ColInfo));
			}
		}
		break;
	#endif

    #ifdef DEKAF2_HAS_ORACLE
	// - - - - - - - - - - - - - - - - -
	case API::OCI8:
	// - - - - - - - - - - - - - - - - -
		{
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 1. OCI8: local "prepare" (parse) of SQL statement (note: in OCI6 this was a trip to the server)
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
		                             (text*)m_sLastSQL.data(), m_sLastSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ExecQuery:OCIStmtPrepare"))
			{
				return (SQLError (/*fForceError=*/false));
			}

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 2. OCI8: pre-execute (parse only) -- redundant trip to server until I get this all working
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
					/*iters=*/1, /*rowoff=*/0, /*snap_in=*/nullptr, /*snap_out=*/nullptr, OCI_DESCRIBE_ONLY);

			if (!WasOCICallOK("ExecQuery:OCIStmtExecute(parse)"))
			{
				return (SQLError (/*fForceError=*/false));
			}

			// Get the number of columns in the query:
			m_iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumColumns,
				0, OCI_ATTR_PARAM_COUNT, (OCIError*)m_dOCI8ErrorHandle);

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 3. OCI8: allocate storage for the results set:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ExecQuery:OCIAttrGet(numcols)"))
			{
				return (SQLError (/*fForceError=*/false));
			}

			m_dColInfo.clear();
			m_dColInfo.reserve();

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 4. OCI8: loop through column information, allocate value storage and bind memory
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			OCIParam* pColParam;	 // <-- column handle
			ub2       iDataType;
			text*     dszColName;
			ub4       iLenColName;
			ub2       iMaxDataSize;

			// go through the column list and retrieve the data type of each column:
			for (uint32_t ii = 1; ii <= m_iNumColumns; ++ii)
			{
				kDebug (3, "  retrieving info about col # {}", ii);

				// get parameter for column ii:
				m_iErrorNum = OCIParamGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, (OCIError*)m_dOCI8ErrorHandle, (void**)&pColParam, ii);
				if (!WasOCICallOK("ExecQuery:OCIParamGet"))
				{
					return (SQLError());
				}

				if (!pColParam)
				{
					kDebug (GetDebugLevel(), "OCI: pColParam is nullptr but was supposed to get allocated by OCIParamGet");
				}

				// Retrieve the data type attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid*) &iDataType, (ub4 *) nullptr, OCI_ATTR_DATA_TYPE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(datatype)"))
				{
					return (SQLError());
				}

				// Retrieve the column name attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid**) &dszColName, (ub4 *) &iLenColName, OCI_ATTR_NAME, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(colname)"))
				{
					return (SQLError());
				}

				// Retreive the maximum datasize:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(void**) &iMaxDataSize, (ub4 *) nullptr, OCI_ATTR_DATA_SIZE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(datasize)"))
				{
					return (SQLError());
				}

				KColInfo ColInfo;

				ColInfo.SetColumnType(DBT::ORACLE8, iDataType, std::max(iMaxDataSize+2, 22+2)); // <-- always malloc at least 24 bytes
				ColInfo.sColName.assign((char* )dszColName, iLenColName);
				ColInfo.dszValue = std::make_unique<char[]>(ColInfo.iMaxDataLen + 1);

				kDebug (GetDebugLevel()+1, "  oci8:column[{}]={}, namelen={}, datatype={}, maxwidth={}, willuse={}",
					ii, ColInfo.sColName, iLenColName, iDataType, iMaxDataSize, ColInfo.iMaxDataLen);

				OCIDefine* dDefine;
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = OCIDefineByPos ((OCIStmt*)m_dOCI8Statement, &dDefine, (OCIError*)m_dOCI8ErrorHandle,
							ii,                                   // column number
							(dvoid *)(ColInfo.dszValue.get()),    // data pointer
							ColInfo.iMaxDataLen,                  // max data length
							SQLT_STR,                             // always bind as a zero-terminated string
							(dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecQuery:OCIDefineByPos"))
				{
					return (SQLError());
				}

				OCIHandleFree ((dvoid*)dDefine, OCI_HTYPE_DEFINE);

				m_dColInfo.push_back(std::move(ColInfo));

			} // foreach column

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. OCI8: execute statement (start the query)...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
						 (OCISnapshot *) nullptr, (OCISnapshot *) nullptr, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iOCI8FirstRowStat = m_iErrorNum;
			if ((m_iErrorNum != OCI_NO_DATA) && !WasOCICallOK("ExecQuery:OCIStmtExecute"))
			{
				return (SQLError());
			}

			// FYI: m_iOCI8FirstRowStat will indicate whether or not OCIStmtExecute() returned the first row
		}
		break;

	// - - - - - - - - - - - - - - - - -
	case API::OCI6:
	// - - - - - - - - - - - - - - - - -
		do // once
		{
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 1. Parse the statement: do not defer the parse, so that errors come back right away
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebug (3, "  calling oparse() to parse SQL statement...");

			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			m_iErrorNum = oparse((Cda_Def*)m_dOCI6ConnectionDataArea, (text *)m_sLastSQL.c_str(), -1, PARSE_NO_DEFER, PARSE_V7_LNG);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			if (!WasOCICallOK("ExecQuery:oparse"))
			{
				return (SQLError  (/*fForceError=*/false));
			}

			int   iMaxWidth=0;
			int   iMaxDataSize=0;
			int   iColNameLen=0;
			short iDataType=0;
			ub2   iColRetlen;
			ub2   iColRetcode;

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 2. determine number of columns in results set (oracle lets you do this *before* the query)
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebug (3, "  determining number of columns in result set...");
			m_iNumColumns = 0;
			while (1)
			{
				uint32_t tmpMaxDataLen = 0;
				char tmpColName[51] = nullptr;
				iColNameLen        = 50;
				sb4 sb4MaxWidth    = 0;
				sb2 sb2DataType    = iDataType;
				sb4 sb4ColNameLen  = iColNameLen;
				sb4 sb4MaxDataSize = iMaxDataSize;

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = odescr((Cda_Def*)m_dOCI6ConnectionDataArea,
					m_iNumColumns+1,
					&sb4MaxWidth,      // max data width coming down the pipe
					&sb2DataType,
					(signed char*) tmpColName,          // column name (overrun protected by iColNameLen)
					&sb4ColNameLen,
					&sb4MaxDataSize,
					nullptr,                      // precision
					nullptr,                      // scale
					nullptr);                     // nulls OK
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

				tmpMaxDataLen      = sb4MaxWidth;
				iDataType          = sb2DataType;
				iColNameLen        = sb4ColNameLen;
				iMaxDataSize       = sb4MaxDataSize;

				if (!WasOCICallOK("ExecQuery:odescr1"))
				{
					if (!tmpMaxDataLen)
					{
						m_iErrorNum = 0;
						break; // no more columns
					}
					if (((Cda_Def*)m_dOCI6ConnectionDataArea)->rc != VAR_NOT_IN_LIST)
						return (SQLError());
				}
				else
				{
					++m_iNumColumns;
					kDebug (3, "    col[{}] = {}", m_iNumColumns, tmpColName);
				}

			} // while counting cols

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 3. allocate storage for the results set:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_dColInfo.clear();
			m_dColInfo.reserve(m_iNumColumns);

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 4. loop through result columns, and bind the memory:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebug (3, "  calling odescr() to bind each column...");
			for (uint32_t ii=1; ii<=m_iNumColumns; ++ii)
			{
				iColNameLen        = 50;
				char sColName[51]  = 0;
				sb4 sb4MaxWidth    = iMaxWidth;
				sb2 sb2DataType    = iDataType;
				sb4 sb4ColNameLen  = iColNameLen;
				sb4 sb4MaxDataSize = iMaxDataSize;

				// now that we have proper allocation for m_Columns[], repeat
				// the odescr() calls and fill in m_Columns[]:
				kDebug (3, "    col[{}]: filling in my m_Columns[] struct...", ii);

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = odescr((Cda_Def*)m_dOCI6ConnectionDataArea, ii,
					&sb4MaxWidth,
					&sb2DataType,
					(signed char*)sColName,   // column name (protected from overrung by iColNameLen)
					&sb4ColNameLen,
					&sb4MaxDataSize,
					nullptr,					   // precision
					nullptr,					   // scale
					nullptr);					   // nulls OK
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

				iMaxWidth    = sb4MaxWidth;
				iDataType    = sb2DataType;
				iColNameLen  = sb4ColNameLen;
				iMaxDataSize = sb4MaxDataSize;

				if (!WasOCICallOK("ExecQuery:odescr2"))
				{
					return (SQLError());
				}

				KColInfo ColInfo;

				ColInfo.SetColumnType(DBT::ORACLE6, iDataType, std::max(iMaxDataSize+2, 22+2)); // <-- always malloc at least 24 bytes
				ColInfo.sColName = sColName;

				kDebug (GetDebugLevel(), "  oci6:column[{}]={}, namelen={}, datatype={}, maxwidth={}, dsize={}, using={} -- OCI6 BUG [TODO]",
						ii, ColInfo.sColName, iColNameLen, iDataType,
						iMaxWidth, iMaxDataSize, ColInfo.iMaxDataLen);

				// allocate at least 20 bytes, which will cover datetimes and numerics:
				ColInfo.iMaxDataLen = std::max (m_dColInfo.iMaxDataLen+2, 20);
				ColInfo.dszValue    = std::make_unique<char[]>(ColInfo.iMaxDataLen + 1);

				// bind some memory to each column to hold the results which will be coming back:
				kDebug (3, "    col[{}]: calling odefin() to bind memory...", ii);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = odefin ((Cda_Def*)m_dOCI6ConnectionDataArea,
							ii,
							(ub1 *)(*ColInfo.dszValue),          // data pointer
							ColInfo.iMaxDataLen,                 // max data length
							STRING_TYPE,                         // always bind as a string
							-1,                                  // scale
							&(ColInfo.indp),                     // used for SQL nullptr determination
							(text *) nullptr,                    // fmt
							-1,                                  // fmtl
							-1,                                  // fmtt
							&iColRetlen,
							&iColRetcode);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecQuery:odefin"))
				{
					return (SQLError());
				}

				m_dColInfo.push_back(std::move(ColInfo));

			} // while binding

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. start the select statement...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebug (3, "  calling oexec() to actually begin the SQL statement...");
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			m_iErrorNum = oexec ((Cda_Def*)m_dOCI6ConnectionDataArea);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			if (!WasOCICallOK("ExecQuery:oexec"))
			{
				return (SQLError());
			}
		}
		while (false); // do once
		break;
	#endif

	#ifdef DEKAF2_HAS_CTLIB
	// - - - - - - - - - - - - - - - - -
	case API::CTLIB:
	// - - - - - - - - - - - - - - - - -
		do
		{
			kDebug (KSQL2_CTDEBUG, "ensuring that no dangling messages are in the ct queue...");
			
			if(IsFlag(F_AutoReset))
			{
				ctlib_login();
			}
			
			ctlib_flush_results ();
			ctlib_clear_errors ();

			kDebug (KSQL2_CTDEBUG, "calling ct_command()...");

			if (ct_command (m_pCtCommand, CS_LANG_CMD, m_sLastSQL.data(), m_sLastSQL.size(), CS_UNUSED) != CS_SUCCEED)
			{
				ctlib_api_error ("KSQL>ExecQuery>ct_command");
				if (--iRetriesLeft && PreparedToRetry())
				{
					continue;
				}
				return (SQLError());
			}

			kDebug (KSQL2_CTDEBUG, "calling ct_send()...");

			if (ct_send(m_pCtCommand) != CS_SUCCEED)
			{
				ctlib_api_error ("KSQL>ExecQuery>ct_send");
				if (--iRetriesLeft && PreparedToRetry())
				{
					continue;
				}
				return (SQLError());
			}

			if (!ctlib_prepare_results ())
			{
				if (--iRetriesLeft && PreparedToRetry())
				{
					continue;
				}
				return (SQLError());
			}

			if (ctlib_check_errors() != 0)
			{
				if (--iRetriesLeft && PreparedToRetry())
				{
					continue;
				}
				return (SQLError());
			}

			break; // success
		}
		while (true);
		break;
	#endif

	// - - - - - - - - - - - - - - - - -
	case API::DBLIB:
	case API::INFORMIX:
	case API::ODBC:
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("unsupported API Set ({})", TxAPISet(m_iAPISet));
		kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	// ensure every column has a name:
	if (m_dColInfo.size() < m_iNumColumns)
	{
		m_dColInfo.resize(m_iNumColumns);
	}

	int cct = 0;
	for (auto& it : m_dColInfo)
	{
		++cct;
		if (it.sColName.empty())
		{
			it.sColName.Format("col:{}", cct);
			kDebug (GetDebugLevel(), "column was missing a name: {}", it.sColName);
		}
	}

	m_bQueryStarted     = true;
	m_iRowNum           = 0;
	m_iNumRowsBuffered  = 0;
	m_iNumRowsAffected  = 0;

	ClearErrorPrefix();

	LogPerformance (Timer.elapsed<std::chrono::milliseconds>().count(), /*bIsQuery=*/true);
	
	if (IsFlag(F_BufferResults))
	{
		return (BufferResults());
	}
	else
	{
		return (true);
	}

} // KSQL::ExecRawQuery

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ParseQuery (KStringView sFormat, ...)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	EndQuery ();
	if (!OpenConnection())
	{
		return (false);
	}

	va_list vaArgList;
	va_start (vaArgList, sFormat);
	m_sLastSQL.FormatV(sFormat, vaArgList);
	va_end (vaArgList);

	if (!IsFlag(F_NoTranslations))
	{
		DoTranslations (m_sLastSQL);
	}

	if (!IsFlag(F_IgnoreSelectKeyword) && ! IsSelect (m_sLastSQL))
	{
		m_sLastError.Format ("{}ParseQuery: query does not start with keyword 'select' [see F_IgnoreSelectKeyword]", m_sErrorPrefix);
		return (SQLError());
	}

	return (ParseRawQuery (m_sLastSQL, 0, "ParseQuery"));

} // ParseQuery
#endif

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ParseRawQuery (KStringView sSQL, int64_t iFlags/*=0*/, KStringView sAPI/*="ParseRawQuery"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}{}\n", sAPI, (sSQL.Contains("\n")) ? "\n" : "", sSQL);
	}

	CopyIfNotSame(m_sLastSQL, sSQL);
	ResetErrorStatus ();

	switch (m_iAPISet)
	{
		// - - - - - - - - - - - - - - - - -
		case API::OCI8:
		// - - - - - - - - - - - - - - - - -
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 1. OCI8: local "prepare" (parse) of SQL statement (note: in OCI6 this was a trip to the server)
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
										 (text*)sSQL.data(), sSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ParseQuery:OCIStmtPrepare"))
				return (SQLError (/*fForceError=*/false));

			m_bStatementParsed = true;
			break;

		// - - - - - - - - - - - - - - - - -
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("unsupported API Set ({}={})", m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	m_bQueryStarted     = true;
	m_iRowNum           = 0;
	m_iNumRowsBuffered  = 0;

	return (true);

} // ParseRawQuery
#endif

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ExecParsedQuery ()
//-----------------------------------------------------------------------------
{
	if (!m_bStatementParsed)
	{
		kWarning ("ParseQuery() was not called yet.");
		kCrashExit (CRASHCODE_DEKAFUSAGE);
		return (false);
	}

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case API::OCI8:
	// - - - - - - - - - - - - - - - - -
		do // once
		{
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 2. OCI8: pre-execute (parse only) -- redundant trip to server until I get this all working
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
					/*iters=*/1, /*rowoff=*/0, /*snap_in=*/nullptr, /*snap_out=*/nullptr, OCI_DESCRIBE_ONLY);

			if (!WasOCICallOK("ExecParsedQuery:OCIStmtExecute(parse)"))
				return (SQLError (/*fForceError=*/false));

			// Get the number of columns in the query:
			m_iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumColumns,
				0, OCI_ATTR_PARAM_COUNT, (OCIError*)m_dOCI8ErrorHandle);

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 3. OCI8: allocate storage for the results set:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(numcols)"))
				return (SQLError (/*fForceError=*/false));

			m_dColInfo.clear();
			m_dColInfo.reserve(m_iNumColumns);

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 4. OCI8: loop through column information, allocate value storage and bind memory
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			OCIParam* pColParam;	 // <-- column handle
			ub2       iDataType;
			text*     dszColName;
			ub4       iLenColName;
			ub2       iMaxDataSize;

			// go through the column list and retrieve the data type of each column:
			for (uint32_t ii = 1; ii <= m_iNumColumns; ++ii)
			{
				kDebug (3, "  retrieving info about col # {}", ii);

				// get parameter for column ii:
				m_iErrorNum = OCIParamGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, (OCIError*)m_dOCI8ErrorHandle, (void**)&pColParam, ii);
				if (!WasOCICallOK("ExecParsedQuery:OCIParamGet"))
					return (SQLError());

				if (!pColParam)
					kDebug (GetDebugLevel(), "OCI: pColParam is nullptr but was supposed to get allocated by OCIParamGet");

				// Retrieve the data type attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid*) &iDataType, (ub4 *) nullptr, OCI_ATTR_DATA_TYPE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(datatype)"))
					return (SQLError());

				// Retrieve the column name attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid**) &dszColName, (ub4 *) &iLenColName, OCI_ATTR_NAME, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(colname)"))
					return (SQLError());

				// Retreive the maximum datasize:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(void**) &iMaxDataSize, (ub4 *) nullptr, OCI_ATTR_DATA_SIZE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(datasize)"))
					return (SQLError());

				KColInfo ColInfo;

				ColInfo.SetColumnType(DBT::ORACLE8, iDataType, std::max(iMaxDataSize+2, 22+2)); // <-- always malloc at least 24 bytes
				ColInfo.sColName.assign(dszColName, iLenColName);
				ColInfo.dszValue = std::make_unique<char[]>(ColInfo.iMaxDataLen + 1);

				kDebug (GetDebugLevel()+1, "  oci8:column[{}]={}, namelen={}, datatype={}, maxwidth={}, willuse={}",
						ii, ColInfo.sColName, iLenColName, iDataType, iMaxDataSize, ColInfo.iMaxDataLen);

				OCIDefine* dDefine;
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = OCIDefineByPos ((OCIStmt*)m_dOCI8Statement, &dDefine, (OCIError*)m_dOCI8ErrorHandle,
							ii,                                   // column number
							(dvoid *)(ColInfo.dszValue.get()),    // data pointer
							ColInfo.iMaxDataLen,                  // max data length
							SQLT_STR,                             // always bind as a zero-terminated string
							(dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecParsedQuery:OCIDefineByPos"))
				{
					return (SQLError());
				}

				OCIHandleFree ((dvoid*)dDefine, OCI_HTYPE_DEFINE);

				m_dColInfo.push_back(std::move(ColInfo));

			} // foreach column

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. OCI8: execute statement (start the query)...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
						 (OCISnapshot *) nullptr, (OCISnapshot *) nullptr, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iOCI8FirstRowStat = m_iErrorNum;
			if ((m_iErrorNum != OCI_NO_DATA) && !WasOCICallOK("ExecParsedQuery:OCIStmtExecute"))
			{
				return (SQLError());
			}

			// FYI: m_iOCI8FirstRowStat will indicate whether or not OCIStmtExecute() returned the first row
		}
		while (false); // do once
		break;

	// - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("unsupported API Set ({}={})", m_iAPISet, TxAPISet(m_iAPISet));
		kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	m_bQueryStarted     = true;
	m_iRowNum           = 0;
	m_iNumRowsBuffered  = 0;
	m_idxBindVar        = 0; // <-- reset index into m_OCIBindArray[]

	if (IsFlag(F_BufferResults))
	{
		return (BufferResults());
	}
	else
	{
		return (true);
	}

} // ExecParsedQuery
#endif

//-----------------------------------------------------------------------------
KROW::Index KSQL::GetNumCols ()
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// FYI: there is nothing database specific in this member function

	if (!QueryStarted())
	{
		m_sLastError.Format ("{}GetNumCols(): called when query was not started.", m_sErrorPrefix);
		SQLError();
		return (0);
	}

	return (m_iNumColumns);

} // KSQL::GetNumCols

//-----------------------------------------------------------------------------
bool KSQL::BufferResults ()
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	if (!QueryStarted())
	{
		m_sLastError.Format ("{}BufferResults(): called when query was not started.", m_sErrorPrefix);
		return (SQLError());
	}

	if (kFileExists (m_sTmpResultsFile))
	{
		kRemoveFile (m_sTmpResultsFile);
	}

	KOutFile file(m_sTmpResultsFile);
	if (!file.Good())
	{
		m_sLastError.Format ("{}BufferResults(): could not buffer results b/c {} could not write to '{}'", m_sErrorPrefix,
			kwhoami(), m_sTmpResultsFile);
		SQLError();
		return (false);
	}

	m_iNumRowsBuffered = 0;

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - -
	case API::MYSQL:
	// - - - - - - - - - - - - - - - - -
		kDebug (3, "mysql_fetch_row() X N ...");

		while ((m_MYSQLRow = mysql_fetch_row (m_dMYSQLResult)))
		{
			++m_iNumRowsBuffered;
			for (KROW::Index ii=0; ii<m_iNumColumns; ++ii)
			{
				char* colval = (m_MYSQLRow)[ii];
				if (!colval)
				{
					colval = (char*)"";   // <-- change db nullptr to cstring ""
				}
				
				// we need to assure that no newlines are in buffered data:
				char* spot = strchr (colval, '\n');
				while (spot)
				{
					*spot = 2; // <-- change them to ^B
					spot = strchr (spot+1, '\n');
				}
				if (strlen(colval) > 50)
				{
					kDebug (3, "  buffered: row[{}]col[{}]: strlen()={}", m_iNumRowsBuffered, ii+1, strlen(colval));
				}
				else
				{
					kDebug (3, "  buffered: row[{}]col[{}]: '{}'", m_iNumRowsBuffered, ii+1, colval);
				}

				file.FormatLine ("{}|{}|{}", m_iNumRowsBuffered, ii+1, strlen(colval));
				file.FormatLine ("{}\n", colval ? colval : "");
			}
		}
		break;
	#endif

    #ifdef DEKAF2_HAS_ORACLE
	// - - - - - - - - - - - - - - - - -
	case API::OCI8:
	// - - - - - - - - - - - - - - - - -
		while (1)
		{
			if (!m_iNumRowsBuffered)
			{
				if (m_iOCI8FirstRowStat == OCI_NO_DATA)
				{
					kDebug (3, "OCIStmtExecute() said we were done");
					break; // while
				}

				// OCI8: first row was already (implicitly) fetched by OCIStmtExecute() [yuk]
				m_iErrorNum = m_iOCI8FirstRowStat;
				m_sLastError.clear();

				kDebug (3, "  OCI8: first row was already fetched by OCIStmtExecute()");
			}
			else
			{
				// make sure SQL nullptr values get left as zero-terminated C strings:
				for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
				{
					m_dColInfo[ii].dszValue.get()[0] = 0;
				}

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = OCIStmtFetch ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("BufferResults::OCIStmtFetch"))
				{
					kDebug (3, "OCIStmtFetch() says we're done");
					break; // end of data from select
				}
			}

			++m_iNumRowsBuffered;

			for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
			{
				if (strlen(m_dColInfo[ii].dszValue.get()) > 50)
				{
					kDebug (3, "  buffered: row[{}]col[{}]: strlen()={}", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue.get());
				}
				else
				{
					kDebug (3, "  buffered: row[{}]col[{}]: '{}'", m_iNumRowsBuffered, ii+1, m_dColInfo[ii].dszValue.get());
				}
				// we need to assure that no newlines are in buffered data:
				char* spot = strchr (m_dColInfo[ii].dszValue.get(), '\n');
				while (spot)
				{
					*spot = 2; // <-- change them to ^B
					spot = strchr (spot+1, '\n');
				}
				fprintf (fp, "{}|{}|{}\n", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue.get()));
				fprintf (fp, "{}\n", m_dColInfo[ii].dszValue ? m_dColInfo[ii].dszValue.get() : "");
			}
		}
		break;

	// - - - - - - - - - - - - - - - - -
	case API::OCI6:
	// - - - - - - - - - - - - - - - - -
		while (1)
		{
			kDebug (3, "calling ofetch() to grab the next row...");
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			m_iErrorNum = ofetch ((Cda_Def*)m_dOCI6ConnectionDataArea);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			if (!WasOCICallOK("BufferResults:ofetch"))
			{
				kDebug (3, "ofetch() says we're done");
				break; // end of data from select
			}

			++m_iNumRowsBuffered;

			// map SQL nullptr values to nullptr-terminated C strings:
			for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
			{
				if (m_dColInfo[ii].indp < 0)
				{
					m_dColInfo[ii].dszValue.get()[0] = 0;
				}

				if (strlen(m_dColInfo[ii].dszValue.get()) > 50)
				{
					kDebug (3, "  buffered: row[{}]col[{}]: strlen()={}", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue.get()));
				}
				else
				{
					kDebug (3, "  buffered: row[{}]col[{}]: '{}'", m_iNumRowsBuffered, ii+1, m_dColInfo[ii].dszValue.get());
				}

				// we need to assure that no newlines are in buffered data:
				char* spot = strchr (m_dColInfo[ii].dszValue.get(), '\n');
				while (spot)
				{
					*spot = 2; // <-- change them to ^B
					spot = strchr (spot+1, '\n');
				}
				fprintf (fp, "{}|{}|{}\n", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue.get()));
				fprintf (fp, "{}\n", m_dColInfo[ii].dszValue ? m_dColInfo[ii].dszValue.get() : "");
			}
		}
		break;
	#endif

	// - - - - - - - - - - - - - - - - -
	case API::CTLIB:
	case API::DBLIB:
	case API::INFORMIX:
	case API::ODBC:
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("unsupported API Set ({})", TxAPISet(m_iAPISet));
		kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

#ifdef DEKAF2_IS_UNIX
	m_bpBufferedResults = fopen (m_sTmpResultsFile.c_str(), "re");
#else
	m_bpBufferedResults = fopen (m_sTmpResultsFile.c_str(), "r");
#endif
	if (!m_bpBufferedResults)
	{
		m_sLastError.Format ("{}BufferResults(): bizarre: {} could not read from '{}', right after creating it.", m_sErrorPrefix,
			kwhoami(), m_sTmpResultsFile);
		SQLError();
		return (false);
	}

	m_bFileIsOpen = true;

    #ifdef DEKAF2_HAS_MYSQL
	if (m_dMYSQLResult)
	{
		//kDebugMemory ((const char*)m_dMYSQLResult, 0);
		mysql_free_result (m_dMYSQLResult);
		m_dMYSQLResult = nullptr;
	}
	#endif

	// Note: results set is freed, fp is open, query is still open,
	// and all results will now be drawn from the tmp file

	return (true);

} // KSQL::BufferResults

//-----------------------------------------------------------------------------
bool KSQL::NextRow ()
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	if (DEKAF2_UNLIKELY(!QueryStarted()))
	{
		m_sLastError.Format ("{}NextRow(): next row called, but no ExecQuery() was started.", m_sErrorPrefix);
		return (SQLError());
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// F_BufferResults: results were placed in a tmp file
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (DEKAF2_LIKELY(!IsFlag(F_BufferResults)))
	{
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// normal operation: return results in real time:
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		kDebug (3, "fetching row...");

		ResetErrorStatus ();

		switch (m_iAPISet)
		{
#ifdef DEKAF2_HAS_MYSQL
				// - - - - - - - - - - - - - - - - -
			case API::MYSQL:
				// - - - - - - - - - - - - - - - - -
				kDebug (3, "mysql_fetch_row()...");

				m_MYSQLRow = mysql_fetch_row (m_dMYSQLResult);
				if (DEKAF2_LIKELY(m_MYSQLRow != nullptr))
				{
					++m_iRowNum;
					kDebug (3, "mysql_fetch_row gave us row {}", m_iRowNum);
					return true;
				}
				else
				{
					kDebug (3, "{} row{} fetched (end was hit)", m_iRowNum, (m_iRowNum==1) ? " was" : "s were");
					return false;
				}
				break;
#endif

#ifdef DEKAF2_HAS_ORACLE
				// - - - - - - - - - - - - - - - - -
			case API::OCI8:
				// - - - - - - - - - - - - - - - - -
				if (!m_iRowNum)
				{
					if (m_iOCI8FirstRowStat == OCI_NO_DATA)
					{
						kDebug (3, "OCIStmtExecute() said we were done");
						return (false); // end of data from select
					}

					// OCI8: first row was already (implicitly) fetched by OCIStmtExecute() [yuk]
					m_iErrorNum = m_iOCI8FirstRowStat;
					m_sLastError.clear();

					kDebug (3, "OCI8: first row was already fetched by OCIStmtExecute()");
				}
				else
				{
					// make sure SQL nullptr values get left as zero-terminated C strings:
					for (auto& Col : m_dColInfo)
					{
						Col.ClearContent();
					}

					//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
					m_iErrorNum = OCIStmtFetch ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
					//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
					if (!WasOCICallOK("NextRow:OCIStmtFetch"))
					{
						kDebug (3, "OCIStmtFetch() says we're done");
						return (false); // end of data from select
					}
					kDebug (3, "OCIStmtFetch() says we got row # {}", m_iRowNum+1);
				}

				++m_iRowNum;

				return (true);

				// - - - - - - - - - - - - - - - - -
			case API::OCI6:
				// - - - - - - - - - - - - - - - - -
			{
				kDebug (3, "calling ofetch() to grab the next row...");

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = ofetch ((Cda_Def*)m_dOCI6ConnectionDataArea);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("NextRow:ofetch"))
				{
					kDebug (3, "ofetch() says we're done");
					return (false); // end of data from select
				}

				++m_iRowNum;

				kDebug (3, "ofetch() says we got row # {}", m_iRowNum);

				// map SQL nullptr values to zero-terminated C strings:
				for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
				{
					if (m_dColInfo[ii].indp < 0)
					{
						kDebug (3, "  fyi: row[{}]col[{}]: nullptr value from database changed to NIL (\"\")",
								   m_iRowNum, m_dColInfo[ii].szColName);
						m_dColInfo[ii].dszValue.get()[0] = 0;
					}
				}

				return (true);
			}
#endif

#ifdef DEKAF2_HAS_CTLIB
				// - - - - - - - - - - - - - - - - -
			case API::CTLIB:
				// - - - - - - - - - - - - - - - - -
				return (ctlib_nextrow());
#endif

				// - - - - - - - - - - - - - - - - -
			case API::DBLIB:
			case API::INFORMIX:
			case API::ODBC:
			default:
				// - - - - - - - - - - - - - - - - -
				kWarning ("unsupported API Set ({})", TxAPISet(m_iAPISet));
				kCrashExit (CRASHCODE_DEKAFUSAGE);
		}
	}
	else
	{
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// results were placed in a tmp file
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		kDebug (3, "fetching buffered row...");

		if (!m_bFileIsOpen)
		{
			m_sLastError.Format ("{}NextRow(): BUG: results are supposed to be buffered but m_bFileIsOpen says false.", m_sErrorPrefix);
			return (SQLError());
		}
		else if (!m_bpBufferedResults)
		{
			m_sLastError.Format ("{}NextRow(): BUG: results are supposed to be buffered but m_bpBufferedResults is nullptr!!", m_sErrorPrefix);
			return (SQLError());
		}

		FreeBufferedColArray (/*fValuesOnly=*/true); // <-- from last row

		if (!m_dBufferedColArray)
		{
			m_dBufferedColArray = (char**) kmalloc (m_iNumColumns * sizeof(char*), "KSQL:NextRow()"); // <-- array of char* ptrs
			for (KROW::Index ii=0; ii<m_iNumColumns; ++ii)
			{
				m_dBufferedColArray[ii] = nullptr;
			}
		}

		++m_iRowNum;

		char szStatLine[25+1];
		for (KROW::Index ii=0; ii<m_iNumColumns; ++ii)
		{
			// first line is a sanity check and the strlen() of data size:
			bool bOK = (fgets (szStatLine, 25+1, m_bpBufferedResults) != nullptr);

			if (!bOK && !ii)
			{
				kDebug (3, "end of buffered results: fgets() returned nullptr");
				return (false);  // <-- no more rows
			}

			else if (!bOK)
			{
				m_sLastError.Format ("{}NextRow(): ran out of lines in buffered results file.", m_sErrorPrefix);
				return (SQLError());
			}

			if (szStatLine[strlen(szStatLine)-1] != '\n')
			{
				m_sLastError.Format ("{}NextRow(): buffered results corrupted stat line for row={}, col={}: '{}'", m_sErrorPrefix, 
					(uint64_t)m_iRowNum, ii+1, szStatLine);
				return (SQLError());
			}

			szStatLine[strlen(szStatLine)-1] = 0; // <-- trim the newline

			// row|col|strlen
			KStack<KStringView> Parts;
			if (kSplit (Parts, szStatLine, "|", "") != 3)
			{
				m_sLastError.Format ("{}NextRow(): buffered results corrupted stat line for row={}, col={}: '{}'", m_sErrorPrefix, 
					(uint64_t)m_iRowNum, ii+1, szStatLine);
				return (SQLError());
			}

			uint64_t iCheckRowNum = Parts[0].UInt64();
			uint64_t iCheckColNum = Parts[1].UInt64();
			uint64_t iDataLen     = Parts[2].UInt64();

			// sanity check the row and col (if we somehow got out of synch, all hell would break loose):
			if ((iCheckRowNum != m_iRowNum) || (iCheckColNum != ii+1))
			{
				if (IsFlag(F_IgnoreSQLErrors))
				{
					kDebug (GetDebugLevel(), "CheckRowNum = {} [should be {}]", iCheckRowNum, m_iRowNum);
					kDebug (GetDebugLevel(), "CheckColNum = {} [should be {}]", iCheckColNum, ii+1);
				}
				else
				{
					kWarning ("CheckRowNum = {} [should be {}]", iCheckRowNum, m_iRowNum);
					kWarning ("CheckColNum = {} [should be {}]", iCheckColNum, ii+1);
				}
				m_sLastError.Format ("{}NextRow(): buffered results stat line out of sync for row={}, col={}", m_sErrorPrefix, 
					(uint64_t)m_iRowNum, ii+1);
				return (SQLError());
			}

			if (iDataLen > 50)
			{
				kDebug (3, "  from-buffer: row[{}]col[{}]: strlen()={}", m_iRowNum, ii+1, iDataLen);
			}
			else
			{
				kDebug (3, "  from-buffer: row[{}]col[{}]: '{}'", m_iRowNum, ii+1, szStatLine);
			}

			// now we know how big the data value is, so we can malloc for it:
			// fyi: tack on a few bytes for newline and NIL...
			m_dBufferedColArray[ii] = (char*) kmalloc (iDataLen+3, "KSQL:NextRow");

			// second line is the data value (could be one HUGE line of gobbly gook):
			bOK = (fgets (m_dBufferedColArray[ii], static_cast<int>(iDataLen+2), m_bpBufferedResults) != nullptr);

			// sanity check: there should be a newline at exactly value[iDataLen]:
			if (!bOK || ((m_dBufferedColArray[ii])[iDataLen] != '\n'))
			{
				m_sLastError.Format ("{}NextRow(): buffered results corrupted data line for row={}, col={}", m_sErrorPrefix, 
					(uint64_t)m_iRowNum, ii+1);
				return (SQLError());
			}

			(m_dBufferedColArray[ii])[iDataLen] = 0;  // <-- trim the newline

			// now change the newlines that were in the actual data back to normal:
			char* spot = strchr (m_dBufferedColArray[ii], 2); // ^B -> ^J
			while (spot)
			{
				*spot = '\n'; // <-- change them back
				spot = strchr (spot+1, 2);
			}
		}
		return (true); // <-- we got a row out of buffer file
	}

	// we never get here..
	return (false);

} // KSQL::NextRow

//-----------------------------------------------------------------------------
bool KSQL::NextRow (KROW& Row, bool fTrimRight)
//-----------------------------------------------------------------------------
{
	bool bGotOne = NextRow();

	Row.clear();

	if (bGotOne)
	{
		kDebug (3, "  data: got row {}, now loading property sheet with {} column values...", m_iRowNum, GetNumCols());
	}
	else
	{
		kDebug (3, "  data: no more rows.");
	}

	// load up a property sheet so we can lookup values by column name:
	// (we can do this even if we didn't get a row back)
	for (KROW::Index ii=1; ii <= GetNumCols(); ++ii)
	{
		const KColInfo& pInfo = GetColProps (ii);
		Row.AddCol (pInfo.sColName, (bGotOne ? Get (ii, fTrimRight) : ""), pInfo.iKSQLDataType, pInfo.iMaxDataLen);
	}

	return (bGotOne);

} // NextRow

//-----------------------------------------------------------------------------
bool KSQL::LoadColumnLayout(KROW& Row, KStringView sColumns)
//-----------------------------------------------------------------------------
{
	KStringView sExpandedColumns = sColumns.empty() ? "*" : sColumns;

	if (GetDBType() == DBT::SQLSERVER)
	{
		if (!ExecRawQuery(kFormat("select top 0 {} from {}", sExpandedColumns, Row.GetTablename()), 0, "LoadColumnLayout"))
		{
			return false;
		}
	}
	else
	{
		if (!ExecRawQuery(kFormat("select {} from {} limit 0", sExpandedColumns, Row.GetTablename()), 0, "LoadColumnLayout"))
		{
			return false;
		}
	}

	for (KROW::Index ii=1; ii <= GetNumCols(); ++ii)
	{
		const KColInfo& pInfo = GetColProps (ii);
		Row.AddCol (pInfo.sColName, "", pInfo.iKSQLDataType, pInfo.iMaxDataLen);
	}

	return true;

} // LoadColumnLayout

//-----------------------------------------------------------------------------
void KSQL::FreeBufferedColArray (bool fValuesOnly/*=false*/)
//-----------------------------------------------------------------------------
{
	//kDebug (3, "{}...", m_dBufferedColArray);

	// FYI: the m_dBufferedColArray is only used with the F_BufferResults flag
	// FYI: there is nothing database specific in this member function

	if (!m_dBufferedColArray)
	{
		return;
	}

	//kDebugMemory (3, (const char*)m_dBufferedColArray, 0);

	for (KROW::Index ii=0; ii<m_iNumColumns; ++ii)
	{
		if (m_dBufferedColArray[ii])
		{
			//kDebug (3, "  free(m_dBufferedColArray[ii]= {}[{}] = {})", m_dBufferedColArray, ii, m_dBufferedColArray+ii);
			kfree (m_dBufferedColArray[ii], "KSQL:FreeBufferedColArray[ii]");  // <-- frees the result of strdup() in NextRow()
			m_dBufferedColArray[ii] = nullptr;
		}
	}

	if (fValuesOnly)
	{
		return;
	}

	//kDebug (3, "  free(m_dBufferedColArray = {})", m_dBufferedColArray);
	kfree (m_dBufferedColArray, "KSQL:m_dBufferedColArray");  // <-- frees the result of malloc() in NextRow()

	m_dBufferedColArray = nullptr;

} // FreeBufferedColArray

//-----------------------------------------------------------------------------
KROW KSQL::SingleRawQuery (KString sSQL, Flags iFlags/*=0*/, KStringView sAPI/*="SingleRawQuery"*/)
//-----------------------------------------------------------------------------
{
	KROW ROW;

	EndQuery ();

	if (IsConnectionOpen() || OpenConnection())
	{
		Flags iHold = GetFlags();
		m_iFlags |= F_IgnoreSQLErrors;
		m_iFlags |= iFlags;

		bool bOK = ExecRawQuery (std::move(sSQL), 0, sAPI);

		m_iFlags = iHold;

		if (!bOK)
		{
			kDebugLog (GetDebugLevel(), "KSQL::{}(): sql error: {}", sAPI, GetLastError());
		}
		else if (!NextRow(ROW))
		{
			kDebugLog (GetDebugLevel(), "KSQL::{}(): expected one row back and didn't get it", sAPI);
		}

		EndQuery();
	}

	return ROW;

} // SingleRawQuery

//-----------------------------------------------------------------------------
KString KSQL::SingleStringRawQuery (KString sSQL, Flags iFlags/*=0*/, KStringView sAPI/*="SingleStringRawQuery"*/)
//-----------------------------------------------------------------------------
{
	auto ROW = SingleRawQuery(std::move(sSQL), iFlags, sAPI);

	if (ROW.empty())
	{
		return {};
	}

	kDebugLog (GetDebugLevel(), "KSQL::{}(): got {}\n", sAPI, ROW.GetValue(0));

	return ROW.GetValue(0);

} // SingleStringRawQuery

//-----------------------------------------------------------------------------
int64_t KSQL::SingleIntRawQuery (KString sSQL, Flags iFlags/*=0*/, KStringView sAPI/*="SingleIntRawQuery"*/)
//-----------------------------------------------------------------------------
{
	auto sValue = SingleStringRawQuery(std::move(sSQL), iFlags, sAPI);

	if (!GetLastError().empty())
	{
		return -1;
	}
	else if (!kIsInteger(sValue))
	{
		return 0;
	}
	else
	{
		return sValue.Int64();
	}

} // SingleIntRawQuery

//-----------------------------------------------------------------------------
bool KSQL::ResetBuffer ()
//-----------------------------------------------------------------------------
{
	kDebug (GetDebugLevel(), "ResetBuffer");

	// FYI: there is nothing database specific in this member function

	if (!m_bQueryStarted)
	{
		m_sLastError.Format ("{}ResetBuffer(): no query is currently open.", m_sErrorPrefix);
		return (SQLError());
	}
	
	if (!IsFlag (F_BufferResults))
	{
		m_sLastError.Format ("{}ResetBuffer(): F_BufferResults flag needs to be set *before* query is run.", m_sErrorPrefix);
		return (SQLError());
	}

	if (m_bFileIsOpen)
	{
		if (m_bpBufferedResults)
		{
			fclose (m_bpBufferedResults);
		}
		m_bpBufferedResults = nullptr;
		m_bFileIsOpen = false;
	}

#ifdef DEKAF2_IS_UNIX
	m_bpBufferedResults = fopen (m_sTmpResultsFile.c_str(), "re");
#else
	m_bpBufferedResults = fopen (m_sTmpResultsFile.c_str(), "r");
#endif
	if (!m_bpBufferedResults)
	{
		m_sLastError.Format ("{}ResetBuffer(): bizarre: {} could not read from '{}'.", m_sErrorPrefix,
			kwhoami(), m_sTmpResultsFile);
		SQLError();
		return (false);
	}

	m_bFileIsOpen = true;
	m_iRowNum     = 0;     // <-- this needs to start over (like we are re-running the query)

	return (true);

} // KSQL::ResetBuffer

//-----------------------------------------------------------------------------
void KSQL::EndQuery (bool bDestructor/*=false*/)
//-----------------------------------------------------------------------------
{
	if (!m_bQueryStarted)
	{
		return;
	}

	if (!bDestructor)
	{
		kDebug (3, "  ...");
	}

	if (m_bQueryStarted)
	{
		if (!bDestructor)
		{
			kDebug (GetDebugLevel()+1, "  {} row{} fetched.", m_iRowNum, (m_iRowNum==1) ? " was" : "s were");
		}
	}

    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - - - - - - - - -
	// MYSQL end of query cleanup:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_dMYSQLResult)
	{
		mysql_free_result (m_dMYSQLResult);
		m_dMYSQLResult = nullptr;
	}
	#endif

	#ifdef DEKAF2_HAS_CTLIB
	if (GetDBType() == DBT::SQLSERVER || GetDBType() == DBT::SYBASE)
	{
		ctlib_flush_results();
	}
	#endif

	m_dColInfo.clear();

	#ifdef DEKAF2_HAS_ORACLE
	if (m_dOCI6ConnectionDataArea)
	{
		ocan ((Cda_Def*)m_dOCI6ConnectionDataArea);  // <-- let OIC6 cursor free it's resources
	}
	#endif

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// my own results buffering cleanup
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_bFileIsOpen)
	{
		if (!bDestructor)
		{
			kDebug (3, "  fclose (m_bpBufferedResults)...");
		}
		if (m_bpBufferedResults)
		{
			fclose (m_bpBufferedResults);
			m_bpBufferedResults = nullptr;
		}
		m_bFileIsOpen = false;
	}

	if (kFileExists (m_sTmpResultsFile))
	{
		kRemoveFile (m_sTmpResultsFile);
	}

	m_iRowNum           = 0;
	m_iNumRowsBuffered  = 0;
#ifdef DEKAF2_HAS_ORACLE
	m_iOCI8FirstRowStat = 0;
#endif

	FreeBufferedColArray ();

	m_bQueryStarted = false;

} // KSQL::EndQuery

//-----------------------------------------------------------------------------
const KSQL::KColInfo& KSQL::GetColProps (KROW::Index iOneBasedColNum)
//-----------------------------------------------------------------------------
{
	static KColInfo s_NullResult;

	kDebug (3, "...");

	if (!QueryStarted())
	{
		m_sLastError.Format ("{}{}() called, but no ExecQuery() was started.", m_sErrorPrefix, "GetColProps");
		SQLError();
		return (s_NullResult);
	}

	if (iOneBasedColNum == 0)
	{
		m_sLastError.Format ("{}{}() called with ColNum < 1 ({})", m_sErrorPrefix, "GetColProps", iOneBasedColNum);
		return (s_NullResult);
	}

	else if (iOneBasedColNum > m_dColInfo.size())
	{
		m_sLastError.Format ("{}{}() called with ColNum > {} ({})", m_sErrorPrefix, "GetColProps",  
			m_iNumColumns, iOneBasedColNum);
		return (s_NullResult);
	}

	return (m_dColInfo[iOneBasedColNum-1]);

} // GetColProps

//-----------------------------------------------------------------------------
KStringView KSQL::Get (KROW::Index iOneBasedColNum, bool fTrimRight/*=true*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	if (!QueryStarted())
	{
		m_sLastError.Format ("{}() called, but no ExecQuery() was started.", m_sErrorPrefix);
		SQLError();
		return ("ERR:NOQUERY-KSQL::Get()");
	}

	if (!m_iRowNum)
	{
		m_sLastError.Format ("{}() called, but NextRow() not called or did not return a row.", m_sErrorPrefix);
		SQLError();
		return ("ERR:NOROW-KSQL::Get()");
	}

	if (iOneBasedColNum == 0)
	{
		return ("ERR:UNDERFLOW-KSQL::Get()");
	}

	if (iOneBasedColNum > m_iNumColumns)
	{
		return ("ERR:OVERFLOW-KSQL::Get()");
	}

	KStringView sRtnValue;

	if (IsFlag (F_BufferResults))
	{
		sRtnValue = m_dBufferedColArray[iOneBasedColNum-1];
	}
	else
	{
		switch (m_iAPISet)
		{
			#ifdef DEKAF2_HAS_MYSQL
			// - - - - - - - - - - - - - - - - -
			case API::MYSQL:
			// - - - - - - - - - - - - - - - - -
				if (m_MYSQLRow)
				{
					sRtnValue = static_cast<MYSQL_ROW>(m_MYSQLRow)[iOneBasedColNum-1];
				}
				else
				{
					return ("ERR:MYSQLRow=nullptr-KSQL::Get()");
				}
				break;
			#endif

			// - - - - - - - - - - - - - - - - -
			#ifdef DEKAF2_HAS_ORACLE
			case API::OCI8:
			case API::OCI6:
			#endif
			#ifdef DEKAF2_HAS_DBLIB
			case API::DBLIB:
			#endif
			#ifdef DEKAF2_HAS_CTLIB
			case API::CTLIB:
			#endif
			// - - - - - - - - - - - - - - - - -
				#if defined(DEKAF2_HAS_ORACLE) || defined(DEKAF2_HAS_CTLIB) || defined(DEKAF2_HAS_DBLIB)
				sRtnValue = m_dColInfo[iOneBasedColNum-1].dszValue.get();
				break;
				#endif

			// - - - - - - - - - - - - - - - - -
			case API::INFORMIX:
			case API::ODBC:
			default:
			// - - - - - - - - - - - - - - - - -
				kWarning ("unsupported API Set ({})", TxAPISet(m_iAPISet));
				kCrashExit (CRASHCODE_DEKAFUSAGE);
		}

	}

	if (fTrimRight)
	{
		sRtnValue.TrimRight();
	}

	if (kWouldLog(3))
	{
		if (sRtnValue.size() > 50)
		{
			kDebug (3, "  data: row[{}]col[{}]: strlen()={}", m_iRowNum, iOneBasedColNum, sRtnValue.size());
		}
		else
		{
			kDebug (3, "  data: row[{}]col[{}]: '{}'", m_iRowNum, iOneBasedColNum, sRtnValue);
		}
	}

	return sRtnValue;

} // KSQL::Get

//-----------------------------------------------------------------------------
time_t KSQL::GetUnixTime (KROW::Index iOneBasedColNum)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// FYI: there is nothing database specific in this member function
	// (we get away with this by fetching all results as strings, then
	// using C routines to do data conversions)

	KString sVal (Get (iOneBasedColNum));

	if (sVal.empty() || sVal == "0" || sVal.starts_with("00000"))
	{
		return (0);
	}

	if (sVal.Contains("ERR"))
	{
		m_sLastError.Format ("{}IntValue(row={},col={}): {}", m_sErrorPrefix, m_iRowNum, iOneBasedColNum+1, sVal);
		SQLError();
		return (0);
	}
 
	struct tm TimeStruct;

	int iSecs;

	if (sscanf (sVal.c_str(), "%*04d-%*02d-%*02d %*02d:%*02d:%02d", &iSecs) == 1) // e.g. "1965-03-31 12:00:00"
	{
		strptime (sVal.c_str(), "%Y-%m-{} %H:%M:{}", &TimeStruct);
	}
	else if (sVal.size() == std::strlen ("20010302213436")) // e.g. "20010302213436"  which means 2001-03-02 21:34:36
	{
		strptime (sVal.c_str(), "%Y%m{}%H%M{}", &TimeStruct);
	}
	else
	{
		m_sLastError.Format ("{}UnixTime({}): expected '{}' to look like '20010302213436' or '2001-03-21 06:18:33'",
		                     m_sErrorPrefix, iOneBasedColNum, sVal);
		SQLError();
		return (0);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// mktime() will fill in the missing info in the time struct and
	// return the unix time (uint64_t):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	time_t iUnixTime = mktime (&TimeStruct);

	//kDebugTmStruct ((char*)"KSQL:UnixTime()", &TimeStruct, /*iMinDebugLevel=*/2);
	kDebug (GetDebugLevel()+1, "  unixtime = {}", iUnixTime);

	return (iUnixTime);

} // KSQL::GetUnixTime

//-----------------------------------------------------------------------------
bool KSQL::SQLError (bool fForceError/*=false*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// FYI: there is nothing database specific in this member function

	if (m_sLastError.empty())
	{
		kDebug (GetDebugLevel(), "something is amuck.  SQLError() called, but m_sLastError is empty.");
		m_sLastError.Format("unknown SQL error");
	}
	
	if (IsFlag(F_IgnoreSQLErrors) && !fForceError)
	{
		kDebug (GetDebugLevel(), "IGNORED: {}", m_sLastError);
	}
	else
	{			
		if (m_TimingCallback)
		{
			m_TimingCallback (*this, /*iMilliseconds=*/0, kFormat ("{}:\n{}", m_sLastError, m_sLastSQL));
		}
		kWarning (m_sLastError);
		if (!m_sLastSQL.empty())
		{
			kWarning (m_sLastSQL);
		}
	}

	return (false);  // <-- return code still needs to be false, just no verbosity into the log

} // KSQL::SQLError

//-----------------------------------------------------------------------------
bool KSQL::WasOCICallOK (KStringView sContext)
//-----------------------------------------------------------------------------
{
	m_sLastError.clear();
    #ifdef DEKAF2_HAS_ORACLE
	switch (m_iErrorNum)
	{
	case OCI_SUCCESS: // 0
		m_sLastError.Format ("{}{}() returned {} ({})", m_sErrorPrefix, sContext, m_iErrorNum, "OCI_SUCCESS");
		kDebugLog (3, "  {} -- returned OCI_SUCCESS", sContext);
		return (true);

	case OCI_SUCCESS_WITH_INFO:
		m_sLastError.Format ("{}{}() returned {} ({})", m_sErrorPrefix, sContext, m_iErrorNum, "OCI_SUCCESS_WITH_INFO");
		kDebugLog (3, "  {} -- returned OCI_SUCCESS_WITH_INFO", sContext);
		return (true);

	case OCI_NEED_DATA:
		m_sLastError.Format ("{}OCI-{:05}: OCI_NEED_DATA [{}]", m_sErrorPrefix, m_iErrorNum, sContext);
		break; // error

	case OCI_INVALID_HANDLE:
		m_sLastError.Format ("{}OCI-{:05}: OCI_INVALID_HANDLE [{}]", m_sErrorPrefix, m_iErrorNum, sContext);
		break; // error

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// ORA-03123: operation would block
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		//   *Cause:  This is a status code that indicates that the operation
		//            cannot complete now. 
		//   *Action: None; this is not an error.  The operation should be retried
		//            again for completion. 

	case OCI_STILL_EXECUTING:  // ORA-03123
		m_sLastError.Format ("{}OCI-{:05}: OCI_STILL_EXECUTE [{}]", m_sErrorPrefix, m_iErrorNum, sContext);
		break; // error

	case OCI_CONTINUE:
		m_sLastError.Format ("{}OCI-{:05}: OCI_CONTINUE [{}]", m_sErrorPrefix, m_iErrorNum, sContext);
		break; // error

	case NO_DATA_FOUND:  // <-- old
	case -1403:          // <-- OERR_NODATA
	case OCI_NO_DATA:    // <-- current
//		m_iErrorNum = OCI_NO_DATA;
//		m_sLastError.Format ("{}OCI-{:05}: OCI_NO_DATA [{}]", m_sErrorPrefix, m_iErrorNum, sContext);
//		break; // error
	case OCI_ERROR:
		if (GetAPISet() == API::OCI6)
		{
			if (m_dOCI6LoginDataArea && m_dOCI6ConnectionDataArea)
			{
				m_iErrorNum = ((Cda_Def*)m_dOCI6ConnectionDataArea)->rc;
				if (m_iErrorNum == 0)         // ORA-00000: no error
					m_iErrorNum = sqlca.sqlcode;

				m_sLastError = m_sErrorPrefix;
				oerhms ((Lda_Def*)m_dOCI6LoginDataArea, ((Cda_Def*)m_dOCI6ConnectionDataArea)->rc, (text*)m_sLastError.c_str()+strlen(m_sLastError.c_str()), MAXLEN_ERR-strlen(m_sLastError.c_str()));
			}
			else
				m_sLastError.Format ("{}unknown error (m_dOCI6LoginDataArea not allocated yet)", m_sErrorPrefix);
		}
		else // API::OCI8
		{
			// Tricky:
			// One would think that after a successful OCI call, the OCI would always return OCI_SUCCESS.
			// However, sometimes Oracle returns OCI_ERROR and only upon calling OCIErrorGet() does it
			// occur that there was actually no error.

			if (m_dOCI8ErrorHandle)
			{
				sb4 sb4Stat;
				m_sLastError = m_sErrorPrefix;
				OCIErrorGet (m_dOCI8ErrorHandle, 1, nullptr, &sb4Stat, (text*)m_sLastError.c_str()+strlen(m_sLastError.c_str()), MAXLEN_ERR-strlen(m_sLastError.c_str()), OCI_HTYPE_ERROR);
				m_iErrorNum = sb4Stat;
			}
			else
			{
				m_sLastError.Format ("{}unknown error (m_dOCI8ErrorHandler not allocated yet)", m_sErrorPrefix);
			}
		}

		if (strchr (m_sLastError.c_str(), '\n'))
			*(strchr (m_sLastError.c_str(),'\n')) = 0;
		snprintf (m_sLastError+strlen(m_sLastError), MAXLEN_ERR, " [{}]", sContext);

		if ( (m_iErrorNum == 0)    // ORA-00000: no error
		  || (m_iErrorNum == 1405) // ORA-01405: fetched column value is nullptr
		  || (m_iErrorNum == 1406) // ORA-01406: fetched column value was truncated
		)
		{
			kDebugLog (3, "  {} -- [ok] returned {}", sContext, m_sLastError.c_str());
			return (true);
		}
		break;

	//----------
	default:
	//----------

		if ((m_iErrorNum < 0) && (GetAPISet() == API::OCI6) && m_dOCI6LoginDataArea)
		{
			// translate old convention into current error messages:
			m_iErrorNum = -m_iErrorNum;
			m_sLastError = m_sErrorPrefix;
			oerhms ((Lda_Def*)m_dOCI6LoginDataArea, m_iErrorNum, (text*)m_sLastError.c_str()+strlen(m_sLastError.c_str()), MAXLEN_ERR-strlen(m_sLastError.c_str()));
			return (false);
		}

		switch (m_iErrorNum)
		{
			// discovered during development with OCI6:
			case  4:    m_sLastError.Format ("{}ORA-00004: object does not exist (?)", m_sErrorPrefix);         break;

			// taken from ldap/public/omupi.h because my switch default was getting called on OCI6 calls:
			//se -1:    m_sLastError.Format ("{}OERR_DUP: duplicate value in index", m_sErrorPrefix);           break; -- dupe of OCI_ERROR
			case -54:   m_sLastError.Format ("{}OERR_LOCKED: table locked", m_sErrorPrefix);                    break;
			case -904:  m_sLastError.Format ("{}OERR_NOCOL: unknown column name", m_sErrorPrefix);              break;
			case -913:  m_sLastError.Format ("{}OERR_2MNYVALS: too many values", m_sErrorPrefix);               break;
			case -942:  m_sLastError.Format ("{}OERR_NOTABLE: unknown table or view", m_sErrorPrefix);          break;
			case -955:  m_sLastError.Format ("{}OERR_KNOWN: object already exists", m_sErrorPrefix);            break;
			case -1002: m_sLastError.Format ("{}OERR_FOOSQ: fetch out of sequence", m_sErrorPrefix);            break;
			case -1009: m_sLastError.Format ("{}OERR_MPARAM: missing mandatory param", m_sErrorPrefix);         break;
			case -1017: m_sLastError.Format ("{}OERR_PASSWD: incorrect username or password", m_sErrorPrefix);  break;
			case -1089: m_sLastError.Format ("{}OERR_SHTDWN: shutdown", m_sErrorPrefix);                        break;
			case -1430: m_sLastError.Format ("{}OERR_DUPCOL: column already exists", m_sErrorPrefix);           break;
			case -1451: m_sLastError.Format ("{}OERR_NULL: column already null", m_sErrorPrefix);               break;
			case -1722: m_sLastError.Format ("{}OERR_INVNUM: invalid number", m_sErrorPrefix);                  break;
			case -3113: m_sLastError.Format ("{}OERR_EOFCC: end-of-file on comm channel", m_sErrorPrefix);      break;
			case -3114: m_sLastError.Format ("{}OERR_NOCONN: not connected", m_sErrorPrefix);                   break;
			case -6111: m_sLastError.Format ("{}OERR_TCPDISC: TCP disconnect", m_sErrorPrefix);                 break;
			case -6550: m_sLastError.Format ("{}OERR_PLSERR: PL/SQL problem", m_sErrorPrefix);                  break;

			default:
				m_sLastError.Format ("{}{}() returned {} ({})", m_sErrorPrefix, sContext, m_iErrorNum, "UNKNOWN OCI RTN");
		}

	}

	kDebugLog (3, "  {} -- returned {}", sContext, m_sLastError);
	#endif

	return (false);  // <-- on error, m_saLastError is formatted, but not sent anywhere yet

} // WasOCICallOK

//-----------------------------------------------------------------------------
bool KSQL::SetAPISet (API iAPISet)
//-----------------------------------------------------------------------------
{
	if (IsConnectionOpen())
	{
		m_sLastError.Format ("{}SetAPISet(): connection is already open.", m_sErrorPrefix);
		return (SQLError());
	}

	InvalidateConnectSummary ();

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// explicit API selection:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (iAPISet != API::NONE)
	{
		m_iAPISet = iAPISet;
		kDebug (GetDebugLevel(), "set to {}", TxAPISet(m_iAPISet));
		return (true);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// check environment: $KSQL_APISET
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	int iEnv = kGetEnv ("KSQL_APISET", "0").Int32();
	if (iEnv)
	{
		m_iAPISet = static_cast<API>(iEnv);
		kDebug (GetDebugLevel(), "(from $KSQL_APISET) set to {}", TxAPISet(m_iAPISet));
		return (true);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// pick the default set of APIs, based on the database type:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	switch (m_iDBType)
	{

		case DBT::NONE:
			m_sLastError.Format ("{}SetAPISet(): API mismatch: database type not set.", m_sErrorPrefix);
			return (false);

		case DBT::MYSQL:                m_iAPISet = API::MYSQL;       break;

		case DBT::SQLITE3:              m_iAPISet = API::SQLITE3;     break;

		case DBT::ORACLE6:
		case DBT::ORACLE7:
		case DBT::ORACLE8:
		case DBT::ORACLE:               m_iAPISet = API::OCI8;        break;

		case DBT::SQLSERVER:
		case DBT::SYBASE:               m_iAPISet = API::CTLIB;       break; // choices: API::DBLIB -or- API::CTLIB

		case DBT::INFORMIX:             m_iAPISet = API::INFORMIX;    break;

		default:
			m_sLastError.Format ("{}SetAPISet(): unsupported database type ({})", m_sErrorPrefix, TxDBType(m_iDBType));
			return (SQLError());
	}

	return (true);

} // SetAPISet

//-----------------------------------------------------------------------------
KSQL::Flags KSQL::SetFlags (Flags iFlags)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// FYI: there is nothing database specific in this member function

	//if (QueryStarted())  KEEF: i dont care about this anymore.  let the flags apply to the next action
	//{
	//	m_sLastError.Format ("{}SetFlags(): you cannot change flags while a query is in process.", m_sErrorPrefix);
	//	SQLError();
	//	return (false);
	//}

	auto iSaved = m_iFlags;
	m_iFlags = iFlags;

	return (iSaved);

} // KSQL::SetFlags

//-----------------------------------------------------------------------------
KString KSQL::GetLastInfo()
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_HAS_MYSQL)
	return mysql_info(m_dMYSQL);
#else
	return {};
#endif

} // KSQL::GeLastInfo

//-----------------------------------------------------------------------------
void KSQL::BuildTranslationList (TXList& pList, DBT iDBType)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	if (iDBType == DBT::NONE)
	{
		iDBType = m_iDBType;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// build the translation array:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	for (const auto& g_Translation : g_Translations)
	{
		KStringView sName = g_Translation.sOriginal;

		switch (iDBType)
		{
			case DBT::MYSQL:
				pList.Add (sName, g_Translation.sMySQL);
				break;

			case DBT::SQLITE3:
				pList.Add (sName, g_Translation.sSQLite3);
				break;

			case DBT::ORACLE6:
			case DBT::ORACLE7:
				pList.Add (sName, g_Translation.sOraclePre8);
				break;

			case DBT::ORACLE8:
			case DBT::ORACLE:
				pList.Add (sName, g_Translation.sOracle);
				break;

			case DBT::SYBASE:
			case DBT::SQLSERVER:
				pList.Add (sName, g_Translation.sSybase);
				break;

			case DBT::INFORMIX:
				pList.Add (sName, g_Translation.sInformix);
				break;

			case DBT::NONE:
				break;
		}
	}

	// non-database specific pairs:
	KString sPID;
	sPID = KString::to_string(kGetPid());

	pList.Add ("PID",        sPID);
	pList.Add ("$$",         std::move(sPID));
	pList.Add ("hostname",   kGetHostname());
	pList.Add ("P",          "+");
	pList.Add ("DC",         "{{");

} // BuildTranslationList

//-----------------------------------------------------------------------------
void KSQL::DoTranslations (KString& sSQL)
//-----------------------------------------------------------------------------
{
	kDebug (3,
			   "BEFORE:\n"
			   "{}",
				   sSQL);

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// do the translations on the string (in place):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	static constexpr KStringView s_sOpen  = "{{";
	static constexpr KStringView s_sClose = "}}";

	if (!kReplaceVariables(sSQL, s_sOpen, s_sClose, /*bQueryEnvironment=*/false, m_TxList))
	{
		kDebug (3, " --> no SQL translations.");
	}
	else
	{
		kDebug (3, "AFTER:\n{}", sSQL);
	}

} // DoTranslations

//-----------------------------------------------------------------------------
KStringView KSQL::TxDBType (DBT iDBType) const
//-----------------------------------------------------------------------------
{
	switch (iDBType)
	{
		case DBT::NONE:         return ("NotInitialized");
		case DBT::MYSQL:        return ("MySQL");
		case DBT::SQLITE3:      return ("SQLite3");
		case DBT::ORACLE6:      return ("Oracle6");
		case DBT::ORACLE7:      return ("Oracle7");
		case DBT::ORACLE8:      return ("Oracle8");
		case DBT::ORACLE:       return ("Oracle");
		case DBT::SQLSERVER:    return ("SQLServer");
		case DBT::SYBASE:       return ("Sybase");
		case DBT::INFORMIX:     return ("Informix");
		default:                return ("MysteryType");
	}

} // TxDBType

//-----------------------------------------------------------------------------
KStringView KSQL::TxAPISet (API iAPISet) const
//-----------------------------------------------------------------------------
{
	switch (iAPISet)
	{
		case API::NONE:         return ("NotInitialized");
		case API::MYSQL:        return ("mysql");
		case API::SQLITE3:      return ("sqlite3");
		case API::OCI6:         return ("oci6");
		case API::OCI8:         return ("oci8");
		case API::DBLIB:        return ("dblib");
		case API::CTLIB:        return ("ctlib");
		case API::INFORMIX:     return ("ifx");
		default:                return ("MysteryAPIs");
	}

} // TxAPISet

//-----------------------------------------------------------------------------
void KSQL::FormatConnectSummary () const
//-----------------------------------------------------------------------------
{
	m_sConnectSummary.clear();

	switch (m_iDBType)
	{
		case DBT::ORACLE6:
		case DBT::ORACLE7:
		case DBT::ORACLE:
			// - - - - - - - - - - - - - -
			// fred@orcl [Oracle]
			// - - - - - - - - - - - - - -
			m_sConnectSummary += m_sUsername;
			m_sConnectSummary += "@";
			m_sConnectSummary += m_sHostname;
			if (GetDBPort())
			{
				m_sConnectSummary += ":";
				m_sConnectSummary += KString::to_string(GetDBPort());
			}
			m_sConnectSummary += " [Oracle:";
			m_sConnectSummary += TxAPISet(m_iAPISet);
			m_sConnectSummary += "]";
			break;

		default:
			// - - - - - - - - - - - - - -
			// fred@mydb:myhost [DBType]
			// - - - - - - - - - - - - - -
			if (!m_sHostname.empty())
			{
				m_sConnectSummary += m_sUsername;
				m_sConnectSummary += "@";
				m_sConnectSummary += m_sHostname;
				if (GetDBPort())
				{
					m_sConnectSummary += ":";
					m_sConnectSummary += KString::to_string(GetDBPort());
				}
				m_sConnectSummary += ":";
				m_sConnectSummary += m_sDatabase;
				m_sConnectSummary += " [";
				m_sConnectSummary += TxDBType(m_iDBType);
				m_sConnectSummary += "]";
			}

			// - - - - - - - - - - - - - -
			// fred@mydb [DBType]
			// - - - - - - - - - - - - - -
			else
			{
				m_sConnectSummary += m_sUsername;
				m_sConnectSummary += "@";
				m_sConnectSummary += m_sDatabase;
				m_sConnectSummary += " [";
				m_sConnectSummary += TxDBType(m_iDBType);
				m_sConnectSummary += "]";
			}

			break;
	}

} // FormatConnectSummary

//-----------------------------------------------------------------------------
const KString& KSQL::ConnectSummary () const
//-----------------------------------------------------------------------------
{
	if (m_sConnectSummary.empty())
	{
		FormatConnectSummary();
	}
	return (m_sConnectSummary);

} // ConnectSummary

#if 0
-----------------------------------------------------------------------------
OCI8: Obsolescent OCI Routines
-----------------------------------------------------------------------------
Release 8.0 of the Oracle Call Interface introduced an entirely new set of 
functions which were not available in release 7.3. Release 8i adds more new 
functions. The earlier 7.x calls are still available, but Oracle strongly 
recommends that existing applications use the new calls to improve performance 
and provide increased functionality. 

7.x OCI Routine  Equivalent or Similar 8.x Oracle OCI Routine  
---------------  ----------------------------------------------------

 obindps(),      OCIBindByName(), OCIBindByPos() 
  obndra(),       (Note: additional bind calls may be necessary for some data types)  
  obndrn(), 
  obndrv()
 
 obreak()        OCIBreak()  

 ocan()          none  

 oclose()        (Note: cursors are not used in Release 8i)

 ocof(), ocon()  OCIStmtExecute() with OCI_COMMIT_ON_SUCCESS mode  

 ocom()          OCITransCommit()  

 odefin(),       OCIDefineByPos() 
  odefinps()     (Note: additional define calls may be necessary for some data types)  

 odescr()        Note: schema objects are described with OCIDescribeAny(). 
                  A describe, as used in release 7.x, will most often be done by 
                  calling OCIAttrGet() on the statement handle after SQL statement execution.  
 
 odessp()        OCIDescribeAny()  
 
 oerhms()        OCIErrorGet()  
 
 oexec(), oexn() OCIStmtExecute()  
 
 oexfet()        OCIStmtExecute(), OCIStmtFetch() (Note: result set rows can be implicitly prefetched)  
 
 ofen(), ofetch() OCIStmtFetch()  
 
 oflng()         none  
 
 ogetpi()        OCIStmtGetPieceInfo()  
 
 olog()          OCILogon()  
 
 ologof()        OCILogoff()  
 
 onbclr(),       Note: non-blocking mode can be set or checked by calling 
  onbset(),       OCIAttrSet() or OCIAttrGet() on the server context handle or 
  onbtst()        service context handle  
 
 oopen()         Note: cursors are not used in Release 8i  
 
 oopt()          none  
 
 oparse()        OCIStmtPrepare(); however, it is all local  
 
 opinit()        OCIInitialize()  
 
 orol()          OCITransRollback()  
 
 osetpi()        OCIStmtSetPieceInfo()  
 
 sqlld2()        SQLSvcCtxGet or SQLEnvGet  
 
 sqllda()        SQLSvcCtxGet or SQLEnvGet  
 
 odsc()          Note: see odescr() above  
 
 oermsg()        OCIErrorGet()  
 
 olon()          OCILogon()  
 
 orlon()         OCILogon()  
 
 oname()         Note: see odescr() above  
 
 osql3()         Note: see oparse() above  

See Also: For information about the additional functionality provided by new 
functions not listed here, see the remaining chapters of this guide. 

-----------------------------------------------------------------------------
OCI8: OCI Routines Not Supported
-----------------------------------------------------------------------------
These OCI routines that were available in previous versions of the OCI are not 
supported in Oracle8i.

OCI Routine  Equivalent or Similar 8.x Oracle OCI Routine  
-----------  ----------------------------------------------------
 obind()     OCIBindByName(), OCIBindByPos() (Note: additional bind calls may 
              be necessary for some data types)  

 obindn()    OCIBindByName(), OCIBindByPos() (Note: additional bind calls may 
              be necessary for some data types)  

 odfinn()    OCIDefineByPos() (Note: additional define calls may be necessary 
              for some data types)  

 odsrbn()    Note: see odescr() above

 ologon()    OCILogon()  

 osql()      Note: see oparse() above

#endif

//-----------------------------------------------------------------------------
bool KSQL::ListTables (KStringView sLike/*="%"*/, bool fIncludeViews/*=false*/, bool fRestrictToMine/*=true*/)
//-----------------------------------------------------------------------------
{
	// ORACLE rtns 3 cols:
	//  TableName
	//  TABLE|VIEW
	//  Owner
	//
	// MySQL rtns 15 cols:
	//	Name
	//	Type
	//	Row_format
	//	Rows
	//	Avg_row_length
	//	Data_length
	//	Max_data_length
	//	Index_length
	//	Data_free
	//	Auto_increment
	//	Create_time
	//	Update_time
	//	Check_time
	//	Create_options
	//	Comment

	switch (m_iDBType)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// NONE OF THE ARGS: sLike, fIncludeViews and fRestrictToMine are supported for MySQL.
		// fIncludeViews:   MySQL supports VIEWS and they appear in the "show table status" results with type=nullptr
		// fRestrictToMine: the MySQL "show table status" command only does exactly that
		if (sLike != "%"_ksv)
		{
			kDebug (GetDebugLevel(), "warning: {}: MySQL 'show table status' does not support arguments", sLike);
		}
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool bOK = ExecQuery ("show table status");
			//SetFlags (iFlags);
			return (bOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::ORACLE7:
	case DBT::ORACLE8:
	case DBT::ORACLE:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// FYI: Oracle USER_TABLES is the same as DBA_TABLES where owner=(connecting user)
		if (fRestrictToMine)
		{
			if (fIncludeViews)
			{
				return (ExecQuery (
					"select table_name, 'TABLE', upper('{}')\n"
					"  from USER_TABLES\n"
					" where table_name like '{}'\n"
					"union\n"
					"select view_name, 'VIEW', upper('{}')\n"
					"  from USER_VIEWS\n"
					" where view_name like '{}'\n"
					" order by 1", 
						m_sUsername, sLike, m_sUsername, sLike));
			}
			else // !fIncludeViews
			{
				return (ExecQuery (
					"select table_name, 'TABLE', upper('{}')\n"
					"  from USER_TABLES\n"
					" where table_name like '{}'\n"
					" order by 1", 
						m_sUsername, sLike));
			}
		}
		else // !fRestrictToMine
		{
			if (fIncludeViews)
			{
				return (ExecQuery (
					"select table_name, 'TABLE', Owner\n"
					"  from DBA_TABLES\n"
					" where table_name like '{}'\n"
					"union\n"
					"select view_name, 'VIEW', Owner\n"
					"  from DBA_VIEWS\n"
					" where view_name like '{}'\n"
					" order by 1", 
						sLike, sLike));
			}
			else // !fIncludeViews
			{
				return (ExecQuery (
					"select table_name, 'TABLE', Owner\n"
					"  from DBA_TABLES\n"
					" where table_name like '{}'\n"
					" order by 1", 
						m_sUsername, sLike));
			}
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::SQLSERVER:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		return (ExecQuery ("select * from sysobjects where type = 'U' and name like '{}' order by name", sLike));

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}ListTables(): {} not supported yet.", m_sErrorPrefix, TxDBType(m_iDBType));
		return (SQLError());
	}

	return (false);

} // ListTables


//-----------------------------------------------------------------------------
bool KSQL::ListProcedures (KStringView sLike/*="%"*/, bool fRestrictToMine/*=true*/)
//-----------------------------------------------------------------------------
{
	// MySQL rtns cols:
	//	Database
	//	Name
	//	Type

	switch (m_iDBType)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// NONE OF THE ARGS: sLike and fRestrictToMine are supported for MySQL.
		// fRestrictToMine: the MySQL "show procedure status" command only does exactly that
		if (sLike != "%"_ksv)
		{
			kDebug (GetDebugLevel(), "warning: {}: MySQL 'show procedure status' does not support arguments", sLike);
		}
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool bOK = ExecQuery ("show procedure status");
			//SetFlags (iFlags);
			return (bOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// case DBT::ORACLE7:
	// case DBT::ORACLE8:
	// case DBT::ORACLE:
	// - - - - - - - - - - - - - - - - - - - - - - - -

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// case SQLSERVER:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// return (ExecQuery ("select * from sysobjects where type = 'P' and name like '{}' order by name", sLike));

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}ListProcedures(): {} not supported yet.", m_sErrorPrefix, TxDBType(m_iDBType));
		return (SQLError());
	}

	return (false);

} // ListProcedures

//-----------------------------------------------------------------------------
bool KSQL::DescribeTable (KStringView sTablename)
//-----------------------------------------------------------------------------
{
	// rtns: ColName, Datatype, Null, Key, Default, Extra

	switch (m_iDBType)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// desc: ColName, Datatype, Null, Key, Default, Extra
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool bOK = ExecQuery ("desc {}", sTablename);
			//SetFlags (iFlags);
			return (bOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::ORACLE7:
	case DBT::ORACLE8:
	case DBT::ORACLE:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		{
		KString sSchemaOwner = GetDBUser();
		sSchemaOwner.MakeUpper();
		return (ExecQuery (
			"select column_name, data_type, nullable, data_length, data_precision, ''\n"
			"  from DBA_TAB_COLUMNS\n"
			" where owner = '{}'\n"
			"   and table_name = '{}'\n"
			" order by column_id", 
				sSchemaOwner, sTablename));
		}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::SQLSERVER:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool bOK = ExecQuery ("sp_help {}", sTablename);
			//SetFlags (iFlags);
			return (bOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}DescribeTable(): {} not supported yet.", m_sErrorPrefix, TxDBType(m_iDBType));
		return (SQLError());
	}

} // DescribeTable

//-----------------------------------------------------------------------------
KJSON KSQL::FindColumn (KStringView sColLike)
//-----------------------------------------------------------------------------
{
	kDebug (3, "{}...", sColLike);
	KJSON list = KJSON::array();

	switch (m_iDBType)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		if (!ExecQuery (
			"select table_name\n"
			"     , column_name\n"
			"     , column_type\n"
			"     , column_key\n"
			"     , column_comment\n"
			"     , is_nullable\n"
			"     , column_default\n"
			"  from INFORMATION_SCHEMA.COLUMNS\n"
			" where upper(column_name) like upper('%s')\n"
			"   and table_schema not in ('information_schema','master')\n"
			" order by table_name desc, 2, 3",
				sColLike))
		{
			return list;
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::ORACLE7:
	case DBT::ORACLE8:
	case DBT::ORACLE:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		m_sLastError = "KSQL::FindColumn() not coded yet for DBT::SQLSERVER";
		kWarningLog (m_sLastError);
		return list;
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::SQLSERVER:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		m_sLastError = "KSQL::FindColumn() not coded yet for DBT::SQLSERVER";
		kWarningLog (m_sLastError);
		return list;
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}DescribeTable(): {} not supported yet.", m_sErrorPrefix, TxDBType(m_iDBType));
		kWarning (m_sLastError);
		return list;
		break;
	}

	KROW  row;
	while (NextRow (row))
	{
		kDebug (3, "{}: found column {} in table {}", sColLike, row["column_name"], row["table_name"]);
		list += row;
	}

	return list;

} // FindColumn

#if 0
//-----------------------------------------------------------------------------
unsigned char* KSQL::EncodeData (unsigned char* sBlobData, BlobType iBlobType, uint64_t iBlobDataLen/*=0*/, bool fInPlace/*=false*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// NOTE: this routine returns NEW MEMORY (upon success) 
	// which needs to be freed (unless BlobType=BT_ASCII and fInPlace=true):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	unsigned char* dszEncodedBlob = nullptr;
	uint64_t  ii,jj;

	// - - - - - - - - - - - - - - - - - - - -
	//case BT_ASCII -- InPlace (no malloc):
	// - - - - - - - - - - - - - - - - - - - -
	if (fInPlace)
	{
		if (iBlobType != BT_ASCII)
		{
			m_sLastError.Format ("{}EncodeData(): fInPlace is true, but BlobType is not ASCII", m_sErrorPrefix);
			SQLError();
			return (nullptr);
		}

		iBlobDataLen   = iBlobDataLen ? iBlobDataLen : strlen((const char*)sBlobData);

		for (ii=0; ii < iBlobDataLen; ++ii)
		{
			switch (sBlobData[ii])
			{
				case '\'': sBlobData[ii] = 1;                break;  // <-- single quote to ^A
				case 10:   sBlobData[ii] = 2;                break;  // <-- ^J to ^B
				case 13:   sBlobData[ii] = 5;                break;  // <-- ^M to ^E
			}
		}

		return (sBlobData);  // <-- no new memory, rtn same ptr passed in
	}

	switch (iBlobType)
	{
	// - - - - - - - - - -
	case BT_ASCII:
	// - - - - - - - - - -
		iBlobDataLen   = iBlobDataLen ? iBlobDataLen : strlen((const char*)sBlobData);
		dszEncodedBlob = (unsigned char*) kmalloc (iBlobDataLen + 1, "KSQL:dszEncodedBlob");

		for (ii=0, jj=0; ii < iBlobDataLen; ++ii, ++jj)
		{
			switch (sBlobData[ii])
			{
				case '\'': dszEncodedBlob[jj] = 1;                break;  // <-- single quote to ^A
				case 10:   dszEncodedBlob[jj] = 2;                break;  // <-- ^J to ^B
				case 13:   dszEncodedBlob[jj] = 5;                break;  // <-- ^M to ^E
				default:   dszEncodedBlob[jj] = sBlobData[ii];  break;  // <-- no translation
			}
		}
		dszEncodedBlob[jj] = 0;

		if (iBlobDataLen < 40)
		{
			kDebug (GetDebugLevel(), "(ASCII): '{}' --> '{}'", sBlobData, dszEncodedBlob);
		}

		return (dszEncodedBlob); // <-- new memory

	// - - - - - - - - - -
	case BT_BINARY:
	// - - - - - - - - - -
		if (!iBlobDataLen)
		{
			m_sLastError.Format ("{}EncodeData(): BlobType is BINARY, but iBlobDataLen not specified", m_sErrorPrefix);
			SQLError();
			return (nullptr);
		}

		dszEncodedBlob = (unsigned char*) kmalloc ((iBlobDataLen+1) * 2, "KSQL:dszEncodedBlob");
		for (ii=0, jj=0; ii<iBlobDataLen; ++ii)
		{
			unsigned char szAdd[2+1];
			snprintf ((char*)szAdd, 2+1, "%02x", sBlobData[ii]);  // <-- 2-digit HEX encoding
			dszEncodedBlob[jj++] = szAdd[0];
			dszEncodedBlob[jj++] = szAdd[1];
		}
		dszEncodedBlob[jj] = 0;

		if (iBlobDataLen < 40)
		{
			kDebug (GetDebugLevel(), "(BINARY): '{}' --> '{}'", sBlobData, dszEncodedBlob);
		}

		return (dszEncodedBlob); // <-- new memory
	}	

	m_sLastError.Format ("{}EncodeData(): unsupported BlobType={}", m_sErrorPrefix, iBlobType);
	SQLError();
	return (nullptr); // <-- failed (no new memory to free)

} // EncodeData

//-----------------------------------------------------------------------------
unsigned char* KSQL::DecodeData (unsigned char* sBlobData, BlobType iBlobType, uint64_t iEncodedLen/*=0*/, bool fInPlace/*=false*/)
//-----------------------------------------------------------------------------
{
	kDebug (GetDebugLevel(), "...");

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// NOTE: this routine returns NEW MEMORY (upon success) 
	// which needs to be freed (unless BlobType=BT_ASCII and fInPlace=true):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	unsigned char* dszDecodedBlob = nullptr;
	uint64_t  ii,jj;

	// - - - - - - - - - - - - - - - - - - - -
	//case BT_ASCII -- InPlace (no malloc):
	// - - - - - - - - - - - - - - - - - - - -
	if (fInPlace)
	{
		if (iBlobType != BT_ASCII)
		{
			m_sLastError.Format ("{}DecodeData(): fInPlace is true, but BlobType is not ASCII", m_sErrorPrefix);
			SQLError();
			return (nullptr);
		}

		iEncodedLen   = iEncodedLen ? iEncodedLen : strlen((const char*)sBlobData);

		for (ii=0; ii < iEncodedLen; ++ii)
		{
			switch (sBlobData[ii])
			{
				case 1:  sBlobData[ii] = '\''; break;  // <-- ^A to single quote
				case 2:  sBlobData[ii] = 10;   break;  // <-- ^B to ^J
				case 5:  sBlobData[ii] = 13;   break;  // <-- ^E to ^M
			}
		}

		return (sBlobData);  // <-- no new memory, rtn same ptr passed in
	}

	switch (iBlobType)
	{
	// - - - - - - - - - -
	case BT_ASCII:
	// - - - - - - - - - -
		iEncodedLen    = iEncodedLen ? iEncodedLen : strlen((const char*)sBlobData);
		dszDecodedBlob = (unsigned char*) kmalloc (iEncodedLen + 1, "KSQL:dszDecodedBlob");

		for (ii=0, jj=0; ii < iEncodedLen; ++ii, ++jj)
		{
			switch (sBlobData[ii])
			{
			case 1:  dszDecodedBlob[ii] = '\'';             break;  // <-- ^A to single quote
			case 2:  dszDecodedBlob[ii] = 10;               break;  // <-- ^B to ^J
			case 5:  dszDecodedBlob[ii] = 13;               break;  // <-- ^E to ^M
			default: dszDecodedBlob[jj] = sBlobData[ii];  break;  // <-- no translation
			}
		}
		dszDecodedBlob[jj] = 0;

		if (iEncodedLen < 40)
		{
			kDebug (GetDebugLevel(), "(ASCII): '{}' --> '{}'", sBlobData, dszDecodedBlob);
		}

		return (dszDecodedBlob); // <-- new memory

	// - - - - - - - - - -
	case BT_BINARY:
	// - - - - - - - - - -
		if (!iEncodedLen)
		{
			m_sLastError.Format ("{}DecodeData(): BlobType is BINARY, but iEncodedLen not specified", m_sErrorPrefix);
			SQLError();
			return (nullptr);
		}

		dszDecodedBlob = (unsigned char*) kmalloc ((iEncodedLen+1) / 2, "KSQL:dszDecodedBlob");
		for (ii=0, jj=0; ii<iEncodedLen; ii+=2)
		{
			unsigned char szHexPair[2+1];
			szHexPair[0] = sBlobData[ii+0];
			szHexPair[1] = sBlobData[ii+1];
			szHexPair[2] = 0;

			// sanity check:
			bool bOK1 = false;
			bool bOK2 = false;

			if (!szHexPair[0])
				bOK1 = true;
			else if ((szHexPair[0] >= '0') && (szHexPair[0] <= '9'))
				bOK1 = true;
			else if ((szHexPair[0] >= 'a') && (szHexPair[0] <= 'f'))
				bOK1 = true;
			else if ((szHexPair[0] >= 'A') && (szHexPair[0] <= 'F'))
				bOK1 = true;

			if (!szHexPair[1])
				bOK2 = true;
			else if ((szHexPair[1] >= '0') && (szHexPair[1] <= '9'))
				bOK2 = true;
			else if ((szHexPair[1] >= 'a') && (szHexPair[1] <= 'f'))
				bOK2 = true;
			else if ((szHexPair[1] >= 'A') && (szHexPair[1] <= 'F'))
				bOK2 = true;

			if (!bOK1 || !bOK2)
			{
				if (kWouldLog(GetDebugLevel())
				{
					kDebug (GetDebugLevel(), "corrupted hex pair in encoded data:");
					kDebug (GetDebugLevel(), "  szHexPair[{}+{}] = %3d ({})", ii, 0, szHexPair[0], bOK1 ? "valid hex digit" : "INVALID HEX DIGIT");
					kDebug (GetDebugLevel(), "  szHexPair[{}+{}] = %3d ({})", ii, 1, szHexPair[1], bOK2 ? "valid hex digit" : "INVALID HEX DIGIT");
					kDebug (GetDebugLevel(), "  EncodedLen={}", iEncodedLen);
					kDebug (GetDebugLevel(), "  sBlobData     = {} (memory dump to follow)...", sBlobData);
					kDebug (GetDebugLevel(), "  sBlobData+Len = {}", sBlobData+iEncodedLen);
				}

				//kDebugMemory ((const char*) sBlobData, 0, /*iLinesBefore=*/0, /*iLinesAfter=*/iEncodedLen/8);

				m_sLastError.Format ("{}DecodeData(): corrupted blob (details in klog)", m_sErrorPrefix);
				SQLError();
				return (nullptr);
			}

			int iValue = 0;
			sscanf ((char*)szHexPair, "%x", &iValue);

			kDebug (GetDebugLevel()+1, "  HexPair[%04lu/%04lu]: '{}' ==> %03d ==> '%c'", ii, iEncodedLen, szHexPair, iValue, iValue);

			dszDecodedBlob[jj++] = iValue;
		}
		dszDecodedBlob[jj] = 0;

		return (dszDecodedBlob); // <-- new memory
	}	

	m_sLastError.Format ("{}DecodeData(): unsupported BlobType={}", m_sErrorPrefix, iBlobType);
	SQLError();
	return (nullptr); // <-- failed (no new memory to free)

} // DecodeData

//-----------------------------------------------------------------------------
bool KSQL::PutBlob (KStringView sBlobTable, KStringView sBlobKey, unsigned char* sBlobData, BlobType iBlobType, uint64_t iBlobDataLen/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	if (!sBlobTable || !sBlobKey || !sBlobData)
	{
		m_sLastError.Format ("{}PutBlob(): BlobTable, BlobKey or BlobData is nullptr", m_sErrorPrefix);
		return (SQLError());
	}

	enum {MAXLEN = 2000};

	switch (iBlobType)
	{
		// - - - - - - - - - -
		case BT_ASCII:
		// - - - - - - - - - -
			iBlobDataLen   = iBlobDataLen ? iBlobDataLen : strlen((const char*)sBlobData);
			break;

		// - - - - - - - - - -
		case BT_BINARY:
		// - - - - - - - - - -
			if (!iBlobDataLen)
			{
				m_sLastError.Format ("KSQL:PutBlob(): BlobType is BINARY, but iBlobDataLen not specified");
				return (SQLError());
			}
			break;
	}
	
	// assumes table looks like this:

	// BlobKey       char(50)      not null,
	// ChunkNum      int           not null,
	// Chunk         {{CHAR2000}}  null,
	// Encoding      int           not null, // 'A'(65):KSQLASCII | 'B'(66):KSQLBINARY
	// EncodedSize   int           not null,
	// DataSize      int           not null

	// 1. delete any existing rows from previous puts:
	ExecSQL ("delete from {} where BlobKey = '{}'", sBlobTable, sBlobKey);

	if (!iBlobDataLen)
	{
		return (true);  // <-- nothing to insert, string is NIL
	}

	unsigned char* dszEncodedBlob = EncodeData (sBlobData, iBlobType, iBlobDataLen); // malloc
	unsigned char  szChunk[MAX_BLOBCHUNKSIZE+1];
	unsigned char* sSpot = dszEncodedBlob;
	int iChunkNum = 0;

	while (sSpot && *sSpot)
	{
		++iChunkNum;

		uint64_t iAmountLeft         = strlen((const char*)sSpot);
		uint64_t iEncodedLenChunk    = (iAmountLeft < MAXLEN) ? iAmountLeft : MAXLEN;
		uint64_t iDataLenChunk       = iEncodedLenChunk / ((iBlobType==BT_ASCII) ? 1 : 2);
		memcpy (szChunk, sSpot, iEncodedLenChunk);
		szChunk[iEncodedLenChunk] = 0;

		kDebug (GetDebugLevel(), "chunk '{}', part[{:02}]: encoding={}, datasize=%04lu", sBlobKey, iChunkNum, iBlobType, iDataLenChunk);

		bool bOK = ExecSQL (
			"insert into {} (BlobKey, ChunkNum, Chunk, Encoding, EncodedSize, DataSize)\n"
			"values ('{}', {}, '{}', {}, {}, {})",
				sBlobTable,
				sBlobKey, iChunkNum, szChunk, iBlobType, iEncodedLenChunk, iDataLenChunk);

		if (!bOK)
		{
			return (false);
		}

		sSpot += iEncodedLenChunk;
	}

	if (dszEncodedBlob)
	{
		free (dszEncodedBlob);
	}

	return (true);

} // PutBlob

//-----------------------------------------------------------------------------
unsigned char* KSQL::GetBlob (KStringView sBlobTable, KStringView sBlobKey, uint64_t* piBlobDataLen/*=nullptr*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// NOTE: this routine returns NEW MEMORY (upon success) 
	// which needs to be freed...
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	// assumes table looks like this:

	// BlobKey       char(50)      not null,
	// ChunkNum      int           not null,
	// Chunk         {{CHAR2000}}  null,
	// Encoding      int           not null, // 'A'(65):KSQLASCII | 'B'(66):KSQLBINARY
	// EncodedSize   int           not null,
	// DataSize      int           not null

	int64_t iBlobDataLen = SingleIntQuery ("select sum(DataSize) from {} where BlobKey='{}'", sBlobTable, sBlobKey);
	if (iBlobDataLen < 0)
	{
		return (nullptr); // db error
	}

	kDebug (GetDebugLevel(), "expecting %lld bytes for BlobKey='{}'", iBlobDataLen, sBlobKey);

	if (!iBlobDataLen)
	{
		return ((unsigned char*)strdup(""));  // <-- return a zero-length string that can be freed
	}

	unsigned char* dszBlobData = (unsigned char*) kmalloc ((uint32_t)(iBlobDataLen+1), "KSQL:dszBlobData");
	unsigned char* sSpot     = dszBlobData;

	if (!ExecQuery ("select ChunkNum, Chunk, Encoding, EncodedSize, DataSize from {} where BlobKey='{}' order by ChunkNum", sBlobTable, sBlobKey))
	{
		return (nullptr);
	}

	while (NextRow())
	{
		int         iChunkNum       = IntValue (1);
		KStringView sEncodedChunk   = (unsigned char*)Get (2, /*fTrimRight=*/false);
		int         iEncoding       = IntValue (3);
		uint64_t    iEncodedSize    = ULongValue (4);
		uint64_t    iDataSize       = ULongValue (5);

		kDebug (GetDebugLevel(), "chunk '{}', part[{:02}]: encoding={}, datasize=%04lu", sBlobKey, iChunkNum, iEncoding, iDataSize);

		// 1. decode this chunk:
		kDebug (GetDebugLevel()+1, "(1): decoding this chunk...");
		unsigned char* dszChunk        = DecodeData (sEncodedChunk, iEncoding, iEncodedSize); // malloc

		// 2. add this chunk on to our return memory:
		kDebug (GetDebugLevel()+1, "(2): adding decoded chunk to Spot={} (BlobData={})",
				sSpot, dszBlobData);
		memcpy (sSpot, dszChunk, iDataSize);

		// 3. position the pointer:
		kDebug (GetDebugLevel()+1, "(3): moving Spot from {} to {}...",
				sSpot, sSpot + iDataSize);
		sSpot += iDataSize;

		if ((dszChunk != sEncodedChunk)
				&& (dszChunk != nullptr))
		{
			kfree (dszChunk);
		}
	}

	kDebug (GetDebugLevel()+1, "checking result...");

	if (sSpot != (dszBlobData + iBlobDataLen))
	{
		kDebug (GetDebugLevel(), "sanity check failed:");
		kDebug (GetDebugLevel(), "    dszBlobData   = {}", dszBlobData);
		kDebug (GetDebugLevel(), "  + iBlobDataLen  = + %8lld", iBlobDataLen);
		kDebug (GetDebugLevel(), "  --------------    ----------");
		kDebug (GetDebugLevel(), "                  = {}", dszBlobData + iBlobDataLen);
		kDebug (GetDebugLevel(), "    but sSpot   = {}", sSpot);
	}

	*sSpot = 0;                       // <-- terminate the blob (in case its ascii)

	if (piBlobDataLen)
	{
		kDebug (GetDebugLevel(), "return length set to %lld", iBlobDataLen);

		*piBlobDataLen = (uint64_t) iBlobDataLen;  // <-- for truly binary data, this data length is essential
	}
	else
	{
		kDebug (GetDebugLevel(), "return length (%lld) not passed back", iBlobDataLen);
	}

	kDebug (GetDebugLevel(), "return data pointer sBlobData = {}", dszBlobData);

	return (dszBlobData);               // <-- new memory which must be freed

} // GetBlob
#endif

#ifdef DEKAF2_HAS_ODBC
//------------------------------------------------------------------------------
bool KSQL::CheckODBC (RETCODE iRetCode)
//------------------------------------------------------------------------------
{
	switch (iRetCode)
	{
		case SQL_SUCCESS:
		case SQL_SUCCESS_WITH_INFO:
			return (true);

		case SQL_NO_DATA_FOUND:
			return (false);

		case SQL_INVALID_HANDLE:
			m_sLastError.Format ("{}invalid odbc handle", m_sErrorPrefix);
			return (false);

		case SQL_ERROR:
		default:
			{
				#if 0
				// ksql.C(4202) : error C2660: 'SQLError' : function does not take 8 parameters
				unsigned char	szSQLState[6];
				unsigned char	szErrorMsg[200];
				SQLError (m_Environment, m_hdbc, m_hstmt, szSQLState, nullptr,
						  szErrorMsg, sizeof(szErrorMsg), nullptr);
				m_sLastError.Format ("{}{}: {}", m_sErrorPrefix, szSQLState, szErrorMsg);
				#else
				m_sLastError.Format ("{}ODBC Error: Code={}", m_sErrorPrefix, iRetCode);
				#endif
			}
			return (false);
	}

} // CheckODBC

#endif

#ifdef DEKAF2_HAS_ORACLE

//-----------------------------------------------------------------------------
bool KSQL::_BindByName (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType)
//-----------------------------------------------------------------------------
{
	if (!m_bStatementParsed)
	{
		m_sLastError.Format ("{}BindByName(): ParseSQL() or ParseQuery() was not called yet.", m_sErrorPrefix);
		return (false);
	}

	if (m_idxBindVar >= MAXBINDVARS)
	{
		m_sLastError.Format ("{}BindByName(): too many bind vars (MAXBINDVARS={})", m_sErrorPrefix, MAXBINDVARS);
		return (false);
	}

	if (sPlaceholder.empty() || !sPlaceholder.front() == ':'))
	{
		m_sLastError.Format ("{}BindByName(): invalid placeholder (should start with ':').", m_sErrorPrefix);
		return (false);
	}

	m_iErrorNum = OCIBindByName (
			(OCIStmt*)   m_dOCI8Statement,
			(OCIBind**)  &(m_OCIBindArray[m_idxBindVar++]),
			(OCIError*)  m_dOCI8ErrorHandle, 
			(CONST text*)sPlaceholder.data(),
			(sb4)        sPlaceholder.size(),
			(dvoid*)     pValue,
			(sb4)        iValueSize,
			(ub2)        iDataType,
			(dvoid*)     /*indp (IN/OUT)*/    nullptr, // Pointer to an indicator variable or array.
			(ub2*)       /*alenp (IN/OUT)*/   nullptr, // Pointer to array of actual lengths of array elements.
			(ub2*)       /*rcodep (OUT)*/     nullptr, // Pointer to array of column level return codes.
			(ub4)        /*maxarr_len(IN)*/   0,    // The maximum possible number of elements
			(ub4)        /*curelep (IN/OUT)*/ 0,    // Pointer to the actual number of elements.
			(ub4)        /*mode (IN)*/        OCI_DEFAULT // OCI_DEFAULT | OCI_DATA_AT_EXEC
	);

	if (!WasOCICallOK("BindByName"))
	{
		kDebug (GetDebugLevel(), m_sLastError);
		return (false);
	}

	return (true);

} // _BindByName - private

//-----------------------------------------------------------------------------
bool KSQL::BindByName (KStringView sPlaceholder, char* sValue)
//-----------------------------------------------------------------------------
{
	return (_BindByName (sPlaceholder, (dvoid*)sValue, strlen(sValue)+1, SQLT_STR));

} // BindByName

//-----------------------------------------------------------------------------
bool KSQL::BindByName (KStringView sPlaceholder, int*   piValue)
//-----------------------------------------------------------------------------
{
	return (_BindByName (sPlaceholder, (dvoid*)piValue, sizeof(int), SQLT_INT));

} // BindByName

//-----------------------------------------------------------------------------
bool KSQL::BindByName (KStringView sPlaceholder, uint32_t*  piValue)
//-----------------------------------------------------------------------------
{
	return (_BindByName (sPlaceholder, (dvoid*)piValue, sizeof(uint32_t), SQLT_UIN));

} // BindByName

//-----------------------------------------------------------------------------
bool KSQL::BindByName (KStringView sPlaceholder, long*  piValue)
//-----------------------------------------------------------------------------
{
	return (_BindByName (sPlaceholder, (dvoid*)piValue, sizeof(long), SQLT_LNG));

} // BindByName

//-----------------------------------------------------------------------------
bool KSQL::BindByName (KStringView sPlaceholder, uint64_t* piValue)
//-----------------------------------------------------------------------------
{
	kAssert (sizeof(uint64_t) == sizeof(uint32_t), "KSQL:BindByName"); // no SQLT_ULN !
	return (_BindByName (sPlaceholder, (dvoid*)piValue, sizeof(uint64_t), SQLT_UIN));

} // BindByName

//-----------------------------------------------------------------------------
bool KSQL::BindByName (KPROPS* pList)
//-----------------------------------------------------------------------------
{
	for (uint32_t ii=0; ii < pList->GetNumPairs(); ++ii)
	{
		char* sPlaceholder = pList->GetName(ii);
		char* sValue       = pList->GetValue(ii);

		if (!BindByName (sPlaceholder, sValue))
		{
			return (false);
		}
	}

	return (true);

} // BindByName

//-----------------------------------------------------------------------------
bool KSQL::_BindByPos (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType)
//-----------------------------------------------------------------------------
{
	if (!m_bStatementParsed)
	{
		m_sLastError.Format ("{}BindByPos(): ParseSQL() or ParseQuery() was not called yet.", m_sErrorPrefix);
		return (false);
	}

	if (m_idxBindVar >= MAXBINDVARS)
	{
		m_sLastError.Format ("{}BindByPos(): too many bind vars (MAXBINDVARS={})", m_sErrorPrefix, MAXBINDVARS);
		return (false);
	}

	m_iErrorNum = OCIBindByPos (
			(OCIStmt*)   m_dOCI8Statement,
			(OCIBind**)  &(m_OCIBindArray[m_idxBindVar++]),
			(OCIError*)  m_dOCI8ErrorHandle, 
			(ub4)        iPosition,
			(dvoid*)     pValue,
			(sb4)        iValueSize,
			(ub2)        iDataType,
			(dvoid*)     /*indp (IN/OUT)*/    nullptr, // Pointer to an indicator variable or array.
			(ub2*)       /*alenp (IN/OUT)*/   nullptr, // Pointer to array of actual lengths of array elements.
			(ub2*)       /*rcodep (OUT)*/     nullptr, // Pointer to array of column level return codes.
			(ub4)        /*maxarr_len(IN)*/   0,    // The maximum possible number of elements
			(ub4)        /*curelep (IN/OUT)*/ 0,    // Pointer to the actual number of elements.
			(ub4)        /*mode (IN)*/        OCI_DEFAULT // OCI_DEFAULT | OCI_DATA_AT_EXEC
	);

	if (!WasOCICallOK("BindByPos"))
	{
		kDebug (GetDebugLevel(), m_sLastError);
		return (false);
	}

	return (true);

} // _BindByPos - private

//-----------------------------------------------------------------------------
bool KSQL::BindByPos (uint32_t iPosition, char** psValue)
//-----------------------------------------------------------------------------
{
	return (_BindByPos (iPosition, (dvoid*)psValue, strlen(*psValue), SQLT_STR));

} // BindByPos

//-----------------------------------------------------------------------------
bool KSQL::BindByPos (uint32_t iPosition, int*   piValue)
//-----------------------------------------------------------------------------
{
	return (_BindByPos (iPosition, (dvoid*)piValue, sizeof(int), SQLT_INT));

} // BindByPos

//-----------------------------------------------------------------------------
bool KSQL::BindByPos (uint32_t iPosition, uint32_t*  piValue)
//-----------------------------------------------------------------------------
{
	return (_BindByPos (iPosition, (dvoid*)piValue, sizeof(uint32_t), SQLT_UIN));

} // BindByPos

//-----------------------------------------------------------------------------
bool KSQL::BindByPos (uint32_t iPosition, long*  piValue)
//-----------------------------------------------------------------------------
{
	return (_BindByPos (iPosition, (dvoid*)piValue, sizeof(long), SQLT_LNG));

} // BindByPos

//-----------------------------------------------------------------------------
bool KSQL::BindByPos (uint32_t iPosition, uint64_t* piValue)
//-----------------------------------------------------------------------------
{
	kAssert (sizeof(uint64_t) == sizeof(uint32_t), "KSQL:BindByPos"); // no SQLT_ULN !
	return (_BindByPos (iPosition, (dvoid*)piValue, sizeof(uint64_t), SQLT_UIN));

} // BindByPos

#endif

//-----------------------------------------------------------------------------
bool KSQL::Load (KROW& Row, bool bSelectAllColumns)
//-----------------------------------------------------------------------------
{
	if (!Row.FormSelect (m_sLastSQL, m_iDBType, bSelectAllColumns))
	{
		m_sLastError = Row.GetLastError();
		return (false);
	}

	if (!ExecLastRawQuery (0, "Load"))
	{
		return false;
	}

	return NextRow(Row);

} // Load


//-----------------------------------------------------------------------------
bool KSQL::ExecLastRawInsert(bool bIgnoreDupes)
//-----------------------------------------------------------------------------
{
	if (!IsFlag(F_NoTranslations))
	{
		DoTranslations (m_sLastSQL);
	}

	auto iSavedFlags = 0;

	if (bIgnoreDupes)
	{
		iSavedFlags = SetFlags (KSQL::F_IgnoreSQLErrors);
	}

	bool bOK = ExecLastRawSQL (0, "Insert");

	if (!bOK && bIgnoreDupes && WasDuplicateError())
	{
		bOK = true;
	}

	if (bIgnoreDupes)
	{
		SetFlags (iSavedFlags);
	}

	kDebug (GetDebugLevel(), "{} rows affected.", m_iNumRowsAffected);

	return (bOK);

} // ExecLastRawInsert

//-----------------------------------------------------------------------------
bool KSQL::Insert (const KROW& Row, bool bIgnoreDupes/*=false*/)
//-----------------------------------------------------------------------------
{
	if (!Row.FormInsert (m_sLastSQL, m_iDBType))
	{
		m_sLastError = Row.GetLastError();
		return (false);
	}

	return ExecLastRawInsert(bIgnoreDupes);

} // Insert

//-----------------------------------------------------------------------------
bool KSQL::Insert (const std::vector<KROW>& Rows, bool bIgnoreDupes)
//-----------------------------------------------------------------------------
{
	// assumed max packet size for sql API: 64 MB (default for MySQL)
	std::size_t iMaxPacket = 64 * 1024 * 1024;
	std::size_t iRows = 0;
	static constexpr std::size_t iMaxRows = 1000;
	uint64_t iFirstColumnHash = 0;

	m_sLastSQL.clear();


	for (const auto& Row : Rows)
	{
		// check that all rows have the same structure
		KHash ColHash;

		for (const auto& Col : Row)
		{
			// hash the name
			ColHash += Col.first;
		}

		if (iFirstColumnHash == 0)
		{
			iFirstColumnHash = ColHash.Hash();
		}
		else
		{
			if (iFirstColumnHash != ColHash.Hash())
			{
				m_sLastError = "differing column layout in rows - abort";
				kDebug(1, m_sLastError);
				return false;
			}
		}

		// do not use the "ignore" insert with SQLServer
		if (!Row.AppendInsert(m_sLastSQL, m_iDBType, false, GetDBType() != DBT::SQLSERVER))
		{
			m_sLastError = Row.GetLastError();
			return false;
		}

		// compute average size per row
		auto iAverageRowSize = m_sLastSQL.size() / ++iRows;

		if (iRows >= iMaxRows ||
			m_sLastSQL.size() + 3 * iAverageRowSize > iMaxPacket)
		{
			// next row gets too close to the maximum, flush now
			if (!ExecLastRawInsert(bIgnoreDupes))
			{
				return false;
			}

			iRows = 0;
			m_sLastSQL.clear();
		}
	}

	if (!iRows)
	{
		// we may either be here right after a flush or with an overall
		// empty input vector (which is then a fail)
		return !Rows.empty();
	}

	return ExecLastRawInsert(bIgnoreDupes);

} // Insert

//-----------------------------------------------------------------------------
bool KSQL::Update (const KROW& Row)
//-----------------------------------------------------------------------------
{
	if (!Row.FormUpdate (m_sLastSQL, m_iDBType))
	{
		m_sLastError = Row.GetLastError();
		return (false);
	}

	if (!IsFlag(F_NoTranslations))
	{
		DoTranslations (m_sLastSQL);
	}

	bool bOK = ExecLastRawSQL (0, "Update");

	kDebug (GetDebugLevel(), "{} rows affected.", m_iNumRowsAffected);

	return (bOK);

} // Update

//-----------------------------------------------------------------------------
bool KSQL::Delete (const KROW& Row)
//-----------------------------------------------------------------------------
{
	if (!Row.FormDelete (m_sLastSQL, m_iDBType))
	{
		m_sLastError = Row.GetLastError();
		return (false);
	}

	if (!IsFlag(F_NoTranslations))
	{
		DoTranslations (m_sLastSQL);
	}

	bool bOK = ExecLastRawSQL (0, "Delete");

	kDebug (GetDebugLevel(), "{} rows affected.", m_iNumRowsAffected);

	return (bOK);

} // Delete

//-----------------------------------------------------------------------------
bool KSQL::PurgeKey (KROW& OtherKeys, KStringView sPKEY, KStringView sValue, KJSON& ChangesMade, KStringView sIgnoreRegex/*=""*/)
//-----------------------------------------------------------------------------
{
	KROW row = OtherKeys;
	for (auto col : OtherKeys)
	{
		row.AddCol (col.first, col.second.sValue, col.second.GetFlags() | KROW::PKEY);
	}

	if (!ChangesMade.is_array())
	{
		ChangesMade = KJSON::array();
	}
	KJSON DataDict = FindColumn (sPKEY);

	if (!BeginTransaction ())
	{
		return (false);
	}

	for (auto& table : DataDict)
	{
		KJSON    obj;
		KString  sTableName = table["table_name"];
		uint64_t iChanged{0};

		obj["table_name"] = sTableName;

		if (sIgnoreRegex && sTableName.ToUpper().MatchRegex (sIgnoreRegex.ToUpper()))
		{
			obj["ignored"] = true;
			iChanged = 0;
		}
		else
		{
			row.SetTablename (kFormat ("{} /*KSQL::PurgeKey*/", sTableName));
			row.AddCol (sPKEY, sValue, KROW::PKEY);
			if (!Delete (row))
			{
				return (false);
			}
			iChanged = GetNumRowsAffected();
		}

		obj["rows_deleted"] = iChanged;
		ChangesMade += obj;
	}

	if (!CommitTransaction ())
	{
		return (false);
	}

	return (true);

} // PurgeKey

//-----------------------------------------------------------------------------
bool KSQL::PurgeKey (KStringView sPKEY, KStringView sValue, KJSON& ChangesMade, KStringView sIgnoreRegex/*=""*/)
//-----------------------------------------------------------------------------
{
	ChangesMade = KJSON::array();
	KJSON DataDict = FindColumn (sPKEY);

	if (!BeginTransaction ())
	{
		return (false);
	}

	for (auto& table : DataDict)
	{
		KJSON    obj;
		KString  sTableName = table["table_name"];
		uint64_t iChanged{0};

		obj["table_name"] = sTableName;

		if (sIgnoreRegex && sTableName.ToUpper().MatchRegex (sIgnoreRegex.ToUpper()))
		{
			obj["ignored"] = true;
			iChanged = 0;
		}
		else
		{
			if (!ExecSQL ("delete from %s /*KSQL::PurgeKey*/ where %s = binary '%s'", sTableName, sPKEY, sValue))
			{
				return (false);
			}
			iChanged = GetNumRowsAffected();
		}

		obj["rows_deleted"] = iChanged;
		ChangesMade += obj;
	}

	if (!CommitTransaction ())
	{
		return (false);
	}

	return (true);

} // PurgeKey

//-----------------------------------------------------------------------------
bool KSQL::PurgeKeyList (KStringView sPKEY, KStringView sInClause, KJSON& ChangesMade, KStringView sIgnoreRegex/*=""*/, bool bDryRun/*=false*/, int64_t* piNumAffected/*=NULL*/)
//-----------------------------------------------------------------------------
{
	ChangesMade = KJSON::array();
	KJSON DataDict = FindColumn (sPKEY);

	if (!BeginTransaction ())
	{
		return (false);
	}

	for (auto& table : DataDict)
	{
		KJSON    obj;
		KString  sTableName = table["table_name"];
		int64_t  iChanged{0};

		obj["table_name"] = sTableName;

		if (sIgnoreRegex && sTableName.ToUpper().MatchRegex (sIgnoreRegex.ToUpper()))
		{
			obj["ignored"] = true;
			iChanged = 0;
		}
		else if (bDryRun)
		{
			iChanged = SingleIntRawQuery (kFormat ("select count(*) from {} /*KSQL::PurgeKey*/ where {} in ({})", sTableName, sPKEY, sInClause));
			if (iChanged < 0)
			{
				return (false);
			}
			obj["rows_selected"] = iChanged;
		}
		else if (!ExecSQL ("delete from %s /*KSQL::PurgeKey*/ where %s in (%s)", sTableName, sPKEY, sInClause))
		{
			return (false);
		}
		else
		{
			iChanged = GetNumRowsAffected();
			obj["rows_deleted"] = iChanged;
		}
		if (piNumAffected)
		{
			*piNumAffected += iChanged;
		}

		ChangesMade += obj;
	}

	if (!CommitTransaction ())
	{
		return (false);
	}

	return (true);

} // PurgeKeyList

//-----------------------------------------------------------------------------
bool KSQL::BulkCopy (KSQL& OtherDB, KStringView sTablename, KStringView sWhereClause/*=""*/, uint16_t iFlushRows/*=1024*/, int32_t iPbarThreshold/*=500*/)
//-----------------------------------------------------------------------------
{
	KBAR    bar;
	int64_t iExpected{-1};
	bool    bPBAR = (iPbarThreshold >= 0);
	time_t  tStarted = time(NULL);

	if (bPBAR)
	{
		KOut.Format (":: {} : {:50} : ", kFormTimestamp(0,"%a %T"), sTablename);
	}
	
	if (bPBAR)
	{
		iExpected = OtherDB.SingleIntRawQuery (kFormat ("select count(*) from {} {}", sTablename, sWhereClause));
		if (iExpected < 0)
		{
			KOut.FormatLine (OtherDB.GetLastError());
			m_sLastError.Format ("{}: {}: {}", OtherDB.ConnectSummary(), OtherDB.GetLastError(), OtherDB.GetLastSQL());
			return false;
		}
	}

	if (bPBAR)
	{
		if (!iExpected)
		{
			KOut.FormatLine ("no rows to copy.");
		}
		else
		{
			KOut.Format ("{} rows... ", kFormNumber(iExpected));
			auto iTarget = SingleIntRawQuery (kFormat ("select count(*) from {} {}", sTablename, sWhereClause));
			if (iTarget < 0)
			{
				KOut.FormatLine ("table does not exist in target, skipping copy.");
			}
			else if (iTarget == iExpected)
			{
				KOut.FormatLine ("target already has {} rows, skipping copy.", kFormNumber(iExpected), sWhereClause);
			}
			else if (iTarget)
			{
				KOut.FormatLine ("target has {} rows, issuing a delete before the copy...", kFormNumber(iExpected));
				ExecRawSQL (kFormat ("delete from {} {}", sTablename, sWhereClause));
				KOut.FormatLine (":: purged {} rows from: {} {}", GetNumRowsAffected(), ConnectSummary(), sTablename);
			}
			else // target table is empty
			{
				KOut.FormatLine ("");
			}
		}
	}

	if (!OtherDB.ExecRawQuery (kFormat ("select * from {} {}", sTablename, sWhereClause)))
	{
		m_sLastError.Format ("{}: {}: {}", OtherDB.ConnectSummary(), OtherDB.GetLastError(), OtherDB.GetLastSQL());
		return false;
	}

	if (bPBAR && (iExpected >= iPbarThreshold))
	{
		bar.Start (iExpected);
	}

	std::vector<KROW> BulkRows;
	KROW row(sTablename);

	while (OtherDB.NextRow (row))
	{
		if (bPBAR)
		{
			bar.Move();
		}
		BulkRows.push_back (row);
		if (BulkRows.size() >= iFlushRows)
		{
			BulkCopyFlush (BulkRows);
		}
	}

	if (BulkRows.size())
	{
		BulkCopyFlush (BulkRows, /*bLast=*/true);
	}

	time_t tTook = time(NULL) - tStarted;
	if (bPBAR)
	{
		bar.Finish();
		if (tTook >= 10)
		{
			KOut.FormatLine (":: {} : {:50} : took {}\n", kFormTimestamp(0,"%a %T"), sTablename, kTranslateSeconds(tTook));
		}
	}

	return true;

} // BulkCopy

//-----------------------------------------------------------------------------
static void BulkCopyFlush_ (const KSQL& db, std::vector<KROW>/*copy*/ BulkRows)
//-----------------------------------------------------------------------------
{
	// db connection in args is only used for a template to open a new one for this thread:
	KSQL db2 (db);
	if (!db2.EnsureConnected())
	{
		KErr.FormatLine (">> spawned db connection: {}", db2.GetLastError());
		return;
	}

	if (!db2.Insert (BulkRows))
	{
		KErr.FormatLine (">> spawned db connection: {}", db2.GetLastError());
		return;
	}

} // BulkCopyFlush_

//-----------------------------------------------------------------------------
void KSQL::BulkCopyFlush (std::vector<KROW>& BulkRows, bool bLast/*=true*/)
//-----------------------------------------------------------------------------
{
	// do bulk-insert on it's own thread so that we can move to the next bunch of rows:
	std::thread SpawnMe (BulkCopyFlush_, *this, /*copy*/BulkRows);
	if (bLast)
	{
		SpawnMe.join();
	}
	else
	{
		SpawnMe.detach();
	}

	BulkRows.clear();

} // BulkCopyFlush

//-----------------------------------------------------------------------------
bool KSQL::UpdateOrInsert (const KROW& Row, bool* pbInserted/*=nullptr*/)
//-----------------------------------------------------------------------------
{
	if (pbInserted)
	{
		*pbInserted = false;
	}

	kDebug (GetDebugLevel()+1, "trying update first...");
	if (!Update (Row))
	{
		return (false); // syntax error in SQL
	}

	if (GetNumRowsAffected() >= 1)
	{
		return (true); // update caught something
	}

	kDebug (GetDebugLevel(), "update caught no rows: proceed to insert...");

	if (pbInserted)
	{
		*pbInserted = true;
	}

	kDebug (GetDebugLevel()+1, "attempting insert...");

	return Insert (Row);

} // UpdateOrInsert

//-----------------------------------------------------------------------------
bool KSQL::UpdateOrInsert (KROW& Row, const KROW& AdditionalInsertCols, bool* pbInserted/*=nullptr*/)
//-----------------------------------------------------------------------------
{
	if (pbInserted)
	{
		*pbInserted = false;
	}

	kDebug (GetDebugLevel()+1, "trying update first...");
	if (!Update (Row))
	{
		return (false); // syntax error in SQL
	}

	if (GetNumRowsAffected() >= 1)
	{
		return (true); // update caught something
	}

	kDebug (GetDebugLevel(), "update caught no rows: proceed to insert...");

	if (pbInserted)
	{
		*pbInserted = true;
	}

	kDebug (GetDebugLevel()+1, "attempting insert...");

	Row += AdditionalInsertCols;

	return Insert (Row);

} // UpdateOrInsert

//-----------------------------------------------------------------------------
uint64_t KSQL::GetLastInsertID ()
//-----------------------------------------------------------------------------
{
	if (m_iDBType == DBT::SQLSERVER)
	{
		int64_t iID = SingleIntQuery ("select @@identity");
		if (iID <= 0)
		{
			m_iLastInsertID = 0;
		}
		else
		{
			m_iLastInsertID = iID;
		}
	}

	return (m_iLastInsertID);

} // GetLastInsertID

#ifdef DEKAF2_HAS_CTLIB

//-----------------------------------------------------------------------------
bool KSQL::ctlib_is_initialized()
//-----------------------------------------------------------------------------
{
	if (!m_pCtContext)
	{
		kDebug(KSQL2_CTDEBUG, "m_pCtContext is invalid");
		return false;
	}

	if (!m_pCtConnection)
	{
		kDebug(KSQL2_CTDEBUG, "m_pCtConnection is invalid");
		return false;
	}

	if (!m_pCtCommand)
	{
		kDebug(KSQL2_CTDEBUG, "m_pCtCommand is invalid");
		return false;
	}

	return true;

} // ctlib_is_initialized

//-----------------------------------------------------------------------------
bool KSQL::ctlib_login ()
//-----------------------------------------------------------------------------
{
	m_sLastError.clear();

	kDebug (KSQL2_CTDEBUG, "cs_ctx_alloc()");
	if (cs_ctx_alloc (CS_VERSION_100, &m_pCtContext) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>cs_ctx_alloc");
		return (SQLError());
	}

	#if 0
	/* Force default date format, some tests rely on it */
	TDSCONTEXT *tds_ctx;
	tds_ctx = (TDSCONTEXT *) (*ctx)->tds_ctx;
	if (tds_ctx && tds_ctx->locale && tds_ctx->locale->date_fmt)
	{
		free(tds_ctx->locale->date_fmt);
		tds_ctx->locale->date_fmt = strdup("%b %e %Y %I:%M%p");
	}
	#endif

	kDebug (KSQL2_CTDEBUG, "ct_init()");
	if (ct_init (m_pCtContext, CS_VERSION_100) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>ct_init failed");
		return (SQLError());
	}

	kDebug (KSQL2_CTDEBUG, "ct_con_alloc()");
	if (ct_con_alloc (m_pCtContext, &m_pCtConnection) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>ct_con_alloc");
		return (SQLError());
	}

	kDebug (KSQL2_CTDEBUG, "ct_con_props() set username");
	auto sUser = GetDBUser();
	if (ct_con_props (m_pCtConnection, CS_SET, CS_USERNAME, sUser.data(), sUser.size(), nullptr) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>set username");
		return (SQLError());
	}

	kDebug (KSQL2_CTDEBUG, "ct_con_props() set password");
	auto sPass = GetDBPass();
	if (ct_con_props (m_pCtConnection, CS_SET, CS_PASSWORD, sPass.data(), sPass.size(), nullptr) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>set password");
		return (SQLError());
	}

	kDebug (KSQL2_CTDEBUG, "connecting as {} to {}.{}\n", GetDBUser(), GetDBHost(), GetDBName());
	int iTryConnect = 0;
	while(true)
	{
		auto sHost = GetDBHost();
		if (ct_connect (m_pCtConnection, sHost.data(), sHost.size()) == CS_SUCCEED)
		{
			break;
		}
		else if (++iTryConnect >= 3)
		{
			//try to connect 3 times.
			ctlib_api_error ("ctlib_login>ct_connect");
			return (SQLError());
		}
	}

	kDebug (KSQL2_CTDEBUG, "ct_cmd_alloc()");
	if (ct_cmd_alloc (m_pCtConnection, &m_pCtCommand) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>ct_cmd_alloc");
		return (SQLError());
	}

	if (!GetDBName().empty())
	{
		if (ctlib_execsql(kFormat("use {}", GetDBName())) != CS_SUCCEED)
		{
			m_sLastError.Format ("{}CTLIB: login ok, could not attach to database: {}", m_sErrorPrefix, GetDBName());
			return (false);
		}
	}

	kDebug (GetDebugLevel()+1, "ensuring that we can process error messages 'inline' instead of using callbacks...");

	kDebug (KSQL2_CTDEBUG, "ct_diag(CS_INIT)");
	if (ct_diag (m_pCtConnection, CS_INIT, CS_UNUSED, CS_UNUSED, nullptr) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>ct_init(CS_INIT) failed");
		return (SQLError());
	}

	return (true);

} // ctlib_login

//-----------------------------------------------------------------------------
bool KSQL::ctlib_logout ()
//-----------------------------------------------------------------------------
{
	if (m_pCtConnection)
	{
		kDebug (KSQL2_CTDEBUG, "calling ct_cancel...");
		if (ct_cancel(m_pCtConnection, nullptr, CS_CANCEL_ALL) != CS_SUCCEED)
		{
			ctlib_api_error ("ctlib_logout>ct_cancel");
			return (SQLError());
		}
	}

	if (m_pCtCommand)
	{
		kDebug (KSQL2_CTDEBUG, "calling ct_cmd_drop...");
		ct_cmd_drop (m_pCtCommand);
		m_pCtCommand = nullptr;
	}

	if (m_pCtConnection)
	{
		kDebug (KSQL2_CTDEBUG, "calling ct_close...");
		ct_close    (m_pCtConnection, CS_UNUSED);

		kDebug (KSQL2_CTDEBUG, "calling ct_con_drop...");
		ct_con_drop (m_pCtConnection);
		m_pCtConnection = nullptr;
	}

	if (m_pCtContext)
	{
		kDebug (KSQL2_CTDEBUG, "calling ct_exit...");
		ct_exit     (m_pCtContext, CS_UNUSED);

		kDebug (KSQL2_CTDEBUG, "calling cs_ctx_drop...");
		cs_ctx_drop (m_pCtContext);
		m_pCtContext = nullptr;
	}

	return (true);

} // ctlib_logout

//-----------------------------------------------------------------------------
uint64_t KSQL::ctlib_get_rows_affected()
//-----------------------------------------------------------------------------
{
	if (!m_iNumRowsAffected && m_pCtCommand)
	{
		CS_INT iNumRows;
		kDebug (KSQL2_CTDEBUG, "calling ct_res_info...");
		if (ct_res_info (m_pCtCommand, CS_ROW_COUNT, &iNumRows, CS_UNUSED, nullptr) == CS_SUCCEED)
		{
			if ((long)iNumRows != -1)
			{
				m_iNumRowsAffected = (uint64_t) iNumRows;
			}
			kDebug (KSQL2_CTDEBUG, "m_iNumRowsAffected = {}", m_iNumRowsAffected);
		}
	}

	return m_iNumRowsAffected;
}

//-----------------------------------------------------------------------------
bool KSQL::ctlib_execsql (KStringView sSQL)
//-----------------------------------------------------------------------------
{
	if (!ctlib_is_initialized())
	{
		return false;
	}

	m_iNumRowsAffected = 0;

	CS_RETCODE iApiRtn;
	CS_INT     iResultType;

	kDebug (KSQL2_CTDEBUG, sSQL);
	kDebug (KSQL2_CTDEBUG, "ensuring that no dangling messages are in the ct queue...");
	ctlib_flush_results ();
	ctlib_clear_errors ();

	kDebug (KSQL2_CTDEBUG, "calling ct_command()...");
	if (ct_command (m_pCtCommand, CS_LANG_CMD, sSQL.data(), sSQL.size(), CS_UNUSED) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_execsql>ct_command");
		return (SQLError());
	}

	kDebug (KSQL2_CTDEBUG, "calling ct_send()...");
	if (ct_send (m_pCtCommand) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_execsql>ct_send");
		return (SQLError());
	}

	kDebug (KSQL2_CTDEBUG, "calling ct_results()...");
	while ((iApiRtn = ct_results (m_pCtCommand, &iResultType)) == CS_SUCCEED)
	{
		kDebug (KSQL2_CTDEBUG, "ct_results said iResultsType={} and returned {}", iResultType, iApiRtn);

		// see http://infocenter.sybase.com/help/index.jsp?topic=/com.sybase.help.sdk_12.5.1.ctref/html/ctref/ctref334.htm

		switch (iResultType)
		{
			case CS_CMD_SUCCEED:
				break;

			case CS_CMD_DONE:
				ctlib_get_rows_affected();
				break;

			case CS_CMD_FAIL:
				kDebug (KSQL2_CTDEBUG, "iResultType CS_CMD_FAIL");
				break;

			case CS_COMPUTE_RESULT:
			case CS_CURSOR_RESULT:
			case CS_PARAM_RESULT:
			case CS_STATUS_RESULT:
			case CS_ROW_RESULT:
				kDebug (KSQL2_CTDEBUG, "receiving fetchable results on a non-query: {}", sSQL);
				ctlib_flush_results();
				return true; // false would trigger a retry ...

			case CS_COMPUTEFMT_RESULT:
			case CS_MSG_RESULT:
			case CS_ROWFMT_RESULT:
			case CS_DESCRIBE_RESULT:
			default:
				kDebug (KSQL2_CTDEBUG, "iResultType ??? ({})", iResultType);
				ctlib_api_error ("ctlib_execsql>ct_results");
				return (SQLError());
		}
	}

	uint32_t iNumErrors = ctlib_check_errors();
	if (iNumErrors)
	{
		kDebug (KSQL2_CTDEBUG, "returning false");
		return (false);
	}
	else
	{
		kDebug (KSQL2_CTDEBUG, "returning true");
		return (true);
	}

} // ctlib_execsql

//-----------------------------------------------------------------------------
bool KSQL::ctlib_nextrow ()
//-----------------------------------------------------------------------------
{
	if (!ctlib_is_initialized())
	{
		return false;
	}

	// make sure SQL nullptr values get left as zero-terminated C strings:
	for (auto& Col : m_dColInfo)
	{
		Col.ClearContent();
	}

	CS_INT iFetched = 0;

	kDebug (KSQL2_CTDEBUG, "calling ct_fetch...");
	if ((ct_fetch (m_pCtCommand, CS_UNUSED, CS_UNUSED, CS_UNUSED, &iFetched) == CS_SUCCEED) && (iFetched > 0))
	{
		for (uint32_t ii = 0; ii < m_iNumColumns; ++ii)
		{
			// map SQL nullptr values to zero-terminated C strings:
			if (m_dColInfo[ii].indp < 0)
			{
				// check that the unique_ptr is allocated
				if (m_dColInfo[ii].dszValue)
				{
					m_dColInfo[ii].dszValue.get()[0] = 0;
				}
			}
		}

		++m_iRowNum;
		return (true); // we got a row
	}

	return (false); // we did not get a row

} // ctlib_nextrow

//-----------------------------------------------------------------------------
bool KSQL::ctlib_api_error (KStringView sContext)
//-----------------------------------------------------------------------------
{
	kDebug (3, sContext);

	uint32_t iMessages = ctlib_check_errors ();
	if (!iMessages)
	{
		m_sLastError.Format ("CT-Lib: API Error: {}", sContext);
	}

	return (false);

} // ctlib_api_error

//-----------------------------------------------------------------------------
uint32_t KSQL::ctlib_check_errors ()
//-----------------------------------------------------------------------------
{
	if (!ctlib_is_initialized())
	{
		return 1;
	}

	uint32_t     iNumErrors = 0;
	CS_INT       iNumMsgs   = 0;
	CS_CLIENTMSG ClientMsg; memset (&ClientMsg, 0, sizeof(ClientMsg));
	CS_SERVERMSG ServerMsg; memset (&ServerMsg, 0, sizeof(ServerMsg));
	int          ii;

	m_sLastError.clear();

	kDebug (KSQL2_CTDEBUG, "calling ct_diag...");

	if (ct_diag (m_pCtConnection, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED, &iNumMsgs) != CS_SUCCEED)
	{
		//return (++iNumErrors); //commented out: don't let ct_diag crash ctlib_check_errors
	}

	if (iNumMsgs)
	{
		kDebug (GetDebugLevel(), "{} client messages", iNumMsgs);
	}

	for (ii=0; ii < iNumMsgs; ++ii)
	{

		kDebug (KSQL2_CTDEBUG, "calling ct_diag...");

		if (ct_diag (m_pCtConnection, CS_GET, CS_CLIENTMSG_TYPE, ii + 1, &ClientMsg) != CS_SUCCEED)
		{
			//return (++iNumErrors);
		}

		// typedef struct _cs_clientmsg
		// {
		// 	CS_INT severity;
		// 	CS_MSGNUM msgnumber;
		// 	CS_CHAR msgstring[CS_MAX_MSG];
		// 	CS_INT msgstringlen;
		// 	CS_INT osnumber;
		// 	CS_CHAR osstring[CS_MAX_MSG];
		// 	CS_INT osstringlen;
		// 	CS_INT status;
		// 	CS_BYTE sqlstate[CS_SQLSTATE_SIZE];
		// 	CS_INT sqlstatelen;
		// } CS_CLIENTMSG;

		m_iErrorNum = ClientMsg.msgnumber;

		if (ii)
		{
			m_sLastError += "\n";	
		}
		m_sLastError += m_sErrorPrefix;	

		KString sAdd;
		sAdd.Format("CTLIB-{:05}: ", ClientMsg.msgnumber);
		m_sLastError += sAdd;	

		int iLayer    = CS_LAYER(ClientMsg.msgnumber);
		int iOrigin   = CS_ORIGIN(ClientMsg.msgnumber);
		int iSeverity = ClientMsg.severity; //CS_SEVERITY(ClientMsg.msgnumber);

		if (iLayer)
		{
			sAdd.Format("Layer {}, ", iLayer);
			m_sLastError+=sAdd;	
		}

		if (iOrigin)
		{
			sAdd.Format("Origin {}, ", iOrigin);
			m_sLastError+=sAdd;
		}

		if (iSeverity)
		{
			sAdd.Format("Severity {}, ", iSeverity);
			m_sLastError+=sAdd;
		}

		sAdd = ClientMsg.msgstring;
		m_sLastError+=sAdd;

		if (ClientMsg.osstringlen > 0)
		{
			sAdd.Format("{}OS-{:05}: {}", m_sErrorPrefix, ClientMsg.osnumber, ClientMsg.osstring);
			m_sLastError+=sAdd;
		}

		++iNumErrors;
	}

	kDebug (KSQL2_CTDEBUG, "calling ct_diag...");

	if (ct_diag (m_pCtConnection, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED, &iNumMsgs) != CS_SUCCEED)
	{
		//return (++iNumErrors);
	}

	if (iNumMsgs)
	{
		kDebug (GetDebugLevel(), "{} server message", iNumMsgs);
	}

	for (ii=0; ii < iNumMsgs; ++ii)
	{
		kDebug (KSQL2_CTDEBUG, "calling ct_diag... message #{:02}", ii);

		if (ct_diag (m_pCtConnection, CS_GET, CS_SERVERMSG_TYPE, ii + 1, &ServerMsg) != CS_SUCCEED)
		{
			//return (++iNumErrors);
		}

		// typedef struct _cs_servermsg
		// {
		// 	CS_MSGNUM msgnumber;
		// 	CS_INT state;
		// 	CS_INT severity;
		// 	CS_CHAR text[CS_MAX_MSG];
		// 	CS_INT textlen;
		// 	CS_CHAR svrname[CS_MAX_NAME];
		// 	CS_INT svrnlen;
		// 	CS_CHAR proc[CS_MAX_NAME];
		// 	CS_INT proclen;
		// 	CS_INT line;
		// 	CS_INT status;
		// 	CS_BYTE sqlstate[CS_SQLSTATE_SIZE];
		// 	CS_INT sqlstatelen;
		// } CS_SERVERMSG;

		if (!m_iErrorNum)
		{
			m_iErrorNum = ServerMsg.msgnumber;
		}
		else
		{
			kDebug(KSQL2_CTDEBUG, "error {} already set from client, server adds {}", ServerMsg.msgnumber);
		}

		if (ii)
		{
			m_sLastError += "\n";
		}
		m_sLastError += m_sErrorPrefix;

		KString sAdd;
		sAdd.Format("SQL-{:05}: ", ServerMsg.msgnumber);
		m_sLastError += sAdd;

		if (ServerMsg.proclen)
		{
			m_sLastError += ServerMsg.proc;
		}

		if (ServerMsg.state)
		{
			sAdd.Format("State {}, ", ServerMsg.state);
			m_sLastError += sAdd;
		}

		if (ServerMsg.severity)
		{
			sAdd.Format("Severity {}, ", ServerMsg.severity);
			m_sLastError += sAdd;
		}

		if (ServerMsg.line)
		{
			sAdd.Format("Line {}, ", ServerMsg.line);
			m_sLastError += sAdd;
		}

		m_sLastError += ServerMsg.text;

		++iNumErrors;
	}

	return (iNumErrors);

} // ctlib_check_errors

//-----------------------------------------------------------------------------
bool KSQL::ctlib_clear_errors ()
//-----------------------------------------------------------------------------
{
	if (!ctlib_is_initialized())
	{
		return false;
	}

	m_iErrorNum = 0;

	kDebug (KSQL2_CTDEBUG, "calling ct_diag...");
	if (ct_diag (m_pCtConnection, CS_CLEAR, CS_ALLMSG_TYPE, CS_UNUSED, nullptr) != CS_SUCCEED)
	{
		kDebug (1, "ct_diag(CS_CLEAR) failed");
	}

	CS_INT iNumMsgs = 0;

	kDebug (KSQL2_CTDEBUG, "calling ct_diag...");
	if (ct_diag (m_pCtConnection, CS_STATUS, CS_ALLMSG_TYPE, CS_UNUSED, &iNumMsgs) != CS_SUCCEED)
	{
		kDebug (1, "ct_diag(CS_STATUS) failed");
	}

	if (iNumMsgs != 0)
	{
		kDebug (1, "ct_diag(CS_CLEAR) failed: there are still {} messages on queue.", iNumMsgs);
		return (false);
	}

	return (true);

} // ctlib_clear_errors

//-----------------------------------------------------------------------------
bool KSQL::ctlib_prepare_results ()
//-----------------------------------------------------------------------------
{
	if (!ctlib_is_initialized())
	{
		return false;
	}

	bool   bLooping    = true;
	bool   bFailed     = false;
	CS_INT iResultType;

	while (bLooping && (ct_results (m_pCtCommand, &iResultType) == CS_SUCCEED))
	{
		switch ((int) iResultType)
		{
			case CS_CMD_SUCCEED:
				kDebug (KSQL2_CTDEBUG, "ct_results: CS_CMD_SUCCEED");
				break;

			case CS_CMD_DONE:
				kDebug (KSQL2_CTDEBUG, "ct_results: CS_CMD_DONE");
				bLooping = false;
				break;

			case CS_CMD_FAIL:
				kDebug (KSQL2_CTDEBUG, "ct_results: CS_CMD_FAIL");
				bFailed = true;
				ctlib_api_error ("ExecQuery>ctlib_prepare_results>ct_results>CS_CMD_FAIL");
				break;

			case CS_COMPUTE_RESULT:
			case CS_CURSOR_RESULT:
			case CS_PARAM_RESULT:
			case CS_STATUS_RESULT:
			case CS_ROW_RESULT:
				kDebug (KSQL2_CTDEBUG, "ct_results: ready for data, got result type {}", iResultType);
				bLooping = false;
				break; // ready for data

			default:
				kDebug (KSQL2_CTDEBUG, "ct_results: {}", iResultType);
				return (ctlib_api_error ("ExecQuery>ctlib_prepare_results>ct_results>???"));
		}

		if (bLooping)
		{
			kDebug (KSQL2_CTDEBUG, "calling ct_results");
		}
	}

	if (bFailed)
	{
		return false;
	}

	kDebug (KSQL2_CTDEBUG, "ready for query results");

	CS_INT iNumCols;
	kDebug (KSQL2_CTDEBUG, "calling ct_res_info");
	if (ct_res_info (m_pCtCommand, CS_NUMDATA, &iNumCols, CS_UNUSED, nullptr) != CS_SUCCEED)
	{
		ctlib_api_error ("NextRow>ct_res_info");
		return (SQLError());
	}
	m_iNumColumns = (uint32_t) iNumCols;

	m_dColInfo.clear();
	m_dColInfo.reserve(m_iNumColumns);

	for (uint32_t ii = 0; ii < m_iNumColumns; ++ii)
	{
		kDebug (KSQL2_CTDEBUG, "retrieving info about col #{:02}", ii);

		CS_DATAFMT colinfo;
		memset (&colinfo, 0, sizeof(colinfo));

		kDebug (KSQL2_CTDEBUG, "using ct_describe() to get column info for col#{:02}...", ii+1);
		if (ct_describe (m_pCtCommand, ii+1, &colinfo) != CS_SUCCEED)
		{
			ctlib_api_error ("NextRow>ct_describe) failed for a column");
			return (SQLError());
		}

		if (colinfo.status & CS_RETURN)
		{
			kDebug (GetDebugLevel(), "CS_RETURN code: column {}  -- ignored", ii); // ignored
		}

		KColInfo ColInfo;

		// TODO check if DBT::SYBASE is the right DBType
		ColInfo.SetColumnType(DBT::SYBASE, ColInfo.iKSQLDataType, std::max(colinfo.maxlength, 8000)); // <-- allocate at least the max-varchar length to avoid overflows
		ColInfo.sColName        = (colinfo.namelen) ? colinfo.name : "";

		enum {SANITY_MAX = 50*1024};
		if (ColInfo.iMaxDataLen > SANITY_MAX)
		{
			kDebug (KSQL2_CTDEBUG, " col#{:02}: name='{}':  maxlength changed from {} to {}",
					ii+1, ColInfo.sColName, ColInfo.iMaxDataLen, SANITY_MAX);
			ColInfo.iMaxDataLen = SANITY_MAX;
		}

		kDebug (KSQL2_CTDEBUG, " col#{:02}: name='{}', maxlength={}, datatype={}",
				ii+1, ColInfo.sColName, ColInfo.iMaxDataLen, ColInfo.iKSQLDataType);

		// allocate at least 20 bytes, which will cover datetimes and numerics:
		ColInfo.iMaxDataLen = std::max (ColInfo.iMaxDataLen+2, KCOL::Len(20));

		ColInfo.dszValue  = std::make_unique<char[]>(ColInfo.iMaxDataLen + 1);

		// set up bindings for the data:
		CS_DATAFMT datafmt;
		memset (&datafmt, 0, sizeof(datafmt));
		datafmt.datatype  = CS_CHAR_TYPE;
		datafmt.format    = CS_FMT_NULLTERM;
		datafmt.maxlength = ColInfo.iMaxDataLen;
		datafmt.count     = 1;
		datafmt.locale    = nullptr;

		kDebug (KSQL2_CTDEBUG, "calling ct_bind...");
		if (ct_bind (m_pCtCommand, ii+1, &datafmt, ColInfo.dszValue.get(), nullptr, &(ColInfo.indp)) != CS_SUCCEED)
		{
			ctlib_api_error ("NextRow>ct_bind");
			return (SQLError());
		}

		m_dColInfo.push_back(std::move(ColInfo));

	} // for each column

	return (true); // ready for dta

} // ctlib_prepare_results

//-----------------------------------------------------------------------------
void KSQL::ctlib_flush_results ()
//-----------------------------------------------------------------------------
{
	if (!ctlib_is_initialized())
	{
		return;
	}

	kDebug (KSQL2_CTDEBUG, "[to flush prior results that might be dangling]");

	CS_INT     iResultType;
	bool       bLooping = true;
	bool       bFlush   = false;

	kDebug (KSQL2_CTDEBUG, "calling ct_results...");
	while (bLooping && (ct_results (m_pCtCommand, &iResultType) == CS_SUCCEED))
	{
		switch ((int) iResultType)
		{
		case CS_CMD_SUCCEED:
			kDebug (KSQL2_CTDEBUG, "ct_results: CS_CMD_SUCCEED");
			break;
		case CS_CMD_DONE:
			kDebug (KSQL2_CTDEBUG, "ct_results: CS_CMD_DONE");
			bLooping = false;
			break;
		case CS_STATUS_RESULT:
			kDebug (KSQL2_CTDEBUG, "ct_results: CS_STATUS_RESULT");
			bLooping = false;
			break;
		case CS_CMD_FAIL:
			kDebug (KSQL2_CTDEBUG, "ct_results: CS_CMD_FAIL");
			break;
		case CS_CURSOR_RESULT:
			kDebug (KSQL2_CTDEBUG, "ct_results: CS_CURSOR_RESULT");
			bLooping = false;
			bFlush   = true;
			break;
		case CS_ROW_RESULT:
			kDebug (KSQL2_CTDEBUG, "ct_results: CS_ROW_RESULT");
			bLooping = false;
			bFlush   = true;
			break;
		case CS_COMPUTE_RESULT:
			kDebug (KSQL2_CTDEBUG, "ct_results: CS_COMPUTE_RESULT");
			bLooping = false;
			break;
		default:
			kDebug (KSQL2_CTDEBUG, "ct_results: {}", iResultType);
			bLooping = false;
			break;
		}

		if (bLooping)
		{
			kDebug (KSQL2_CTDEBUG, "calling ct_results...");
		}
	}

	kDebug (KSQL2_CTDEBUG, "calling ct_cancel...");
	if (ct_cancel(m_pCtConnection, nullptr, CS_CANCEL_ALL) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_flush_results>ct_cancel");
	}

	CS_INT iFetched;
	kDebug (KSQL2_CTDEBUG, "ctlib_nextrow: calling ct_fetch...");
	if (bFlush && (ct_fetch (m_pCtCommand, CS_UNUSED, CS_UNUSED, CS_UNUSED, &iFetched) == CS_SUCCEED) && (iFetched > 0))
	{
		kDebug (KSQL2_CTDEBUG, "dumping row...\n");
	}

} // ctlib_flush_results

#endif

//-----------------------------------------------------------------------------
size_t KSQL::OutputQuery (KString sSQL, KStringView sFormat, FILE* fpout/*=stdout*/)
//-----------------------------------------------------------------------------
{
	OutputFormat iFormat = FORM_ASCII;

	if ((sFormat == "-ascii") || (sFormat == "-query"))
	{
		iFormat = FORM_ASCII;
	}
	else if (sFormat == "-csv")
	{
		iFormat = FORM_CSV;
	}
	else if (sFormat == "-html")
	{
		iFormat = FORM_HTML;
	}

	return (OutputQuery (std::move(sSQL), iFormat, fpout));

} // OutputQuery

//-----------------------------------------------------------------------------
size_t KSQL::OutputQuery (KString sSQL, OutputFormat iFormat/*=FORM_ASCII*/, FILE* fpout/*=stdout*/)
//-----------------------------------------------------------------------------
{
	if (!ExecRawQuery (std::move(sSQL), GetFlags(), "OutputQuery"))
	{
		return (-1);
	}

	KProps<KString, std::size_t, false, true> Widths;
	KROW   Row;
	size_t iNumRows = 0;
	enum   { MAXCOLWIDTH = 80 };

	if (iFormat == FORM_ASCII)
	{
		while (NextRow (Row))
		{
			if (++iNumRows == 1) // factor col headers into col widths
			{
				for (const auto& it : Row)
				{
					const KString& sName(it.first);
					const KString& sValue(it.first);
					auto iLen = sValue.length();
					auto iMax = Widths.Get (sName);
					if ((iLen > iMax) && (iLen <= MAXCOLWIDTH))
					{
						Widths.Add (sName, iLen);
					}
				}
			}

			for (const auto& it : Row)
			{
				const KString& sName(it.first);
				const KString& sValue(it.second.sValue);
				auto iLen = sValue.length();
				auto iMax = Widths.Get (sName);
				if ((iLen > iMax) && (iLen <= MAXCOLWIDTH))
				{
					Widths.Add (sName, iLen);
				}
			}
		}
		EndQuery ();
		ExecLastRawQuery (GetFlags(), "OutputQuery");
	}

	iNumRows = 0;
	while (NextRow (Row))
	{
		// output column headers:
		if (++iNumRows == 1)
		{
			bool bFirst { true };
			switch (iFormat)
			{
				case FORM_ASCII:
					for (const auto& it : Row)
					{
						const KString& sName = it.first;
						int iMax = static_cast<int>(Widths.Get (sName));
						fprintf (fpout, "%s%-*.*s-+", (bFirst) ? "+-" : "-", iMax, iMax, KLog::BAR.c_str());
						bFirst = false;
					}
					fprintf (fpout, "\n");
					for (const auto& it : Row)
					{
						const KString& sName = it.first;
						int iMax = static_cast<int>(Widths.Get (sName));
						fprintf (fpout, "%s%-*.*s |", (bFirst) ? "| " : " ", iMax, iMax, sName.c_str());
						bFirst = false;
					}
					fprintf (fpout, "\n");
					for (const auto& it : Row)
					{
						const KString& sName = it.first;
						int iMax = static_cast<int>(Widths.Get (sName));
						fprintf (fpout, "%s%-*.*s-+", (bFirst) ? "+-" : "-", iMax, iMax, KLog::BAR.c_str());
						bFirst = false;
					}
					fprintf (fpout, "\n");
					break;
				case FORM_HTML:
					fprintf (fpout, "<table>\n");
					fprintf (fpout, "<tr>\n");
					for (const auto& it : Row)
					{
						const KString& sName = it.first;
						fprintf (fpout, " <th>%s</th>\n", sName.c_str());
					}
					fprintf (fpout, "</tr>\n");
					break;
				case FORM_CSV:
					for (const auto& it : Row)
					{
						const KString& sName = it.first;
						fprintf (fpout, "%s\"%s\"", (bFirst) ? "" : ",", sName.c_str());
						bFirst = false;
					}
					fprintf (fpout, "\n");
					break;
			}
		}

		// output row:
		bool bFirst { true };
		switch (iFormat)
		{
			case FORM_ASCII:
				for (const auto& it : Row)
				{
					const KString& sName  = it.first;
					const KString& sValue = it.second.sValue;
					int iMax = static_cast<int>(Widths.Get (sName));
					fprintf (fpout, "%s%-*.*s |", (bFirst) ? "| " : " ", iMax, iMax, sValue.c_str());
					bFirst = false;
				}
				fprintf (fpout, "\n");
				break;
			case FORM_HTML:
				fprintf (fpout, "<tr>\n");
				for (const auto& it : Row)
				{
					const KString& sValue = it.second.sValue;
					fprintf (fpout, " <td>%s</td>\n", sValue.c_str());
				}
				fprintf (fpout, "</tr>\n");
				break;
			case FORM_CSV:
				for (const auto& it : Row)
				{
					const KString& sValue = it.second.sValue;
					fprintf (fpout, "%s\"%s\"", (bFirst) ? "" : ",", sValue.c_str());
					bFirst = false;
				}
				fprintf (fpout, "\n");
				break;
		}

	} // while

	if (iNumRows)
	{
		bool bFirst { true };
		switch (iFormat)
		{
			case FORM_CSV:
				break;
			case FORM_HTML:
				fprintf (fpout, "</table>\n");
				break;
			case FORM_ASCII:
				for (const auto& it : Row)
				{
					const KString& sName  = it.first;
					int iMax  = static_cast<int>(Widths.Get (sName));
					fprintf (fpout, "%s%-*.*s-+", (bFirst) ? "+-" : "-", iMax, iMax, KLog::BAR.c_str());
					bFirst = false;
				}
				fprintf (fpout, "\n");
				break;
		}
	}

	return (iNumRows);

} // OutputQuery

//-----------------------------------------------------------------------------
void KSQL::ResetErrorStatus ()
//-----------------------------------------------------------------------------
{
	m_iErrorNum  = 0;
	m_sLastError.clear();

} // ResetErrorStatus

//-----------------------------------------------------------------------------
bool KSQL::BeginTransaction (KStringView sOptions/*=""*/)
//-----------------------------------------------------------------------------
{
	// TODO: code for non-MySQL

	return ExecSQL ("start transaction%s%s", sOptions.empty() ? "" : " ", sOptions.empty() ? KStringView("") : sOptions);

} // BeginTransaction

//-----------------------------------------------------------------------------
bool KSQL::CommitTransaction (KStringView sOptions/*=""*/)
//-----------------------------------------------------------------------------
{
	// TODO: code for non-MySQL

	auto iSave = GetNumRowsAffected();
	bool bOK   = ExecSQL ("commit%s%s", sOptions.empty() ? "" : " ", sOptions.empty() ? KStringView("") : sOptions);
	m_iNumRowsAffected = GetNumRowsAffected() + iSave;

	return bOK;

} // CommitTransaction

//-----------------------------------------------------------------------------
KString KSQL::FormAndClause (KStringView sDbCol, KStringView sQueryParm, uint64_t iFlags/*=FAC+NORMAL*/, KStringView sSplitBy/*=","*/)
//-----------------------------------------------------------------------------
{
	KString sClause;

	m_sLastError.clear();
	if (sQueryParm.empty())
	{
		return sClause; // empty
	}

	if (NeedsEscape(sQueryParm))
	{
		// we do not expect escapable characters here
		kWarning ("possible SQL injection attempt: {}", sQueryParm);
		if (m_TimingCallback)
		{
			m_TimingCallback (*this, /*iMilliseconds=*/0, kFormat ("{}:\n{}", m_sLastError, sQueryParm));
		}
		// note: probably leave m_sLastError alone
		return sClause; // empty
	}

	KString sLowerParm (sQueryParm);
	sLowerParm.MakeLower();

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// BETWEEN logic:
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	if (iFlags & FAC_BETWEEN)
	{
		auto Parts = sQueryParm.Split("-");

		switch (Parts.size())
		{
			case 1: // single value
				if (iFlags & FAC_NUMERIC)
				{
					sClause = kFormat ("   and {} = {}", sDbCol, Parts[0].UInt16());
				}
				else
				{
					sClause = kFormat ("   and {} = '{}'", sDbCol, Parts[0]);
				}
				break;

			case 2: // two values
				if (iFlags & FAC_NUMERIC)
				{
					sClause = kFormat ("   and {} between {} and {}", sDbCol, Parts[0].UInt16(), Parts[1].UInt16());
				}
				else
				{
					sClause = kFormat ("   and {} between '{}' and '{}'", sDbCol, Parts[0], Parts[1]);
				}
				break;

			default:
				kDebug(1, "invalid Parms: {}", sQueryParm);
				break;
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// comma-delimed list of LIKE expressions
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else if (iFlags & FAC_LIKE)
	{
		for (KString sOne : sQueryParm.Split(sSplitBy))
		{
			sOne.Replace ('*','%',/*start=*/0,/*bAll=*/true); // allow * wildcards too
			if (!sOne.Contains ("%"))
			{
				sOne = kFormat ("{}{}{}", "%", sOne, "%");
			}
			if (sClause)
			{
				if (iFlags & FAC_SUBSELECT)
				{
					sClause += ')'; // needs an extra close paren
				}
				sClause += '\n';
				sClause += kFormat ("    or {} like '{}'", sDbCol, sOne); // OR and no parens
			}
			else
			{
				sClause += kFormat ("   and ({} like '{}'", sDbCol, sOne); // open paren
			}
		}
		if (sClause)
		{
			sClause += ')'; // close paren for AND
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// full-text search using SQL tolower and like operator
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else if (iFlags & FAC_TEXT_CONTAINS)
	{
		sClause = kFormat ("   and lower({}) like '%{}%'", sDbCol, sLowerParm);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// time intervals >= 'day', 'week', 'month' and 'year'
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else if (iFlags & FAC_TIME_PERIODS)
	{
		if (!kStrIn (sLowerParm, "minute,hour,day,week,month,quarter,year"))
		{
			m_sLastError.Format ("invalid time period: {}, should be one of: minute,hour,day,week,month,quarter,year", sQueryParm);
			return sClause; // empty
		}

		sClause = kFormat ("   and {} >= date_sub(now(), interval 1 {})", sDbCol, sLowerParm);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// single value (no comma):
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else if (!sQueryParm.Contains(sSplitBy))
	{
		if (iFlags & FAC_NUMERIC)
		{
			sClause = kFormat ("   and {} = {}", sDbCol, sQueryParm.UInt64());
		}
		else if (iFlags & FAC_LIKE)
		{
			KString sOne (sQueryParm);
			sOne.Replace ('*','%',/*start=*/0,/*bAll=*/true); // allow * wildcards too
			if (!sOne.Contains ("%"))
			{
				sOne = kFormat ("{}{}{}", "%", sOne, "%");
			}

			sClause = kFormat ("   and {} like '{}'", sDbCol, sOne);
		}
		else
		{
			sClause = kFormat ("   and {} = '{}'", sDbCol, sQueryParm);
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// comma-delimed list using IN clause
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else
	{
		KString sList;

		for (const auto& sOne : sQueryParm.Split(sSplitBy))
		{
			if (iFlags & FAC_NUMERIC)
			{
				sList += kFormat ("{}{}", sList ? "," : "", sOne.UInt64());
			}
			else
			{
				sList += kFormat ("{}'{}'", sList ? "," : "", sOne);
			}
		}

		sClause = kFormat ("   and {} in ({})", sDbCol, sList);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// finish up AND clause
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sClause.empty())
	{
		if (iFlags & FAC_SUBSELECT)
		{
			sClause += ')'; // needs an extra close paren
		}
		sClause += '\n';
	}

	return sClause;

} // FormAndClause

//-----------------------------------------------------------------------------
KString KSQL::FormGroupBy (uint8_t iNumCols)
//-----------------------------------------------------------------------------
{
	KString sGroupBy;
	for (uint8_t ii{1}; ii <= iNumCols; ++ii)
	{
		sGroupBy += kFormat ("{}{}", sGroupBy ? "," : " group by ", ii);
	}

	if (sGroupBy)
	{
		sGroupBy += "\n";
	}

	return sGroupBy;

} // FormGroupBy

//-----------------------------------------------------------------------------
bool KSQL::FormOrderBy (KStringView sCommaDelimedSort, KString& sOrderBy, const KJSON& Config)
//-----------------------------------------------------------------------------
{
	kDebug (2, "...");

	if (Config.is_null())
	{
		return true;
	}
	else if (! Config.is_object())
	{
		m_sLastError.Format ("BUG: FormOrderBy: Config is not object of key/value pairs: {}", Config.dump('\t'));
		return false;
	}

	auto ParmList = sCommaDelimedSort.Split (",");

	bool bResetFlag = KLog::getInstance().ShowStackOnJsonError(false);

	for (auto parm : ParmList)
	{
		KString sParm (parm);
		bool    bDesc = sParm.Contains(" desc");
		bool    bFound{false};

		sParm.Replace (" descending","");
		sParm.Replace (" descend",   "");
		sParm.Replace (" desc",      "");
		sParm.Replace (" ascending", "");
		sParm.Replace (" ascend",    "");
		sParm.Replace (" asc",       "");
		sParm.Replace (" ",          "");

		for (auto& it : Config.items())
		{
			try
			{
				KString sMatchParm = it.key();
				KString sDbCol     = it.value();

				if (sMatchParm.ToLower() == sParm.ToLower())
				{
					sOrderBy += kFormat ("{} {}{}\n", sOrderBy ? "     ," : " order by", sDbCol, bDesc ? " desc" : "");
					bFound = true;
					break; // inner for
				}
			}
			catch (const KJSON::exception& exc)
			{
				m_sLastError.Format ("BUG: FormOrderBy: Config is not object of key/value pairs: {}", Config.dump('\t'));
				KLog::getInstance().ShowStackOnJsonError(bResetFlag);
				return false;
			}
		}

		if (!bFound)
		{
			// runtime/user error: attempt to sort by a column that is not specified in the Config:
			m_sLastError.Format ("attempt to sort by unknown column '{}'", sParm);
			KLog::getInstance().ShowStackOnJsonError(bResetFlag);
			return false;
		}
	}

	KLog::getInstance().ShowStackOnJsonError(bResetFlag);
	return true;

} // FormOrderBy

//-----------------------------------------------------------------------------
bool KSQL::GetLock (KStringView sName, int16_t iTimeoutSeconds)
//-----------------------------------------------------------------------------
{
	if (m_iDBType == DBT::MYSQL)
	{
		m_sLastSQL = kFormat("SELECT GET_LOCK(\"{}\", {})", EscapeString(sName), iTimeoutSeconds);
		return SingleIntRawQuery (m_sLastSQL, 0, "GetLock") >= 1;
	}

	kDebug(1, "not supported for {}", TxDBType(m_iDBType));

	return false;

} // GetLock

//-----------------------------------------------------------------------------
bool KSQL::ReleaseLock (KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_iDBType == DBT::MYSQL)
	{
		m_sLastSQL = kFormat("SELECT RELEASE_LOCK(\"{}\")", EscapeString(sName));
		return SingleIntRawQuery (m_sLastSQL, 0, "ReleaseLock") >= 1;
	}

	kDebug(1, "not supported for {}", TxDBType(m_iDBType));

	return false;

} // ReleaseLock

//-----------------------------------------------------------------------------
bool KSQL::IsLocked (KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_iDBType == DBT::MYSQL)
	{
		m_sLastSQL = kFormat("SELECT IS_USED_LOCK(\"{}\")", EscapeString(sName));
		return SingleIntRawQuery (m_sLastSQL, 0, "IsLocked") >= 1;
	}

	kDebug(1, "not supported for {}", TxDBType(m_iDBType));

	return false;

} // IsLocked

static constexpr KStringView sColName = "schema_rev";

//-----------------------------------------------------------------------------
bool KSQL::EnsureSchema (KStringView sSchemaVersionTable,
                         KStringView sSchemaFolder, 
						 KStringView sFilenamePrefix,
						 uint16_t iCurrentSchema,
						 uint16_t iInitialSchema/*=100*/,
						 bool bForce /* = false */,
						 const SchemaCallback& Callback /* = nullptr */)
//-----------------------------------------------------------------------------
{
	kDebug (1, "tbl={}, force={} ...", sSchemaVersionTable, bForce ? "true" : "false");

	if (iCurrentSchema < iInitialSchema)
	{
		kDebug (1, "requested schema {} is lower than the initial schema {}", iCurrentSchema, iInitialSchema);
		return true;
	}

	auto     iSigned    = (bForce) ? 0 : SingleIntQuery ("select %s from %s", sColName, sSchemaVersionTable);
	uint16_t iSchemaRev = (iSigned < 0) ? 0 : static_cast<uint16_t>(iSigned);
	KString  sError;

	kDebug (2, "current rev in db determined to be: {}", iSchemaRev);
	kDebug (2, "current rev that code expected is:  {}", iCurrentSchema);

	if (iSchemaRev >= iCurrentSchema)
	{
		return true; // all set
	}

	if (!GetLock (sSchemaVersionTable, 120))
	{
		kWarning("Could not acquire schema update lock within 120 seconds. Another process may be updating the schema. Abort.");
		m_sLastError = kFormat("schema updater for table {} is locked", sSchemaVersionTable);
		if (m_TimingCallback)
		{
			m_TimingCallback (*this, /*iMilliseconds=*/0, kFormat ("{}", m_sLastError));
		}
		return false;
	}

	// query rev again after acquiring the lock
	iSigned    = (bForce) ? 0 : SingleIntQuery ("select %s from %s", sColName, sSchemaVersionTable);
	iSchemaRev = (iSigned < 0) ? 0 : static_cast<uint16_t>(iSigned);

	if (iSchemaRev < iCurrentSchema)
	{
		if (!BeginTransaction())
		{
			return false;
		}

		uint16_t iFromRev = iSchemaRev;

		for (auto ii = std::max(++iSchemaRev, iInitialSchema); ii <= iCurrentSchema; ++ii)
		{
			kDebug (1, KLog::DASH);
			kDebug (1, "attempting to apply schema version {} to db {} ...", ii, ConnectSummary());
			kDebug (1, KLog::DASH);

			KString sFile = kFormat ("{}/{}{}.ksql", sSchemaFolder, sFilenamePrefix, ii);

			if (!kFileExists (sFile))
			{
				sError = kFormat ("missing schema file: {}", sFile);
				break; // for
			}

			if (!ExecSQLFile (sFile))
			{
				sError= GetLastError();
				break; // for
			}

			if (Callback)
			{
				if (!Callback(iFromRev++, ii))
				{
					sError = "callback returned with abort request";
					break;
				}
			}

			if (ii == iInitialSchema)
			{
				ExecSQL ("drop table if exists %s", sSchemaVersionTable);

				if (!ExecSQL ("create table %s ( %s smallint not null primary key )", sSchemaVersionTable, sColName))
				{
					sError= GetLastError();
					break; // for
				}
			}

			if (!ExecSQL ("update %s set %s=%u", sSchemaVersionTable, sColName, ii))
			{
				sError= GetLastError();
				break; // for
			}

			auto iUpdated = GetNumRowsAffected();

			if (!iUpdated && !ExecSQL ("insert into %s values (%u)", sSchemaVersionTable, ii))
			{
				sError= GetLastError();
				break; // for
			}
			else if (iUpdated > 1)
			{
				ExecSQL ("truncate table %s", sSchemaVersionTable);

				if (!ExecSQL ("insert into %s values (%u)", sSchemaVersionTable, ii))
				{
					sError= GetLastError();
					break; // for
				}
			}

		} // for each schema upgrade

		if (sError)
		{
			kDebug (1, sError);
			ExecSQL ("rollback");
			m_sLastError = sError;
			return false;
		}
		else
		{
			if (!CommitTransaction())
			{
				return false;
			}
		}

	} // if upgrade was needed

	ReleaseLock (sSchemaVersionTable);

	kDebug (2, "schema should be all set at version {} now", iCurrentSchema);

	return true;

} // EnsureSchema

//-----------------------------------------------------------------------------
uint16_t KSQL::GetSchema (KStringView sTablename)
//-----------------------------------------------------------------------------
{
	m_sLastSQL = kFormat("SELECT {} FROM {}", sColName, EscapeString(sTablename));

	auto iSigned = SingleIntQuery ("select %s from %s", sColName, EscapeString(sTablename));

	if (iSigned <= 0)
	{
		return 0;
	}
	else
	{
		return static_cast<uint16_t>(iSigned);
	}

} // GetSchema

//-----------------------------------------------------------------------------
bool KSQL::EnsureConnected (KStringView sProgramName, KString sDBCFile, const IniParms& INI)
//-----------------------------------------------------------------------------
{
	if (IsConnectionOpen())
	{
		kDebug (2, "already connected to: {}", ConnectSummary());
		return true; // already connected
	}

	KString sUpperProgramName = sProgramName.ToUpper();
	KString sDBType (kFormat("{}_DBTYPE", sUpperProgramName));
	KString sDBUser (kFormat("{}_DBUSER", sUpperProgramName));
	KString sDBPass (kFormat("{}_DBPASS", sUpperProgramName));
	KString sDBHost (kFormat("{}_DBHOST", sUpperProgramName));
	KString sDBName (kFormat("{}_DBNAME", sUpperProgramName));
	KString sDBPort (kFormat("{}_DBPORT", sUpperProgramName));
	KString sLiveDB (kFormat("{}_DBLIVE", sUpperProgramName));
	KString sDBC    (kFormat("{}_DBC"   , sUpperProgramName));

	if ((sDBCFile.empty() || !kFileExists(sDBCFile)) && !sDBC.empty() && kFileExists(kGetEnv(sDBC)))
	{
		sDBCFile = kGetEnv(sDBC);
	}

	bool bIsDefaultDBCFile = sDBCFile.empty();

	if (bIsDefaultDBCFile)
	{
		if (!INI.Get(sDBC).empty())
		{
			sDBCFile = INI.Get(sDBC);
			if (!kFileExists(sDBCFile))
			{
				sDBCFile.clear();
			}
		}

		if (sDBCFile.empty() && !sProgramName.empty())
		{
			// still no dbc file? try a default one
			sDBCFile.Format("/etc/{}.dbc", sProgramName.ToLower());
		}
	}

	if (kWouldLog(3))
	{
		kDebug (3, "looks like we need to connect...");

		kDebug (3,
			" 1. environment vars:\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}",
				sDBType, kGetEnv(sDBType),
				sDBUser, kGetEnv(sDBUser),
				sDBPass, kGetEnv(sDBPass),
				sDBHost, kGetEnv(sDBHost),
				sDBName, kGetEnv(sDBName),
				sDBPort, kGetEnv(sDBPort),
				sLiveDB, kGetEnv(sLiveDB));

		kDebug (3,
			" 2. DBC FILE:\n"
			"    {:<18} : {} ({})",
					"dbcfile",
					sDBCFile,
					kFileExists(sDBCFile) ? "exists" : "does not exist");

		kDebug (3,
			" 3. INI PARMS:\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}\n"
			"    {:<18} : {}",
				sDBType, INI.Get(sDBType),
				sDBUser, INI.Get(sDBUser),
				sDBPass, INI.Get(sDBPass),
				sDBHost, INI.Get(sDBHost),
				sDBName, INI.Get(sDBName),
				sDBPort, INI.Get(sDBPort),
				sLiveDB, INI.Get(sLiveDB));
	}
	
	const auto& sDBCContent = s_DBCCache.Get(sDBCFile);

	if (!sDBCContent.empty())
	{
		kDebug (3, "loading: {}", sDBCFile);
		if (!SetConnect (sDBCFile, sDBCContent))
		{
			return false;
		}
	}

	KString sInitialConfig = ConnectSummary();

	kDebug (3, "checking for environment overrides (piecemeal acceptable)");

	KString sSetDBType;
	if (GetDBType() != DBT::NONE)
	{
		sSetDBType = TxDBType(GetDBType());
	}

	KString sSetDBPort;
	if (GetDBPort() != 0)
	{
		sSetDBPort = KString::to_string(GetDBPort());
	}

	if (bIsDefaultDBCFile)
	{
		// we constructed the DBC file name ourselves - treat it only with higher precedence
		// than the ini parms
		SetDBType (kFirstNonEmpty<KStringView>(kGetEnv (sDBType), sSetDBType  , INI.Get (sDBType)));
		SetDBUser (kFirstNonEmpty<KStringView>(kGetEnv (sDBUser), GetDBUser() , INI.Get (sDBUser)));
		SetDBPass (kFirstNonEmpty<KStringView>(kGetEnv (sDBPass), GetDBPass() , INI.Get (sDBPass)));
		SetDBHost (kFirstNonEmpty<KStringView>(kGetEnv (sDBHost), GetDBHost() , INI.Get (sDBHost)));
		SetDBName (kFirstNonEmpty<KStringView>(kGetEnv (sDBName), GetDBName() , INI.Get (sDBName)));
		SetDBPort (kFirstNonEmpty<KStringView>(kGetEnv (sDBPort), sSetDBPort  , INI.Get (sDBPort)).UInt32());
	}
	else
	{
		// we have an explicit DBC file given to us - honour it with the highest precedence
		SetDBType (kFirstNonEmpty<KStringView>(sSetDBType  , kGetEnv (sDBType), INI.Get (sDBType)));
		SetDBUser (kFirstNonEmpty<KStringView>(GetDBUser() , kGetEnv (sDBUser), INI.Get (sDBUser)));
		SetDBPass (kFirstNonEmpty<KStringView>(GetDBPass() , kGetEnv (sDBPass), INI.Get (sDBPass)));
		SetDBHost (kFirstNonEmpty<KStringView>(GetDBHost() , kGetEnv (sDBHost), INI.Get (sDBHost)));
		SetDBName (kFirstNonEmpty<KStringView>(GetDBName() , kGetEnv (sDBName), INI.Get (sDBName)));
		SetDBPort (kFirstNonEmpty<KStringView>(sSetDBPort  , kGetEnv (sDBPort), INI.Get (sDBPort)).UInt32());
	}

	if (kWouldLog(1))
	{
		KString sChanged = ConnectSummary();
		if (sInitialConfig != sChanged)
		{
			kDebug (2, "db configuration changed through ini file or env vars\n  from: {}\n    to: {}",
					sInitialConfig, sChanged);
		}
	}

	if (!GetDBUser() && !GetDBPass() && !GetDBName())
	{
		m_sLastError.Format ("no db connection defined");
		kDebug (1, m_sLastError);
		return false;
	}

	kDebug (3, "attempting to connect ...");

	if (!OpenConnection())
	{
		return false;
	}

	kDebug (1, "connected to: {}", ConnectSummary());

	// we have an open database connection
	return true;

} // KSQL::EnsureConnected - 1

//-----------------------------------------------------------------------------b
bool KSQL::EnsureConnected ()
//-----------------------------------------------------------------------------
{
	if (IsConnectionOpen())
	{
		kDebug (2, "already connected to: {}", ConnectSummary());
		return true; // already connected
	}

	kDebug (3, "attempting to connect ...");

	if (!OpenConnection())
	{
		return false;
	}

	kDebug (1, "connected to: {}", ConnectSummary());

	// we have an open database connection
	return true;

} // KSQL::EnsureConnected - 2

//-----------------------------------------------------------------------------
KString KSQL::ConvertTimestamp (KStringView sTimestamp)
//-----------------------------------------------------------------------------
{
	KString sNew (sTimestamp);

	// 0123 45 67 8 9A BC DE F
	// 2020 05 12 T 23 45 39 Z --> 2020-05-12 23:45:39
	if (sTimestamp.MatchRegex ("^[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]T[0-9][0-9][0-9][0-9][0-9][0-9]Z$"))
	{
		sNew = kFormat ("{}-{}-{} {}:{}:{}",
			sTimestamp.Left(4), sTimestamp.Mid(4,2), sTimestamp.Mid(6,2),
			sTimestamp.Mid(9,2), sTimestamp.Mid(11,2), sTimestamp.Mid(13,2));

		kDebug (2, "{} --> {}", sTimestamp, sNew);
	}
	else
	{
		kDebug (2, "unchanged: {}", sTimestamp);
	}

	return sNew;

} // ConvertTimestap


//-----------------------------------------------------------------------------
DbSemaphore::DbSemaphore (KSQL& db, KString sAction, bool bThrow, bool bWait, int16_t iTimeout)
//-----------------------------------------------------------------------------
	: m_db      { db }
	, m_sAction { sAction }
	, m_bThrow  { bThrow }
{
	if (!bWait)
	{
		CreateSemaphore(iTimeout);
	}

} // ctor

//-----------------------------------------------------------------------------
bool DbSemaphore::CreateSemaphore (int16_t iTimeout)
//-----------------------------------------------------------------------------
{
	if (!m_bIsSet)
	{
		m_sLastError.clear ();

		auto iSave = m_db.GetFlags ();
		m_db.SetFlags (KSQL::F_IgnoreSQLErrors);
		auto bOK = m_db.GetLock(m_sAction, iTimeout);
		m_db.SetFlags (iSave);

		if (!bOK)
		{
			m_sLastError.Format ("could not create named lock '{}', already exists", m_sAction);
			kDebug(1, m_sLastError);

			if (m_bThrow)
			{
				throw KException(m_sLastError);
			}
		}
		else
		{
			m_bIsSet = true;
		}
	}

	return m_bIsSet;

} // CreateSemaphore

//-----------------------------------------------------------------------------
bool DbSemaphore::ClearSemaphore ()
//-----------------------------------------------------------------------------
{
	if (m_bIsSet)
	{
		m_sLastError.clear ();

		auto iSave = m_db.GetFlags ();
		m_db.SetFlags (KSQL::F_IgnoreSQLErrors);
		auto bOK = m_db.ReleaseLock(m_sAction);
		m_db.SetFlags (iSave);

		if (!bOK)
		{
			m_sLastError.Format ("could not release named lock '{}'", m_sAction);
			kDebug(1, m_sLastError);

			if (m_bThrow)
			{
				throw KException(m_sLastError);
			}
		}
		else
		{
			m_bIsSet = false;
		}
	}

	return !m_bIsSet;

} // ClearSemaphore


KSQL::DBCCache KSQL::s_DBCCache;

} // namespace dekaf2
