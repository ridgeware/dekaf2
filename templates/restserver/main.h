
// dekaf2 generated project using the {{ProjectType}} template

#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/krest.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Config options for {{ProjectName}}
class ServerOptions : public KREST::Options
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	uint16_t                   iSSOLevel { 0 };       // the SSO level - 0 = off, 1 = AUTHORIZATION header present, 2 = checked AUTHORIZATION header
	bool                       bUseTLS { false };     // use TLS
	bool                       bTerminate { false };  // stop running?

}; // ServerOptions

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class {{ProjectName}}
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	{{ProjectName}} ();

	int Main (int argc, char** argv);

	void ApiOptions (KRESTServer& HTTP);
	void ApiVersion (KRESTServer& HTTP);

	static constexpr KStringViewZ s_sProjectName    { "{{ProjectName}}" };
	static constexpr KStringViewZ s_sProjectVersion { "{{ProjectVersion}}" };

//----------
protected:
//----------

	void SetupInputFile(KOptions::ArgList& ArgList);

	ServerOptions m_ServerOptions;

//----------
private:
//----------

	static constexpr KStringView s_SSOProvider  = "{{SSOURL}}";
	static constexpr KStringView s_SSOScope     = "{{SSOSCOPE}}";

	static constexpr KStringViewZ s_sRecordFlag = "/tmp/{{LowerProjectName}}.record";
	static constexpr KStringViewZ s_sRecordFile = "/tmp/{{LowerProjectName}}.log";

	KOptions m_CLI { false };

}; // {{ProjectName}}
