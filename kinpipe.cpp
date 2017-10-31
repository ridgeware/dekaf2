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
KInPipe::KInPipe(KStringView sProgram)
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
bool KInPipe::Open(KStringView sProgram)
//-----------------------------------------------------------------------------
{
	kDebug(3, "Program to be opened: {}", sProgram);

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
	if (m_readPdes[0] == -1)
	{
		kWarning("FAILED to open program: {} | ERROR: {}", sProgram, strerror(errno));
		m_iExitCode = errno;
		return false;
	}
	else
	{
		kDebug(3, "opened program {} successfully...", sProgram);
		KFDReader::open(m_readPdes[0]);
		return KFDReader::good();
	}

} // Open

//-----------------------------------------------------------------------------
int KInPipe::Close ()
//-----------------------------------------------------------------------------
{
	int iExitCode = -1;

	// Close Stream
	KFDReader::close();
	// Close the pipe
	::close(m_readPdes[1]);
	// Child has been cut off from parent, let it terminate for up to a minute
	WaitForFinished(60000);

	// is the child still running?
	if (false == IsRunning())
	{
		// child not running
		iExitCode = m_iExitCode;
	}
	else
	{
		// the child process has been giving us trouble. Kill it
		kill(m_pid, SIGKILL);
	}

	m_pid = -1;
	m_readPdes[0] = -1;
	m_readPdes[1] = -1;

	return (iExitCode);

} // Close

//-----------------------------------------------------------------------------
bool KInPipe::OpenReadPipe(KStringView sProgram)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_pid               = -1;
	m_bChildStatusValid = false;
	m_iChildStatus      = -1;
	m_iExitCode         = -1;

	// try to open a pipe
	if (pipe(m_readPdes) < 0)
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
			m_pid = -1;
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
			splitArgsInPlace(sCmd, argV);

			execvp(argV[0], const_cast<char* const*>(argV.data()));

			_exit(127);
		} // end case 0

	} // end switch

	/* only parent gets here */
	::close(m_readPdes[1]);

	return true;
} // OpenReadPipe

} // end namespace dekaf2
