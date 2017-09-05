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

#pragma once

#include "kstring.h"
#include "kfdwriter.h"

// TODO REMOVE
#include <iostream>

namespace dekaf2

{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KOutPipe : public KFPWriter
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/*
	 * The sProgram is a KString of this format:
	 * path_to_program arg1 arg2 arg3...
	 * where path_to_program will also be handed in as argv[0]
	 * If spaces are needed within an arg, use " :
	 * path_to_program arg1 "arg2 with spaces" arg3
	 */

	//-----------------------------------------------------------------------------
	/// Default Constructor
	KOutPipe();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Open Constructor
	KOutPipe(const KString& sProgram);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Default Virtual Destructor
	virtual ~KOutPipe();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Opens A WritePipe
	virtual bool Open(const KString& sProgram);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes A WritePipe
	virtual int Close();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Checks if child on other side of pipe is still running
	bool IsRunning();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool WaitForFinished(int msecs);
	//-----------------------------------------------------------------------------

//--------
public:
//--------

	//-----------------------------------------------------------------------------
	/// Opens a pipe for writing
	bool OpenWritePipe(const KString& sProgram);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool wait();
	//-----------------------------------------------------------------------------

	typedef std::vector<char*> CharVec;

	//-----------------------------------------------------------------------------
	/// Splits args into char*[] terminated with NULL
	bool splitArgs(KString& argString, CharVec& vector );
	//-----------------------------------------------------------------------------

	FILE* m_writePipe{nullptr};
	pid_t m_writePid{-2};
	int   m_iWriteExitCode{0};
	int   m_writePdes[2]{-2,-2};
	int   m_iWriteChildStatus{-2};
	bool  m_bWriteChildStatusValid{false};

}; // class KOutPipe

} // end namespace dekaf2
