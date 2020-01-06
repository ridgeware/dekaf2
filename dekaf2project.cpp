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

using namespace dekaf2;

constexpr KStringView g_Synopsis[] = {
	" ",
	"dekaf2project -- generate dekaf2 based project setups",
	" ",
	"usage: dekaf2project [<Options>] <ProjectName>",
	"",
	" where options are:"
	"",
	"  -type <ProjectType>  :: type of project, default = cli",
	"  -path <ProjectPath>  :: path to project root, name is added, default = current directory",
	"  -sso  <URL> <Scope>  :: URL and scope of SSO server, default = none",
	"  -version <Version>   :: version string, default = 0.0.1"
};

//-----------------------------------------------------------------------------
void ShowAllTemplates()
//-----------------------------------------------------------------------------
{
	KDirectory Templates(kFormat("{}{}templates", DEKAF2_SHARED_DIRECTORY, kDirSep), KDirectory::EntryType::DIRECTORY);
	Templates.Sort();

	KOut.WriteLine();
	KOut.WriteLine("available project types for the -type parameter:");
	KOut.WriteLine();

	for (const auto& Template : Templates)
	{
		// check if the template directory is empty - then ignore it
		KDirectory Target(Template.Path(), KDirectory::EntryType::REGULAR);

		if (!Target.empty())
		{
			KOut.FormatLine(" {}", Template.Filename());
		}
	}

	KOut.WriteLine();

} // ShowAllTemplates

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Config
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// set fixed variables
	void SetupVariables()
	//-----------------------------------------------------------------------------
	{
		Variables.insert("Dekaf2Version"        , DEKAF_VERSION);
		Variables.insert("ProjectVersion"       , sProjectVersion);
		Variables.insert("ProjectType"          , sProjectType);
		Variables.insert("ProjectName"          , sProjectName);
		Variables.insert("LowerProjectName"     , sProjectName.ToLower());
		Variables.insert("ProjectPath"          , sProjectPath);
		Variables.insert("SSOProvider"          , SSOProvider.Serialize());
		Variables.insert("SSOScope"             , SSOScope);

		// create lists of source and header files

		KString sSourceFiles;
		KString sHeaderFiles;

		for (const auto& File : Directory)
		{
			bool bIsHeader = File.Filename().ends_with(".h");
			bool bIsSource = File.Filename().ends_with(".cpp");

			if (!File.Filename().starts_with("main."))
			{
				if (bIsSource)
				{
					sSourceFiles += kFormat("    {}\n", File.Filename());
				}
				else if (bIsHeader)
				{
					sHeaderFiles += kFormat("    {}\n", File.Filename());
				}
			}
			else
			{
				// main - replace with project name
				if (bIsSource)
				{
					sSourceFiles += kFormat("    {}.cpp\n", sProjectName.ToLower());
				}
				else if (bIsHeader)
				{
					sHeaderFiles += kFormat("    {}.h\n", sProjectName.ToLower());
				}
			}
		}

		Variables.insert("SourceFiles", sSourceFiles);
		Variables.insert("HeaderFiles", sHeaderFiles);

	} // SetupVariables

	//-----------------------------------------------------------------------------
	/// compute remaining values from set values
	void Finish()
	//-----------------------------------------------------------------------------
	{
		sTemplateDir = DEKAF2_SHARED_DIRECTORY;
		sTemplateDir += kDirSep;
		sTemplateDir += "templates";
		sTemplateDir += kDirSep;
		sTemplateDir += sProjectType;

		if (!Directory.Open(sTemplateDir) || Directory.empty())
		{
			ShowAllTemplates();
			throw KException(kFormat("cannot open template type: {}", sProjectType));
		}

		Directory.Sort();

		if (sProjectPath.empty())
		{
			sProjectPath = kGetCWD();
		}

		sOutputDir = sProjectPath;
		sOutputDir += kDirSep;
		sOutputDir += sProjectName;

		SetupVariables();

	} // Finish

	KString sProjectType { "cli" };
	KString sProjectName;
	KString sProjectPath;
	KString sProjectVersion { "0.0.1" };
	KURL SSOProvider;
	KString SSOScope;
	bool bIsDone { false };

	KString sOutputDir;
	KString sTemplateDir;
	KVariables Variables { true };
	KDirectory Directory;

}; // Config

//-----------------------------------------------------------------------------
void SetupOptions (KOptions& Options, Config& Config)
//-----------------------------------------------------------------------------
{
	Options.RegisterUnknownCommand([&](KOptions::ArgList& Commands)
	{
		if (!Config.sProjectName.empty() || Commands.size() > 1)
		{
			throw KOptions::Error("project name defined multiple times");
		}
		Config.sProjectName = Commands.pop();
	});

	// keep the -name option, it was the previous way to set the name
	Options.RegisterOption("name", "missing project name", [&](KStringViewZ sName)
	{
		if (!Config.sProjectName.empty())
		{
			throw KOptions::Error("project name defined multiple times");
		}
		Config.sProjectName = sName;
	});

	Options.RegisterOption("help",[&]()
	{
		for (const auto& it : g_Synopsis)
		{
			KOut.WriteLine(it);
		}
		ShowAllTemplates();
		Config.bIsDone = true;
	});

	Options.RegisterOption("path", "missing project path", [&](KStringViewZ sPath)
	{
		Config.sProjectPath = sPath;
	});

	Options.RegisterOption("version", "missing version string", [&](KStringViewZ sVersion)
	{
		Config.sProjectVersion = sVersion;
	});

	Options.RegisterOption("type", "missing project type", [&](KStringViewZ sType)
	{
		Config.sProjectType = sType;
	});

	Options.RegisterOption("sso", 2, "missing SSO server URL and scope", [&](KOptions::ArgList& SSO)
	{
		Config.SSOProvider = SSO.pop();
		Config.SSOScope    = SSO.pop();
	});

} // SetupOptions

//-----------------------------------------------------------------------------
void PrintReplacedFile(KStringViewZ sOutFile, const KVariables& Variables, KStringViewZ sInFile)
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

	OutFile.Write(Variables.Replace(kReadAll(sInFile)));

} // PrintReplacedFile

//-----------------------------------------------------------------------------
void InstallTemplateDir(const Config& Config)
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: reading configuration from : {}", Config.sTemplateDir);
	KOut.FormatLine(":: creating project at        : {}", Config.sOutputDir);

	if (!kMakeDir(Config.sOutputDir))
	{
		throw KException(kFormat("cannot create output directory: {}", Config.sOutputDir));
	}

	for (const auto& File : Config.Directory)
	{
		KString sOutFile = Config.sOutputDir;
		sOutFile += kDirSep;

		if (File.Filename() == "main.h")
		{
			sOutFile += Config.sProjectName.ToLower();
			sOutFile += ".h";
		}
		else if (File.Filename() == "main.cpp")
		{
			sOutFile += Config.sProjectName.ToLower();
			sOutFile += ".cpp";
		}
		else
		{
			sOutFile += File.Filename();
		}

		PrintReplacedFile(sOutFile, Config.Variables, File.Path());
	}

} // InstallTemplateDir

//-----------------------------------------------------------------------------
void CreateBuildSystem(const Config& Config, KStringView sBuildType)
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: creating build system      : build{}{}", kDirSep, sBuildType).Flush();

	KString sBuildDir = Config.sOutputDir;
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
		kSystem(kFormat("cmake -G Xcode -S {} -B {}", Config.sOutputDir, sBuildDir), sShellOutput);
	}
	else
	{
		kSystem(kFormat("cmake -DCMAKE_BUILD_TYPE={} -S {} -B {}", sBuildType, Config.sOutputDir, sBuildDir), sShellOutput);
	}

	if (sShellOutput.ToLowerASCII().Contains("error"))
	{
		KErr.WriteLine();
		KErr.WriteLine(sShellOutput);
		KErr.WriteLine();
	}

} // CreateBuildSystem

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	try
	{
		kInit("D2PROJECT");

		Config Config;
		KOptions Options(true);
		Options.Throw();
		SetupOptions(Options, Config);
		auto iErrors = Options.Parse(argc, argv, KOut);

		if (!iErrors)
		{
			if (Config.bIsDone)
			{
				return 0;
			}

			if (Config.sProjectName.empty())
			{
				throw KException("project name is empty");
			}

			Config.Finish();

			if (kDirExists(Config.sOutputDir))
			{
				throw KException(kFormat("output directory already exists: {}", Config.sOutputDir));
			}

			InstallTemplateDir(Config);

			CreateBuildSystem(Config, "Release");
			CreateBuildSystem(Config, "Debug");
#ifdef DEKAF2_IS_OSX
			CreateBuildSystem(Config, "Xcode");
#endif
			KOut.WriteLine(":: done");
		}

		return (iErrors);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", argv[0], ex.what());
	}

	return 1;

} // main

