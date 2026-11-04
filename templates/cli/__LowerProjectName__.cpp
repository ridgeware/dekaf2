
#include "__LowerProjectName__.h"
//@if db
#include "db.h"
//@endif
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/core/errors/kexception.h>

using namespace dekaf2;

//@if db
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

//@endif
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
void __ProjectName__::ShowVersion()
//-----------------------------------------------------------------------------
{
	kPrintLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int __ProjectName__::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options
			.SetBriefDescription       ("dekaf2 __ProjectType__ template")
			// .SetHelpSeparator          ("::")             // the column separator between option and help text
			// .SetLinefeedBetweenOptions (false)            // add linefeed between separate options or commands?
			// .SetWrappedHelpIndent      (1)                // the indent for continuation help text
			.SetSpacingPerSection      (true)                // whether commands and options get the same or separate column layout
			//.SetAdditionalHelp         (g_sAdditionalHelp) // extra help text at end of generated help
		;

//@if db
		KStringViewZ sDBCFile     = Options("dbc <file>  : set database connection", "").Type(KOptions::File);
		bool         bSchema      = Options("schema      : check (possibly upgrade) the database schema", false);
		bool         bSchemaForce = Options("schemaforce : force recreation of the schema (be careful)", false);
		bool         bShowVersion = Options("version     : show version information", false);
//@else
		bool bShowVersion = Options("version : show version information", false);
//@endif

		// ---- insert project specific options here ----

		// check that all options were consumed - this also prints the help if it was requested
		if (!Options.Check())
		{
			return 1;
		}

		if (bShowVersion)
		{
			ShowVersion();
			return 0;
		}
//@if db

		if (!sDBCFile.empty())
		{
			// set the static DBC file
			DB::SetDBCFilename (sDBCFile);
		}

		if (bSchemaForce)
		{
			kDebug (1, "schema FORCE logic");
			DB::Get ()->EnsureSchema (/*bForce=*/true);
			return 0;
		}

		if (bSchema)
		{
			auto pdb = DB::Get ();

			uint16_t iAmAt = pdb->GetSchema ("__UpperProjectName___SCHEMA");
			uint16_t iWant = DB::CURRENT_SCHEMA;
			if (iAmAt != iWant)
			{
				pdb->EnsureSchema (/*bForce=*/false);
			}
			return 0;
		}
//@endif
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
//@if db

// static variables
bool __ProjectName__::s_bINILoaded { false };
__ProjectName__::IniParms __ProjectName__::s_INI;
//@endif

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
#endif
