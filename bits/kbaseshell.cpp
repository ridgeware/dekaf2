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

#include "../klog.h"
#include "kbaseshell.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KBaseShell::~KBaseShell()
//-----------------------------------------------------------------------------
{
	Close();
}

//-----------------------------------------------------------------------------
bool KBaseShell::IntOpen (const KString& sCommand, bool bWrite)
//-----------------------------------------------------------------------------
{
	kDebug(3, "{}", sCommand);

	Close(); // ensure a previous pipe is closed

	if (sCommand.empty())
	{
		return false;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	m_pipe = popen(sCommand.c_str(), bWrite ? "w" : "r");

	if (!m_pipe)
	{
		kDebug (0, "POPEN CMD FAILED: {} ERROR: {}", sCommand, strerror(errno));
		m_iExitCode = errno;
		return false;
	}

	kDebug(3, "POPEN: ok...");
	return true;

} // IntOpen

//-----------------------------------------------------------------------------
int KBaseShell::Close()
//-----------------------------------------------------------------------------
{
	if (m_pipe)
	{
		int ret = pclose(m_pipe);

		if (ret)
		{
			// we see spurious errors here, but errno cannot reliably
			// be used for pclose (see man pages). So we assume it's
			// just noise, and log at a reduced level
			kDebug(1, "could probably not close popen file");
		}

		m_pipe = nullptr;
	}

	return 0;

} // Close

} // end of namespace dekaf2
