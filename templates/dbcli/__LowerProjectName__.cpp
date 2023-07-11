
#include "__LowerProjectName__.h"
#include "db.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
const __ProjectName__::IniParms& __ProjectName__::GetIniParms ()
//-----------------------------------------------------------------------------
{
	if (!s_bINILoaded)
	{
		static std::mutex Mutex;
		std::lock_guard Lock(Mutex);

		s_INI.Load (__UpperProjectName___INI);

		s_bINILoaded = true;
	}

	return s_INI;

} // GetIniParms

//-----------------------------------------------------------------------------
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	KInit()
		.SetName(s_sProjectName)
		.SetMultiThreading()
		.SetOnlyShowCallerOnJsonError();
}

//-----------------------------------------------------------------------------
void __ProjectName__::SetupOptions (KOptions& Options)
//-----------------------------------------------------------------------------
{
	Options
		.Throw()
		.SetBriefDescription       ("__LowerProjectName__ -- dekaf2 __ProjectType__ template")
		// .SetHelpSeparator          ("::")              // the column separator between option and help text
		// .SetLinefeedBetweenOptions (false)             // add linefeed between separate options or commands?
		// .SetWrappedHelpIndent      (1)                 // the indent for continuation help text
		.SetSpacingPerSection      (true)                 // whether commands and options get the same or separate column layout
		// .SetAdditionalHelp         (g_sAdditionalHelp) // extra help text at end of generated help
	;

	Options
		.Option("dbc", "dbc file name")
		.Help("set database connection")
	([&](KStringViewZ sFileName)
	{
		if (!kFileExists (sFileName))
		{
			throw KOptions::WrongParameterError(kFormat("dbc file does not exist: {}", sFileName));
		}
		// set the static DBC file
		DB::SetDBCFilename (sFileName);
	});

	Options
		.Option("schema")
		.Help("check (possibly upgrade) the database schema")
		.Stop()
	([&]()
	{
		auto pdb = DB::Get ();

		uint16_t iAmAt = pdb->GetSchema ("__UpperProjectName___SCHEMA");
		uint16_t iWant = DB::CURRENT_SCHEMA;
		if (iAmAt != iWant)
		{
			pdb->EnsureSchema (/*bForce=*/false);
		}
	});

	Options
		.Option("schemaforce")
		.Help("force recreation of the schema (be careful)")
		.Stop()
	([&]()
	{
		kDebug (1, "schema FORCE logic");
		DB::Get ()->EnsureSchema (/*bForce=*/true);
	});

	Options
		.Option("version")
		.Help("show version information")
		.Stop()
	([&]()
	{
		ShowVersion();
	});

} // ctor

//-----------------------------------------------------------------------------
void __ProjectName__::ShowVersion()
//-----------------------------------------------------------------------------
{
	kPrintLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		KOptions Options(true);
		SetupOptions(Options);

		auto iRetVal = Options.Parse(argc, argv, KOut);

		if (Options.Terminate() || iRetVal)
		{
			// either error or completed
			return iRetVal;
		}
	}

	// ---- insert project code here ----

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return __ProjectName__().Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
	}
	return 1;

} // main


// static variables
bool __ProjectName__::s_bINILoaded { false };
__ProjectName__::IniParms __ProjectName__::s_INI;

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
#endif
