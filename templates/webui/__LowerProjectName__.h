
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <mutex>

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

	/// the values that the settings form edits
	struct Settings
	{
		KString sGreeting  { "hello" };
		bool    bShowTime  { true    };
	};

	/// the home page
	void ApiHome         (KRESTServer& HTTP);
	/// the settings form
	void ApiSettings     (KRESTServer& HTTP);
	/// stores the submitted settings form
	void ApiSaveSettings (KRESTServer& HTTP);
	/// a page with the navigation, returns the node for the content
	KHTMLNode BeginPage  (html::Page& Page);
	/// the settings form, bound to Bound
	void AddSettingsForm (KHTMLNode Parent, Settings& Bound);
	/// sends the page as response
	void SendPage        (KRESTServer& HTTP, html::Page& Page);
	void ShowVersion     ();

	KREST::Options m_ServerOptions;
	Settings       m_Settings;
	std::mutex     m_SettingsMutex;

}; // __ProjectName__
