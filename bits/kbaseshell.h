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

/// @file kbaseshell.h
/// basic shell I/O class

#include "../kdefinitions.h"
#include "../kstring.h"
#include <cstdio>

#ifndef DEKAF2_IS_UNIX

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KBaseShell
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
	/// Default Constructor
	KBaseShell() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructor
	~KBaseShell();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes pipe saving exit code. The wait timeout is not used
	int Close(int iWaitMilliseconds = -1);
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

	//-----------------------------------------------------------------------------
	/// Executes given command via a shell pipe saving FILE* pipe in class member
	bool IntOpen(KString sCommand, bool bWrite,
				 const std::vector<std::pair<KString, KString>>& Environment = {});
	//-----------------------------------------------------------------------------

	FILE* m_pipe      { nullptr };
	int   m_iExitCode { 0 };


}; // class KPIPE

DEKAF2_NAMESPACE_END

#endif // !DEKAF2_IS_UNIX
