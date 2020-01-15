
// dekaf2 generated project using the __ProjectType__ template

#pragma once

#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/koptions.h>
#include <dekaf2/kurl.h>
#include <dekaf2/khttp_method.h>
#include <dekaf2/kassociative.h>
#include <dekaf2/kmime.h>

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

	void ServerQuery ();
	void ShowVersion ();

	using HeaderMap = KMap<KString, KString>;

	struct Config
	{
		KURL        URL;
		KHTTPMethod Method     { KHTTPMethod::GET };
		KMIME       MimeType   { KMIME::JSON };
		KString     sBody;
		HeaderMap   Headers;
		bool        bTerminate { false };
	};

	Config m_Config;

//----------
private:
//----------

	KOptions m_CLI { true };

}; // __ProjectName__
