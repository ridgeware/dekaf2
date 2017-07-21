///////////////////////////////////////////////////////////////////////////////
// DEKAF(tm): Lighter, Faster, Smarter(tm)
// Copyright (c) 2000-2017, Ridgeware, Inc.
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
///////////////////////////////////////////////////////////////////////////////
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include "klog.h"
#include "kpipe.h"
#include <iostream>
#include <streambuf>
namespace dekaf2
{

//-----------------------------------------------------------------------------
KPIPE::KPIPE ()
//-----------------------------------------------------------------------------
{
	// initialization moved to header file per c++ standard techniques
} // constructor

//-----------------------------------------------------------------------------
KPIPE::~KPIPE ()
//-----------------------------------------------------------------------------
{
	Close();
} // destructor

//-----------------------------------------------------------------------------
bool KPIPE::Open(const KString& sCommand, const char* pszMode)
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KPIPE::Open(): %s %s", sCommand.c_str(), pszMode);

	//std::cout << "KPIPE::Open (\"" << sCommand.c_str() << "\" , \"" << pszMode << "\")"<< std::endl;
	if (pszMode &&
	    (strcmp(pszMode, "r")  &&
	     strcmp(pszMode, "w")  &&
	     strcmp(pszMode, "rw") &&
	     strcmp(pszMode, "a")))
		KLog().warning("KPIPE::Open(): suspicious Mode '%s'", pszMode);
	    //std::cout << "KPIPE::Open(): suspicious mode\"" << pszMode << "\""<< std::endl;

	Close(); // ensure a previous pipe is closed
	errno = 0;

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// shell out to run the command:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	//m_pipe = pipeopen (sCommand, pszMode); // see dekaf.h for kpopen() macro
	// need to replace the stock popen with a custom version that allows
	// us to kill a hung process at the other end of the pipe.
	m_pipe = popen(sCommand.c_str(), pszMode);

	// - - - - - - - - - - - - - - - - - - - - - - - -
	// interpret success:
	// - - - - - - - - - - - - - - - - - - - - - - - -
	if (!m_pipe)
	{
		KLog().debug (0, "POPEN CMD FAILED: %s ERROR: %s",
		              sCommand.c_str(),
		              strerror(errno));
		exit(errno);
		//kCheckSystemResources (errno);
	}
	else
		KLog().debug(2, "POPEN: ok...");
	return (m_pipe != NULL);
} // Open

//-----------------------------------------------------------------------------
int KPIPE::Close()
//-----------------------------------------------------------------------------
{
	KLog().debug(3, "KPIPE::Close");

	if (m_pipe)
	{
		m_iExitcode = pclose (m_pipe);
	}
	m_pipe = nullptr;
	return (m_iExitcode);

	KLog().debug(3, "KPIPE::Close::Done:: Exit Code = %u", m_iExitcode);

	return (m_iExitcode);
} // Close

//-----------------------------------------------------------------------------
bool KPIPE::getline(KString & sOutputBuffer, size_t iMaxLen/*=0*/, bool bTextOnly/*=false*/ )
//-----------------------------------------------------------------------------
{
	/*
	 *
	Functionally identical to fgets with the result in a string object
		needs
			reference to an output buffer
			maximum number of bytes to receive
		returns
			true - all went well and there is data in the buffer to process
			false - there was an error or the pipe is empty
		NOTE:
			existing data in the output buffer will be discarded
	*/
	KLog().debug(3, "KPIPE::getline(KString & sOutputBuffer, size_t iMaxLen )");

	sOutputBuffer.clear();
	if (!m_pipe)
	{
		return false;
	}
	iMaxLen = (iMaxLen == 0) ? 999999: iMaxLen;
	for (int i = 0; i < iMaxLen; i++)
	{
		int iCh = fgetc(m_pipe);
		switch (iCh)
		{
			case EOF:
			{
				return !sOutputBuffer.empty();
			}
			case '\r':
				if (!bTextOnly) // don't want EOL chars in text only mode
				{
					sOutputBuffer += static_cast<KString::value_type>(iCh);
				}
				break;
			case '\n':
				if (!bTextOnly) // don't want EOL chars in text only mode
				{
					sOutputBuffer += static_cast<KString::value_type>(iCh);
				}
				return true;
			default:
				sOutputBuffer += static_cast<KString::value_type>(iCh);
				break;
		}
	}
	return (0 < sOutputBuffer.length());
} // getline

//-----------------------------------------------------------------------------
KPIPE::operator FILE* ()
//-----------------------------------------------------------------------------
{

	return (m_pipe);

}

} // end of namespace dekaf2
