
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kprops.h>

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

	using IniParms = KProps<KString, KString, false, true>;
	static const IniParms& GetIniParms ();

	static constexpr KStringViewZ s_sProjectName    { "__ProjectName__" };
	static constexpr KStringViewZ s_sProjectVersion { "__ProjectVersion__" };
	static constexpr KStringViewZ __UpperProjectName___INI = "/etc/__LowerProjectName__.ini";

//----------
protected:
//----------

	void ShowVersion ();

//----------
private:
//----------

	void SetupOptions (KOptions& Options);

	static IniParms s_INI;
	static bool     s_bINILoaded;

}; // __ProjectName__
