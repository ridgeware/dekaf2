
#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kfilesystem.h>
#include <dekaf2/kwriter.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KGrep
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	KGrep ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ s_sProjectName    { "kgrep" };
	static constexpr KStringViewZ s_sProjectVersion { "0.0.1" };

//----------
private:
//----------

	void ShowVersion     ();
	void Grep            (KInStream& InStream, KStringView sFilename);
	void AddFiles        (KOptions::ArgList& Args);
	void PrintError      (KStringView sError);
	KString Highlight    (KStringView sIn, const std::vector<KStringView>& Matches);

	KStringView          m_sProgramName;
	KString              m_sSearch;
	std::vector<KString> m_Files;
	bool                 m_bRegularExpression { false };
	bool                 m_bRecursive         { false };
	bool                 m_bIgnoreCase        { false };
	bool                 m_bInvertMatch       { false };
	bool                 m_bPrintFilename     { false };
	bool                 m_bIsASCII           { true  };
	bool                 m_bIsTerminal        { false };

}; // KGrep
