
#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kwriter.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KReplace
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KReplace ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ s_sProjectName    { "kreplace" };
	static constexpr KStringViewZ s_sProjectVersion { "0.0.1"    };

//----------
private:
//----------

	void ShowVersion ();
	void Search (const KString sFilename);
	void Grep   (const KString sFilename);
	void Revert ();

	KString     m_sSearch;
	KString     m_sReplace;
	std::vector<KString> m_Files;
	bool        m_bRegularExpression { false };
	bool        m_bReplace           { false };
	bool        m_bRecursive         { false };
	bool        m_bQuiet             { false };
	bool        m_bBackup            { true  };
	bool        m_bDryRun            { false };
	bool        m_bRevert            { false };
	bool        m_bIgnoreCase        { false };
	bool        m_bInvertMatch       { false };
	bool        m_bIsASCII           { true  };

	struct Stats
	{
		std::size_t iFound { 0 };
		std::size_t iFiles { 0 };
		std::size_t iBytes { 0 };

		void Print(KOutStream& os);
	};

	Stats    m_Stats;
	KOptions m_CLI { true };

}; // KReplace
