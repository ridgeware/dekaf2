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

#include <dekaf2/system/process/bits/kbaseprocess.h>
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
		PipeWrite = 1 << 1,
		Text      = 1 << 2  ///< on Windows, the pipes translate between CRLF and LF, as with _popen()
	};

	//-----------------------------------------------------------------------------
	/// Get error code, 0 indicates no errors (alias for GetExitCode)
	int GetErrno()
	//-----------------------------------------------------------------------------
	{
		return m_iExitCode;
	}

	//-----------------------------------------------------------------------------
	/// Terminate the running process. Initially with signal SIGINT, after Timeout with SIGKILL.
	/// In an own process group, the signals reach all processes of the group. Windows has no
	/// SIGINT for a single child: there the child is terminated right away, together with all
	/// processes it started.
	bool Kill(KDuration Timeout);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the process group for the next Open(), see KProcessGroup. Default is
	/// KProcessGroup::Auto. Without effect on Windows.
	void SetProcessGroup(KProcessGroup::Mode Group)
	//-----------------------------------------------------------------------------
	{
		m_ProcessGroup = Group;
	}

//--------
protected:
//--------

	OpenMode            m_Mode         { OpenMode::None };
	KProcessGroup::Mode m_ProcessGroup { KProcessGroup::Auto };

	// we use this nested arrangement to ensure we have all descriptors in one single array
	int      m_readPdes[4] { -1, -1, -1, -1 };
	int*     m_writePdes   { &m_readPdes[2] };

	//-----------------------------------------------------------------------------
	/// opens the pipe(s) and executes sCommand - split at whitespace into arguments
	/// when sShell is empty, else passed as one argument to `sShell -c`. On Windows,
	/// sCommand is the command line for the child, which splits it into its arguments
	/// itself, and with any sShell the command interpreter (%COMSPEC%, cmd.exe) executes
	/// sCommand.
	bool Open(KString sCommand, KStringViewZ sShell, OpenMode Mode,
			  const std::vector<std::pair<KString, KString>>& Environment);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// opens the pipe(s) and executes the argument vector as is - no shell, no word
	/// splitting, hence safe for arguments with whitespace or quotes (like file names
	/// from outside). The first element is the program, searched in the PATH
	bool Open(std::vector<KString> Args, OpenMode Mode,
			  const std::vector<std::pair<KString, KString>>& Environment);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	int Close(KDuration Timeout = KDuration::max());
	//-----------------------------------------------------------------------------

//--------
private:
//--------

#ifdef DEKAF2_IS_WINDOWS
	//-----------------------------------------------------------------------------
	/// opens the pipe(s) and starts the child with a Windows command line
	bool OpenCommandLine(KStringView sCommandLine, OpenMode Mode,
	                     const std::vector<std::pair<KString, KString>>& Environment);
	//-----------------------------------------------------------------------------
#endif

}; // KBasePipe

DEKAF2_ENUM_IS_FLAG(KBasePipe::OpenMode)

DEKAF2_NAMESPACE_END

#endif
