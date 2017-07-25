///////////////////////////////////////////////////////////////////////////////
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "kstring.h"
#include "kstreamiter.h"

/*
KPIPE pipe;
pipe.Open ("ls -l", "r");
KString sOutput;

for (auto it = pipe.begin(); it != pipe.END(); ++it) {
	sOutput += *it;

}
*/

namespace dekaf2
{

//-----------------------------------------------------------------------------
class KPIPE : public KStreamIter
//-----------------------------------------------------------------------------
{
public:
	KPIPE ();
	virtual ~KPIPE ();

	bool Open        (const KString& sCommand, const char* pszMode="r");
	int  Close       ();
	bool isOpen      ()       { return (m_pipe != NULL); }

	bool getline     (KString& sTheLine, size_t iMaxLen=0, bool bTextOnly = false);
	virtual bool getNext() { return getline(m_sLine, m_iLen); }
	bool appendline  (KString& sBufferToAppend, size_t iMaxLen=0);

	int getErrno     () const { return m_iExitcode; } //0 = success

	operator FILE*          ();
	operator const KString& ();
	operator KString        ();
	//KStreamIter& getIter();
/*
	const_iterator begin();
	const_iterator end();    // help from theo guys
*/
private:
	FILE*        m_pipe{nullptr};
	//KStreamIter  m_kiter;
	int          m_iExitcode{0};
}; // class KPIPE

} // end of namespace DEKAF2
