
#include "{{LowerProjectName}}.h"
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>

using namespace dekaf2;

constexpr KStringView g_Help[] = {
	"",
	"{{LowerProjectName}} -- dekaf2 {{ProjectType}} template",
	"",
	"usage: {{LowerProjectName}} [<options>]",
	"",
	"where <options> are:",
	"   -help                  :: this text",
	"   -version,rev,revision  :: show version information",
	"   -d[d[d]]               :: different levels of debug messages",
	""
};

//-----------------------------------------------------------------------------
{{ProjectName}}::{{ProjectName}} ()
//-----------------------------------------------------------------------------
{
	m_CLI.RegisterHelp(g_Help);

	m_CLI.RegisterOption("version,rev,revision", [&]()
	{
		ShowVersion();
	});

} // ctor

//-----------------------------------------------------------------------------
void {{ProjectName}}::ShowVersion()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", s_sProjectName, s_sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int {{ProjectName}}::Main(int argc, char** argv)
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
		return {{ProjectName}}().Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", {{ProjectName}}::s_sProjectName, ex.what());
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", {{ProjectName}}::s_sProjectName, ex.what());
	}
	return 1;

} // main
