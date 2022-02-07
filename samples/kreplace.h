
#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kwriter.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class kreplace
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	kreplace ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ s_sProjectName    { "kreplace" };
	static constexpr KStringViewZ s_sProjectVersion { "0.0.1" };

//----------
protected:
//----------

	void ShowVersion ();
	void Search(const KString sFilename);
	struct Config
	{
		bool        bRegularExpression { false };
		bool        bTerminate         { false };
		bool        bReplace           { false };
		bool        bRecursive         { false };
		bool        bQuiet             { false };
		bool        bBackup            { true  };
		bool        bDryRun            { false };
		KString     sSearch;
		KString     sReplace;
		std::vector<KString> Files;
	};

	struct Stats
	{
		std::size_t iFound { 0 };
		std::size_t iFiles { 0 };
		std::size_t iBytes { 0 };

		void Print(KOutStream& os);
	};

	Stats m_Stats;

	Config m_Config;

//----------
private:
//----------

	KOptions m_CLI { true };

}; // kreplace
