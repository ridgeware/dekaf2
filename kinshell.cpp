#include "kinshell.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KInShell::KInShell(const KString& sCommand)
//-----------------------------------------------------------------------------
{
	Open(sCommand);

} // Immediate Open Constructor

//-----------------------------------------------------------------------------
KInShell::~KInShell()
//-----------------------------------------------------------------------------
{
	Close();

} // Default Destructor

//-----------------------------------------------------------------------------
bool KInShell::Open(const KString& sCommand)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KPIPE::Open(): {0}", sCommand.c_str());

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sCommand.empty())
	{
		m_pipe = popen(sCommand.c_str(), "r");
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!m_pipe)
	{
		KLog().debug (0, "KPIPE::Open(): POPEN CMD FAILED: {} ERROR: {}", sCommand, strerror(errno));
		m_iExitCode = errno;
		return false;
	}
	else
	{
		KLog().debug(3, "KPIPE::Open(): POPEN: ok...");
		KFPReader::open(m_pipe);
		SetReaderTrim("");
		return KFPReader::good();
	}

} // Open

} // END NAMESPACE dekaf2
