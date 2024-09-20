
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>   // for getpwuid()

#include <dekaf2/kstring.h>
#include <dekaf2/kregex.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/klog.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kfilesystem.h>

using namespace dekaf2;

const char* g_Usage[] = {
	"",
	"usage: statinfo [options] { <file[s]> | - }",
	"",
	"where [options] are:",
	"  -d[d[d]]         :: klogging to stdout",
	"  -noheader        :: eliminate header row",
	"  -offset <secs>   :: add and offset (in secs) to UTC",
	"",
	NULL
};

typedef struct
{
	time_t iUnixTime;
	char   szTimestamp[19+1];  // 2001-03-31 12:00:00
	char   szHourOfDay[2+1];   // 12
	char   szDayOfWeek[5+1];   // 5:THU
	char   szWeekSpan[33+1];   // 2001-03-04(SUN)-->2001-03-10(SAT)
	char   szMonthOfYear[6+1]; // 03:MAR
	char   szYear[4+1];        // 2001

} DFIELDS;

void  DoStat   (char* pszPath, time_t tOffset=0);
int   FileType (const char* pszFilename, KString& szFiletype);
bool  kExtendedDateFields (struct tm* ptmStruct, DFIELDS* pDF);
bool  kExtendedDateFields (time_t iUnixTime, DFIELDS* pDF);
bool  kExtendedDateFields (KStringViewZ pszMMsDDsYYYY, KStringViewZ pszHH, DFIELDS* pDF);

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	if (argc <= 1)
	{
		for (unsigned int ii=0; g_Usage[ii]; ++ii)
		{
			printf ("%s\n", g_Usage[ii]);
		}
		return (1);
	}

	time_t tOffset   = 0;
	bool   tNoHeader = false;

	for (int ii=1; ii<argc; ++ii)
	{
		if (!strcmp (argv[ii], "-offset"))
		{
			tOffset = KString(argv[++ii]).Int64();
		}
		else if (!strcmp (argv[ii], "-noheader"))
		{
			tNoHeader = true;
		}
		else if (!strcmp (argv[ii], "-"))
		{
			enum {MAX=1000};
			char szPath[MAX+1];

			if (!tNoHeader)
			{
				printf ("#lastmod-utc|lastmod-string|bytes|owner|uid|type|pathname\n");
			}

			while (fgets (szPath, MAX, stdin))
			{
				// trim whitespace:
				while (*szPath && (szPath[strlen(szPath)-1] <= ' '))
				{
					szPath[strlen(szPath)-1] = 0;
				}
				DoStat (szPath, tOffset);
			}
		}
		else
		{
			if (!tNoHeader)
			{
				printf ("#lastmod-utc|lastmod-string|bytes|owner|uid|type|pathname\n");
			}
			DoStat (argv[ii], tOffset);
		}
	}

	return (0);

} // main -- statinfo

//-----------------------------------------------------------------------------
void DoStat (char* pszPath, time_t tOffset/*=0*/)
//-----------------------------------------------------------------------------
{
	static struct stat StatStruct;

	// skip incoming blank lines and comments:
	if (!pszPath || !*pszPath || (*pszPath == '#'))
		return;

	if (!strcmp (pszPath, ".") || !strcmp (pszPath, "..")) {
		printf ("# ignored: %s\n", pszPath);
		return;
	}

	// automatically extract filename from SWISH output:
	// 288 /full/path/to/file/with possible spaces.txt "Title with possible spaces" 87838
	char* space    = strchr (pszPath, ' ');
	char* swishOut = strstr (pszPath, " \"");
	if ((space && swishOut) && (swishOut > space)) {
		*swishOut = 0;
		pszPath   = space+1;
		while (*pszPath == ' ')
			++pszPath;
	}

	// output a comment if stat() fails:
	if (stat (pszPath, &StatStruct) != 0) {
		printf ("# %s: %s\n", strerror(errno), pszPath);
		return;
	}

	time_t  tLastMod;
	KString sOutput;
	KRegex  regex{"[0-9][0-9][0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]"};
	kDebug(1, "identify -format '%%[exif:DateTimeOriginal]' '{}'", pszPath);
	kSystem (kFormat("identify -format '%%[exif:DateTimeOriginal]' '{}'", pszPath), sOutput);
	if (regex.Matches (sOutput.c_str() ))
	{
		// raw identify: 2016:08:13 13:52:11
		kDebug(1, "off-exif: %s", sOutput.c_str());
		sOutput.Replace (":", "-", /*bReplaceAll=*/false); // only twice
		sOutput.Replace (":", "-", /*bReplaceAll=*/false);
		kDebug(1, "parsable: %s", sOutput.c_str());
		tLastMod = KUnixTime(KUTCTime (sOutput.c_str())).to_time_t();
		kDebug(1, "{}: off exif:DateTimeOriginal: {}", pszPath, /*(SHUGE)*/ tLastMod);
	}
	else {
		tLastMod = StatStruct.st_mtime;
		kDebug(1, "{}: imagemagick failed, using mtime off stat: {}", pszPath, /*(SHUGE)*/ tLastMod);
	}

	// watch for prelabelled somedir/K1231373376-IMG-343.jpg and use it for lastmod:
	KString sBase (pszPath);
	char* lastslash = strrchr (pszPath, '/');
	if (lastslash) {
		sBase = lastslash + 1;
	}
	if (sBase.MatchRegex ("^K[0-9]{10}")) {
		sBase = sBase.Left  (strlen("K1231373376"));
		sBase = sBase.Right (strlen("1231373376"));
		kDebug(1, "extracting lastmod info from filename: %s", pszPath);
		tLastMod = atoi (sBase.c_str());
		kDebug(1, "off filename: atoi({}) = st_mtime = {}\n", sBase.c_str(), /*(SHUGE)*/ tLastMod);
	}

	if (tOffset) {
		tLastMod += tOffset;
		kDebug(1, "offset   = {}\n", /*(SHUGE)*/ tOffset);
		kDebug(1, "adjusted = {}\n", /*(SHUGE)*/ tLastMod);
	}

	char szLastMod[50+1];
	//char szOwner[50+1];
	KStringViewZ szOwner;
	struct tm*     pTimeStruct = localtime (&tLastMod);
	struct passwd* pPass       = getpwuid (StatStruct.st_uid);
	KString szFiletype; // since some old style c function are used KStringViewZ might be better, but I had compiler issues -Lenny

	strftime (szLastMod, 50, "%Y-%m-%d %a %H:%M:%S %Z", pTimeStruct);

	if (pPass)
	{
		szOwner = pPass->pw_name;
	}
	else
	{
		szOwner = kFormat("{}", StatStruct.st_uid);
	}

	if (StatStruct.st_mode & S_IFDIR)
		szFiletype = "dir";
	else
		FileType (pszPath, szFiletype);

/*
	printf ("%lld|%s|%lld|%s|%lld|%s|%s\n", 
		tLastMod,
		szLastMod,
		StatStruct.st_size,
		szOwner,
		StatStruct.st_uid,
		szFiletype,
		pszPath);
		*/

	KOut.FormatLine("{}|{}|{}|{}|{}|{}|{}\n", 
		tLastMod,
		szLastMod,
		StatStruct.st_size,
		szOwner,
		StatStruct.st_uid,
		szFiletype,
		pszPath);

} // DoStat

//-----------------------------------------------------------------------------
int FileType (const char* pszFilename, KString& szFiletype) //char szFiletype[20+1])
//-----------------------------------------------------------------------------
{
	//*szFiletype = 0;

	enum  {MAXCHUNK = 512};
	//char  Chunk[MAXCHUNK+1];
	KString Chunk;
	/*
	int	  fd	 = open (pszFilename, O_RDONLY);

	if (!fd) {
		strcpy (szFiletype, "ERR");
		return false; //(FALSE);
	}

	long  iBytes = read (fd, Chunk, MAXCHUNK);
	*/
	long iBytes = Chunk.size();

	kReadTextFile(pszFilename, Chunk, /*bToUnixLineFeeds*/ false, MAXCHUNK);
	unsigned int  iNotPrintable = 0;
	int   fIsBinary	    = false; //FALSE;

	//Chunk[iBytes] = 0;
	//close (fd);

	if (Chunk.empty()) {
		szFiletype = "EMPTY";
		return false; //(FALSE);
	}

	enum { U2B = 64 }; // <-- ('A' - ^A)
	static char s_pszSolBinary[] = { 127, 'E', 'L', 'F', 0 };
	static char s_pszHpBinary[]  = { 'B'-U2B, 'P'-U2B, 'A'-U2B, 'H'-U2B, 'E'-U2B, 'R'-U2B, 0 };
	static char s_pszWinBinary[] = { 'M', 'Z', static_cast<char>(144), 0 };

//	if (strmatchN (Chunk, s_pszSolBinary)) {
	if (Chunk.StartsWith(s_pszSolBinary)) {
		szFiletype = "exe(SunOS)";
		return true; //(TRUE);
	}
	if (Chunk.StartsWith(s_pszHpBinary)) {
		szFiletype = "exe(HP-UX)";
		return true; //(TRUE);
	}
	if (Chunk.StartsWith(s_pszWinBinary)) {
		szFiletype = "exe(Win32)";
		return true; //(TRUE);
	}

	char* lastdot = (char*)strrchr (pszFilename, '.');
	if (lastdot && (strlen (lastdot+1) < 5)) {
		szFiletype = lastdot+1;
		szFiletype = szFiletype.ToLower();
		if (szFiletype.Contains("htm"))
			szFiletype = "html";
	}

	for (long ii=0; ((ii<iBytes) && (ii<MAXCHUNK)); ++ii)
	{
		switch (Chunk[ii])
		{
		case 10: // ^J
		case 13: // ^M
		case ' ':
		case '\t':
			break;	// <-- char is printable/not binary
		default:
			if ((Chunk[ii] < ' ') || (Chunk[ii] > '~'))
				++iNotPrintable;
			break;
		}
	}

	// over 10% non-printable in order to be a binary file:
	fIsBinary = (((iNotPrintable*100)/iBytes) > 10);

	szFiletype += (fIsBinary) ? "(binary)" : "(text)";


	return (fIsBinary);

} // FileType

//-----------------------------------------------------------------------------
bool kExtendedDateFields (struct tm* ptmStruct, DFIELDS* pDF)
//-----------------------------------------------------------------------------
{
	pDF->iUnixTime = mktime (ptmStruct);

	// a few easy ones:
	strftime (pDF->szHourOfDay,  2+1, "%H", ptmStruct);
	strftime (pDF->szYear,       4+1, "%Y", ptmStruct);
	strftime (pDF->szTimestamp, 19+1, "%Y-%m-%d %H:%M:%S", ptmStruct);

	enum   {MAX = 50};
	//_TCHAR szBuf[MAX+1]; tchars are apparently for UTF-16 processing??? -Lenny
	char szBuf[MAX+1]; // strftime needs a char* buffer it can write to - if we're using time_t I think this is required -Lenny

	// format Day of Week:
	strftime (szBuf, MAX, "%a", ptmStruct);
	//_sntprintf (pDF->szDayOfWeek, sizeof(pDF->szDayOfWeek), "%d:%3s", ptmStruct->tm_wday + 1, szBuf);
	// TODO @Joe START HERE - this next line "compiles" but does get rid of the specific string sizes.
	// I don't know if you need the string sizes in the DFIELDS struct to stay the same or not
	strcpy(pDF->szDayOfWeek, kFormat("{}:{}", ptmStruct->tm_wday + 1, szBuf).c_str());

	// format Month Of Year:
	strftime (szBuf, MAX, "%b", ptmStruct);
	//_sntprintf (pDF->szMonthOfYear, sizeof(pDF->szMonthOfYear), "%02d:%3s", ptmStruct->tm_mon + 1, szBuf);
	pDF->szMonthOfYear = kFormat("{}:{}", ptmStruct->tm_mon + 1, szBuf);

	// move time back to first day of week (prior Sunday):
	time_t iUnixTime = pDF->iUnixTime - (ptmStruct->tm_wday * 24 * 60 * 60);
	ptmStruct = localtime (&iUnixTime);

	char szWeekEnd[MAX]; // was _TCHAR

	// format the first portion of the Week string:
	strftime (szBuf, MAX, "%a", ptmStruct);
	//_sntprintf (pDF->szWeekSpan, sizeof(pDF->szWeekSpan), "%04d-%02d-%02d(%3s)-->",
	//	ptmStruct->tm_year+1900, ptmStruct->tm_mon+1, ptmStruct->tm_mday, szBuf);
	pDF->szWeekSpan = kFormat("{}-{}-{}({})-->", ptmStruct->tm_year+1900, ptmStruct->tm_mon+1, ptmStruct->tm_mday, szBuf);

	// move time back to last day of week (this coming Saturday):
	iUnixTime += 6 * 24 * 60 * 60;
	ptmStruct = localtime (&iUnixTime);

	// format the tail portion:
	strftime (szBuf, MAX, "%a", ptmStruct);
//	_sntprintf (szWeekEnd, MAX, "%04d-%02d-%02d(%3s)", 
//		ptmStruct->tm_year+1900, ptmStruct->tm_mon+1, ptmStruct->tm_mday, szBuf);
	szWeekEnd = kFormat("{}-{}-{}({})",ptmStruct->tm_year+1900, ptmStruct->tm_mon+1, ptmStruct->tm_mday, szBuf);


	// combine them:
	_tcsnccat (pDF->szWeekSpan, szWeekEnd, sizeof(pDF->szWeekSpan) - strlen(pDF->szWeekSpan) - 1);

	ktoupper (pDF->szDayOfWeek);
	ktoupper (pDF->szWeekSpan);
	ktoupper (pDF->szMonthOfYear);

	kDebug (2, "kExtendedDateFields:");
	kDebug (2, "  %-12s = %lu (secs since 1970)", "UnixTime", pDF->iUnixTime);
	kDebug (2, "  %-12s = %s", "Timestamp", pDF->szTimestamp);
	kDebug (2, "  %-12s = %s", "HourOfDay", pDF->szHourOfDay);
	kDebug (2, "  %-12s = %s", "DayOfWeek", pDF->szDayOfWeek);
	kDebug (2, "  %-12s = %s", "WeekSpan", pDF->szWeekSpan);
	kDebug (2, "  %-12s = %s", "MonthOfYear", pDF->szMonthOfYear);
	kDebug (2, "  %-12s = %s", "Year", pDF->szYear);

	return true; //(TRUE);

} // kExtendedDateFields - v1

//-----------------------------------------------------------------------------
bool kExtendedDateFields (time_t iUnixTime, DFIELDS* pDF)
//-----------------------------------------------------------------------------
{
	struct tm* ptmStruct = localtime (&iUnixTime);
	return (kExtendedDateFields (ptmStruct, pDF));

} // kExtendedDateFields - v2

//-----------------------------------------------------------------------------
bool kExtendedDateFields (KStringViewZ pszMMsDDsYYYY, KStringViewZ pszHH, DFIELDS* pDF)
//-----------------------------------------------------------------------------
{
	//				 0 1 2 3 4 5 6 7 8 9
	// hardcoded for M M / D D / Y Y Y Y
	if (_tcsclen(pszMMsDDsYYYY)!=10)
	{
		kDebug("kExtendeDateFields: invalid arg '%s' (must be MM/DD/YYYY)", pszMMsDDsYYYY);
		return false; //(FALSE);
	}

	_TCHAR szCopy[10+1];
	kstrncpy (szCopy, pszMMsDDsYYYY, sizeof(szCopy));

	// nil terminate at the slashes:
	szCopy[2] = 0;
	szCopy[5] = 0;

	// parse date string:
	int iMonth		= _ttoi(szCopy);
	int iDay		= _ttoi(szCopy+3);
	int iYear		= _ttoi(szCopy+6);		// 4-digit year
	int iHour		= _ttoi(pszHH);

	if (iYear < 1900)
	{
		kDebug("kExtendedDateFields: invalid year (%d), must be at least 1900.", iYear);
		return false; //(FALSE);
	}

	// fill in part of tmStruct, then call mktime():
	struct tm tmStruct;
	memset ((void*)&tmStruct, 0, sizeof(tmStruct));
	tmStruct.tm_sec   = 0;             // 0-59
	tmStruct.tm_min   = 0;             // 0-59
	tmStruct.tm_hour  = iHour;         // 0-23
	tmStruct.tm_mday  = iDay;          // 1-31
	tmStruct.tm_mon   = iMonth - 1;    // 0=Jan, 1=Feb, etc.
	tmStruct.tm_year  = iYear - 1900;  // years since 1900
	tmStruct.tm_wday  = 0;             // mktime() will fill in
	tmStruct.tm_yday  = 0;             // mktime() will fill in
	tmStruct.tm_isdst = -1;            // convert to daylight savings time

	return (kExtendedDateFields (&tmStruct, pDF));

} // kExtendedDateFields - v3

