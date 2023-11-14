/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kbar.h
/// provides ascii-style progress bar

#include "kstring.h"
#include "kstringview.h"
#include "kwriter.h"
#include <mutex>

namespace dekaf2
{

//-----------------------------------------------------------------------------
/// a single thread implementation of a progress bar
class DEKAF2_PUBLIC KBAR
//-----------------------------------------------------------------------------
{

//----------
public:
//----------

	enum
	{
		SLIDER = 0x000001,
		STATIC = 0x000010,

		DEFAULT_WIDTH     = 0,   // 0 = use current terminal width
		DEFAULT_DONE_CHAR = '%',

		KPS_START = 'S',
		KPS_ADD   = 'A',
		KPS_END   = 'E',
		KPS_STOP  = '!'
	};

	KBAR  (uint64_t iExpected=0, uint32_t iWidth=DEFAULT_WIDTH, uint64_t iFlags=SLIDER, int chDone=DEFAULT_DONE_CHAR, KOutStream& Out = KOut);
	~KBAR ();

	void      SetFlags (uint64_t iFlags);
	bool      Start  (uint64_t iExpected);
	bool      Adjust (uint64_t iExpected);
	bool      Move   (int64_t iDelta=1);
	void      Break  (KStringView sMsg="!!!");
	void      Finish ();
	KString   GetBar (uint16_t iStaticWidth=30, int chBlank=' ');
	uint64_t  GetSoFar() const { return m_iSoFar; }
	void      RepaintSlider ();
	bool      IsActive ()  { return (m_iExpected > 0); }
	uint64_t  GetExpected() { return m_iExpected; }

//----------
private:
//----------
	
	DEKAF2_PRIVATE
	void  _SliderAction(int iAction, uint64_t iSoFarLast, uint64_t iSoFarNow);

	uint64_t    m_iFlags;
	uint32_t    m_iWidth;
	uint64_t    m_iExpected;
	uint64_t    m_iSoFar;
	int         m_chDone{DEFAULT_DONE_CHAR};
	KOutStream& m_Out;
	bool        m_bSliding;

}; // KBAR

//-----------------------------------------------------------------------------
/// multi-thread wrapper for the progress bar
class DEKAF2_PUBLIC KSharedBar : private KBAR
//-----------------------------------------------------------------------------
{

//----------
public:
//----------

	using KBAR::KBAR;

	bool      Start  (uint64_t iExpected);
	bool      Adjust (uint64_t iExpected);
	bool      Move   (int64_t iDelta=1);
	void      Break  (KStringView sMsg="!!!");
	void      Finish ();
	KString   GetBar (uint16_t iStaticWidth=30, int chBlank=' ');
	uint64_t  GetSoFar() const;
	void      RepaintSlider ();

//----------
private:
//----------

	mutable std::mutex m_Mutex;

};

} // namespace dekaf2
