/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2020, Ridgeware, Inc.
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

#include "kdefinitions.h"
#include "dekaf2.h"
#include "klog.h"
#include "kstring.h"
#include "kreader.h"
#include "kwriter.h"
#include "kfilesystem.h"
#include "koptions.h"
#include "kjson.h"
#include "kreplacer.h"
#include "kurl.h"
#include "ksystem.h"
#include "kxml.h"

using namespace DEKAF2_NAMESPACE_NAME;

constexpr KStringView g_Synopsis[] = {
	" ",
	"dekaf2project -- generate dekaf2 based project setups",
	" ",
	"usage: dekaf2project [<Options>] <ProjectName>",
	"",
	" where options are:"
	"",
	"  -t,type <ProjectType>  :: type of project, default = cli",
	"  -p,path <ProjectPath>  :: path to project root, name is added, default = current directory",
	"  -s,sso  <URL> <Scope>  :: URL and scope of SSO server, default = none",
	"  -v,version <Version>   :: version string, default = 0.0.1"
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class CreateProject
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	CreateProject();
	int Main (int argc, char* argv[]);

	static void ShowAllTemplates();

//----------
private:
//----------

	/// set fixed variables
	void SetupVariables();
	/// compute remaining values from set values
	void FinishSetup();
	void PrintReplacedFile (KStringViewZ sOutFile, KStringViewZ sInFile);
	void CreateBuildSystem (KStringView sBuildType);
	void InstallTemplateDir();
	void CopyDirRecursive(KStringViewZ sOutputDir, KStringViewZ sFromDir, uint16_t iRecursion = 1);

	KString m_sProjectType { "cli" };
	KString m_sProjectName;
	KString m_sProjectPath;
	KString m_sProjectVersion { "0.0.1" };
	KURL m_SSOProvider;
	KString m_SSOScope;
	bool m_bIsDone { false };

	KString m_sOutputDir;
	KString m_sTemplateDir;
	KReplacer m_Variables { "__", "__", false };

	KOptions m_Options { true };

}; // CreateProject

//-----------------------------------------------------------------------------
CreateProject::CreateProject ()
//-----------------------------------------------------------------------------
{
	kInit("D2PROJECT");

	m_Options.Throw();

	m_Options.RegisterUnknownCommand([&](KOptions::ArgList& Commands)
	{
		if (!m_sProjectName.empty() || Commands.size() > 1)
		{
			throw KOptions::Error("project name defined multiple times");
		}
		m_sProjectName = Commands.pop();
	});

	// keep the -name option, it was the previous way to set the name
	m_Options.Option("name", "project name")
	([&](KStringViewZ sName)
	{
		if (!m_sProjectName.empty())
		{
			throw KOptions::Error("project name defined multiple times");
		}
		m_sProjectName = sName;
	});

	m_Options.Option("help")([&]()
	{
		for (const auto& it : g_Synopsis)
		{
			KOut.WriteLine(it);
		}
		ShowAllTemplates();
		m_bIsDone = true;
	});

	m_Options.Option("p,path", "project path")
	([&](KStringViewZ sPath)
	{
		m_sProjectPath = sPath;
	});

	m_Options.Option("v,version", "version string")
	([&](KStringViewZ sVersion)
	{
		m_sProjectVersion = sVersion;
	});

	m_Options.Option("t,type", "project type")
	([&](KStringViewZ sType)
	{
		m_sProjectType = sType;
	});

	m_Options.Option("s,sso", "SSO server URL and scope").MinArgs(2)
	([&](KOptions::ArgList& SSO)
	{
		m_SSOProvider = SSO.pop();
		m_SSOScope    = SSO.pop();
	});

} // ctor

//-----------------------------------------------------------------------------
void CreateProject::ShowAllTemplates()
//-----------------------------------------------------------------------------
{
	KDirectory Templates(kFormat("{}{}templates", DEKAF2_SHARED_DIRECTORY, kDirSep), KFileType::DIRECTORY);
	Templates.Sort();

	KOut.WriteLine();
	KOut.WriteLine("available project types for the -type parameter:");
	KOut.WriteLine();

	for (const auto& Template : Templates)
	{
		// check if the template directory is empty - then ignore it
		KDirectory Target(Template.Path(), KFileType::FILE);

		if (!Target.empty())
		{
			KOut.FormatLine(" {}", Template.Filename());
			auto ManifestFile = Target.Find("manifest.xml");
			if (ManifestFile != Target.end())
			{
				KXML Manifest(KInFile(ManifestFile->Path()), true);
				auto Description = Manifest.Child("manifest").Child("description");
				if (Description)
				{
					auto sDescription = Description.GetValue();
					sDescription.TrimLeft("\t ");
					sDescription.TrimLeft("\r\n");
					sDescription.TrimRight("\r\n\t ");
					KOut.WriteLine(sDescription);
					KOut.WriteLine();
				}
			}
		}
	}

	KOut.WriteLine();

} // ShowAllTemplates

//-----------------------------------------------------------------------------
void CreateProject::SetupVariables()
//-----------------------------------------------------------------------------
{
	m_Variables.insert("Dekaf2Version"        , DEKAF_VERSION);
	m_Variables.insert("ProjectVersion"       , m_sProjectVersion);
	m_Variables.insert("ProjectType"          , m_sProjectType);
	m_Variables.insert("ProjectName"          , m_sProjectName);
	m_Variables.insert("LowerProjectName"     , m_sProjectName.ToLower());
	m_Variables.insert("UpperProjectName"     , m_sProjectName.ToUpper());
	m_Variables.insert("ProjectPath"          , m_sProjectPath);
	m_Variables.insert("SSOProvider"          , m_SSOProvider.Serialize());
	m_Variables.insert("SSOScope"             , m_SSOScope);

	// create lists of source and header files

	KString sSourceFiles;
	KString sHeaderFiles;

	KDirectory Directory;

	if (!Directory.Open(m_sTemplateDir) || Directory.empty())
	{
		ShowAllTemplates();
		throw KException(kFormat("cannot open template type: {}", m_sProjectType));
	}

	Directory.Sort();

	for (const auto& File : Directory)
	{
		if (File.Filename().ends_with(".cpp"))
		{
			sSourceFiles += kFormat("    {}\n", m_Variables.Replace(File.Filename()));
		}
		else if (File.Filename().ends_with(".h"))
		{
			sHeaderFiles += kFormat("    {}\n", m_Variables.Replace(File.Filename()));
		}
	}

	m_Variables.insert("SourceFiles", sSourceFiles);
	m_Variables.insert("HeaderFiles", sHeaderFiles);

} // SetupVariables

//-----------------------------------------------------------------------------
/// compute remaining values from set values
void CreateProject::FinishSetup()
//-----------------------------------------------------------------------------
{
	m_sTemplateDir = DEKAF2_SHARED_DIRECTORY;
	m_sTemplateDir += kDirSep;
	m_sTemplateDir += "templates";
	m_sTemplateDir += kDirSep;
	m_sTemplateDir += m_sProjectType;

	if (m_sProjectPath.empty())
	{
		m_sProjectPath = kGetCWD();
	}

	m_sOutputDir = m_sProjectPath;
	m_sOutputDir += kDirSep;
	m_sOutputDir += m_sProjectName;

	SetupVariables();

} // FinishSetup

//-----------------------------------------------------------------------------
void CreateProject::PrintReplacedFile(KStringViewZ sOutFile, KStringViewZ sInFile)
//-----------------------------------------------------------------------------
{
	if (!kFileExists(sInFile))
	{
		throw KException(kFormat("input file not found: {}", sInFile));
	}

	KOutFile OutFile(sOutFile, std::ios_base::trunc);

	if (!OutFile.is_open())
	{
		throw KException(kFormat("cannot create output file: {}", sOutFile));
	}

	KOut.FormatLine(":: creating file              : {}", kBasename(sOutFile)).Flush();

	OutFile.Write(m_Variables.Replace(kReadAll(sInFile)));

} // PrintReplacedFile

//-----------------------------------------------------------------------------
void CreateProject::CopyDirRecursive(KStringViewZ sOutputDir, KStringViewZ sFromDir, uint16_t iRecursion)
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: entering directory         : {}", sOutputDir);

	if (!kMakeDir(sOutputDir))
	{
		throw KException(kFormat("cannot create directory: {}", sOutputDir));
	}

	KDirectory Directory(sFromDir);

	if (iRecursion == 1)
	{
		// do not copy the manifest from the root directory, if any
		Directory.Match("manifest.xml", true);
	}

	Directory.Sort();

	for (const auto& File : Directory)
	{
		if (File.Type() == KFileType::DIRECTORY)
		{
			KString sNewOut  = kFormat("{}{}{}", sOutputDir, kDirSep, File.Filename());
			KString sNewFrom = kFormat("{}{}{}", sFromDir,   kDirSep, File.Filename());
			CopyDirRecursive(sNewOut, sNewFrom, ++iRecursion);
		}
		else
		{
			KString sOutFile = kFormat("{}{}{}", sOutputDir, kDirSep, m_Variables.Replace(File.Filename()));
			PrintReplacedFile(sOutFile, File.Path());
		}
	}

} // CopyDirRecursive

//-----------------------------------------------------------------------------
void CreateProject::InstallTemplateDir()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: reading configuration from : {}", m_sTemplateDir);
	KOut.FormatLine(":: creating project at        : {}", m_sOutputDir);

	CopyDirRecursive(m_sOutputDir, m_sTemplateDir);

} // InstallTemplateDir

//-----------------------------------------------------------------------------
void CreateProject::CreateBuildSystem(KStringView sBuildType)
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: creating build system      : build{}{}", kDirSep, sBuildType).Flush();

	KString sBuildDir = m_sOutputDir;
	sBuildDir += kDirSep;
	sBuildDir += "build";
	sBuildDir += kDirSep;
	sBuildDir += sBuildType;

	if (!kCreateDir(sBuildDir))
	{
		throw KException { kFormat("cannot create directory: {}", sBuildDir) };
	}

	// call cmake

	KString sShellOutput;

	if (sBuildType == "Xcode")
	{
		kSystem(kFormat("cmake -G Xcode -S {} -B {}", m_sOutputDir, sBuildDir), sShellOutput);
	}
	else
	{
		kSystem(kFormat("cmake -DCMAKE_BUILD_TYPE={} -S {} -B {}", sBuildType, m_sOutputDir, sBuildDir), sShellOutput);
	}

	if (sShellOutput.ToLowerASCII().contains("error"))
	{
		KErr.WriteLine();
		KErr.WriteLine(sShellOutput);
	}

} // CreateBuildSystem

//-----------------------------------------------------------------------------
int CreateProject::Main(int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	auto iErrors = m_Options.Parse(argc, argv, KOut);

	if (iErrors || m_bIsDone)
	{
		return iErrors;
	}

	if (m_sProjectName.empty())
	{
		throw KException("project name is empty");
	}

	FinishSetup();

	if (kDirExists(m_sOutputDir))
	{
		throw KException(kFormat("output directory already exists: {}", m_sOutputDir));
	}

	InstallTemplateDir();

	CreateBuildSystem("Release");
	CreateBuildSystem("Debug");
#ifdef DEKAF2_IS_OSX
	CreateBuildSystem("Xcode");
#endif
	KOut.WriteLine(":: done");

	return (iErrors);

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	try
	{
		return CreateProject().Main(argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", argv[0], ex.what());
	}

	return 1;

} // main

