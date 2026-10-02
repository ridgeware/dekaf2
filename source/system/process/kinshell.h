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

/// @file kinshell.h
/// provides reading pipe access to a shell instance.

#include <dekaf2/io/pipes/kinpipe.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup system_process
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Read on a shell instance. Unlike KInPipe, the pipe is in text mode on Windows: it
/// translates CRLF into LF, as with _popen(). On Windows, the command interpreter
/// (%COMSPEC%, cmd.exe) executes the command for any shell name.
class DEKAF2_PUBLIC KInShell : public KInPipe
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Default KInShell Constructor
	KInShell() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructor which takes and executes command immediately
	/// @param sCommand the command to execute
	/// @param sShell path to a shell to use for execution of the command (e.g. "/bin/sh"). If empty will execute child directly
	/// @param Environment a vector of a pair of KString name and values that will be added to the child's environment
	KInShell(KString sCommand, KStringViewZ sShell = "/bin/sh",
	         const std::vector<std::pair<KString, KString>>& Environment = {})
	//-----------------------------------------------------------------------------
	{
		Open(std::move(sCommand), sShell, Environment);
	}

	//-----------------------------------------------------------------------------
	/// Executes given command via a shell pipe from which output can be read
	/// @param sCommand the command to execute
	/// @param sShell path to a shell to use for execution of the command (e.g. "/bin/sh"). If empty will execute child directly
	/// @param Environment a vector of a pair of KString name and values that will be added to the child's environment
	/// @return true on success
	bool Open(KString sCommand, KStringViewZ sShell = "/bin/sh",
	          const std::vector<std::pair<KString, KString>>& Environment = {});
	//-----------------------------------------------------------------------------

}; // END KInShell


/// @}

DEKAF2_NAMESPACE_END
