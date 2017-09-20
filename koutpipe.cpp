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

#include "koutpipe.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KOutPipe::KOutPipe()
//-----------------------------------------------------------------------------
{} // Default Constructor

//-----------------------------------------------------------------------------
KOutPipe::KOutPipe(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	Open(sProgram);

} // Immediate Open Constructor

//-----------------------------------------------------------------------------
KOutPipe::~KOutPipe()
//-----------------------------------------------------------------------------
{
	Close();

} // Default Destructor

//-----------------------------------------------------------------------------
bool KOutPipe::Open(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	//## use kDebug - it prints automatically the function name
	KLog().debug(3, "KOutPipe::Open(): {}", sProgram);

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// Use vfork()/execvp() to run the program:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!sProgram.empty())
	{
		OpenWritePipe(sProgram);
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (m_writePdes[0] == -2)
	{
		//## use kWarning - it prints automatically the function name
		KLog().debug (0, "KOutPipe::Open(): OpenReadPipe CMD FAILED: {} ERROR: {}", sProgram, strerror(errno));
		m_iExitCode = errno;
		return false;
	}
	else
	{
		//## use kDebug - it prints automatically the function name
		KLog().debug(3, "KOutPipe::Open(): OpenReadPipe: ok...");
		KFDWriter::open(m_writePdes[1]);
		return KFDWriter::good();
	}

} // Open

//-----------------------------------------------------------------------------
int KOutPipe::Close()
//-----------------------------------------------------------------------------
{
	int iExitCode = -1;

	// Send EOF by closing write end of pipe
	::close(m_writePdes[1]);
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

	return (iExitCode);

} // Close

//-----------------------------------------------------------------------------
bool KOutPipe::OpenWritePipe(const KString& sProgram)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_pid               = -2;
	m_bChildStatusValid = false;
	m_iChildStatus      = -2;
	m_iExitCode         = -2;

	// try to open a pipe
	if (pipe(m_writePdes) < 0)
	{
		return false;
	} // could not create pipe

	// create a child
	switch (m_pid = vfork())
	{
		case -1: /* error */
		{
			// could not create the child
			::close(m_writePdes[0]);
			::close(m_writePdes[1]);
			m_pid = -2;
			break;
		}

		case 0: /* child */
		{
			::close(m_writePdes[1]);
			if (m_writePdes[0] != fileno(stdin))
			{
				::dup2(m_writePdes[0], fileno(stdin));
				::close(m_writePdes[0]);
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
	::close(m_writePdes[0]);

	return true;
} // OpenReadPipe

} // end namespace dekaf2
