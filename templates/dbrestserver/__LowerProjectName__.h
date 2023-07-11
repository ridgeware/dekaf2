
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include "db.h"
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/krest.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Config options for __ProjectName__
class ServerOptions : public KREST::Options
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	uint16_t                   iSSOLevel { 0 };       // the SSO level - 0 = off, 1 = AUTHORIZATION header present, 2 = checked AUTHORIZATION header

}; // ServerOptions

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class __ProjectName__
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	__ProjectName__ ();

	int Main (int argc, char** argv);

	void ApiOptions (KRESTServer& HTTP);
	void ApiVersion (KRESTServer& HTTP);

	using IniParms = KProps<KString, KString, false, true>;
	static const IniParms& GetIniParms ();

	static constexpr KStringViewZ s_sProjectName    { "__ProjectName__" };
	static constexpr KStringViewZ s_sProjectVersion { "__ProjectVersion__" };
	static constexpr KStringViewZ __UpperProjectName___INI = "/etc/__LowerProjectName__.ini";

//----------
protected:
//----------

	void SetupInputFile (KOptions::ArgList& ArgList);

	ServerOptions m_ServerOptions;

//----------
private:
//----------

	void SetupOptions (KOptions& Options);

	static constexpr KStringView s_SSOProvider  = "__SSOProvider__";
	static constexpr KStringView s_SSOScope     = "__SSOScope__";

	static constexpr KStringViewZ s_sRecordFlag = "/tmp/__LowerProjectName__.record";
	static constexpr KStringViewZ s_sRecordFile = "/tmp/__LowerProjectName__.log";

	static IniParms s_INI;
	static bool     s_bINILoaded;

}; // __ProjectName__
