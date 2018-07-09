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
#include "../kstring.h"
#include "../klog.h"

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
	/// Default Constructor
	KBasePipe();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Default Virtual Destructor
	virtual ~KBasePipe();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Opens A Pipe
	virtual bool Open(KStringView sProgram) = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes A Pipe
	virtual int Close() = 0;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Checks if child on other side of pipe is still running
	virtual bool IsRunning();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Waits up to the number of given milliseconds for the child to terminate
	/// Will return early if child terminates
	bool WaitForFinished(int msecs);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Splits args into char*[] terminated with NULL
	typedef std::vector<char*> CharVec;
	bool splitArgsInPlace(KString& argString, CharVec& vector );
	//-----------------------------------------------------------------------------

//--------
protected:
//--------

	enum { EXIT_CODE_NOT_SET = INT_MIN };

	pid_t m_pid { -1 };
	int   m_iExitCode { EXIT_CODE_NOT_SET };

	//-----------------------------------------------------------------------------
	// waitpid wrapper to ensure it is called only once after child exits
	void wait();
	//-----------------------------------------------------------------------------

}; // KBasePipe

}
