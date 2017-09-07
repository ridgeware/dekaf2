/*
//-----------------------------------------------------------------------------//
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
*/

#include "kinpipe.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KInPipe::KInPipe()
//-----------------------------------------------------------------------------
{} // Default Constructor

//-----------------------------------------------------------------------------
KInPipe::KInPipe(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	Open(sProgram);

} // Immediate Open Constructor

//-----------------------------------------------------------------------------
KInPipe::~KInPipe()
//-----------------------------------------------------------------------------
{
	Close();

} // Default Destructor

//-----------------------------------------------------------------------------
bool KInPipe::Open(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KInPipe::Open(): {}", sProgram);

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// Use vfork()/execvp() to run the program:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sProgram.empty())
	{
		OpenReadPipe(sProgram);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_readPdes[0] == -2)
	{
		KLog().debug (0, "KInPipe::Open(): OpenReadPipe CMD FAILED: {} ERROR: {}", sProgram, strerror(errno));
		m_iReadExitCode = errno;
		return false;
	}
	else
	{
		KLog().debug(3, "KInPipe::Open(): OpenReadPipe: ok...");
		KFDReader::open(m_readPdes[0]);
		return KFDReader::good();
	}

} // Open

//-----------------------------------------------------------------------------
int KInPipe::Close ()
//-----------------------------------------------------------------------------
{
	int iExitCode = -1;

	// Close the pipe
	::close(m_readPdes[1]);
	// Child has been cut off from parent, let it terminate
	WaitForFinished(60000);

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
	wait();

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

	return bResponse;

} // IsRunning

//-----------------------------------------------------------------------------
bool KInPipe::OpenReadPipe(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_readPid               = -2;
	m_bReadChildStatusValid = false;
	m_iReadChildStatus      = -2;
	m_iReadExitCode         = -2;

	// try to open a pipe
	if (pipe(m_readPdes) < 0)
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

	/* only parent gets here */
	::close(m_readPdes[1]);

	return true;
} // OpenReadPipe

//-----------------------------------------------------------------------------
bool KInPipe::wait()
//-----------------------------------------------------------------------------
{
	int iStatus = 0;

	pid_t iPid;

	// status can only be read ONCE
	if (true == m_bReadChildStatusValid)
	{
		// status has already been set. do not read it again, you might get an invalid status.
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
