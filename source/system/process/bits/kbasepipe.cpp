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

#include "kbasepipe.h"

#ifdef DEKAF2_HAS_PIPES

#include <dekaf2/ksplit.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/kchildprocess.h>
#include <dekaf2/klog.h>
#include <dekaf2/kstring.h>
#include <dekaf2/kformat.h>
#include <csignal>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KBasePipe::Open(KString sCommand, KStringViewZ sShell, OpenMode Mode, const std::vector<std::pair<KString, KString>>& Environment)
//-----------------------------------------------------------------------------
{
	Close(); // ensure a previous pipe is closed

	if (m_pid)
	{
		kWarning("cannot open pipe '{}': {}", sCommand, "old one still running");
		return false;
	}

	m_Mode = Mode;

	m_iExitCode = 0;

	sCommand.TrimLeft();

	if (sCommand.empty())
	{
		m_iExitCode = EINVAL;
		return false;
	}

	if (m_Mode & PipeRead)
	{
		if (::pipe(m_readPdes) < 0)
		{
			kWarning("cannot open input pipe '{}': {}", sCommand, ::strerror(errno));
			return false;
		}
	}

	if (m_Mode & PipeWrite)
	{
		if (::pipe(m_writePdes) < 0)
		{
			kWarning("cannot open output pipe '{}': {}", sCommand, ::strerror(errno));
			return false;
		}
	}

	kDebug(2, "executing: {}", sCommand);

	// we need to do the object allocations in the parent
	// process as otherwise leak detectors would claim the
	// child has lost allocated memory (as the child would
	// never run the destructor)

	std::vector<const char*> argV;

	if (sShell.empty())
	{
		kSplitArgsInPlace(argV, sCommand);
	}
	else
	{
		argV.reserve(4);
		argV.push_back(sShell.c_str());
		argV.push_back("-c");
		argV.push_back(sCommand.c_str());
	}

	// terminate with nullptr
	argV.push_back(nullptr);

	// create a child
	switch (m_pid = ::fork())
	{
		case -1: /* error */
		{
			kWarning("cannot fork '{}': {}", sCommand, ::strerror(errno));

			// could not create the child
			if (m_Mode & PipeRead)
			{
				CloseAndResetFileDescriptor(m_readPdes[0]);
				// the other side gets closed anyway in the regular parent code
			}

			if (m_Mode & PipeWrite)
			{
				CloseAndResetFileDescriptor(m_writePdes[1]);
				// the other side gets closed anyway in the regular parent code
			}

			m_pid = 0;
			break;
		}

		case 0: /* child */
		{
			detail::kCloseOwnFilesForExec(false, m_readPdes, 4);

			// enable SIGPIPE!
			::signal(SIGPIPE, SIG_DFL);

			if (m_Mode & PipeWrite)
			{
				// Bind to Child's stdin
				CloseAndResetFileDescriptor(m_writePdes[1]);
				if (m_writePdes[0] != ::fileno(stdin))
				{
					::dup2(m_writePdes[0], ::fileno(stdin));
					CloseAndResetFileDescriptor(m_writePdes[0]);
				}
			}

			if (m_Mode & PipeRead)
			{
				// Bind Child's stdout
				CloseAndResetFileDescriptor(m_readPdes[0]);
				if (m_readPdes[1] != ::fileno(stdout))
				{
					::dup2(m_readPdes[1], ::fileno(stdout));
					CloseAndResetFileDescriptor(m_readPdes[1]);
				}
			}

			// set additional environment variables. execvpe() is a GNU extension
			// and cannot be used on MacOS or Windows.
			// also note that we do not replace the parent's environment, instead
			// we inherit it
			kSetEnv(Environment);

			// execute the command
			::execvp(argV[0], const_cast<char* const*>(argV.data()));

			::_exit(DEKAF2_POPEN_COMMAND_NOT_FOUND);
		}

	}

	// only parent gets here

	if (m_Mode & PipeRead)
	{
		// close write end of read pipe (for child use)
		CloseAndResetFileDescriptor(m_readPdes[1]);
	}

	if (m_Mode & PipeWrite)
	{
		// close read end of write pipe (for child use)
		CloseAndResetFileDescriptor(m_writePdes[0]);
	}

	return true;

} // Open

//-----------------------------------------------------------------------------
int KBasePipe::Close(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (m_pid > 0)
	{
		if (m_Mode & PipeRead)
		{
			// Close read on stdout pipe
			CloseAndResetFileDescriptor(m_readPdes[0]);
		}

		if (m_Mode & PipeWrite)
		{
			// send EOF by closing write end of pipe
			CloseAndResetFileDescriptor(m_writePdes[1]);
		}

		// child has been cut off from parent, let it terminate
		WaitOrKill(Timeout);
	}

	return m_iExitCode;

} // Close

//-----------------------------------------------------------------------------
bool KBasePipe::Kill(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return true;
	}

	// send a SIGINT
	::kill(m_pid, SIGINT);

	// call Close() which will send a SIGKILL after waiting
	Close(Timeout);

	return true;

} // Kill

DEKAF2_NAMESPACE_END

#endif
