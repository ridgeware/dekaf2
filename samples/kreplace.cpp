/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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
 */

#include "kreplace.h"
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
void KReplace::Stats::Print(KOutStream& os)
//-----------------------------------------------------------------------------
{
	os.FormatLine("{} files, {} hits, {}", iFiles, iFound, kFormBytes(iBytes, 1, " "));
}

//-----------------------------------------------------------------------------
KReplace::KReplace ()
//-----------------------------------------------------------------------------
{
	KInit(false).SetName(s_sProjectName).SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

	m_CLI.Option("e,expression"  ).Help("is regular expression"         ).Set(m_bRegularExpression, true);
	m_CLI.Option("s,search  [expression]").Help("search [expression]"   ).Set(m_sSearch);
	m_CLI.Option("r,replace [expression]").Help("replace with [expression]").Callback([&](KStringViewZ sArg) { m_sReplace = sArg; m_bReplace = true; });
	m_CLI.Option("R,recursive"   ).Help("recursive directory traversal" ).Set(m_bRecursive        , true );
	m_CLI.Option("q,quiet"       ).Help("quiet operation"               ).Set(m_bQuiet            , true );
	m_CLI.Option("u,unsafe"      ).Help("no backup files"               ).Set(m_bBackup           , false);
	m_CLI.Option("dry"           ).Help("dry run, no file modifications").Set(m_bDryRun           , true );
	m_CLI.Option("i,ignore-case" ).Help("ignore case in comparison"     ).Set(m_bIgnoreCase       , true );
	m_CLI.Option("v,invert-match").Help("show lines that do not match"  ).Set(m_bInvertMatch      , true );
	m_CLI.Option("V,version"     ).Help("show version information"      ).Final() ([&]() { ShowVersion(); });
	m_CLI.Option("revert"        ).Help("rename files with .old suffix to name without .old suffix").Set(m_bRevert, true );

	m_CLI.UnknownCommand([&](KOptions::ArgList& Args)
	{
		while (!Args.empty())
		{
			auto sArg = Args.pop();
			
			if (kFileExists(sArg))
			{
				m_Files.push_back(sArg);
			}
			else if (kDirExists(sArg))
			{
				KDirectory Dir(sArg, KFileType::FILE, m_bRecursive);

				for (auto& File : Dir)
				{
					m_Files.push_back(File);
				}
			}
		}
		Args.clear();
	});

} // ctor

//-----------------------------------------------------------------------------
void KReplace::Search(const KString sFilename)
//-----------------------------------------------------------------------------
{
	if (!m_bQuiet)
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

	if (!m_bQuiet)
	{
		KOut.Format(" - {}", kFormBytes(sContent.size(), 1, " "));
	}

	std::size_t iFound { 0 };

	if (!m_bReplace)
	{
		KString::size_type iPos = 0;

		if (m_bRegularExpression)
		{
			for (;;)
			{
				auto sMatch = sContent.MatchRegex(m_sSearch, iPos);

				if (sMatch.empty())
				{
					break;
				}

				++iFound;
				iPos = (sMatch.data() - sContent.data()) + sMatch.size();
			}
		}
		else
		{
			if (m_bIgnoreCase)
			{
				if (m_bIsASCII) sContent.MakeLowerASCII ();
				else            sContent.MakeLower      ();
			}

			for (;;)
			{
				iPos = sContent.find(m_sSearch, iPos);

				if (iPos == KString::npos)
				{
					break;
				}

				++iFound;
				++iPos;
			}
		}
	}
	else
	{
		if (m_bRegularExpression)
		{
			iFound = sContent.ReplaceRegex(m_sSearch, m_sReplace);
		}
		else
		{
			if (m_bIgnoreCase)
			{
				throw KError("case insensitive replace is not available when not in regex mode");
			}
			else
			{
				iFound = sContent.Replace (m_sSearch, m_sReplace);
			}
		}
	}

	if (!m_bQuiet)
	{
		KOut.Format(" - {} {}", m_bReplace ? "replaced" : "found", iFound);
	}

	if (iFound && m_bReplace && !m_bDryRun)
	{
		if (m_bBackup)
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

	if (!m_bQuiet)
	{
		KOut.WriteLine();
	}

	m_Stats.iFound += iFound;

} // Search

//-----------------------------------------------------------------------------
void KReplace::ShowVersion()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
void KReplace::Revert()
//-----------------------------------------------------------------------------
{
	// search in filelist for files with suffix .old, and rename them
	for (const auto& sOldFile : m_Files)
	{
		++m_Stats.iFiles;

		if (kExtension(sOldFile) == "old")
		{
			KString sNewName = kRemoveExtension(sOldFile);

			if (!m_bDryRun)
			{
				kRename(sOldFile, sNewName);
			}

			++m_Stats.iFound;

			if (!m_bQuiet)
			{
				kPrintLine("renamed {} to {}", sOldFile, sNewName);
			}
		}
	}

} // Revert

//-----------------------------------------------------------------------------
int KReplace::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	auto iRetVal = m_CLI.Parse(argc, argv, KOut);

	if (iRetVal	|| m_CLI.Terminate()) return iRetVal;

	if (m_Files.empty()) throw KError("no input files");
	if (m_sSearch.empty() && !m_bRevert) throw KError("no search expression");

	for (auto ch : m_sSearch)
	{
		if (static_cast<unsigned char>(ch) > 127)
		{
			// this search contains other than ASCII characters
			m_bIsASCII = false;
			break;
		}
	}

	if (m_bRegularExpression)
	{
		// enable multi-line mode (^ and $ will match any \n instead of only
		// start and end of the string)
		if (m_bIgnoreCase)
		{
			// and also enable case insensitive matching
			m_sSearch.insert(0, "(?im)");
		}
		else
		{
			m_sSearch.insert(0, "(?m)");
		}
	}
	else
	{
		m_sSearch.MakeLower();
	}

	if (m_bDryRun && !m_bQuiet)
	{
		KOut.WriteLine("dry run, no file modifications");
	}

	if (m_bRevert) 
	{
		Revert();
	}
	else
	{
		for (const auto& sFile : m_Files)
		{
			Search(sFile);
		}
	}

	if (!m_bQuiet)
	{
		m_Stats.Print(KOut);
	}

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return KReplace().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", KReplace::s_sProjectName, ex.what());
	}
	return 1;

} // main

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ KReplace::s_sProjectName;
constexpr KStringViewZ KReplace::s_sProjectVersion;
#endif
