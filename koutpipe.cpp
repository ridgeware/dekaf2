/*
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

#ifdef DEKAF2_HAS_PIPES

#include "kchildprocess.h"
#include "ksplit.h"
#include "klog.h"
#include <signal.h>

namespace dekaf2
{

//-----------------------------------------------------------------------------
KOutPipe::KOutPipe()
//-----------------------------------------------------------------------------
{} // Default Constructor

//-----------------------------------------------------------------------------
KOutPipe::KOutPipe(KStringView sProgram)
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
bool KOutPipe::Open(KStringView sProgram)
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
	OpenWritePipe(sProgram);

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
		kDebug(3, "opened program {} successfully...", sProgram);
		KFDWriter::open(m_writePdes[1]);
		return KFDWriter::good();
	}

} // Open

//-----------------------------------------------------------------------------
int KOutPipe::Close()
//-----------------------------------------------------------------------------
{
	if (m_pid > 0)
	{
		// Close stream
		KFDWriter::close();
		// Send EOF by closing write end of pipe
		::close(m_writePdes[1]);
		// Child has been cut off from parent, let it terminate for up to a minute
		WaitForFinished(60000);

		// Did the child terminate properly?
		if (IsRunning())
		{
			// the child process has been giving us trouble. Kill it
			kill(m_pid, SIGKILL);
			m_iExitCode = -1;
		}

		m_pid = -1;
		m_writePdes[0] = -1;
		m_writePdes[1] = -1;
	}
	
	return (m_iExitCode == EXIT_CODE_NOT_SET) ? -1 : m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
bool KOutPipe::OpenWritePipe(KStringView sProgram)
//-----------------------------------------------------------------------------
{
	// Reset status vars and pipes.
	m_pid       = -1;
	m_iExitCode = EXIT_CODE_NOT_SET;

	// try to open a pipe
	if (pipe(m_writePdes) < 0)
	{
		// could not create pipe
		return false;
	}

	// we need to do the object allocations in the parent
	// process as otherwise leak detectors would claim the
	// child has lost allocated memory (as the child would
	// never run the destructor)
	KString sCmd(sProgram); // need non const for split
	std::vector<const char*> argV;
	kSplitArgsInPlace(argV, sCmd);
	// terminate with nullptr
	argV.push_back(nullptr);

	// create a child
	switch (m_pid = vfork())
	{
		case -1: /* error */
		{
			// could not create the child
			::close(m_writePdes[0]);
			::close(m_writePdes[1]);
			m_pid = -1;
			break;
		}

		case 0: /* child */
		{
			detail::kCloseOwnFilesForExec(false, m_writePdes, 2);

			::close(m_writePdes[1]);
			if (m_writePdes[0] != fileno(stdin))
			{
				::dup2(m_writePdes[0], fileno(stdin));
				::close(m_writePdes[0]);
			}

			// execute the command
			execvp(argV[0], const_cast<char* const*>(argV.data()));

			_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
		} // end case 0

	} // end switch

	/* only parent gets here */
	::close(m_writePdes[0]);

	return true;
} // OpenReadPipe

} // end namespace dekaf2

#endif
