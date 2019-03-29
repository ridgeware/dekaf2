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

#pragma once

// Dekaf Includes
#include "kcppcompat.h"
#include "../kstring.h"

#ifdef DEKAF2_HAS_PIPES

// Generic Includes
#include <sys/wait.h>

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBasePipe
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Checks if child on other side of pipe is still running
	bool IsRunning();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Waits up to the number of given milliseconds for the child to terminate
	/// Will return early if child terminates
	bool Wait(int msecs);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get error code, 0 indicates no errors
	int GetErrno()
	//-----------------------------------------------------------------------------
	{
		return m_iExitCode;
	}

//--------
protected:
//--------

	enum OpenMode
	{
		PipeRead =  1 << 0,
		PipeWrite = 1 << 1
	};

	pid_t m_pid { 0 };
	int m_iExitCode { 0 };

	// we use this nested arrangement to ensure we have all descriptors in one single array
	int m_readPdes[4] { -1,-1,-1,-1 };
	int* m_writePdes { &m_readPdes[2] };

	//-----------------------------------------------------------------------------
	void wait();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Open(KString sCommand, bool bAsShellCommand, int mode);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	int Close(int mode);
	//-----------------------------------------------------------------------------

}; // KBasePipe

}

#endif
