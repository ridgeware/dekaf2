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
		m_iExitCode = errno;
		return false;
	}
	else
	{
		KLog().debug(3, "KPipe::Open(): OpenPipeRW: ok...");

		m_reader.open(m_readPdes[0]);
		m_writer.open(m_writePdes[1]);
		return (m_reader.good() && m_writer.good());
	}

} // Open

//-----------------------------------------------------------------------------
int KPipe::Close()
//-----------------------------------------------------------------------------
{
	int iExitCode = -1;

	// Close read on of stdout pipe
	::close(m_readPdes[0]);
	m_reader.close();
	// Send EOF by closing write end of pipe
	::close(m_writePdes[1]);
	m_writer.close();

	// Child has been cut off from parent, let it terminate
	WaitForFinished(60000);

	// Did the child terminate properly?
	if (false == IsRunning())
	{
		iExitCode = m_iExitCode;
	} // child not running

	// the child process has been giving us trouble. Kill it
	else
	{
		kill(m_pid, SIGKILL);
	}

	m_pid = -2;
	m_writePdes[0] = -2;
	m_writePdes[1] = -2;
	m_readPdes[0] = -2;
	m_readPdes[1] = -2;

	return (iExitCode);

} // Close

//-----------------------------------------------------------------------------
bool KPipe::OpenPipeRW(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_pid               = -2;
	m_bChildStatusValid = false;
	m_iChildStatus      = -2;
	m_iExitCode         = -2;

	// try to open read and write pipes
	if ((pipe(m_readPdes) < 0) || (pipe(m_writePdes) < 0))
	{
		return false;
	} // could not create pipe

	// create a child
	switch (m_pid = vfork())
	{
		case -1: /* error */
		{
			// could not create the child
			::close(m_readPdes[0]);
			::close(m_readPdes[1]);
			::close(m_writePdes[0]);
			::close(m_writePdes[1]);
			m_pid = -2;
			break;
		}

		case 0: /* child */
		{
			// Bind to Child's stdin
			::close(m_writePdes[1]);
			if (m_writePdes[0] != fileno(stdin))
			{
				::dup2(m_writePdes[0], fileno(stdin));
				::close(m_writePdes[0]);
			}

			// Bind Child's stdout
			::close(m_readPdes[0]);
			if (m_readPdes[1] != fileno(stdout))
			{
				::dup2(m_readPdes[1], fileno(stdout));
				::close(m_readPdes[1]);
			}

			// execute the command
			KString sCmd(sProgram); // need non const for split
			std::vector<char*> argV;
			splitArgs(sCmd, argV);

			execvp(argV[0], const_cast<char* const*>(argV.data()));

			_exit(127);
		} // end case 0

	} // end switch

	/* only parent gets here; */
	::close(m_readPdes[1]); // close write end of read pipe (for child use)
	::close(m_writePdes[0]); // close read end of write pipe (for child use)

	return true;
} // OpenPipeRW

} // end namespace dekaf2
