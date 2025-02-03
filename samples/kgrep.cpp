
#include "kgrep.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kregex.h>
#include <dekaf2/kformat.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kparallel.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
KGrep::KGrep ()
//-----------------------------------------------------------------------------
{
	KInit(false).SetName(s_sProjectName);

} // ctor

//-----------------------------------------------------------------------------
void KGrep::PrintError(KStringView sError)
//-----------------------------------------------------------------------------
{
	if (m_sProgramName.empty()) m_sProgramName = s_sProjectName;
	kPrintLine(KErr, ">> {}: {}", m_sProgramName, sError);

} // PrintError

//-----------------------------------------------------------------------------
KString KGrep::Highlight(KStringView sIn, const std::vector<KStringView>& Matches)
//-----------------------------------------------------------------------------
{
	KString sOut;
	sOut.reserve(sIn.size() + Matches.size() * 9);

	KStringView::size_type iTotalStart = 0;

	KStringView sHighlightOn  = "\033[01;31m";
	KStringView sHighlightOff = "\033[m";

	for (auto Match : Matches)
	{
		auto iStart = Match.data() - sIn.data();
		auto iSize  = Match.size();

		sOut += sIn.ToView(iTotalStart, iStart - iTotalStart);
		sOut += sHighlightOn;
		sOut += sIn.ToView(iStart, iSize);
		sOut += sHighlightOff;

		iTotalStart += (iStart - iTotalStart) + iSize;
	}

	sOut += sIn.ToView(iTotalStart);

	return sOut;

} // Highlight

//-----------------------------------------------------------------------------
void KGrep::Grep(KInStream& InStream, KStringView sFilename)
//-----------------------------------------------------------------------------
{
	std::vector<KStringView> Matches;

	for (const auto& sLine : InStream)
	{
		bool bFound = false;
		std::size_t iFoundAt = 0;
		Matches.clear();

		if (m_bRegularExpression)
		{

			for (;;)
			{
				auto sMatch = sLine.MatchRegex(m_sSearch, iFoundAt);

				if (sMatch.empty() == m_bInvertMatch)
				{
					bFound = true;

					if (m_bInvertMatch)
					{
						break;
					}

					Matches.push_back(sMatch);
					iFoundAt  = sMatch.data() - sLine.data();
					iFoundAt += sMatch.size();
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			for (;;)
			{
				if (m_bIgnoreCase)
				{
					if (m_bIsASCII) iFoundAt = sLine.ToLowerASCII().find(m_sSearch, iFoundAt);
					else            iFoundAt = sLine.ToLower     ().find(m_sSearch, iFoundAt);
				}
				else
				{
					iFoundAt = sLine.find(m_sSearch, iFoundAt);
				}
				
				if ((iFoundAt != KString::npos) != m_bInvertMatch)
				{
					bFound = true;

					if (m_bInvertMatch)
					{
						break;
					}

					Matches.push_back(KStringView(sLine.data() + iFoundAt, m_sSearch.size()));
					iFoundAt += m_sSearch.size();
				}
				else
				{
					break;
				}
			}
		}

		if (bFound)
		{
			if (m_bPrintFilename)
			{
				kPrint("{}:", sFilename);
			}

			if (!m_bInvertMatch && m_bIsTerminal)
			{
				kWriteLine(Highlight(sLine, Matches));
			}
			else
			{
				kWriteLine(sLine);
			}
		}
	}

} // Grep

//-----------------------------------------------------------------------------
void KGrep::AddFiles (std::vector<KStringViewZ> Files)
//-----------------------------------------------------------------------------
{
	for (auto sFile : Files)
	{
		if (m_sSearch.empty())
		{
			m_sSearch = sFile;
			continue;
		}

		KFileStat Stat(sFile);

		switch (Stat.Type())
		{
			case KFileType::FILE:
				m_Files.push_back(sFile);
				break;

			case KFileType::DIRECTORY:
			{
				KDirectory Dir(sFile, KFileType::FILE, m_bRecursive);

				for (auto& File : Dir)
				{
					// using operator const KString&()
					m_Files.push_back(File);
				}
				break;
			}

			case KFileType::UNEXISTING:
				PrintError(kFormat("file not found: {}", sFile));
				break;

			default:
				PrintError(kFormat("bad file type '{}': {}", Stat.Type().Serialize(), sFile));
				break;
		}
	}

} // AddFiles

//-----------------------------------------------------------------------------
void KGrep::ShowVersion()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}",s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int KGrep::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	KStopTime OverallTime;

	{
		// setup CLI option parsing
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options.SetAdditionalArgDescription("<search expression> <file[s]>");

		m_bRegularExpression = Options("E,regexp             : is a regular expression"       , false);
		m_bRecursive         = Options("r,recursive          : recursive directory traversal" , false);
		m_bIgnoreCase        = Options("i,ignore-case        : ignore case in comparison"     , false);
		m_bInvertMatch       = Options("v,invert-match       : show lines that do not match"  , false);
		m_bPrintFilename     = Options("H                    : always print file name headers", false);
		m_iParallel          = Options("p,parallel <threads> : parallel threads for search, defaults to cpu cores", 0UL);
		bool bShowVersion    = Options("V,version            : show version information"      , false);

		AddFiles(Options.GetUnknownCommands());

		// do a final check if all required options were set
		if (!Options.Check()) return 1;

		m_sProgramName = Options.GetProgramName();

		if (bShowVersion)
		{
			ShowVersion();
			return 0;
		}

		if (!m_iParallel) m_iParallel = std::thread::hardware_concurrency();
	}

	if (m_sSearch.empty()) throw KError("no search expression");

	for (auto ch : m_sSearch)
	{
		if (static_cast<unsigned char>(ch) > 127)
		{
			// this search contains other than ASCII characters
			m_bIsASCII = false;
			break;
		}
	}

	if (m_bIgnoreCase)
	{
		if (m_bRegularExpression)
		{
			m_sSearch.insert(0, "(?i)");
		}
		else
		{
			if (m_bIsASCII) m_sSearch.MakeLowerASCII();
			else            m_sSearch.MakeLower();
		}
	}

	if (!m_bPrintFilename && m_Files.size() > 1) m_bPrintFilename = true;

	m_bIsTerminal = kStdOutIsTerminal();

	if (m_Files.empty())
	{
		Grep(KIn, "stdin");
	}
	else
	{
		kDebug(2, "threads: {}, files: {}", m_iParallel, m_Files.size());
		kParallelForEach(m_Files, [this](KStringViewZ sFilename)
		{
			KInFile InFile(sFilename);

			if (!InFile.is_open())
			{
				kPrintLine(KErr, ">> cannot open file - skipped: {}", sFilename);
			}
			else
			{
				Grep(InFile, sFilename);
			}
		}, m_iParallel);
	}

	kDebug(1, "search took {}", OverallTime.elapsed());

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return KGrep().Main(argc, argv);
	}
	catch (const std::exception& ex)
	{
		kPrintLine(KErr, ">> {}: {}", kBasename(*argv), ex.what());
	}

	return 1;

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ KGrep::s_sProjectName;
constexpr KStringViewZ KGrep::s_sProjectVersion;
#endif
