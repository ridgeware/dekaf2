
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/rest/framework/krest.h>
#include <atomic>

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

	/// the page to try the chat in a browser
	void ApiPage (KRESTServer& HTTP);
	/// the websocket of the chat
	void ApiChat (KRESTServer& HTTP);
	void ShowVersion ();

	KREST::Options           m_ServerOptions;
	std::atomic<std::size_t> m_iClients { 0 };

}; // __ProjectName__
