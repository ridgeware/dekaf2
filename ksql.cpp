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
// #define DEKAF2_HAS_ODBC    <-- conditionally defined by makefile

#include "ksql.h"   // <-- public header (should have no dependent headers other than DEKAF header)
#include "kcrashexit.h"
#include "ksystem.h"
#include "kregex.h"
#include "kstack.h"
#include "ksplit.h"
#include "kstringutils.h"
#include <string.h> // for strncpy()
#include <unistd.h> // for sleep()

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
  //nclude <mysql_com.h>      // included by mysql.h
  //nclude <mysql_version.h>  // included by mysql.h
#endif

#ifdef DEKAF2_HAS_DBLIB
  // dependent headers when building DBLIB (these are *not* part of our distribution):
  #include <config_freetds.h>  // will be taken from: ksql/src/3p-XXXXX, produced by gnu "configure" on each platform
  #include <sqlfront.h>        // dblib top level include
  #include <sqldb.h>           // dblib top level include

  int dblib_msg_handler(DBPROCESS * dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *srvname, char *procname, int line);
  int dblib_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
#endif

#ifdef CTLIBCOMPILE
  #include <config_freetds.h>  // will be taken from: ksql/src/3p-XXXXX, produced by gnu "configure" on each platform
  #include <ctpublic.h>        // CTLIB, alternative to DBLIB for Sybase and SQLServer
  #define CTDEBUG 2
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

#ifndef max
 #define max(X,Y) (((X)>(Y)) ? (X) : (Y))
#endif
#ifndef min
 #define min(X,Y) (((X)<(Y)) ? (X) : (Y))
#endif

#ifdef linux
 #define DO_NOT_HAVE_STRPTIME
#endif

typedef struct
{
	KStringView sOriginal;
	KStringView sMySQL;
	KStringView sOraclePre8;
	KStringView sOracle;
	KStringView sSybase;
	KStringView sInformix;
}
SQLTX;

SQLTX g_Translations[] = {
	// ----------------  -----------  --------------   --------------   --------------  ----------------------------
	// ORIGINAL          MYSQL        ORACLEpre8       ORACLE8.x...     SQLSERVER       INFORMIX
	// ----------------  -----------  --------------   --------------   --------------  ----------------------------
	{ "+",          "+",/*CONCAT()*/  "||",            "||",            "+",            "||"                     },
	{ "CAT",        "+",/*CONCAT()*/  "||",            "||",            "+",            "||"                     },
	{ "DATETIME",        "timestamp", "date",          "date",          "datetime",     "?datetime-col?"         }, // column type
	{ "NOW",             "now()",     "sysdate",       "sysdate",       "getdate()",    "current year to second" }, // function
	{ "MAXCHAR",         "text",      "varchar(2000)", "varchar(4000)", "text",         "char(2000)"             },
	{ "CHAR2000",        "text",      "varchar(2000)", "varchar(2000)", "text",         "char(2000)"             },
	{ "PCT",             "%",         "%",             "%",             "%",            "%"                      },
	{ "AUTO_INCREMENT",  "auto_increment", "",         "",              "identity",     ""                       }
	// ----------------  -----------  --------------   --------------   --------------  ----------------------------
};

typedef unsigned char uint8_t;

int ToINT(const uint8_t (&iSourceArray)[4])
{
	return ((iSourceArray[3]<<24) + (iSourceArray[2]<<16) + (iSourceArray[1]<<8) + iSourceArray[0]);
}

void FromINT(uint8_t (&cTargetArray)[4], int iSourceValue)
{
	// FromINT encodes a source integer into a four-byte little endian target

	const uint32_t iTargetSize = sizeof(cTargetArray);
	int iRemainingSourceBits = iSourceValue;

	for (uint32_t iTargetIndex = 0; iTargetIndex < iTargetSize; iTargetIndex++)
	{
		cTargetArray[iTargetIndex] = (static_cast<uint32_t>(iRemainingSourceBits) & 0x000000ff);
		iRemainingSourceBits >>= 8;
	}
}

static uint64_t s_ulDebugID = 0;

uint32_t KSQL::m_iDebugLevel = 1;

KString  kExpandVariable  (KStringView sName, KPROPS* pVarList=NULL);
uint32_t kExpandVariables (KString& sAnyString, KPROPS *pVarList=NULL);
void*    kmalloc          (uint32_t iNumBytes, const char* pszContext, bool fClearMemory=true);

//-----------------------------------------------------------------------------
static void*  kfree (void* dPointer, const char* sContext/*=NULL*/)
//-----------------------------------------------------------------------------
{
	typedef enum {fubar=1} EX;

	if (dPointer)
	{
		try {
			free (dPointer);
		}
		catch (EX) {
			kWarning ("kfree: would have crashed on free() of 0x{} {}", 
				dPointer,
				(sContext) ? " : "      : "",
				(sContext) ? sContext : "");
		}
	}

	return (NULL);

} // kfree

//-----------------------------------------------------------------------------
void* kmalloc (uint32_t iNumBytes, const char* pszContext, bool bClearMemory/*=TRUE*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "{}: dynamic allocation of {} bytes", pszContext, iNumBytes);

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
KString kExpandVariable (KStringView sName, KPROPS* pVarList/*=NULL*/)
//-----------------------------------------------------------------------------
{
	KString sValue;
	if (pVarList) {
		sValue = pVarList->Get(sName);
	}

	//if sName wasn't found in pVarList, then we will try to search it environment
	if (sValue.empty()) {
		sValue = kGetEnv(sName, "");
	}

	return sValue;

} // kExpandVariable

//-----------------------------------------------------------------------------
uint32_t kExpandVariables (KString& sAnyString, KPROPS *pVarList/*=NULL*/)
//-----------------------------------------------------------------------------
{
	uint32_t iNumReplacements = 0;

	// TODO:IVAN: https://techqa1.translations.com/jira/browse/ONE-2810

	// isolate variables
	// if env[] is not null, use env structure for expansion
	// otherwise use getenv("varname") for expansion
	// follow spec carefully

	KString::size_type iStartIndex = sAnyString.find("${");

	//going through all string and finding ${ combination
	while (iStartIndex != KString::npos)
	{
		//checking, whether $ sign is screened
		if (iStartIndex>0 && sAnyString.at(iStartIndex-1)=='\\')
		{
			iStartIndex = sAnyString.find("${",iStartIndex+1);
			continue;
		}

		//extracting name of variable
		KString::size_type iEndIndex = sAnyString.find("}", iStartIndex);

		if (iEndIndex == KString::npos)
		{
			kDebugLog(1, "kExpandVariables: string {} has unamtched { at position {}", sAnyString, iStartIndex);
			return 0;
		}

		KString sVariableName = sAnyString.substr(iStartIndex+2, iEndIndex-iStartIndex-2);

		KString sValue = "";

		//validating symbols
		if (!sVariableName.empty() && !KRegex::Matches(sVariableName,"^[0-9]+.*|[^a-zA-Z0-9_]+"))
		{
			kDebugLog(2, "kExpandVariables: Evaluate value '{}'", sVariableName);

			sValue = kExpandVariable (sVariableName, pVarList);

			if (!sValue.empty())
			{
				sAnyString.replace(iStartIndex,iEndIndex-iStartIndex+1, sValue);
			}

			kDebugLog(2, "kExpandVariables: value {} replaced with {}", sVariableName, sValue);
			++iNumReplacements;
		}
		iStartIndex = sAnyString.find("${", iStartIndex+1+sValue.length());
	}

	return (iNumReplacements);

} // kExpandVariables

//-----------------------------------------------------------------------------
/// Single KString implementation of KStack template for support of legacy dekaf code.
class KSTACK : public KStack <KString>
//-----------------------------------------------------------------------------
{
public:
	KStringView Get (size_t iIndex)
	{
		return (GetItem (iIndex));
	}
};

//-----------------------------------------------------------------------------
/// Wrapper around kSplit() for support of legacy dekaf code.
inline size_t kParseDelimedList (KStringView sString, KSTACK& Parts, int chDelim=',', bool bTrim=true)
//-----------------------------------------------------------------------------
{
	KString sDelim;
	sDelim.Printf ("%c", chDelim);
	if (bTrim) {
		return (kSplit (Parts, sString, sDelim));
	}
	else {
		return (kSplit (Parts, sString, sDelim, /*sTrim=*/""));
	}
}

//-----------------------------------------------------------------------------
KSQL::KSQL (uint32_t iFlags/*=0*/, int iDebugID/*=0*/, SQLTYPE iDBType/*=DBT_MYSQL*/, KStringView sUsername/*=NULL*/, KStringView sPassword/*=NULL*/, KStringView sDatabase/*=NULL*/, KStringView sHostname/*=NULL*/, uint32_t iDBPortNum/*=0*/)
//-----------------------------------------------------------------------------
{
	_init (iDebugID);

	// this tmp file is used to hold buffered results (if flag F_BufferResults is set):
	m_sTmpResultsFile.Format ("{}/ksql-{}.res", GetTempDir(), getpid()*100 + m_iDebugID);

	m_iFlags = iFlags;

	if (!sUsername.empty())
	{
		SetConnect (iDBType, sUsername, sPassword, sDatabase, sHostname, iDBPortNum);
		// already calls SetAPISet() and FormatConnectSummary()
	}

} // KSQL - constructor 1

//-----------------------------------------------------------------------------
KSQL::KSQL (KSQL& Another, int iDebugID/*=0*/)
//-----------------------------------------------------------------------------
{
	_init (iDebugID);

	// this tmp file is used to hold buffered results (if flag F_BufferResults is set):
	m_sTmpResultsFile.Format ("{}/ksql-{}.res", GetTempDir(), getpid()*100 + m_iDebugID);

	CopyConnection (&Another);

	if (Another.IsConnectionOpen()) {
		OpenConnection(); // no way to pass back status in a constructor
	}

} // KSQL - constructor 2

//-----------------------------------------------------------------------------
KSQL::KSQL (KSQL* pAnother, int iDebugID/*=0*/)
//-----------------------------------------------------------------------------
{
	_init (iDebugID);

	// this tmp file is used to hold buffered results (if flag F_BufferResults is set):
	m_sTmpResultsFile.Format ("{}/ksql-{}.res", GetTempDir(), getpid()*100 + m_iDebugID);

	CopyConnection (pAnother);

	if (pAnother->IsConnectionOpen()) {
		OpenConnection(); // no way to pass back status in a constructor
	}

} // KSQL - constructor 3

//-----------------------------------------------------------------------------
bool KSQL::CopyConnection (KSQL* pAnother)
//-----------------------------------------------------------------------------
{
	if (IsConnectionOpen()) {
		kDebugLog(1, "[{}] CopyConnection: cannot copy connection info: already connected.", m_iDebugID);
		return (false);
	}

	m_iFlags = pAnother->GetFlags();
	SetConnect (pAnother->GetDBType(), pAnother->GetDBUser(), pAnother->GetDBPass(), pAnother->GetDBName(), pAnother->GetDBHost(), pAnother->GetDBPort());

	return (true);

} // CopyConnection

//-----------------------------------------------------------------------------
void KSQL::_init (int iDebugID)
//-----------------------------------------------------------------------------
{
	m_iDebugID          = (iDebugID) ? iDebugID : ++s_ulDebugID;

	kDebugLog (3, "[{}]KSQL::KSQL()...", m_iDebugID);

	m_iFlags            = 0;
	m_iDBType           = DBT_MYSQL;
	m_iAPISet           = 0;
	m_iDBPortNum        = 0;
	m_iErrorNum         = 0;
	m_sUsername.clear();  // order: UPDH
	m_sPassword.clear();
	m_sDatabase.clear();
	m_sHostname.clear();
	m_sDBCFile.clear();
	m_sConnectSummary.clear();

	m_dMYSQL           = NULL;
	m_MYSQLRow         = NULL;
	m_dMYSQLResult     = NULL;

	m_dOCI6LoginDataArea       = NULL;
	m_sOCI6HostDataArea.clear();
	m_dOCI6ConnectionDataArea  = NULL;

	m_dOCI8EnvHandle           = NULL;
	m_dOCI8ErrorHandle         = NULL;
	m_dOCI8ServerHandle        = NULL;
	m_dOCI8ServerContext       = NULL;
	m_dOCI8Session             = NULL;
	m_dOCI8Statement           = NULL;
	m_iOCI8FirstRowStat        = 0;

	m_dColInfo                 = NULL;
	m_iOraSessionMode          = 0; /*OCI_DEFAULT*/
	m_iOraServerMode           = 0; /*OCI_LM_DEF*/

	m_pDBPROC           = NULL;  // DBLIB
	m_pCtContext        = NULL;  // CTLIB
	m_pCtConnection     = NULL;  // CTLIB
	m_pCtCommand        = NULL;  // CTLIB

	m_sCursorName.clear();
	m_iCursor           = 0;     // CTLIB

	ClearErrorPrefix();

	m_bpBufferedResults = NULL;
	m_bConnectionIsOpen = false;
	m_bFileIsOpen       = false;
	m_bQueryStarted     = false;

	m_sLastSQL = "";
	m_sTmpResultsFile.clear();

	m_iRowNum           = 0;
	m_iNumColumns       = 0;
	m_iNumRowsBuffered  = 0;
	m_iNumRowsAffected  = 0;
	m_iLastInsertID     = 0;
	m_dBufferedColArray = NULL;

	__sync_lock_test_and_set(&m_bDisableRetries, 0);

	m_iWarnIfOverNumSeconds  = 0;
	m_bpWarnIfOverNumSeconds = NULL;

    #ifdef DEKAF2_HAS_ORACLE
	m_bStatementParsed  = false;
	m_iMaxBindVars      = 0;
	m_idxBindVar        = 0;
	#endif

    #ifdef DEKAF2_HAS_ODBC
	m_Environment       = NULL;
	m_hdbc              = SQL_NULL_HSTMT;
	m_hstmt             = NULL;
	m_sConnectString.clear();
	m_sConnectOutput.clear();
	#endif

} // _init

//-----------------------------------------------------------------------------
KSQL::~KSQL ()
//-----------------------------------------------------------------------------
{
	EndQuery ();
	CloseConnection ();  // <-- this calls FreeAll()

} // destructor

//-----------------------------------------------------------------------------
void KSQL::FreeAll ()
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]FreeAll()...", m_iDebugID);
	kDebugLog (3, "[{}]  instance cleanup:", m_iDebugID);
	kDebugLog (3, "    m_bConnectionIsOpen        = {}", (m_bConnectionIsOpen) ? "true" : "false");
	kDebugLog (3, "    m_bFileIsOpen              = {}", (m_bFileIsOpen) ? "true" : "false");
	kDebugLog (3, "    m_bQueryStarted            = {}", (m_bQueryStarted) ? "true" : "false");

	kDebugLog (3, "  dynamic memory outlook:");
	kDebugLog (3, "    m_dMYSQL                   = {}{}", m_dMYSQL, (m_dMYSQL) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_MYSQLRow                 = {}{}", m_MYSQLRow, (m_MYSQLRow) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_dMYSQLResult             = {}{}", m_dMYSQLResult, (m_dMYSQLResult) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_dOCI6LoginDataArea       = {}{}", m_dOCI6LoginDataArea, (m_dOCI6LoginDataArea) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_dOCI6ConnectionDataArea  = {}{}", m_dOCI6ConnectionDataArea, (m_dOCI6ConnectionDataArea) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_dOCI8EnvHandle           = {}{}", m_dOCI8EnvHandle, (m_dOCI8EnvHandle) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_dOCI8ErrorHandle         = {}{}", m_dOCI8ErrorHandle, (m_dOCI8ErrorHandle) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_dOCI8ServerContext       = {}{}", m_dOCI8ServerContext, (m_dOCI8ServerContext) ? " (needs to be freed)" : "");
#if USE_SERVER_ATTACH
	kDebugLog (3, "    m_dOCI8ServerHandle        = {}{}", m_dOCI8ServerHandle, (m_dOCI8ServerHandle) ? " (needs to be freed)" : "");
	kDebugLog (3, "    m_dOCI8Session             = {}{}", m_dOCI8Session, (m_dOCI8Session) ? " (needs to be freed)" : "");
#endif
	kDebugLog (3, "    m_dOCI8Statement           = {}{}", m_dOCI8Statement, (m_dOCI8Statement) ? " (needs to be freed)" : "");
//	kDebugLog (3, "    m_dColInfo                 = {}{}", m_dColInfo, (m_dColInfo) ? " (needs to be freed)" : "");
//	kDebugLog (3, "    m_dBufferedColArray        = {}{}", m_dBufferedColArray, (m_dBufferedColArray) ? " (needs to be freed)" : "");

	if (m_dMYSQL)
	{
		//kfree (m_dMYSQL, "~KSQL:m_dMYSQL");  -- should have been freed by mysql_close()
		m_dMYSQL = NULL;
	}

	if (m_dOCI6LoginDataArea)
	{
		kfree (m_dOCI6LoginDataArea, "~KSQL:m_dOCI6LoginDataArea");
		m_dOCI6LoginDataArea = NULL;
	}

	if (m_dOCI6ConnectionDataArea)
	{
		kfree (m_dOCI6ConnectionDataArea, "~KSQL:m_dOCI6ConnectionDataArea");
		m_dOCI6ConnectionDataArea = NULL;
	}

#if 0
	An OCI application should perform the following three steps before it terminates: 
	1. Delete the user session by calling OCISessionEnd() for each session. 
	2. Delete access to the data source(s) by calling OCIServerDetach() for each source. 
	3. Explicitly deallocate all handles by calling OCIHandleFree() for each handle 
	4. Delete the environment handle, which deallocates all other handles associated with it. 
	Note: When a parent OCI handle is freed, any child handles associated with it are freed automatically. 
#endif

	// de-allocate these in reverse order of their allocation in OpenConnection():
#ifdef DEKAF2_HAS_ORACLE
	if (m_dOCI8Statement)
	{
		OCIHandleFree (m_dOCI8Statement,     OCI_HTYPE_STMT);
		m_dOCI8Statement = NULL;
	}
	if (m_dOCI8Session)
	{
		OCISessionEnd ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle, (OCISession*)m_dOCI8Session, m_iOraSessionMode);
		OCIHandleFree (m_dOCI8Session,       OCI_HTYPE_SESSION);
		m_dOCI8Session = NULL;
	}
	if (m_dOCI8ServerHandle)
	{
		OCIServerDetach ((OCIServer*)m_dOCI8ServerHandle, (OCIError*)m_dOCI8ErrorHandle, m_iOraServerMode);
		OCIHandleFree (m_dOCI8ServerHandle,  OCI_HTYPE_SERVER);
		m_dOCI8ServerHandle = NULL;
	}
	if (m_dOCI8ServerContext)
	{
		OCIHandleFree (m_dOCI8ServerContext, OCI_HTYPE_SVCCTX);
		m_dOCI8ServerContext = NULL;
	}
	if (m_dOCI8ErrorHandle)
	{
		OCIHandleFree (m_dOCI8ErrorHandle,   OCI_HTYPE_ERROR);
		m_dOCI8ErrorHandle = NULL;
	}
	if (m_dOCI8EnvHandle)
	{
		OCIHandleFree (m_dOCI8EnvHandle,     OCI_HTYPE_ENV);
		m_dOCI8EnvHandle = NULL;
	}

	//OCITerminate (OCI_DEFAULT); // <-- don't do it: it might affect other KSQL instances
#endif

} // FreeAll

#define NOT_IF_ALREADY_OPEN(FUNC) \
	kDebugLog (3, "[{}]KSQL::{}()...", m_iDebugID, FUNC); \
	if (IsConnectionOpen()) { \
		m_sLastError.Format ("{} cannot change database connection when it's already open", m_sErrorPrefix); \
		return (false); \
	}


//-----------------------------------------------------------------------------
bool KSQL::SetConnect (SQLTYPE iDBType, KStringView sUsername, KStringView sPassword, KStringView sDatabase, KStringView sHostname/*=NULL*/, uint32_t iDBPortNum/*=0*/)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN("SetConnect");

	m_iDBType    = iDBType;
	m_iDBPortNum = iDBPortNum;
	m_sUsername  = sUsername;
	m_sPassword  = sPassword;
	m_sDatabase  = sDatabase;
	m_sHostname  = sHostname;

	SetAPISet (0); // <-- pick the default APIs for this DBType
	FormatConnectSummary();
	kDebug(1, "{}", ConnectSummary());

	return (true);

} // SetConnect

//-----------------------------------------------------------------------------
bool KSQL::SetDBType (SQLTYPE iDBType)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBType");
	m_iDBType    = iDBType;
	SetAPISet (0); // <-- pick the default APIs for this DBType
	FormatConnectSummary();
	return (true);

} // SetDBType

//-----------------------------------------------------------------------------
bool KSQL::SetDBType (KStringView sDBType)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBType");

	if (sDBType == "oracle")
	{
		return (SetDBType (DBT_ORACLE));
	}
	else if (sDBType == "mysql")
	{
		return (SetDBType (DBT_MYSQL));
	}
	else if (sDBType == "sqlserver")
	{
		return (SetDBType (DBT_SQLSERVER));
	}
	else if (sDBType == "sybase")
	{
		return (SetDBType (DBT_SYBASE));
	}
	else
	{
		m_sLastError.Format ("unsupported dbtype: {}", sDBType);
		return (false);
	}

} // SetDBType

//-----------------------------------------------------------------------------
bool KSQL::SetDBUser (KStringView sUsername)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBUser");
	m_sUsername = sUsername;
	FormatConnectSummary();
	return (true);

} // SetDBUser

//-----------------------------------------------------------------------------
bool KSQL::SetDBPass (KStringView sPassword)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBPass");
	m_sPassword = sPassword;
	FormatConnectSummary();
	return (true);

} // SetDBPass

//-----------------------------------------------------------------------------
bool KSQL::SetDBHost (KStringView sHostname)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBHost");
	m_sHostname = sHostname;
	FormatConnectSummary();
	return (true);

} // SetDBHost

//-----------------------------------------------------------------------------
bool KSQL::SetDBName (KStringView sDatabase)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBName");
	m_sDatabase = sDatabase;
	FormatConnectSummary();
	return (true);

} // SetDBName

//-----------------------------------------------------------------------------
bool KSQL::SetDBPort (int iDBPortNum)
//-----------------------------------------------------------------------------
{
	NOT_IF_ALREADY_OPEN ("SetDBPort");
	m_iDBPortNum = static_cast<uint32_t>(iDBPortNum);
	FormatConnectSummary();
	return (true);

} // SetDBPort

//-----------------------------------------------------------------------------
unsigned char* KSQL::decrypt (unsigned char* string) const
//-----------------------------------------------------------------------------
{
	size_t iLen = strlen((const char*)string);
	for (size_t ii=0; ii < iLen; ++ii) {
		string[ii] -= 127;
	}

	return (string);

} // decrypt

//-----------------------------------------------------------------------------
unsigned char* KSQL::encrypt (unsigned char* string) const
//-----------------------------------------------------------------------------
{
	size_t iLen = strlen((const char*)string);
	for (size_t ii=0; ii < iLen; ++ii) {
		string[ii] += 127;
	}

	return (string);

} // encrypt

//-----------------------------------------------------------------------------
bool KSQL::EncodeDBCData(DBCFILEv2& dbc)
//-----------------------------------------------------------------------------
{
	bool bResultOK = true;

	// initialize with random values to make it harder to crack:
	for (uint32_t iDbcDataIndex = 0; iDbcDataIndex < sizeof(dbc); ++iDbcDataIndex)
	{
		((char* )&dbc)[iDbcDataIndex] = 127 + (rand() % 127);
	}

	kstrncpy (dbc.szLeader, "KSQLDBC2", sizeof(dbc.szLeader));

	FromINT(dbc.iDBType, m_iDBType);
	FromINT(dbc.iAPISet, m_iAPISet);

	kstrncpy ((char* )dbc.szUsername, m_sUsername.c_str(), sizeof(dbc.szUsername)); // order: UPDH
	kstrncpy ((char* )dbc.szPassword, m_sPassword.c_str(), sizeof(dbc.szPassword));
	kstrncpy ((char* )dbc.szDatabase, m_sDatabase.c_str(), sizeof(dbc.szDatabase));
	kstrncpy ((char* )dbc.szHostname, m_sHostname.c_str(), sizeof(dbc.szHostname));
	FromINT(dbc.iDBPortNum, m_iDBPortNum);

	// crude encryption:
	encrypt (dbc.szHostname);
	encrypt (dbc.szUsername);
	encrypt (dbc.szPassword);
	encrypt (dbc.szDatabase);

	return bResultOK;

} // KSQL::EncodeDBCData (DBCFilev2)

//-----------------------------------------------------------------------------
bool KSQL::EncodeDBCData(DBCFILEv3& dbc)
//-----------------------------------------------------------------------------
{
	bool bResultOK = true;

	// initialize with random values to make it harder to crack:
	for (uint32_t iDbcDataIndex = 0; iDbcDataIndex < sizeof(dbc); ++iDbcDataIndex)
	{
		((char* )&dbc)[iDbcDataIndex] = 127 + (rand() % 127);
	}

	kstrncpy (dbc.szLeader, "KSQLDBC3", sizeof(dbc.szLeader));

	FromINT(dbc.iDBType, m_iDBType);
	FromINT(dbc.iAPISet, m_iAPISet);

	kstrncpy ((char* )dbc.szUsername, m_sUsername.c_str(), sizeof(dbc.szUsername)); // order: UPDH
	kstrncpy ((char* )dbc.szPassword, m_sPassword.c_str(), sizeof(dbc.szPassword));
	kstrncpy ((char* )dbc.szDatabase, m_sDatabase.c_str(), sizeof(dbc.szDatabase));
	kstrncpy ((char* )dbc.szHostname, m_sHostname.c_str(), sizeof(dbc.szHostname));
	FromINT(dbc.iDBPortNum, m_iDBPortNum);

	// crude encryption:
	encrypt (dbc.szHostname);
	encrypt (dbc.szUsername);
	encrypt (dbc.szPassword);
	encrypt (dbc.szDatabase);

	return bResultOK;

} // KSQL::EncodeDBCData (DBCFilev3)

//-----------------------------------------------------------------------------
bool KSQL::SaveConnect (KString sDBCFile)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]KSQL::SaveConnect()...", m_iDebugID);
	
	DBCFILEv2 dbc_v2;
	DBCFILEv3 dbc_v3;
	void*     pDbcStruct = 0;
	int       iDbcStructSize = 0;

	if (EncodeDBCData(dbc_v2))
	{
		// Store DBC as a version 2 file if we can
		pDbcStruct = (void*)&dbc_v2;
		iDbcStructSize = sizeof(dbc_v2);
	}
	else if (EncodeDBCData(dbc_v3))
	{
		// Use version 3 DBC if version 2 won't work
		pDbcStruct = (void*)&dbc_v3;
		iDbcStructSize = sizeof(dbc_v3);
	}
	else
	{
		return false;
	}

	kRemoveFile (sDBCFile);
	int fd = open(sDBCFile.c_str(), O_WRONLY|O_CREAT, 0644);

	if (fd == -1)
	{
		m_sLastError.Format ("{}SaveConnect(): could not write to '{}' ({}).", m_sErrorPrefix, sDBCFile, strerror(errno));
		return (false);
	}

	if (write (fd, pDbcStruct, iDbcStructSize) != iDbcStructSize)
	{
		m_sLastError.Format ("{}SaveConnect(): could not write all {} bytes to '{}'.", m_sErrorPrefix, iDbcStructSize, sDBCFile);
		close (fd);
		return (false);
	}

	close (fd);

	return (true);

} // SaveConnect

//-----------------------------------------------------------------------------
bool KSQL::DecodeDBCData (unsigned char* sBuffer, long iNumRead, KStringView sDBCFile)
//-----------------------------------------------------------------------------
{
	sBuffer[8] = 0;
	if (KASCII::strmatch ((char* )sBuffer, "KSQLDBC1"))
	{
		#ifdef WIN32
		m_sLastError.Format ("{}DecodeDBCData(): old format (DBC1) doesn't work on win32", m_sErrorPrefix);
		kDebugLog(GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
		return (false);
		#else

		kDebugLog((GetDebugLevel() + 1), "[{}] KSQL:DecodeDBCData(): old format (1)", m_iDebugID);
		
		if (iNumRead != sizeof(DBCFILEv1))
		{
			m_sLastError.Format ("{}DecodeDBCData(): corrupted DBC ({}) file '{}'.", m_sErrorPrefix, "V1", sDBCFile);
			kDebugLog(GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
			return (false);
		}

		DBCFILEv1 dbc;
		memcpy (&dbc, sBuffer, sizeof(DBCFILEv1));

		m_iDBType    = static_cast<SQLTYPE>(dbc.iDBType);
		m_iAPISet    = dbc.iAPISet;
		m_iDBPortNum = static_cast<uint32_t>(dbc.iDBPortNum);

		// crude decryption:
		decrypt (dbc.szHostname);
		decrypt (dbc.szUsername);
		decrypt (dbc.szPassword);
		decrypt (dbc.szDatabase);

		m_sUsername = (const char*) dbc.szUsername;
		m_sPassword = (const char*) dbc.szPassword;
		m_sDatabase = (const char*) dbc.szDatabase;
		m_sHostname = (const char*) dbc.szHostname;
		#endif
	}
	else if (KASCII::strmatch ((char* )sBuffer, "KSQLDBC2"))
	{
		kDebugLog((GetDebugLevel() + 1), "[{}] KSQL:DecodeDBCData(): compact format (2)", m_iDebugID);
		
		if (iNumRead != sizeof(DBCFILEv2))
		{
			m_sLastError.Format ("{}DecodeDBCData(): corrupted DBC ({}) file '{}'.", m_sErrorPrefix, "V2", sDBCFile);
			kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
			return (false);
		}

		DBCFILEv2 dbc;
		memcpy (&dbc, sBuffer, sizeof(DBCFILEv2));

		m_iDBType    = static_cast<SQLTYPE>(ToINT(dbc.iDBType));
		m_iAPISet    = ToINT(dbc.iAPISet);
		m_iDBPortNum = ToINT(dbc.iDBPortNum);

		// crude decryption:
		decrypt (dbc.szHostname);
		decrypt (dbc.szUsername);
		decrypt (dbc.szPassword);
		decrypt (dbc.szDatabase);

		m_sUsername = (const char*) dbc.szUsername;
		m_sPassword = (const char*) dbc.szPassword;
		m_sDatabase = (const char*) dbc.szDatabase;
		m_sHostname = (const char*) dbc.szHostname;
	}
	else if (KASCII::strmatch ((char* )sBuffer, "KSQLDBC3"))
	{
		bool bInvalid = false;

		kDebugLog((GetDebugLevel() + 1), "[{}] KSQL:DecodeDBCData(): current format (3)", m_iDebugID);
		
		if (iNumRead != sizeof(DBCFILEv3))
		{
			bInvalid = true;
		}

		if (!bInvalid)
		{
			DBCFILEv3 dbc;
			memcpy(&dbc, sBuffer, sizeof(dbc));

			m_iDBType    = static_cast<SQLTYPE>(ToINT(dbc.iDBType));
			m_iAPISet    = ToINT(dbc.iAPISet);
			m_iDBPortNum = ToINT(dbc.iDBPortNum);

			// Verify that all name fields include a null terminator
			if ((strnlen((const char*)dbc.szUsername, sizeof(dbc.szUsername)) >= sizeof(dbc.szUsername)) ||
				(strnlen((const char*)dbc.szPassword, sizeof(dbc.szPassword)) >= sizeof(dbc.szPassword)) ||
				(strnlen((const char*)dbc.szDatabase, sizeof(dbc.szDatabase)) >= sizeof(dbc.szDatabase)) ||
				(strnlen((const char*)dbc.szHostname, sizeof(dbc.szHostname)) >= sizeof(dbc.szHostname)))
			{
				bInvalid = true;
			}

			if (!bInvalid)
			{
				// crude decryption:
				decrypt (dbc.szHostname);
				decrypt (dbc.szUsername);
				decrypt (dbc.szPassword);
				decrypt (dbc.szDatabase);

				m_sUsername = (const char*) dbc.szUsername;
				m_sPassword = (const char*) dbc.szPassword;
				m_sDatabase = (const char*) dbc.szDatabase;
				m_sHostname = (const char*) dbc.szHostname;
			}
		}

		if (bInvalid)
		{
			m_sLastError.Format ("{}DecodeDBCData(): corrupted DBC ({}) file '{}'.", m_sErrorPrefix, "V3", sDBCFile);
			kDebugLog(GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
			return (false);
		}
	}
	else if (strncmp((const char*)sBuffer, (const char*)"KSQLDBC", strlen((const char*)"KSQLDBC")) == 0)
	{
		/* It's an unrecognized header but it follows the same pattern as the others:
		   This version of the software can't process the data, but it may be a future version of DBC,
		   so provide a helpful error message.
		*/
		m_sLastError.Format("{}DecodeDBCData(): unrecognized DBC version in DBC file '{}'.", m_sErrorPrefix, sDBCFile);
		kDebugLog(GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
		return false;
	}
	else
	{
		m_sLastError.Format ("{}DecodeDBCData(): invalid header on DBC file '{}'.", m_sErrorPrefix, sDBCFile);
		kDebugLog(GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
		return (false);
	}
	return true;

} // DecodeDBCData

//-----------------------------------------------------------------------------
bool KSQL::LoadConnect (KString sDBCFile)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]KSQL::LoadConnect()...", m_iDebugID);

	m_iDBType           = DBT_NONE;
	m_iAPISet           = 0;
	m_iDBPortNum        = 0;
	m_sUsername.clear();
	m_sPassword.clear();
	m_sDatabase.clear();
	m_sHostname.clear();
	m_sDBCFile.clear();

	if (IsConnectionOpen())
	{
		m_sLastError.Format ("{}LoadConnect(): can't call LoadConnect on an OPEN DATABASE.", m_sErrorPrefix);
		kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
		return (false);
	}

	// MAX must be larger than our largest DBCFILE format, so we can detect if the DBC file is too large.
	enum {MAX = (sizeof(DBCFILEv3) + 1)};
	unsigned char   szBuf[MAX+1];  memset (szBuf, 0, MAX+1);

	kDebugLog (GetDebugLevel(), "[{}] KSQL:LoadConnect(): opening '{}'...", m_iDebugID, sDBCFile);

	int fd = open (sDBCFile.c_str(), O_RDONLY);

	if (fd == -1)
	{
		m_sLastError.Format ("{}LoadConnect(): could not read from '{}'.", m_sErrorPrefix, sDBCFile);
		kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
		return (false);
	}

	long iNumRead     = read (fd, (void*)szBuf, MAX);

	close (fd);

	if (!iNumRead)
	{
		m_sLastError.Format ("{}LoadConnect(): empty DBC file '{}'.", m_sErrorPrefix, sDBCFile);
		kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
		return (false);
	}
	if (!DecodeDBCData(szBuf, iNumRead, sDBCFile))
	{
		return false;
	}

	FormatConnectSummary();

	m_sDBCFile = sDBCFile;

	return (true);

} // LoadConnect

//-----------------------------------------------------------------------------
bool KSQL::OpenConnection (KStringView sListOfHosts, KStringView sDelimeter/* = ","*/)
//-----------------------------------------------------------------------------
{
	KStack <KString> stackOfHosts;
	kSplit (stackOfHosts, sListOfHosts, sDelimeter);

	for (size_t ii=0; ii < stackOfHosts.size(); ++ii)
	{
		KStringView sDBHost = stackOfHosts.GetItem(ii);
		SetDBHost (sDBHost);

		if (OpenConnection()) {
			kDebugLog (2,"DbEnsureConnected: host {} is up", sDBHost);
			return true;
		}
		else
		{
			kDebugLog (2,"DbEnsureConnected: host {} is down", sDBHost);
		}
	}

	return false;

} // KSQL::OpenConnection

//-----------------------------------------------------------------------------
void KSQL::DisableRetries()
//-----------------------------------------------------------------------------
{
	__sync_lock_test_and_set(&m_bDisableRetries, 1);

} // KSQL::DisableRetries

//-----------------------------------------------------------------------------
void KSQL::EnableRetries()
//-----------------------------------------------------------------------------
{
	__sync_lock_test_and_set(&m_bDisableRetries, 0);

} // KSQL::DisableRetries

//-----------------------------------------------------------------------------
bool KSQL::OpenConnection ()
//-----------------------------------------------------------------------------
{
    #ifdef DEKAF2_HAS_ORACLE
	static bool s_fOCI8Initialized = false;
	#endif

	kDebugLog (3, "[{}]bool KSQL::OpenConnection()...", m_iDebugID);

	if (m_bConnectionIsOpen)
		return (true);

	m_sLastError = "";
	if (!m_iAPISet)
		SetAPISet (0); // <-- pick the default APIs for this DBType

	FormatConnectSummary ();

	KStringView dbt = TxDBType(m_iDBType);
	KStringView api = TxAPISet(m_iAPISet);

	kDebugLog (GetDebugLevel() + 1, "[{}]connect info:", m_iDebugID);
	kDebugLog (GetDebugLevel() + 1, "  Summary  = {}", m_sConnectSummary);
	kDebugLog (GetDebugLevel() + 1, "  DBType   = {} ( {} )", m_iDBType, dbt);
	kDebugLog (GetDebugLevel() + 1, "  APISet   = {} ( {} )", m_iAPISet, api);
	kDebugLog (GetDebugLevel() + 1, "  DBUser   = {}", m_sUsername);
	kDebugLog (GetDebugLevel() + 1, "  DBHost   = {}", m_sHostname);
	kDebugLog (GetDebugLevel() + 1, "  DBPort   = {}", m_iDBPortNum);
	kDebugLog (GetDebugLevel() + 1, "  DBName   = {}", m_sDatabase);
	kDebugLog (GetDebugLevel() + 1, "  Flags    = {} ( {}{}{}{})", 
		m_iFlags, 
		IsFlag(F_IgnoreSQLErrors)  ? "IgnoreSQLErrors "  : "",
		IsFlag(F_BufferResults)    ? "BufferResults "    : "",
		IsFlag(F_NoAutoCommit)     ? "NoAutoCommit "     : "",
		IsFlag(F_NoTranslations)   ? "NoTranslations "   : "");

	kDebugLog (GetDebugLevel(), "[{}] connecting to {}...", m_iDebugID, m_sConnectSummary);

    #ifdef DEKAF2_HAS_ORACLE
	char*  sOraHome = kGetEnv("ORACLE_HOME","");
	#endif

    #ifdef DEKAF2_HAS_ODBC
	SWORD nResult;
	#endif

    #ifdef DEKAF2_HAS_DBLIB
	LOGINREC* pLogin = NULL;
	#endif
	
    #ifdef DEKAF2_HAS_MYSQL
	int iPortNum = 0;
	iPortNum = GetDBPort();
	#endif

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case 0:
	// - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}CANNOT CONNECT (no connect info!)", m_sErrorPrefix);
		return (SQLError ());

    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - -
	case API_MYSQL:
	// - - - - - - - - - - - - - - - - -
		if (m_sHostname.empty())
		{
			m_sHostname = "localhost";
		}

		//kDebugLog (GetDebugLevel(), "[{}]: connecting to mysql, Username='{}', Hostname='{}', Database='{}'...",
		//	m_iDebugID, m_sUsername, m_sHostname, m_sDatabase);

		//m_dMYSQL = (MYSQL*) kmalloc (sizeof(MYSQL),"KSQL:m_dMYSQL"); // <-- will gracefully crash on malloc failure
		//mysql_init ((MYSQL*)m_dMYSQL);

		static std::mutex s_OnceInitMutex;
		static bool s_fOnceInitFlag = false;
		if (!s_fOnceInitFlag)
		{
			std::lock_guard<std::mutex> Lock(s_OnceInitMutex);
			if (!s_fOnceInitFlag)
			{
				kDebugLog (3, "mysql_library_init()...");
				mysql_library_init(0, NULL, NULL);
				s_fOnceInitFlag = true;
			}
		}

		kDebugLog (3, "mysql_init()...");
		m_dMYSQL = mysql_init (NULL);

		kDebugLog (3, "mysql_real_connect()...");

		if (!mysql_real_connect ((MYSQL*)m_dMYSQL, m_sHostname.c_str(), m_sUsername.c_str(), m_sPassword.c_str(), m_sDatabase.c_str(), /*port*/ iPortNum, /*sock*/NULL,
			/*flag*/CLIENT_FOUND_ROWS)) // <-- this flag corrects the behavior of GetNumRowsAffected()
		{
			m_iErrorNum = mysql_errno ((MYSQL*)m_dMYSQL);
			m_sLastError.Format ("{}MSQL-{}: {}", m_sErrorPrefix, GetLastErrorNum(), mysql_error((MYSQL*)m_dMYSQL));
			if (m_dMYSQL)
			{
				mysql_close((MYSQL*) m_dMYSQL);
			}
			return (SQLError ());
		}
		break;
	#endif

    #ifdef DEKAF2_HAS_ODBC
	// - - - - - - - - - - - - - - - - -
	case API_DEKAF2_HAS_ODBC:
	// - - - - - - - - - - - - - - - - -
		// DEKAF2_HAS_ODBC initialization:
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

		snprintf ((char* )m_sConnectString, MAX_DEKAF2_HAS_ODBCSTR, "DSN={};UID={};PWD={}", m_sDatabase, m_sUsername, m_sPassword);
		if (!m_sHostname.empty()) {
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

		if (!CheckDEKAF2_HAS_ODBC (SQLDriverConnect ( // level 1 api
			m_hdbc,
			NULL,    // hwnd
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
			kDebugLog (2, "KSQL(warning): connection string was tailored by odbc driver.");
			kDebugLog (2, "KSQL(warning): orig connect string: '{}'\n", m_sConnectString);
			kDebugLog (2, "KSQL(warning):  new connect string: '{}'\n", m_sConnectOutput);
		}
		break;
	#endif

    #ifdef DEKAF2_HAS_ORACLE
	// - - - - - - - - - - - - - - - - -
	case API_OCI8:
	// - - - - - - - - - - - - - - - - -
		kDebugLog (2, "[{}] ORACLE_HOME='{}'", m_iDebugID, sOraHome);
		if (!*sOraHome)
			kDebugLog (2, "KSQL::OpenConnection(): $ORACLE_HOME not set");

		if (!s_fOCI8Initialized)
		{
			// see this excellent write up on Shared Data Mode:
			//   http://www.oradoc.com/ora816/appdev.816/a76975/oci02bas.htm#425685
			// This sounds perfect for DEKAF applications....
			OCIInitialize (OCI_DEFAULT, // OCI_SHARED | OCI_THREADED,
				NULL, 
				NULL,  // extern "C" (dvoid * (*)(dvoid *, size_t))          NULL,
				NULL,  // extern "C" (dvoid * (*)(dvoid *, dvoid *, size_t)) NULL,
				NULL); // extern "C" (void (*)(dvoid *, dvoid *))            NULL);
			s_fOCI8Initialized = true;
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// OCI8 Initialization sequence, extracted from cdemo81.c:
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		OCIEnvInit      ((OCIEnv **)&m_dOCI8EnvHandle, OCI_DEFAULT, 0, NULL);
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8ErrorHandle,   OCI_HTYPE_ERROR,   0, NULL);
	#if USE_SERVER_ATTACH
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8ServerContext, OCI_HTYPE_SVCCTX,  0, NULL);
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8ServerHandle,  OCI_HTYPE_SERVER,  0, NULL);
		OCIServerAttach ((OCIServer*)m_dOCI8ServerHandle, (OCIError*)m_dOCI8ErrorHandle, (text *)"", strlen(""), m_iOraServerMode);

		/* set attribute server context in the service context */
		OCIAttrSet (m_dOCI8ServerContext, OCI_HTYPE_SVCCTX, m_dOCI8ServerHandle,
		            0, OCI_ATTR_SERVER, (OCIError *) m_dOCI8ErrorHandle);

		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8Session,       OCI_HTYPE_SESSION, 0, NULL);

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
			if (IsFlag(F_IgnoreSQLErrors)) {
				kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
			}
			else {
				kWarning ("[{}] {}", m_iDebugID, m_sLastError);
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
			return (SQLError());
	#endif

		// now allocate the one and only one statement (1 per KSQL instance):
		OCIHandleAlloc  (m_dOCI8EnvHandle,    &m_dOCI8Statement,     OCI_HTYPE_STMT,    0, NULL);

		// sanity checks:
		kAssert (m_dOCI8EnvHandle,     "m_dOCI8EnvHandle is still NULL after OpenConnection() logic");
		kAssert (m_dOCI8ErrorHandle,   "m_dOCI8ErrorHandle is still NULL after OpenConnection() logic");
		kAssert (m_dOCI8ServerContext, "m_dOCI8ServerContext is still NULL after OpenConnection() logic");
		#ifdef USE_SERVER_ATTACH
		kAssert (m_dOCI8ServerHandle,  "m_dOCI8ServerHandle is still NULL after OpenConnection() logic");
		kAssert (m_dOCI8Session,       "m_dOCI8Session is still NULL after OpenConnection() logic");
		#endif
		kAssert (m_dOCI8Statement,     "m_dOCI8Statement is still NULL after OpenConnection() logic");
		break;

	// - - - - - - - - - - - - - - - - -
	case API_OCI6:
	// - - - - - - - - - - - - - - - - -
		kDebugLog (GetDebugLevel(), "[{}] ORACLE_HOME='{}'", m_iDebugID, sOraHome);
		if (!*sOraHome) {
			kDebugLog (GetDebugLevel(), "KSQL::OpenConnection(): $ORACLE_HOME not set");
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
			return (SQLError());

		kDebugLog (GetDebugLevel()+1, "OCI6: connect through olog() succeeded.\n");
	
		// Only one cursor is associated with each KSQL instance, so open it now:
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		m_iErrorNum = oopen ((Cda_Def*)m_dOCI6ConnectionDataArea, (Lda_Def*)m_dOCI6LoginDataArea, NULL, -1, -1, NULL, -1);
		//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		if (!WasOCICallOK("OpenConnection:oopen"))
			return (SQLError());

		break;
	#endif

    #ifdef DEKAF2_HAS_DBLIB
	// - - - - - - - - - - - - - - - - -
	case API_DBLIB:
	// - - - - - - - - - - - - - - - - -
		dbinit();
		dberrhandle (NULL); // TODO: DBLIB BLOCKED until I figure out how to bind a member function here
		dbmsghandle (NULL); 

		pLogin = dblogin();
		DBSETLUSER (pLogin, GetDBUser());
		DBSETLPWD  (pLogin, GetDBPass());
		DBSETLAPP  (pLogin, "KSQL");

		m_pDBPROC = dbopen(pLogin, GetDBHost());
		if (!m_pDBPROC) {
			dbloginfree(pLogin);
			pLogin = NULL;
			if (m_sLastError == "") {
				m_sLastError.Format ("{}DBLIB: connection failed.", m_sErrorPrefix);
			}
			return (false); // connection failed	
		}

		if (!GetDBName().empty()) {
			kDebugLog (GetDebugLevel()+1, "use {}", GetDBName());
			dbuse(m_pDBPROC, GetDBName());
		}
		break;
	#endif

	#ifdef CTLIBCOMPILE
	// - - - - - - - - - - - - - - - - -
	case API_CTLIB:
	// - - - - - - - - - - - - - - - - -
		if (!ctlib_login()) {
			if (m_sLastError == "") {
				m_sLastError.Format ("{}CTLIB: connection failed.", m_sErrorPrefix);
			}
			return (SQLError ());
		}
		break;
	#endif

	// - - - - - - - - - - - - - - - - -
	case API_INFORMIX:
	default:
	// - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}API Set not coded yet ({}={})", m_sErrorPrefix, GetAPISet(), TxAPISet(GetAPISet()));
		return (SQLError ());
	}

	m_bConnectionIsOpen = true;
		
	kDebug (3, "[{}] connection is now open...", m_iDebugID);

	if (!IsFlag(F_NoTranslations))
		BuildTranslationList (&m_TxList);

    #ifdef DEKAF2_HAS_DBLIB
	if (pLogin) {
		dbloginfree (pLogin);
		pLogin = NULL;
	}
	#endif

	return (m_bConnectionIsOpen);

} // OpenConnection

//-----------------------------------------------------------------------------
void KSQL::CloseConnection ()
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]void KSQL::CloseConnection()...", m_iDebugID);

	m_sLastError = "";
	if (m_bConnectionIsOpen)
	{
		kDebugLog (GetDebugLevel(), "[{}]disconnecting from {}...", m_iDebugID, ConnectSummary());

		ResetErrorStatus ();

		switch (m_iAPISet)
		{
        #ifdef DEKAF2_HAS_MYSQL
		// - - - - - - - - - - - - - - - - -
		case API_MYSQL:
		// - - - - - - - - - - - - - - - - -
			kDebugLog (3, "mysql_close()...");
			mysql_close ((MYSQL*)m_dMYSQL);
			break;
		#endif

        #ifdef DEKAF2_HAS_ORACLE
		// - - - - - - - - - - - - - - - - -
		case API_OCI8:
		// - - - - - - - - - - - - - - - - -
			#ifdef USE_SERVER_ATTACH
			OCISessionEnd   ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle, (OCISession*)m_dOCI8Session, OCI_DEFAULT);
			OCIServerDetach ((OCIServer*)m_dOCI8ServerHandle,  (OCIError*)m_dOCI8ErrorHandle, OCI_DEFAULT);
			#else
			OCILogoff ((OCISvcCtx*)m_dOCI8ServerContext, (OCIError*)m_dOCI8ErrorHandle);
			#endif
			break;

		// - - - - - - - - - - - - - - - - -
		case API_OCI6:
		// - - - - - - - - - - - - - - - - -
			oclose ((Cda_Def*)m_dOCI6ConnectionDataArea);   // <-- close the cursor
			ologof ((Lda_Def*)m_dOCI6LoginDataArea);   // <-- close the connection      -- this sometimes causes a BUS ERROR -- why??
			break;
		#endif

        #ifdef DEKAF2_HAS_DBLIB
		// - - - - - - - - - - - - - - - - -
		case API_DBLIB:
		// - - - - - - - - - - - - - - - - -
			dbexit ();
			break;
		#endif

		#ifdef CTLIBCOMPILE
		// - - - - - - - - - - - - - - - - -
		case API_CTLIB:
		// - - - - - - - - - - - - - - - - -
			ctlib_logout ();
			break;
		#endif

		// - - - - - - - - - - - - - - - - -
		case API_INFORMIX:
		case API_DEKAF2_HAS_ODBC:
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("[{}] KSQL::CloseConnection(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
		}
	}

	FreeAll ();

	m_bConnectionIsOpen = false;

} // CloseConnection

//-----------------------------------------------------------------------------
void KSQL::SetErrorPrefix (KStringView sPrefix, uint32_t iLineNum/*=0*/)
//-----------------------------------------------------------------------------
{
	if (iLineNum && !sPrefix.empty()) {
		m_sErrorPrefix.Format ("{}:{}: ", sPrefix, iLineNum);
	}
	else if (!sPrefix.empty()) {
		m_sErrorPrefix.Format ("{}: ", sPrefix);
	}
	else {
		ClearErrorPrefix();
	}

} // SetErrorPrefix

//-----------------------------------------------------------------------------
bool KSQL::ExecRawSQL (KStringView sSQL, uint64_t iFlags/*=0*/, KStringView sAPI/*="ExecRawSQL"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug)) {
		kDebugLog (GetDebugLevel(), "[{}]{}: {}\n", m_iDebugID, sAPI, m_sLastSQL);
	}

	m_iNumRowsAffected = 0;

	//if (sSQL != m_sLastSQL) { // be careful, we might have called ExecRawSQL() with m_sLastSQL already
		m_sLastSQL = sSQL;
	//}

	EndQuery();

	bool   fOK          = false;
	int    iRetriesLeft = NUM_RETRIES;
	int    iSleepFor    = 0;
	time_t tStarted     = 0;

	if (m_iWarnIfOverNumSeconds) {
		tStarted = time(NULL);
	}

	while (!fOK && iRetriesLeft && !__sync_add_and_fetch(&m_bDisableRetries, 0))
	{
		ResetErrorStatus ();

		switch (m_iAPISet)
		{
        #ifdef DEKAF2_HAS_MYSQL
		// - - - - - - - - - - - - - - - - -
		case API_MYSQL:
		// - - - - - - - - - - - - - - - - -
			do // once
			{
				if (!m_dMYSQL) {
					kDebug (1, "KSQL::ExecRawSQL: lost m_dMYSQL pointer.  Reopening connection ...");
					CloseConnection();
					OpenConnection();
				}
				if (!m_dMYSQL) {
					kDebug (1, "KSQL::ExecRawSQL: failed.  aborting query or SQL:\n{}", m_sLastSQL);
					break; // once
				}
				kDebugLog (3, "mysql_query(): m_dMYSQL is {}, m_sLastSQL is {} bytes long", m_dMYSQL ? "not null" : "NULL", m_sLastSQL.size());
				if (mysql_query ((MYSQL*)m_dMYSQL, m_sLastSQL.c_str()))
				{
					m_iErrorNum = mysql_errno ((MYSQL*)m_dMYSQL);
					m_sLastError.Format ("{}MSQL-{}: {}", m_sErrorPrefix, GetLastErrorNum(), mysql_error((MYSQL*)m_dMYSQL));
					break; // inner do once
				}
				m_iNumRowsAffected = 0;
				kDebugLog (3, "mysql_affected_rows()...");
				my_ulonglong iNumRows = mysql_affected_rows ((MYSQL*)m_dMYSQL);
				if ((uint64_t)iNumRows != (uint64_t)(4294967295lu))
				{
					m_iNumRowsAffected = (uint64_t) iNumRows;
				}

				kDebugLog (3, "mysql_insert_id()...");
				my_ulonglong iNewID = mysql_insert_id ((MYSQL*)m_dMYSQL);
				m_iLastInsertID = (uint64_t) iNewID;
		
				if (m_iLastInsertID)
					kDebugLog (GetDebugLevel(), "[{}]ExecSQL: last insert ID = {}", m_iDebugID, m_iLastInsertID);
				kDebugLog (3, "no insert ID.");

				fOK = true;
			}
			while (false); // do once
			break;
		#endif

        #ifdef DEKAF2_HAS_ORACLE
		// - - - - - - - - - - - - - - - - -
		case API_OCI8:
		// - - - - - - - - - - - - - - - - -
			do // once
			{
				kDebugLog (3, "OCIStmtPrepare...");
				m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
				                             (text*)sSQL, strlen(sSQL), OCI_NTV_SYNTAX, OCI_DEFAULT);

				if (!WasOCICallOK("ExecSQL:OCIStmtPrepare")) {
					break; // inner do once
				}
	
				// Note: Using the OCI_COMMIT_ON_SUCCESS mode of the OCIExecute() call, the 
				// application can selectively commit transactions at the end of each 
				// statement execution, saving an extra roundtrip by calling OCITransCommit().
				kDebugLog (3, "OCIStmtExecute...");
				m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
								 (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, IsFlag(F_NoAutoCommit) ? 0 : OCI_COMMIT_ON_SUCCESS);

				if (!WasOCICallOK("ExecSQL:OCIStmtExecute")) {
					break; // inner do once
				}
	
				m_iNumRowsAffected = 0;
				m_iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumRowsAffected,
					0, OCI_ATTR_ROW_COUNT, (OCIError*)m_dOCI8ErrorHandle);

				fOK = true;
			}
			while (false); // do once
			break;

		// - - - - - - - - - - - - - - - - -
		case API_OCI6:
		// - - - - - - - - - - - - - - - - -
			do // once
			{
				// let the RDBMS parse the SQL expression:
				kDebugLog (3, "oparse...");
				m_iErrorNum = oparse ((Cda_Def*)m_dOCI6ConnectionDataArea, (text *)sSQL, (sb4) -1, (sword) PARSE_NO_DEFER, (ub4) PARSE_V7_LNG);
				if (!WasOCICallOK("ExecSQL:oparse")) {
					break; // inner do once
				}
		
				// execute it:
				kDebugLog (3, "oexec...");
				m_iErrorNum = oexec ((Cda_Def*)m_dOCI6ConnectionDataArea);
				if (!WasOCICallOK("ExecSQL:oexec")) {
					break; // inner do once
				}
		
				// now issue a "commit" or it will be rolled back automatically:
				if (!IsFlag(F_NoAutoCommit))
				{
					m_iErrorNum = ocom ((Lda_Def*)m_dOCI6LoginDataArea);
					if (!WasOCICallOK("ExecSQL:ocom"))
					{
						m_sLastError += "\non auto commit.";	
						break; // inner do once
					}
				}

				// ORACLE TODO: deal with SEQUENCE values
				m_iNumRowsAffected = ((Cda_Def*)m_dOCI6ConnectionDataArea)->rpc; // ocidfn.h: rows processed count

				ocan ((Cda_Def*)m_dOCI6ConnectionDataArea);  // <-- let cursor free it's resources

				fOK = true;
			}
			while (false); // do once
			break;
		#endif

		#ifdef CTLIBCOMPILE
		// - - - - - - - - - - - - - - - - -
		case API_CTLIB:
		// - - - - - - - - - - - - - - - - -
			fOK = ctlib_execsql (sSQL);
			if (!fOK) {
				kDebugLog (3, "ctlib_execsql returned false");
			}
			break;
		#endif

		// - - - - - - - - - - - - - - - - -
		case API_DBLIB:
		case API_INFORMIX:
		case API_DEKAF2_HAS_ODBC:
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("[{}] KSQL::ExecSQL(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);

		} // switch

		if (!fOK)
		{
			if (!(iRetriesLeft--) || !PreparedToRetry()) {
				iRetriesLeft = 0; // stop looping
			}
			else
			{
				// the first retry should have no delay:
				if (iSleepFor) {
					kDebugLog (GetDebugLevel(), "sleeping {} seconds...", iSleepFor);
					std::this_thread::sleep_for(std::chrono::seconds(iSleepFor));
				}

				// increase iSleepFor in case we end up here again:
				if (!iSleepFor) {
					iSleepFor = 1;  // second retry should have a delay of one second
				}
				else {
					iSleepFor *= 2; // double it each time after that
				}
			}
		}

	} // retry loop

	ClearErrorPrefix();

	if (!fOK) {
		return (SQLError());
	}

	if (m_iWarnIfOverNumSeconds)
	{
		time_t tTook = time(NULL) - tStarted;
		if (tTook >= m_iWarnIfOverNumSeconds) 
		{
			KString sWarning;
			sWarning.Format (
				"KSQL: warning: statement took longer than {} seconds (took {}):\n"
				"{}\n"
				"KSQL: {} rows affected.\n",
					m_iWarnIfOverNumSeconds,
					kTranslateSeconds(tTook),
					m_sLastSQL,
					m_iNumRowsAffected);

			if (m_bpWarnIfOverNumSeconds) {
				fprintf (m_bpWarnIfOverNumSeconds, "%s", sWarning.c_str());
				fflush (m_bpWarnIfOverNumSeconds);
			}
			else {
				kWarning ("{}", sWarning);
			}
		}
	}

	return (fOK);

} // ExecRawSQL

//-----------------------------------------------------------------------------
bool KSQL::PreparedToRetry ()
//-----------------------------------------------------------------------------
{
	// Note: this is called every time m_sLastError and m_iLastErrorNum have
	// been set by an RDBMS error.

	bool fConnectionLost = false;

	switch (m_iDBType)
	{
	case DBT_MYSQL:
		switch (m_iErrorNum)
		{
		# ifdef CR_SERVER_GONE_ERROR
		case CR_SERVER_GONE_ERROR:
		case CR_SERVER_LOST:
		# else
		case 2006:
		case 2013:
		# endif
			fConnectionLost = true;
			break;
		}
		break;

	case DBT_SQLSERVER:
	case DBT_SYBASE:
		switch (m_iErrorNum)
		{
		// SQL-01205: State 45, Severity 13, Line 1, Transaction (Process ID 95) was deadlocked on lock resources with 
		//  another process and has been chosen as the deadlock victim. Rerun the transaction.
		case  1205:
		// CTLIB-20004: Read from SQL server failed.
		// CTLIB-20006: Write to SQL Server failed.
		case 20004:
		case 20006:
			fConnectionLost = true;
		}
		break;
	default:
		break;
	}

	if (fConnectionLost)
	{
		if (IsFlag(F_IgnoreSQLErrors)) {
			kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, GetLastError());
			kDebugLog (GetDebugLevel(), "[{}] automatic retry now in progress...", m_iDebugID);
		}
		else {
			kWarning ("[{}] {}", m_iDebugID, GetLastError());
			kWarning ("[{}] automatic retry now in progress...", m_iDebugID);
		}

		KSQL SaveMe;
		SaveMe.CopyConnection (this);
		CloseConnection ();
		CopyConnection (&SaveMe);

		#if 0 // too much black magic
		if (!m_sDBCFile.empty() && kFileExists(m_sDBCFile)) {
			kDebugLog (GetDebugLevel(), "[{}] re-reading connection in from dbc file: {}", m_iDebugID, m_sDBCFile);
			LoadConnect (m_sDBCFile);
		}
		#endif

		OpenConnection ();

		if (IsConnectionOpen()) {
			kDebugLog (GetDebugLevel(), "[{}] new connection looks good.", m_iDebugID);
		}
		else if (IsFlag(F_IgnoreSQLErrors)) {
			kDebugLog (GetDebugLevel(), "[{}] NEW CONNECTION FAILED.", m_iDebugID);
		}
		else {
			kWarning ("[{}] NEW CONNECTION FAILED.", m_iDebugID);
		}

		return (true); // <-- we are now prepare for automatic retry
	}
	else {
        #ifdef DEKAF2_HAS_MYSQL
		kDebugLog (3, "FYI: cannot retry: {} [{},{}]: {}", GetLastErrorNum(), 2006, 2013, GetLastError());
		#endif
		return (false); // <-- no not retry
	}

} // PreparedToRetry

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ParseSQL (KStringView sFormat, ...)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]bool KSQL::ParseSQL()...", m_iDebugID);

	m_iLastInsertID = 0;
	EndQuery ();
	if (!OpenConnection())
		return (false);

	kDebugLog (3, "[{}]ParseSQL: format={}", m_iDebugID, sFormat);

	va_list vaArgList;
	va_start (vaArgList, sFormat);
	m_sLastSQL.FormatV(sFormat, vaArgList);
	va_end (vaArgList);

	if (!IsFlag(F_NoTranslations))
		DoTranslations (m_sLastSQL, m_iDBType);

	return (ParseRawSQL (m_sLastSQL, 0, "ParseSQL"));

} // ParseSQL

//-----------------------------------------------------------------------------
bool KSQL::ParseRawSQL (KStringView sSQL, int64_t iFlags/*=0*/, KStringView sAPI/*="ParseRawSQL"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug)) {
		kDebugLog (GetDebugLevel(), "[{}]{}: {}{}\n", m_iDebugID, sAPI, (strchr(m_sLastSQL,'\n')) ? "\n" : "", m_sLastSQL);
	}

	if (sSQL != m_sLastSQL) {
		m_sLastSQL = sSQL; // for error message display
	}

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case API_OCI8:
	// - - - - - - - - - - - - - - - - -
		m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
		                             (text*)sSQL, strlen(sSQL), OCI_NTV_SYNTAX, OCI_DEFAULT);
		if (!WasOCICallOK("ParseSQL:OCIStmtPrepare"))
			return (SQLError (/*fForceError=*/false));

		m_bStatementParsed = true;
		m_iMaxBindVars     = 5;
		m_idxBindVar       = 0;
		break;

	// - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("[{}] KSQL::ParseSQL(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
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
	if (!m_bStatementParsed) {
		kWarning ("[{}] KSQL::ExecParsedSQL(): ParseSQL() or ParseQuery() was not called yet.", m_iDebugID);
		kCrashExit (CRASHCODE_DEKAFUSAGE);
		return (false);
	}

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case API_OCI8:
	// - - - - - - - - - - - - - - - - -
		m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
						 (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, IsFlag(F_NoAutoCommit) ? 0 : OCI_COMMIT_ON_SUCCESS);
		if (!WasOCICallOK("ParseSQL:OCIStmtExecute"))
			return (SQLError (/*fForceError=*/false));

		m_iNumRowsAffected = 0;
		m_iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumRowsAffected,
			0, OCI_ATTR_ROW_COUNT, (OCIError*)m_dOCI8ErrorHandle);

		break;

	// - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("[{}] KSQL::ExecParseSQL(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
		kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	m_idxBindVar = 0; // <-- reset index into m_OCIBindArray[]

	ClearErrorPrefix();

	return (true);

} // ExecParsedSQL
#endif

//-----------------------------------------------------------------------------
bool KSQL::ExecSQLFile (KString sFilename)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]bool KSQL::ExecSQLFile()...", m_iDebugID);

	EndQuery ();
	if (!OpenConnection())
		return (false);

	kDebugLog (GetDebugLevel(), "[{}]ExecSQLFile: {}\n", m_iDebugID, sFilename);

	if (!kFileExists (sFilename))
	{
		m_sLastError.Format ("{}:ExecSQLFile(): file '{}' does not exist.", m_sErrorPrefix, sFilename);
		return (SQLError());
	}

	FILE* fp = fopen (sFilename.c_str(), "r");
	if (!fp)
	{
		m_sLastError.Format ("{}ExecSQLFile(): {} could not read from file '{}'", m_sErrorPrefix, kwhoami(), sFilename);
		SQLError();
		return (false);
	}

	m_sLastSQL.clear();

	enum      {MAXLINE = 5000};
	char      szLine[MAXLINE+2];
	bool      fDone              = false;
	bool      fOK                = true;
	uint64_t  iTotalRowsAffected = 0;
	uint32_t  iLineNum           = 1;
	uint32_t  iLineNumStart      = 0;
	uint32_t  iStatement         = 0;
	bool      fDropStatement     = false;
	bool      fHasNewline        = false;

	// SPECIAL DIRECTIVES:
	//   //DROP|                      -- drop statement (set ignore db errors flag for that statement)
	//   //define {{FOO}} definition  -- simple macro
	//   //MYS|                       -- line applies to MySQL only
	//   //ORA|                       -- line applies to Oracle only
	//   //SYB|                       -- line applies to Sybase only
	//   //MSS|                       -- line applies to (MS)SQLServer only
	enum   {MAXLEADER = 10};
	char   szLeader[MAXLEADER+1];
	snprintf (szLeader, MAXLEADER, "//%3.3s|", (m_iDBType == DBT_SQLSERVER) ? "MSS" : TxDBType(m_iDBType)); 
	KASCII::ktoupper (szLeader);

	// Special strings for supporting the "delimiter" statement
	const char* szDefaultDelim   = ";";
	enum        {MAXDELIM = 1000};
	char        szDelimiter[MAXDELIM+1];

	kstrncpy (szDelimiter, szDefaultDelim, MAXDELIM);

	// Loop through and process every line of the SQL file
	while (!fDone && fgets(szLine, MAXLINE, fp))
	{
		bool fFoundSpecialLeader = false;
		fHasNewline	= false;
		if (szLine[strlen(szLine)-1] == '\n')
		{
			iLineNum++;
			fHasNewline	= true;
		}
		
		kDebugLog (GetDebugLevel()+1, "[{}] {}", iLineNum, szLine);

		char* szStart = KASCII::ktrimleft(szLine);

		if (KASCII::strmatchN (szStart, "#include"))
		{
			KSTACK Parts;
			if (kParseDelimedList (szStart, Parts, '\"', /*fTrim=*/false) != 3) {
				m_sLastError.Format ("{}:{}: malformed include directive (file should be enclosed in double quotes).", sFilename, iLineNum);
				return (SQLError());
			}

			// Allow for ${myvar} variable expansion on include filenames
			KString sIncludedFile = Parts.Get(1);
			kExpandVariables (sIncludedFile);

			if (!kFileExists (sIncludedFile)) {
				m_sLastError.Format ("{}:{}: missing include file: {}", sFilename, iLineNum, sIncludedFile);
				return (SQLError());
			}
			kDebugLog (GetDebugLevel(), "{}, line {}: recursing to include file: {}", sFilename, iLineNum, sIncludedFile);
			if (!ExecSQLFile (sIncludedFile)) {
				// m_sLastError is already set
				return (SQLError());
			}
			continue; // done with #include
		}

		// look for special directives:
		while (KASCII::strmatchN (szStart, "//"))
		{
			kDebugLog (3, " line {}: seeing if this is a special directive or just a comment..", iLineNum);

			if (KASCII::strmatchN (szStart, szLeader))
			{
				kDebugLog (GetDebugLevel()+1, " line {}: contains {}-specific code (included)", iLineNum, TxDBType(m_iDBType));

				szStart += strlen(szLeader);
				szStart = KASCII::ktrimleft(szStart);
				fFoundSpecialLeader = true;
			}
			else if (KASCII::strmatchN (szStart, "//DROP|"))
			{
				kDebugLog (GetDebugLevel()+1, " line {}: is a drop statement", iLineNum);
				szStart += strlen("//DROP|");
				szStart = KASCII::ktrimleft(szStart);
				fFoundSpecialLeader = true;
				fDropStatement = true;
			}
			else if (KASCII::strmatchN (szStart, "//define"))
			{
				kDebugLog (GetDebugLevel()+1, " line {}: is a macro definition", iLineNum);

				char* sToken = KASCII::ktrimleft(szStart+strlen("//define"));
				if (!KASCII::strmatchN (sToken, "{{")) {
					m_sLastError.Format ("{}:{}: malformed token: {}", sFilename, iLineNum, szStart);
					return (false);
				}
				char* sEnd = strstr (sToken, "}}");
				if (!sEnd) {
					m_sLastError.Format ("{}:{}: malformed {{token}}: {}", sFilename, iLineNum, szStart);
					return (false);
				}
				char* sValue = KASCII::ktrimleft(sEnd+strlen("}}"));
				*sEnd = 0;
				m_TxList.Add (sToken+strlen("{{"), KASCII::ktrimright(sValue));
				szStart = (char*)""; // we ate this line
				fFoundSpecialLeader = true;
			}
			else {
				kDebugLog (3, " line {}: done processing directive[s]", iLineNum);
				break; // quit processing directives
			}
		}

		if (!szStart || !*szStart) {
			continue;
		}

		// allow full-line comments (end of line comments are a pain because of quoted inserts and updates):
		if (KASCII::strmatchN (szStart, "//")      // C++
		 || KASCII::strmatchN (szStart, "#")       // MySQL
		 || KASCII::strmatchN (szStart, "--")      // Oracle
		 || KASCII::strmatchN (szStart, "rem ")    // Oracle
		 || KASCII::strmatchN (szStart, "REM ")    // Oracle
		)
		{
			continue; // <-- skip full line comment lines
		}

		// the "delimiter" is currently only used for stored procedure files
		if (KASCII::strmatchNI (szStart, "delimiter")) {
			kstrncpy (szDelimiter, szStart+strlen("delimiter"), MAXDELIM);
			szDelimiter[MAXDELIM] = 0;
			const char* szTrimDelim = KASCII::ktrimleft(KASCII::ktrimright(szDelimiter));
			if (!*szTrimDelim)
			{
				szTrimDelim = szDefaultDelim;
			}
			memmove (szDelimiter, szTrimDelim, strlen(szTrimDelim) + 1);
			kDebugLog (GetDebugLevel(), "line {} setting delimiter={}\n", iLineNum, szDelimiter);
			continue; // <-- skip delimiter statement after extracting the delimiter
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// trimming must be done very carefully:
		//  * we could be in the middle of a quoted string
		//  * don't trim LEFT b/c it affects SQL statement readability
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		size_t iLenDelim       = strlen(szDelimiter);
		size_t iLenPiece       = strlen(szStart);
		bool   fFoundDelimiter = false;
		while ((iLenPiece >= iLenDelim) &&
		    (  (szStart[iLenPiece-1] <= ' ')
		    || (KASCII::strmatch (szStart+iLenPiece-iLenDelim, szDelimiter))))
		{
			if (KASCII::strmatch (szStart+iLenPiece-iLenDelim, szDelimiter)) {
				fFoundDelimiter = true;
				iLenPiece -= iLenDelim;
			} else {
				--iLenPiece;
			}
			szStart[iLenPiece] = 0;
		}

		if (!iLineNumStart || m_sLastSQL == "") {
			iLineNumStart = iLineNum;
		}

		if (fFoundSpecialLeader) {
			m_sLastSQL += szStart;  // <-- add the line to the SQL statement buffer
		}
		else {
			m_sLastSQL += szLine;    // <-- add the line to the SQL statement buffer [with leading whitespace]
		}

		if (fHasNewline) {
			m_sLastSQL += "\n";  // <-- maintain readability by replacing the newline
		}

		// execute the SQL statement everytime we encounter a semicolon at the end:
		if (fFoundDelimiter)
		{
			ExecSQLFileGo (sFilename, iLineNumStart, iStatement, iTotalRowsAffected, fOK, fDone, fDropStatement /*, m_sLastSQL*/);
		}

	} // while getting lines

	// execute the last SQL statement (in case they forgot the semicolon):
	if (!fDone && m_sLastSQL != "")
	{
		ExecSQLFileGo (sFilename, iLineNumStart, iStatement, iTotalRowsAffected, fOK, fDone, fDropStatement /*, m_sLastSQL*/);
	}

	kDebugLog (GetDebugLevel(), "ExecSQLFile({}): TotalRowsAffected = {}", sFilename, iTotalRowsAffected);
	m_iNumRowsAffected = iTotalRowsAffected;

	ClearErrorPrefix ();

	return (fOK);

} // KSQL::ExecSQLFile

//-----------------------------------------------------------------------------
void KSQL::ExecSQLFileGo (KStringView sFilename, uint32_t& iLineNumStart, uint32_t& iStatement, uint64_t& iTotalRowsAffected, bool& fOK, bool& fDone, bool& fDropStatement /*, m_sLastSQL*/)
//-----------------------------------------------------------------------------
{
	++iStatement;

	if (fDropStatement) {
		kDebugLog (GetDebugLevel(), "setting F_IgnoreSQLErrors");
		SetFlags (F_IgnoreSQLErrors);
	}

	SetErrorPrefix (sFilename, iLineNumStart);

	kDebugLog (GetDebugLevel()+1, "ExecSQLFileGo({}): statement # {}:\n{}\n", sFilename, iStatement, m_sLastSQL);

	if (m_sLastSQL == "")
	{
		kDebugLog (GetDebugLevel()+1, "ExecSQLFile({}): statement # {} is EMPTY.", sFilename, iStatement);
	}
	else if (m_sLastSQL == "exit\n"   || m_sLastSQL == "quit\n")
	{
		kDebugLog (GetDebugLevel()+1, "ExecSQLFile({}): statement # {} is '{}' (stopping).", sFilename, iStatement, m_sLastSQL);
		fOK = fDone = true;
	}
	else if (KASCII::strmatchN (m_sLastSQL.c_str(), "select") || KASCII::strmatchN (m_sLastSQL.c_str(), "SELECT"))
	{
		kDebugLog (3, "ExecSQLFile({}): statement # {} is a QUERY...", sFilename, iStatement);

		if (!IsFlag(F_NoTranslations))
			DoTranslations (m_sLastSQL, m_iDBType);
	
		if (!ExecRawQuery (m_sLastSQL, 0, "ExecSQLFile") && !fDropStatement)
		{
			fOK   = false;
			fDone = true;
		}
	}
	else
	{
		kDebugLog (3, "ExecSQLFile({}): statement # {} is not a query...", sFilename, iStatement);

		if (!IsFlag(F_NoTranslations))
			DoTranslations (m_sLastSQL, m_iDBType);

		if (!ExecRawSQL (m_sLastSQL, 0, "ExecSQLFile") && !fDropStatement)
		{
			fOK   = false;
			fDone = true;
		}
		else {
			iTotalRowsAffected += GetNumRowsAffected();
		}
	}

	if (fDropStatement) {
		SetFlags (0);
	}

	m_sLastSQL    = "";     // reset buffer
	iLineNumStart  = 0;     // reset line num
	fDropStatement = false; // reset flags

} // ExecSQLFileGo

//-----------------------------------------------------------------------------
bool KSQL::ExecRawQuery (KStringView sSQL, uint64_t iFlags/*=0*/, KStringView sAPI/*="ExecRawQuery"*/)
//-----------------------------------------------------------------------------
{
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug))
		kDebugLog (GetDebugLevel(), "[{}]{}: {}{}\n", m_iDebugID, sAPI, (strchr(m_sLastSQL.c_str(),'\n')) ? "\n" : "", m_sLastSQL);

	if (sSQL != m_sLastSQL) {
		m_sLastSQL += sSQL;
	}

	EndQuery();

	time_t tStarted     = 0;
	if (m_iWarnIfOverNumSeconds) {
		tStarted = time(NULL);
	}

	#if defined(CTLIBCOMPILE)
	uint32_t iRetriesLeft = NUM_RETRIES;
	#endif
	
#ifndef _WIN32
	timeval startTime;
	gettimeofday (&startTime, 0);
#endif

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - -
	case API_MYSQL:
	// - - - - - - - - - - - - - - - - -
		do // once
		{
			// with mysql, I can get away with calling my ExecSQL() function for the
			// start of queries...
			if (!ExecRawSQL (sSQL, F_NoKlogDebug, "ExecRawQuery")) {
				// note: retry logic for MySQL is inside ExecRawSQL()
				return (false);
			}

			kDebugLog (3, "mysql_use_result()...");
			m_dMYSQLResult = mysql_use_result ((MYSQL*)m_dMYSQL);
			if (!m_dMYSQLResult)
			{
				m_iErrorNum = mysql_errno ((MYSQL*)m_dMYSQL);
				m_sLastError.Format ("{}MSQL-{}: {}", m_sErrorPrefix, GetLastErrorNum(), mysql_error((MYSQL*)m_dMYSQL));
				return (SQLError());
			}

			kDebugLog (3, "[{}] getting col info from mysql...", m_iDebugID);
			kDebugLog (3, "mysql_field_count()...");

			m_iNumColumns = mysql_field_count ((MYSQL*)m_dMYSQL);

			kDebugLog (3, "[{}] num columns: {}", m_iDebugID, m_iNumColumns);
			kDebugLog (3, "  mallocing for a results row with {} columns...", m_iNumColumns);
			m_dColInfo = (COLINFO*) kmalloc (m_iNumColumns * sizeof(COLINFO), "    KSQL:ExecQuery:m_dColInfo");

			// { char*  dszSANITY = (char*)malloc(75); /*strcpy (dszSANITY, "SANITY");*/ /*free (dszSANITY);*/ }

			MYSQL_FIELD* pField;

			kDebugLog (3, "mysql_fetch_field() X N ...");

			for (int ii=1; ((pField = mysql_fetch_field((MYSQL_RES*)m_dMYSQLResult))); ++ii)
			{
				m_dColInfo[ii-1].iDataType   = 0; // TODO:KEEF
				m_dColInfo[ii-1].iMaxDataLen = 0; // TODO:KEEF
				kstrncpy (m_dColInfo[ii-1].szColName, (char* )(pField->name), MAXCOLNAME+1);
				m_dColInfo[ii-1].dszValue    = NULL;
				m_dColInfo[ii-1].indp        = 0;
			}
		}
		while (false); // do once
		break;
	#endif

    #ifdef DEKAF2_HAS_ORACLE
	// - - - - - - - - - - - - - - - - -
	case API_OCI8:
	// - - - - - - - - - - - - - - - - -
		do // once
		{
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 1. OCI8: local "prepare" (parse) of SQL statement (note: in OCI6 this was a trip to the server)
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
		                             (text*)sSQL, strlen(sSQL), OCI_NTV_SYNTAX, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ExecQuery:OCIStmtPrepare"))
				return (SQLError (/*fForceError=*/false));

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 2. OCI8: pre-execute (parse only) -- redundant trip to server until I get this all working
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
					/*iters=*/1, /*rowoff=*/0, /*snap_in=*/NULL, /*snap_out=*/NULL, OCI_DESCRIBE_ONLY);

			if (!WasOCICallOK("ExecQuery:OCIStmtExecute(parse)"))
				return (SQLError (/*fForceError=*/false));

			// Get the number of columns in the query:
			m_iErrorNum = OCIAttrGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, &m_iNumColumns,
				0, OCI_ATTR_PARAM_COUNT, (OCIError*)m_dOCI8ErrorHandle);

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 3. OCI8: allocate storage for the results set:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			if (!WasOCICallOK("ExecQuery:OCIAttrGet(numcols)"))
				return (SQLError (/*fForceError=*/false));

			kDebugLog (3, "  mallocing for a results row with {} columns...", m_iNumColumns);
			m_dColInfo = (COLINFO*) kmalloc (m_iNumColumns * sizeof(COLINFO), "    KSQL:ExecQuery:m_dColInfo");

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
				kDebugLog (3, "  retrieving info about col # {}", ii);

				// get parameter for column ii:
				m_iErrorNum = OCIParamGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, (OCIError*)m_dOCI8ErrorHandle, (void**)&pColParam, ii);
				if (!WasOCICallOK("ExecQuery:OCIParamGet"))
					return (SQLError());

				if (!pColParam)
					kDebugLog (GetDebugLevel(), "[{}] OCI: pColParam is NULL but was supposed to get allocated by OCIParamGet", m_iDebugID);

				// Retrieve the data type attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid*) &iDataType, (ub4 *) NULL, OCI_ATTR_DATA_TYPE, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(datatype)"))
					return (SQLError());

				// Retrieve the column name attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid**) &dszColName, (ub4 *) &iLenColName, OCI_ATTR_NAME, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(colname)"))
					return (SQLError());
				dszColName[iLenColName] = 0;  // <-- OCIAttrGet() does not even terminate the string (duh)

				// Retreive the maximum datasize:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(void**) &iMaxDataSize, (ub4 *) NULL, OCI_ATTR_DATA_SIZE, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecQuery:OCIAttrGet(datasize)"))
					return (SQLError());

				m_dColInfo[ii-1].iDataType   = iDataType;
				m_dColInfo[ii-1].iMaxDataLen = max(iMaxDataSize+2,22+2); // <-- always malloc at least 24 bytes

				kDebugLog (GetDebugLevel()+1, "  oci8:column[{}]={}, namelen={}, datatype={}, maxwidth={}, willuse={}", 
					(int)ii, (char* )dszColName, (uint32_t)iLenColName, (int)iDataType, (int)iMaxDataSize, m_dColInfo[ii-1].iMaxDataLen);

				kstrncpy (m_dColInfo[ii-1].szColName, (char* )dszColName, MAXCOLNAME+1);

				m_dColInfo[ii-1].dszValue    = (char *) kmalloc ((int)(m_dColInfo[ii-1].iMaxDataLen + 1), "  KSQL:ExecQuery:dszValue:oci8");

				OCIDefine* dDefine;
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = OCIDefineByPos ((OCIStmt*)m_dOCI8Statement, &dDefine, (OCIError*)m_dOCI8ErrorHandle,
							ii,                                   // column number
							(dvoid *)(m_dColInfo[ii-1].dszValue), // data pointer
							m_dColInfo[ii-1].iMaxDataLen,          // max data length
							SQLT_STR,                             // always bind as a zero-terminated string
							(dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecQuery:OCIDefineByPos"))
				{
					return (SQLError());
				}

				OCIHandleFree ((dvoid*)dDefine, OCI_HTYPE_DEFINE);

			} // foreach column

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. OCI8: execute statement (start the query)...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
						 (OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iOCI8FirstRowStat = m_iErrorNum;
			if ((m_iErrorNum != OCI_NO_DATA) && !WasOCICallOK("ExecQuery:OCIStmtExecute"))
			{
				return (SQLError());
			}

			// FYI: m_iOCI8FirstRowStat will indicate whether or not OCIStmtExecute() returned the first row
		}
		while (false); // do once
		break;

	// - - - - - - - - - - - - - - - - -
	case API_OCI6:
	// - - - - - - - - - - - - - - - - -
		do // once
		{
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 1. Parse the statement: do not defer the parse, so that errors come back right away
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebugLog (3, "  calling oparse() to parse SQL statement...");

			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			m_iErrorNum = oparse((Cda_Def*)m_dOCI6ConnectionDataArea, (text *)sSQL, -1, PARSE_NO_DEFER, PARSE_V7_LNG);
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
			kDebugLog (3, "  determining number of columns in result set...");
			m_iNumColumns = 0;
			while (1)
			{
				static COLINFO TmpCol;
				iColNameLen = 50;
				sb4 sb4MaxWidth    = 0;
				sb2 sb2DataType    = iDataType;
				sb4 sb4ColNameLen  = iColNameLen;
				sb4 sb4MaxDataSize = iMaxDataSize;

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = odescr((Cda_Def*)m_dOCI6ConnectionDataArea,
					m_iNumColumns+1,
					&sb4MaxWidth,      // max data width coming down the pipe
					&sb2DataType,
					(signed char*) TmpCol.szColName,          // column name (overrun protected by iColNameLen)
					&sb4ColNameLen,
					&sb4MaxDataSize,
					NULL,                      // precision
					NULL,                      // scale
					NULL);                     // nulls OK
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

				TmpCol.iMaxDataLen = sb4MaxWidth;
				iDataType          = sb2DataType;
				iColNameLen        = sb4ColNameLen;
				iMaxDataSize       = sb4MaxDataSize;

				if (!WasOCICallOK("ExecQuery:odescr1"))
				{
					if (!TmpCol.iMaxDataLen)
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
					kDebugLog (3, "    col[{}] = {}", m_iNumColumns, TmpCol.szColName);
				}

			} // while counting cols

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 3. allocate storage for the results set:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebugLog (3, "  mallocing for a results row with {} columns...", m_iNumColumns);
			m_dColInfo = (COLINFO*) kmalloc (m_iNumColumns * sizeof(COLINFO), "    KSQL::ExecQuery");

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 4. loop through result columns, and bind the memory:
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebugLog (3, "  calling odescr() to bind each column...");
			for (uint32_t ii=1; ii<=m_iNumColumns; ++ii)
			{
				iColNameLen = 50;
				sb4 sb4MaxWidth   = iMaxWidth;
				sb2 sb2DataType   = iDataType;
				sb4 sb4ColNameLen = iColNameLen;
				sb4 sb4MaxDataSize = iMaxDataSize;

				// now that we have proper allocation for m_Columns[], repeat
				// the odescr() calls and fill in m_Columns[]:
				kDebugLog (3, "    col[{}]: filling in my m_Columns[] struct...", ii);

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = odescr((Cda_Def*)m_dOCI6ConnectionDataArea, ii,
					&sb4MaxWidth,
					&sb2DataType,
					(signed char*)m_dColInfo[ii-1].szColName,   // column name (protected from overrung by iColNameLen)
					&sb4ColNameLen,
					&sb4MaxDataSize,
					NULL,					   // precision
					NULL,					   // scale
					NULL);					   // nulls OK
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

				iMaxWidth    = sb4MaxWidth;
				iDataType    = sb2DataType;
				iColNameLen  = sb4ColNameLen;
				iMaxDataSize = sb4MaxDataSize;

				if (!WasOCICallOK("ExecQuery:odescr2"))
				{
					return (SQLError());
				}

				m_dColInfo[ii-1].iDataType   = iDataType;
				m_dColInfo[ii-1].iMaxDataLen = max(iMaxDataSize+2,22+2); // <-- always malloc at least 24 bytes

				kDebugLog (GetDebugLevel(), "  oci6:column[{}]={}, namelen={}, datatype={}, maxwidth={}, dsize={}, using={} -- OCI6 BUG [TODO]", 
						ii, m_dColInfo[ii-1].szColName, (uint32_t)iColNameLen, (int)iDataType, 
						(int)iMaxWidth, (int)iMaxDataSize, m_dColInfo[ii-1].iMaxDataLen);

				// allocate at least 20 bytes, which will cover datetimes and numerics:
				m_dColInfo[ii-1].iMaxDataLen = min (m_dColInfo[ii-1].iMaxDataLen+2, 20);

				m_dColInfo[ii-1].dszValue = (char *) kmalloc ((int)(m_dColInfo[ii-1].iMaxDataLen + 1), "    KSQL::ExecQuery");
				// bind some memory to each column to hold the results which will be coming back:
				kDebugLog (3, "    col[{}]: calling odefin() to bind memory...", ii);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = odefin ((Cda_Def*)m_dOCI6ConnectionDataArea,
							ii,
							(ub1 *)(m_dColInfo[ii-1].dszValue),  // data pointer
							m_dColInfo[ii-1].iMaxDataLen,        // max data length
							STRING_TYPE,                         // always bind as a string
							-1,                                  // scale
							&(m_dColInfo[ii-1].indp),            // used for SQL NULL determination
							(text *) NULL,                       // fmt
							-1,                                  // fmtl
							-1,                                  // fmtt
							&iColRetlen,
							&iColRetcode);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecQuery:odefin"))
				{
					return (SQLError());
				}

			} // while binding

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. start the select statement...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			kDebugLog (3, "  calling oexec() to actually begin the SQL statement...");
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

	#ifdef CTLIBCOMPILE
	// - - - - - - - - - - - - - - - - -
	case API_CTLIB:
	// - - - - - - - - - - - - - - - - -
		do // once
		{
			kDebugLog (CTDEBUG, "ensuring that no dangling messages are in the ct queue...");
			
			if(IsFlag(F_AutoReset)) {
				ctlib_login();
			}
			
			ctlib_flush_results ();
			ctlib_clear_errors ();

			kDebugLog (CTDEBUG, "calling ct_command()...");

			if (ct_command (m_pCtCommand, CS_LANG_CMD, sSQL, CS_NULLTERM, CS_UNUSED) != CS_SUCCEED) {
				ctlib_api_error ("KSQL>ExecQuery>ct_command");
				if (--iRetriesLeft && PreparedToRetry())
					continue;
				return (SQLError());
			}

			kDebugLog (CTDEBUG, "calling ct_send()...");

			if (ct_send(m_pCtCommand) != CS_SUCCEED) {
				ctlib_api_error ("KSQL>ExecQuery>ct_send");
				if (--iRetriesLeft && PreparedToRetry())
					continue;
				return (SQLError());
			}
			if (!ctlib_prepare_results ()) {
				if (--iRetriesLeft && PreparedToRetry())
					continue;
				return (SQLError());
			}
			if (ctlib_check_errors() != 0) {
				if (--iRetriesLeft && PreparedToRetry())
					continue;
				return (SQLError());
			}

			break; // success
		}
		while (true);
		break;
	#endif

	// - - - - - - - - - - - - - - - - -
	case API_DBLIB:
	case API_INFORMIX:
	case API_DEKAF2_HAS_ODBC:
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("[{}] KSQL::ExecQuery(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
		kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	// ensure every column has a name:
	for (uint32_t ii = 0; ii < m_iNumColumns; ++ii)
	{
		if (!(m_dColInfo[ii].szColName[0])) {
			snprintf (m_dColInfo[ii].szColName, MAXCOLNAME, "col:%d", ii+1);
			kDebugLog (GetDebugLevel(), "column was missing a name: {}", m_dColInfo[ii].szColName);
		}
	}

	m_bQueryStarted     = true;
	m_iRowNum           = 0;
	m_iNumRowsBuffered  = 0;
	m_iNumRowsAffected  = 0;

	ClearErrorPrefix();
	
#ifndef _WIN32
	int64_t iMicrSecs = 0;
	timeval endTime;
	gettimeofday (&endTime, 0);
	iMicrSecs = (endTime.tv_sec - startTime.tv_sec) * 1000000 + endTime.tv_usec - startTime.tv_usec;
	iMicrSecs /= 1000;
	
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug)) {
		kDebugLog (GetDebugLevel(), "[DB Query Time]: {} mls", iMicrSecs);
	}
#endif

	if (m_iWarnIfOverNumSeconds)
	{
		time_t tTook = time(NULL) - tStarted;
		if (tTook >= m_iWarnIfOverNumSeconds) 
		{
			KString sWarning;
			sWarning.Format (
				"KSQL: warning: query took longer than {} seconds (took {}):\n"
				"{}\n",
					m_iWarnIfOverNumSeconds,
					kTranslateSeconds(tTook),
					m_sLastSQL);

			if (m_bpWarnIfOverNumSeconds) {
				fprintf (m_bpWarnIfOverNumSeconds, "%s", sWarning.c_str());
				fflush (m_bpWarnIfOverNumSeconds);
			}
			else {
				kWarning ("{}", sWarning);
			}
		}
	}

	if (IsFlag(F_BufferResults))
		return (BufferResults());
	else
		return (true);

} // KSQL::ExecRawQuery

#ifdef DEKAF2_HAS_ORACLE
//-----------------------------------------------------------------------------
bool KSQL::ParseQuery (KStringView sFormat, ...)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]bool KSQL::ParseQuery()...", m_iDebugID);

	EndQuery ();
	if (!OpenConnection())
		return (false);

	va_list vaArgList;
	va_start (vaArgList, sFormat);
	m_sLastSQL.FormatV(sFormat, vaArgList);
	va_end (vaArgList);

	if (!IsFlag(F_NoTranslations))
		DoTranslations (m_sLastSQL, m_iDBType);

	if (!IsFlag(F_IgnoreSelectKeyword) && !KASCII::strmatchN (m_sLastSQL.c_str(), "select") && !KASCII::strmatchN (m_sLastSQL.c_str(), "SELECT"))
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
	if (!(iFlags & F_NoKlogDebug) && !(m_iFlags & F_NoKlogDebug)) {
		kDebugLog (GetDebugLevel(), "[{}]{}: {}{}\n", m_iDebugID, sAPI, (strchr(m_sLastSQL.c_str(),'\n')) ? "\n" : "", m_sLastSQL);
	}

	if (sSQL != m_sLastSQL) {
		m_sLastSQL += sSQL; // for error message display
	}

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case API_OCI8:
	// - - - - - - - - - - - - - - - - -
		// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
		// 1. OCI8: local "prepare" (parse) of SQL statement (note: in OCI6 this was a trip to the server)
		// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
		m_iErrorNum = OCIStmtPrepare ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
		                             (text*)sSQL, strlen(sSQL), OCI_NTV_SYNTAX, OCI_DEFAULT);
		// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
		if (!WasOCICallOK("ParseQuery:OCIStmtPrepare"))
			return (SQLError (/*fForceError=*/false));

		m_bStatementParsed = true;
		break;

	// - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("[{}] KSQL::ParseQuery(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
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
	if (!m_bStatementParsed) {
		kWarning ("[{}] KSQL::ExecParsedQuery(): ParseQuery() was not called yet.", m_iDebugID);
		kCrashExit (CRASHCODE_DEKAFUSAGE);
		return (false);
	}

	ResetErrorStatus ();

	switch (m_iAPISet)
	{
	// - - - - - - - - - - - - - - - - -
	case API_OCI8:
	// - - - - - - - - - - - - - - - - -
		do // once
		{
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 2. OCI8: pre-execute (parse only) -- redundant trip to server until I get this all working
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 
					/*iters=*/1, /*rowoff=*/0, /*snap_in=*/NULL, /*snap_out=*/NULL, OCI_DESCRIBE_ONLY);

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

			kDebugLog (3, "  mallocing for a results row with {} columns...", m_iNumColumns);
			m_dColInfo = (COLINFO*) kmalloc (m_iNumColumns * sizeof(COLINFO), "    KSQL:ExecParsedQuery:m_dColInfo");

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
				kDebugLog (3, "  retrieving info about col # {}", ii);

				// get parameter for column ii:
				m_iErrorNum = OCIParamGet ((OCIStmt*)m_dOCI8Statement, OCI_HTYPE_STMT, (OCIError*)m_dOCI8ErrorHandle, (void**)&pColParam, ii);
				if (!WasOCICallOK("ExecParsedQuery:OCIParamGet"))
					return (SQLError());

				if (!pColParam)
					kDebugLog (GetDebugLevel(), "[{}] OCI: pColParam is NULL but was supposed to get allocated by OCIParamGet", m_iDebugID);

				// Retrieve the data type attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid*) &iDataType, (ub4 *) NULL, OCI_ATTR_DATA_TYPE, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(datatype)"))
					return (SQLError());

				// Retrieve the column name attribute:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(dvoid**) &dszColName, (ub4 *) &iLenColName, OCI_ATTR_NAME, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(colname)"))
					return (SQLError());
				dszColName[iLenColName] = 0;  // <-- OCIAttrGet() does not even terminate the string (duh)

				// Retreive the maximum datasize:
				m_iErrorNum = OCIAttrGet ((dvoid*) pColParam, OCI_DTYPE_PARAM, 
					(void**) &iMaxDataSize, (ub4 *) NULL, OCI_ATTR_DATA_SIZE, 
					(OCIError*)m_dOCI8ErrorHandle);
				if (!WasOCICallOK("ExecParsedQuery:OCIAttrGet(datasize)"))
					return (SQLError());

				m_dColInfo[ii-1].iDataType   = iDataType;
				m_dColInfo[ii-1].iMaxDataLen = max(iMaxDataSize+2,22+2); // <-- always malloc at least 24 bytes

				kDebugLog (GetDebugLevel()+1, "  oci8:column[{}]={}, namelen={}, datatype={}, maxwidth={}, willuse={}", 
						(int)ii, (char*)dszColName, (uint32_t)iLenColName, (int)iDataType, (int)iMaxDataSize, m_dColInfo[ii-1].iMaxDataLen);

				kstrncpy (m_dColInfo[ii-1].szColName, (char*)dszColName, 20+1);

				m_dColInfo[ii-1].dszValue    = (char *) kmalloc ((int)(m_dColInfo[ii-1].iMaxDataLen + 1), "  KSQL:ExecParsedQuery:dszValue:oci6");

				OCIDefine* dDefine;
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = OCIDefineByPos ((OCIStmt*)m_dOCI8Statement, &dDefine, (OCIError*)m_dOCI8ErrorHandle,
							ii,                                   // column number
							(dvoid *)(m_dColInfo[ii-1].dszValue), // data pointer
							m_dColInfo[ii-1].iMaxDataLen,          // max data length
							SQLT_STR,                             // always bind as a zero-terminated string
							(dvoid *) 0, (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("ExecParsedQuery:OCIDefineByPos"))
				{
					return (SQLError());
				}

				OCIHandleFree ((dvoid*)dDefine, OCI_HTYPE_DEFINE);

			} // foreach column

			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			// 5. OCI8: execute statement (start the query)...
			// -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -
			m_iErrorNum = OCIStmtExecute ((OCISvcCtx*)m_dOCI8ServerContext, (OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, 0,
						 (OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
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
		kWarning ("[{}] KSQL::ExecParsedQuery(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
		kCrashExit (CRASHCODE_DEKAFUSAGE);
	}

	m_bQueryStarted     = true;
	m_iRowNum           = 0;
	m_iNumRowsBuffered  = 0;
	m_idxBindVar        = 0; // <-- reset index into m_OCIBindArray[]

	if (IsFlag(F_BufferResults))
		return (BufferResults());
	else
		return (true);

} // ExecParsedQuery
#endif

//-----------------------------------------------------------------------------
uint32_t KSQL::GetNumCols ()
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]uint32_t KSQL::GetNumCols()...", m_iDebugID);

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
	kDebugLog (3, "[{}]bool KSQL::BufferResults()...", m_iDebugID);

	if (!QueryStarted())
	{
		m_sLastError.Format ("{}BufferResults(): called when query was not started.", m_sErrorPrefix);
		return (SQLError());
	}

	if (kFileExists (m_sTmpResultsFile))
	{
		kRemoveFile (m_sTmpResultsFile);
	}

	KOutFile file(m_sTmpResultsFile.c_str());
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
	case API_MYSQL:
	// - - - - - - - - - - - - - - - - -
		kDebugLog (3, "mysql_fetch_row() X N ...");

		while ((m_MYSQLRow = mysql_fetch_row ((MYSQL_RES*)m_dMYSQLResult)))
		{
			++m_iNumRowsBuffered;
			for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
			{
				char* colval = ((MYSQL_ROW)(m_MYSQLRow))[ii];
				if (!colval)
					colval = (char*)"";   // <-- change db NULL to cstring ""
				
				// we need to assure that no newlines are in buffered data:
				char* spot = strchr (colval, '\n');
				while (spot)
				{
					*spot = 2; // <-- change them to ^B
					spot = strchr (spot+1, '\n');
				}
				if (strlen(colval) > 50)
				{
					kDebugLog (3, "  buffered: row[{}]col[{}]: strlen()={}", m_iNumRowsBuffered, ii+1, strlen(colval));
				}
				else
				{
					kDebugLog (3, "  buffered: row[{}]col[{}]: '{}'", m_iNumRowsBuffered, ii+1, colval);
				}

				file.FormatLine ("{}|{}|{}", m_iNumRowsBuffered, ii+1, strlen(colval));
				file.FormatLine ("{}\n", colval ? colval : "");
			}
		}
		break;
	#endif

    #ifdef DEKAF2_HAS_ORACLE
	// - - - - - - - - - - - - - - - - -
	case API_OCI8:
	// - - - - - - - - - - - - - - - - -
		while (1)
		{
			if (!m_iNumRowsBuffered)
			{
				if (m_iOCI8FirstRowStat == OCI_NO_DATA)
				{
					kDebugLog (3, "OCIStmtExecute() said we were done");
					break; // while
				}

				// OCI8: first row was already (implicitly) fetched by OCIStmtExecute() [yuk]
				m_iErrorNum     = m_iOCI8FirstRowStat;
				m_sLastError    = "";

				kDebugLog (3, "  OCI8: first row was already fetched by OCIStmtExecute()");
			}
			else
			{
				// make sure SQL NULL values get left as zero-terminated C strings:
				for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
					m_dColInfo[ii].dszValue[0] = 0;

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = OCIStmtFetch ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("BufferResults::OCIStmtFetch"))
				{
					kDebugLog (3, "OCIStmtFetch() says we're done");
					break; // end of data from select
				}
			}

			++m_iNumRowsBuffered;

			for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
			{
				if (strlen(m_dColInfo[ii].dszValue) > 50)
					kDebugLog (3, "  buffered: row[{}]col[{}]: strlen()={}", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue));
				else
					kDebugLog (3, "  buffered: row[{}]col[{}]: '{}'", m_iNumRowsBuffered, ii+1, m_dColInfo[ii].dszValue);

				// we need to assure that no newlines are in buffered data:
				char* spot = strchr (m_dColInfo[ii].dszValue, '\n');
				while (spot)
				{
					*spot = 2; // <-- change them to ^B
					spot = strchr (spot+1, '\n');
				}
				fprintf (fp, "{}|{}|{}\n", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue));
				fprintf (fp, "{}\n", m_dColInfo[ii].dszValue ? m_dColInfo[ii].dszValue : "");
			}
		}
		break;

	// - - - - - - - - - - - - - - - - -
	case API_OCI6:
	// - - - - - - - - - - - - - - - - -
		while (1)
		{
			kDebugLog (3, "calling ofetch() to grab the next row...");
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			m_iErrorNum = ofetch ((Cda_Def*)m_dOCI6ConnectionDataArea);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			if (!WasOCICallOK("BufferResults:ofetch"))
			{
				kDebugLog (3, "ofetch() says we're done");
				break; // end of data from select
			}

			++m_iNumRowsBuffered;

			// map SQL NULL values to NULL-terminated C strings:
			for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
			{
				if (m_dColInfo[ii].indp < 0)
					m_dColInfo[ii].dszValue[0] = 0;

				if (strlen(m_dColInfo[ii].dszValue) > 50)
					kDebugLog (3, "  buffered: row[{}]col[{}]: strlen()={}", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue));
				else
					kDebugLog (3, "  buffered: row[{}]col[{}]: '{}'", m_iNumRowsBuffered, ii+1, m_dColInfo[ii].dszValue);

				// we need to assure that no newlines are in buffered data:
				char* spot = strchr (m_dColInfo[ii].dszValue, '\n');
				while (spot)
				{
					*spot = 2; // <-- change them to ^B
					spot = strchr (spot+1, '\n');
				}
				fprintf (fp, "{}|{}|{}\n", m_iNumRowsBuffered, ii+1, strlen(m_dColInfo[ii].dszValue));
				fprintf (fp, "{}\n", m_dColInfo[ii].dszValue ? m_dColInfo[ii].dszValue : "");
			}
		}
		break;
	#endif

	// - - - - - - - - - - - - - - - - -
	case API_CTLIB:
	case API_DBLIB:
	case API_INFORMIX:
	case API_DEKAF2_HAS_ODBC:
	default:
	// - - - - - - - - - - - - - - - - -
		kWarning ("[{}] KSQL:BufferResults(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
		kCrashExit (CRASHCODE_DEKAFUSAGE);
	}
	
	m_bpBufferedResults = fopen (m_sTmpResultsFile.c_str(), "r");
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
		mysql_free_result ((MYSQL_RES*)m_dMYSQLResult);
		m_dMYSQLResult = NULL;
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
	kDebugLog (3, "[{}]bool KSQL::NextRow()...", m_iDebugID);

	if (!QueryStarted())
	{
		m_sLastError.Format ("{}NextRow(): next row called, but no ExecQuery() was started.", m_sErrorPrefix);
		return (SQLError());
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// F_BufferResults: results were placed in a tmp file
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (IsFlag(F_BufferResults))
	{
		kDebugLog (3, "[{}]NextRow(): fetching buffered row...", m_iDebugID);

		if (!m_bFileIsOpen)
		{
			m_sLastError.Format ("{}NextRow(): BUG: results are supposed to be buffered but m_bFileIsOpen says false.", m_sErrorPrefix);
			return (SQLError());
		}
		else if (!m_bpBufferedResults)
		{
			m_sLastError.Format ("{}NextRow(): BUG: results are supposed to be buffered but m_bpBufferedResults is NULL!!", m_sErrorPrefix);
			return (SQLError());
		}

		FreeBufferedColArray (/*fValuesOnly=*/true); // <-- from last row

		if (!m_dBufferedColArray)
		{
			m_dBufferedColArray = (char**) kmalloc (m_iNumColumns * sizeof(char*), "KSQL:NextRow()"); // <-- array of char* ptrs
			for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
				m_dBufferedColArray[ii] = NULL;
		}

		++m_iRowNum;

		char szStatLine[25+1];
		for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
		{
			// first line is a sanity check and the strlen() of data size:
			bool fOK = (fgets (szStatLine, 25+1, m_bpBufferedResults) != NULL);

			if (!fOK && !ii)
			{
				kDebugLog (3, "NextRow(): end of buffered results: fgets() returned NULL");
				return (false);  // <-- no more rows
			}

			else if (!fOK)
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
			KSTACK Parts;
			if (kParseDelimedList (szStatLine, Parts, '|', /*fTrim=*/false) != 3)
			{
				m_sLastError.Format ("{}NextRow(): buffered results corrupted stat line for row={}, col={}: '{}'", m_sErrorPrefix, 
					(uint64_t)m_iRowNum, ii+1, szStatLine);
				return (SQLError());
			}

			uint64_t iCheckRowNum = Parts.Get(0).UInt64();
			uint64_t iCheckColNum = Parts.Get(1).UInt64();
			uint64_t iDataLen     = Parts.Get(2).UInt64();

			// sanity check the row and col (if we somehow got out of synch, all hell would break loose):
			if ((iCheckRowNum != m_iRowNum) || (iCheckColNum != ii+1))
			{
				if (IsFlag(F_IgnoreSQLErrors)) {
					kDebugLog (GetDebugLevel(), "[{}] NextRow(): CheckRowNum = {} [should be {}]", m_iDebugID, iCheckRowNum, m_iRowNum);
					kDebugLog (GetDebugLevel(), "[{}] NextRow(): CheckColNum = {} [should be {}]", m_iDebugID, iCheckColNum, ii+1);
				}
				else {
					kWarning ("[{}] NextRow(): CheckRowNum = {} [should be {}]", m_iDebugID, iCheckRowNum, m_iRowNum);
					kWarning ("[{}] NextRow(): CheckColNum = {} [should be {}]", m_iDebugID, iCheckColNum, ii+1);
				}
				m_sLastError.Format ("{}NextRow(): buffered results stat line out of sync for row={}, col={}", m_sErrorPrefix, 
					(uint64_t)m_iRowNum, ii+1);
				return (SQLError());
			}

			if (iDataLen > 50) {
				kDebugLog (3, "  from-buffer: row[{}]col[{}]: strlen()={}", m_iRowNum, ii+1, iDataLen);
			}
			else {
				kDebugLog (3, "  from-buffer: row[{}]col[{}]: '{}'", m_iRowNum, ii+1, szStatLine);
			}

			// now we know how big the data value is, so we can malloc for it:
			// fyi: tack on a few bytes for newline and NIL...
			m_dBufferedColArray[ii] = (char*) kmalloc (iDataLen+3, "KSQL:NextRow");

			// second line is the data value (could be one HUGE line of gobbly gook):
			fOK = (fgets (m_dBufferedColArray[ii], iDataLen+2, m_bpBufferedResults) != NULL);

			// sanity check: there should be a newline at exactly value[iDataLen]:
			if (!fOK || ((m_dBufferedColArray[ii])[iDataLen] != '\n'))
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

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// normal operation: return results in real time:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	else
	{
		kDebugLog (3, "[{}]NextRow(): fetching row...", m_iDebugID);

		ResetErrorStatus ();

		switch (m_iAPISet)
		{
        #ifdef DEKAF2_HAS_MYSQL
		// - - - - - - - - - - - - - - - - -
		case API_MYSQL:
		// - - - - - - - - - - - - - - - - -
			kDebugLog (3, "mysql_fetch_row()...");

			m_MYSQLRow = mysql_fetch_row ((MYSQL_RES*)m_dMYSQLResult);
			if (m_MYSQLRow != NULL)
				++m_iRowNum;
	
			if (!m_MYSQLRow)
			{
				kDebugLog (3, "[{}]NextRow(): {} row{} fetched (end was hit)", m_iDebugID, m_iRowNum, (m_iRowNum==1) ? " was" : "s were");
			}
			else
			{
				kDebugLog (3, "[{}]NextRow(): mysql_getch_row gave us row {}", m_iDebugID, m_iRowNum);
			}

			return (m_MYSQLRow != NULL);
		#endif

        #ifdef DEKAF2_HAS_ORACLE
		// - - - - - - - - - - - - - - - - -
		case API_OCI8:
		// - - - - - - - - - - - - - - - - -
			if (!m_iRowNum)
			{
				if (m_iOCI8FirstRowStat == OCI_NO_DATA)
				{
					kDebugLog (3, "OCIStmtExecute() said we were done");
					return (false); // end of data from select
				}

				// OCI8: first row was already (implicitly) fetched by OCIStmtExecute() [yuk]
				m_iErrorNum     = m_iOCI8FirstRowStat;
				m_sLastError    = "";

				kDebugLog (3, "OCI8: first row was already fetched by OCIStmtExecute()");
			}
			else
			{
				// make sure SQL NULL values get left as zero-terminated C strings:
				for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
					m_dColInfo[ii].dszValue[0] = 0;

				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				m_iErrorNum = OCIStmtFetch ((OCIStmt*)m_dOCI8Statement, (OCIError*)m_dOCI8ErrorHandle, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
				//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
				if (!WasOCICallOK("NextRow:OCIStmtFetch"))
				{
					kDebugLog (3, "OCIStmtFetch() says we're done");
					return (false); // end of data from select
				}
				kDebugLog (3, "OCIStmtFetch() says we got row # {}", m_iRowNum+1);
			}

			++m_iRowNum;

			return (true);

		// - - - - - - - - - - - - - - - - -
		case API_OCI6:
		// - - - - - - - - - - - - - - - - -
		{
			kDebugLog (3, "calling ofetch() to grab the next row...");

			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			m_iErrorNum = ofetch ((Cda_Def*)m_dOCI6ConnectionDataArea);
			//  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
			if (!WasOCICallOK("NextRow:ofetch"))
			{
				kDebugLog (3, "ofetch() says we're done");
				return (false); // end of data from select
			}

			++m_iRowNum;

			kDebugLog (3, "ofetch() says we got row # {}", m_iRowNum);

			// map SQL NULL values to zero-terminated C strings:
			for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
			{
				if (m_dColInfo[ii].indp < 0)
				{
					kDebugLog (3, "  fyi: row[{}]col[{}]: NULL value from database changed to NIL (\"\")", 
							m_iRowNum, m_dColInfo[ii].szColName);
					m_dColInfo[ii].dszValue[0] = 0;
				}
			}

			return (true);
		}
		#endif
	
		#ifdef CTLIBCOMPILE
		// - - - - - - - - - - - - - - - - -
		case API_CTLIB:
		// - - - - - - - - - - - - - - - - -
			return (ctlib_nextrow());
			break;
		#endif

		// - - - - - - - - - - - - - - - - -
		case API_DBLIB:
		case API_INFORMIX:
		case API_DEKAF2_HAS_ODBC:
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("[{}] KSQL:NextRow(): unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
		}
	}

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
		kDebugLog (3, "[{}]   data: got row {}, now loading property sheet with {} column values...", m_iDebugID, m_iRowNum, GetNumCols());
	}
	else
	{
		kDebugLog (3, "  data: no more rows.");
	}

	// load up a property sheet so we can lookup values by column name:
	// (we can do this even if we didn't get a row back)
	for (uint32_t ii=1; ii <= GetNumCols(); ++ii) {
		COLINFO* pInfo = GetColProps (ii);
		Row.Add (pInfo->szColName, bGotOne ? Get (ii, fTrimRight) : "");
	}

	return (bGotOne);

} // NextRow 

//-----------------------------------------------------------------------------
void KSQL::FreeBufferedColArray (bool fValuesOnly/*=false*/)
//-----------------------------------------------------------------------------
{
	//kDebugLog (3, "[{}]void KSQL::FreeBufferedColArray({})...", m_iDebugID, m_dBufferedColArray);

	// FYI: the m_dBufferedColArray is only used with the F_BufferResults flag
	// FYI: there is nothing database specific in this member function

	if (!m_dBufferedColArray)
		return;

	//kDebugMemory (3, (const char*)m_dBufferedColArray, 0);

	for (uint32_t ii=0; ii<m_iNumColumns; ++ii)
	{
		if (m_dBufferedColArray[ii])
		{
			//kDebugLog (3, "  free(m_dBufferedColArray[ii]= {}[{}] = {})", m_dBufferedColArray, ii, m_dBufferedColArray+ii);
			kfree (m_dBufferedColArray[ii], "KSQL:FreeBufferedColArray[ii]");  // <-- frees the result of strdup() in NextRow()
			m_dBufferedColArray[ii] = NULL;
		}
	}

	if (fValuesOnly)
		return;

	//kDebugLog (3, "  free(m_dBufferedColArray = {})", m_dBufferedColArray);
	kfree (m_dBufferedColArray, "KSQL:m_dBufferedColArray");  // <-- frees the result of malloc() in NextRow()

	m_dBufferedColArray = NULL;

} // FreeBufferedColArray

//-----------------------------------------------------------------------------
int64_t KSQL::SingleIntRawQuery (KStringView sSQL, uint64_t iFlags/*=0*/, KStringView sAPI/*="SingleIntRawQuery"*/)
//-----------------------------------------------------------------------------
{
	EndQuery ();
	if (!OpenConnection())
		return (false);

	uint64_t iHold = GetFlags();
	m_iFlags |= F_IgnoreSQLErrors;

	bool fOK = ExecRawQuery (sSQL, 0, sAPI);

	m_iFlags = iHold;

	if (!fOK) {
		kDebugLog (GetDebugLevel(), "[{}]{}: sql error, so we return -1", m_iDebugID, sAPI);
		return (-1);
	}

	if (!NextRow())
	{
		kDebugLog (GetDebugLevel(), "[{}]{}: expected one row back and didn't get it, so we return -1", m_iDebugID, sAPI);
		EndQuery();
		return (-1);
	}

	int64_t iValue = Get (1).Int64();
	kDebugLog (GetDebugLevel(), "[{}]{}: got {}\n", m_iDebugID, sAPI, iValue);

	EndQuery();
	return (iValue);

} // SingleIntRawQuery

//-----------------------------------------------------------------------------
bool KSQL::ResetBuffer ()
//-----------------------------------------------------------------------------
{
	kDebugLog (GetDebugLevel(), "[{}]ResetBuffer", m_iDebugID);

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
			fclose (m_bpBufferedResults);
		m_bpBufferedResults = NULL;
		m_bFileIsOpen = false;
	}

	m_bpBufferedResults = fopen (m_sTmpResultsFile.c_str(), "r");
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
void KSQL::EndQuery ()
//-----------------------------------------------------------------------------
{
	if (!m_bQueryStarted)
		return;

	kDebugLog (3, "  [{}]void KSQL::EndQuery()...", m_iDebugID);

	if (m_bQueryStarted)
	{
		kDebugLog (GetDebugLevel()+1, "  [{}]EndQuery: {} row{} fetched.", m_iDebugID, m_iRowNum, (m_iRowNum==1) ? " was" : "s were");
	}

    #ifdef DEKAF2_HAS_MYSQL
	// - - - - - - - - - - - - - - - - - - - - - - - -
	// MYSQL end of query cleanup:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_dMYSQLResult)
	{
		mysql_free_result ((MYSQL_RES*)m_dMYSQLResult);
		m_dMYSQLResult = NULL;
	}
	#endif

	if (m_dColInfo)
	{
		for (uint32_t ii=0; ii<m_iNumColumns; ++ii) {
			if (m_dColInfo[ii].dszValue)
				kfree (m_dColInfo[ii].dszValue, "KSQL:EndQuery:m_dColInfo[ii].dszValue");
		}
		kfree (m_dColInfo, "KSQL:EndQuery:m_dColInfo");
		m_dColInfo = NULL;
        #ifdef DEKAF2_HAS_ORACLE
		if (m_dOCI6ConnectionDataArea)
			ocan ((Cda_Def*)m_dOCI6ConnectionDataArea);  // <-- let OIC6 cursor free it's resources
		#endif
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// my own results buffering cleanup
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_bFileIsOpen)
	{
		kDebugLog (3, "  [{}] fclose (m_bpBufferedResults)...", m_iDebugID);
		if (m_bpBufferedResults) {
			fclose (m_bpBufferedResults);
			m_bpBufferedResults = NULL;
		}
		m_bFileIsOpen = false;
	}

	if (kFileExists (m_sTmpResultsFile))
		kRemoveFile (m_sTmpResultsFile);

	m_iRowNum           = 0;
	m_iNumRowsBuffered  = 0;
	m_iOCI8FirstRowStat = 0;

	FreeBufferedColArray ();

	m_bQueryStarted = false;

} // KSQL::EndQuery

//-----------------------------------------------------------------------------
KSQL::COLINFO* KSQL::GetColProps (uint32_t iOneBasedColNum)
//-----------------------------------------------------------------------------
{
	static bool    s_fOnce = false;
	static COLINFO s_NullResult;

	if (!s_fOnce) {
		memset (&s_NullResult, 0, sizeof(COLINFO));
		s_fOnce = true;
	}

	kDebugLog (3, "[{}]char* KSQL::GetColProps()...", m_iDebugID);

	if (!QueryStarted())
	{
		m_sLastError.Format ("{}{}() called, but no ExecQuery() was started.", m_sErrorPrefix, "GetColProps");
		SQLError();
		return (&s_NullResult);
	}

	if (iOneBasedColNum <= 0) {
		m_sLastError.Format ("{}{}() called with ColNum < 1 ({})", m_sErrorPrefix, "GetColProps", iOneBasedColNum);
		return (&s_NullResult);
	}

	else if (iOneBasedColNum > m_iNumColumns) {
		m_sLastError.Format ("{}{}() called with ColNum > {} ({})", m_sErrorPrefix, "GetColProps",  
			m_iNumColumns, iOneBasedColNum);
		return (&s_NullResult);
	}

	else if (!m_dColInfo) {
		m_sLastError.Format ("{}{}() called but m_dColInfo is NULL", m_sErrorPrefix, "GetColProps");
		return (&s_NullResult);
	}

	return (m_dColInfo + (iOneBasedColNum-1));

} // GetColProps

//-----------------------------------------------------------------------------
KStringView KSQL::Get (uint32_t iOneBasedColNum, bool fTrimRight/*=true*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]char* KSQL::Get()...", m_iDebugID);

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

	if (iOneBasedColNum <= 0)
		return ("ERR:UNDERFLOW-KSQL::Get()");

	else if (iOneBasedColNum > m_iNumColumns)
		return ("ERR:OVERFLOW-KSQL::Get()");

	else if (IsFlag (F_BufferResults))
	{
		if (fTrimRight)
			return (KASCII::ktrimright(m_dBufferedColArray[iOneBasedColNum-1]));
		else
			return (m_dBufferedColArray[iOneBasedColNum-1]);
	}

	else
	{
		char *szRtnValue = (char*)"";

		switch (m_iAPISet)
		{
        #ifdef DEKAF2_HAS_MYSQL
		// - - - - - - - - - - - - - - - - -
		case API_MYSQL:
		// - - - - - - - - - - - - - - - - -
			szRtnValue = ((MYSQL_ROW)(m_MYSQLRow))[iOneBasedColNum-1];
			break;
		#endif

		// - - - - - - - - - - - - - - - - -
		case API_OCI8:
		case API_OCI6:
		case API_DBLIB:
		case API_CTLIB:
		// - - - - - - - - - - - - - - - - -
			szRtnValue = m_dColInfo[iOneBasedColNum-1].dszValue;
			break;
	
		// - - - - - - - - - - - - - - - - -
		case API_INFORMIX:
		case API_DEKAF2_HAS_ODBC:
		default:
		// - - - - - - - - - - - - - - - - -
			kWarning ("[{}] KSQL: unsupported API Set ({}={})", m_iDebugID, m_iAPISet, TxAPISet(m_iAPISet));
			kCrashExit (CRASHCODE_DEKAFUSAGE);
		}

		if (!szRtnValue) {
			szRtnValue = (char*)"";
		}

		if (strlen(szRtnValue) > 50) {
			kDebugLog (3, "  data: row[{}]col[{}]: strlen()={}", m_iRowNum, iOneBasedColNum, strlen(szRtnValue));
		}
		else {
			kDebugLog (3, "  data: row[{}]col[{}]: '{}'", m_iRowNum, iOneBasedColNum, szRtnValue);
		}

		if (fTrimRight) {
			return (KASCII::ktrimright(szRtnValue));
		}
		else {
			return (szRtnValue);
		}
	}

	return ("");

} // KSQL::Get

//-----------------------------------------------------------------------------
time_t KSQL::GetUnixTime (uint32_t iOneBasedColNum)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]time_t KSQL::UnixTime()...", m_iDebugID);

	// FYI: there is nothing database specific in this member function
	// (we get away with this by fetching all results as strings, then
	// using C routines to do data conversions)

	struct tm   TimeStruct;
	KString     sTmp (Get (iOneBasedColNum));
	const char* val = sTmp.c_str();

	if (!(*val) || KASCII::strmatch (val, "0") || KASCII::strmatchN (val, "00000")) {
		return (0);
	}
	else if (strstr (val, "ERR"))
	{
		m_sLastError.Format ("{}IntValue(row={},col={}): {}", m_sErrorPrefix, m_iRowNum, iOneBasedColNum+1, val);
		SQLError();
		return (0);
	}
 
	#ifndef DO_NOT_HAVE_STRPTIME

	int iSecs;
	if (sscanf (val, "%*04d-%*02d-%*02d %*02d:%*02d:%02d", &iSecs) == 1) // e.g. "1965-03-31 12:00:00"
	{
		strptime (val, "%Y-%m-{} %H:%M:{}", &TimeStruct);
	}
	else if (strlen(val) == strlen ("20010302213436")) // e.g. "20010302213436"  which means 2001-03-02 21:34:36
	{
		strptime (val, "%Y%m{}%H%M{}", &TimeStruct);
	}
	else
	{
		m_sLastError.Format ("{}UnixTime({}): expected '{}' to look like '20010302213436' or '2001-03-21 06:18:33'", m_sErrorPrefix,
			iOneBasedColNum, val);
		SQLError();
		return (0);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// mktime() will fill in the missing info in the time struct and
	// return the unix time (uint64_t):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	time_t iUnixTime = mktime (&TimeStruct);

	#else
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// this was a noble effort to compute all this from scratch and it
	// worked, but I did this because I could not find the mktime() API
	// which handily converts a tm struct to a unix time (uint64_t):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	int Arr[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0 };
	TimeStruct.tm_yday = TimeStruct.tm_mday - 1;
	for (int ii=1; ii <= TimeStruct.tm_mon; ++ii)
		TimeStruct.tm_yday += Arr[ii];

	int iSince1970 = TimeStruct.tm_year - 70;
	int iLeapYears = (TimeStruct.tm_yday > (31+28)) ? (iSince1970/4) : ((iSince1970-1)/4);

	time_t iUnixTime = 
		  (iSince1970 * 365 * 24 * 60 * 60)
		+ ((TimeStruct.tm_yday + 1) * 24 * 60 * 60)
		+ (iLeapYears    * 24 * 60 * 60)
		+ (TimeStruct.tm_hour * 60 * 60)
		+ (     5        * 60 * 60) // <-- hardcoded EST conversion
		+ (TimeStruct.tm_min * 60)
		+ (TimeStruct.tm_sec);
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	#endif

	//kDebugTmStruct ((char*)"KSQL:UnixTime()", &TimeStruct, /*iMinDebugLevel=*/2);
	kDebugLog (GetDebugLevel()+1, "  unixtime = {}", iUnixTime);

	return (iUnixTime);

} // KSQL::GetUnixTime

//-----------------------------------------------------------------------------
bool KSQL::SQLError (bool fForceError/*=false*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]bool KSQL::SQLError()...", m_iDebugID);

	// FYI: there is nothing database specific in this member function

	if (m_sLastError == "")
	{
		kDebugLog (GetDebugLevel(), "KSQL: something is amuck.  SQLError() called, but m_sLastError is empty.");
		m_sLastError.Format("unknown SQL error");
	}
	
	if (IsFlag(F_IgnoreSQLErrors) && !fForceError)
	{
		kDebugLog (GetDebugLevel(), "IGNORED: {}", m_sLastError);
	}
	else
	{			
		kWarning ("[{}] {}", m_iDebugID, m_sLastError);
		if (m_sLastSQL != "")
			kWarning ("[{}] {}", m_iDebugID, m_sLastSQL);
	}

	return (false);  // <-- return code still needs to be false, just no verbosity into the log

} // KSQL::SQLError

//-----------------------------------------------------------------------------
bool KSQL::WasOCICallOK (KStringView sContext)
//-----------------------------------------------------------------------------
{
	m_sLastError = "";
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
		if (GetAPISet() == API_OCI6)
		{
			if (m_dOCI6LoginDataArea && m_dOCI6ConnectionDataArea)
			{
				m_iErrorNum = ((Cda_Def*)m_dOCI6ConnectionDataArea)->rc;
				if (m_iErrorNum == 0)         // ORA-00000: no error
					m_iErrorNum = sqlca.sqlcode;

				m_sLastError.Format ("{}", m_sErrorPrefix);
				oerhms ((Lda_Def*)m_dOCI6LoginDataArea, ((Cda_Def*)m_dOCI6ConnectionDataArea)->rc, (text*)m_sLastError.c_str()+strlen(m_sLastError.c_str()), MAXLEN_ERR-strlen(m_sLastError.c_str()));
			}
			else
				m_sLastError.Format ("{}unknown error (m_dOCI6LoginDataArea not allocated yet)", m_sErrorPrefix);
		}
		else // API_OCI8
		{
			// Tricky:
			// One would think that after a successful OCI call, the OCI would always return OCI_SUCCESS.
			// However, sometimes Oracle returns OCI_ERROR and only upon calling OCIErrorGet() does it
			// occur that there was actually no error.

			if (m_dOCI8ErrorHandle)
			{
				sb4 sb4Stat;
				m_sLastError.Format ("{}", m_sErrorPrefix);
				OCIErrorGet (m_dOCI8ErrorHandle, 1, NULL, &sb4Stat, (text*)m_sLastError.c_str()+strlen(m_sLastError.c_str()), MAXLEN_ERR-strlen(m_sLastError.c_str()), OCI_HTYPE_ERROR);
				m_iErrorNum = sb4Stat;
			}
			else
				m_sLastError.Format ("{}unknown error (m_dOCI8ErrorHandler not allocated yet)", m_sErrorPrefix);
		}

		if (strchr (m_sLastError.c_str(), '\n'))
			*(strchr (m_sLastError.c_str(),'\n')) = 0;
		snprintf (m_sLastError+strlen(m_sLastError), MAXLEN_ERR, " [{}]", sContext);

		if ( (m_iErrorNum == 0)    // ORA-00000: no error
		  || (m_iErrorNum == 1405) // ORA-01405: fetched column value is NULL
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

		if ((m_iErrorNum < 0) && (GetAPISet() == API_OCI6) && m_dOCI6LoginDataArea)
		{
			// translate old convention into current error messages:
			m_iErrorNum = -m_iErrorNum;
			m_sLastError.Format ("{}", m_sErrorPrefix);
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
bool KSQL::SetAPISet (int iAPISet)
//-----------------------------------------------------------------------------
{
	if (IsConnectionOpen())
	{
		m_sLastError.Format ("{}SetAPISet(): connection is already open.", m_sErrorPrefix);
		return (SQLError());
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// explicit API selection:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	if (iAPISet)
	{
		m_iAPISet = iAPISet;
		kDebugLog (GetDebugLevel(), "APISet set to {} ({})", m_iAPISet, TxAPISet(m_iAPISet));
		return (true);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// check environment: $KSQL_APISET
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	int iEnv = atoi (kGetEnv ("KSQL_APISET", "0"));
	if (iEnv)
	{
		m_iAPISet = iEnv;
		kDebugLog (GetDebugLevel(), "APISet (from $KSQL_APISET) set to {} ({})", m_iAPISet, TxAPISet(m_iAPISet));
		return (true);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// pick the default set of APIs, based on the database type:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	switch (m_iDBType)
	{

	case 0:
		m_sLastError.Format ("{}SetAPISet(): API mismatch: database type not set.", m_sErrorPrefix);
		return (false);

	case DBT_MYSQL:                m_iAPISet = API_MYSQL;       break;

	case DBT_ORACLE6:
	case DBT_ORACLE7:
	case DBT_ORACLE8:
	case DBT_ORACLE:               m_iAPISet = API_OCI8;        break;

	case DBT_SQLSERVER:
	case DBT_SYBASE:               m_iAPISet = API_CTLIB;       break; // choices: API_DBLIB -or- API_CTLIB

	case DBT_INFORMIX:             m_iAPISet = API_INFORMIX;    break;

	default:
		m_sLastError.Format ("{}SetAPISet(): unsupported database type ({}={})", m_sErrorPrefix, m_iDBType, TxDBType(m_iDBType));
		return (SQLError());

	}

	FormatConnectSummary ();

	return (true);

} // SetAPISet

//-----------------------------------------------------------------------------
bool KSQL::SetFlags (uint64_t iFlags)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]void KSQL::SetFlags()...", m_iDebugID);

	// FYI: there is nothing database specific in this member function

	if (QueryStarted())
	{
		m_sLastError.Format ("{}SetFlags(): you cannot change flags while a query is in process.", m_sErrorPrefix);
		SQLError();
		return (false);
	}

	m_iFlags = iFlags;

	return (true);

} // KSQL::SetFlags

//-----------------------------------------------------------------------------
KString KSQL::GetLastInfo()
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_HAS_MYSQL)
	return mysql_info(static_cast<MYSQL*>(m_dMYSQL));
#else
	return "";
#endif

} // KSQL::GeLastInfo

//-----------------------------------------------------------------------------
void KSQL::BuildTranslationList (KPROPS* pList, int iDBType/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]KSQL::BuildTranslationList()...", m_iDebugID);

	if (!iDBType)
		iDBType = m_iDBType;

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// build the translation array:
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	for (size_t jj=0; jj < std::extent<decltype(g_Translations)>::value; ++jj)
	{
		KStringView sName = g_Translations[jj].sOriginal;

		switch (iDBType)
		{
		case DBT_MYSQL:
			pList->Add (sName, g_Translations[jj].sMySQL);
			break;

		case DBT_ORACLE6:
		case DBT_ORACLE7:
			pList->Add (sName, g_Translations[jj].sOraclePre8);
			break;

		case DBT_ORACLE8:
		case DBT_ORACLE:
			pList->Add (sName, g_Translations[jj].sOracle);
			break;

		case DBT_SYBASE:
		case DBT_SQLSERVER:
			pList->Add (sName, g_Translations[jj].sSybase);
			break;

		case DBT_INFORMIX:
			pList->Add (sName, g_Translations[jj].sInformix);
			break;
		}
	}

	// non-database specific pairs:
	char szPID[10+1];  snprintf (szPID, 10, "%lld", (uint64_t)getpid());
	char szDID[10+1];  snprintf (szDID, 10, "%lld", (uint64_t)m_iDebugID);
	
	pList->Add ("PID",        szPID);
	pList->Add ("$$",         szPID);
	pList->Add ("DID",        szDID);
//	pList->Add ("hostname",   khostname());
	pList->Add ("P",          "+");
	pList->Add ("DC",         "{{");

} // BuildTranslationList

//-----------------------------------------------------------------------------
uint32_t ktx (KString& sTarget, KPROPS& List)
//-----------------------------------------------------------------------------
{
	uint32_t iNumReplacements = 0;
	for (auto& it : List)
	{
		KString sToken;
		sToken.Printf ("{{%s}}", it.first);
		
		iNumReplacements += sTarget.Replace(sToken, it.second, /*bReplaceAll=*/true);
	}
	
	return (iNumReplacements);
	
} // ktx

//-----------------------------------------------------------------------------
void KSQL::DoTranslations (KString& sSQL, int iDBType/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "[{}]KSQL::DoTranslations()...", m_iDebugID);
	kDebugLog (3, "KSQL:DoTranslations():");
	kDebugLog (3, "BEFORE:");
	kDebugLog (3, "{}", sSQL);

	if (!iDBType)
		iDBType = m_iDBType;

	if (!strstr (sSQL.c_str(), "{{"))
	{
		kDebugLog (3, " --> no SQL translations.");
		return;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// do the translations on the string (in place):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	//m_TxList.DebugPairs (2, "m_TxList:"); -- TODO
	ktx (sSQL, m_TxList);

	kDebugLog (3, "AFTER:");
	kDebugLog (3, "{}", sSQL);

} // DoTranslations

//-----------------------------------------------------------------------------
const char* KSQL::TxDBType (int iDBType) const
//-----------------------------------------------------------------------------
{
	switch (iDBType)
	{
	case 0:                 return ("NotInitialized");
	case DBT_MYSQL:         return ("MySQL");
	case DBT_ORACLE6:       return ("Oracle6");
	case DBT_ORACLE7:       return ("Oracle7");
	case DBT_ORACLE8:       return ("Oracle8");
	case DBT_ORACLE:        return ("Oracle");
	case DBT_SQLSERVER:     return ("SQLServer");
	case DBT_SYBASE:        return ("Sybase");
	case DBT_INFORMIX:      return ("Informix");
	default:                return ("MysteryType");
	}

} // TxDBType

//-----------------------------------------------------------------------------
const char* KSQL::TxAPISet (int iAPISet) const
//-----------------------------------------------------------------------------
{
	switch (iAPISet)
	{
	case 0:                 return ("NotInitialized");
	case API_MYSQL:         return ("mysql");
	case API_OCI6:          return ("oci6");
	case API_OCI8:          return ("oci8");
	case API_DBLIB:         return ("dblib");
	case API_CTLIB:         return ("ctlib");
	case API_INFORMIX:      return ("ifx");
	default:                return ("MysteryAPIs");
	}

} // TxAPISet

//-----------------------------------------------------------------------------
void KSQL::FormatConnectSummary ()
//-----------------------------------------------------------------------------
{
	m_sConnectSummary = "";

	switch (m_iDBType)
	{
	case DBT_ORACLE6:
	case DBT_ORACLE7:
	case DBT_ORACLE:
		// - - - - - - - - - - - - - -
		// fred@orcl [Oracle]
		// - - - - - - - - - - - - - -
		m_sConnectSummary += m_sUsername;
		m_sConnectSummary += "@";
		m_sConnectSummary += m_sHostname;
		if (GetDBPort()) {
			m_sConnectSummary += ":";
			m_sConnectSummary += GetDBPort();
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
			if (GetDBPort()) {
				m_sConnectSummary += ":";
				m_sConnectSummary += GetDBPort();
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
	case KSQL::DBT_MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// NONE OF THE ARGS: sLike, fIncludeViews and fRestrictToMine are supported for MySQL.
		// fIncludeViews:   MySQL supports VIEWS and they appear in the "show table status" results with type=NULL
		// fRestrictToMine: the MySQL "show table status" command only does exactly that
		if (sLike != KStringView("%")) {
			kDebugLog (GetDebugLevel(), "warning: KSQL::ListTables({}): MySQL 'show table status' does not support arguments", sLike);
		}
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool fOK = ExecQuery ("show table status");
			//SetFlags (iFlags);
			return (fOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case KSQL::DBT_ORACLE7:
	case KSQL::DBT_ORACLE8:
	case KSQL::DBT_ORACLE:
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
	case DBT_SQLSERVER:
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
	case KSQL::DBT_MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// NONE OF THE ARGS: sLike and fRestrictToMine are supported for MySQL.
		// fRestrictToMine: the MySQL "show procedure status" command only does exactly that
		if (sLike != KStringView("%")) {
			kDebugLog (GetDebugLevel(), "warning: KSQL::ListProcedures({}): MySQL 'show procedure status' does not support arguments", sLike);
		}
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool fOK = ExecQuery ("show procedure status");
			//SetFlags (iFlags);
			return (fOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// case KSQL::DBT_ORACLE7:
	// case KSQL::DBT_ORACLE8:
	// case KSQL::DBT_ORACLE:
	// - - - - - - - - - - - - - - - - - - - - - - - -

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// case DBT_SQLSERVER:
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
	case KSQL::DBT_MYSQL:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		// desc: ColName, Datatype, Null, Key, Default, Extra
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool fOK = ExecQuery ("desc {}", sTablename);
			//SetFlags (iFlags);
			return (fOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case KSQL::DBT_ORACLE7:
	case KSQL::DBT_ORACLE8:
	case KSQL::DBT_ORACLE:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		{
		char szSchemaOwner[50+1];
		kstrncpy (szSchemaOwner, KString(GetDBUser()).c_str(), 50+1);
		KASCII::ktoupper (szSchemaOwner);
		return (ExecQuery (
			"select column_name, data_type, nullable, data_length, data_precision, ''\n"
			"  from DBA_TAB_COLUMNS\n"
			" where owner = '{}'\n"
			"   and table_name = '{}'\n"
			" order by column_id", 
				szSchemaOwner, sTablename));
		}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	case KSQL::DBT_SQLSERVER:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		{
			//uint32_t iFlags = GetFlags();
			EndQuery ();
			SetFlags (F_IgnoreSelectKeyword);
			bool fOK = ExecQuery ("sp_help {}", sTablename);
			//SetFlags (iFlags);
			return (fOK);
		}
		break;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	default:
	// - - - - - - - - - - - - - - - - - - - - - - - -
		m_sLastError.Format ("{}DescribeTable(): {} not supported yet.", m_sErrorPrefix, TxDBType(m_iDBType));
		return (SQLError());
	}

} // DescribeTable

#if 0
//-----------------------------------------------------------------------------
unsigned char* KSQL::EncodeData (unsigned char* sBlobData, int iBlobType, uint64_t iBlobDataLen/*=0*/, bool fInPlace/*=false*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "EncodeData()...");

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// NOTE: this routine returns NEW MEMORY (upon success) 
	// which needs to be freed (unless BlobType=BT_ASCII and fInPlace=true):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	unsigned char* dszEncodedBlob = NULL;
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
			return (NULL);
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
			kDebugLog (GetDebugLevel(), "EncodeData(ASCII): '{}' --> '{}'", sBlobData, dszEncodedBlob);

		return (dszEncodedBlob); // <-- new memory

	// - - - - - - - - - -
	case BT_BINARY:
	// - - - - - - - - - -
		if (!iBlobDataLen)
		{
			m_sLastError.Format ("{}EncodeData(): BlobType is BINARY, but iBlobDataLen not specified", m_sErrorPrefix);
			SQLError();
			return (NULL);
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
			kDebugLog (GetDebugLevel(), "EncodeData(BINARY): '{}' --> '{}'", sBlobData, dszEncodedBlob);

		return (dszEncodedBlob); // <-- new memory
	}	

	m_sLastError.Format ("{}EncodeData(): unsupported BlobType={}", m_sErrorPrefix, iBlobType);
	SQLError();
	return (NULL); // <-- failed (no new memory to free)

} // EncodeData

//-----------------------------------------------------------------------------
unsigned char* KSQL::DecodeData (unsigned char* sBlobData, int iBlobType, uint64_t iEncodedLen/*=0*/, bool fInPlace/*=false*/)
//-----------------------------------------------------------------------------
{
	//#ifdef WIN32
	//iDebugLevel = (iDebugLevel) ? 3 : 0;
	//#endif

	kDebugLog (GetDebugLevel(), "DecodeData()...");

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// NOTE: this routine returns NEW MEMORY (upon success) 
	// which needs to be freed (unless BlobType=BT_ASCII and fInPlace=true):
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	unsigned char* dszDecodedBlob = NULL;
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
			return (NULL);
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
			kDebugLog (GetDebugLevel(), "DecodeData(ASCII): '{}' --> '{}'", sBlobData, dszDecodedBlob);

		return (dszDecodedBlob); // <-- new memory

	// - - - - - - - - - -
	case BT_BINARY:
	// - - - - - - - - - -
		if (!iEncodedLen)
		{
			m_sLastError.Format ("{}DecodeData(): BlobType is BINARY, but iEncodedLen not specified", m_sErrorPrefix);
			SQLError();
			return (NULL);
		}

		dszDecodedBlob = (unsigned char*) kmalloc ((iEncodedLen+1) / 2, "KSQL:dszDecodedBlob");
		for (ii=0, jj=0; ii<iEncodedLen; ii+=2)
		{
			unsigned char szHexPair[2+1];
			szHexPair[0] = sBlobData[ii+0];
			szHexPair[1] = sBlobData[ii+1];
			szHexPair[2] = 0;

			// sanity check:
			bool fOK1 = false;
			bool fOK2 = false;

			if (!szHexPair[0])
				fOK1 = true;
			else if ((szHexPair[0] >= '0') && (szHexPair[0] <= '9'))
				fOK1 = true;
			else if ((szHexPair[0] >= 'a') && (szHexPair[0] <= 'f'))
				fOK1 = true;
			else if ((szHexPair[0] >= 'A') && (szHexPair[0] <= 'F'))
				fOK1 = true;

			if (!szHexPair[1])
				fOK2 = true;
			else if ((szHexPair[1] >= '0') && (szHexPair[1] <= '9'))
				fOK2 = true;
			else if ((szHexPair[1] >= 'a') && (szHexPair[1] <= 'f'))
				fOK2 = true;
			else if ((szHexPair[1] >= 'A') && (szHexPair[1] <= 'F'))
				fOK2 = true;

			if (!fOK1 || !fOK2)
			{
				kDebugLog (GetDebugLevel(), "[{}] KSQL:DecodeData(): corrupted hex pair in encoded data:", m_iDebugID);
				kDebugLog (GetDebugLevel(), "  szHexPair[{}+{}] = %3d ({})", ii, 0, szHexPair[0], fOK1 ? "valid hex digit" : "INVALID HEX DIGIT");
				kDebugLog (GetDebugLevel(), "  szHexPair[{}+{}] = %3d ({})", ii, 1, szHexPair[1], fOK2 ? "valid hex digit" : "INVALID HEX DIGIT");
				kDebugLog (GetDebugLevel(), "  EncodedLen={}", iEncodedLen);
				kDebugLog (GetDebugLevel(), "  sBlobData     = {} (memory dump to follow)...", sBlobData);
				kDebugLog (GetDebugLevel(), "  sBlobData+Len = {}", sBlobData+iEncodedLen);

				kDebugMemory ((const char*) sBlobData, 0, /*iLinesBefore=*/0, /*iLinesAfter=*/iEncodedLen/8);

				m_sLastError.Format ("{}DecodeData(): corrupted blob (details in klog)", m_sErrorPrefix);
				SQLError();
				return (NULL);
			}

			int iValue = 0;
			sscanf ((char*)szHexPair, "%x", &iValue);

			kDebugLog (GetDebugLevel()+1, "  HexPair[%04lu/%04lu]: '{}' ==> %03d ==> '%c'", ii, iEncodedLen, szHexPair, iValue, iValue);

			dszDecodedBlob[jj++] = iValue;
		}
		dszDecodedBlob[jj] = 0;

		return (dszDecodedBlob); // <-- new memory
	}	

	m_sLastError.Format ("{}DecodeData(): unsupported BlobType={}", m_sErrorPrefix, iBlobType);
	SQLError();
	return (NULL); // <-- failed (no new memory to free)

} // DecodeData

//-----------------------------------------------------------------------------
bool KSQL::PutBlob (KStringView sBlobTable, KStringView sBlobKey, unsigned char* sBlobData, int iBlobType, uint64_t iBlobDataLen/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "PutBlob()...");

	if (!sBlobTable || !sBlobKey || !sBlobData)
	{
		m_sLastError.Format ("{}PutBlob(): BlobTable, BlobKey or BlobData is NULL", m_sErrorPrefix);
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

	default:
		m_sLastError.Format ("KSQL:DecodeData(): unsupported BlobType={}", iBlobType);
		return (SQLError());
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
		return (true);  // <-- nothing to insert, string is NIL

	unsigned char* dszEncodedBlob = EncodeData (sBlobData, iBlobType, iBlobDataLen); // malloc
	unsigned char  szChunk[MAX_BLOBCHUNKSIZE+1];
	unsigned char* sSpot        = dszEncodedBlob;
	int    iChunkNum      = 0;

	while (sSpot && *sSpot)
	{
		++iChunkNum;

		uint64_t iAmountLeft         = strlen((const char*)sSpot);
		uint64_t iEncodedLenChunk    = (iAmountLeft < MAXLEN) ? iAmountLeft : MAXLEN;
		uint64_t iDataLenChunk       = iEncodedLenChunk / ((iBlobType==BT_ASCII) ? 1 : 2);
		memcpy (szChunk, sSpot, iEncodedLenChunk);
		szChunk[iEncodedLenChunk] = 0;

		kDebugLog (GetDebugLevel(), "[{}]PutBlob(): chunk '{}', part[{:02}]: encoding={}, datasize=%04lu", m_iDebugID, sBlobKey, iChunkNum, iBlobType, iDataLenChunk);

		bool fOK = ExecSQL (
			"insert into {} (BlobKey, ChunkNum, Chunk, Encoding, EncodedSize, DataSize)\n"
			"values ('{}', {}, '{}', {}, {}, {})",
				sBlobTable,
				sBlobKey, iChunkNum, szChunk, iBlobType, iEncodedLenChunk, iDataLenChunk);

		if (!fOK)
			return (false);

		sSpot += iEncodedLenChunk;
	}

	if (dszEncodedBlob)
		free (dszEncodedBlob);

	return (true);

} // PutBlob

//-----------------------------------------------------------------------------
unsigned char* KSQL::GetBlob (KStringView sBlobTable, KStringView sBlobKey, uint64_t* piBlobDataLen/*=NULL*/)
//-----------------------------------------------------------------------------
{
	kDebugLog (3, "GetBlob()...");

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
		return (NULL); // db error

	kDebugLog (GetDebugLevel(), "KSQL:GetBlob(): expecting %lld bytes for BlobKey='{}'", iBlobDataLen, sBlobKey);

	if (!iBlobDataLen)
		return ((unsigned char*)strdup(""));  // <-- return a zero-length string that can be freed

	unsigned char* dszBlobData = (unsigned char*) kmalloc ((uint32_t)(iBlobDataLen+1), "KSQL:dszBlobData");
	unsigned char* sSpot     = dszBlobData;

	if (!ExecQuery ("select ChunkNum, Chunk, Encoding, EncodedSize, DataSize from {} where BlobKey='{}' order by ChunkNum", sBlobTable, sBlobKey))
		return (NULL);

	while (NextRow())
	{
		int         iChunkNum       = IntValue (1);
		KStringView sEncodedChunk   = (unsigned char*)Get (2, /*fTrimRight=*/false);
		int         iEncoding       = IntValue (3);
		uint64_t    iEncodedSize    = ULongValue (4);
		uint64_t    iDataSize       = ULongValue (5);

		kDebugLog (GetDebugLevel(), "[{}]GetBlob(): chunk '{}', part[{:02}]: encoding={}, datasize=%04lu", m_iDebugID, sBlobKey, iChunkNum, iEncoding, iDataSize);

		// 1. decode this chunk:
		kDebugLog (GetDebugLevel()+1, "KSQL:GetBlob(1): decoding this chunk...");
		unsigned char* dszChunk        = DecodeData (sEncodedChunk, iEncoding, iEncodedSize); // malloc

		// 2. add this chunk on to our return memory:
		kDebugLog (GetDebugLevel()+1, "KSQL:GetBlob(2): adding decoded chunk to Spot={} (BlobData={})",
				sSpot, dszBlobData);
		memcpy (sSpot, dszChunk, iDataSize);

		// 3. position the pointer:
		kDebugLog (GetDebugLevel()+1, "KSQL:GetBlob(3): moving Spot from {} to {}...",
				sSpot, sSpot + iDataSize);
		sSpot += iDataSize;

		if ((dszChunk != sEncodedChunk)
				&& (dszChunk != NULL))
		{
			kfree (dszChunk);
		}
	}

	kDebugLog (GetDebugLevel()+1, "KSQL:GetBlob(): checking result...");

	if (sSpot != (dszBlobData + iBlobDataLen))
	{
		kDebugLog (GetDebugLevel(), "[{}] GetBlob(): sanity check failed:", m_iDebugID);
		kDebugLog (GetDebugLevel(), "    dszBlobData   = {}", dszBlobData);
		kDebugLog (GetDebugLevel(), "  + iBlobDataLen  = + %8lld", iBlobDataLen);
		kDebugLog (GetDebugLevel(), "  --------------    ----------");
		kDebugLog (GetDebugLevel(), "                  = {}", dszBlobData + iBlobDataLen);
		kDebugLog (GetDebugLevel(), "    but sSpot   = {}", sSpot);
	}

	*sSpot = 0;                       // <-- terminate the blob (in case its ascii)

	if (piBlobDataLen)
	{
		kDebugLog (GetDebugLevel(), "KSQL:GetBlob(): return length set to %lld", iBlobDataLen);

		*piBlobDataLen = (uint64_t) iBlobDataLen;  // <-- for truly binary data, this data length is essential
	}
	else {
		kDebugLog (GetDebugLevel(), "KSQL:GetBlob(): return length (%lld) not passed back", iBlobDataLen);
	}

	kDebugLog (GetDebugLevel(), "KSQL:GetBlob(): return data pointer sBlobData = {}", dszBlobData);

	return (dszBlobData);               // <-- new memory which must be freed

} // GetBlob
#endif

#ifdef DEKAF2_HAS_ODBC
//------------------------------------------------------------------------------
bool KSQL::CheckDEKAF2_HAS_ODBC (RETCODE iRetCode)
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
			SQLError (m_Environment, m_hdbc, m_hstmt, szSQLState, NULL,
					  szErrorMsg, sizeof(szErrorMsg), NULL);
			m_sLastError.Format ("{}{}: {}", m_sErrorPrefix, szSQLState, szErrorMsg);
			#else
			m_sLastError.Format ("{}DEKAF2_HAS_ODBC Error: Code={}", m_sErrorPrefix, iRetCode);
			#endif
		}
		return (false);
	}

} // CheckDEKAF2_HAS_ODBC

#endif

#ifdef DEKAF2_HAS_ORACLE

//-----------------------------------------------------------------------------
bool KSQL::_BindByName (KStringView sPlaceholder, dvoid* pValue, sb4 iValueSize, ub2 iDataType)
//-----------------------------------------------------------------------------
{
	if (!m_bStatementParsed) {
		m_sLastError.Format ("{}BindByName(): ParseSQL() or ParseQuery() was not called yet.", m_sErrorPrefix);
		return (false);
	}

	if (m_idxBindVar >= MAXBINDVARS) {
		m_sLastError.Format ("{}BindByName(): too many bind vars (MAXBINDVARS={})", m_sErrorPrefix, MAXBINDVARS);
		return (false);
	}

	if (!sPlaceholder || !KASCII::strmatchN (sPlaceholder, ":")) {
		m_sLastError.Format ("{}BindByName(): invalid placeholder (should start with ':').", m_sErrorPrefix);
		return (false);
	}

	m_iErrorNum = OCIBindByName (
			(OCIStmt*)   m_dOCI8Statement,
			(OCIBind**)  &(m_OCIBindArray[m_idxBindVar++]),
			(OCIError*)  m_dOCI8ErrorHandle, 
			(CONST text*)sPlaceholder,
			(sb4)        strlen(sPlaceholder),
			(dvoid*)     pValue,
			(sb4)        iValueSize,
			(ub2)        iDataType,
			(dvoid*)     /*indp (IN/OUT)*/    NULL, // Pointer to an indicator variable or array.
			(ub2*)       /*alenp (IN/OUT)*/   NULL, // Pointer to array of actual lengths of array elements.
			(ub2*)       /*rcodep (OUT)*/     NULL, // Pointer to array of column level return codes.
			(ub4)        /*maxarr_len(IN)*/   0,    // The maximum possible number of elements
			(ub4)        /*curelep (IN/OUT)*/ 0,    // Pointer to the actual number of elements.
			(ub4)        /*mode (IN)*/        OCI_DEFAULT // OCI_DEFAULT | OCI_DATA_AT_EXEC
	);

	if (!WasOCICallOK("BindByName"))
	{
		kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
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
			return (false);
	}

	return (true);

} // BindByName

//-----------------------------------------------------------------------------
bool KSQL::_BindByPos (uint32_t iPosition, dvoid* pValue, sb4 iValueSize, ub2 iDataType)
//-----------------------------------------------------------------------------
{
	if (!m_bStatementParsed) {
		m_sLastError.Format ("{}BindByPos(): ParseSQL() or ParseQuery() was not called yet.", m_sErrorPrefix);
		return (false);
	}

	if (m_idxBindVar >= MAXBINDVARS) {
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
			(dvoid*)     /*indp (IN/OUT)*/    NULL, // Pointer to an indicator variable or array.
			(ub2*)       /*alenp (IN/OUT)*/   NULL, // Pointer to array of actual lengths of array elements.
			(ub2*)       /*rcodep (OUT)*/     NULL, // Pointer to array of column level return codes.
			(ub4)        /*maxarr_len(IN)*/   0,    // The maximum possible number of elements
			(ub4)        /*curelep (IN/OUT)*/ 0,    // Pointer to the actual number of elements.
			(ub4)        /*mode (IN)*/        OCI_DEFAULT // OCI_DEFAULT | OCI_DATA_AT_EXEC
	);

	if (!WasOCICallOK("BindByPos"))
	{
		kDebugLog (GetDebugLevel(), "[{}] {}", m_iDebugID, m_sLastError);
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
bool KSQL::Insert (KROW& Row)
//-----------------------------------------------------------------------------
{
	m_sLastSQL = "";
	
	if (!Row.FormInsert (m_sLastSQL, m_iDBType)) {
		m_sLastError = Row.GetLastError();
		return (false);
	}

	if (!IsFlag(F_NoTranslations))
		DoTranslations (m_sLastSQL, m_iDBType);

	return (ExecRawSQL (m_sLastSQL, 0, "Insert"));

} // Insert

//-----------------------------------------------------------------------------
bool KSQL::Update (KROW& Row)
//-----------------------------------------------------------------------------
{
	m_sLastSQL = "";

	if (!Row.FormUpdate (m_sLastSQL, m_iDBType)) {
		m_sLastError = Row.GetLastError();
		return (false);
	}

	if (!IsFlag(F_NoTranslations))
		DoTranslations (m_sLastSQL, m_iDBType);

	return (ExecRawSQL (m_sLastSQL, 0, "Update"));

} // Update

//-----------------------------------------------------------------------------
bool KSQL::Delete (KROW& Row)
//-----------------------------------------------------------------------------
{
	m_sLastSQL = "";

	if (!Row.FormDelete (m_sLastSQL, m_iDBType)) {
		m_sLastError.Format("{}", Row.GetLastError());
		return (false);
	}

	if (!IsFlag(F_NoTranslations))
		DoTranslations (m_sLastSQL, m_iDBType);

	return (ExecRawSQL (m_sLastSQL, 0, "Delete"));

} // Delete

//-----------------------------------------------------------------------------
bool KSQL::InsertOrUpdate (KROW& Row, bool* pbInserted/*=NULL*/)
//-----------------------------------------------------------------------------
{
	if (pbInserted) {
		*pbInserted = false;
	}

	kDebugLog (GetDebugLevel()+1, "InsertOrUpdate: trying update first...");
	if (!Update (Row)) {
		return (false); // syntax error in SQL
	}

	if (GetNumRowsAffected() >= 1)
	{
		return (true); // update caught something
	}

	kDebugLog (GetDebugLevel(), "InsertOrUpdate: update caught no rows: proceed to insert...");

	if (pbInserted) {
		*pbInserted = true;
	}

	kDebugLog (GetDebugLevel()+1, "InsertOrUpdate: attempting insert...");
	bool fOK = Insert (Row);

	return (fOK);

} // DbInsertOrUpdate 

//-----------------------------------------------------------------------------
uint64_t KSQL::GetLastInsertID ()
//-----------------------------------------------------------------------------
{
	if (m_iDBType == KSQL::DBT_SQLSERVER)
	{
		int64_t iID = SingleIntQuery ("select @@identity");
		if (iID <= 0) 
			m_iLastInsertID = 0;
		else
			m_iLastInsertID = iID;
	}

	return (m_iLastInsertID);

} // GetLastInsertID

#ifdef CTLIBCOMPILE
//-----------------------------------------------------------------------------
bool KSQL::ctlib_login ()
//-----------------------------------------------------------------------------
{
	m_sLastError = "";
	kDebugLog (CTDEBUG, "cs_ctx_alloc()");
	if (cs_ctx_alloc (CS_VERSION_100, &m_pCtContext) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_login>cs_ctx_alloc");
		return (SQLError());
	}

	#if 0
	/* Force default date format, some tests rely on it */
	TDSCONTEXT *tds_ctx;
	tds_ctx = (TDSCONTEXT *) (*ctx)->tds_ctx;
	if (tds_ctx && tds_ctx->locale && tds_ctx->locale->date_fmt) {
		free(tds_ctx->locale->date_fmt);
		tds_ctx->locale->date_fmt = strdup("%b %e %Y %I:%M%p");
	}
	#endif

	kDebugLog (CTDEBUG, "ct_init()");
	if (ct_init (m_pCtContext, CS_VERSION_100) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_login>ct_init failed");
		return (SQLError());
	}

	kDebugLog (CTDEBUG, "ct_con_alloc()");
	if (ct_con_alloc (m_pCtContext, &m_pCtConnection) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_login>ct_con_alloc");
		return (SQLError());
	}

	kDebugLog (2, "ct_con_props() set username");
	if (ct_con_props (m_pCtConnection, CS_SET, CS_USERNAME, GetDBUser(), CS_NULLTERM, NULL) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_login>set username");
		return (SQLError());
	}

	kDebugLog (2, "ct_con_props() set password");
	if (ct_con_props (m_pCtConnection, CS_SET, CS_PASSWORD, GetDBPass(), CS_NULLTERM, NULL) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_login>set password");
		return (SQLError());
	}

	kDebugLog (2, "ct_connect(): connecting as {} to {}.{}\n", m_sUsername, GetDBHost(), m_sDatabase);
	int iTryConnect = 0;
	while(true) {
		if (ct_connect (m_pCtConnection, GetDBHost(), CS_NULLTERM) == CS_SUCCEED) {
			break;
		}
		else if (++iTryConnect >= 3){ //try to connect 3 times.
			ctlib_api_error ("ctlib_login>ct_connect");
			return (SQLError());
		}
	}

	kDebugLog (CTDEBUG, "ct_cmd_alloc()");
	if (ct_cmd_alloc (m_pCtConnection, &m_pCtCommand) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_login>ct_cmd_alloc");
		return (SQLError());
	}

	const char* _dbname = GetDBName();
	if (*_dbname) {
		char szSQL[MAXLEN_PATH+1];
		snprintf (szSQL, MAXLEN_PATH, "use {}", _dbname);
		if (ctlib_execsql(szSQL) != CS_SUCCEED) {
			m_sLastError.Format ("{}CTLIB: login ok, could not attach to database: {}", m_sErrorPrefix, _dbname);
			return (false);
		}
	}

	kDebugLog (GetDebugLevel()+1, "ensuring that we can process error messages 'inline' instead of using callbacks...");

	kDebugLog (CTDEBUG, "ct_diag(CS_INIT)");
	if (ct_diag (m_pCtConnection, CS_INIT, CS_UNUSED, CS_UNUSED, NULL) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_login>ct_init(CS_INIT) failed");
		return (SQLError());
	}

	return (true);

} // ctlib_login

//-----------------------------------------------------------------------------
bool KSQL::ctlib_logout ()
//-----------------------------------------------------------------------------
{
	kDebugLog (CTDEBUG, "ctlib_logout: calling {}...", "ct_cancel");
	if (ct_cancel(m_pCtConnection, NULL, CS_CANCEL_ALL) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_logout>ct_cancel");
		return (SQLError());
	}

	kDebugLog (CTDEBUG, "ctlib_logout: calling {}...", "ct_cmd_drop");
	ct_cmd_drop (m_pCtCommand);

	kDebugLog (CTDEBUG, "ctlib_logout: calling {}...", "ct_close   ");
	ct_close    (m_pCtConnection, CS_UNUSED);

	kDebugLog (CTDEBUG, "ctlib_logout: calling {}...", "ct_con_drop");
	ct_con_drop (m_pCtConnection);

	kDebugLog (CTDEBUG, "ctlib_logout: calling {}...", "ct_exit    ");
	ct_exit     (m_pCtContext, CS_UNUSED);

	kDebugLog (CTDEBUG, "ctlib_logout: calling {}...", "cs_ctx_drop");
	cs_ctx_drop (m_pCtContext);

	return (true);

} // ctlib_logout

//-----------------------------------------------------------------------------
bool KSQL::ctlib_execsql (KStringView sSQL)
//-----------------------------------------------------------------------------
{
	m_iNumRowsAffected = 0;

	CS_RETCODE iApiRtn;
	CS_INT     iResultType;
	bool       fError = false;

	kDebugLog (CTDEBUG, "ctlib_execsql: {}", sSQL);
	kDebugLog (CTDEBUG, "ensuring that no dangling messages are in the ct queue...");
	ctlib_flush_results ();
	ctlib_clear_errors ();

	kDebugLog (CTDEBUG, "calling ct_command()...");
	if (ct_command (m_pCtCommand, CS_LANG_CMD, sSQL, CS_NULLTERM, CS_UNUSED) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_execsql>ct_command");
		return (SQLError());
	}

	kDebugLog (CTDEBUG, "calling ct_send()...");
	if (ct_send (m_pCtCommand) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_execsql>ct_send");
		return (SQLError());
	}

	kDebugLog (CTDEBUG, "calling ct_results()...");
	while ((iApiRtn = ct_results (m_pCtCommand, &iResultType)) == CS_SUCCEED)
	{
		kDebugLog (CTDEBUG, "ct_results said iResultsType={} and returned {}", iResultType, iApiRtn);

		switch (iResultType)
		{
		case CS_CMD_SUCCEED:
			break; // switch
		case CS_CMD_DONE:
			break; // switch
		case CS_CMD_FAIL:
			fError = true;
			kDebugLog (CTDEBUG, "ctlib_execsql>ct_results>iResultType CS_CMD_FAIL");
			break; // switch
		default:
			fError = true;
			kDebugLog (CTDEBUG, "ctlib_execsql>ct_results>iResultType ???");
			break; // switch
		}
	}

	#if 0
	switch (iApiRtn)
	{
	case CS_END_RESULTS:
		break;
	case CS_FAIL:
	default:
		ctlib_api_error ("ctlib_execsql>ct_results");
		return (SQLError());
	}
	#endif

	CS_INT iNumRows;
	kDebugLog (CTDEBUG, "calling {}...", "ct_res_info");
	if (ct_res_info (m_pCtCommand, CS_ROW_COUNT, &iNumRows, CS_UNUSED, NULL) == CS_SUCCEED)
	{
		if ((long)iNumRows != -1) {
			m_iNumRowsAffected = (uint64_t) iNumRows;
		}
		kDebugLog (CTDEBUG, "m_iNumRowsAffected = {}", m_iNumRowsAffected);
	}

	uint32_t iNumErrors = ctlib_check_errors();
	if (iNumErrors)
	{
		kDebugLog (CTDEBUG, "ctlib_execsql: returning false");
		return (false);
	}
	else {
		kDebugLog (CTDEBUG, "ctlib_execsql: returning true");
		return (true);
	}

} // ctlib_execsql

//-----------------------------------------------------------------------------
bool KSQL::ctlib_nextrow ()
//-----------------------------------------------------------------------------
{
	CS_INT iFetched;

	kDebugLog (CTDEBUG, "ctlib_nextrow: calling ct_fetch...");
	if ((ct_fetch (m_pCtCommand, CS_UNUSED, CS_UNUSED, CS_UNUSED, &iFetched) == CS_SUCCEED) && (iFetched > 0))
	{
		for (uint32_t ii = 0; ii < m_iNumColumns; ++ii)
		{
			// map SQL NULL values to zero-terminated C strings:
			if (m_dColInfo[ii].indp < 0) {
				m_dColInfo[ii].dszValue[0] = 0;
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
	kDebugLog (3, "ctlib_api_error ({})", sContext);

	uint32_t iMessages = ctlib_check_errors ();
	if (!iMessages) {
		m_sLastError.Format ("CT-Lib: API Error: {}", sContext);
	}

	return (false);

} // ctlib_api_error

//-----------------------------------------------------------------------------
uint32_t KSQL::ctlib_check_errors ()
//-----------------------------------------------------------------------------
{
	uint32_t         iNumErrors = 0;
	CS_INT       iNumMsgs   = 0;
	CS_CLIENTMSG ClientMsg; memset (&ClientMsg, 0, sizeof(ClientMsg));
	CS_SERVERMSG ServerMsg; memset (&ServerMsg, 0, sizeof(ServerMsg));
	int          ii;

	m_sLastError = "";

	kDebugLog (CTDEBUG, " in ctlib_check_errors() calling ct_diag...");

	if (ct_diag (m_pCtConnection, CS_STATUS, CS_CLIENTMSG_TYPE, CS_UNUSED, &iNumMsgs) != CS_SUCCEED) {
		//return (++iNumErrors); //commented out: don't let ct_diag crash ctlib_check_errors
	}

	if (iNumMsgs) {
		kDebugLog (GetDebugLevel(), "ctlib_check_errors: {} client messages", iNumMsgs);
	}

	for (ii=0; ii < iNumMsgs; ++ii) {

		kDebugLog (CTDEBUG, "calling {}...", "ct_diag");

		if (ct_diag (m_pCtConnection, CS_GET, CS_CLIENTMSG_TYPE, ii + 1, &ClientMsg) != CS_SUCCEED) {
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

		if (ii) {
			m_sLastError += "\n";	
		}
		m_sLastError += m_sErrorPrefix;	

		KString sAdd;
		sAdd.Format("CTLIB-{:05}: ", ClientMsg.msgnumber);
		m_sLastError += sAdd;	

		int iLayer    = CS_LAYER(ClientMsg.msgnumber);
		int iOrigin   = CS_ORIGIN(ClientMsg.msgnumber);
		int iSeverity = ClientMsg.severity; //CS_SEVERITY(ClientMsg.msgnumber);

		if (iLayer) {
			sAdd.Format("Layer {}, ", iLayer);
			m_sLastError+=sAdd;	
		}

		if (iOrigin) {
			sAdd.Format("Origin {}, ", iOrigin);
			m_sLastError+=sAdd;
		}

		if (iSeverity) {
			sAdd.Format("Severity {}, ", iSeverity);
			m_sLastError+=sAdd;
		}

		sAdd.Format("{}", ClientMsg.msgstring);
		m_sLastError+=sAdd;

		if (ClientMsg.osstringlen > 0) {
			sAdd.Format("{}OS-{:05}: {}", m_sErrorPrefix, ClientMsg.osnumber, ClientMsg.osstring);
			m_sLastError+=sAdd;
		}

		++iNumErrors;
	}

	kDebugLog (CTDEBUG, "calling {}...", "ct_diag");

	if (ct_diag (m_pCtConnection, CS_STATUS, CS_SERVERMSG_TYPE, CS_UNUSED, &iNumMsgs) != CS_SUCCEED) {
		//return (++iNumErrors);
	}

	if (iNumMsgs) {
		kDebugLog (GetDebugLevel(), "ctlib_check_errors: {} server message", iNumMsgs);
	}

	for (ii=0; ii < iNumMsgs; ++ii) {

		kDebugLog (CTDEBUG, "calling ct_diag... message #%i", ii);

		if (ct_diag (m_pCtConnection, CS_GET, CS_SERVERMSG_TYPE, ii + 1, &ServerMsg) != CS_SUCCEED) {
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

		m_iErrorNum = ServerMsg.msgnumber;

		if (ii) {
			m_sLastError += "\n";
		}
		m_sLastError += m_sErrorPrefix;

		KString sAdd;
		sAdd.Format("SQL-{:05}: ", ServerMsg.msgnumber);
		m_sLastError += sAdd;

		if (ServerMsg.proclen) {
			m_sLastError += ServerMsg.proc;
		}
		if (ServerMsg.state) {
			sAdd.Format("State {}, ", ServerMsg.state);
			m_sLastError += sAdd;
		}
		if (ServerMsg.severity) {
			sAdd.Format("Severity {}, ", ServerMsg.severity);
			m_sLastError += sAdd;
		}
		if (ServerMsg.line) {
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
	m_iErrorNum = 0;

	kDebugLog (CTDEBUG, "calling {}...", "ct_diag");
	if (ct_diag (m_pCtConnection, CS_CLEAR, CS_ALLMSG_TYPE, CS_UNUSED, NULL) != CS_SUCCEED) {
		kDebugLog (1, "[{}] ctlib_clear_errors>cs_diag(CS_CLEAR) failed", m_iDebugID);
	}

	CS_INT iNumMsgs = 0;

	kDebugLog (CTDEBUG, "calling {}...", "ct_diag");
	if (ct_diag (m_pCtConnection, CS_STATUS, CS_ALLMSG_TYPE, CS_UNUSED, &iNumMsgs) != CS_SUCCEED) {
		kDebugLog (1, "[{}] ctlib_clear_errors>cs_diag(CS_STATUS) failed", m_iDebugID);
	}

	if (iNumMsgs != 0) {
		kDebugLog (1,, "[{}] ctlib_clear_errors>cs_diag(CS_CLEAR) failed: there are still {} messages on queue.", m_iDebugID, iNumMsgs);
		return (false);
	}

	return (true);

} // ctlib_clear_errors

//-----------------------------------------------------------------------------
bool KSQL::ctlib_prepare_results ()
//-----------------------------------------------------------------------------
{
	#if 0
	kDebugLog (GetDebugLevel(), "[{}] KSQL::ctlib_prepare_results()  -- TODO/WIP", m_iDebugID);
	return (true);  // TODO: WIP
	#endif

	bool   fLooping    = true;
	CS_INT iResultType;

	kDebugLog (CTDEBUG, "ctlib_prepare_results: preparing for results...");

	kDebugLog (CTDEBUG, "calling {}...", "ct_results");
	while (fLooping && (ct_results (m_pCtCommand, &iResultType) == CS_SUCCEED))
	{
		switch ((int) iResultType)
		{
		case CS_CMD_SUCCEED:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", "CS_CMD_SUCCEED");
			break;
		case CS_CMD_DONE:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", "CS_CMD_DONE");
			fLooping = false;
			break;
		case CS_STATUS_RESULT:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", "CS_STATUS_RESULT");
			fLooping = false;
			break;
		case CS_CMD_FAIL:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", "CS_CMD_FAIL");
			return (ctlib_api_error ("ExecQuery>ctlib_prepare_results>ct_results>CS_CMD_FAIL"));
		case CS_CURSOR_RESULT:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", "CS_CURSOR_RESULT");
			fLooping = false;
			break; // ready for data
		case CS_ROW_RESULT:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", "CS_ROW_RESULT");
			fLooping = false;
			break; // ready for data
		case CS_COMPUTE_RESULT:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", "CS_COMPUTE_RESULT");
			return (ctlib_api_error ("ExecQuery>ctlib_prepare_results>ct_results>CS_COMPUTE_RESULT"));
		default:
			kDebugLog (CTDEBUG, "ExecQuery>ctlib_prepare_results>ct_results>{}", iResultType);
			return (ctlib_api_error ("ExecQuery>ctlib_prepare_results>ct_results>???"));
		}

		if (fLooping) {
			kDebugLog (CTDEBUG, "calling {}...", "ct_results");
		}
	}

	kDebugLog (CTDEBUG, "ctlib_prepare_results: ready for query results");

	CS_INT iNumCols;
	kDebugLog (CTDEBUG, "calling {}...", "ct_res_info");
	if (ct_res_info (m_pCtCommand, CS_NUMDATA, &iNumCols, CS_UNUSED, NULL) != CS_SUCCEED) {
		ctlib_api_error ("NextRow>ct_res_info");
		return (SQLError());
	}
	m_iNumColumns = (uint32_t) iNumCols;

	kDebugLog (CTDEBUG, "  mallocing for a results row with {} columns...", m_iNumColumns);
	m_dColInfo = (COLINFO*) kmalloc (m_iNumColumns * sizeof(COLINFO), "    KSQL:ExecQuery:m_dColInfo");

	for (uint32_t ii = 0; ii < m_iNumColumns; ++ii)
	{
		kDebugLog (CTDEBUG, "  retrieving info about col # %02u", ii);

		CS_DATAFMT colinfo;
		memset (&colinfo, 0, sizeof(colinfo));

		kDebugLog (CTDEBUG, "using ct_describe() to get column info for col#%02u...", ii+1);
		if (ct_describe (m_pCtCommand, ii+1, &colinfo) != CS_SUCCEED) {
			ctlib_api_error ("NextRow>ct_describe) failed for a column");
			return (SQLError());
		}

		if (colinfo.status & CS_RETURN) {
			kDebugLog (GetDebugLevel, "CS_RETURN code: column {}  -- ignored", ii); // ignored
		}

		kstrncpy (m_dColInfo[ii].szColName, (colinfo.namelen) ? colinfo.name : "", MAXCOLNAME+1);

		m_dColInfo[ii].iDataType   = colinfo.datatype;
		m_dColInfo[ii].iMaxDataLen = max(colinfo.maxlength+2, 8000); //<- allocate at least the max-varchar length to avoid overflows
		m_dColInfo[ii].indp        = 0;

		enum {SANITY_MAX = 50*1024};
		if ((m_dColInfo[ii].iMaxDataLen < 0) || (m_dColInfo[ii].iMaxDataLen > SANITY_MAX)) {
			kDebugLog (CTDEBUG, " col#{:02}: name='{}':  maxlength changed from {} to {}",
					ii+1, m_dColInfo[ii].szColName, m_dColInfo[ii].iMaxDataLen, SANITY_MAX);
			m_dColInfo[ii].iMaxDataLen = SANITY_MAX;
		}

		kDebugLog (CTDEBUG, " col#{:02}: name='{}', maxlength={}, datatype={}", 
				ii+1, m_dColInfo[ii].szColName, m_dColInfo[ii].iMaxDataLen, m_dColInfo[ii].iDataType);

		m_dColInfo[ii].dszValue    = (char*)kmalloc ((int)(m_dColInfo[ii].iMaxDataLen + 1), "  KSQL:ExecQuery:dszValue:ctlib");

		// set up bindings for the data:
		CS_DATAFMT datafmt;
		memset (&datafmt, 0, sizeof(datafmt));
		datafmt.datatype  = CS_CHAR_TYPE;
		datafmt.format    = CS_FMT_NULLTERM;
		datafmt.maxlength = m_dColInfo[ii].iMaxDataLen;
		datafmt.count     = 1;
		datafmt.locale    = NULL;

		kDebugLog (CTDEBUG, "calling {}...", "ct_bind");
		if (ct_bind (m_pCtCommand, ii+1, &datafmt, m_dColInfo[ii].dszValue, NULL, &(m_dColInfo[ii].indp)) != CS_SUCCEED) {
			ctlib_api_error ("NextRow>ct_bind");
			return (SQLError());
		}

	} // for each column

	return (true); // ready for dta

} // ctlib_prepare_results

//-----------------------------------------------------------------------------
void KSQL::ctlib_flush_results ()
//-----------------------------------------------------------------------------
{
	kDebugLog (CTDEBUG, "ctlib_flush_results... [to flush prior results that might be dangling]");

	CS_INT     iResultType;
	bool       fLooping = true;
	bool       fFlush   = false;

	kDebugLog (CTDEBUG, "calling {}...", "ct_results");
	while (fLooping && (ct_results (m_pCtCommand, &iResultType) == CS_SUCCEED))
	{
		switch ((int) iResultType)
		{
		case CS_CMD_SUCCEED:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", "CS_CMD_SUCCEED");
			break;
		case CS_CMD_DONE:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", "CS_CMD_DONE");
			fLooping = false;
			break;
		case CS_STATUS_RESULT:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", "CS_STATUS_RESULT");
			fLooping = false;
			break;
		case CS_CMD_FAIL:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", "CS_CMD_FAIL");
			fLooping = false;
			break;
		case CS_CURSOR_RESULT:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", "CS_CURSOR_RESULT");
			fLooping = false;
			fFlush   = true;
			break;
		case CS_ROW_RESULT:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", "CS_ROW_RESULT");
			fLooping = false;
			fFlush   = true;
			break;
		case CS_COMPUTE_RESULT:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", "CS_COMPUTE_RESULT");
			fLooping = false;
			break;
		default:
			kDebugLog (CTDEBUG, "ctlib_flush_results>ct_results>{}", iResultType);
			fLooping = false;
			break;
		}

		if (fLooping) {
			kDebugLog (CTDEBUG, "calling {}...", "ct_results");
		}
	}

	DebugLog (CTDEBUG, "calling {}...", "ct_cancel");
	if (ct_cancel(m_pCtConnection, NULL, CS_CANCEL_ALL) != CS_SUCCEED) {
		ctlib_api_error ("ctlib_flush_results>ct_cancel");
	}

	CS_INT iFetched;
	kDebugLog (CTDEBUG, "ctlib_nextrow: calling ct_fetch...");
	if (fFlush && (ct_fetch (m_pCtCommand, CS_UNUSED, CS_UNUSED, CS_UNUSED, &iFetched) == CS_SUCCEED) && (iFetched > 0))
	{
		kDebugLog (CTDEBUG, "ctlib_flush_results>ct_fetch> dumping row...\n");
	}

} // ctlib_flush_results

#endif

//-----------------------------------------------------------------------------
size_t KSQL::OutputQuery (KStringView sSQL, KStringView sFormat, FILE* fpout/*=stdout*/)
//-----------------------------------------------------------------------------
{
	int iFormat = FORM_ASCII;

	if ((sFormat == "-ascii") || (sFormat == "-query"))
	{
		iFormat = FORM_ASCII;
	}
	else if (sFormat == "-csv") {
		iFormat = FORM_CSV;
	}
	else if (sFormat == "-html") {
		iFormat = FORM_HTML;
	}

	return (OutputQuery (sSQL, iFormat, fpout));

} // OutputQuery

//-----------------------------------------------------------------------------
size_t KSQL::OutputQuery (KStringView sSQL, int iFormat/*=FORM_ASCII*/, FILE* fpout/*=stdout*/)
//-----------------------------------------------------------------------------
{
	if (!ExecRawQuery (sSQL, GetFlags(), "OutputQuery")) {
		return (-1);
	}

	KPROPS Widths;
	KROW   Row;
	size_t iNumRows = 0;
	enum   {MAXCOLWIDTH = 80};
	size_t ii;

	if (iFormat == FORM_ASCII)
	{
		while (NextRow (Row))
		{
			if (++iNumRows == 1) // factor col headers into col widths
			{
				for (ii=0; ii < Row.size(); ++ii)
				{
					KString sName(Row.GetName (ii));
					KString sValue(Row.GetName (ii));
					size_t    iLen   = sValue.length();
					size_t    iMax   = Widths.Get (sName).UInt32();
					if ((iLen > iMax) && (iLen <= MAXCOLWIDTH))  {
						Widths.Add (sName, std::to_string(iLen));
					}
				}
			}

			for (ii=0; ii < Row.size(); ++ii)
			{
				KString sName(Row.GetName (ii));
				KString sValue(Row.GetValue (ii));
				size_t  iLen  = sValue.length();
				size_t  iMax  = Widths.Get (sName).UInt32();
				if ((iLen > iMax) && (iLen <= MAXCOLWIDTH))  {
					Widths.Add (sName, std::to_string(iLen));
				}
			}
		}
		EndQuery ();
		ExecRawQuery (sSQL, GetFlags(), "OutputQuery");
	}

	//if (iFormat == FORM_ASCII) {
	//	Widths.DebugPairs (1, "Max Widths:");
	//}

	iNumRows = 0;
	while (NextRow (Row))
	{
		//Row.DebugPairs (1, "Row");

		// output column headers:
		if (++iNumRows == 1)
		{
			switch (iFormat)
			{
			case FORM_ASCII:
				for (ii=0; ii < Row.size(); ++ii)
				{
					KString sName = Row.GetName (ii);
					int     iMax  = Widths.Get (sName).Int32();
					fprintf (fpout, "%s%-*.*s-+", (!ii) ? "+-" : "-", iMax, iMax, BAR);
				}
				fprintf (fpout, "\n");
				for (ii=0; ii < Row.size(); ++ii)
				{
					KString sName = Row.GetName (ii);
					int     iMax  = Widths.Get (sName).Int32();
					fprintf (fpout, "%s%-*.*s |", (!ii) ? "| " : " ", iMax, iMax, sName.c_str());
				}
				fprintf (fpout, "\n");
				for (ii=0; ii < Row.size(); ++ii)
				{
					KString sName = Row.GetName (ii);
					int     iMax  = Widths.Get (sName).Int32();
					fprintf (fpout, "%s%-*.*s-+", (!ii) ? "+-" : "-", iMax, iMax, BAR);
				}
				fprintf (fpout, "\n");
				break;
			case FORM_HTML:
				fprintf (fpout, "<table>\n");
				fprintf (fpout, "<tr>\n");
				for (ii=0; ii < Row.size(); ++ii)
				{
					KString sName = Row.GetName (ii);
					fprintf (fpout, " <th>%s</th>\n", sName.c_str());
				}
				fprintf (fpout, "</tr>\n");
				break;
			case FORM_CSV:
				for (ii=0; ii < Row.size(); ++ii)
				{
					KString sName = Row.GetName (ii);
					fprintf (fpout, "%s\"%s\"", (!ii) ? "" : ",", sName.c_str());
				}
				fprintf (fpout, "\n");
				break;
			}
		}

		// output row:
		switch (iFormat)
		{
		case FORM_ASCII:
			for (ii=0; ii < Row.size(); ++ii)
			{
				KString sName  = Row.GetName (ii);
				KString sValue = Row.GetValue (ii);
				int     iMax   = Widths.Get (sName).Int32();
				fprintf (fpout, "%s%-*.*s |", (!ii) ? "| " : " ", iMax, iMax, sValue.c_str());
			}
			fprintf (fpout, "\n");
			break;
		case FORM_HTML:
			fprintf (fpout, "<tr>\n");
			for (ii=0; ii < Row.size(); ++ii)
			{
				KString sValue = Row.GetValue (ii);
				fprintf (fpout, " <td>%s</td>\n", sValue.c_str());
			}
			fprintf (fpout, "</tr>\n");
			break;
		case FORM_CSV:
			for (ii=0; ii < Row.size(); ++ii)
			{
				KString sValue = Row.GetValue (ii);
				fprintf (fpout, "%s\"%s\"", (!ii) ? "" : ",", sValue.c_str());
			}
			fprintf (fpout, "\n");
			break;
		}

	} // while

	if (iNumRows)
	{
		switch (iFormat)
		{
		case FORM_HTML:
			fprintf (fpout, "</table>\n");
			break;
		case FORM_ASCII:
			for (ii=0; ii < Row.size(); ++ii)
			{
				KString sName = Row.GetName (ii);
				int     iMax  = Widths.Get (sName).Int32();
				fprintf (fpout, "%s%-*.*s-+", (!ii) ? "+-" : "-", iMax, iMax, BAR);
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
	m_sLastError = "";

} // ResetErrorStatus

} // namespace dekaf2
