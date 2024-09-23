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
	time_t  iUnixTime;
	KString sTimestamp;   // 2001-03-31 12:00:00
	KString sHourOfDay;   // 12
	KString sDayOfWeek;   // 5:THU
	KString sWeekSpan;    // 2001-03-04(SUN)-->2001-03-10(SAT)
	KString sMonthOfYear; // 03:MAR
	KString sYear;        // 2001

} DFIELDS;

void  DoStat   (KString/*copy*/ sPath, time_t tOffset=0);
bool  FileType (const KString& sFilename, KString& sFiletype);
bool  kExtendedDateFields (struct tm* ptmStruct, DFIELDS& DF);
bool  kExtendedDateFields (time_t iUnixTime, DFIELDS& DF);
bool  kExtendedDateFields (KString/*copy*/ sMMsDDsYYYY, KStringViewZ sHH, DFIELDS& DF);

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

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
				KOut.FormatLine ("#lastmod-utc|lastmod-string|bytes|owner|uid|type|pathname");
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
				KOut.FormatLine ("#lastmod-utc|lastmod-string|bytes|owner|uid|type|pathname");
			}
			DoStat (argv[ii], tOffset);
		}
	}

	return (0);

} // main -- statinfo

//-----------------------------------------------------------------------------
void DoStat (KString/*copy*/ sPath, time_t tOffset/*=0*/)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	// skip incoming blank lines and comments:
	if (!sPath || sPath.StartsWith ("#") || kStrIn(sPath,".,.."))
	{
		return;
	}

	// automatically extract filename from SWISH output:
	// 288 /full/path/to/file/with possible spaces.txt "Title with possible spaces" 87838
	if (sPath.Contains (" \""))
	{
		auto iSpace = sPath.find(" ");
		if (iSpace > 0)
		{
			sPath = sPath.Mid (iSpace);
			sPath.ClipAt (" \"");
			sPath.Trim();
		}
	}

	struct stat StatStruct;
	if (stat (sPath.c_str(), &StatStruct) != 0) {
		KOut.FormatLine ("# {}: {}", strerror(errno), sPath); // output a comment if stat() fails:
		return;
	}

	time_t  tLastMod;
	KString sOutput;

	// use ImageMagick to get date photo was taken:
	auto    sCmd = kFormat ("identify -format '%%[exif:DateTimeOriginal]' '{}'", sPath);
	kDebug (1, sCmd);
	kSystem (sCmd, sOutput);

	KRegex regex{"[0-9][0-9][0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]"};
	if (regex.Matches (sOutput))
	{
		// raw identify: 2016:08:13 13:52:11
		kDebug (1, "off-exif: %s", sOutput);
		sOutput.Replace (":", "-", /*bReplaceAll=*/false); // only twice
		sOutput.Replace (":", "-", /*bReplaceAll=*/false);
		kDebug (1, "parsable: %s", sOutput.c_str());
		tLastMod = KUnixTime(KUTCTime (sOutput.c_str())).to_time_t();
		kDebug (1, "{}: off exif:DateTimeOriginal: {}", sPath, /*(SHUGE)*/ tLastMod);
	}
	else
	{
		tLastMod = StatStruct.st_mtime;
		kDebug (1, "{}: imagemagick failed, using mtime off stat: {}", sPath, /*(SHUGE)*/ tLastMod);
	}

	// watch for prelabelled somedir/K1231373376-IMG-343.jpg and use it for lastmod:
	KString sBase = kBasename (sPath);
	if (sBase.MatchRegex ("^K[0-9]{10}"))
	{
		sBase = sBase.Left  (strlen("K1231373376"));
		sBase = sBase.Right (strlen("1231373376"));
		kDebug (1, "extracting lastmod info from filename: %s", sPath);
		tLastMod = atoi (sBase.c_str());
		kDebug (1, "off filename: atoi({}) = st_mtime = {}\n", sBase.c_str(), /*(SHUGE)*/ tLastMod);
	}

	if (tOffset)
	{
		tLastMod += tOffset;
		kDebug(1, "offset   = {}\n", /*(SHUGE)*/ tOffset);
		kDebug(1, "adjusted = {}\n", /*(SHUGE)*/ tLastMod);
	}

	KStringViewZ   sOwner;
	struct tm*     pTimeStruct = localtime (&tLastMod);
	struct passwd* pPass       = getpwuid (StatStruct.st_uid);
	auto           sLastMod    = kFormTimestamp (mktime(pTimeStruct), "%Y-%m-%d %a %H:%M:%S %Z");
	KString        sFiletype;

	if (pPass)
	{
		sOwner = pPass->pw_name;
	}
	else
	{
		sOwner = kFormat("{}", StatStruct.st_uid);
	}

	if (StatStruct.st_mode & S_IFDIR)
	{
		sFiletype = "dir";
	}
	else
	{
		FileType (sPath, sFiletype);
	}

	auto sFinal = kFormat ("{}|{}|{}|{}|{}|{}|{}", 
		tLastMod,
		sLastMod,
		StatStruct.st_size,
		sOwner,
		StatStruct.st_uid,
		sFiletype,
		sPath);

	kDebug (1, sFinal);
	KOut.WriteLine (sFinal);

} // DoStat

//-----------------------------------------------------------------------------
bool FileType (const KString& sFilename, KString& sFiletype)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	enum    {MAXCHUNK = 512};
	KString sChunk;

	kReadTextFile (sFilename, sChunk, /*bToUnixLineFeeds*/false, MAXCHUNK);

	size_t  iNotPrintable = 0;
	bool    bIsBinary     = false;

	if (sChunk.empty())
	{
		sFiletype = "EMPTY";
		return false;
	}

	enum { U2B = 64 }; // <-- ('A' - ^A)
	static char s_sSolBinary[] = { 127, 'E', 'L', 'F', 0 };
	static char s_sHpBinary[]  = { 'B'-U2B, 'P'-U2B, 'A'-U2B, 'H'-U2B, 'E'-U2B, 'R'-U2B, 0 };
	static char s_sWinBinary[] = { 'M', 'Z', static_cast<char>(144), 0 };

	if (sChunk.StartsWith(s_sSolBinary))
	{
		sFiletype = "exe(SunOS)";
		return true; //(TRUE);
	}
	else if (sChunk.StartsWith(s_sHpBinary))
	{
		sFiletype = "exe(HP-UX)";
		return true; //(TRUE);
	}
	else if (sChunk.StartsWith(s_sWinBinary))
	{
		sFiletype = "exe(Win32)";
		return true; //(TRUE);
	}

	sFiletype = kExtension(sFilename).ToLower();

	long    iBytes = sChunk.size();
	for (long ii=0; ((ii<iBytes) && (ii<MAXCHUNK)); ++ii)
	{
		switch (sChunk[ii])
		{
		case 10: // ^J
		case 13: // ^M
		case ' ':
		case '\t':
			break;	// <-- char is printable/not binary
		default:
			if ((sChunk[ii] < ' ') || (sChunk[ii] > '~'))
				++iNotPrintable;
			break;
		}
	}

	// over 10% non-printable in order to be a binary file:
	bIsBinary = (((iNotPrintable*100)/iBytes) > 10);

	sFiletype += (bIsBinary) ? "(binary)" : "(text)";

	return (bIsBinary);

} // FileType

//-----------------------------------------------------------------------------
bool kExtendedDateFields (struct tm* ptmStruct, DFIELDS& DF)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	DF.iUnixTime = mktime (ptmStruct);

	// a few easy ones:
	DF.sHourOfDay   = kFormTimestamp (DF.iUnixTime, "%H");
	DF.sYear        = kFormTimestamp (DF.iUnixTime, "%Y");
	DF.sTimestamp   = kFormTimestamp (DF.iUnixTime, "%Y-%m-%d %H:%M:%S");
	DF.sDayOfWeek   = kFormat("{}:{}", ptmStruct->tm_wday + 1, kFormTimestamp (DF.iUnixTime, "%a"));
	DF.sMonthOfYear = kFormat("{}:{}", ptmStruct->tm_mon + 1,  kFormTimestamp (DF.iUnixTime, "%b"));

	// move time back to first day of week (prior Sunday):
	time_t iUnixTime = DF.iUnixTime - (ptmStruct->tm_wday * 24 * 60 * 60);
	ptmStruct = localtime (&iUnixTime);

	// format the first portion of the Week string:
	DF.sWeekSpan    = kFormat("{}-{}-{}({})-->", ptmStruct->tm_year+1900, ptmStruct->tm_mon+1, ptmStruct->tm_mday, kFormTimestamp (iUnixTime, "%a"));

	// move time to last day of week (this coming Saturday):
	iUnixTime += 6 * 24 * 60 * 60;
	ptmStruct = localtime (&iUnixTime);

	// format the tail portion:
	DF.sWeekSpan   += kFormat("{}-{}-{}({})", ptmStruct->tm_year+1900, ptmStruct->tm_mon+1, ptmStruct->tm_mday, kFormTimestamp (iUnixTime, "%a"));

	DF.sDayOfWeek.MakeUpper();
	DF.sWeekSpan.MakeUpper();
	DF.sMonthOfYear.MakeUpper();

	kDebug (2, "kExtendedDateFields:");
	kDebug (2, "  %-12s = %lu (secs since 1970)", "UnixTime", DF.iUnixTime);
	kDebug (2, "  %-12s = %s", "Timestamp",                   DF.sTimestamp);
	kDebug (2, "  %-12s = %s", "HourOfDay",                   DF.sHourOfDay);
	kDebug (2, "  %-12s = %s", "DayOfWeek",                   DF.sDayOfWeek);
	kDebug (2, "  %-12s = %s", "WeekSpan",                    DF.sWeekSpan);
	kDebug (2, "  %-12s = %s", "MonthOfYear",                 DF.sMonthOfYear);
	kDebug (2, "  %-12s = %s", "Year",                        DF.sYear);

	return true; //(TRUE);

} // kExtendedDateFields - v1

//-----------------------------------------------------------------------------
bool kExtendedDateFields (time_t iUnixTime, DFIELDS& DF)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	struct tm* ptmStruct = localtime (&iUnixTime);
	return (kExtendedDateFields (ptmStruct, DF));

} // kExtendedDateFields - v2

//-----------------------------------------------------------------------------
bool kExtendedDateFields (KString/*copy*/ sMMsDDsYYYY, KStringViewZ sHH, DFIELDS& DF)
//-----------------------------------------------------------------------------
{
	kDebug (1, "...");

	//				 0 1 2 3 4 5 6 7 8 9
	// hardcoded for M M / D D / Y Y Y Y
	if (sMMsDDsYYYY.MatchRegex ("^[0-9][0-9][-/:_][0-9][0-9][-/:_][0-9][0-9][0-9][0-9]$"))
	{
		kDebug (1, "kExtendeDateFields: invalid arg '{}' (must be MM/DD/YYYY)", sMMsDDsYYYY);
		return false;
	}

	sMMsDDsYYYY.Replace ("-", "/",  0, /*all=*/true);
	sMMsDDsYYYY.Replace (":", "/",  0, /*all=*/true);
	sMMsDDsYYYY.Replace ("_", "/",  0, /*all=*/true);

	auto parts = sMMsDDsYYYY.Split("/");

	// parse date string:
	auto iMonth		= parts[0].UInt16();
	auto iDay		= parts[1].UInt16();
	auto iYear		= parts[2].UInt16();
	auto iHour		= sHH.UInt16();

	if (iYear < 1900)
	{
		kDebug (1, "kExtendedDateFields: invalid year ({}), must be at least 1900.", iYear);
		return false;
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

	return (kExtendedDateFields (&tmStruct, DF));

} // kExtendedDateFields - v3

