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
/// Execute another process and attach write pipe to its std::in
class KOutPipe : public KFDWriter, public KBasePipe
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
	KOutPipe(KStringView sProgram);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Default Destructor
	~KOutPipe();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Opens A WritePipe
	bool Open(KStringView sProgram);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes A WritePipe
	int Close();
	//-----------------------------------------------------------------------------

//--------
private:
//--------

	//-----------------------------------------------------------------------------
	/// Opens a pipe for writing
	bool OpenWritePipe(KStringView sProgram);
	//-----------------------------------------------------------------------------

	int   m_writePdes[2]{-1,-1};

}; // class KOutPipe

} // end namespace dekaf2

#endif
