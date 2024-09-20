
#include "dekaf.h"
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <errno.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <pwd.h>   // for getpwuid()

void DoStat   (char* pszPath, time_t tOffset=0);
int  FileType (const char* pszFilename, char szFiletype[20+1]);

LPCTSTR g_Usage[] = {
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

//-----------------------------------------------------------------------------
int main_dekaf (ULONG iKID, int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	kInit ("STAT", "stdout", ".statinfo.dbg");
	kSetDebug (0);

	if (argc <= 1) {
		for (UINT ii=0; g_Usage[ii]; ++ii) {
			printf ("%s\n", g_Usage[ii]);
		}
		return (1);
	}

	time_t tOffset   = 0;
	bool   tNoHeader = false;

	for (int ii=1; ii<argc; ++ii)
	{
		if      (strmatch (argv[ii], "-d"))   {  kSetDebug (1); }
		else if (strmatch (argv[ii], "-dd"))  {  kSetDebug (2); }
		else if (strmatch (argv[ii], "-ddd")) {  kSetDebug (3); }
		else if (strmatch (argv[ii], "-offset"))
		{
			tOffset = katoh (argv[++ii]);
		}
		else if (strmatch (argv[ii], "-noheader"))
		{
			tNoHeader = true;
		}
		else if (strmatch (argv[ii], "-"))
		{
			enum {MAX=1000};
			char szPath[MAX+1];

			if (!tNoHeader) {
				printf ("#lastmod-utc|lastmod-string|bytes|owner|uid|type|pathname\n");
			}

			while (fgets (szPath, MAX, stdin)) {
				// trim whitespace:
				while (*szPath && (szPath[strlen(szPath)-1] <= ' '))
					szPath[strlen(szPath)-1] = 0;
				DoStat (szPath, tOffset);
			}
		}
		else
		{
			if (!tNoHeader) {
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
	KREGEX  regex;
	kDebugLog ("identify -format '%%[exif:DateTimeOriginal]' '%s'", pszPath);
	ksystem (sOutput, "identify -format '%%[exif:DateTimeOriginal]' '%s'", pszPath);
	if (regex.Matches (sOutput.c_str(), "[0-9][0-9][0-9][0-9]:[0-9][0-9]:[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]"))
	{
		// raw identify: 2016:08:13 13:52:11
		kDebugLog ("off-exif: %s", sOutput.c_str());
		sOutput.SubString (":", "-"); // only twice
		sOutput.SubString (":", "-");
		kDebugLog ("parsable: %s", sOutput.c_str());
		tLastMod = kParseDate (sOutput.c_str());
		kDebugLog ("%s: off exif:DateTimeOriginal: %"PHUGE, pszPath, (SHUGE) tLastMod);
	}
	else {
		tLastMod = StatStruct.st_mtime;
		kDebugLog ("%s: imagemagick failed, using mtime off stat: %"PHUGE, pszPath, (SHUGE) tLastMod);
	}

	// watch for prelabelled somedir/K1231373376-IMG-343.jpg and use it for lastmod:
	KString sBase (pszPath);
	char* lastslash = strrchr (pszPath, '/');
	if (lastslash) {
		sBase = lastslash + 1;
	}
	if (sBase.FindRegex ("^K[0-9]{10}")) {
		sBase = sBase.Left  (strlen("K1231373376"));
		sBase = sBase.Right (strlen("1231373376"));
		kDebugLog ("extracting lastmod info from filename: %s", pszPath);
		tLastMod = atoi (sBase.c_str());
		kDebugLog ("off filename: atoi(%s) = st_mtime = %" PHUGE "\n", sBase.c_str(), (SHUGE) tLastMod);
	}

	if (tOffset) {
		tLastMod += tOffset;
		kDebugLog ("offset   = %" PHUGE "\n", (SHUGE) tOffset);
		kDebugLog ("adjusted = %" PHUGE "\n", (SHUGE) tLastMod);
	}

	char szLastMod[50+1];
	char szOwner[50+1];
	struct tm*     pTimeStruct = localtime (&tLastMod);
	struct passwd* pPass       = getpwuid (StatStruct.st_uid);
	char szFiletype[20+1];

	strftime (szLastMod, 50, "%Y-%m-%d %a %H:%M:%S %Z", pTimeStruct);

	if (pPass)
		strncpy (szOwner, pPass->pw_name, 50);
	else
		sprintf (szOwner, "%lu", (ULONG) StatStruct.st_uid);

	if (StatStruct.st_mode & S_IFDIR)
		strcpy (szFiletype, "dir");
	else
		FileType (pszPath, szFiletype);

	printf ("%" PHUGE "|%s|%" PHUGE "|%s|%" PHUGE "|%s|%s\n", 
		(SHUGE) tLastMod,
		szLastMod,
		(SHUGE) StatStruct.st_size,
		szOwner,
		(SHUGE) StatStruct.st_uid,
		szFiletype,
		pszPath);

} // DoStat

//-----------------------------------------------------------------------------
int FileType (const char* pszFilename, char szFiletype[20+1])
//-----------------------------------------------------------------------------
{
	*szFiletype = 0;

	enum  {MAXCHUNK = 512};
	char  Chunk[MAXCHUNK+1];
	int	  fd	 = open (pszFilename, O_RDONLY);

	if (!fd) {
		strcpy (szFiletype, "ERR");
		return (FALSE);
	}

	long  iBytes = read (fd, Chunk, MAXCHUNK);
	UINT  iNotPrintable = 0;
	int   fIsBinary	    = FALSE;

	Chunk[iBytes] = 0;
	close (fd);

	if (iBytes <= 0) {
		strcpy (szFiletype, "EMPTY");
		return (FALSE);
	}

	enum { U2B = 64 }; // <-- ('A' - ^A)
	static char s_pszSolBinary[] = { 127, 'E', 'L', 'F', 0 };
	static char s_pszHpBinary[]  = { 'B'-U2B, 'P'-U2B, 'A'-U2B, 'H'-U2B, 'E'-U2B, 'R'-U2B, 0 };
	static char s_pszWinBinary[] = { 'M', 'Z', 144, 0 };

	if (strmatchN (Chunk, s_pszSolBinary)) {
		strcpy (szFiletype, "exe(SunOS)");
		return (TRUE);
	}
	if (strmatchN (Chunk, s_pszHpBinary)) {
		strcpy (szFiletype, "exe(HP-UX)");
		return (TRUE);
	}
	if (strmatchN (Chunk, s_pszWinBinary)) {
		strcpy (szFiletype, "exe(Win32)");
		return (TRUE);
	}

	char* lastdot = (char*)strrchr (pszFilename, '.');
	if (lastdot && (strlen (lastdot+1) < 5)) {
		strcpy (szFiletype, lastdot+1);
		ktolower (szFiletype);
		if (strstr (szFiletype, "htm"))
			strcpy (szFiletype, "html");
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

	strcat (szFiletype, fIsBinary ? "(binary)" : "(text)");


	return (fIsBinary);

} // FileType

