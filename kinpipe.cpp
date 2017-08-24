#include "kinpipe.h"
#include "klog.h"

//#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>

namespace dekaf2
{

//-----------------------------------------------------------------------------
KInPipe::KInPipe()
//-----------------------------------------------------------------------------
{}

//-----------------------------------------------------------------------------
KInPipe::KInPipe(const KString& sCommand)
//-----------------------------------------------------------------------------
{
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
int KInPipe::Close ()
//-----------------------------------------------------------------------------
{
	int iExitCode = -1;
	int fdes = 0;

	// is the pipe still valid?
	if (NULL == m_readPipe)
	{
		return iExitCode;
	} // not open

	/*
	 * pclose returns -1 if stream is not associated with a
	 * `popened' command, if already `pclosed', or waitpid
	 * returns an error.
	 */
	if ((fdes = fileno(m_readPipe)) == 0)
	{
		return iExitCode;
	} // could not convert the FILE to fd

	(void)fclose(m_readPipe);
	m_readPipe = NULL;


	// is the child still running?
	if (false == IsRunning())
	{
		iExitCode = m_iReadExitCode;

		return (iExitCode);
	} // child not running

	// the child process has been giving us trouble. Kill it
	if (-2 != m_readPid)
	{
		kill(m_readPid, SIGKILL);
	}

	m_readPid = -2;

	return (iExitCode);

} // Close

//-----------------------------------------------------------------------------
bool KInPipe::IsRunning()
//-----------------------------------------------------------------------------
{

	bool bResponse = false;

	// sets m_iReadChildStatus if iPid is not zero
	pid_t iPid;
	if (!wait(iPid))
	{
		return true;
	}

	// Did we fail to get a status?
	if (-1 == m_iReadChildStatus)
	{
		m_iReadExitCode = -1;
		return bResponse;
	}

	// Do we have an exit status code to interpret?
	if (false == m_bReadChildStatusValid)
	{
		bResponse = true;
		return bResponse;
	}

	// did the called function "exit" normally?
	if (WIFEXITED(m_iReadChildStatus))
	{
		m_iReadExitCode = WEXITSTATUS(m_iReadChildStatus);
		return bResponse;
	}

	//m_iReadExitCode = -1;

	return bResponse;

} // IsRunning

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
			::close(m_readPdes[0]);
			::close(m_readPdes[1]);
			m_readPid = -2;
			break;
		}

		case 0: /* child */
		{
			::close(m_readPdes[0]);
			if (m_readPdes[1] != fileno(stdout))
			{
				::dup2(m_readPdes[1], fileno(stdout));
				::close(m_readPdes[1]);
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
} // OpenReadPipe

//-----------------------------------------------------------------------------
bool KInPipe::wait(pid_t iPid)
//-----------------------------------------------------------------------------
{
	int iStatus = 0;

	// status can only be read ONCE
	if (true == m_bReadChildStatusValid)
	{
		// status has already been set. do not read it again, you might get an invalid status.
		iPid = m_readPid;
		return true;
	} // end status is already set

	iPid = waitpid(m_readPid, &iStatus, WNOHANG);

	// is the status valid?
	if (0 < iPid)
	{
		// save the status
		m_iReadChildStatus = iStatus;
		m_bReadChildStatusValid = true;
		return true;
	}

	if ((iPid == -1) && (errno != EINTR))
	{
		// TODO log
		m_iReadChildStatus = -1;
		m_bReadChildStatusValid = true;
		return true;
	}

	return false;
} // wait

} // end namespace dekaf2
