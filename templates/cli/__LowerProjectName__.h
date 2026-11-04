
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/util/cli/koptions.h>
//@if db
#include <dekaf2/containers/associative/kprops.h>
//@endif

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

//@if db
	using IniParms = KProps<KString, KString, false, true>;
	static const IniParms& GetIniParms ();

//@endif
	static constexpr KStringViewZ s_sProjectName    { "__ProjectName__" };
	static constexpr KStringViewZ s_sProjectVersion { "__ProjectVersion__" };
//@if db
	static constexpr KStringViewZ __UpperProjectName___INI = "/etc/__LowerProjectName__.ini";
//@endif

//----------
protected:
//----------

	void ShowVersion ();

//@if db
//----------
private:
//----------

	static IniParms s_INI;
	static bool     s_bINILoaded;

//@endif
}; // __ProjectName__
