// ktail.h
//
// A small desktop tool on KWebApp: browse a directory, follow a log file live
// over a WebSocket, keep a note per file, and reach the native side for what
// only the desktop can do - reveal in the file manager, notifications. The
// class is the whole application, ktail.cpp adds the command line. It touches
// the four capability categories of the KWebApp design: pure UI (the listing),
// local files (the notes), OS integration (notifications), and native-only
// functions (reveal), and shows how one page serves the window and, with
// -listen, browsers on the network behind KWebApp's login.

#pragma once

#include <dekaf2/web/app/kwebapp.h>
#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <memory>

using namespace dekaf2;

class KTail : public KErrorBase
{

//----------
public:
//----------

	struct Config
	{
		/// the directory to browse, made absolute - empty means the current one
		KString sRoot;
		KString sTitle   { "ktail" };
		KString sVersion { "0.1"   };
		/// open a window? false runs the servers alone
		bool    bWindow  { true  };
		/// enable the web inspector in the window
		bool    bDebug   { false };
		/// serve browsers on the network: "[address:]port", empty for none
		KString sListen;
		/// the one account for the network, with the password in a file
		KString sUser;
		KString sPasswordFile;
		/// TLS certificate and key for the network, else a self-signed one is made
		KString sCert;
		KString sKey;
	};

	/// sets up the routes and starts the loopback server - check HasError()
	KTail(Config Config);

	/// runs the window until it is closed
	int Run();

	/// the KWebApp, to bind more functions or to drive the page
	KWebApp& App() { return *m_App; }
	/// the browsed directory, absolute
	const KString& GetRoot() const { return m_Config.sRoot; }
	/// the directory the notes are kept in
	const KString& GetNotesDir() const { return m_sNotesDir; }
	/// the note file for a file below the root
	KString GetNotePath(KStringView sAbsolute) const;

//----------
private:
//----------

	void  Page     (KRESTServer& HTTP);
	void  SaveNote (KRESTServer& HTTP);
	void  Tail     (KRESTServer& HTTP);
	void  TailLoop (KWebSocket& WebSocket, KString sPath);
	KJSON Reveal   (const KJSON& jArg);
	bool  Resolve  (KStringView sRelative, KString& sAbsolute) const;

	Config                   m_Config;
	KRESTRoutes              m_Routes;
	std::unique_ptr<KWebApp> m_App;
	KString                  m_sNotesDir;

}; // KTail
