
#include "{{LowerProjectName}}.h"
#include <dekaf2/kstring.h>
#include <dekaf2/kstringview.h>
#include <dekaf2/kstream.h>
#include <dekaf2/kexception.h>

using namespace dekaf2;

constexpr KStringView g_Help[] = {
	"",
	"{{LowerProjectName}} -- dekaf2 CLI template",
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
	Options.RegisterHelp(g_Help);

	Options.RegisterOption("version,rev,revision", [&]()
	{
		ShowVersion();
	});

} // ctor

//-----------------------------------------------------------------------------
void {{ProjectName}}::ShowVersion()
//-----------------------------------------------------------------------------
{
	KOut.FormatLine(":: {} v{}", sProjectName, sProjectVersion);

} // ShowVersion

//-----------------------------------------------------------------------------
int {{ProjectName}}::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	int iRetval = Options.Parse(argc, argv, KOut);

	return iRetval;

} // Main

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		{{ProjectName}} {{ProjectName}};
		return {{ProjectName}}.Main (argc, argv);
	}
	catch (const KException& ex)
	{
		KErr.FormatLine(">> {}: {}", ex.what());
		return 1;
	}

	return 0;

} // main
