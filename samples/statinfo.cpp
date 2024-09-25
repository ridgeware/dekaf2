#include <dekaf2/dekaf2.h>
#include <dekaf2/kreader.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kregex.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/klog.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kwriter.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/ktime.h>
#include <dekaf2/kformat.h>

using namespace dekaf2;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DateFields
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	DateFields () = default;
	DateFields (KUnixTime UnixTime);
	DateFields (KString/*copy*/ sMMsDDsYYYY, KStringView sHH);

	bool       ok()  
	{
		return iUnixTime.ok();
	}

	KUnixTime  iUnixTime;
	KString    sTimestamp;   // 2001-03-31 12:00:00
	KString    sHourOfDay;   // 12
	KString    sDayOfWeek;   // 5:THU
	KString    sWeekSpan;    // 2001-03-04(SUN)-->2001-03-10(SAT)
	KString    sMonthOfYear; // 03:MAR
	KString    sYear;        // 2001

}; // DateFields

//-----------------------------------------------------------------------------
DateFields::DateFields (KUnixTime UnixTime)
//-----------------------------------------------------------------------------
{
	iUnixTime = UnixTime;

	KUTCTime UTCTime(UnixTime);

	// a few easy ones:
	sHourOfDay   = kFormat ("{:%H}", UTCTime);
	sYear        = kFormat ("{:%Y}", UTCTime);
	sTimestamp   = kFormat ("{:%Y-%m-%d %H:%M:%S}", UTCTime);
	sDayOfWeek   = kFormat ("{}:{:%a}", UTCTime.weekday().c_encoding() + 1, UTCTime).ToUpper();
	sMonthOfYear = kFormat ("{}:{:%b}", unsigned(UTCTime.month()), UTCTime).ToUpper();

	// move time back to first day of week (prior Sunday):
	KDate LastSunday(UTCTime);

	if (LastSunday.weekday() != chrono::Sunday)
	{
		LastSunday.to_previous(chrono::Sunday);
	}

	KDate NextSaturday(UTCTime);

	if (NextSaturday.weekday() != chrono::Saturday)
	{
		NextSaturday.to_next(chrono::Saturday);
	}

	sWeekSpan = kFormat ("{:%Y-%m-%d(%a)}-->{:%Y-%m-%d(%a)}", LastSunday, NextSaturday).ToUpper();

	if (kWouldLog(2))
	{
		kDebug (2, "kExtendedDateFields:");
		kDebug (2, "  {:<12} = {} (secs since 1970)", "UnixTime", iUnixTime.to_time_t());
		kDebug (2, "  {:<12} = {}", "Timestamp",                  sTimestamp);
		kDebug (2, "  {:<12} = {}", "HourOfDay",                  sHourOfDay);
		kDebug (2, "  {:<12} = {}", "DayOfWeek",                  sDayOfWeek);
		kDebug (2, "  {:<12} = {}", "WeekSpan",                   sWeekSpan);
		kDebug (2, "  {:<12} = {}", "MonthOfYear",                sMonthOfYear);
		kDebug (2, "  {:<12} = {}", "Year",                       sYear);
	}

} // DateFields ctor

//-----------------------------------------------------------------------------
DateFields::DateFields (KString/*copy*/ sMMsDDsYYYY, KStringView sHH)
//-----------------------------------------------------------------------------
{
	//				 0 1 2 3 4 5 6 7 8 9
	// hardcoded for M M / D D / Y Y Y Y

	if (sMMsDDsYYYY.size() == 10)
	{
		sMMsDDsYYYY.Replace ("-", "/",  0, /*all=*/true);
		sMMsDDsYYYY.Replace (":", "/",  0, /*all=*/true);
		sMMsDDsYYYY.Replace ("_", "/",  0, /*all=*/true);
	}
	// parse
	KUTCTime UTCTime("MM/DD/YYYY", sMMsDDsYYYY);

	// got a valid date?
	if (!UTCTime.ok())
	{
		kDebug (1, "invalid arg '{}' (must be MM/DD/YYYY)", sMMsDDsYYYY);
		return;
	}

	if (UTCTime.year() < chrono::year(1900))
	{
		kDebug (1, "invalid year ({:%Y}), must be at least 1900.", UTCTime);
		return;
	}

	UTCTime.hour(chrono::hours(sHH.UInt16()));

	// now call the primary constructor
	*this = KUnixTime(UTCTime);

} // DateFields ctor

// =============================== StatInfo ==================================

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class StatInfo : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	int Main(int argc, char* argv[]);

//------
private:
//------

	void DoStat             (KString/*copy*/ sPath);
	bool FileType           (KStringViewZ sFilename, KString& sFiletype);
	void OpenDirectory      (KStringViewZ sDirectory);
	void WriteHeader        ();

	KOutStream*     m_OutStream  { &KOut };
	chrono::seconds m_tOffset    { chrono::seconds(0) };
	bool            m_bNoHeader  { false };
	bool            m_bRecursive { false };

}; // StatInfo

//-----------------------------------------------------------------------------
int StatInfo::Main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	// initialize, do not start a signal handler thread
	KInit(false);

	// we will throw when setting errors
	SetThrowOnError(true);

	// setup CLI option parsing
	KOptions Options(true, KLog::STDOUT, /*bThrow*/true);

	Options.SetAdditionalArgDescription("{ <file[s]> | - }");

	Options.Option("o,offset"   ).Help("add an offset (in secs) to UTC").Set(m_tOffset);
	Options.Option("n,noheader" ).Help("eliminate header row"          ).Set(m_bNoHeader , true);
	Options.Option("r,recursive").Help("traverse directories"          ).Set(m_bRecursive, true);

	// catch any args that are not known options - those are file names to inspect
	Options.UnknownCommand([&](KOptions::ArgList& Args)
	{
		WriteHeader();

		while (!Args.empty())
		{
			auto sFile = Args.pop();

			if (sFile == "-")
			{
				// read file names from stdin
				for (auto& sFile : KIn)
				{
					sFile.Trim();
					DoStat(sFile);
				}
			}
			else
			{
				DoStat(sFile);
			}
		}
	});

	Options.Parse(argc, argv);

	return (0);

} // StatInfo::Main

//-----------------------------------------------------------------------------
void StatInfo::OpenDirectory (KStringViewZ sDirectory)
//-----------------------------------------------------------------------------
{
	KDirectory Dir(sDirectory, KFileType::FILE | KFileType::DIRECTORY | KFileType::SYMLINK);

	Dir.RemoveHidden();

	for (auto& File : Dir)
	{
		DoStat(kFormat("{}{}{}", sDirectory, kDirSep, File.Filename()));
	}

} // OpenDirectory

//-----------------------------------------------------------------------------
void StatInfo::DoStat (KString/*copy*/ sPath)
//-----------------------------------------------------------------------------
{
	// skip incoming blank lines and comments:
	if (!sPath || sPath.starts_with ('#') || sPath.In(".,.."))
	{
		return;
	}

	// automatically extract filename from SWISH output:
	// 288 /full/path/to/file/with possible spaces.txt "Title with possible spaces" 87838
	if (sPath.contains (" \""))
	{
		auto iSpace = sPath.find(" ");
		if (iSpace > 0)
		{
			sPath = sPath.Mid (iSpace);
			sPath.ClipAt (" \"");
			sPath.Trim();
		}
	}

	KFileStat StatStruct(sPath);

	if (!StatStruct.Exists())
	{
		if (StatStruct.GetLastErrorCode())
		{
			kPrintLine (*m_OutStream, "# {}", StatStruct.GetLastError()); // output a comment if stat() fails:
		}
		else
		{
			kPrintLine (*m_OutStream, "# {}: file not found", sPath);
		}
		return;
	}

	KUnixTime tLastMod;

	// watch for prelabelled somedir/K1231373376-IMG-343.jpg and use it for lastmod:
	KString sBase = kBasename (sPath);
	if (sBase.MatchRegex ("^K[0-9]{10}"))
	{
		sBase = sBase.Left  (KStringView("K1231373376").size());
		sBase = sBase.Right (KStringView("1231373376").size());
		kDebug (1, "extracting lastmod info from filename: {}", sPath);
		tLastMod = KUnixTime::from_time_t(sBase.Int64());
		kDebug (1, "off filename: atoi({}) = st_mtime = {} > {:%T %F}\n", sBase, tLastMod.to_time_t(), tLastMod);
	}

	KString sOwner = kFirstNonEmpty(kGetUsername(StatStruct.UID()), kFormat("{}", StatStruct.UID()));
	KString sFiletype;

	if (StatStruct.IsDirectory())
	{
		if (m_bRecursive)
		{
			sPath.remove_suffix(kDirSep);
			OpenDirectory(sPath);
			return;
		}
		else
		{
			sFiletype = "dir";
		}
	}
	else if (StatStruct.IsFile())
	{
		if (!tLastMod.ok())
		{
			// only run imagemagick if we do not yet have a timestamp off the filename
			KString sOutput;

			// use ImageMagick to get date photo was taken:
			auto    sCmd = kFormat ("identify -format '%[exif:DateTimeOriginal] %[exif:OffsetTimeOriginal]' '{}'", sPath);
			kDebug  (1, sCmd);
			kSystem (sCmd, sOutput);

			//            output: 2024:04:18 16:36:42 +02:00
			tLastMod = KUnixTime("YYYY:MM:DD hh:mm:ss ZZZ:ZZ", sOutput);

			if (tLastMod.ok())
			{
				kDebug (1, "off-exif: {}", sOutput);
				kDebug (1, "{}: off exif:DateTimeOriginal: {} > {:%T %F}", sPath, tLastMod.to_time_t(), tLastMod);
			}
		}

		FileType (sPath, sFiletype);
	}
	else
	{
		// pipes, sockets, devices ..
		sFiletype = StatStruct.Type().Serialize();
	}

	if (!tLastMod.ok())
	{
		tLastMod = StatStruct.ModificationTime();
		kDebug (1, "{}: imagemagick failed, using mtime off stat: {} > {:%T %F}", sPath, tLastMod.to_time_t(), tLastMod);
	}

	if (m_tOffset != chrono::seconds(0))
	{
		tLastMod += m_tOffset;
		kDebug(1, "offset   = {}\n", m_tOffset);
		kDebug(1, "adjusted = {}\n", tLastMod.to_time_t());
	}

	auto sLastMod = kFormat ("{:%Y-%m-%d %a %H:%M:%S %Z}", tLastMod);

	auto sFinal = kFormat ("{}|{}|{}|{}|{}|{}|{}",
		tLastMod.to_time_t(),
		sLastMod,
		StatStruct.Size(),
		sOwner,
		StatStruct.UID(),
		sFiletype,
		sPath);

	kDebug (1, sFinal);
	kWriteLine (*m_OutStream, sFinal);

//	DateFields DF(tLastMod);

} // DoStat

//-----------------------------------------------------------------------------
bool StatInfo::FileType (KStringViewZ sFilename, KString& sFiletype)
//-----------------------------------------------------------------------------
{
	constexpr std::size_t MAXCHUNK = 512;
	KString sChunk;

	kReadBinaryFile (sFilename, sChunk, MAXCHUNK);

	std::size_t iNotPrintable = 0;
	bool        bIsBinary     = false;

	if (sChunk.empty())
	{
		sFiletype = "EMPTY";
		return false;
	}

	constexpr char U2B = 64 ; // <-- ('A' - ^A)

	static constexpr char s_sSolBinary[] = { 127, 'E', 'L', 'F', 0 };
	static constexpr char s_sHpBinary[]  = { 'B'-U2B, 'P'-U2B, 'A'-U2B, 'H'-U2B, 'E'-U2B, 'R'-U2B, 0 };
	static constexpr char s_sWinBinary[] = { 'M', 'Z', static_cast<char>(144), 0 };

	constexpr KStringView sSolBinary { s_sSolBinary };
	constexpr KStringView sHpBinary  { s_sHpBinary  };
	constexpr KStringView sWinBinary { s_sWinBinary };

	if (sChunk.starts_with(sSolBinary))
	{
		sFiletype = "exe(SunOS)";
		return true;
	}
	else if (sChunk.starts_with(sHpBinary))
	{
		sFiletype = "exe(HP-UX)";
		return true;
	}
	else if (sChunk.starts_with(sWinBinary))
	{
		sFiletype = "exe(Win32)";
		return true;
	}

	sFiletype = kExtension(sFilename).ToLower();

	for (auto ch : sChunk)
	{
		switch (ch)
		{
		case 10: // ^J
		case 13: // ^M
		case ' ':
		case '\t':
			break;	// <-- char is printable/not binary
		default:
			if ((ch < ' ') || (ch > '~'))
				++iNotPrintable;
			break;
		}
	}

	// over 10% non-printable in order to be a binary file:
	bIsBinary = (((iNotPrintable*100)/sChunk.size()) > 10);

	sFiletype += (bIsBinary) ? "(binary)" : "(text)";

	return (bIsBinary);

} // FileType

//-----------------------------------------------------------------------------
void StatInfo::WriteHeader()
//-----------------------------------------------------------------------------
{
	if (!m_bNoHeader)
	{
		kWriteLine (*m_OutStream, "#lastmod-utc|lastmod-string|bytes|owner|uid|type|pathname");
		m_bNoHeader = true;
	}

} // WriteHeader

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return StatInfo().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		kPrintLine(KErr, "#>> {}: {}", *argv, ex.what());
	}

	return 1;

} // main
