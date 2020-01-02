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

using namespace dekaf2;

constexpr KStringView g_Synopsis[] = {
	" ",
	"dekaf2generator -- generate dekaf2 based project setups",
	" ",
	"usage: dekaf2generator [...]",
	"",
	" where:"
	"",
	"  -name <ProjectName>        :: name of the project",
	"  -path <ProjectPath>        :: relative or absolute path to the project root, name is added",
	"  -version <Version>         :: version string, default = 0.0.1",
	"  -type <cli | rest | http>  :: type of project, default = cli, see also -client/-server",
	"  -client | -server          :: default = client",
	"  -sso  <ServerURL>          :: URL of SSO server if any",
	""
};

enum class ProjectType
{
	CLI,
	REST,
	HTTP,
};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct Config
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	ProjectType pType;
	KString sProjectName;
	KString sProjectPath;
	KString sVersion { "0.0.1" };
	KURL SSOServer;
	bool bIsServer { false };

}; // Config

//-----------------------------------------------------------------------------
void SetupOptions (KOptions& Options, Config& Config)
//-----------------------------------------------------------------------------
{
	Options.RegisterOption("name", "missing project name", [&](KStringViewZ sName)
	{
		Config.sProjectName = sName;
	});

	Options.RegisterOption("path", "missing project path", [&](KStringViewZ sPath)
	{
		Config.sProjectPath = sPath;
	});

	Options.RegisterOption("version", "missing version string", [&](KStringViewZ sVersion)
	{
		Config.sVersion = sVersion;
	});

	Options.RegisterOption("type", "missing project type", [&](KStringViewZ sType)
	{
		switch (sType.ToLowerASCII().Hash())
		{
			case "cli"_hash:
				Config.pType = ProjectType::CLI;
				break;

			case "rest"_hash:
				Config.pType = ProjectType::REST;
				break;

			case "http"_hash:
				Config.pType = ProjectType::HTTP;
				break;

			default:
				throw KOptions::WrongParameterError();
		}
	});

	Options.RegisterOption("client", [&]()
	{
		Config.bIsServer = false;
	});

	Options.RegisterOption("server", [&]()
	{
		Config.bIsServer = true;
	});

	Options.RegisterOption("sso", "missing SSO server URL", [&](KStringViewZ sURL)
	{
		Config.SSOServer = sURL;
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
		throw KException(kFormat("cannot open output file: {}", sOutFile));
	}

	OutFile.Write(Variables.Replace(kReadAll(sInFile)));

} // PrintReplacedFile

//-----------------------------------------------------------------------------
void InstallTemplateDir(const KString& sTemplateDir, const KVariables& Variables, KStringViewZ sOutDir)
//-----------------------------------------------------------------------------
{
	if (!kDirExists(sTemplateDir))
	{
		throw KException(kFormat("template directory does not exist: {}", sTemplateDir));
	}

	if (!kMakeDir(sOutDir))
	{
		throw KException(kFormat("cannot create output directory: {}", sOutDir));
	}

	KDirectory Directory(sTemplateDir);

	for (const auto& File : Directory)
	{
		KString sInFile = sTemplateDir;
		sInFile += kDirSep;
		sInFile += File.Filename();

		KString sOutFile = sOutDir;
		sOutFile += kDirSep;
		sOutFile += File.Filename();

		PrintReplacedFile(sOutFile, Variables, sInFile);
	}

} // InstallTemplateDir

//-----------------------------------------------------------------------------
KString AddServerOrClient(bool bIsServer, KStringView sName)
//-----------------------------------------------------------------------------
{
	KString sRet = sName;
	sRet += (bIsServer) ? "server" : "client";
	return sRet;

} // AddServerOrClient

//-----------------------------------------------------------------------------
KString GetTemplateDir(const Config& Config)
//-----------------------------------------------------------------------------
{
	KString sDir;

	switch (Config.pType)
	{
		case ProjectType::CLI:
			sDir = "cli";
			break;

		case ProjectType::HTTP:
			sDir = AddServerOrClient(Config.bIsServer, "http");
			break;

		case ProjectType::REST:
			sDir = AddServerOrClient(Config.bIsServer, "rest");
			break;
	}

	return sDir;

} // GetTemplateDir

//-----------------------------------------------------------------------------
KVariables SetupVariables(const Config& Config)
//-----------------------------------------------------------------------------
{
	KVariables Variables(true);

	Variables.insert("Dekaf2Version", "v" DEKAF_VERSION);
	Variables.insert("Version"      , Config.sVersion);
	Variables.insert("ProjectName"  , Config.sProjectName);
	Variables.insert("ProjectPath"  , Config.sProjectPath);
	Variables.insert("SSOServer"    , Config.SSOServer.Serialize());

	return Variables;

} // SetupVariables

//-----------------------------------------------------------------------------
int main (int argc, char* argv[])
//-----------------------------------------------------------------------------
{
	try
	{
		kInit("D2PROJECT");

		Config Config;
		KOptions Options(true);
		Options.RegisterHelp(g_Synopsis);
		SetupOptions(Options, Config);
		auto iErrors = Options.Parse(argc, argv, KOut);

		if (!iErrors)
		{
			if (Config.sProjectName.empty())
			{
				throw KException("project name is empty");
			}
			KString sOutDir = Config.sProjectPath;
			sOutDir += kDirSep;
			sOutDir += Config.sProjectName;
			InstallTemplateDir(GetTemplateDir(Config), SetupVariables(Config), sOutDir);
		}

		return (iErrors);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}", ex.what());
	}

	return 1;

} // main

