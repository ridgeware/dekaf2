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
#include "kscopeguard.h"
#include "ktime.h"
#include "kduration.h"
#include <cstdint>
#include <utility>

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
  #if DEKAF2_HAS_INCLUDE(<mysql/errmsg.h>)
    #include <mysql/errmsg.h>   // mysql error codes (start with CR_)
  #elif DEKAF2_HAS_INCLUDE(<mariadb/errmsg.h>)
    #include <mariadb/errmsg.h>
  #else
    #error "cannot find header file with mysql error codes"
  #endif
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

DEKAF2_NAMESPACE_BEGIN

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
					iKSQLDataType = KCOL::NUMERIC;
					break;

				case MYSQL_TYPE_DECIMAL:
				case 246: // MYSQL_TYPE_NEWDECIMAL:
				case 247: // MYSQL_TYPE_ENUM:
				case 255: // MYSQL_TYPE_GEOMETRY:
					iKSQLDataType = KCOL::NUMERIC;
					if (iMaxDataLen >= 19)
					{
						// this is a large integer as well (see below)..
						iKSQLDataType |= KCOL::INT64NUMERIC;
					}
					break;

				case MYSQL_TYPE_LONGLONG:
				case MYSQL_TYPE_INT24:
					// make sure we flag large integers - this is important when we want to
					// convert them into JSON integers, which have a limit of 53 bits
					// - values larger than that need to be represented as strings..
					iKSQLDataType = KCOL::NUMERIC | KCOL::INT64NUMERIC;
					break;

					// always the problem child:
				case MYSQL_TYPE_NULL:
					iKSQLDataType = KCOL::NOFLAG;
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
					iKSQLDataType = KCOL::NOFLAG;
					break;
			}
			break;

		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
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
					iKSQLDataType = KCOL::NUMERIC;
					break;

				case CS_DECIMAL_TYPE:
				case CS_NUMERIC_TYPE:
				case CS_LONG_TYPE:
					iKSQLDataType = KCOL::NUMERIC;
					if (iMaxDataLen >= 19)
					{
						// this is a large integer as well..
						iKSQLDataType |= KCOL::INT64NUMERIC;
					}
					break;

				case CS_BIGINT_TYPE:
				case CS_UBIGINT_TYPE:
					iKSQLDataType = KCOL::NUMERIC | KCOL::INT64NUMERIC;
					break;

#endif
				default:
					iKSQLDataType = KCOL::NOFLAG;
					break;
			}
			break;

		case DBT::ORACLE6:
		case DBT::ORACLE7:
		case DBT::ORACLE8:
		case DBT::ORACLE:
		case DBT::INFORMIX:
		default:
			iKSQLDataType = KCOL::NOFLAG;
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
	auto iTotal = Total();

	if (iTotal == 0)
	{
		return "TOTAL=0";
	}

	// Suppress showing the TOTAL when only one type of SQL statement was executed
	if ((iTotal == iSelect) || (iTotal == iInsert) || (iTotal == iUpdate) || (iTotal == iDelete) ||
		(iTotal == iCreate) || (iTotal == iAlter)  || (iTotal == iDrop)   || (iTotal == iTransact) ||
		(iTotal == iExec)   || (iTotal == iAction) || (iTotal == iInfo)   || (iTotal == iTblMaint) ||
		(iTotal == iOther))
	{
		iTotal = 0;
	}

	return kFormat ("{}{}{}{}{}{}{}{}{}{}{}{}{}{}",
				(iTotal    ? kFormat("TOTAL={} ",    iTotal)    : ""),
				(iSelect   ? kFormat("SELECT={} ",   iSelect)   : ""),
				(iInsert   ? kFormat("INSERT={} ",   iInsert)   : ""),
				(iUpdate   ? kFormat("UPDATE={} ",   iUpdate)   : ""),
				(iDelete   ? kFormat("DELETE={} ",   iDelete)   : ""),
				(iCreate   ? kFormat("CREATE={} ",   iCreate)   : ""),
				(iAlter    ? kFormat("ALTER={} ",    iAlter)    : ""),
				(iDrop     ? kFormat("DROP={} ",     iDrop)     : ""),
				(iTransact ? kFormat("TRANSACT={} ", iTransact) : ""),
				(iExec     ? kFormat("EXEC={} ",     iExec)     : ""),
				(iAction   ? kFormat("Action={} ",   iAction)   : ""),
				(iTblMaint ? kFormat("TblMaint={} ", iTblMaint) : ""),
				(iInfo     ? kFormat("Info={} ",     iInfo)     : ""),
				(iOther    ? kFormat("Other={} ",    iOther)    : ""));
}

//-----------------------------------------------------------------------------
void KSQL::KSQLStatementStats::Increment(KStringView sLastSQL, QueryType QueryType)
//-----------------------------------------------------------------------------
{
	if (QueryType == QueryType::None)
	{
		QueryType = KSQL::GetQueryType(sLastSQL);
	}

	switch (QueryType)
	{
		case QueryType::Select:
			++iSelect;
			break;

		case QueryType::Insert:
			++iInsert;
			break;

		case QueryType::Update:
			++iUpdate;
			break;

		case QueryType::Delete:
			++iDelete;
			break;

		case QueryType::Create:
			++iCreate;
			break;

		case QueryType::Alter:
			++iAlter;
			break;

		case QueryType::Drop:
			++iDrop;
			break;

		case QueryType::Transaction:
			++iTransact;
			break;

		case QueryType::Execute:
			++iExec;
			break;

		case QueryType::Action:
			++iAction;
			break;

		case QueryType::Maintenance:
			++iTblMaint;
			break;

		case QueryType::Info:
			++iInfo;
			break;

		case QueryType::Other:
			// TODO: SMALL BUG HERE...
			// If you are running a statement that starts with a K&R style comment like this:
			//   /* hello world */
			//   drop table FUBAR;
			//
			// The statistics will be messed up.  We see the comment as SQL, fail to skip over it, and
			// the DROP gets classified as OTHER.
			kDebug (2, "Unrecognized SQL={}", sLastSQL);
			++iOther;
			break;

		case QueryType::Any:
		case QueryType::None:
			kDebug (1, "Any/None SQL={}", sLastSQL);
			break;
	}

} // Increment

//-----------------------------------------------------------------------------
KSQL::KSQL ()
//-----------------------------------------------------------------------------
: m_iDBType             (DBT::MYSQL)
, m_QueryTimeout        (s_QueryTimeout)
, m_QueryTypeForTimeout (s_QueryTypeForTimeout)
{
	kDebug (3, "...");

	// enable the query timeout if the preset static values were good
	m_bEnableQueryTimeout = m_QueryTimeout > std::chrono::milliseconds(0) && m_QueryTypeForTimeout != QueryType::None;

} // KSQL - default constructor - do not connect

//-----------------------------------------------------------------------------
KSQL::KSQL (DBT iDBType, KStringView sUsername, KStringView sPassword, KStringView sDatabase, KStringView sHostname, uint16_t iDBPortNum)
//-----------------------------------------------------------------------------
: KSQL () // delegate to default constructor first
{
	kDebug (3, "DBType: {}, username: {}", TxDBType(iDBType), sUsername);

	if (!sUsername.empty())
	{
		SetConnect (iDBType, sUsername, sPassword, sDatabase, sHostname, iDBPortNum);
		// already calls SetAPISet() and InvalidateConnectSummary()
	}

} // KSQL - construct with connection details, but do not connect yet

//-----------------------------------------------------------------------------
KSQL::KSQL (KStringView sDBCFile)
//-----------------------------------------------------------------------------
: KSQL () // delegate to default constructor first
{
	kDebug(2, "DBC file: {}", sDBCFile);

	EnsureConnected ("", sDBCFile);

} // KSQL - construct and connect from a DBC file

//-----------------------------------------------------------------------------
KSQL::KSQL (KStringView sIdentifierList,
			KStringView sDBCFile,
			const IniParms& INI)
//-----------------------------------------------------------------------------
: KSQL () // delegate to default constructor first
{
	kDebug(2, "Identifiers: {}, DBC file: {}", sIdentifierList, sDBCFile);

	EnsureConnected (sIdentifierList, sDBCFile, INI);

} // KSQL - construct and connect from a DBC file or env or INI parms

//-----------------------------------------------------------------------------
KSQL::KSQL (const KSQL& other)
//-----------------------------------------------------------------------------
: m_iFlags(other.m_iFlags)
, m_bEnableQueryTimeout(other.m_bEnableQueryTimeout)
, m_QueryTimeout(other.m_QueryTimeout)
, m_QueryTypeForTimeout(other.m_QueryTypeForTimeout)
, m_iWarnIfOverMilliseconds(other.m_iWarnIfOverMilliseconds)
, m_TimingCallback(other.m_TimingCallback)
{
	kDebug (3, "...");

	ClearTempResultsFile();

	if (!other.GetDBUser().empty())
	{
		m_sDBCFile = other.m_sDBCFile;
		SetConnect (other.GetDBType(), other.GetDBUser(), other.GetDBPass(), other.GetDBName(), other.GetDBHost(), other.GetDBPort());
	}

} // KSQL - copy constructor, but do not connect yet

//-----------------------------------------------------------------------------
KSQL::~KSQL ()
//-----------------------------------------------------------------------------
{
	EndQuery (/*bDestructor=*/true);
	PurgeTempTables ();
	CloseConnection (/*bDestructor=*/true);  // <-- this calls FreeAll()

} // destructor

//-----------------------------------------------------------------------------
const KString& KSQL::GetTempResultsFile()
//-----------------------------------------------------------------------------
{
	if (m_sNeverReadMeDirectlyTmpResultsFile.empty())
	{
		m_sNeverReadMeDirectlyTmpResultsFile.Format ("{}/ksql-{}-{}-{}.res", GetTempDir(), kGetPid(), kGetTid(), kRandom());
	}
	return m_sNeverReadMeDirectlyTmpResultsFile;

} // GetTempResultsFile

//-----------------------------------------------------------------------------
void KSQL::RemoveTempResultsFile()
//-----------------------------------------------------------------------------
{
	if (!m_sNeverReadMeDirectlyTmpResultsFile.empty())
	{
		kRemoveFile(m_sNeverReadMeDirectlyTmpResultsFile);
	}

} // RemoveTempResultsFile

//-----------------------------------------------------------------------------
void KSQL::ClearError()
//-----------------------------------------------------------------------------
{
	m_sLastErrorSetOnlyWithSetError.clear();
	m_iErrorSetOnlyWithSetError = 0;

} // ClearError

//-----------------------------------------------------------------------------
bool KSQL::SetError(KString sError, uint32_t iErrorNum/*=-1*/, bool bNoThrow/*=false*/)
//-----------------------------------------------------------------------------
{
	m_sLastErrorSetOnlyWithSetError = std::move(sError);
	m_iErrorSetOnlyWithSetError     = iErrorNum;

	bool bIgnore = IsFlag(F_IgnoreSQLErrors);

	if (!m_sErrorPrefix.empty() && !m_sLastErrorSetOnlyWithSetError.starts_with(m_sErrorPrefix))
	{
		m_sLastErrorSetOnlyWithSetError.insert(0, m_sErrorPrefix);
	}

	kDebug(GetDebugLevel(), m_sLastErrorSetOnlyWithSetError);

	if (!bIgnore && !m_sLastSQL.empty())
	{
		kDebug (GetDebugLevel(), kLimitSize(m_sLastSQL.str(), 4096));
	}

	if (m_TimingCallback)
	{
		m_TimingCallback (*this, chrono::milliseconds(0), GetLastError());
	}

	if (!bNoThrow && m_bMayThrow && (!bIgnore))
	{
		iErrorNum = GetLastErrorNum();

		if (iErrorNum == 0)
		{
			iErrorNum = -1;
		}

		throw Exception(GetLastError(), iErrorNum);
	}

	return false;

} // SetError

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
		return SetError(kFormat("{} cannot change database connection when it's already open", FUNC)); \
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
	if (sDBType == "sqlserver15")
	{
		return (SetDBType (DBT::SQLSERVER15));
	}
	if (sDBType == "sybase")
	{
		return (SetDBType (DBT::SYBASE));
	}
#endif

	return SetError(kFormat ("unsupported dbtype: {}", sDBType));

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
bool KSQL::SaveConnect (KStringViewZ sDBCFile)
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
				return SetError(kFormat ("SaveConnect(): could not write to '{}'.", sDBCFile));
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
				return SetError(kFormat ("SaveConnect(): could not write to '{}'.", sDBCFile));
			}
			return true;
		}
	}
	return SetError(kFormat ("SaveConnect(): could not encode all strings into '{}' - too long.", sDBCFile));

} // SaveConnect

//-----------------------------------------------------------------------------
bool KSQL::DecodeDBCData (KStringView sBuffer, KStringViewZ sDBCFile)
//-----------------------------------------------------------------------------
{
	std::unique_ptr<DBCFileBase> dbc;

	if (sBuffer.starts_with("KSQLDBC1"))
	{
#ifdef WIN32
		return SetError("old format (DBC1) doesn't work on win32");
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
		return SetError(kFormat ("unrecognized DBC version in DBC file '{}'.", sDBCFile));
	}
	else
	{
		return SetError(kFormat ("invalid header on DBC file '{}'.", sDBCFile));
	}

	if (!dbc->SetBuffer(sBuffer))
	{
		return SetError(kFormat ("corrupted DBC file '{}'.", sDBCFile));
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
bool KSQL::SetConnect (KStringViewZ sDBCFile, KStringView sDBCFileContent)
//-----------------------------------------------------------------------------
{
	kDebug (3, sDBCFile);

	if (IsConnectionOpen())
	{
		return SetError("can't call SetConnect on an OPEN DATABASE");
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
		return SetError(kFormat ("empty DBC file '{}'", sDBCFile));
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
bool KSQL::LoadConnect (KStringViewZ sDBCFile)
//-----------------------------------------------------------------------------
{
	kDebug (3, sDBCFile);

	if (IsConnectionOpen())
	{
		return SetError ("can't call LoadConnect on an OPEN DATABASE");
	}

	kDebug (GetDebugLevel(), "opening '{}'...", sDBCFile);

	auto sBuffer = s_DBCCache.Get(sDBCFile);

	if (sBuffer->empty())
	{
		return SetError(kFormat("dbc file empty or unexisting: {}", sDBCFile));
	}

	return SetConnect (sDBCFile, sBuffer.get());

} // LoadConnect

//-----------------------------------------------------------------------------
bool KSQL::OpenConnection (KStringView sListOfHosts, KStringView sDelimiter/* = ","*/, uint16_t iConnectTimeoutSecs/*=0*/)
//-----------------------------------------------------------------------------
{
	m_iConnectTimeoutSecs = iConnectTimeoutSecs; // <-- save timeout in case we have to reconnect

	for (auto sDBHost : sListOfHosts.Split(sDelimiter))
	{
		SetDBHost (sDBHost);

		if (OpenConnection(iConnectTimeoutSecs))
		{
			kDebug (3, "host {} is up", sDBHost);
			return true;
		}
		else
		{
			kDebug (3, "host {} is down", sDBHost);
		}
	}

	return false;

} // KSQL::OpenConnection

//-----------------------------------------------------------------------------
bool KSQL::OpenConnection (uint16_t iConnectTimeoutSecs/*=0*/)
//-----------------------------------------------------------------------------
{
	m_iConnectTimeoutSecs = iConnectTimeoutSecs; // <-- save timeout in case we have to reconnect

    #ifdef DEKAF2_HAS_ORACLE
	static bool s_fOCI8Initialized = false;
	#endif

	kDebug (3, "...");

	if (IsConnectionOpen())
	{
		return (true);
	}

	ClearError();

	if (m_iAPISet == API::NONE)
	{
		SetAPISet (API::NONE); // <-- pick the default APIs for this DBType
	}

	InvalidateConnectSummary ();
	ResetConnectionID ();

	if (kWouldLog(GetDebugLevel() + 1))
	{
		kDebug (GetDebugLevel() + 1, "connect info:");
		kDebug (GetDebugLevel() + 1, "  DBType   = {}", TxDBType(m_iDBType));
		kDebug (GetDebugLevel() + 1, "  APISet   = {}", TxAPISet(m_iAPISet));
		kDebug (GetDebugLevel() + 1, "  DBUser   = {}", m_sUsername);
		kDebug (GetDebugLevel() + 1, "  DBHost   = {}", m_sHostname);
		kDebug (GetDebugLevel() + 1, "  DBPort   = {}", m_iDBPortNum);
		kDebug (GetDebugLevel() + 1, "  DBName   = {}", m_sDatabase);
		kDebug (GetDebugLevel() + 1, "  Flags    = {} ( {}{}{}{})",
			std::to_underlying(m_iFlags),
			IsFlag(F_IgnoreSQLErrors)  ? "IgnoreSQLErrors "  : "",
			IsFlag(F_BufferResults)    ? "BufferResults "    : "",
			IsFlag(F_NoAutoCommit)     ? "NoAutoCommit "     : "",
			IsFlag(F_NoTranslations)   ? "NoTranslations "   : "");
	}

	kDebug (2, "connecting to: {}...", ConnectSummary());

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

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case API::NONE:
	// - - - - - - - - - - - - - - - - -
		return SetError("CANNOT CONNECT (no connect info!)");

    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - -
	case API::MYSQL:
	// - - - - - - - - - - - - - - - - -
		if (m_sHostname.empty())
		{
			m_sHostname = "localhost";
		}

		// init the mysql lib, exactly once
		static std::once_flag s_once;
		std::call_once(s_once, []
		{
			kDebug (3, "mysql_library_init()...");
			mysql_library_init(0, nullptr, nullptr);
		});

		kDebug (3, "mysql_init()...");
		m_dMYSQL = mysql_init (nullptr);

		if (!m_dMYSQL)
		{
			return SetError("could not init mysql connector");
		}

		if (iConnectTimeoutSecs)
		{
			kDebug (3, "mysql_options(CONNECT_TIMEOUT={})...", iConnectTimeoutSecs);
			unsigned int iTimeoutUINT{iConnectTimeoutSecs};
			mysql_options (m_dMYSQL, MYSQL_OPT_CONNECT_TIMEOUT, &iTimeoutUINT);
		}

		kDebug (3, "mysql_real_connect()...");
		if (!mysql_real_connect (m_dMYSQL, m_sHostname.c_str(), m_sUsername.c_str(), m_sPassword.c_str(), m_sDatabase.c_str(), /*port*/ iPortNum, /*sock*/nullptr,
			/*flag*/CLIENT_FOUND_ROWS)) // <-- this flag corrects the behavior of GetNumRowsAffected()
		{
			auto iErrorNum = mysql_errno (m_dMYSQL);
			auto sError    = kFormat ("KSQL: MSQL-{}: {}", iErrorNum, mysql_error(m_dMYSQL));

			mysql_close(m_dMYSQL);
			m_dMYSQL = nullptr;

			return SetError (sError, iErrorNum);
		}

		// set the connection ID right at connection time - with mysql there is
		// no need to execute an extra query (select CONNECTION_ID())
		m_iConnectionID = m_dMYSQL->thread_id;

		mysql_set_character_set (m_dMYSQL, "utf8mb4"); // by default
		break;
	#endif

    #ifdef DEKAF2_HAS_ODBC
	// - - - - - - - - - - - - - - - - -
	case API::ODBC:
	// - - - - - - - - - - - - - - - - -
		// ODBC initialization:
		if (SQLAllocEnv (&m_Environment) != SQL_SUCCESS)
		{
			return SetError("SQLAllocEnv() failed");
		}

		if (SQLAllocConnect (m_Environment, &m_hdbc) != SQL_SUCCESS)
		{
			SQLFreeEnv (m_Environment);
			return SetError("SQLAllocConnect() failed");
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
		kDebug (3, "ORACLE_HOME='{}'", sOraHome);
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
		auto iErrorNum = OCISessionBegin ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle, (OCISession*)m_dOCI8Session,
			OCI_CRED_RDBMS, m_iOraSessionMode);
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		KString sError;
		if (!WasOCICallOK("OpenConnection:OCISessionBegin", iErrorNum, sError))
		{
			return SetError(sError, iErrorNum);
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
		auto iErrorNum = OCILogon ((OCIEnv*)m_dOCI8EnvHandle, (OCIError*)m_dOCI8ErrorHandle, (OCISvcCtx**)(&m_dOCI8ServerContext),
			(text*) m_sUsername.c_str(), m_sUsername.length()),
			(text*) m_sPassword.c_str(), m_sPassword.length()),
			(text*) m_sHostname.c_str(), m_sHostname.length()));  // FYI: m_sHostname holds the ORACLE_SID
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		KString sError;
		if (!WasOCICallOK("OpenConnection:OCILogon", iErrorNum, sError))
		{
			return SetError(sError, iErrorNum);
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
		auto iErrorNum = olog ((Lda_Def*)m_dOCI6LoginDataArea, (ub1*)m_sOCI6HostDataArea,
				(text *)m_sUsername, strlen(m_sUsername),
		        (text *)m_sPassword, strlen(m_sPassword),
				(text *)m_sHostname, strlen(m_sHostname), OCI_LM_DEF);  // FYI: m_sHostname holds the ORACLE_SID
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		KString sError;
		if (!WasOCICallOK("OpenConnection:olog"))
		{
			return SetError(sError, iErrorNum);
		}

		kDebug (GetDebugLevel()+1, "OCI6: connect through olog() succeeded.\n");
	
		// Only one cursor is associated with each KSQL instance, so open it now:
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		iErrorNum = oopen ((Cda_Def*)m_dOCI6ConnectionDataArea, (Lda_Def*)m_dOCI6LoginDataArea, nullptr, -1, -1, nullptr, -1);
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		if (!WasOCICallOK("OpenConnection:oopen", iErrorNum, sError))
		{
			return SetError(sError, iErrorNum);
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
			// ctlib_login logs and throws, or returns false - no need to further log here
			return false;
		}
		break;
	#endif

	// - - - - - - - - - - - - - - - - -
	case API::INFORMIX:
	default:
	// - - - - - - - - - - - - - - - - -
		return SetError(kFormat ("API Set not coded yet, or connector not linked in ({})", TxAPISet(GetAPISet())));
	}

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

	m_bConnectionIsOpen = true;

	kDebug (3, "connected to: {}", ConnectSummary());

	return true;

} // OpenConnection

//-----------------------------------------------------------------------------
void KSQL::CloseConnection (bool bDestructor/*=false*/)
//-----------------------------------------------------------------------------
{
	if (!bDestructor)
	{
		kDebug (3, "...");
	}

	if (IsConnectionOpen())
	{
		if (!bDestructor)
		{
			kDebug (GetDebugLevel(), "disconnecting from {}...", ConnectSummary());
		}

		ClearError ();
		ResetConnectionID ();

		switch (m_iAPISet)
		{
        #ifdef DEKAF2_HAS_MYSQL
		// - - - - - - - - - - - - - - - - -
		case API::MYSQL:
		// - - - - - - - - - - - - - - - - -
			if (m_dMYSQL)
			{
				if (!bDestructor)
				{
					kDebug (3, "mysql_close()...");
				}
				mysql_close (m_dMYSQL);
				m_dMYSQL = nullptr;
			}
			else
			{
				kDebug(1, "mysql connector was already null");
			}
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
bool kSameBufferStart(const KStringRef& sStr, KStringView svView)
//-----------------------------------------------------------------------------
{
	return (sStr.data() == svView.data());
}

//-----------------------------------------------------------------------------
inline
void CopyIfNotSame(KStringRef& sTarget, KStringView svView)
//-----------------------------------------------------------------------------
{
	if (!kSameBufferStart(sTarget, svView))
	{
		sTarget = svView;
	}
}

//-----------------------------------------------------------------------------
bool KSQL::IsSelect (KStringView sSQL)
//-----------------------------------------------------------------------------
{
	return (GetQueryType(sSQL) == QueryType::Select);
}

//-----------------------------------------------------------------------------
bool KSQL::IsReadOnlyViolation (QueryType QueryType)
//-----------------------------------------------------------------------------
{
	if (IsFlag(F_ReadOnlyMode) &&
			(QueryType & ~(QueryType::Select | QueryType::Action | QueryType::Info)) != QueryType::None)
	{
		SetError(kFormat ("KSQL: attempt to perform a non-query on a READ ONLY db connection:\n{}", m_sLastSQL));
		return true;
	}
	return false;

} // IsReadOnlyViolation

//-----------------------------------------------------------------------------
KSQL::QueryType KSQL::GetQueryType(KStringView sQuery, bool bAllowAny)
//-----------------------------------------------------------------------------
{
	sQuery.TrimLeft();
	sQuery.erase(sQuery.find(' '));

	switch (sQuery.CaseHash())
	{
		case "select"_casehash:
		case "table"_casehash:
		case "values"_casehash:
			return QueryType::Select;

		case "insert"_casehash:
		case "import"_casehash:
		case "load"_casehash:
			return QueryType::Insert;

		case "update"_casehash:
			return QueryType::Update;

		case "delete"_casehash:
		case "truncate"_casehash:
			return QueryType::Delete;

		case "create"_casehash:
			return QueryType::Create;

		case "alter"_casehash:
		case "rename"_casehash:
			return QueryType::Alter;

		case "drop"_casehash:
			return QueryType::Drop;

		case "start"_casehash:
		case "begin"_casehash:
		case "commit"_casehash:
		case "rollback"_casehash:
			return QueryType::Transaction;

		case "exec"_casehash:
		case "call"_casehash:
		case "do"_casehash:
			return QueryType::Execute;

		case "kill"_casehash:
		case "use"_casehash:
		case "set"_casehash:
		case "grant"_casehash:
		case "lock"_casehash:
		case "unlock"_casehash:
			return QueryType::Action;

		case "analyze"_casehash:
		case "optimize"_casehash:
		case "check"_casehash:
		case "repair"_casehash:
			return QueryType::Maintenance;

		case "desc"_casehash:
		case "explain"_casehash:
		case "show"_casehash:
			return QueryType::Info;

		case "any"_casehash:
			return bAllowAny ? QueryType::Any : QueryType::Other;

		default:
			return QueryType::Other;
	}

	// gcc..
	return QueryType::Other;

} // GetQueryType

//-----------------------------------------------------------------------------
KStringViewZ KSQL::SerializeOneQueryType(QueryType OneQuery)
//-----------------------------------------------------------------------------
{
	switch (OneQuery)
	{
		case QueryType::Any:
			return "any";

		case QueryType::None:
			return "none";

		case QueryType::Select:
			return "select";

		case QueryType::Insert:
			return "insert";

		case QueryType::Update:
			return "update";

		case QueryType::Delete:
			return "delete";

		case QueryType::Create:
			return "create";

		case QueryType::Alter:
			return "alter";

		case QueryType::Drop:
			return "drop";

		case QueryType::Transaction:
			return "transaction";

		case QueryType::Execute:
			return "execute";

		case QueryType::Action:
			return "action";

		case QueryType::Maintenance:
			return "maintenance";

		case QueryType::Info:
			return "info";

		case QueryType::Other:
			return "other";
	}

	// gcc ..
	return "other";

} // SerializeQueryType

//-----------------------------------------------------------------------------
std::vector<KStringViewZ> KSQL::SerializeQueryTypes(QueryType Query)
//-----------------------------------------------------------------------------
{
	std::vector<KStringViewZ> Result;

	if (Query >= QueryType::Any)
	{
		Result.push_back("any");
	}
	else
	{
		using eType = std::underlying_type<QueryType>::type;

		eType mask = 1;

		for (;;)
		{
			auto Masked = Query & static_cast<QueryType>(mask);

			if (Masked != QueryType::None)
			{
				Result.push_back(SerializeOneQueryType(Masked));
			}

			if (mask >= static_cast<eType>(QueryType::Any))
			{
				break;
			}

			mask <<= 1;
		}
	}

	return Result;

} // SerializeQueryType

//-----------------------------------------------------------------------------
bool KSQL::ExecLastRawSQL (Flags iFlags/*=0*/, KStringView sAPI/*="ExecLastRawSQL"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}\n", sAPI, kLimitSize(m_sLastSQL.str(), 4096));
	}

	auto QueryType = GetQueryType(m_sLastSQL.str());

	if (IsReadOnlyViolation (QueryType))
	{
		return false;
	}

	bool bQueryTypeMatch { false };

	if (m_bEnableQueryTimeout && m_QueryTimeout > std::chrono::milliseconds(0) && (m_QueryTypeForTimeout & QueryType) == QueryType)
	{
		// we shall cancel long-running queries
		bQueryTypeMatch = true;

		// make sure we have a connection ID (this may lead to a recursion, and we want it
		// right at this place, not any later!)
		// Due to this recursion, make sure that all code above this point is recursion
		// safe!
		if (GetConnectionID(false) == 0)
		{
			// recursion protection..
			DisableQueryTimeout();
			auto sBackupSQL = m_sLastSQL;
			GetConnectionID();
			m_sLastSQL = sBackupSQL;
			EnableQueryTimeout();
		}

		// add this query to the list of timed connections..
		kDebug(2, "'{}' query will timeout after {}ms", SerializeOneQueryType(QueryType), m_QueryTimeout.count());
		s_TimedConnections.Add(*this);
	}

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard  = [this]() noexcept { s_TimedConnections.Remove(*this); };
	// construct the scope guard to revoke our connection from the connection timeout
	// watchlist at end of block
	auto GuardQueryTimeout = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	if (!bQueryTypeMatch)
	{
		// or don't ..
		GuardQueryTimeout.release();
	}

	m_SQLStmtStats.Collect(m_sLastSQL.str(), QueryType);

	m_iNumRowsAffected  = 0;
	EndQuery();

	bool    bOK          = false;
	int     iRetriesLeft = m_bDisableRetries ? 1 : NUM_RETRIES;
	int     iSleepFor    = 0;
	// we delay error output until the end of the loop, therefore
	// temporarily store the error here
	int     iErrorNum    = -1;
	KString sError;

	KStopTime Timer;

	while (!bOK && iRetriesLeft)
	{
		ClearError ();

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
						OpenConnection(m_iConnectTimeoutSecs);

						if (!m_dMYSQL)
						{
							kDebug (1, "failed.  aborting query or SQL:\n{}", kLimitSize(m_sLastSQL.str(), 4096));
							break;
						}
					}

					kDebug (3, "mysql_real_query(): m_dMYSQL is {}, SQL is {} bytes long", m_dMYSQL ? "not null" : "nullptr", m_sLastSQL.size());
					if (mysql_real_query (m_dMYSQL, m_sLastSQL.data(), m_sLastSQL.size()))
					{
						iErrorNum = mysql_errno (m_dMYSQL);
						sError    = kFormat ("KSQL: MSQL-{}: {}", iErrorNum, mysql_error(m_dMYSQL));
						break;
					}

					m_iNumRowsAffected = 0;
					kDebug (3, "mysql_affected_rows()...");
					my_ulonglong iNumRows = mysql_affected_rows (m_dMYSQL);
					if ((uint64_t)iNumRows != (uint64_t)(-1))
					{
						m_iNumRowsAffected = (uint64_t) iNumRows;
					}

					// TBC maybe it's time to change the selection from !Select
					// to Insert | Update ?
					if (QueryType != QueryType::Select)
					{
						// only refresh the last insert ID if this was NOT a
						// select statement - otherwise we get the last insert ID
						// from the last insert over and over again
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
				iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
											(text*)m_sLastSQL.data(), m_sLastSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);

				if (!WasOCICallOK("ExecSQL:OCIStmtPrepare", iErrorNum, sError))
				{
					break;
				}

				// Note: Using the OCI_COMMIT_ON_SUCCESS mode of the OCIExecute() call, the
				// application can selectively commit transactions at the end of each
				// statement execution, saving an extra roundtrip by calling OCITransCommit().
				kDebug (3, "OCIStmtExecute...");
				iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
								 (CONST OCISnapshot *) nullptr, (OCISnapshot *) nullptr, IsFlag(F_NoAutoCommit) ? 0 : OCI_COMMIT_ON_SUCCESS);

				if (!WasOCICallOK("ExecSQL:OCIStmtExecute", iErrorNum, sError))
				{
					break;
				}

				m_iNumRowsAffected = 0;
				iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumRowsAffected,
					0, OCI_ATTR_ROW_COUNT, (OCIError*)m_dOCI8ErrorHandle);

				bOK = true;
				break;

			// - - - - - - - - - - - - - - - - -
			case API::OCI6:
			// - - - - - - - - - - - - - - - - -
				// let the RDBMS parse the SQL expression:
				kDebug (3, "oparse...");
				iErrorNum = oparse ((Cda_Def*)m_dOCI6ConnectionDataArea, (text *)m_sLastSQL.c_str(), (sb4) -1, (sword) PARSE_NO_DEFER, (ub4) PARSE_V7_LNG);
				if (!WasOCICallOK("ExecSQL:oparse", iErrorNum, sError))
				{
					break;
				}

				// execute it:
				kDebug (3, "oexec...");
				iErrorNum = oexec ((Cda_Def*)m_dOCI6ConnectionDataArea);
				if (!WasOCICallOK("ExecSQL:oexec", iErrorNum, sError))
				{
					break;
				}

				// now issue a "commit" or it will be rolled back automatically:
				if (!IsFlag(F_NoAutoCommit))
				{
					iErrorNum = ocom ((Lda_Def*)m_dOCI6LoginDataArea);
					if (!WasOCICallOK("ExecSQL:ocom", iErrorNum, sError))
					{
						sError += "\non auto commit.";
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
					OpenConnection(m_iConnectTimeoutSecs);

					if (!ctlib_is_initialized())
					{
						kDebug (1, "failed.  aborting query or SQL:\n{}", kLimitSize(m_sLastSQL.str(), 4096));
						break; // once
					}
				}

				bOK = ctlib_execsql (m_sLastSQL.str());

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
			if (!(iRetriesLeft--) || !PreparedToRetry(iErrorNum, &sError))
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

	if (!bOK)
	{
		return SetError(sError, iErrorNum);
	}

	LogPerformance (Timer.milliseconds(), /*bIsQuery=*/false);

	return (bOK);

} // ExecLastRawSQL

//-----------------------------------------------------------------------------
void KSQL::LogPerformance (KDuration iMilliseconds, bool bIsQuery)
//-----------------------------------------------------------------------------
{
	auto iQueryType = GetQueryType(m_sLastSQL.str());
	if ((iQueryType == QueryType::Insert) || (iQueryType == QueryType::Update) || (iQueryType == QueryType::Delete))
	{
		kDebug (GetDebugLevel(), "KSQL: {} rows affected.\n", kFormNumber(m_iNumRowsAffected));
	}

	if (!(GetFlags() & F_NoKlogDebug))
	{
		KString sThreshold;
		if (m_iWarnIfOverMilliseconds > chrono::milliseconds::zero())
		{
			sThreshold += kFormat (", warning set to {} ms", kFormNumber(m_iWarnIfOverMilliseconds));
			kDebug (GetDebugLevel(), "took: {} ms{}", kFormNumber(iMilliseconds), sThreshold);
		}
	}

	if (m_iWarnIfOverMilliseconds > chrono::milliseconds::zero()
		&& (iMilliseconds > m_iWarnIfOverMilliseconds))
	{
		KString sWarning;
		sWarning.Format (
				"KSQL: warning: statement took longer than {} ms (took {}) against db {}:\n"
				"{}\n",
					kFormNumber(m_iWarnIfOverMilliseconds),
					kFormNumber(iMilliseconds),
					ConnectSummary(),
					m_sLastSQL);

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
bool KSQL::PreparedToRetry (uint32_t iErrorNum, KString* sError)
//-----------------------------------------------------------------------------
{
	// Note: this is called every time m_sLastError and m_iLastErrorNum have
	// been set by an RDBMS error.

	if (m_iConnectionID > 0 && s_CanceledConnections.HasAndRemove(m_iConnectionID))
	{
		// this was a canceled connection or query - do not retry
		kDebug(2, "connection {} was canceled on demand, query retry aborted", m_iConnectionID);
		// make sure all state is correctly reset
		CloseConnection();
		OpenConnection(m_iConnectTimeoutSecs);

		if (sError)
		{
			// notify our caller that this connection was canceled on demand
			*sError = "connection was canceled on demand";
		}

		return false;
	}

	bool bConnectionLost { false };
	bool bDisplayWarning { false };

	switch (m_iDBType)
	{
#ifdef DEKAF2_HAS_MYSQL
		case DBT::MYSQL:
			switch (iErrorNum)
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
			case DBT::SQLSERVER15:
			case DBT::SYBASE:
				switch (iErrorNum)
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
				m_TimingCallback (*this, chrono::milliseconds(0), kFormat ("{}\n{}", GetLastError(), "automatic retry now in progress..."));
			}
		}

		CloseConnection ();
		OpenConnection (m_iConnectTimeoutSecs);

		if (IsConnectionOpen())
		{
			kDebug (GetDebugLevel(), "new connection looks good.");
		}
		else if (IsFlag(F_IgnoreSQLErrors))
		{
			kDebug (GetDebugLevel(), "NEW CONNECTION FAILED.");
		}
		else
		{
			kWarning ("NEW CONNECTION FAILED.");
			if (m_TimingCallback)
			{
				m_TimingCallback (*this, chrono::milliseconds(0), "NEW CONNECTION FAILED.");
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
	if (!OpenConnection(m_iConnectTimeoutSecs))
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
bool KSQL::ParseRawSQL (KStringView sSQL, Flags iFlags/*=Flags::F_None*/, KStringView sAPI/*="ParseRawSQL"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}{}\n", sAPI, (sSQL.contains("\n")) ? "\n" : "", sSQL);
	}

	if (IsReadOnlyViolation (m_sLastSQL))
	{
		return false;
	}

	CopyIfNotSame(m_sLastSQL, sSQL);
	ResetErrorStatus ();

	switch (m_iAPISet)
	{
		// - - - - - - - - - - - - - - - - -
		case API::OCI8:
		// - - - - - - - - - - - - - - - - -
			auto iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
										 (text*)sSQL.data(), sSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);
			KString sError;
			if (!WasOCICallOK("ParseSQL:OCIStmtPrepare", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
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
			auto iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
							 (CONST OCISnapshot *) nullptr, (OCISnapshot *) nullptr, IsFlag(F_NoAutoCommit) ? 0 : OCI_COMMIT_ON_SUCCESS);
			KString sError;
			if (!WasOCICallOK("ParseSQL:OCIStmtExecute", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
			}

			m_iNumRowsAffected = 0;
			iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumRowsAffected,
				0, OCI_ATTR_ROW_COUNT, (OCIError*)m_dOCI8ErrorHandle);

			break;

		// - - - - - - - - - - - - - - - - -
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("unsupported API Set ({}={})", m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	m_idxBindVar = 0; // <-- reset index into m_OCIBindArray[]

	return (true);

} // ExecParsedSQL
#endif

//-----------------------------------------------------------------------------
bool KSQL::ExecSQLFile (KStringViewZ sFilename)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	KAtScopeEnd( ClearErrorPrefix() );

	EndQuery ();
	if (!IsConnectionOpen() && !OpenConnection(m_iConnectTimeoutSecs))
	{
		return (false);
	}

	kDebug (GetDebugLevel(), "{}\n", sFilename);

	if (!kFileExists (sFilename))
	{
		return SetError(kFormat ("ExecSQLFile(): file '{}' does not exist.", sFilename));
	}

	KInFile File(sFilename);
	if (!File.is_open())
	{
		return SetError(kFormat ("ExecSQLFile(): {} could not read from file '{}'", kGetWhoAmI(), sFilename));
	}

	if (IsFlag(F_ReadOnlyMode))
	{
		return SetError(kFormat ("KSQL: attempt to execute a SQL file on a READ ONLY db connection: {}", sFilename));
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
	KString sLeader = (m_iDBType == DBT::SQLSERVER || m_iDBType == DBT::SQLSERVER15)
		? "MSS"
		: TxDBType(m_iDBType).ToUpper();

	if (sLeader.size() > 3)
	{
		sLeader.erase(3);
	}
	sLeader += '|';
	sLeader.insert(0, "//");

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
			auto Parts = sStart.Split('"');
			if (Parts.size() < 2)
			{
				return SetError(kFormat ("{}:{}: malformed include directive (file should be enclosed in double quotes).", sFilename, Parms.iLineNum));
			}

			// Allow for ${myvar} variable expansion on include filenames
			KString sIncludedFile = Parts[1];

			static constexpr KStringView s_sOpen  = "${";
			static constexpr KStringView s_sClose = "}";

			kReplaceVariables(sIncludedFile, s_sOpen, s_sClose, true /* bQueryEnvironment */);

			// TODO check if test for escaped ${ is needed ( "\${" )

			if (!kFileExists (sIncludedFile))
			{
				return SetError(kFormat ("{}:{}: missing include file: {}", sFilename, Parms.iLineNum, sIncludedFile));
			}
			kDebug (GetDebugLevel(), "{}, line {}: recursing to include file: {}", sFilename, Parms.iLineNum, sIncludedFile);
			if (!ExecSQLFile (sIncludedFile))
			{
				// Error is already set
				return false;
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
					return SetError(kFormat ("{}:{}: malformed token: {}", sFilename, Parms.iLineNum, sStart));
				}
				auto iEnd = sStart.find("}}"_ksv);
				if (iEnd == KStringView::npos)
				{
					return SetError(kFormat ("{}:{}: malformed {{token}}: {}", sFilename, Parms.iLineNum, sStart));
				}
				KStringView sKey   = sStart.substr(strlen("{{"), iEnd-strlen("{{")).Trim();
				KStringView sValue = sStart.substr(iEnd+strlen("}}")).Trim();
				m_TxList.Add (sKey, sValue);
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
			m_sLastSQL.ref() += sStart;  // <-- add the line to the SQL statement buffer
		}
		else
		{
			m_sLastSQL.ref() += sLine;    // <-- add the line to the SQL statement buffer [with leading whitespace]
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
	}

	auto Guard = ScopedFlags (Parms.fDropStatement ? F_IgnoreSQLErrors : F_None);

	SetErrorPrefix (sFilename, Parms.iLineNumStart);

	kDebug (GetDebugLevel()+1, "{}: statement # {}:\n{}\n", sFilename, Parms.iStatement, kLimitSize(m_sLastSQL.str(), 4096));

	if (m_sLastSQL.empty())
	{
		kDebug (GetDebugLevel()+1, "{}: statement # {} is EMPTY.", sFilename, Parms.iStatement);
	}
	else if (m_sLastSQL == "exit\n"   || m_sLastSQL == "quit\n")
	{
		kDebug (GetDebugLevel()+1, "{}: statement # {} is '{}' (stopping).", sFilename, Parms.iStatement, kLimitSize(m_sLastSQL.str(), 4096));
		Parms.fOK = Parms.fDone = true;
	}
	else if (IsSelect(m_sLastSQL.str()))
	{
		kDebug (3, "{}: statement # {} is a QUERY...", sFilename, Parms.iStatement);

		if (!IsFlag(F_NoTranslations))
		{
			DoTranslations (m_sLastSQL);
		}
	
		if (!ExecLastRawQuery (Flags::F_None, "ExecSQLFile") && !Parms.fDropStatement)
		{
			Parms.fOK   = false;
			Parms.fDone = true;
		}
	}
	else if (m_sLastSQL.str().starts_with("analyze") || m_sLastSQL.str().starts_with("ANALYZE"))
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
					kDebug (3, "{} table {} {}={}", sOp, sTable, sMsgType, sMsgText);
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

		if (!ExecLastRawSQL (Flags::F_None, "ExecSQLFile") && !Parms.fDropStatement)
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
		SetFlags (Flags::F_None);
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
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}{}\n", sAPI, (m_sLastSQL.str().find('\n') != KString::npos) ? "\n" : "", kLimitSize(m_sLastSQL.str(), 4096));
	}

	EndQuery();

	if (!IsConnectionOpen() && !OpenConnection(m_iConnectTimeoutSecs))
	{
		return (false);
	}

	if (!(iFlags & F_IgnoreSelectKeyword) && !IsFlag(F_IgnoreSelectKeyword) && ! IsSelect (m_sLastSQL.str()))
	{
		return SetError ("ExecQuery: query does not start with keyword 'select' [see F_IgnoreSelectKeyword]");
	}

	KStopTime Timer;

	#if defined(DEKAF2_HAS_CTLIB)
	uint32_t iRetriesLeft = NUM_RETRIES;
	#endif

	ClearError ();

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
				auto iErrorNum = mysql_errno (m_dMYSQL);
				if (iErrorNum == 0)
				{
					return SetError("KSQL: expected query results but got none. Did you intend to use ExecSQL() instead of ExecQuery() ?", iErrorNum);
				}
				else
				{
					return SetError(kFormat("KSQL: MSQL-{}: {}", iErrorNum, mysql_error(m_dMYSQL)), iErrorNum);
				}
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
				ColInfo.SetColumnType (DBT::MYSQL, pField->type, static_cast<KCOL::Len>(pField->length));
				kDebug (3, "col {:35} mysql_datatype: {:4} => ksql_flags: 0x{:08x} = {}",
					ColInfo.sColName, std::to_underlying(pField->type), std::to_underlying(ColInfo.iKSQLDataType), KCOL::FlagsToString(ColInfo.iKSQLDataType));

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
			auto iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
		                             (text*)m_sLastSQL.data(), m_sLastSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			KString sError;
			if (!WasOCICallOK("ExecQuery:OCIStmtPrepare", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
			}

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 2. OCI8: pre-execute (parse only) -- redundant trip to server until I get this all working
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
					/*iters=*/1, /*rowoff=*/0, /*snap_in=*/nullptr, /*snap_out=*/nullptr, OCI_DESCRIBE_ONLY);

			if (!WasOCICallOK("ExecQuery:OCIStmtExecute(parse)", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
			}

			// Get the number of columns in the query:
			iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumColumns,
				0, OCI_ATTR_PARAM_COUNT, (OCIError*)m_dOCI8ErrorHandle);

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 3. OCI8: allocate storage for the results set:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ExecQuery:OCIAttrGet(numcols)", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
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
				iErrorNum = OCIParamGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, (OCIError*)m_dOCI8ErrorHandle, (void**)&pColParam, ii);
				if (!WasOCICallOK("ExecQuery:OCIParamGet", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				if (!pColParam)
				{
					kDebug (GetDebugLevel(), "OCI: pColParam is nullptr but was supposed to get allocated by OCIParamGet");
				}

				// Retrieve the data type attribute:
				iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM,
					(dvoid*) &iDataType, (ub4 *) nullptr, OCI_ATTR_DATA_TYPE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(datatype)", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				// Retrieve the column name attribute:
				iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM,
					(dvoid**) &dszColName, (ub4 *) &iLenColName, OCI_ATTR_NAME, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(colname)", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				// Retreive the maximum datasize:
				iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM,
					(void**) &iMaxDataSize, (ub4 *) nullptr, OCI_ATTR_DATA_SIZE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(datasize)", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				KColInfo ColInfo;

				ColInfo.SetColumnType(DBT::ORACLE8, iDataType, std::max(iMaxDataSize+2, 22+2)); // <-- always malloc at least 24 bytes
				ColInfo.sColName.assign((char* )dszColName, iLenColName);
				ColInfo.dszValue = std::make_unique<char[]>(ColInfo.iMaxDataLen + 1);

				kDebug (GetDebugLevel()+1, "  oci8:column[{}]={}, namelen={}, datatype={}, maxwidth={}, willuse={}",
					ii, ColInfo.sColName, iLenColName, iDataType, iMaxDataSize, ColInfo.iMaxDataLen);

				OCIDefine* dDefine;
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				iErrorNum = OCIDefineByPos ((OCIStmt*)m_dOCI8Statement, &dDefine, (OCIError*)m_dOCI8ErrorHandle,
							ii,                                   // column number
							(dvoid *)(ColInfo.dszValue.get()),    // data pointer
							ColInfo.iMaxDataLen,                  // max data length
							SQLT_STR,                             // always bind as a zero-terminated string
							(dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecQuery:OCIDefineByPos", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				OCIHandleFree ((dvoid*)dDefine, OCI_HTYPE_DEFINE);

				m_dColInfo.push_back(std::move(ColInfo));

			} // foreach column

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. OCI8: execute statement (start the query)...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
						 (OCISnapshot *) nullptr, (OCISnapshot *) nullptr, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iOCI8FirstRowStat = iErrorNum;
			if ((iErrorNum != OCI_NO_DATA) && !WasOCICallOK("ExecQuery:OCIStmtExecute", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
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
			iErrorNum = oparse((Cda_Def*)m_dOCI6ConnectionDataArea, (text *)m_sLastSQL.c_str(), -1, PARSE_NO_DEFER, PARSE_V7_LNG);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			KString sError;
			if (!WasOCICallOK("ExecQuery:oparse", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
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
				iErrorNum = odescr((Cda_Def*)m_dOCI6ConnectionDataArea,
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

				if (!WasOCICallOK("ExecQuery:odescr1", iErrorNum, sError))
				{
					if (!tmpMaxDataLen)
					{
						iErrorNum = 0;
						break; // no more columns
					}
					if (((Cda_Def*)m_dOCI6ConnectionDataArea)->rc != VAR_NOT_IN_LIST)
					{
						return SetError(sError, iErrorNum);
					}
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
				iErrorNum = odescr((Cda_Def*)m_dOCI6ConnectionDataArea, ii,
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

				if (!WasOCICallOK("ExecQuery:odescr2", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
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
				iErrorNum = odefin ((Cda_Def*)m_dOCI6ConnectionDataArea,
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
				if (!WasOCICallOK("ExecQuery:odefin", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				m_dColInfo.push_back(std::move(ColInfo));

			} // while binding

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. start the select statement...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebug (3, "  calling oexec() to actually begin the SQL statement...");
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			iErrorNum = oexec ((Cda_Def*)m_dOCI6ConnectionDataArea);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			if (!WasOCICallOK("ExecQuery:oexec", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
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

			if (ct_command (m_pCtCommand, CS_LANG_CMD, m_sLastSQL.data(), static_cast<CS_INT>(m_sLastSQL.size()), CS_UNUSED) != CS_SUCCEED)
			{
				ctlib_api_error ("KSQL>ExecQuery>ct_command");
				if (--iRetriesLeft && PreparedToRetry(m_iCtLibErrorNum))
				{
					continue;
				}
				return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
			}

			kDebug (KSQL2_CTDEBUG, "calling ct_send()...");

			if (ct_send(m_pCtCommand) != CS_SUCCEED)
			{
				ctlib_api_error ("KSQL>ExecQuery>ct_send");
				if (--iRetriesLeft && PreparedToRetry(m_iCtLibErrorNum))
				{
					continue;
				}
				return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
			}

			if (!ctlib_prepare_results ())
			{
				if (--iRetriesLeft && PreparedToRetry(m_iCtLibErrorNum))
				{
					continue;
				}
				return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
			}

			if (ctlib_check_errors() != 0)
			{
				if (--iRetriesLeft && PreparedToRetry(m_iCtLibErrorNum))
				{
					continue;
				}
				return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
		SetError(kFormat("unsupported API Set ({})", TxAPISet(m_iAPISet)));
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

	LogPerformance (Timer.milliseconds(), /*bIsQuery=*/true);
	
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
	if (!OpenConnection(m_iConnectTimeoutSecs))
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
		return SetError ("ParseQuery: query does not start with keyword 'select' [see F_IgnoreSelectKeyword]");
	}

	return (ParseRawQuery (m_sLastSQL, 0, "ParseQuery"));

} // ParseQuery
#endif

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ParseRawQuery (KStringView sSQL, Flags iFlags/*=Flags::F_None*/, KStringView sAPI/*="ParseRawQuery"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): {}{}\n", sAPI, (sSQL.contains("\n")) ? "\n" : "", sSQL);
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
			auto iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
										 (text*)sSQL.data(), sSQL.size(), OCI_NTV_SYNTAX, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			KString sError;
			if (!WasOCICallOK("ParseQuery:OCIStmtPrepare", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
			}

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
			auto iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle,
					/*iters=*/1, /*rowoff=*/0, /*snap_in=*/nullptr, /*snap_out=*/nullptr, OCI_DESCRIBE_ONLY);
			KString sError;
			if (!WasOCICallOK("ExecParsedQuery:OCIStmtExecute(parse)", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
			}

			// Get the number of columns in the query:
			iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumColumns,
				0, OCI_ATTR_PARAM_COUNT, (OCIError*)m_dOCI8ErrorHandle);

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 3. OCI8: allocate storage for the results set:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(numcols)", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
			}

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
				iErrorNum = OCIParamGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, (OCIError*)m_dOCI8ErrorHandle, (void**)&pColParam, ii);
				if (!WasOCICallOK("ExecParsedQuery:OCIParamGet", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				if (!pColParam)
					kDebug (GetDebugLevel(), "OCI: pColParam is nullptr but was supposed to get allocated by OCIParamGet");

				// Retrieve the data type attribute:
				iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM,
					(dvoid*) &iDataType, (ub4 *) nullptr, OCI_ATTR_DATA_TYPE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(datatype)", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				// Retrieve the column name attribute:
				mErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM,
					(dvoid**) &dszColName, (ub4 *) &iLenColName, OCI_ATTR_NAME, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(colname)", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				// Retreive the maximum datasize:
				iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM,
					(void**) &iMaxDataSize, (ub4 *) nullptr, OCI_ATTR_DATA_SIZE,
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(datasize)", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				KColInfo ColInfo;

				ColInfo.SetColumnType(DBT::ORACLE8, iDataType, std::max(iMaxDataSize+2, 22+2)); // <-- always malloc at least 24 bytes
				ColInfo.sColName.assign(dszColName, iLenColName);
				ColInfo.dszValue = std::make_unique<char[]>(ColInfo.iMaxDataLen + 1);

				kDebug (GetDebugLevel()+1, "  oci8:column[{}]={}, namelen={}, datatype={}, maxwidth={}, willuse={}",
						ii, ColInfo.sColName, iLenColName, iDataType, iMaxDataSize, ColInfo.iMaxDataLen);

				OCIDefine* dDefine;
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				iErrorNum = OCIDefineByPos ((OCIStmt*)m_dOCI8Statement, &dDefine, (OCIError*)m_dOCI8ErrorHandle,
							ii,                                   // column number
							(dvoid *)(ColInfo.dszValue.get()),    // data pointer
							ColInfo.iMaxDataLen,                  // max data length
							SQLT_STR,                             // always bind as a zero-terminated string
							(dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecParsedQuery:OCIDefineByPos", iErrorNum, sError))
				{
					return SetError(sError, iErrorNum);
				}

				OCIHandleFree ((dvoid*)dDefine, OCI_HTYPE_DEFINE);

				m_dColInfo.push_back(std::move(ColInfo));

			} // foreach column

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. OCI8: execute statement (start the query)...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
						 (OCISnapshot *) nullptr, (OCISnapshot *) nullptr, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iOCI8FirstRowStat = m_iErrorNum;
			if ((iErrorNum != OCI_NO_DATA) && !WasOCICallOK("ExecParsedQuery:OCIStmtExecute", iErrorNum, sError))
			{
				return SetError(sError, iErrorNum);
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
		// bool false converts to 0
		return SetError("GetNumCols(): called when query was not started.");
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
		return SetError("BufferResults(): called when query was not started.");
	}

	RemoveTempResultsFile();

	KOutFile file(GetTempResultsFile());
	if (!file.Good())
	{
		return SetError(kFormat ("BufferResults(): could not buffer results b/c {} could not write to '{}'",
								 kwhoami(), GetTempResultsFile()));
	}

	m_iNumRowsBuffered = 0;

	ClearError ();

	switch (m_iAPISet)
	{
    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - -
	case API::MYSQL:
	// - - - - - - - - - - - - - - - - -
		kDebug (3, "mysql_fetch_row() X N ...");

		while ((m_MYSQLRow = mysql_fetch_row (m_dMYSQLResult)))
		{
			m_MYSQLRowLens = mysql_fetch_lengths(m_dMYSQLResult);

			++m_iNumRowsBuffered;
			for (KROW::Index ii=0; ii<m_iNumColumns; ++ii)
			{
				KString sColVal;
				if (m_MYSQLRowLens)
				{
					sColVal = KStringView(m_MYSQLRow[ii], m_MYSQLRowLens[ii]);
				}
				else
				{
					// fall back to C strcpy
					sColVal = m_MYSQLRow[ii];
				}

				// we need to assure that no newlines are in buffered data:
				sColVal.Replace('\n', 2);  // <-- change them to ^B (warning: this kills binary columns, it should be switched to an escaping format)

				if (sColVal.size() > 50)
				{
					kDebug (3, "  buffered: row[{}]col[{}]: strlen()={}", m_iNumRowsBuffered, ii+1, sColVal.size());
				}
				else
				{
					kDebug (3, "  buffered: row[{}]col[{}]: '{}'", m_iNumRowsBuffered, ii+1, sColVal);
				}

				file.FormatLine ("{}|{}|{}", m_iNumRowsBuffered, ii+1, sColVal.size());
				file.FormatLine ("{}\n", sColVal);
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
			uint32_t iErrorNum = 0;

			if (!m_iNumRowsBuffered)
			{
				if (m_iOCI8FirstRowStat == OCI_NO_DATA)
				{
					kDebug (3, "OCIStmtExecute() said we were done");
					break; // while
				}

				// OCI8: first row was already (implicitly) fetched by OCIStmtExecute() [yuk]
				iErrorNum = m_iOCI8FirstRowStat;

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
				iErrorNum = OCIStmtFetch ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				KString sError;
				if (!WasOCICallOK("BufferResults::OCIStmtFetch", iErrorNum, sError))
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
			auto iErrorNum = ofetch ((Cda_Def*)m_dOCI6ConnectionDataArea);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			KString sResult;
			if (!WasOCICallOK("BufferResults:ofetch"), iErrorNum, sResult)
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
	m_bpBufferedResults = fopen (GetTempResultsFile().c_str(), "re");
#else
	m_bpBufferedResults = fopen (GetTempResultsFile().c_str(), "r");
#endif
	if (!m_bpBufferedResults)
	{
		return SetError(kFormat ("BufferResults(): bizarre: {} could not read from '{}', right after creating it.",
								 kwhoami(), GetTempResultsFile()));
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
		return SetError("NextRow(): next row called, but no ExecQuery() was started.");
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

		ClearError ();

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
					m_MYSQLRowLens = mysql_fetch_lengths(m_dMYSQLResult);
					return true;
				}
				else
				{
					kDebug (3, "{} row{} fetched (end was hit)", m_iRowNum, (m_iRowNum==1) ? " was" : "s were");
					m_MYSQLRowLens = nullptr;
					return false;
				}
				break;
#endif

#ifdef DEKAF2_HAS_ORACLE
				// - - - - - - - - - - - - - - - - -
			case API::OCI8:
				// - - - - - - - - - - - - - - - - -
				uint32_t iErrorNum = 0;

				if (!m_iRowNum)
				{
					if (m_iOCI8FirstRowStat == OCI_NO_DATA)
					{
						kDebug (3, "OCIStmtExecute() said we were done");
						return (false); // end of data from select
					}

					// OCI8: first row was already (implicitly) fetched by OCIStmtExecute() [yuk]
					iErrorNum = m_iOCI8FirstRowStat;

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
					iErrorNum = OCIStmtFetch ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
					//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
					KString sError;
					if (!WasOCICallOK("NextRow:OCIStmtFetch", iErrorNum, sError))
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
				auto iErrorNum = ofetch ((Cda_Def*)m_dOCI6ConnectionDataArea);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				KString sError;
				if (!WasOCICallOK("NextRow:ofetch", iErrorNum, sError))
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
			return SetError("NextRow(): BUG: results are supposed to be buffered but m_bFileIsOpen says false.");
		}
		else if (!m_bpBufferedResults)
		{
			return SetError("NextRow(): BUG: results are supposed to be buffered but m_bpBufferedResults is nullptr!!");
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
				return SetError("NextRow(): ran out of lines in buffered results file.");
			}

			if (szStatLine[strlen(szStatLine)-1] != '\n')
			{
				return SetError(kFormat ("NextRow(): buffered results corrupted stat line for row={}, col={}: '{}'",
										 m_iRowNum, ii+1, szStatLine));
			}

			szStatLine[strlen(szStatLine)-1] = 0; // <-- trim the newline

			// row|col|strlen
			KStack<KStringView> Parts;
			if (kSplit (Parts, szStatLine, '|') != 3)
			{
				return SetError(kFormat ("NextRow(): buffered results corrupted stat line for row={}, col={}: '{}'",
										 m_iRowNum, ii+1, szStatLine));
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
				return SetError(kFormat ("NextRow(): buffered results stat line out of sync for row={}, col={}",
										 m_iRowNum, ii+1));
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
				return SetError(kFormat ("NextRow(): buffered results corrupted data line for row={}, col={}",
										 m_iRowNum, ii+1));
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

	if (GetDBType() == DBT::SQLSERVER ||
		GetDBType() == DBT::SQLSERVER15)
	{
		if (!ExecRawQuery(FormatSQL("select top 0 {} from {}", sExpandedColumns, Row.GetTablename()), Flags::F_None, "LoadColumnLayout"))
		{
			return false;
		}
	}
	else
	{
		if (!ExecRawQuery(FormatSQL("select {} from {} limit 0", sExpandedColumns, Row.GetTablename()), Flags::F_None, "LoadColumnLayout"))
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
KROW KSQL::SingleRawQuery (KSQLString sSQL, Flags iFlags/*=0*/, KStringView sAPI/*="SingleRawQuery"*/)
//-----------------------------------------------------------------------------
{
	KROW ROW;

	auto Guard = ScopedFlags(F_IgnoreSQLErrors | iFlags);

	bool bOK = ExecRawQuery (std::move(sSQL), Flags::F_None, sAPI);

	if (!bOK)
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): sql error: {}", sAPI, GetLastError());
	}
	else if (!NextRow(ROW))
	{
		kDebugLog (GetDebugLevel(), "KSQL::{}(): expected one row back and didn't get it", sAPI);
	}
	else if (kWouldLog(1) && NextRow())
	{
		kDebugLog (1, "KSQL::{}(): expected one row back, but got at least two", sAPI);
	}

	EndQuery();

	return ROW;

} // SingleRawQuery

//-----------------------------------------------------------------------------
KString KSQL::SingleStringRawQuery (KSQLString sSQL, Flags iFlags/*=0*/, KStringView sAPI/*="SingleStringRawQuery"*/)
//-----------------------------------------------------------------------------
{
	auto ROW = SingleRawQuery(std::move(sSQL), iFlags, sAPI);

	if (ROW.empty())
	{
		return {};
	}

	kDebugLog (GetDebugLevel(), "KSQL::{}(): got {}\n", sAPI, ROW.GetValue(0));

	if (ROW.size() > 1)
	{
		kDebugLog (1, "KSQL::{}(): expected one result column, but got {}", sAPI, ROW.size());
	}

	return ROW.GetValue(0);

} // SingleStringRawQuery

//-----------------------------------------------------------------------------
int64_t KSQL::SingleIntRawQuery (KSQLString sSQL, Flags iFlags/*=0*/, KStringView sAPI/*="SingleIntRawQuery"*/)
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
		return SetError ("ResetBuffer(): no query is currently open.");
	}
	
	if (!IsFlag (F_BufferResults))
	{
		return SetError ("ResetBuffer(): F_BufferResults flag needs to be set *before* query is run.");
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
	m_bpBufferedResults = fopen (GetTempResultsFile().c_str(), "re");
#else
	m_bpBufferedResults = fopen (GetTempResultsFile().c_str(), "r");
#endif
	if (!m_bpBufferedResults)
	{
		return SetError(kFormat ("ResetBuffer(): bizarre: {} could not read from '{}'.",
								 kwhoami(), GetTempResultsFile()));
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
	else
	{
		kDebug (GetDebugLevel()+1, "  {} row{} fetched.", m_iRowNum, (m_iRowNum==1) ? " was" : "s were");
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
	if (GetDBType() == DBT::SQLSERVER   ||
		GetDBType() == DBT::SQLSERVER15 ||
		GetDBType() == DBT::SYBASE)
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

	RemoveTempResultsFile();

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
		SetError(kFormat ("{}() called, but no ExecQuery() was started.", "GetColProps"));
		return (s_NullResult);
	}

	if (iOneBasedColNum == 0)
	{
		SetError(kFormat ("{}() called with ColNum < 1 ({})", "GetColProps", iOneBasedColNum));
		return (s_NullResult);
	}

	else if (iOneBasedColNum > m_dColInfo.size())
	{
		SetError(kFormat ("{}() called with ColNum > {} ({})", "GetColProps", m_iNumColumns, iOneBasedColNum));
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
		SetError("() called, but no ExecQuery() was started.");
		return ("ERR:NOQUERY-KSQL::Get()");
	}

	if (!m_iRowNum)
	{
		SetError("() called, but NextRow() not called or did not return a row.");
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
					if (m_MYSQLRowLens)
					{
						sRtnValue = KStringView(static_cast<MYSQL_ROW>(m_MYSQLRow)[iOneBasedColNum-1],
												m_MYSQLRowLens[iOneBasedColNum-1]);
					}
					else
					{
						// fall back to C strcpy
						sRtnValue = static_cast<MYSQL_ROW>(m_MYSQLRow)[iOneBasedColNum-1];
					}
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
KUnixTime KSQL::GetUnixTime (KROW::Index iOneBasedColNum)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	// FYI: there is nothing database specific in this member function
	// (we get away with this by fetching all results as strings, then
	// using C routines to do data conversions)

	KString sVal (Get (iOneBasedColNum));

	if (sVal.empty() || sVal == "0" || sVal.starts_with("00000"))
	{
		return KUnixTime(0);
	}

	if (sVal.contains("ERR"))
	{
		SetError(kFormat ("IntValue(row={},col={}): {}", m_iRowNum, iOneBasedColNum+1, sVal));
		return KUnixTime(0);
	}

	KUnixTime tTime { 0 };

	switch (sVal.size())
	{
		// try the expected formats first
		case "1965-03-31 12:00:00"_ksv.size():
			tTime = kParseTimestamp("YYYY-MM-DD hh:mm:ss", sVal);
			break;

		case "20010302213436"_ksv.size():
			tTime = kParseTimestamp("YYYYMMDDhhmmss", sVal);
			break;
	}

	if (!tTime.ok())
	{
		// try if any of the other predefined time stamps matches
		tTime = kParseTimestamp(sVal);

		if (!tTime.ok())
		{
			SetError(kFormat ("UnixTime({}): expected '{}' to look like '20010302213436' or '2001-03-21 06:18:33'",
							  iOneBasedColNum, sVal));
		}
	}

	return tTime;
 
} // KSQL::GetUnixTime

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::WasOCICallOK (KStringView sContext, uint32_t iErrorNum, KStringRef& sError)
//-----------------------------------------------------------------------------
{
	sError.clear();
	switch (iErrorNum)
	{
	case OCI_SUCCESS: // 0
		sError = kFormat ("{}() returned {} ({})", sContext, m_iErrorNum, "OCI_SUCCESS");
		kDebugLog (3, "  {} -- returned OCI_SUCCESS", sContext);
		return (true);

	case OCI_SUCCESS_WITH_INFO:
		sError = kFormat ("{}() returned {} ({})", sContext, m_iErrorNum, "OCI_SUCCESS_WITH_INFO");
		kDebugLog (3, "  {} -- returned OCI_SUCCESS_WITH_INFO", sContext);
		return (true);

	case OCI_NEED_DATA:
		sError = kFormat ("OCI-{:05}: OCI_NEED_DATA [{}]", m_iErrorNum, sContext);
		break; // error

	case OCI_INVALID_HANDLE:
		sError = kFormat ("OCI-{:05}: OCI_INVALID_HANDLE [{}]", m_iErrorNum, sContext);
		break; // error

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// ORA-03123: operation would block
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		//   *Cause:  This is a status code that indicates that the operation
		//            cannot complete now. 
		//   *Action: None; this is not an error.  The operation should be retried
		//            again for completion. 

	case OCI_STILL_EXECUTING:  // ORA-03123
		sError = kFormat ("OCI-{:05}: OCI_STILL_EXECUTE [{}]", m_iErrorNum, sContext);
		break; // error

	case OCI_CONTINUE:
		sError = kFormat ("OCI-{:05}: OCI_CONTINUE [{}]", m_iErrorNum, sContext);
		break; // error

	case NO_DATA_FOUND:  // <-- old
	case -1403:          // <-- OERR_NODATA
	case OCI_NO_DATA:    // <-- current
//		m_iErrorNum = OCI_NO_DATA;
//		sError = kFormat ("OCI-{:05}: OCI_NO_DATA [{}]", m_iErrorNum, sContext);
//		break; // error
	case OCI_ERROR:
		if (GetAPISet() == API::OCI6)
		{
			if (m_dOCI6LoginDataArea && m_dOCI6ConnectionDataArea)
			{
				iErrorNum = ((Cda_Def*)m_dOCI6ConnectionDataArea)->rc;
				if (iErrorNum == 0)
				{
					// ORA-00000: no error
					iErrorNum = sqlca.sqlcode;
				}
				// TODO rewrite!
				oerhms ((Lda_Def*)m_dOCI6LoginDataArea, ((Cda_Def*)m_dOCI6ConnectionDataArea)->rc, (text*)sError.c_str()+strlen(sError.c_str()), MAXLEN_ERR-strlen(sError.c_str()));
			}
			else
				sError = "unknown error (m_dOCI6LoginDataArea not allocated yet)";
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
				// TODO rewrite!
				OCIErrorGet (m_dOCI8ErrorHandle, 1, nullptr, &sb4Stat, (text*)sError.c_str()+strlen(sError.c_str()), MAXLEN_ERR-strlen(sError.c_str()), OCI_HTYPE_ERROR);
				iErrorNum = sb4Stat;
			}
			else
			{
				sError = "unknown error (m_dOCI8ErrorHandler not allocated yet)";
			}
		}

		// TODO rewrite!
		if (strchr (sError.c_str(), '\n'))
			*(strchr (sError.c_str(),'\n')) = 0;
		snprintf (sError+strlen(sError), MAXLEN_ERR, " [{}]", sContext);

		if ( (iErrorNum == 0)    // ORA-00000: no error
		  || (iErrorNum == 1405) // ORA-01405: fetched column value is nullptr
		  || (iErrorNum == 1406) // ORA-01406: fetched column value was truncated
		)
		{
			kDebugLog (3, "  {} -- [ok] returned {}", sContext, sError.c_str());
			return (true);
		}
		break;

	//----------
	default:
	//----------

		if ((iErrorNum < 0) && (GetAPISet() == API::OCI6) && m_dOCI6LoginDataArea)
		{
			// translate old convention into current error messages:
			iErrorNum = -m_iErrorNum;
			// TODO rewrite!
			oerhms ((Lda_Def*)m_dOCI6LoginDataArea, iErrorNum, (text*)sError.c_str()+strlen(sError.c_str()), MAXLEN_ERR-strlen(sError.c_str()));
			return (false);
		}

		switch (m_iErrorNum)
		{
			// discovered during development with OCI6:
			case  4:    sError = "ORA-00004: object does not exist (?)";         break;

			// taken from ldap/public/omupi.h because my switch default was getting called on OCI6 calls:
			//se -1:    sError = "OERR_DUP: duplicate value in index";           break; -- dupe of OCI_ERROR
			case -54:   sError = "OERR_LOCKED: table locked";                    break;
			case -904:  sError = "OERR_NOCOL: unknown column name";              break;
			case -913:  sError = "OERR_2MNYVALS: too many values";               break;
			case -942:  sError = "OERR_NOTABLE: unknown table or view";          break;
			case -955:  sError = "OERR_KNOWN: object already exists";            break;
			case -1002: sError = "OERR_FOOSQ: fetch out of sequence";            break;
			case -1009: sError = "OERR_MPARAM: missing mandatory param";         break;
			case -1017: sError = "OERR_PASSWD: incorrect username or password";  break;
			case -1089: sError = "OERR_SHTDWN: shutdown";                        break;
			case -1430: sError = "OERR_DUPCOL: column already exists";           break;
			case -1451: sError = "OERR_NULL: column already null";               break;
			case -1722: sError = "OERR_INVNUM: invalid number";                  break;
			case -3113: sError = "OERR_EOFCC: end-of-file on comm channel";      break;
			case -3114: sError = "OERR_NOCONN: not connected";                   break;
			case -6111: sError = "OERR_TCPDISC: TCP disconnect";                 break;
			case -6550: sError = "OERR_PLSERR: PL/SQL problem";                  break;

			default:
				sError = kFormat("{}() returned {} ({})", sContext, m_iErrorNum, "UNKNOWN OCI RTN");
		}

	}

	kDebugLog (3, "  {} -- returned {}", sContext, sError));

	return (false);  // <-- on error, m_saLastError is formatted, but not sent anywhere yet

} // WasOCICallOK
#endif

//-----------------------------------------------------------------------------
bool KSQL::SetAPISet (API iAPISet)
//-----------------------------------------------------------------------------
{
	if (IsConnectionOpen())
	{
		return SetError("SetAPISet(): connection is already open.");
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
			return SetError("SetAPISet(): API mismatch: database type not set.");

		case DBT::MYSQL:                m_iAPISet = API::MYSQL;       break;

		case DBT::SQLITE3:              m_iAPISet = API::SQLITE3;     break;

		case DBT::ORACLE6:
		case DBT::ORACLE7:
		case DBT::ORACLE8:
		case DBT::ORACLE:               m_iAPISet = API::OCI8;        break;

		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
		case DBT::SYBASE:               m_iAPISet = API::CTLIB;       break; // choices: API::DBLIB -or- API::CTLIB

		case DBT::INFORMIX:             m_iAPISet = API::INFORMIX;    break;

		default:
			return SetError(kFormat ("SetAPISet(): unsupported database type ({})", TxDBType(m_iDBType)));
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
	//  return SetError ("SetFlags(): you cannot change flags while a query is in process.");
	//}

	auto iSaved = m_iFlags;
	m_iFlags = iFlags;

	return (iSaved);

} // KSQL::SetFlags

//-----------------------------------------------------------------------------
KSQL::Flags KSQL::SetFlag (Flags iFlag)
//-----------------------------------------------------------------------------
{
	return SetFlags (GetFlags() | iFlag);

} // KSQL::SetFlag

//-----------------------------------------------------------------------------
KSQL::Flags KSQL::ClearFlag (Flags iFlag)
//-----------------------------------------------------------------------------
{
	return SetFlags (GetFlags() & ~iFlag);

} // KSQL::ClearFlag

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
			case DBT::SQLSERVER15:
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
void KSQL::DoTranslations (KSQLString& sSQL)
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

	if (!kReplaceVariables(sSQL.ref(), s_sOpen, s_sClose, /*bQueryEnvironment=*/false, m_TxList))
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
		case DBT::SQLSERVER15:  return ("SQLServer15");
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
uint64_t KSQL::GetHash () const
//-----------------------------------------------------------------------------
{
	if (!m_iConnectHash)
	{
		m_iConnectHash = kFormat("{}:{}:{}:{}:{}:{}", TxAPISet(m_iAPISet), m_sUsername, m_sPassword, m_sHostname, m_sDatabase, m_iDBPortNum).Hash();
	}
	return m_iConnectHash;
}

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
			EndQuery ();
			auto Guard = ScopedFlags (F_IgnoreSelectKeyword);
			return ExecQuery ("show table status");
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
	case DBT::SQLSERVER15:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		return (ExecQuery ("select * from sysobjects where type = 'U' and name like '{}' order by name", sLike));

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		return SetError(kFormat ("ListTables(): {} not supported yet.", TxDBType(m_iDBType)));
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
			EndQuery ();
			auto Guard = ScopedFlags (F_IgnoreSelectKeyword);
			return ExecQuery ("show procedure status");
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// case DBT::ORACLE7:
	// case DBT::ORACLE8:
	// case DBT::ORACLE:
	// - - - - - - - - - - - - - - - - - - - - - - - -

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// case SQLSERVER:
	// case SQLSERVER15:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// return (ExecQuery ("select * from sysobjects where type = 'P' and name like '{}' order by name", sLike));

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		return SetError(kFormat( "ListProcedures(): {} not supported yet.", TxDBType(m_iDBType)));
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
			EndQuery ();
			auto Guard = ScopedFlags (F_IgnoreSelectKeyword);
			return ExecQuery ("desc {}", sTablename);
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
	case DBT::SQLSERVER15:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			auto Guard = ScopedFlags (F_IgnoreSelectKeyword);
			return ExecQuery ("sp_help {}", sTablename);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		return SetError(kFormat ("DescribeTable(): {} not supported yet.", TxDBType(m_iDBType)));
	}

} // DescribeTable

//-----------------------------------------------------------------------------
KJSON KSQL::FindColumn (KStringView sColLike, KStringView sSchemaName/*current dbname*/, KStringView sTablenameLike/*="%"*/)
//-----------------------------------------------------------------------------
{
	KString sTableSchema = (sSchemaName ? KString(sSchemaName) : GetDBName());

	kDebug (3, "{}.{}...", sTableSchema, sColLike);
	KJSON list = KJSON::array();

	switch (m_iDBType)
	{
	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		if (!ExecQuery (
			"select table_schema\n"
			"     , table_name\n"
			"     , column_name\n"
			"     , column_type\n"
			"     , column_key\n"
			"     , column_comment\n"
			"     , is_nullable\n"
			"     , column_default\n"
			"  from INFORMATION_SCHEMA.COLUMNS\n"
			" where upper(column_name) like upper('{}')\n"
			"   and table_schema not in ('information_schema','master')\n"
			"   and table_schema = '{}'\n"
			"   and table_name like '{}'\n"
			" order by table_schema, table_name desc, column_name, column_type",
				sColLike,
				sTableSchema,
				sTablenameLike))
		{
			return list; // empty
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case DBT::ORACLE7:
	case DBT::ORACLE8:
	case DBT::ORACLE:
	case DBT::SQLSERVER:
	case DBT::SQLSERVER15:
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		SetError(kFormat ("KSQL::FindColumn() not coded yet for {}", TxDBType(m_iDBType)));
		return list; // empty
	}

	KROW  row;
	while (NextRow (row))
	{
		kDebug (3, "{}: found column {} in table {}.{}", sColLike, row["column_name"], row["table_schema"], row["table_name"]);
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
			SetError ("EncodeData(): fInPlace is true, but BlobType is not ASCII");
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
			SetError ("EncodeData(): BlobType is BINARY, but iBlobDataLen not specified");
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

	SetError(kFormat ("EncodeData(): unsupported BlobType={}", iBlobType));
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
			SetError ("DecodeData(): fInPlace is true, but BlobType is not ASCII");
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
			SetError ("DecodeData(): BlobType is BINARY, but iEncodedLen not specified");
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

				SetError ("DecodeData(): corrupted blob (details in klog)");
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

	SetError(kFormat ("DecodeData(): unsupported BlobType={}", iBlobType));
	return (nullptr); // <-- failed (no new memory to free)

} // DecodeData

//-----------------------------------------------------------------------------
bool KSQL::PutBlob (KStringView sBlobTable, KStringView sBlobKey, unsigned char* sBlobData, BlobType iBlobType, uint64_t iBlobDataLen/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "...");

	if (!sBlobTable || !sBlobKey || !sBlobData)
	{
		return SetError ("PutBlob(): BlobTable, BlobKey or BlobData is nullptr");
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
				return SetError ("KSQL:PutBlob(): BlobType is BINARY, but iBlobDataLen not specified");
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

		kDebug (GetDebugLevel(), "chunk '{}', part[{:02}]: encoding={}, datasize={:04}", sBlobKey, iChunkNum, iBlobType, iDataLenChunk);

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

	kDebug (GetDebugLevel(), "expecting {} bytes for BlobKey='{}'", iBlobDataLen, sBlobKey);

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

		kDebug (GetDebugLevel(), "chunk '{}', part[{:02}]: encoding={}, datasize={:04}", sBlobKey, iChunkNum, iEncoding, iDataSize);

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
		kDebug (GetDebugLevel(), "  + iBlobDataLen  = + {:08}", iBlobDataLen);
		kDebug (GetDebugLevel(), "  --------------    ----------");
		kDebug (GetDebugLevel(), "                  = {}", dszBlobData + iBlobDataLen);
		kDebug (GetDebugLevel(), "    but sSpot   = {}", sSpot);
	}

	*sSpot = 0;                       // <-- terminate the blob (in case its ascii)

	if (piBlobDataLen)
	{
		kDebug (GetDebugLevel(), "return length set to {}", iBlobDataLen);

		*piBlobDataLen = (uint64_t) iBlobDataLen;  // <-- for truly binary data, this data length is essential
	}
	else
	{
		kDebug (GetDebugLevel(), "return length ({}) not passed back", iBlobDataLen);
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
	m_sLastSQL = Row.FormSelect (m_iDBType, bSelectAllColumns);

	if (m_sLastSQL.empty())
	{
		return SetError(Row.GetLastError());
	}

	if (!ExecLastRawQuery (Flags::F_None, "Load"))
	{
		// ExecLastRawQuery sets the errors itself
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

	auto Guard = ScopedFlags(bIgnoreDupes ? F_IgnoreSQLErrors : F_None, false);

	bool bOK = ExecLastRawSQL (Flags::F_None, "Insert");

	if (!bOK && bIgnoreDupes && WasDuplicateError())
	{
		kDebug (2, "ignoring dupe error: {}", GetLastError());
		bOK = true; // <-- override return status
	}

	kDebug (GetDebugLevel(), "{} rows affected.", m_iNumRowsAffected);

	return (bOK);

} // ExecLastRawInsert

//-----------------------------------------------------------------------------
bool KSQL::Insert (const KROW& Row, bool bIgnoreDupes/*=false*/)
//-----------------------------------------------------------------------------
{
	m_sLastSQL = Row.FormInsert(m_iDBType);

	if (m_sLastSQL.empty())
	{
		return SetError(Row.GetLastError());
	}

	return ExecLastRawInsert(bIgnoreDupes);

} // Insert

//-----------------------------------------------------------------------------
bool KSQL::Insert (const std::vector<KROW>& Rows, bool bIgnoreDupes)
//-----------------------------------------------------------------------------
{
	static constexpr std::size_t iMaxRows = 1000;

	KString     sLastTablename;
	// assumed max packet size for sql API: 64 MB (default for MySQL)
	std::size_t iMaxPacket       = 64 * 1024 * 1024;
	std::size_t iRows            = 0;
	uint64_t    iFirstColumnHash = 0;
	uint64_t    iInsertedRows    = 0;

	m_sLastSQL.clear();

	for (const auto& Row : Rows)
	{
		if (!iRows)
		{
			if (!Row.GetTablename().empty())
			{
				sLastTablename = Row.GetTablename();
			}
			else
			{
				// set the tablename from the previous list, it
				// will be used to start the value insert again
				Row.SetTablename(sLastTablename);
			}
		}
		else if (!Row.GetTablename().empty() &&
				 sLastTablename != Row.GetTablename())
		{
			// start a new bulk insert for the new table
			auto bResult = ExecLastRawInsert(bIgnoreDupes);

			iInsertedRows += GetNumRowsAffected();

			if (!bResult)
			{
				return false;
			}
			
			iRows = 0;
			iFirstColumnHash = 0;
			m_sLastSQL.clear();
			sLastTablename = Row.GetTablename();
		}

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
				m_iNumRowsAffected = iInsertedRows;
				return SetError("differing column layout in rows - abort");
			}
		}

		// do not use the "ignore" insert with SQLServer
		if (!Row.AppendInsert(m_sLastSQL, m_iDBType, false, GetDBType() != DBT::SQLSERVER && GetDBType() != DBT::SQLSERVER15))
		{
			m_iNumRowsAffected = iInsertedRows;
			return SetError(Row.GetLastError());
		}

		// compute average size per row
		auto iAverageRowSize = m_sLastSQL.size() / ++iRows;

		if (iRows >= iMaxRows ||
			m_sLastSQL.size() + 3 * iAverageRowSize > iMaxPacket)
		{
			// next row gets too close to the maximum, flush now
			if (!ExecLastRawInsert(bIgnoreDupes))
			{
				m_iNumRowsAffected = iInsertedRows + GetNumRowsAffected();
				return false;
			}

			iInsertedRows += GetNumRowsAffected();
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

	auto bResult = ExecLastRawInsert(bIgnoreDupes);

	iInsertedRows += GetNumRowsAffected();
	m_iNumRowsAffected = iInsertedRows;

	return bResult;

} // Insert

//-----------------------------------------------------------------------------
bool KSQL::Update (const KROW& Row)
//-----------------------------------------------------------------------------
{
	m_sLastSQL = Row.FormUpdate (m_iDBType);

	if (m_sLastSQL.empty())
	{
		return SetError(Row.GetLastError());
	}

	if (!IsFlag(F_NoTranslations))
	{
		DoTranslations (m_sLastSQL);
	}

	bool bOK = ExecLastRawSQL (Flags::F_None, "Update");

	kDebug (GetDebugLevel(), "{} rows affected.", m_iNumRowsAffected);

	return (bOK);

} // Update

//-----------------------------------------------------------------------------
bool KSQL::Delete (const KROW& Row)
//-----------------------------------------------------------------------------
{
	m_sLastSQL = Row.FormDelete (m_iDBType);

	if (m_sLastSQL.empty())
	{
		return SetError(Row.GetLastError());
	}

	if (!IsFlag(F_NoTranslations))
	{
		DoTranslations (m_sLastSQL);
	}

	bool bOK = ExecLastRawSQL (Flags::F_None, "Delete");

	if (!bOK)
	{
		return false;
	}

	kDebug (GetDebugLevel(), "{} rows affected.", m_iNumRowsAffected);

	return (bOK);

} // Delete

//-----------------------------------------------------------------------------
bool KSQL::PurgeKey (KStringView sSchemaName, const KROW& OtherKeys, KStringView sPKEY_colname, KStringView sValue, KJSON& ChangesMade, KStringView sIgnoreRegex/*=""*/)
//-----------------------------------------------------------------------------
{
	KROW row = OtherKeys;

	for (auto& col : row)
	{
		col.second.AddFlags(KCOL::PKEY);
	}

	if (!ChangesMade.is_array())
	{
		ChangesMade = KJSON::array();
	}
	KJSON DataDict = FindColumn (sPKEY_colname, sSchemaName);

	if (!BeginTransaction ())
	{
		return (false);
	}

	for (auto& table : DataDict)
	{
		KJSON    obj;
		KString  sTableSchema = table["table_schema"];		
		KString  sTablename   = table["table_name"];
		uint64_t iChanged{0};

		obj["table_name"] = sTablename;

		if (sIgnoreRegex && sTablename.ToUpper().MatchRegex (sIgnoreRegex.ToUpper()))
		{
			obj["ignored"] = true;
			iChanged = 0;
		}
		else
		{
			// do not escape the table name - KROW takes care of that
			row.SetTablename (kFormat ("{}.{} /*KSQL::PurgeKey*/", sTableSchema, sTablename));
			row.AddCol (sPKEY_colname, sValue, KCOL::PKEY);
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
bool KSQL::PurgeKey (KStringView sSchemaName, KStringView sPKEY_colname, KStringView sValue, KJSON& ChangesMade, KStringView sIgnoreRegex/*=""*/)
//-----------------------------------------------------------------------------
{
	ChangesMade = KJSON::array();
	KJSON DataDict = FindColumn (sPKEY_colname, sSchemaName);

	if (!BeginTransaction ())
	{
		return (false);
	}

	for (auto& table : DataDict)
	{
		KJSON    obj;
		KString  sTableSchema = table["table_schema"];
		KString  sTablename = table["table_name"];
		uint64_t iChanged{0};

//		obj["table_schema"] = sTableSchema; // It makes sense to add this to the JSON result but it then names the smoketest baselines schema name dependent
		obj["table_name"] = sTablename;		

		if (sIgnoreRegex && sTablename.ToUpper().MatchRegex (sIgnoreRegex.ToUpper()))
		{
			obj["ignored"] = true;
			iChanged = 0;
		}
		else
		{
			if (!ExecSQL ("delete from {}.{} /*KSQL::PurgeKey*/ where {} = binary '{}'", sTableSchema, sTablename, sPKEY_colname, sValue))
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
bool KSQL::PurgeKeyList (KStringView sSchemaName, KStringView sPKEY_colname, const KSQLString& sInClause, KJSON& ChangesMade, KStringView sIgnoreRegex/*=""*/, bool bDryRun/*=false*/, int64_t* piNumAffected/*=NULL*/)
//-----------------------------------------------------------------------------
{
	ChangesMade = KJSON::array();
	KJSON DataDict = FindColumn (sPKEY_colname, sSchemaName);

	if (!BeginTransaction ())
	{
		return (false);
	}

	for (auto& table : DataDict)
	{
		KJSON    obj;
		KString  sTableSchema = table["table_schema"];
		KString  sTablename = table["table_name"];
		int64_t  iChanged{0};

		obj["table_name"] = sTablename;

		if (sIgnoreRegex && sTablename.ToUpper().MatchRegex (sIgnoreRegex.ToUpper()))
		{
			obj["ignored"] = true;
			iChanged = 0;
		}
		else if (bDryRun)
		{
			iChanged = SingleIntQuery ("select count(*) from {}.{} /*KSQL::PurgeKey*/ where {} in ({})", sTableSchema, sTablename, sPKEY_colname, sInClause);
			if (iChanged < 0)
			{
				return (false);
			}
			obj["rows_selected"] = iChanged;
		}
		else if (!ExecSQL ("delete from {}.{} /*KSQL::PurgeKey*/ where {} in ({})", sTableSchema, sTablename, sPKEY_colname, sInClause))
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
bool KSQL::BulkCopy (KSQL& OtherDB, KStringView sTablename, const KSQLString& sWhereClause/*=""*/, uint16_t iFlushRows/*=1024*/, int32_t iPbarThreshold/*=500*/)
//-----------------------------------------------------------------------------
{
	KBAR    bar;
	int64_t iExpected{-1};
	bool    bPBAR = (iPbarThreshold >= 0);
	KUnixTime tStarted = KUnixTime::now();

	if (bPBAR)
	{
		KOut.Format (":: {} : {:50} : ", kFormTimestamp(tStarted ,"%a %T"), sTablename);
	}
	
	if (bPBAR)
	{
		iExpected = OtherDB.SingleIntQuery ("select count(*) from {} {}", sTablename, sWhereClause);
		if (iExpected < 0)
		{
			KOut.WriteLine (OtherDB.GetLastError());
			return SetError(kFormat ("{}: {}: {}", OtherDB.ConnectSummary(), OtherDB.GetLastError(), OtherDB.GetLastSQL()));
		}
	}

	if (bPBAR)
	{
		if (!iExpected)
		{
			KOut.WriteLine ("no rows to copy.");
		}
		else
		{
			KOut.FormatLine ("{} rows... ", kFormNumber(iExpected));
			auto iTarget = SingleIntQuery ("select count(*) from {} {}", sTablename, sWhereClause);
			if (iTarget < 0)
			{
				KOut.WriteLine (":: table does not exist in target, skipping copy.");
			}
			else if (iTarget == iExpected)
			{
				KOut.FormatLine (":: target already has {} rows, skipping copy.", kFormNumber(iExpected), sWhereClause);
			}
			else if (iTarget)
			{
				KOut.FormatLine (":: target has {} rows, issuing a delete before the copy...", kFormNumber(iExpected));
				ExecSQL ("delete from {} {}", sTablename, sWhereClause);
				KOut.FormatLine (":: purged {} rows from: {} {}", GetNumRowsAffected(), ConnectSummary(), sTablename);
			}
			else // target table is empty
			{
				KOut.WriteLine ();
			}
		}
	}

	if (!OtherDB.ExecQuery ("select * from {} {}", sTablename, sWhereClause))
	{
		return SetError(kFormat ("{}: {}: {}", OtherDB.ConnectSummary(), OtherDB.GetLastError(), OtherDB.GetLastSQL()));
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

	KDuration tTook = KUnixTime::now() - tStarted;
	if (bPBAR)
	{
		bar.Finish();
		if (tTook >= chrono::seconds(10))
		{
			KOut.FormatLine (":: {} : {:50} : took {}\n", kFormTimestamp(kNow(), "%a %T"), sTablename, kTranslateDuration(tTook));
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
	if (m_iDBType == DBT::SQLSERVER ||
		m_iDBType == DBT::SQLSERVER15)
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
	m_sCtLibLastError.clear();

	kDebug (KSQL2_CTDEBUG, "cs_ctx_alloc()");
	if (cs_ctx_alloc (CS_VERSION_100, &m_pCtContext) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>cs_ctx_alloc");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
	}

	kDebug (KSQL2_CTDEBUG, "ct_con_alloc()");
	if (ct_con_alloc (m_pCtContext, &m_pCtConnection) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>ct_con_alloc");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
	}

	kDebug (KSQL2_CTDEBUG, "ct_con_props() set username");
	auto sUser = GetDBUser();
	if (ct_con_props (m_pCtConnection, CS_SET, CS_USERNAME, sUser.data(), static_cast<CS_INT>(sUser.size()), nullptr) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>set username");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
	}

	kDebug (KSQL2_CTDEBUG, "ct_con_props() set password");
	auto sPass = GetDBPass();
	if (ct_con_props (m_pCtConnection, CS_SET, CS_PASSWORD, sPass.data(), static_cast<CS_INT>(sPass.size()), nullptr) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>set password");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
	}

	kDebug (KSQL2_CTDEBUG, "connecting as {} to {}.{}\n", GetDBUser(), GetDBHost(), GetDBName());
	int iTryConnect = 0;
	while(true)
	{
		auto sHost = GetDBHost();
		if (ct_connect (m_pCtConnection, sHost.data(), static_cast<CS_INT>(sHost.size())) == CS_SUCCEED)
		{
			break;
		}
		else if (++iTryConnect >= 3)
		{
			//try to connect 3 times.
			ctlib_api_error ("ctlib_login>ct_connect");
			return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
		}
	}

	kDebug (KSQL2_CTDEBUG, "ct_cmd_alloc()");
	if (ct_cmd_alloc (m_pCtConnection, &m_pCtCommand) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>ct_cmd_alloc");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
	}

	if (!GetDBName().empty())
	{
		if (ctlib_execsql(kFormat("use {}", GetDBName())) != CS_SUCCEED)
		{
			return SetError(kFormat("CTLIB: login ok, could not attach to database: {}", GetDBName()));
		}
	}

	kDebug (GetDebugLevel()+1, "ensuring that we can process error messages 'inline' instead of using callbacks...");

	kDebug (KSQL2_CTDEBUG, "ct_diag(CS_INIT)");
	if (ct_diag (m_pCtConnection, CS_INIT, CS_UNUSED, CS_UNUSED, nullptr) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_login>ct_init(CS_INIT) failed");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
			return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
	if (ct_command (m_pCtCommand, CS_LANG_CMD, sSQL.data(), static_cast<CS_INT>(sSQL.size()), CS_UNUSED) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_execsql>ct_command");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
	}

	kDebug (KSQL2_CTDEBUG, "calling ct_send()...");
	if (ct_send (m_pCtCommand) != CS_SUCCEED)
	{
		ctlib_api_error ("ctlib_execsql>ct_send");
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
				return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
		m_sCtLibLastError.Format ("CT-Lib: API Error: {}", sContext);
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

	m_sCtLibLastError.clear();

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

		m_iCtLibErrorNum = ClientMsg.msgnumber;

		if (ii)
		{
			m_sCtLibLastError += "\n";
		}
		m_sCtLibLastError += m_sErrorPrefix;

		m_sCtLibLastError += kFormat("CTLIB-{:05}: ", ClientMsg.msgnumber);

		int iLayer    = CS_LAYER(ClientMsg.msgnumber);
		int iOrigin   = CS_ORIGIN(ClientMsg.msgnumber);
		int iSeverity = ClientMsg.severity; //CS_SEVERITY(ClientMsg.msgnumber);

		if (iLayer)
		{
			m_sCtLibLastError += kFormat("Layer {}, ", iLayer);
		}

		if (iOrigin)
		{
			m_sCtLibLastError += kFormat("Origin {}, ", iOrigin);
		}

		if (iSeverity)
		{
			m_sCtLibLastError += kFormat("Severity {}, ", iSeverity);
		}

		m_sCtLibLastError += ClientMsg.msgstring;

		if (ClientMsg.osstringlen > 0)
		{
			m_sCtLibLastError += kFormat("OS-{:05}: {}", ClientMsg.osnumber, ClientMsg.osstring);
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

		if (!m_iCtLibErrorNum)
		{
			m_iCtLibErrorNum = ServerMsg.msgnumber;
		}
		else
		{
			kDebug(KSQL2_CTDEBUG, "error {} already set from client, server adds {}", m_iCtLibErrorNum, ServerMsg.msgnumber);
		}

		if (ii)
		{
			m_sCtLibLastError += "\n";
		}
		// this is the sole place except SetError() where we reference m_sErrorPrefix..
		// TODO replace by smart insert after '\n' in SetError()
		m_sCtLibLastError += m_sErrorPrefix;

		m_sCtLibLastError += kFormat("SQL-{:05}: ", ServerMsg.msgnumber);

		if (ServerMsg.proclen)
		{
			m_sCtLibLastError += ServerMsg.proc;
		}

		if (ServerMsg.state)
		{
			m_sCtLibLastError += kFormat("State {}, ", ServerMsg.state);
		}

		if (ServerMsg.severity)
		{
			m_sCtLibLastError += kFormat("Severity {}, ", ServerMsg.severity);
		}

		if (ServerMsg.line)
		{
			m_sCtLibLastError += kFormat("Line {}, ", ServerMsg.line);
		}

		m_sCtLibLastError += ServerMsg.text;

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

	m_sCtLibLastError.clear();
	m_iCtLibErrorNum = 0;

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
		return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
			return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
		}

		if (colinfo.status & CS_RETURN)
		{
			kDebug (GetDebugLevel(), "CS_RETURN code: column {}  -- ignored", ii); // ignored
		}

		KColInfo ColInfo;

		// TODO check if DBT::SYBASE is the right DBType
		ColInfo.SetColumnType(DBT::SYBASE, ColInfo.iKSQLDataType, std::max(colinfo.maxlength, 8000)); // <-- allocate at least the max-varchar length to avoid overflows
		ColInfo.sColName        = (colinfo.namelen) ? colinfo.name : "";

		static constexpr KCOL::Len SANITY_MAX = 50*1024;
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
			return SetError(m_sCtLibLastError, m_iCtLibErrorNum);
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
size_t KSQL::OutputQuery (KStringView sSQL, KStringView sFormat, FILE* fpout/*=stdout*/)
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

	return (OutputQuery (sSQL, iFormat, fpout));

} // OutputQuery

//-----------------------------------------------------------------------------
size_t KSQL::OutputQuery (KStringView sSQL, OutputFormat iFormat/*=FORM_ASCII*/, FILE* fpout/*=stdout*/)
//-----------------------------------------------------------------------------
{
	KSQLString sSafeSQL;
	sSafeSQL.ref() = sSQL;

	if (!ExecRawQuery (sSafeSQL, GetFlags(), "OutputQuery"))
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
					const KString& sValue(it.first); // <-- correct
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
						fprintf (fpout, "%s%-*.*s-+", (bFirst) ? "+-" : "-", iMax, iMax, kFormat("{:-^100}", "").c_str());
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
						fprintf (fpout, "%s%-*.*s-+", (bFirst) ? "+-" : "-", iMax, iMax, kFormat("{:-^100}", "").c_str());
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
					fprintf (fpout, "%s%-*.*s-+", (bFirst) ? "+-" : "-", iMax, iMax, kFormat("{:-^100}", "").c_str());
					bFirst = false;
				}
				fprintf (fpout, "\n");
				break;
		}
	}

	return (iNumRows);

} // OutputQuery

//-----------------------------------------------------------------------------
bool KSQL::BeginTransaction (KStringView sOptions/*=""*/)
//-----------------------------------------------------------------------------
{
	switch (GetDBType())
	{
		case DBT::MYSQL:
			return sOptions.empty() ? ExecSQL("start transaction") : ExecSQL("start transaction {}", sOptions);

		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
			if (!sOptions.empty()) kDebug(2, "options are not supported for SQLServer: {}", sOptions);
			return ExecSQL ("begin transaction");

		default:
			kDebug(2, "DB Type not supported: {}", TxDBType(GetDBType()));
			return false;
	}

	return false;

} // BeginTransaction

//-----------------------------------------------------------------------------
bool KSQL::CommitTransaction (KStringView sOptions/*=""*/)
//-----------------------------------------------------------------------------
{
	switch (GetDBType())
	{
		case DBT::MYSQL:
		{
			auto iSave = GetNumRowsAffected();
			bool bOK = sOptions.empty() ? ExecSQL("commit") : ExecSQL("commit {}", sOptions);
			m_iNumRowsAffected = GetNumRowsAffected() + iSave;
			return bOK;
		}

		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
		{
			if (!sOptions.empty()) kDebug(2, "options are not supported for SQLServer: {}", sOptions);
			auto iSave = GetNumRowsAffected();
			bool bOK = ExecSQL ("commit");
			m_iNumRowsAffected = GetNumRowsAffected() + iSave;
			return bOK;
		}

		default:
			kDebug(2, "DB Type not supported: {}", TxDBType(GetDBType()));
			return false;
	}

	return false;

} // CommitTransaction

//-----------------------------------------------------------------------------
bool KSQL::RollbackTransaction (KStringView sOptions/*=""*/)
//-----------------------------------------------------------------------------
{
	switch (GetDBType())
	{
		case DBT::MYSQL:
			return sOptions.empty() ? ExecSQL("rollback") : ExecSQL("rollback {}", sOptions);

		case DBT::SQLSERVER:
		case DBT::SQLSERVER15:
			if (!sOptions.empty()) kDebug(2, "options are not supported for SQLServer: {}", sOptions);
			return ExecSQL ("rollback");

		default:
			kDebug(2, "DB Type not supported: {}", TxDBType(GetDBType()));
			return false;
	}

	return false;

} // RollbackTransaction

//-----------------------------------------------------------------------------
KSQLString KSQL::FormAndClause (const KSQLString& sDbCol, KStringView sQueryParm, FAC iFlags/*=FAC::FAC_NORMAL*/, KStringView sSplitBy/*=","*/)
//-----------------------------------------------------------------------------
{
	KSQLString sClause;

	if (sQueryParm.empty())
	{
		return sClause; // empty
	}

	kDebug (3, "dbcol={}, queryparm={}, splitby={}", sDbCol, sQueryParm, sSplitBy);

	if ((iFlags == FAC_NORMAL) && sQueryParm.Contains("%"))
	{
		iFlags |= FAC_LIKE;
	}

	KSQLString sOperator;

	// special cases:
	if (sQueryParm == "!")
	{
		sClause = FormatSQL ("   and {} is not null\n", sDbCol);
		if (iFlags & FAC_SUBSELECT)
		{
			sClause += ")";
		}
		return sClause;
	}
	else if (sQueryParm.remove_prefix ('!'))
	{
		sOperator = "!=";
		sQueryParm.Trim();
	}
	else if (sQueryParm.remove_prefix ('>'))
	{
		sOperator = ">";
		sQueryParm.Trim();
	}
	else if (sQueryParm.remove_prefix ('<'))
	{
		sOperator = "<";
		sQueryParm.Trim();
	}
	else
	{
		sOperator = "=";
	}

	KString sLowerParm (sQueryParm);
	sLowerParm.MakeLower();

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// BETWEEN logic:
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	if (iFlags & FAC_BETWEEN)
	{
		auto Parts = sQueryParm.Split(sSplitBy);

		switch (Parts.size())
		{
			case 1: // single value
				if (iFlags & FAC_DECIMAL)
				{
					sClause = FormatSQL ("   and {} {} {}", sDbCol, sOperator, Parts[0].Double());
				}
				else if (iFlags & FAC_SIGNED)
				{
					sClause = FormatSQL ("   and {} {} {}", sDbCol, sOperator, Parts[0].Int64());
				}
				else if (iFlags & FAC_NUMERIC)
				{
					sClause = FormatSQL ("   and {} {} {}", sDbCol, sOperator, Parts[0].UInt64());
				}
				else
				{
					sClause = FormatSQL ("   and {} {} '{}'", sDbCol, sOperator, Parts[0]);
				}
				break;

			case 2: // two values
				if (iFlags & FAC_DECIMAL)
				{
					sClause = FormatSQL ("   and {} between {} and {}", sDbCol, Parts[0].Double(), Parts[1].Double());
				}
				else if (iFlags & FAC_SIGNED)
				{
					sClause = FormatSQL ("   and {} between {} and {}", sDbCol, Parts[0].Int64(), Parts[1].Int64());
				}
				else if (iFlags & FAC_NUMERIC)
				{
					sClause = FormatSQL ("   and {} between {} and {}", sDbCol, Parts[0].UInt64(), Parts[1].UInt64());
				}
				else
				{
					sClause = FormatSQL ("   and {} between '{}' and '{}'", sDbCol, Parts[0], Parts[1]);
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
			if (!sOne.contains ("%"))
			{
				// sOne is inserted later with string escape, no need to escape here
				sOne = kFormat("%{}%", sOne);
			}
			if (sClause)
			{
				if (iFlags & FAC_SUBSELECT)
				{
					sClause += ")"; // needs an extra close paren
				}
				sClause += "\n";
				sClause += FormatSQL ("    or {} like '{}'", sDbCol, sOne); // OR and no parens
			}
			else
			{
				sClause += FormatSQL ("   and ({} like '{}'", sDbCol, sOne); // open paren
			}
		}
		if (sClause)
		{
			sClause += ")"; // close paren for AND
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// full-text search using SQL tolower and like operator
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else if (iFlags & FAC_TEXT_CONTAINS)
	{
		sClause = FormatSQL ("   and lower({}) like '%{}%'", sDbCol, sLowerParm);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// time intervals >= 'day', 'week', 'month' and 'year'
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else if (iFlags & FAC_TIME_PERIODS)
	{
		if (!kStrIn (sLowerParm, "minute,hour,day,week,month,quarter,year"))
		{
			SetError(kFormat ("invalid time period: {}, should be one of: minute,hour,day,week,month,quarter,year", sQueryParm));
			return sClause; // empty
		}

		sClause = FormatSQL ("   and {} >= date_sub(now(), interval 1 {})", sDbCol, sLowerParm);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// single value (no comma):
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else if (!sQueryParm.contains(sSplitBy))
	{
		if (iFlags & FAC_DECIMAL)
		{
			sClause = FormatSQL ("   and {} {} {}", sDbCol, sOperator, sQueryParm.Double());
		}
		else if (iFlags & FAC_SIGNED)
		{
			sClause = FormatSQL ("   and {} {} {}", sDbCol, sOperator, sQueryParm.Int64());
		}
		else if (iFlags & FAC_NUMERIC)
		{
			sClause = FormatSQL ("   and {} {} {}", sDbCol, sOperator, sQueryParm.UInt64());
		}
		else if (iFlags & FAC_LIKE)
		{
			KString sOne (sQueryParm);
			sOne.Replace ('*','%',/*start=*/0,/*bAll=*/true); // allow * wildcards too
			if (!sOne.contains ("%"))
			{
				// sOne is inserted later with string escape, no need to escape here
				sOne = kFormat ("%{}%", sOne);
			}

			sClause = FormatSQL ("   and {} like '{}'", sDbCol, sOne);
		}
		else
		{
			sClause = FormatSQL ("   and {} {} '{}'", sDbCol, sOperator, sQueryParm);
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// comma-delimed list using IN clause
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	else
	{
		KSQLString sList;

		for (const auto& sOne : sQueryParm.Split(sSplitBy))
		{
			if (iFlags & FAC_DECIMAL)
			{
				sList += FormatSQL ("{}{}", sList ? "," : "", sOne.Double());
			}
			else if (iFlags & FAC_SIGNED)
			{
				sList += FormatSQL ("{}{}", sList ? "," : "", sOne.Int64());
			}
			else if (iFlags & FAC_NUMERIC)
			{
				sList += FormatSQL ("{}{}", sList ? "," : "", sOne.UInt64());
			}
			else
			{
				sList += FormatSQL ("{}'{}'", sList ? "," : "", sOne);
			}
		}

		sClause = FormatSQL ("   and {} in ({})", sDbCol, sList);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - -
	// finish up AND clause
	// - - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sClause.empty())
	{
		if (iFlags & FAC_SUBSELECT)
		{
			sClause += ")"; // needs an extra close paren
		}
		sClause += "\n";
	}

	kDebug (3, "clause={}", sClause);

	return sClause;

} // FormAndClause

//-----------------------------------------------------------------------------
KSQLString KSQL::FormGroupBy (uint8_t iNumCols)
//-----------------------------------------------------------------------------
{
	KSQLString sGroupBy;

	for (uint8_t ii{1}; ii <= iNumCols; ++ii)
	{
		sGroupBy.ref() += kFormat ("{}{}", sGroupBy ? "," : " group by ", ii);
	}

	if (sGroupBy)
	{
		sGroupBy.ref() += "\n";
	}

	kDebug (3, "groupby={}", sGroupBy);

	return sGroupBy;

} // FormGroupBy

//-----------------------------------------------------------------------------
KString KSQL::ToYYYYMMDD (KString/*copy*/ sDate)
//-----------------------------------------------------------------------------
{
	KString sYYYYMMDD{sDate};

	if (sDate)
	{
		sDate.ClipAt (" "); // trim off any vestiage of time
		sDate.Replace ("-"," ", /*all=*/true);
		sDate.Replace ("/"," ", /*all=*/true);
		auto parts = sDate.Split(" ");
		if ((parts.size() <= 3) && (parts.size() >= 2))
		{
			auto iPart1 = parts[0].UInt16();
			auto iPart2 = parts[1].UInt16();
			auto iPart3 = (parts.size() ==3) ? parts[2].UInt16() : 0;

			uint16_t iYear;
			uint16_t iMonth;
			uint16_t iDay;

			if (iPart1 >= 1965)
			{
				// yyyy - m - d
				iYear  = iPart1;
				iMonth = iPart2;
				iDay   = iPart3;
			}
			else
			{
				// m / d / [yyyy]   <-- if its all numbers, assume American format 
				iMonth = iPart1;
				iDay   = iPart2;
				iYear  = iPart3 ? iPart3 : kFormTimestamp(kNow(),"%Y").UInt16();
			}

			sYYYYMMDD.Format ("{:04}-{:02}-{:02}", iYear, iMonth, iDay);
		}
	}

	kDebug (1, "date={}, yyyymmdd={}", sDate, sYYYYMMDD);

	return sYYYYMMDD;

} // ToYYYYMMDD

//-----------------------------------------------------------------------------
KString KSQL::FullDay (KStringView sDate)
//-----------------------------------------------------------------------------
{
	if (!sDate)
	{
		return "";
	}

	auto sYYYYMMDD = ToYYYYMMDD (sDate);
	return kFormat ("{} 00:00:00,{} 23:59:59", sYYYYMMDD, sYYYYMMDD);

} // FullDay

//-----------------------------------------------------------------------------
bool KSQL::FormOrderBy (KStringView sCommaDelimedSort, KSQLString& sOrderBy, const KJSON& Config)
//-----------------------------------------------------------------------------
{
	if (Config.is_null())
	{
		kDebug (3, "empty config");
		return true;
	}
	else if (! Config.is_object())
	{
		return SetError(kFormat ("BUG: FormOrderBy: Config is not object of key/value pairs: {}", Config.dump('\t')));
	}

	auto ParmList = sCommaDelimedSort.Split (",");
	kDebug (3, "sort='{}' has {} parms ...", sCommaDelimedSort, ParmList.size());

#ifndef DEKAF2_WRAPPED_KJSON
	bool bResetFlag   = KLog::getInstance().ShowStackOnJsonError(false);
	// make sure the setting is automatically reset, even when throwing
	KAtScopeEnd( KLog::getInstance().ShowStackOnJsonError(bResetFlag) );
#endif

	for (KString sParm : ParmList)
	{
		bool    bDesc = sParm.contains(" desc");
		bool    bFound { false };

		if (bDesc)
		{
			sParm.Replace (" descending","");
			sParm.Replace (" descend",   "");
			sParm.Replace (" desc",      "");
		}
		else if (sParm.contains(" asc"))
		{
			sParm.Replace (" ascending", "");
			sParm.Replace (" ascend",    "");
			sParm.Replace (" asc",       "");
		}
		sParm.Replace (" ",          "");

		for (auto& it : Config.items())
		{
			try
			{
				const auto& sMatchParm = it.key();

				if (kCaselessEqual(sMatchParm, sParm))
				{
					const KString& sDbCol = it.value();
					kDebug (3, "matched sort parm: {} to: {}", sParm, sDbCol);
					sOrderBy += FormatSQL ("{} {}{}\n", !sOrderBy.empty() ? "     ," : " order by", sDbCol, bDesc ? " desc" : "");
					bFound = true;
					break; // inner for
				}
			}
			catch (const KJSON::exception& exc)
			{
				return SetError(kFormat ("BUG: FormOrderBy: Config is not object of key/value pairs: {}", Config.dump('\t')));
			}
		}

		if (!bFound)
		{
			// runtime/user error: attempt to sort by a column that is not specified in the Config:
			return SetError(kFormat ("attempt to sort by unknown column '{}'", sParm));
		}
	}

	return true;

} // FormOrderBy

//-----------------------------------------------------------------------------
bool KSQL::GetLock (KStringView sName, chrono::seconds iTimeoutSeconds)
//-----------------------------------------------------------------------------
{
	if (m_iDBType == DBT::MYSQL)
	{
		return SingleIntQuery ("select GET_LOCK('{}', {})", sName, iTimeoutSeconds.count()) >= 1;
	}

	// else fall through to table based locking

	return GetPersistentLock(sName, iTimeoutSeconds);

} // GetLock

//-----------------------------------------------------------------------------
bool KSQL::ReleaseLock (KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_iDBType == DBT::MYSQL)
	{
		return SingleIntQuery ("select RELEASE_LOCK('{}')", sName) >= 1;
	}

	// else fall through to table based locking

	return ReleasePersistentLock(sName);

} // ReleaseLock

//-----------------------------------------------------------------------------
bool KSQL::IsLocked (KStringView sName)
//-----------------------------------------------------------------------------
{
	if (m_iDBType == DBT::MYSQL)
	{
		return SingleIntQuery ("select IS_USED_LOCK('{}')", sName) >= 1;
	}

	// else fall through to table based locking

	return IsPersistentlyLocked(sName);

} // IsLocked

//-----------------------------------------------------------------------------
bool KSQL::GetPersistentLock (KStringView sName, chrono::seconds iTimeoutSeconds)
//-----------------------------------------------------------------------------
{
	auto sTablename = kFormat ("{}_LOCK", sName);

	for (;;)
	{
		auto Guard = ScopedFlags (KSQL::F_IgnoreSQLErrors);

		kDebug (2, "obtaining lock: {}", sName);
		if (ExecSQL (
			"create table {}\n"
			"(\n"
			"  created_utc  timestamp    not null,\n"
			"  geo_ip       varchar(50)  null,\n"
			"  hostname     varchar(100) null\n"
			")", sTablename))
		{
			AddTempTable (sTablename);

			KROW row(sTablename);
			row.AddCol ("created_utc", "utc_timestamp()", KCOL::EXPRESSION);
			row.AddCol ("hostname",    kGetHostname(),    KCOL::NOFLAG,    100);
			Insert(row);

			kDebug (2, "obtained lock: {}", sName);
			return true;  // the lock has been obtained
		}

		if (iTimeoutSeconds > chrono::seconds::zero())
		{
			kDebug (2, "lock failed: {}, sleeping ...", sName);
			kMilliSleep (1000);
			--iTimeoutSeconds;
		}
		else
		{
			kDebug (2, "lock failed: {}", sName);
			return false;
		}
	}

} // GetPersistentLock

//-----------------------------------------------------------------------------
bool KSQL::ReleasePersistentLock (KStringView sName)
//-----------------------------------------------------------------------------
{
	auto sTablename = kFormat ("{}_LOCK", sName);
	auto Guard      = ScopedFlags (KSQL::F_IgnoreSQLErrors);

	kDebug (2, "releasing lock: {}", sName);
	if (ExecSQL ("drop table {}", sTablename))
	{
		RemoveTempTable (sTablename);

		kDebug (2, "released lock: {}", sName);
		return true;  // the lock has been released
	}

	kDebug (2, "release lock failed: {}", sName);
	return false;

} // ReleasePersistentLock

//-----------------------------------------------------------------------------
bool KSQL::IsPersistentlyLocked (KStringView sName)
//-----------------------------------------------------------------------------
{
	auto sTablename = kFormat ("{}_LOCK", sName);
	auto Guard      = ScopedFlags (KSQL::F_IgnoreSQLErrors);

	return DescribeTable (sTablename);

} // IsPersistentlyLocked

static constexpr KStringView SCHEMA_REV = "schema_rev";

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

	constexpr chrono::seconds WAIT_FOR_SECS{5};
	auto     iSigned    = (bForce) ? 0 : GetSchemaVersion (sSchemaVersionTable);
	uint16_t iSchemaRev = (iSigned < 0) ? 0 : static_cast<uint16_t>(iSigned);
	KString  sError;

	kDebug (3, "current rev in db determined to be: {}", iSchemaRev);
	kDebug (3, "current rev that code expected is:  {}", iCurrentSchema);

	if (iSchemaRev >= iCurrentSchema)
	{
		return true; // all set
	}

	auto sLock = kFormat ("{}_{}", sSchemaVersionTable, GetDBName()).ToUpper();
	if (!GetLock (sLock, WAIT_FOR_SECS))
	{
		kWarning(kFormat("Could not acquire schema update lock within {}. Another process may be updating the schema. Abort.", WAIT_FOR_SECS)); // note that format appends a 's' to the chrono::seconds type
		return SetError(kFormat("schema updater for table {} is locked.  gave up after {}", sSchemaVersionTable, WAIT_FOR_SECS));
	}

	// query rev again after acquiring the lock
	iSigned    = (bForce) ? 0 : GetSchemaVersion (sSchemaVersionTable);
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
			kDebug (1, kFormat("{:-^100}", ""));
			kDebug (1, "attempting to apply schema version {} to db {} ...", ii, ConnectSummary());
			kDebug (1, kFormat("{:-^100}", ""));

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
				ExecSQL ("drop table if exists {}", sSchemaVersionTable);

				if (!ExecSQL ("create table {} ( {} smallint not null primary key )", sSchemaVersionTable, SCHEMA_REV))
				{
					sError= GetLastError();
					break; // for
				}
			}

			if (!ExecSQL ("update {} set {}={}", sSchemaVersionTable, SCHEMA_REV, ii))
			{
				sError= GetLastError();
				break; // for
			}

			auto iUpdated = GetNumRowsAffected();

			if (!iUpdated && !ExecSQL ("insert into {} values ({})", sSchemaVersionTable, ii))
			{
				sError= GetLastError();
				break; // for
			}
			else if (iUpdated > 1)
			{
				ExecSQL ("truncate table {}", sSchemaVersionTable);

				if (!ExecSQL ("insert into {} values ({})", sSchemaVersionTable, ii))
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
			return SetError(sError);
		}
		else
		{
			if (!CommitTransaction())
			{
				// would throw in CommitTransaction()
				return false;
			}
		}

	} // if upgrade was needed

	ReleaseLock (sLock);

	kDebug (3, "schema should be all set at version {} now", iCurrentSchema);

	return true;

} // EnsureSchema

//-----------------------------------------------------------------------------
uint16_t KSQL::GetSchemaVersion (KStringView sTablename)
//-----------------------------------------------------------------------------
{
	auto iSigned = SingleIntQuery ("select {} from {}", SCHEMA_REV, sTablename);

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
static void ApplIniAndEnvironment (const KString& sName, KProps<KString, KString, /*order-matters=*/false, /*unique-keys=*/true>& INI, KStringRef& sVariable)
//-----------------------------------------------------------------------------
{
	{
		const auto& sValue = INI.Get(sName);
		if (!sValue.empty())
		{
			kDebugLog (3, "KSQL::EnsureConnected: applying ini parm: {}={}", sName, sValue);
			sVariable = sValue;
		}
	}

	{
		const auto& sValue = kGetEnv(sName);
		if (!sValue.empty())
		{
			kDebugLog (3, "KSQL::EnsureConnected: applying env var: {}={}", sName, sValue);
			sVariable = sValue;
		}
	}

} // ApplIniAndEnvironment

//-----------------------------------------------------------------------------
bool KSQL::EnsureConnected (KStringView sIdentifierList, KString sDBCArg, const IniParms& INI, uint16_t iConnectTimeoutSecs/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebug (3, "sIdentifierList={}, sDBCArg={} ...", sIdentifierList, sDBCArg);

	m_iConnectTimeoutSecs = iConnectTimeoutSecs; // <-- save timeout in case we have to reconnect

	if (IsConnectionOpen())
	{
		kDebug (3, "already connected to: {}", ConnectSummary());
		return true; // already connected
	}

	kDebug (3, "looks like we need to connect...");

	KString sDBType {"mysql"};
	KString sDBUser;
	KString sDBPass;
	KString sDBHost {"localhost"};
	KString sDBName;
	KString sDBPort;
	bool    bReady{false};

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// first use the DBC arg that might have been passed in, this trumps all:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (sDBCArg)
	{
		if (kFileExists(sDBCArg))
		{
			kDebug (3, "using dbc={} which was passed into the KSQL constructor", sDBCArg);

			// Note: the reason why the DBC arg trumps all is because this is how the CLI is typically handled
			// if a user explicitly says "program -dbc something.dbc ..." the connection defined in that dbc file
			// should ignore anything in INI files and in the environment.
			if (!LoadConnect (sDBCArg))
			{
				return false; // error was set
			}
			bReady = true;
			kDebug (3, "all ini parms and environment vars will be ignored because of the explicit DBC specified");
		}
		else
		{
			kDebug (3, "ignoring dbc={} which was passed into the KSQL constructor, because file does not exist", sDBCArg);
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// otherwise build a db connection off INI file[s] and ENV VAR[s]:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (!bReady)
	{
		// application can pass in a LIST of identifiers for KSQL to try, e.g. "GREEN,BLUE"
		// the db connection is built left to right with the LAST setting prevailing
		// e.g.
		//               GREEN_    BLUE_          NET RESULT
		//  XXX_DBUSER   fred      sam            sam
		//  XXX_DBNAME   flubber                  flubber
		//  XXX_DBHOST             nutter         nutter

		for (auto sIdentifier : sIdentifierList.Split(","))
		{
			auto sIdentifierLC = sIdentifier.Trim().ToLower();
			auto sIdentifierUC = sIdentifier.Trim().ToUpper();

			kDebug (3, "attempting to find connection parms using: {} ...", sIdentifierUC);

			// see if there is an identifier-based DBC file in a known location:
			auto sKnown = kFormat ("/etc/{}.dbc", sIdentifierLC);
			kDebug (3, "looking for: {}", sKnown);
			if (kFileExists (sKnown))
			{
				if (!LoadConnect (sKnown))
				{
					kDebug (3, "{}", GetLastError());
					return false; // error was set
				}
				kDebug (3, "got: {}", ConnectSummary());
			}

			// look for DBC in the environment:
			auto sEnv = kFormat ("{}_DBC", sIdentifierUC);
			auto sDBC = kGetEnv (sEnv);
			if (sDBC)
			{
				kDebug (3, "looking for: {}", sDBC);
				if (kFileExists (sDBC))
				{
					if (!LoadConnect (sDBC))
					{
						return false; // error was set
					}
					kDebug (3, "got: {}", ConnectSummary());
				}
				else
				{
					kDebug (1, "ignored {}={} because file does not exist", sEnv, sDBC);
				}
			}

			// see if there is an identifier-based DBC entry in the INI file:
			auto sINI    = kFormat ("/etc/{}.ini", sIdentifierLC);
			KProps<KString, KString, /*order-matters=*/false, /*unique-keys=*/true> INI;
			kDebug (3, "looking for: {}", sINI);
			if (kFileExists (sINI))
			{
				if (!INI.Load (sINI))
				{
					return SetError(kFormat ("invalid ini file: {}", sINI));
				}
			}

			// now allow arbitrary environmental overrides, piecemeal:
			ApplIniAndEnvironment (kFormat ("{}_DBTYPE", sIdentifierUC), INI, sDBType);
			ApplIniAndEnvironment (kFormat ("{}_DBUSER", sIdentifierUC), INI, sDBUser);
			ApplIniAndEnvironment (kFormat ("{}_DBPASS", sIdentifierUC), INI, sDBPass);
			ApplIniAndEnvironment (kFormat ("{}_DBHOST", sIdentifierUC), INI, sDBHost);
			ApplIniAndEnvironment (kFormat ("{}_DBNAME", sIdentifierUC), INI, sDBName);
			ApplIniAndEnvironment (kFormat ("{}_DBPORT", sIdentifierUC), INI, sDBPort);
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// apply the "meshing" of any overrides to the current connection:
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		if (sDBType)          {    SetDBType (sDBType);         }
		if (sDBUser)          {    SetDBUser (sDBUser);         }
		if (sDBPass)          {    SetDBPass (sDBPass);         }
		if (sDBHost)          {    SetDBHost (sDBHost);         }
		if (sDBName)          {    SetDBName (sDBName);         }
		if (sDBPort.UInt16()) {  SetDBPort (sDBPort.UInt16());  }
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// double check that we have enough parms set:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (!GetDBUser() && !GetDBPass() && !GetDBName())
	{
		return SetError ("no db connection defined");
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// backdoor for QA automation
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (IsConnectionTestOnly())
	{
		KOut.FormatLine (":: {}: would have attempted a connection to:", KSQL_CONNECTION_TEST_ONLY);
		KOut.FormatLine (":: {} = {}", "DBTYPE", TxDBType(GetDBType()));
		KOut.FormatLine (":: {} = {}", "DBUSER", GetDBUser());
		KOut.FormatLine (":: {} = {} (hashed)", "DBPASS", GetDBPass().Hash());
		KOut.FormatLine (":: {} = {}", "DBHOST", GetDBHost());
		KOut.FormatLine (":: {} = {}", "DBNAME", GetDBName());
		KOut.FormatLine (":: {} = {}", "DBPORT", GetDBPort());
		auto Guard = ScopedFlags(F_IgnoreSQLErrors);
		SetError(kFormat ("{} set", KSQL_CONNECTION_TEST_ONLY));
		return false;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// now use the resulting settings to open the db connection:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	kDebug (3, "attempting to connect ...");

	if (!OpenConnection(iConnectTimeoutSecs))
	{
		return false;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// we have an open database connection
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	return true;

} // KSQL::EnsureConnected - 1

//-----------------------------------------------------------------------------b
bool KSQL::EnsureConnected (uint16_t iConnectTimeoutSecs/*=0*/)
//-----------------------------------------------------------------------------
{
	m_iConnectTimeoutSecs = iConnectTimeoutSecs; // <-- save timeout in case we have to reconnect

	if (IsConnectionOpen())
	{
		kDebug (3, "already connected to: {}", ConnectSummary());
		return true; // already connected
	}

	kDebug (3, "attempting to connect ...");

	if (!OpenConnection(iConnectTimeoutSecs))
	{
		return false;
	}

	// we have an open database connection
	return true;

} // KSQL::EnsureConnected - 2

//-----------------------------------------------------------------------------
void KSQL::ConnectionIDs::Add(ConnectionID iConnectionID)
//-----------------------------------------------------------------------------
{
	m_Connections.unique()->insert(iConnectionID);

} // ConnectionIDs::Add

//-----------------------------------------------------------------------------
bool KSQL::ConnectionIDs::Has(ConnectionID iConnectionID) const
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.shared();

	return Connections->find(iConnectionID) != Connections->end();

} // ConnectionIDs::Has

//-----------------------------------------------------------------------------
bool KSQL::ConnectionIDs::HasAndRemove(ConnectionID iConnectionID)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	auto it = Connections->find(iConnectionID);

	if (it != Connections->end())
	{
		Connections->erase(it);
		return true;
	}
	else
	{
		return false;
	}

} // ConnectionIDs::HasAndRemove

KSQL::ConnectionIDs KSQL::s_CanceledConnections;

//-----------------------------------------------------------------------------
KSQL::TimedConnectionIDs::TimedConnectionIDs()
//-----------------------------------------------------------------------------
: m_bQuit(false)
{
} // TimedConnectionIDs::ctor

//-----------------------------------------------------------------------------
KSQL::TimedConnectionIDs::~TimedConnectionIDs()
//-----------------------------------------------------------------------------
{
	m_bQuit = true;

	if (m_Watcher)
	{
		m_Watcher->join();
	}

} // TimedConnectionIDs::dtor

//-----------------------------------------------------------------------------
void KSQL::TimedConnectionIDs::Add(KSQL& Instance)
//-----------------------------------------------------------------------------
{
	Add(Instance.GetConnectionID(false), Instance.m_QueryTimeout, Instance);

} // TimedConnectionIDs::Add

//-----------------------------------------------------------------------------
void KSQL::TimedConnectionIDs::Add(ConnectionID iConnectionID, std::chrono::milliseconds Timeout, const KSQL& Instance)
//-----------------------------------------------------------------------------
{
	std::call_once(m_Once, [this]
	{
		kDebug (2, "creating watcher thread");
		m_Watcher = std::make_unique<std::thread>(&TimedConnectionIDs::Watcher, this);
	});

	Clock::time_point Expires = Clock::now() + Timeout;
	auto iServerHash = Instance.GetHash();

	// make sure we have a connector to this instance for timed kills
	if (!m_DBs.shared()->contains(iServerHash))
	{
		auto pdb = std::make_unique<KSQL>(Instance);
		kDebug(2, "adding KSQL instance for timed kills for instance {}", iServerHash);
		m_DBs.unique()->insert( { iServerHash, std::move(pdb) } );
	}

	// now insert this query and its expiration time
	kDebug(2, "adding connection {} of server {} to watch list", iConnectionID, iServerHash);
	m_Connections.unique()->insert( { iConnectionID, iServerHash, Expires } );

} // TimedConnectionIDs::Add

//-----------------------------------------------------------------------------
bool KSQL::TimedConnectionIDs::Remove(ConnectionID iConnectionID, uint64_t iServerHash)
//-----------------------------------------------------------------------------
{
	kDebug(2, "removing connection {} of server {} from watch list", iConnectionID, iServerHash);

	auto Connections = m_Connections.unique();

	auto it = Connections->find(std::make_tuple(iConnectionID, iServerHash));

	if (it == Connections->end())
	{
		return false;
	}

	Connections->erase(it);

	return true;

} // TimedConnectionIDs::Remove

//-----------------------------------------------------------------------------
bool KSQL::TimedConnectionIDs::Remove(KSQL& Instance)
//-----------------------------------------------------------------------------
{
	// GetConnectionID() must not query for the ID if invalid,
	// that simply means the connection was just canceled (and
	// the connection already removed from the watchlist)
	return Remove(Instance.GetConnectionID(false), Instance.GetHash());

} // TimedConnectionIDs::Remove

//-----------------------------------------------------------------------------
void KSQL::TimedConnectionIDs::Watcher()
//-----------------------------------------------------------------------------
{
	for (;m_bQuit == false;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		if (m_bQuit)
		{
			return;
		}

		std::vector<Row> Queries;

		{
			auto iUpper = Clock::now();

			auto Connections = m_Connections.shared();

			auto range = Connections->get<IndexByTime>().range(boost::multi_index::unbounded,
															   [&iUpper](const Clock::time_point& compare)
			{
				return (iUpper > compare);
			});

			for (; range.first != range.second; ++range.first)
			{
				Queries.push_back(std::move(*range.first));
			}
		}

		// now kill all found connections
		for (const auto& Query : Queries)
		{
			kDebug(2, "killing timed out connection {} of server {}", Query.ID, Query.iServerHash);
			auto Servers = m_DBs.unique();
			auto pdb = Servers->find(Query.iServerHash);

			if (pdb != Servers->end() && pdb->second != nullptr)
			{
				// we have a connector - make sure it does not generate
				// new timeout queries ..
				pdb->second->SetQueryTimeout(std::chrono::milliseconds(0));
				// and finally kill the connection ..
				pdb->second->KillConnection(Query.ID);
			}
			else
			{
				kDebug(2, "could not find server with hash {}", Query.iServerHash);
			}

			// and remove from watchlist if not done by the client itself
			Remove(Query.ID, Query.iServerHash);
		}
	}

} // TimedConnectionIDs::Watcher

KSQL::TimedConnectionIDs KSQL::s_TimedConnections;

//-----------------------------------------------------------------------------
KSQL::ConnectionID KSQL::GetConnectionID(bool bQueryIfUnknown)
//-----------------------------------------------------------------------------
{
	if (!m_iConnectionID && bQueryIfUnknown)
	{
		int64_t iID { 0 };

		switch (GetDBType())
		{
			case DBT::MYSQL:
				iID = SingleIntQuery("select CONNECTION_ID()");
				break;

			case DBT::SQLSERVER:
			case DBT::SQLSERVER15:
				iID = SingleIntQuery("select @@spid");
				break;

			default:
				kDebug(2, "DB Type not supported: {}", TxDBType(GetDBType()));
				return 0;
		}

		m_iConnectionID = static_cast<uint64_t>(iID);
		kDebug(2, "queried connection ID: {}", m_iConnectionID);
	}
	else
	{
		kDebug(2, "return connection ID: {}", m_iConnectionID);
	}

	return m_iConnectionID;

} // GetConnectionID

//-----------------------------------------------------------------------------
bool KSQL::KillConnection(ConnectionID iConnectionID)
//-----------------------------------------------------------------------------
{
	if (iConnectionID == 0)
	{
		kDebug(2, "missing connection ID");
		return false;
	}

	// we have to add the ID to the static list _before_ actually
	// canceling it, as otherwise it might become reconnected
	// before we added it to the list
	s_CanceledConnections.Add(iConnectionID);

	if (ExecSQL("kill {}", iConnectionID))
	{
		kDebug(2, "canceled connection ID {}", iConnectionID);
		return true;
	}
	else
	{
		kDebug(2, "could not cancel connection ID {}", iConnectionID);
		// remove from list if failed
		s_CanceledConnections.HasAndRemove(iConnectionID);
		return false;
	}

} // KillConnection

//-----------------------------------------------------------------------------
bool KSQL::KillQuery(ConnectionID iConnectionID)
//-----------------------------------------------------------------------------
{
	if (iConnectionID == 0)
	{
		kDebug(2, "missing connection ID");
		return false;
	}

	bool bRet { false };

	// we have to add the ID to the static list _before_ actually
	// canceling it, as otherwise it might become restarted
	// before we added it to the list
	s_CanceledConnections.Add(iConnectionID);

	switch (GetDBType())
	{
		case DBT::MYSQL:
			bRet = ExecSQL("kill query {}", iConnectionID);
			break;

		default:
			kDebug(2, "DB Type not supported: {}", TxDBType(GetDBType()));
			break;
	}

	if (bRet)
	{
		kDebug(2, "canceled query of connection ID {}", iConnectionID);
		return true;
	}
	else
	{
		kDebug(2, "could not cancel query of connection ID {}", iConnectionID);
		// remove from list if failed
		s_CanceledConnections.HasAndRemove(iConnectionID);
		return false;
	}

	return bRet;

} // KillQuery

//-----------------------------------------------------------------------------
KSQL::ConnectionInfo KSQL::GetConnectionInfo(ConnectionID iConnectionID)
//-----------------------------------------------------------------------------
{
	ConnectionInfo Info;
	Info.ID = iConnectionID;

	auto Guard = ScopedFlags(F_IgnoreSelectKeyword);

	if (ExecQuery("select host /* KSQL::GetConnectionInfo() */\n"
				  ", db \n"
				  ", state \n"
				  ", info_binary \n"
				  ", time \n"
				  "from INFORMATION_SCHEMA.PROCESSLIST \n"
				  "where ID = {}",
				  iConnectionID) && NextRow())
	{
		Info.sHost    = Get(1);
		Info.sDB      = Get(2);
		Info.sState   = Get(3);
		Info.sQuery   = Get(4);
		Info.iSeconds = Get(5).UInt64();
	}

	return Info;

} // GetConnectionInfo

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

		kDebug (3, "{} --> {}", sTimestamp, sNew);
	}
	else
	{
		kDebug (3, "unchanged: {}", sTimestamp);
	}

	return sNew;

} // ConvertTimestap


//-----------------------------------------------------------------------------
DbSemaphore::DbSemaphore (KSQL& db, KString sAction, bool bThrow, bool bWait, chrono::seconds iTimeout, bool bVerbose)
//-----------------------------------------------------------------------------
	: m_db       { db       }
	, m_sAction  { std::move(sAction) }
	, m_bThrow   { bThrow   }
	, m_bVerbose { bVerbose }
{
	if (!bWait)
	{
		CreateSemaphore (iTimeout);
	}

} // ctor

//-----------------------------------------------------------------------------
bool DbSemaphore::CreateSemaphore (chrono::seconds iTimeout)
//-----------------------------------------------------------------------------
{
	if (!m_bIsSet)
	{
		m_sLastError.clear ();

		if (m_bVerbose)
		{
			KOut.FormatLine (":: {:%a %T}: {} : getting lock '{}' ...", kNow(), m_db.ConnectSummary(), m_sAction);
		}

		if (! m_db.GetPersistentLock (m_sAction, iTimeout))
		{
			m_sLastError.Format ("could not create named lock '{}', already exists", m_sAction);

			if (m_bVerbose)
			{
				KOut.FormatLine (":: {:%a %T}: {} : {}.", kNow(), m_db.ConnectSummary(), m_sLastError);
			}

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

		if (m_bVerbose)
		{
			KOut.FormatLine (":: {:%a %T}: {} : releasing lock '{}' ...", kNow(), m_db.ConnectSummary(), m_sAction);
		}

		if (!m_db.ReleasePersistentLock(m_sAction))
		{
			if (m_db.IsPersistentlyLocked(m_sAction))
			{
				m_sLastError.Format ("could not release persistent lock '{}'", m_sAction);
				kDebug(1, m_sLastError);

				if (m_bThrow)
				{
					throw KException(m_sLastError);
				}
			}
			else
			{
				kDebug(2, "persistent lock was already released: '{}'", m_sAction);
				m_bIsSet = false;
			}
		}
		else
		{
			m_bIsSet = false;
		}
	}

	return !m_bIsSet;

} // ClearSemaphore

//-----------------------------------------------------------------------------
bool KSQL::ShowCounts (KStringView sRegex/*=""*/)
//-----------------------------------------------------------------------------
{
	std::vector<KString> Tables;
	uint8_t iMax = 0;

	{
		auto Guard = ScopedFlags (F_IgnoreSelectKeyword);

		if (!ExecQuery ("show tables"))
		{
			return false;
		}
	}

	while (NextRow())
	{
		auto sTablename = Get(1);
		if (!sRegex || sTablename.ToLower().MatchRegex(sRegex.ToLower()))
		{
			Tables.push_back (sTablename);
			if (sTablename.length() > iMax)
			{
				iMax = static_cast<decltype(iMax)>(sTablename.length());
			}
		}
	}

	EndQuery();

	for (auto& sTable : Tables)
	{
		KOut.Format (":: {:<{}}  ...  ", sTable, iMax);
		uint64_t iCount = SingleIntQuery ("select count(*) from {}", sTable);
		KOut.WriteLine (kFormat ("{:>15}", kFormNumber(iCount)));
	}

	return true;

} // ShowCounts

//-----------------------------------------------------------------------------
KJSON KSQL::LoadSchema (KStringView sDBName/*=""*/, KStringView sStartsWith/*=""*/, const KJSON& options/*={}*/)
//-----------------------------------------------------------------------------
{
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// tables obtained via:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// show tables like 'TEST%';
	// +------------------------------+
	// | Tables_in_ksql (TEST%) |
	// +------------------------------+
	// | testschema1_ksql             |
	// | testschema22_ksql            |
	// | testschema2_ksql             |
	// +------------------------------+
	// 3 rows in set (0.007 sec)

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// table and index info obtained via:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// show create table testschema1_ksql;
	// +------------------+--------------------------------------------------+
	// | Table            | Create Table                                     |
	// +------------------+--------------------------------------------------+
	// | testschema1_ksql | CREATE TABLE `testschema1_ksql` (
	//   `anum` int(11) NOT NULL,
	//   `astring` char(10) COLLATE utf8mb4_unicode_ci NOT NULL,
	//   `adate` timestamp NOT NULL DEFAULT current_timestamp(),
	//   `newstring` char(200) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
	//   PRIMARY KEY (`astring`),
	//   KEY `idx01` (`anum`)
	// ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci |
	// +------------------+--------------------------------------------------+
	// 1 row in set (0.000 sec)

	// result:
	// {
	// 	"tables": [
	// 		{
	// 			"columns": [
	// 				{
	// 					"anum": "int(11) NOT NULL"
	// 				},
	// 				{
	// 					"astring": "char(100) COLLATE utf8mb4_unicode_ci NOT NULL"
	// 				},
	// 				{
	// 					"adate": "timestamp NOT NULL DEFAULT current_timestamp()"
	// 				},
	// 				{
	// 					"PRIMARY KEY": "(`astring`)"
	// 				},
	// 				{
	// 					"idx01": "(`anum`)"
	// 				}
	// 			],
	// 			"tablename": "TESTSCHEMA1_KSQL",
	// 			"meta_info": {
	// 			    "engine": "...",
	//              "version": "...",
	//              "row_format": "...",
	//              "version": "...",
	//              "row_format": "...",
	//              "table_collation": "...",
	//              "create_options": "...",
	//              "max_index_length": "...",
	//              "temporary": "...",
	// 			}
	// 		}
	// 	]
	// }

	if (!sDBName)
	{
		sDBName = GetDBName();
	}

	KJSON jSchema;

	if (sDBName.empty())
	{
		SetError("no database name");
		return KJSON{};
	}

	if (!ExecSQL("use {}", sDBName))
	{
		return KJSON{};
	}

	auto Guard = ScopedFlags(F_IgnoreSelectKeyword);

	if (!ExecQuery("show tables like '{}%'", sStartsWith))
	{
		return KJSON{};
	}

	std::vector<KString> Tables;

	for (const auto& row : *this)
	{
		Tables.push_back(row.GetValue(0).ToUpper());
	}

#ifdef DEKAF2_WRAPPED_KJSON
	const bool bIgnoreCollate = options(DIFF::ignore_collate);
	const bool bShowMetaInfo  = options(DIFF::show_meta_info);
#else
	const auto bIgnoreCollate = kjson::GetBool(options, DIFF::ignore_collate);
	const auto bShowMetaInfo  = kjson::GetBool(options, DIFF::show_meta_info);
#endif

	for (const auto& sTable : Tables)
	{
		if (!ExecQuery("show create table {}", sTable))
		{
			return KJSON{};
		}

		if (!NextRow())
		{
			SetError (kFormat ("did not get a row back from: {}", GetLastSQL()));
			return KJSON{};
		}

		KString/*copy*/ sCreateTable = Get(2);

		if (bIgnoreCollate)
		{
			// remove collate from each column def, e.g. COLLATE utf8mb4_unicode_ci
			sCreateTable.ReplaceRegex ("COLLATE [a-zA-Z0-9_]* ", "", /*all=*/true);
		}

		auto CreateTableLines = sCreateTable.Split("\n", ", ");

		// create a new table info
		KJSON jTable;
		jTable["tablename"] = sTable;

		for (auto sLine : CreateTableLines)
		{
			if (sLine.starts_with('`') || sLine.remove_prefix("KEY "))
			{
				// column definition: `created_utc`
				// index definition: KEY `IDX01`

				sLine.remove_prefix(1);
				auto iPos = sLine.find('`');
				if (iPos == KStringView::npos)
				{
					SetError(kFormat("bad column definition: `{}", sLine));
				}

				KStringView sColName(sLine.data(), iPos);
				sLine.remove_prefix(iPos + 2);
				jTable["columns"].push_back(KJSON
				{
					{ sColName, sLine }
				});
			}
			else if (sLine.remove_prefix("PRIMARY KEY "))
			{
				jTable["columns"].push_back(KJSON
				{
					{ "PRIMARY KEY", sLine }
				});
			}
		}

		if (bShowMetaInfo)
		{
			if (!ExecQuery(
				"select engine\n"
				"     , version\n"
				"     , row_format\n"
				"     , table_collation\n"
				"     , create_options\n"
				"     , max_index_length\n"
				"     , temporary\n"
				"  from information_schema.TABLES\n"
				" where table_schema='{}'\n"
				"   and table_name='{}'",
					sDBName, sTable))
			{
				return KJSON{};
			}

			KROW row;
			if (!NextRow(row))
			{
				SetError (kFormat ("did not get a row back from: {}", GetLastSQL()));
				return KJSON{};
			}

			jTable["meta_info"] = row;
		}

		kDebug (3, jTable.dump(1,'\t'));

		jSchema["tables"].push_back(jTable);
	}

	return jSchema;

} // LoadSchema

//-----------------------------------------------------------------------------
size_t KSQL::DiffSchemas (const KJSON& LeftSchema,
						  const KJSON& RightSchema,
						  DIFF::Diffs& Diffs,
						  KStringRef& sSummary,
						  const KJSON& options/*={}*/)
//-----------------------------------------------------------------------------
{
	size_t  iTotalDiffs { 0 };
	sSummary.clear();        // <-- return result (verbosity)
	Diffs.clear();           // <-- return result (SQL commands)

#ifdef DEKAF2_WRAPPED_KJSON
	      bool     bShowMissingTablesWithColumns
	                             = options(DIFF::include_columns_on_missing_tables);
	      bool     bShowMetaInfo = options(DIFF::show_meta_info);
	const KString& sDiffPrefix   = options(DIFF::diff_prefix   );
	const KString& sLeftSchema   = options(DIFF::left_schema   );
	const KString& sRightSchema  = options(DIFF::right_schema  );
	const KString& sLeftPrefix   = options(DIFF::left_prefix   );
	const KString& sRightPrefix  = options(DIFF::right_prefix  );
#else
	bool bShowMissingTablesWithColumns = kjson::GetBool(options, DIFF::include_columns_on_missing_tables);
	bool       bShowMetaInfo = kjson::GetBool(options, DIFF::show_meta_info);
	const auto& sDiffPrefix  = kjson::GetStringRef(options, DIFF::diff_prefix );
	const auto& sLeftSchema  = kjson::GetStringRef(options, DIFF::left_schema );
	const auto& sRightSchema = kjson::GetStringRef(options, DIFF::right_schema);
	const auto& sLeftPrefix  = kjson::GetStringRef(options, DIFF::left_prefix );
	const auto& sRightPrefix = kjson::GetStringRef(options, DIFF::right_prefix);
#endif

	static const KJSON s_jEmpty;

	//-----------------------------------------------------------------------------
	auto FindTable = [](const KJSON& Array, const KString& sProperty, const KString& sName) -> const KJSON&
	//-----------------------------------------------------------------------------
	{
		for (const auto& jElement : Array)
		{
			if (kjson::GetStringRef(jElement,sProperty) == sName)
			{
				return jElement;
			}
		}

		return s_jEmpty;

	}; // lambda: FindTable

	//-----------------------------------------------------------------------------
	auto FindField = [](const KJSON& Array, const KString& sName) -> const KJSON&
	//-----------------------------------------------------------------------------
	{
		for (const auto& jElement : Array)
		{
			if (jElement.items().begin().key() == sName)
			{
				return jElement;
			}
		}
		
		return s_jEmpty;

	}; // lambda: FindField

	//-----------------------------------------------------------------------------
	auto PrintColumn = [&sDiffPrefix] (KStringView sPrefix, const KJSON& jColumn, KStringRef& sResult)
	//-----------------------------------------------------------------------------
	{
		if (jColumn.is_object() && !jColumn.empty())
		{
			auto it = jColumn.items().begin();
			sResult += kFormat("{}{} {} {}\n",
				sDiffPrefix,
				sPrefix,
				it.key(),
#ifdef DEKAF2_WRAPPED_KJSON
				it.value().String());
#else
				it.value().get_ref<const KString&>());
#endif
		}

	}; // lambda: PrintColumn

	//-----------------------------------------------------------------------------
	auto FormCreateAction = [this] (KStringView sTablename, const KJSON& jColumn, KSQLString& sResult)
	//-----------------------------------------------------------------------------
	{
		if (jColumn.is_object() && !jColumn.empty())
		{
			auto        it       = jColumn.items().begin();
			const auto& sName    = it.key();
#ifdef DEKAF2_WRAPPED_KJSON
			KString     sValue   = it.value().String(); sValue.Trim();
#else
			KString     sValue   = it.value().get_ref<const KString&>(); sValue.Trim();
#endif
			if (sResult.empty())
			{
				sResult =  FormatSQL ("create table {}\n(\n    {:<25} {}\n", sTablename, sName, sValue);
			}
			else
			{
				sResult += FormatSQL ("  , {:<25} {}\n", sName, sValue);
			}
		}

	}; // lambda: FormCreateAction

	//-----------------------------------------------------------------------------
	auto FormAlterAction  = [this] (KStringView sTablename, const KJSON& jColumn, KStringView sVerb) -> KSQLString
	//-----------------------------------------------------------------------------
	{
		KSQLString sResult;

		if (jColumn.is_object() && !jColumn.empty())
		{
			auto        it       = jColumn.items().begin();
			const auto& sName    = it.key();
#ifdef DEKAF2_WRAPPED_KJSON
			const auto& sValue   = it.value().String();
#else
			const auto& sValue   = it.value().get_ref<const KString&>();
#endif
			bool        bIsIndex = sValue.StartsWith("(");

			if (sName == "PRIMARY KEY")
			{
				switch (sVerb.Hash())
				{
				case "drop"_hash:
					sResult += FormatSQL ("alter table {} drop primary key", sTablename);
					break;
				case "add"_hash:
					sResult += FormatSQL ("alter table {} add primary key {}", sTablename, sValue);
					break;
				case "modify"_hash:
				default:
					sResult += FormatSQL ("alter table {} drop primary key; ", sTablename);
					sResult += FormatSQL ("alter table {} add primary key {}", sTablename, sValue);
					break;
				}
			}
			else if (bIsIndex)
			{
				switch (sVerb.Hash())
				{
				case "drop"_hash:
					sResult += FormatSQL ("drop index {} on {}", sName, sTablename);
					break;
				case "add"_hash:
				case "modify"_hash:
				default:
					sResult += FormatSQL ("drop index if exists {} on {}; ", sName, sTablename);
					sResult += FormatSQL ("create index {} on {} {}", sName, sTablename, sValue); // <-- TODO: how do I know if its UNIQUE ??
					break;
				}
			}
			else
			{
				switch (sVerb.Hash())
				{
				case "drop"_hash:
					sResult = FormatSQL ("alter table {} {} column {}", sTablename, sVerb, sName);
					break;
				case "add"_hash:
				case "modify"_hash:
				default:
					sResult = FormatSQL ("alter table {} {} column {} {}", sTablename, sVerb, sName, sValue);
					break;
				}
			}
		}

		return sResult;

	}; // lambda: FormAlterAction

	//-----------------------------------------------------------------------------
	auto DiffOneTable = [&](const KString& sTablename, const KJSON& left, const KJSON& right) -> size_t
	//-----------------------------------------------------------------------------
	{
		size_t iTableDiffs{0};

		sSummary += kFormat ("{}{}", sDiffPrefix, sTablename); // <-- no newline on purpose

		int iCreateTable { 0 };
		KSQLString sLeftCreate;
		KSQLString sRightCreate;

		if (left.is_null())
		{
			sSummary += kFormat (" <-- table is only in {}\n", sRightSchema);
			sRightCreate = FormatSQL ("drop table {}", sTablename);
			iCreateTable = 'L'; // left
			++iTableDiffs;
		}
		else if (right.is_null())
		{
			sSummary += kFormat (" <-- table is only in {}\n", sLeftSchema);
			sLeftCreate = FormatSQL ("drop table {}", sTablename);
			iCreateTable = 'R'; // right
			++iTableDiffs;
		}
		else
		{
			sSummary += "\n";
		}

		std::set<KString> VisitedColumns;

		if (left.is_array()) // i.e. table exists on left
		{
			for (const auto& oLeftColumn : left)
			{
				auto  sField       = oLeftColumn.items().begin().key();
				auto& oRightColumn = FindField(right, sField);
				VisitedColumns.insert(sField);

				switch (iCreateTable)
				{
				case 'L':
					FormCreateAction (sTablename, oRightColumn, sLeftCreate);
					break;
				case 'R':
					FormCreateAction (sTablename, oLeftColumn, sRightCreate);
					break;
				}

				// check if equal
				if (oRightColumn == oLeftColumn)
				{
					// no diff
					continue;
				}

				if (! iCreateTable)
				{
					if (oLeftColumn.is_null())
					{
						// columns/indexes on the LEFT table but not on the RIGHT table
						Diffs.emplace_back(
							kFormat ("{}.{}: column/index on the RIGHT table but not on the LEFT table: we can either drop it from the RIGHT or add it to the LEFT", sTablename, sField),
							FormAlterAction (sTablename, oRightColumn, /*sVerb=*/"add"),
							FormAlterAction (sTablename, oRightColumn, /*sVerb=*/"drop")
						);
					}
					else if (oRightColumn.is_null())
					{
						// columns/indexes on the LEFT table but not on the RIGHT table
						Diffs.emplace_back(
							kFormat ("{}.{}: column/index on the LEFT table but not on the RIGHT table: we can either drop it from the LEFT or add it to the RIGHT", sTablename, sField),
							FormAlterAction (sTablename, oLeftColumn, /*sVerb=*/"drop"),
							FormAlterAction (sTablename, oLeftColumn, /*sVerb=*/"add")
						);
					}
					else
					{
						Diffs.emplace_back(
							kFormat ("{}.{}: column/index definition differs: we can either synch LEFT to RIGHT or synch RIGHT to LEFT", sTablename, sField),
							FormAlterAction (sTablename, oRightColumn, /*sVerb=*/"modify"),
							FormAlterAction (sTablename, oLeftColumn,  /*sVerb=*/"modify")
						);
					}
					++iTableDiffs;
				}

				// verbosity:
				if (!iCreateTable || bShowMissingTablesWithColumns)
				{
					// columns differ
					PrintColumn (sLeftPrefix , oLeftColumn , sSummary);
					PrintColumn (sRightPrefix, oRightColumn, sSummary);
				}

			} // for each column in the left table

		} // if left table is not null

		if (right.is_array()) // i.e. table exists on right
		{
			for (const auto& oRightColumn : right)
			{
				auto sField = oRightColumn.items().begin().key();

				// check if already visited
				if (VisitedColumns.find(sField) != VisitedColumns.end())
				{
					continue;
				}

				++iTableDiffs;
				// columns/indexes on the RIGHT table but not on the LEFT table
				switch (iCreateTable)
				{
				case 'R':
					FormCreateAction (sTablename, oRightColumn, sLeftCreate);
					break;
				case 0:
					Diffs.emplace_back(
						kFormat ("{}.{}: columns/indexes on the RIGHT table but not on the LEFT table", sTablename, sField),
						FormAlterAction (sTablename, oRightColumn, /*sVerb=*/"add"),
						FormAlterAction (sTablename, oRightColumn, /*sVerb=*/"drop")
					);
				}

				// verbosity:
				if (!iCreateTable || bShowMissingTablesWithColumns)
				{
					PrintColumn (sRightPrefix, oRightColumn, sSummary);
				}
			}
		}

		sSummary += "\n";
		switch (iCreateTable)
		{
		case 'L':
			sLeftCreate += ")";
			Diffs.emplace_back(
				kFormat ("{}: table is only in the RIGHT schema: we can either drop it from the RIGHT or add it to the LEFT", sTablename),
				sLeftCreate,
				sRightCreate
			);
			break;
		case 'R':
			sRightCreate += ")";
			Diffs.emplace_back(
				kFormat ("{}: table is only in the LEFT schema: we can either drop it from the LEFT or add it to the RIGHT", sTablename),
				sLeftCreate,
				sRightCreate
			);
			break;
		}

		if (Diffs.empty())
		{
			sSummary.clear(); // <-- this happens when columns match but are in a different order in the two databases
		}

		return iTableDiffs;

	}; // lambda: DiffOneTable

	//-----------------------------------------------------------------------------
	auto DiffOneTableMetaInfo = [&](const KString& sTablename, const KJSON& left, const KJSON& right) -> size_t
	//-----------------------------------------------------------------------------
	{
		size_t iTableDiffs{0};

		for (auto& it : left.items())
		{
			KString sLeftKey    = it.key();
			KString sLeftValue  = it.value();
			KString sRightValue = kjson::GetString(right,sLeftKey);

			kDebug (3, "meta info key={}, left={}, right={}", sLeftKey, sLeftValue, sRightValue);

			if (sLeftValue != sRightValue)
			{
				sSummary += kFormat ("{}{} {}: {} = {}\n", sDiffPrefix, sLeftPrefix,  sTablename, sLeftKey, sLeftValue);
				sSummary += kFormat ("{}{} {}: {} = {}\n", sDiffPrefix, sRightPrefix, sTablename, sLeftKey, sRightValue);
				++iTableDiffs;
			}
		}

		return iTableDiffs;

	}; // lambda: DiffOneTableMetaInfo

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// logic starts here
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	try
	{
		KJSON      LohmannDiffs  = KJSON::diff (LeftSchema, RightSchema); // raw diff, not very useful

		sSummary.clear();

		if (! LohmannDiffs.empty())
		{
#ifdef DEKAF2_WRAPPED_KJSON
			auto LeftTableList  = LeftSchema ("tables");
			auto RightTableList = RightSchema("tables");
#else
			auto LeftTableList  = kjson::GetArray(LeftSchema,  "tables");
			auto RightTableList = kjson::GetArray(RightSchema, "tables");
#endif
			std::set<KStringView> VisitedTables;

			for (const auto& oLeftTable : LeftTableList)
			{
				const KString& sTablename = kjson::GetStringRef(oLeftTable, "tablename");

				// find the table in the right list
				auto& oRightTable = FindTable (RightTableList, "tablename", sTablename);
				VisitedTables.insert(sTablename);

				if (oLeftTable == oRightTable)
				{
					// no diff..
					continue;
				}

				// tables differ
				iTotalDiffs += DiffOneTable (sTablename, kjson::GetArray(oLeftTable, "columns"), kjson::GetArray(oRightTable, "columns"));

				if (bShowMetaInfo && ! kjson::GetObject(oRightTable, "meta_info").is_null())
				{
					iTotalDiffs += DiffOneTableMetaInfo (sTablename, kjson::GetObject(oLeftTable, "meta_info"), kjson::GetObject(oRightTable, "meta_info"));
				}
			}

			for (const auto& oRightTable : RightTableList)
			{
				const KString& sTablename = kjson::GetStringRef(oRightTable, "tablename");

				// check if we have it already visited
				if (VisitedTables.find(sTablename) != VisitedTables.end())
				{
					continue;
				}

				// table exists only in RightTableList..
				iTotalDiffs += DiffOneTable (sTablename, KJSON{}, kjson::GetArray(oRightTable, "columns"));
			}

		} // if diffs (across entire two schema)
	}
	catch (const KJSON::exception& jex)
	{
		SetError(jex.what());
	}

	return iTotalDiffs;

} // DiffSchemas

//-----------------------------------------------------------------------------
void KSQL::SetQueryTimeout(std::chrono::milliseconds Timeout, QueryType QueryType)
//-----------------------------------------------------------------------------
{
	if (Timeout.count() == 0 || QueryType == QueryType::None)
	{
		kDebug (3, "clearing per-instance query timeout settings");
		// clear timeout settings
		m_bEnableQueryTimeout = false;
		m_QueryTimeout        = std::chrono::milliseconds(0);
		m_QueryTypeForTimeout = QueryType::None;
	}
	else
	{
		kDebug (3, "setting per-instance query timeout to {}ms for query types '{}'", Timeout.count(), kJoined(SerializeQueryTypes(QueryType)));
		m_bEnableQueryTimeout = true;
		m_QueryTimeout        = Timeout;
		m_QueryTypeForTimeout = QueryType;
	}

} // SetQueryTimeout

//-----------------------------------------------------------------------------
void KSQL::ResetQueryTimeout()
//-----------------------------------------------------------------------------
{
	kDebug (3, "resetting per-instance query timeout settings to defaults");
	m_QueryTimeout        = s_QueryTimeout;
	m_QueryTypeForTimeout = s_QueryTypeForTimeout;
	// enable the query timeout if the preset static values were good
	m_bEnableQueryTimeout = m_QueryTimeout > std::chrono::milliseconds(0) && m_QueryTypeForTimeout != QueryType::None;

} // ResetQueryTimeout

//-----------------------------------------------------------------------------
void KSQL::SetDefaultQueryTimeout(std::chrono::milliseconds Timeout, QueryType QueryType)
//-----------------------------------------------------------------------------
{
	if (Timeout.count() == 0 || QueryType == QueryType::None)
	{
		kDebug (3, "clearing default query timeout settings");
		// clear timeout settings
		s_QueryTimeout        = std::chrono::milliseconds(0);
		s_QueryTypeForTimeout = QueryType::None;
	}
	else
	{
		kDebug (3, "setting default query timeout to {}ms for query types '{}'", Timeout.count(), kJoined(SerializeQueryTypes(QueryType)));
		s_QueryTimeout        = Timeout;
		s_QueryTypeForTimeout = QueryType;
	}

} // SetQueryTimeout

//-----------------------------------------------------------------------------
bool KSQL::IsConnectionTestOnly ()
//-----------------------------------------------------------------------------
{
	static bool s_sEnvIsConnectionTest = kGetEnv(KSQL_CONNECTION_TEST_ONLY).Bool();

	return s_sEnvIsConnectionTest;

} // IsConnectionTestOnly

//-----------------------------------------------------------------------------
KSQLString KSQL::EscapeType(DBT iDBType, const char* value)
//-----------------------------------------------------------------------------
{
	// const char* is special: we do not escape it if it is from the data segment
	// - however, if from dynamic memory we _do_ escape it
	if (kIsInsideDataSegment(value))
	{
		return value;
	}
	else
	{
		return KROW::EscapeChars(value, iDBType);
	}

} // EscapeType

//-----------------------------------------------------------------------------
bool KSQL::IsDynamicString(KStringView sStr, bool bThrowIfDynamic)
//-----------------------------------------------------------------------------
{
	if (!kIsInsideDataSegment(sStr.data()))
	{
		if (bThrowIfDynamic)
		{
			KException ex(kFormat("dynamic strings are not allowed: {}", sStr.LeftUTF8(50)));
			kException(ex);
			throw ex;
		}
		return true;
	}

	return false;

} // IsDynamicString

//-----------------------------------------------------------------------------
bool KSQL::IsDynamicString(const char* sAddr, bool bThrowIfDynamic)
//-----------------------------------------------------------------------------
{
	if (!kIsInsideDataSegment(sAddr))
	{
		if (bThrowIfDynamic)
		{
			KStringView sStr(sAddr);
			KException ex(kFormat("dynamic strings are not allowed: {}", sStr.LeftUTF8(50)));
			kException(ex);
			throw ex;
		}
		return true;
	}

	return false;

} // IsDynamicString

//-----------------------------------------------------------------------------
KSQLString KSQL::EscapeFromQuotedList(KStringView sList)
//-----------------------------------------------------------------------------
{
	KSQLString sResult;

	for (auto sItem : sList.Split(",", "'"))
	{
		sResult += FormatSQL("{}'{}'", (sResult.empty() ? "" : ","), sItem);
	}

	return sResult;

} // EscapeFromQuotedList

//-----------------------------------------------------------------------------
void KSQL::AddTempTable (KStringView sTablename)
//-----------------------------------------------------------------------------
{
	m_TempTables.insert (sTablename);

} // AddTempTable

//-----------------------------------------------------------------------------
void KSQL::RemoveTempTable (KStringView sTablename)
//-----------------------------------------------------------------------------
{
	m_TempTables.erase (sTablename);

} // RemoveTempTable

//-----------------------------------------------------------------------------
void KSQL::PurgeTempTables ()
//-----------------------------------------------------------------------------
{
	for (const auto& sTablename : m_TempTables)
	{
		ExecSQL ("drop table {} /*KSQL::PurgeTempTables() on {}*/", sTablename, ConnectSummary());
	}

} // PurgeTempTables

std::atomic<std::chrono::milliseconds> KSQL::s_QueryTimeout        { std::chrono::milliseconds(0) };
std::atomic<KSQL::QueryType>           KSQL::s_QueryTypeForTimeout { QueryType::None              };

KSQL::DBCCache KSQL::s_DBCCache;

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ KSQL::KSQL_CONNECTION_TEST_ONLY;
constexpr KStringViewZ KSQL::DIFF::diff_prefix;
constexpr KStringViewZ KSQL::DIFF::left_schema;
constexpr KStringViewZ KSQL::DIFF::left_prefix;
constexpr KStringViewZ KSQL::DIFF::right_schema;
constexpr KStringViewZ KSQL::DIFF::right_prefix;
constexpr KStringViewZ KSQL::DIFF::include_columns_on_missing_tables;
constexpr KStringViewZ KSQL::DIFF::ignore_collate;
constexpr KStringViewZ KSQL::DIFF::show_meta_info;
#endif

DEKAF2_NAMESPACE_END
