#include "kinpipe.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KInPipe::KInPipe()
//-----------------------------------------------------------------------------
{
	SetReaderTrim("");
}

//-----------------------------------------------------------------------------
KInPipe::KInPipe(const KString& sCommand)
//-----------------------------------------------------------------------------
{
	SetReaderTrim("");
	Open(sCommand);

} // Immediate Open Constructor

//-----------------------------------------------------------------------------
KInPipe::~KInPipe()
//-----------------------------------------------------------------------------
{
	Close();

} // Default Destructor

//-----------------------------------------------------------------------------
bool KInPipe::Open(const KString& sCommand)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KInPipe::Open(): {}", sCommand);

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sCommand.empty())
	{
		OpenReadPipe(sCommand);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!m_readPipe)
	{
		KLog().debug (0, "KInPipe::Open(): OpenReadPipe CMD FAILED: {} ERROR: {}", sCommand, strerror(errno));
		m_iReadExitCode = errno;
		return false;
	}
	else
	{
		KLog().debug(3, "KInPipe::Open(): OpenReadPipe: ok...");
		KFPReader::open(m_readPipe);
		return KFPReader::good();
	}

} // Open

//-----------------------------------------------------------------------------
bool KInPipe::OpenReadPipe(const KString& sCommand)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_readPipe              = nullptr;
	m_readPid               = -2;
	m_bReadChildStatusValid = false;
	m_iReadChildStatus      = -2;
	m_iReadExitCode         = -2;

	// try to open a pipe
	if (pipe(m_readPdes) < 0)
	{
		// no pipe
		return false;
	} // could not create pipe

	// create a child
	switch (m_readPid = vfork())
	{
		case -1: /* error */
		{
			// could not create the child
			(void)::close(m_readPdes[0]);
			(void)::close(m_readPdes[1]);
			m_readPid = -2;
			break;
		}

		case 0: /* child */
		{
			(void)::close(m_readPdes[0]);
			if (m_readPdes[1] != fileno(stdout))
			{
				(void)dup2(m_readPdes[1], fileno(stdout));
				(void)::close(m_readPdes[1]);
			}

			// execute the command
			execl("/bin/sh", "sh", "-c", sCommand.c_str(), NULL);
			_exit(127);
		} // end case 0

	} // end switch

	/* only parent gets here; assume fdopen can't fail...  */
	m_readPipe = ::fdopen(m_readPdes[0], "r");
	(void)::close(m_readPdes[1]);

	return true;
}
} // end namespace dekaf2
