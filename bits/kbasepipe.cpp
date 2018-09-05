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
#include "../ksplit.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KBasePipe::KBasePipe()
//-----------------------------------------------------------------------------
{} // Default Constructor

//-----------------------------------------------------------------------------
KBasePipe::~KBasePipe()
//-----------------------------------------------------------------------------
{} // Default Destructor

//-----------------------------------------------------------------------------
void KBasePipe::wait()
//-----------------------------------------------------------------------------
{
	// status can only be read ONCE
	if (m_iExitCode == EXIT_CODE_NOT_SET)
	{
		int iStatus = 0;
		pid_t iPid = waitpid(m_pid, &iStatus, WNOHANG);

		// did the process terminate?
		if (iPid > 0)
		{
			// yes, check if return status is valid
			if (WIFEXITED(iStatus))
			{
				// yes, convert into exit code
				m_iExitCode = WEXITSTATUS(iStatus);
			}
			else
			{
				// set a dummy exit code
				m_iExitCode = -1;
			}
		}
		else if ((iPid == -1) && (errno != EINTR))
		{
			kDebug(0, "Got an invalid status iPid = -1. Errno {} : {}", errno, strerror(errno));
			m_iExitCode = -1;
		}
		// if iPid == 0 the process is still running
	}

} // wait

//-----------------------------------------------------------------------------
bool KBasePipe::IsRunning()
//-----------------------------------------------------------------------------
{
	if (m_pid <= 0)
	{
		return false;
	}

	wait();

	return (m_pid > 0 && m_iExitCode == EXIT_CODE_NOT_SET);

} // IsRunning

//-----------------------------------------------------------------------------
bool KBasePipe::WaitForFinished(int msecs)
//-----------------------------------------------------------------------------
{
	if (msecs >= 0)
	{
		int counter = 0;
		while (IsRunning())
		{
			usleep(1000);
			++counter;

			if (counter == msecs)
            {
				return false;
            }
		}
		return true;
	}
	return false;

} // WaitForFinished

} // end namespace dekaf2
