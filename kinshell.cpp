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
	kDebug(3, "{}", sCommand);

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
		kDebug (0, "POPEN CMD FAILED: {} ERROR: {}", sCommand, strerror(errno));
		m_iExitCode = errno;
		return false;
	}
	else
	{
		kDebug(3, "POPEN: ok...");
		KFPReader::open(m_pipe);
		return KFPReader::good();
	}

} // Open

} // END NAMESPACE dekaf2
