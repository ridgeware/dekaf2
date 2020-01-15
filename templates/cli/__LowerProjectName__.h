
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class __ProjectName__
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	__ProjectName__ ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ s_sProjectName    { "__ProjectName__" };
	static constexpr KStringViewZ s_sProjectVersion { "__ProjectVersion__" };

//----------
protected:
//----------

	void ShowVersion ();

	struct Config
	{
		bool        bTerminate { false };
	};

	Config m_Config;

//----------
private:
//----------

	KOptions m_CLI { true };
	bool     m_Config;

}; // __ProjectName__
