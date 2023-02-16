
#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/krest.h>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Config options for RestServer
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
class RestServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	RestServer ();

	int Main (int argc, char** argv);

	void ApiOptions (KRESTServer& HTTP);
	void ApiVersion (KRESTServer& HTTP);
	void ApiChat    (KRESTServer& HTTP);

	static constexpr KStringViewZ s_sProjectName    { "RestServer" };
	static constexpr KStringViewZ s_sProjectVersion { "0.0.1" };

//----------
protected:
//----------

	void SetupInputFile(KOptions::ArgList& ArgList);

	ServerOptions m_ServerOptions;

//----------
private:
//----------

	static constexpr KStringView s_SSOProvider  = "";
	static constexpr KStringView s_SSOScope     = "";

	static constexpr KStringViewZ s_sRecordFlag = "/tmp/restserver.record";
	static constexpr KStringViewZ s_sRecordFile = "/tmp/restserver.log";

	KOptions m_CLI { false };

}; // RestServer
