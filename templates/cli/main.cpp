
#include "__LowerProjectName__.h"
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
	"   -help                  :: this text",
	"   -version,rev,revision  :: show version information",
	"   -d[d[d]]               :: different levels of debug messages",
	""
};

//-----------------------------------------------------------------------------
__ProjectName__::__ProjectName__ ()
//-----------------------------------------------------------------------------
{
	m_CLI.RegisterHelp(g_Help);

	m_CLI.RegisterOption("version,rev,revision", [&]()
	{
		ShowVersion();
	});

} // ctor

//-----------------------------------------------------------------------------
void __ProjectName__::ShowVersion()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int __ProjectName__::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	return m_CLI.Parse(argc, argv, KOut);

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
