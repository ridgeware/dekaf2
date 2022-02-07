
#include "kreplace.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>
#include <dekaf2/kregex.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
void kreplace::Stats::Print(KOutStream& os)
//-----------------------------------------------------------------------------
{
	os.FormatLine("{} files, {} hits, {} kb", iFiles, iFound, iBytes / 1024);
}

//-----------------------------------------------------------------------------
kreplace::kreplace ()
//-----------------------------------------------------------------------------
{
	KInit(false).SetName(s_sProjectName).SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

	m_CLI.Option("e,expression").Help("is regular expression")
	([&]
	{
		m_Config.bRegularExpression = true;
	});

	m_CLI.Option("s,search").Help("search [expression]")
	([&](KStringViewZ sSearch)
	{
		m_Config.sSearch = sSearch;
	});

	m_CLI.Option("r,replace").Help("replace with [expression]")
	([&](KStringViewZ sReplace)
	{
		m_Config.bReplace = true;
		m_Config.sReplace = sReplace;
	});

	m_CLI.Option("R,recursive").Help("recursive directory traversal")
	([&](KStringViewZ sReplace)
	{
		m_Config.bReplace = true;
		m_Config.sReplace = sReplace;
	});

	m_CLI.Option("q,quiet").Help("quiet operation")
	([&]()
	{
		m_Config.bQuiet = true;
	});

	m_CLI.Option("u,unsafe").Help("no backup files")
	([&]()
	{
		m_Config.bBackup = false;
	});

	m_CLI.Option("dry").Help("dry run, no file modifications")
	([&]
	{
		m_Config.bDryRun = true;
	});

	m_CLI.Option("v,version").Help("show version information")
	([&]()
	{
		ShowVersion();
		m_Config.bTerminate = true;
	});

	m_CLI.RegisterUnknownCommand([&](KOptions::ArgList& Args)
	{
		for (auto& sArg : Args)
		{
			if (kFileExists(sArg))
			{
				kDebug(1, "adding: {}", sArg);
				m_Config.Files.push_back(sArg);
			}
			else if (kDirExists(sArg))
			{
				kDebug(1, "directory: {}", sArg);
				KDirectory Dir(sArg, KFileType::FILE, m_Config.bRecursive);

				for (auto& File : Dir)
				{
					kDebug(1, "adding: {}", File.Filename());
					m_Config.Files.push_back(File);
				}
			}
		}
		Args.clear();
	});

} // ctor

//-----------------------------------------------------------------------------
void kreplace::Search(const KString sFilename)
//-----------------------------------------------------------------------------
{
	if (!m_Config.bQuiet)
	{
		KOut.Format("reading {}", sFilename);
	}

	KString sContent;

	if (!kReadAll(sFilename, sContent))
	{
		KErr.FormatLine(" >> cannot open file - skipped: {}", sFilename);
		return;
	}

	++m_Stats.iFiles;
	m_Stats.iBytes += sContent.size();

	if (!m_Config.bQuiet)
	{
		KOut.Format(" - {} kb", sContent.size() / 1024);
	}

	std::size_t iFound { 0 };

	if (m_Config.bRegularExpression)
	{
		iFound = sContent.ReplaceRegex(m_Config.sSearch, m_Config.sReplace);
	}
	else
	{
		iFound = sContent.Replace(m_Config.sSearch, m_Config.sReplace);
	}

	if (!m_Config.bQuiet)
	{
		KOut.Format(" - {} {}", m_Config.bReplace ? "replaced" : "found", iFound);
	}

	if (iFound && m_Config.bReplace && !m_Config.bDryRun)
	{
		if (m_Config.bBackup)
		{
			auto os = kCreateFileWithBackup(sFilename);

			if (os)
			{
				os->Write(sContent);
			}
			else
			{
				KErr.FormatLine("\n>> cannot write to file - skipped: ", sFilename);
			}
		}
		else
		{
			if (!kWriteFile(sFilename, sContent))
			{
				KErr.FormatLine("\n>> cannot write to file - skipped: ", sFilename);
			}
		}
	}

	if (!m_Config.bQuiet)
	{
		KOut.WriteLine();
	}

	m_Stats.iFound += iFound;

} // Search

//-----------------------------------------------------------------------------
void kreplace::ShowVersion()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int kreplace::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	{
		auto iRetVal = m_CLI.Parse(argc, argv, KOut);

		if (iRetVal	|| m_Config.bTerminate)
		{
			// either error or completed
			return iRetVal;
		}

		if (m_Config.Files.empty())
		{
			throw KError("no input files");
		}

		if (m_Config.sSearch.empty())
		{
			throw KError("no search expression");
		}

		if (m_Config.bDryRun && !m_Config.bQuiet)
		{
			KOut.WriteLine("dry run, no file modifications");
		}

		for (const auto& sFile : m_Config.Files)
		{
			Search(sFile);
		}

		if (!m_Config.bQuiet)
		{
			m_Stats.Print(KOut);
		}
	}

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return kreplace().Main (argc, argv);
	}
	catch (const KError& ex)
	{
		KErr.FormatLine(">> {}: {}", kreplace::s_sProjectName, ex.what());
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", kreplace::s_sProjectName, ex.what());
	}
	return 1;

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ kreplace::s_sProjectName;
constexpr KStringViewZ kreplace::s_sProjectVersion;
#endif
