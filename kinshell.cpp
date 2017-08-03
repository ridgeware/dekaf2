#include "kinshell.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool KInShell::Open(KStringView sCommand)
//-----------------------------------------------------------------------------
{
	KString sCmd(sCommand); // char* is not compatible with KStringView
	KLog().debug(3, "KPIPE::Open(): {0}", sCmd.c_str());

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sCmd.empty())
	{
		m_pipe = popen(sCmd.c_str(), "r");
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!m_pipe)
	{
		KLog().debug (0, "KPIPE::Open(): POPEN CMD FAILED: %s ERROR: %s", sCmd.c_str(), strerror(errno));
		m_iExitCode = errno;
	}
	else
	{
		KLog().debug(2, "KPIPE::Open(): POPEN: ok...");
		KFPReader::open(m_pipe);
	}
	return (m_pipe != NULL);
} // Open

} // END NAMESPACE dekaf2
