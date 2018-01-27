/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include <signal.h>
#include "kpipe.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KPipe::KPipe()
//-----------------------------------------------------------------------------
{} // Default Constructor

//-----------------------------------------------------------------------------
KPipe::KPipe(KStringView sProgram)
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
bool KPipe::Open(KStringView sProgram)
//-----------------------------------------------------------------------------
{
	kDebug(3, "Program to be opened: {}", sProgram);

	Close(); // ensure a previous pipe is closed

	if (sProgram.empty())
	{
		return false;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// Use vfork()/execvp() to run the program:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	OpenPipeRW(sProgram);

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_writePdes[0] == -1)
	{
		kWarning("FAILED to open program: {} | ERROR: {}", sProgram, strerror(errno));
		m_iExitCode = errno;
		return false;
	}
	else
	{
		kDebug(3, "opened program successfully...");
		KFDStream::open(m_readPdes[0], m_writePdes[1]);
		return KFDStream::good();
	}

} // Open

//-----------------------------------------------------------------------------
int KPipe::Close()
//-----------------------------------------------------------------------------
{
	int iExitCode = -1;

	if (m_bIsOpen)
	{
		// Close reader/writer
		KFDStream::close();
		// Close read on of stdout pipe
		::close(m_readPdes[0]);
		// Send EOF by closing write end of pipe
		::close(m_writePdes[1]);
		// Child has been cut off from parent, let it terminate
		WaitForFinished(60000);

		// Did the child terminate properly?
		if (false == IsRunning())
		{
			iExitCode = m_iExitCode;
		} // child not running
		else 	// the child process has been giving us trouble. Kill it
		{
			kill(m_pid, SIGKILL);
		}

		m_pid = -1;
		m_writePdes[0] = -1;
		m_writePdes[1] = -1;
		m_readPdes[0] = -1;
		m_readPdes[1] = -1;
		m_bIsOpen = false;
	}

	return (iExitCode);

} // Close

//-----------------------------------------------------------------------------
bool KPipe::OpenPipeRW(KStringView sProgram)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_pid               = -1;
	m_bChildStatusValid = false;
	m_iChildStatus      = -1;
	m_iExitCode         = -1;

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
			splitArgsInPlace(sCmd, argV);

			execvp(argV[0], const_cast<char* const*>(argV.data()));

			_exit(127);
		} // end case 0

	} // end switch

	/* only parent gets here; */
	::close(m_readPdes[1]); // close write end of read pipe (for child use)
	::close(m_writePdes[0]); // close read end of write pipe (for child use)

	m_bIsOpen = true;

	return true;
} // OpenPipeRW

} // end namespace dekaf2
