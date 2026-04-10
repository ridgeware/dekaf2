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

#include "kbaseprocess.h"
#include <dekaf2/core/strings/kstring.h>

#ifdef DEKAF2_HAS_PIPES

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class DEKAF2_PUBLIC KBasePipe : public KBaseProcess
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum OpenMode
	{
		None      = 0,
		PipeRead  = 1 << 0,
		PipeWrite = 1 << 1
	};

	//-----------------------------------------------------------------------------
	/// Get error code, 0 indicates no errors (alias for GetExitCode)
	int GetErrno()
	//-----------------------------------------------------------------------------
	{
		return m_iExitCode;
	}

	//-----------------------------------------------------------------------------
	/// Terminate the running process. Initially with signal SIGINT, after Timeout with SIGKILL
	bool Kill(KDuration Timeout);
	//-----------------------------------------------------------------------------

//--------
protected:
//--------

	OpenMode m_Mode        { OpenMode::None };

	// we use this nested arrangement to ensure we have all descriptors in one single array
	int      m_readPdes[4] { -1, -1, -1, -1 };
	int*     m_writePdes   { &m_readPdes[2] };

	//-----------------------------------------------------------------------------
	bool Open(KString sCommand, KStringViewZ sShell, OpenMode Mode,
			  const std::vector<std::pair<KString, KString>>& Environment);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	int Close(KDuration Timeout = KDuration::max());
	//-----------------------------------------------------------------------------

}; // KBasePipe

DEKAF2_ENUM_IS_FLAG(KBasePipe::OpenMode)

DEKAF2_NAMESPACE_END

#endif
