
#include "__LowerProjectName__.h"
#include "db.h"
#include <dekaf2/dekaf2.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>

using namespace dekaf2;

constexpr KStringView g_Help[] = {
	"",
	"__LowerProjectName__ -- dekaf2 __ProjectType__ template",
	"",
	"usage: __LowerProjectName__ [<options>]",
	"",
	"where <options> are:",
	"   -version               :: show software version and exit",
	"   -help                  :: this help message",
	"   -[d[d[d]]]             :: 3 optional levels of stdout debugging",
	"   -dbc <file>            :: set database connection",
	"   -schema                :: check (possibly upgrade) the database schema",
	"   -schemaforce           :: force recreation of the schema (be careful)",
	""
};

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
	KInit().SetName(s_sProjectName).SetMultiThreading().SetOnlyShowCallerOnJsonError();

	m_CLI.Throw();

	m_CLI.RegisterHelp(g_Help);

	m_CLI.RegisterOption("dbc", "dbc file name", [&](KStringViewZ sFileName)
	{
		if (!kFileExists (sFileName))
		{
			throw KOptions::WrongParameterError(kFormat("dbc file does not exist: {}", sFileName));
		}
		// set the static DBC file
		DB::SetDBCFilename (sFileName);
	});

	m_CLI.RegisterOption("schema", [&]()
	{
		auto pdb = DB::Get ();

		uint16_t iAmAt = pdb->GetSchema ("__UpperProjectName___SCHEMA");
		uint16_t iWant = DB::CURRENT_SCHEMA;
		if (iAmAt != iWant)
		{
			pdb->EnsureSchema (/*bForce=*/false);
		}
		m_Config.bTerminate = true;
	});

	m_CLI.RegisterOption("schemaforce", [&]()
	{
		kDebug (1, "schema FORCE logic");
		DB::Get ()->EnsureSchema (/*bForce=*/true);
		m_Config.bTerminate = true;
	});

	m_CLI.RegisterOption("version,rev,revision", [&]()
	{
		ShowVersion();
		m_Config.bTerminate = true;
	});

} // ctor

//-----------------------------------------------------------------------------
void __ProjectName__::ShowVersion()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int __ProjectName__::Main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// ---------------- parse CLI ------------------
	{
		auto iRetVal = m_CLI.Parse(argc, argv, KOut);

		if (iRetVal	|| m_Config.bTerminate)
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
