
// an example program for the __ProjectName__ library

#include <__LowerProjectName__/__LowerProjectName__.h>
#include <dekaf2/core/init/dekaf2.h>
#include <dekaf2/core/errors/kexception.h>
#include <dekaf2/io/streams/kstream.h>
#include <dekaf2/util/cli/koptions.h>

using namespace dekaf2;

//-----------------------------------------------------------------------------
int main (int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		KInit();

		KOptions Options(false, argc, argv, KLog::STDOUT, /*bThrow*/true);

		Options.SetBriefDescription("example program for the __ProjectName__ library");

		KStringViewZ sName = Options("name <name> : the name to greet, default world", "world");

		// check that all options were consumed - this also prints the help if it was requested
		if (!Options.Check())
		{
			return Options.Terminate() ? 0 : 1;
		}

		__ProjectName__ Library;

		kPrintLine(":: {} v{}", __ProjectName__::s_sProjectName, __ProjectName__::Version());
		kWriteLine(Library.Greet(sName));

		return 0;
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", __ProjectName__::s_sProjectName, ex.what());
	}

	return 1;

} // main
