#include "kpipewriter.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool KPipeWriter::Open(KStringView sCommand)
//-----------------------------------------------------------------------------
{
	KString sCmd(sCommand); // char* is not compatible with KStringView
	KLog().debug(3, "KPIPE::Open(): {0}", sCmd.c_str());

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	m_pipe = popen(sCmd.c_str(), "w");

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
		KFPWriter::open(m_pipe);
	}
	return (m_pipe != NULL);
} // Open

//-----------------------------------------------------------------------------
int  KPipeWriter::Close()
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KPIPE::Close");

	if (m_pipe)
	{
		m_iExitCode = pclose (m_pipe);
		m_pipe = nullptr;
	}
	else
	{
		return -1; //attemtping to close a pipe that is not open
	}

	KLog().debug(3, "KPIPE::Close::Done:: Exit Code = %u", m_iExitCode);
	return (m_iExitCode);
} // Close

//-----------------------------------------------------------------------------
KPipeWriter::operator FILE* ()
//-----------------------------------------------------------------------------
{
	return (m_pipe);
}

} // END NAMESPACE dekaf2
