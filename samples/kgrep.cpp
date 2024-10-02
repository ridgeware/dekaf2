
#include "kgrep.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kregex.h>
#include <dekaf2/kformat.h>
#include <dekaf2/kfilesystem.h>

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
void KGrep::Grep(const KString sFilename)
//-----------------------------------------------------------------------------
{
	KInFile InFile(sFilename);

	if (!InFile.is_open())
	{
		kPrintLine(KErr, " >> cannot open file - skipped: {}", sFilename);
		return;
	}

	for (const auto& sLine : InFile)
	{
		bool bFound = false;

		if (m_bRegularExpression)
		{
			bFound = (sLine.MatchRegex(m_sSearch).empty() == m_bInvertMatch);
		}
		else
		{
			if (m_bIgnoreCase)
			{
				if (m_bIsASCII) bFound = (sLine.ToLowerASCII().contains(m_sSearch) == !m_bInvertMatch);
				else            bFound = (sLine.ToLower     ().contains(m_sSearch) == !m_bInvertMatch);
			}
			else
			{
				bFound = (sLine.contains(m_sSearch) == !m_bInvertMatch);
			}
		}

		if (bFound)
		{
			if (m_bPrintFilename)
			{
				kPrint("{}:", sFilename);
			}

			kWriteLine(sLine);
		}
	}

} // Grep

//-----------------------------------------------------------------------------
void KGrep::AddFiles (KOptions::ArgList& Args)
//-----------------------------------------------------------------------------
{
	while (!Args.empty())
	{
		auto sArg = Args.pop();

		if (m_sSearch.empty())
		{
			m_sSearch = sArg;
			continue;
		}

		KFileStat Stat(sArg);

		switch (Stat.Type())
		{
			case KFileType::FILE:
				m_Files.push_back(sArg);
				break;

			case KFileType::DIRECTORY:
			{
				KDirectory Dir(sArg, KFileType::FILE, m_bRecursive);

				for (auto& File : Dir)
				{
					m_Files.push_back(File);
				}
				break;
			}

			case KFileType::UNEXISTING:
				PrintError(kFormat("file not found: {}", sArg));
				break;

			default:
				PrintError(kFormat("bad file type '{}': {}", Stat.Type().Serialize(), sArg));
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
	{
		// setup CLI option parsing
		KOptions Options(true, KLog::STDOUT, /*bThrow*/true);

		Options.SetAdditionalArgDescription("<search expression> <file[s]>");

		Options.Option("e,regexp"      ).Help("is regular expression"         ).Set(m_bRegularExpression , true);
		Options.Option("R,recursive"   ).Help("recursive directory traversal" ).Set(m_bRecursive         , true);
		Options.Option("i,ignore-case" ).Help("ignore case in comparison"     ).Set(m_bIgnoreCase        , true);
		Options.Option("v,invert-match").Help("show lines that do not match"  ).Set(m_bInvertMatch       , true);
		Options.Option("H"             ).Help("always print file name headers").Set(m_bPrintFilename     , true);
		Options.Option("V,version"     ).Help("show version information"      ).Final() ([&]() { ShowVersion(); });

		Options.RegisterUnknownCommand([&](KOptions::ArgList& Args) { AddFiles(Args); });

		auto iRetVal = Options.Parse(argc, argv, KOut);

		if (iRetVal	|| Options.Terminate()) return iRetVal;

		m_sProgramName = Options.GetProgramName();
	}

	if (m_Files  .empty()) throw KError("no input files"      );
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

	for (const auto& sFile : m_Files) Grep(sFile);

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
constexpr KStringViewZ kreplace::s_sProjectName;
constexpr KStringViewZ kreplace::s_sProjectVersion;
#endif
