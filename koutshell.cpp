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
//	Close();

} // Default Destructor

//-----------------------------------------------------------------------------
bool KOutShell::Open(const KString& sCommand)
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
		m_pipe = popen(sCommand.c_str(), "w");
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
		KFPWriter::open(m_pipe);
		return KFPWriter::good();
	}

} // Open

} // END NAMESPACE dekaf2
