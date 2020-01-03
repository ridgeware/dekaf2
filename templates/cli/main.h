#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>

using namespace dekaf2;

class {{ProjectName}}
{

public:

	{{ProjectName}} ();

	int Main (int argc, char** argv);

	static constexpr KStringViewZ sProjectName { "{{ProjectName}}" };
	static constexpr KStringViewZ sProjectVersion { "{{ProjectVersion}}" };

protected:

	void ShowVersion ();

private:

	KOptions Options { true };

}; // {{ProjectName}}
