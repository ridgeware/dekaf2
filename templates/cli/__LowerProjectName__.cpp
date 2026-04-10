
#include "__LowerProjectName__.h"
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/core/errors/kexception.h>

using namespace dekaf2;

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
		// .SetHelpSeparator          ("::")             // the column separator between option and help text
		// .SetLinefeedBetweenOptions (false)            // add linefeed between separate options or commands?
		// .SetWrappedHelpIndent      (1)                // the indent for continuation help text
		.SetSpacingPerSection      (true)                // whether commands and options get the same or separate column layout
		//.SetAdditionalHelp         (g_sAdditionalHelp) // extra help text at end of generated help
	;

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
int __ProjectName__::Main(int argc, char** argv)
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

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr KStringViewZ __ProjectName__::s_sProjectName;
constexpr KStringViewZ __ProjectName__::s_sProjectVersion;
#endif
