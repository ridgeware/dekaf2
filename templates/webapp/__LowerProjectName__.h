
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/web/app/kwebapp.h>
#include <atomic>
#include <memory>

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

	/// the page of the application, in the window and in browsers
	void ApiPage (KRESTServer& HTTP);
	/// sends the click count to all pages
	void BroadcastClicks ();
	void ShowVersion ();

	// the routes must outlive the application
	KRESTRoutes              m_Routes;
	std::unique_ptr<KWebApp> m_App;
	std::atomic<std::size_t> m_iClicks { 0 };

}; // __ProjectName__
