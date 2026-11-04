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

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kjoin.h>
#include <dekaf2/io/readwrite/kreader.h>
#include <dekaf2/io/readwrite/kwriter.h>
#include <dekaf2/io/streams/koutstringstream.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/data/template/kreplacer.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/data/xml/kxml.h>

using namespace DEKAF2_NAMESPACE_NAME;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class CreateProject
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	CreateProject();
	int Main (int argc, char* argv[]);

	/// returns the list of the project types with their features
	KString GetAllTemplates() const;

//----------
private:
//----------

	/// a part that can be added to a project type
	struct Feature
	{
		KString sName;
		KString sDescription;
		bool    bEnabled { false };
	};

	/// the content of the manifest.xml of a project type
	struct Manifest
	{
		KString              sDescription;
		/// the features that can be added to the project type
		std::vector<Feature> Features;
	};

	/// read the manifest of the project type in sTemplateDir
	static Manifest ReadManifest(KStringViewZ sTemplateDir);
	/// add the names of a comma separated list to Names
	static void AddFeatureNames(std::vector<KString>& Names, KStringView sList);
	/// enable the features of -with
	void SelectFeatures();
	/// returns the feature with sName, or nullptr
	const Feature* FindFeature(KStringView sName) const;
	/// returns the directories with the files of the enabled features
	std::vector<KString> GetFeatureDirs() const;
	/// remove the lines of the features that are not enabled, and the marker lines
	KString SelectLines(KStringView sContent, KStringView sFilename) const;
	/// set fixed variables
	void SetupVariables();
	/// compute remaining values from set values
	void FinishSetup();
	void PrintReplacedFile (KStringViewZ sOutFile, KStringViewZ sInFile);
	void CreateBuildSystem (KStringView sBuildType);
	void InstallTemplateDir();
	void CopyDirRecursive(KStringViewZ sOutputDir, KStringViewZ sFromDir, uint16_t iRecursion = 1, bool bAddFeature = false);

	KString m_sProjectType { "cli" };
	std::vector<KString> m_With;
	std::vector<Feature> m_Features;
	KString m_sProjectName;
	KString m_sProjectPath;
	KString m_sProjectVersion { "0.0.1" };
	KURL m_SSOProvider;
	KString m_SSOScope;

	KString m_sCanonicalTemplateDir;
	KString m_sOutputDir;
	KString m_sTemplateDir;
	KReplacer m_Variables { "__", "__", false };

}; // CreateProject

//-----------------------------------------------------------------------------
CreateProject::CreateProject ()
//-----------------------------------------------------------------------------
{
	kInit("D2PROJECT");

	m_sCanonicalTemplateDir = kReadLink(kFormat("{}/../{}/templates", Dekaf::getInstance().GetProgPath(), DEKAF2_SHARED_DIRECTORY), true);

} // ctor

//-----------------------------------------------------------------------------
KString CreateProject::GetAllTemplates() const
//-----------------------------------------------------------------------------
{
	KString sTemplates;
	KOutStringStream Out(sTemplates);

	KDirectory Templates(m_sCanonicalTemplateDir, KFileType::DIRECTORY);
	Templates.Sort();

	Out.WriteLine();
	Out.WriteLine("available project types for the -type parameter:");
	Out.WriteLine();

	for (const auto& Template : Templates)
	{
		// directories starting with + hold the files of features, not project types
		if (Template.Filename().starts_with('+'))
		{
			continue;
		}

		// check if the template directory is empty - then ignore it
		KDirectory Target(Template.Path(), KFileType::FILE);

		if (!Target.empty())
		{
			Out.FormatLine(" {}", Template.Filename());

			auto Manifest = ReadManifest(Template.Path());

			if (!Manifest.sDescription.empty())
			{
				Out.WriteLine(Manifest.sDescription);
			}

			std::size_t iMaxName { 0 };

			for (const auto& Feature : Manifest.Features)
			{
				iMaxName = std::max(iMaxName, Feature.sName.size());
			}

			for (const auto& Feature : Manifest.Features)
			{
				Out.FormatLine("\t-with {:<{}} : {}", Feature.sName, iMaxName, Feature.sDescription);
			}

			Out.WriteLine();
		}
	}

	Out.Flush();

	return sTemplates;

} // GetAllTemplates

//-----------------------------------------------------------------------------
CreateProject::Manifest CreateProject::ReadManifest(KStringViewZ sTemplateDir)
//-----------------------------------------------------------------------------
{
	Manifest Result;

	auto sManifestFile = kFormat("{}{}manifest.xml", sTemplateDir, kDirSep);

	if (!kFileExists(sManifestFile))
	{
		return Result;
	}

	KXML XML(KInFile(sManifestFile), true);
	auto Root = XML.Child("manifest");

	// the description keeps its indent
	auto sDescription = Root.Child("description").GetValue();
	sDescription.TrimLeft("\t ");
	sDescription.TrimLeft("\r\n");
	sDescription.TrimRight("\r\n\t ");
	Result.sDescription = sDescription;

	for (auto Node = Root.Child("feature"); Node; Node = Node.Next("feature"))
	{
		Feature NewFeature;

		NewFeature.sName = Node.Attribute("name").GetValue();

		auto sFeatureDescription = Node.GetValue();
		sFeatureDescription.Trim();
		NewFeature.sDescription = sFeatureDescription;

		if (NewFeature.sName.empty())
		{
			throw KException(kFormat("feature without a name in {}", sManifestFile));
		}

		if (NewFeature.sName == "all")
		{
			throw KException(kFormat("feature name 'all' is reserved for all features, in {}", sManifestFile));
		}

		Result.Features.push_back(std::move(NewFeature));
	}

	return Result;

} // ReadManifest

//-----------------------------------------------------------------------------
void CreateProject::AddFeatureNames(std::vector<KString>& Names, KStringView sList)
//-----------------------------------------------------------------------------
{
	for (auto sName : sList.Split(","))
	{
		if (!sName.empty())
		{
			Names.push_back(sName);
		}
	}

} // AddFeatureNames

//-----------------------------------------------------------------------------
const CreateProject::Feature* CreateProject::FindFeature(KStringView sName) const
//-----------------------------------------------------------------------------
{
	for (const auto& Feature : m_Features)
	{
		if (Feature.sName == sName)
		{
			return &Feature;
		}
	}

	return nullptr;

} // FindFeature

//-----------------------------------------------------------------------------
void CreateProject::SelectFeatures()
//-----------------------------------------------------------------------------
{
	for (const auto& sName : m_With)
	{
		if (sName == "all")
		{
			for (auto& Feature : m_Features)
			{
				Feature.bEnabled = true;
			}

			continue;
		}

		auto Feature = std::find_if(m_Features.begin(), m_Features.end(), [&sName](const CreateProject::Feature& Known)
		{
			return Known.sName == sName;
		});

		if (Feature == m_Features.end())
		{
			std::vector<KStringView> Names;

			for (const auto& Known : m_Features)
			{
				Names.push_back(Known.sName);
			}

			throw KException(kFormat("project type {} has no feature '{}'{}{}",
			                         m_sProjectType,
			                         sName,
			                         Names.empty() ? " - it has no features" : " - its features are: ",
			                         kJoined(Names, ", ")));
		}

		Feature->bEnabled = true;
	}

} // SelectFeatures

//-----------------------------------------------------------------------------
std::vector<KString> CreateProject::GetFeatureDirs() const
//-----------------------------------------------------------------------------
{
	std::vector<KString> Dirs;

	for (const auto& Feature : m_Features)
	{
		if (Feature.bEnabled)
		{
			// the files of a feature can belong to the project type, or be shared by all types
			for (auto sDir : { kFormat("{}{}+{}", m_sTemplateDir,          kDirSep, Feature.sName),
			                   kFormat("{}{}+{}", m_sCanonicalTemplateDir, kDirSep, Feature.sName) })
			{
				if (kDirExists(sDir))
				{
					Dirs.push_back(std::move(sDir));
				}
			}
		}
	}

	return Dirs;

} // GetFeatureDirs

//-----------------------------------------------------------------------------
KString CreateProject::SelectLines(KStringView sContent, KStringView sFilename) const
//-----------------------------------------------------------------------------
{
	// a block between //@if and //@endif (or #@if and #@endif), with an optional //@else
	struct Block
	{
		bool        bWrite;     // are the lines of the block written?
		bool        bOuter;     // are the lines around the block written?
		bool        bCondition; // the value of the //@if condition
		bool        bElse;      // was the //@else seen?
		std::size_t iIf;        // the line of the //@if
	};

	std::vector<Block> Blocks;
	KString            sResult;
	std::size_t        iLine { 0 };

	auto Error = [&](KStringView sMessage)
	{
		return KException(kFormat("{} in {}:{}", sMessage, sFilename, iLine));
	};

	while (!sContent.empty())
	{
		++iLine;

		auto iEnd  = sContent.find('\n');
		auto sLine = sContent.substr(0, iEnd == KStringView::npos ? KStringView::npos : iEnd + 1);
		sContent.remove_prefix(sLine.size());

		bool bWrite = Blocks.empty() || Blocks.back().bWrite;

		KStringView sMarker = sLine;
		sMarker.TrimLeft(" \t");

		if (!sMarker.remove_prefix("//@") && !sMarker.remove_prefix("#@"))
		{
			if (bWrite)
			{
				sResult += sLine;
			}

			continue;
		}

		sMarker.TrimRight();

		auto iSpace     = sMarker.find_first_of(" \t");
		auto sDirective = sMarker.substr(0, iSpace);
		auto sArgument  = iSpace == KStringView::npos ? KStringView{} : sMarker.substr(iSpace);
		sArgument.Trim();

		if (sDirective == "if")
		{
			bool bNegate = sArgument.remove_prefix("!");
			sArgument.TrimLeft();

			auto Feature = FindFeature(sArgument);

			if (!Feature)
			{
				throw Error(kFormat("unknown feature '{}'", sArgument));
			}

			bool bCondition = Feature->bEnabled != bNegate;

			Blocks.push_back({ bWrite && bCondition, bWrite, bCondition, false, iLine });
		}
		else if (sDirective == "else")
		{
			if (Blocks.empty() || Blocks.back().bElse)
			{
				throw Error("@else without @if");
			}

			auto& Current  = Blocks.back();
			Current.bElse  = true;
			Current.bWrite = Current.bOuter && !Current.bCondition;
		}
		else if (sDirective == "endif")
		{
			if (Blocks.empty())
			{
				throw Error("@endif without @if");
			}

			Blocks.pop_back();
		}
		else
		{
			throw Error(kFormat("unknown template directive '@{}'", sDirective));
		}
	}

	if (!Blocks.empty())
	{
		iLine = Blocks.back().iIf;
		throw Error("missing @endif for the @if");
	}

	return sResult;

} // SelectLines

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
		KOut.Write(GetAllTemplates());
		throw KException(kFormat("cannot open template type: {}", m_sProjectType));
	}

	// the files of the project type and of its enabled features
	std::vector<KString> Filenames;

	for (const auto& File : Directory)
	{
		Filenames.push_back(File.Filename());
	}

	for (const auto& sFeatureDir : GetFeatureDirs())
	{
		KDirectory FeatureDirectory(sFeatureDir, KFileType::FILE);

		for (const auto& File : FeatureDirectory)
		{
			Filenames.push_back(File.Filename());
		}
	}

	std::sort(Filenames.begin(), Filenames.end());

	for (const auto& sFilename : Filenames)
	{
		if (sFilename.ends_with(".cpp"))
		{
			sSourceFiles += kFormat("    {}\n", m_Variables.Replace(sFilename));
		}
		else if (sFilename.ends_with(".h"))
		{
			sHeaderFiles += kFormat("    {}\n", m_Variables.Replace(sFilename));
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
	// directories starting with + hold the files of features, not project types
	if (m_sProjectType.starts_with('+'))
	{
		KOut.Write(GetAllTemplates());
		throw KException(kFormat("cannot open template type: {}", m_sProjectType));
	}

	m_sTemplateDir = kFormat("{}{}{}", m_sCanonicalTemplateDir, kDirSep, m_sProjectType);
	m_Features     = ReadManifest(m_sTemplateDir).Features;

	SelectFeatures();

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

	OutFile.Write(m_Variables.Replace(SelectLines(kReadAll(sInFile), sInFile)));

} // PrintReplacedFile

//-----------------------------------------------------------------------------
void CreateProject::CopyDirRecursive(KStringViewZ sOutputDir, KStringViewZ sFromDir, uint16_t iRecursion, bool bAddFeature)
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
			if (iRecursion == 1 && File.Filename().starts_with('+'))
			{
				// the files of a feature are added by InstallTemplateDir()
				continue;
			}

			KString sNewOut  = kFormat("{}{}{}", sOutputDir, kDirSep, m_Variables.Replace(File.Filename()));
			KString sNewFrom = kFormat("{}{}{}", sFromDir,   kDirSep, File.Filename());
			CopyDirRecursive(sNewOut, sNewFrom, iRecursion + 1, bAddFeature);
		}
		else
		{
			KString sOutFile = kFormat("{}{}{}", sOutputDir, kDirSep, m_Variables.Replace(File.Filename()));

			if (bAddFeature && kFileExists(sOutFile))
			{
				throw KException(kFormat("the feature file {} exists already in the project", File.Path()));
			}

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

	std::vector<KStringView> Enabled;

	for (const auto& Feature : m_Features)
	{
		if (Feature.bEnabled)
		{
			Enabled.push_back(Feature.sName);
		}
	}

	if (!Enabled.empty())
	{
		KOut.FormatLine(":: with features              : {}", kJoined(Enabled, ", "));
	}

	CopyDirRecursive(m_sOutputDir, m_sTemplateDir);

	for (const auto& sFeatureDir : GetFeatureDirs())
	{
		CopyDirRecursive(m_sOutputDir, sFeatureDir, 1, /*bAddFeature*/true);
	}

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
	// ---------------- parse CLI ------------------
	{
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options
			.SetBriefDescription        ("generate dekaf2 based project setups")
			.SetAdditionalArgDescription("<ProjectName>");

		m_sProjectType                  = Options("t,type <ProjectType> : type of project, default cli", "cli");
		m_sProjectPath                  = Options("p,path <ProjectPath> : path to the project root, the name is added, default the current directory", "");
		m_sProjectVersion               = Options("v,version <Version>  : version string, default 0.0.1", "0.0.1");
		std::vector<KStringViewZ> SSO   = Options("s,sso <URL> <Scope>  : URL and scope of the SSO server, default none", std::vector<KStringViewZ>{}).MaxArgs(2);
		std::vector<KStringViewZ> With  = Options("with <features>      : comma separated list of features to add to the project type, or all", std::vector<KStringViewZ>{}).MaxArgs(1);
		KStringViewZ              sPath = Options("templates <path>     : path to the templates, default the system install directory", "");
		// -name was the previous way to set the project name
		KStringViewZ              sName = Options("name <ProjectName>   : project name, instead of the last argument", "");

		auto Names = Options.GetUnknownCommands();

		if (!sPath.empty())
		{
			m_sCanonicalTemplateDir = sPath;
		}

		// the help is printed by Check(), after -templates was read
		Options.SetAdditionalHelp(GetAllTemplates());

		// check that all options were consumed - this also prints the help if it was requested
		if (!Options.Check())
		{
			return Options.Terminate() ? 0 : 1;
		}

		if (!sName.empty())
		{
			Names.push_back(sName);
		}

		if (Names.size() > 1)
		{
			throw KException(kFormat("project name defined multiple times: {}", kJoined(Names, ", ")));
		}

		if (Names.empty())
		{
			throw KException("project name is empty");
		}

		m_sProjectName = Names.front();

		if (!SSO.empty())
		{
			if (SSO.size() != 2)
			{
				throw KException("-sso needs a URL and a scope");
			}

			m_SSOProvider = SSO[0];
			m_SSOScope    = SSO[1];
		}

		for (auto sFeatures : With)
		{
			AddFeatureNames(m_With, sFeatures);
		}
	}

	// checked after parsing, as -templates may name another directory
	if (!kDirExists(m_sCanonicalTemplateDir))
	{
		throw KException(kFormat("template directory not found: {}", m_sCanonicalTemplateDir));
	}

	FinishSetup();

	if (kDirExists(m_sOutputDir))
	{
		throw KException(kFormat("output directory already exists: {}", m_sOutputDir));
	}

	try
	{
		InstallTemplateDir();
	}
	catch (const KException&)
	{
		// the output directory did not exist before, remove the incomplete project
		kRemoveDir(m_sOutputDir);
		throw;
	}

	CreateBuildSystem("Release");
	CreateBuildSystem("Debug");
#ifdef DEKAF2_IS_OSX
	CreateBuildSystem("Xcode");
#endif
	KOut.WriteLine(":: done");

	return 0;

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

