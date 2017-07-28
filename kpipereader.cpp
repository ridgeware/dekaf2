#include "kpipereader.h"
#include "klog.h"

//#include <stdio.h>
//#include <unistd.h>
namespace dekaf2
{

//-----------------------------------------------------------------------------
bool KPipeReader::Open(KStringView sCommand)
//-----------------------------------------------------------------------------
{
	KString sCmd(sCommand); // char* is not compatible with KStringView
	KLog().debug(3, "KPIPE::Open(): {0}", sCmd.c_str());

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	m_pipe = popen(sCmd.c_str(), "r");

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
		bool bSuccess = KFILEReader::Open(m_pipe); // Call KFILEReader::Open(FILE* fileptr);
		if (bSuccess)
		{
			KLog().debug(2, "KPIPE::Open(): POPEN: ok...");
		}
		else
		{
			KLog().debug(2, "KPIPE::Open(): POPEN: ok, but reader failed to initialize.");
			return false;
		}
	}
	return (m_pipe != NULL);
} // Open

//-----------------------------------------------------------------------------
void  KPipeReader::Close()
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KPIPE::Close");

	if (m_pipe)
	{
		m_iExitCode = pclose (m_pipe);
		m_pipe = nullptr;
	}
	// TODO Discuss with Joachim, method should indicate "error" on close
	// Minimally return a bool, but int allows returning exit code and distinguishing from this error.
//	else
//	{
//		return -1; //attemtping to close a pipe that is not open
//	}

	KLog().debug(3, "KPIPE::Close::Done:: Exit Code = %u", m_iExitCode);
	//KFILEReader::Close(); // make sure reader is also closed
	//return (m_iExitCode);
} // Close


//-----------------------------------------------------------------------------
KPipeReader::operator FILE* ()
//-----------------------------------------------------------------------------
{
	return (m_pipe);
}


} // END NAMESPACE dekaf2
