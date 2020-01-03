
// dekaf2 generated project using the {{ProjectType}} template

#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class {{ProjectName}}
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	{{ProjectName}} ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ s_sProjectName    { "{{ProjectName}}" };
	static constexpr KStringViewZ s_sProjectVersion { "{{ProjectVersion}}" };

//----------
protected:
//----------

	void ShowVersion ();

//----------
private:
//----------

	KOptions m_CLI { true };

}; // {{ProjectName}}
