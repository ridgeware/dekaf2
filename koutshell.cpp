#include "koutshell.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KOutShell::KOutShell(const KString& sCommand)
//-----------------------------------------------------------------------------
{
	Open(sCommand);

} // Immediate Open Constructor

//-----------------------------------------------------------------------------
KOutShell::~KOutShell()
//-----------------------------------------------------------------------------
{
	Close();

} // Default Destructor

//-----------------------------------------------------------------------------
bool KOutShell::Open(const KString& sCommand)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KOutShell::Open(): {}", sCommand);

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sCommand.empty())
	{
		m_pipe = popen(sCommand.c_str(), "w");
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!m_pipe)
	{
		KLog().debug (0, "KOutShell::Open(): POPEN CMD FAILED: {} ERROR: {}", sCommand, strerror(errno));
		m_iExitCode = errno;
		return false;
	}
	else
	{
		KLog().debug(3, "KOutShell::Open(): POPEN: ok...");
		KFPWriter::open(m_pipe);
		return KFPWriter::good();
	}

} // Open

} // END NAMESPACE dekaf2
