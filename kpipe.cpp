#include "kpipe.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KPipe::KPipe()
//-----------------------------------------------------------------------------
{} // Default Constructor

//-----------------------------------------------------------------------------
KPipe::KPipe(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	Open(sProgram);

} // Immediate Open Constructor

//-----------------------------------------------------------------------------
KPipe::~KPipe()
//-----------------------------------------------------------------------------
{
	Close();

} // Default Destructor

//-----------------------------------------------------------------------------
bool KPipe::Open(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KPipe::Open(): {}", sProgram);

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// Use vfork()/execvp() to run the program:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sProgram.empty())
	{
		OpenPipeRW(sProgram);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_writePdes[0] == -2)
	{
		KLog().debug (0, "KPipe::Open(): OpenPipeRW CMD FAILED: {} ERROR: {}", sProgram, strerror(errno));
		m_iWriteExitCode = errno;
		return false;
	}
	else
	{
		KLog().debug(3, "KPipe::Open(): OpenPipeRW: ok...");
		KFDWriter::open(m_writePdes[1]);
		return KFDWriter::good();
	}

} // Open

//-----------------------------------------------------------------------------
int KPipe::Close()
//-----------------------------------------------------------------------------
{
	int iExitCode = -1;

	// Close read on of stdout pipe
	::close(m_readPdes[0]);
	// Send EOF by closing write end of pipe
	::close(m_writePdes[1]);
	// Child has been cut off from parent, let it terminate
	KOutPipe::WaitForFinished(60000);

	// Did the child terminate properly?
	if (false == IsRunning())
	{
		iExitCode = m_iWriteExitCode;
		return (iExitCode);
	} // child not running

	// the child process has been giving us trouble. Kill it
	else
	{
		kill(m_writePid, SIGKILL);
	}

	m_writePid = -2;
	m_writePdes[0] = -2;
	m_writePdes[1] = -2;

	m_readPid = -2;
	m_readPdes[0] = -2;
	m_readPdes[1] = -2;

	return (iExitCode);

} // Close

//-----------------------------------------------------------------------------
bool KPipe::IsRunning()
//-----------------------------------------------------------------------------
{

	bool bResponse = false;

	// sets m_iReadChildStatus if iPid is not zero
	wait();

	// Did we fail to get a status?
	if ((-1 == m_iWriteChildStatus) || (-1 == m_iReadChildStatus))
	{
		m_iWriteExitCode = -1;
		m_iReadExitCode = -1;
		return bResponse;
	}

	// Do we have an exit status code to interpret?
	if ((false == m_bWriteChildStatusValid) || (false == m_bReadChildStatusValid))
	{
		bResponse = true;
		return bResponse;
	}

	// did the called function "exit" normally?
	if ((WIFEXITED(m_iWriteChildStatus)) || (WIFEXITED(m_iReadChildStatus)))
	{
		m_iWriteExitCode = WEXITSTATUS(m_iWriteChildStatus);
		m_iReadExitCode = WEXITSTATUS(m_iReadChildStatus);
		return bResponse;
	}

	return bResponse;

} // IsRunning

//-----------------------------------------------------------------------------
bool KPipe::OpenPipeRW(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_readPid               = -2;
	m_bReadChildStatusValid = false;
	m_iReadChildStatus      = -2;
	m_iReadExitCode         = -2;

	// try to open read and write pipes
	if ((pipe(m_readPdes) < 0) || (pipe(m_writePdes) < 0))
	{
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
			m_writePid = m_readPid;

			// Bind Child's stdout
			::close(m_readPdes[0]);
			if (m_readPdes[1] != fileno(stdout))
			{
				::dup2(m_readPdes[1], fileno(stdout));
				::close(m_readPdes[1]);
			}

			// Bind to Child's stdin
			::close(m_writePdes[1]);
			if (m_writePdes[0] != fileno(stdin))
			{
				::dup2(m_writePdes[0], fileno(stdin));
				::close(m_writePdes[0]);
			}

			// execute the command
			KString sCmd(sProgram); // need non const for split
			std::vector<char*> argV;
			KOutPipe::splitArgs(sCmd, argV);

			execvp(argV[0], const_cast<char* const*>(argV.data()));

			_exit(127);
		} // end case 0

	} // end switch

	/* only parent gets here; */
	::close(m_readPdes[1]); // close write end of read pipe (for child use)
	::close(m_writePdes[0]); // close read end of write pipe (for child use)

	return true;
} // OpenPipeRW

//-----------------------------------------------------------------------------
// waitpid wrapper to ensure it is called only once after child exits
bool KPipe::wait()
//-----------------------------------------------------------------------------
{
	int iStatus = 0;

	pid_t iPid;

	// status can only be read ONCE
	if ((true == m_bWriteChildStatusValid) || (true == m_bReadChildStatusValid))
	{
		// status has already been set. do not read it again, you might get an invalid status.
		return true;
	} // end status is already set

	iPid = waitpid(m_writePid, &iStatus, WNOHANG);

	// is the status valid?
	if (0 < iPid)
	{
		// save the status
		m_iWriteChildStatus = iStatus;
		m_bWriteChildStatusValid = true;
		m_iReadChildStatus = iStatus;
		m_bReadChildStatusValid = true;
		return true;
	}

	if ((iPid == -1) && (errno != EINTR))
	{
		// TODO log
		m_iWriteChildStatus = -1;
		m_bWriteChildStatusValid = true;
		m_iReadChildStatus = -1;
		m_bReadChildStatusValid = true;

		return true;
	}

	return false;
} // wait

} // end namespace dekaf2
