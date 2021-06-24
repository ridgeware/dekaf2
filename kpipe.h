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

#include "bits/kbasepipe.h"

#ifdef DEKAF2_HAS_PIPES

#include "kfdstream.h"

namespace dekaf2
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Execute another process and attach pipes to its std::in and std::out
class KPipe : public KBasePipe, public KFDStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Default Constructor
	KPipe() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Opens a pipe. If sShell is not empty will execute command in a sub shell.
	/// @param sCommand the command to execute
	/// @param sShell path to a shell to use for execution of the command (e.g. "/bin/sh")
	KPipe(KString sCommand, KStringViewZ sShell = "")
	//-----------------------------------------------------------------------------
	{
		Open(std::move(sCommand), sShell);
	}

	//-----------------------------------------------------------------------------
	/// legacy, deprecated!
	/// Opens a pipe. If bAsShellCommand is true will execute command in a sub shell.
	KPipe(KString sCommand, bool bAsShellCommand)
	//-----------------------------------------------------------------------------
	{
		Open(std::move(sCommand), bAsShellCommand);
	}

	//-----------------------------------------------------------------------------
	~KPipe()
	//-----------------------------------------------------------------------------
	{
		Close();
	}

	//-----------------------------------------------------------------------------
	/// Opens a pipe. If sShell is not empty will execute command in a sub shell.
	/// @param sCommand the command to execute
	/// @param sShell path to a shell to use for execution of the command (e.g. "/bin/sh")
	bool Open(KString sCommand, KStringViewZ sShell = "");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// legacy, deprecated!
	/// Opens a pipe. If bAsShellCommand is true will execute command in a sub shell.
	bool Open(KString sCommand, bool bAsShellCommand)
	//-----------------------------------------------------------------------------
	{
		return Open(std::move(sCommand), KStringViewZ(bAsShellCommand ? "/bin/sh" : ""));
	}

	//-----------------------------------------------------------------------------
	/// Closes a pipe, waits for iWaitMilliseconds then kills child process, if -1
	/// waits until child terminates.
	int Close(int iWaitMilliseconds = -1);
	//-----------------------------------------------------------------------------

}; // class KPipe

} // end namespace dekaf2

#endif
