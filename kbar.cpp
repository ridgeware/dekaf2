///////////////////////////////////////////////////////////////////////////////
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2003, Ridgeware, Inc.
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

#include "kstring.h"
#include "kbar.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KBAR::KBAR (uint64_t iExpected/*=0*/, uint32_t iWidth/*=DEFAULT_WIDTH*/, uint64_t iFlags/*=SLIDER*/, int chDone/*='%'*/)
//-----------------------------------------------------------------------------
{
	_init();

	m_iExpected = iExpected;
	m_iWidth    = iWidth;
	m_iFlags    = iFlags;
	m_chDone    = chDone;

	if (m_iExpected && (m_iFlags & SLIDER))
	{
		_SliderAction (KPS_START, 0, 0);
		m_bSliding = true;
	}

} // constructor

//-----------------------------------------------------------------------------
KBAR::~KBAR()
//-----------------------------------------------------------------------------
{
	Finish();
}

//-----------------------------------------------------------------------------
void KBAR::_init()
//-----------------------------------------------------------------------------
{
	m_iFlags = 0;
	m_iWidth = 0;
	m_iExpected = 0;
	m_iSoFar = 0;
	m_sStatic.clear();
	m_chDone = '%';
	m_bSliding = false;

} // _init

//-----------------------------------------------------------------------------
bool KBAR::Start (uint64_t iExpected)
//-----------------------------------------------------------------------------
{
	m_iExpected = iExpected;

	if (m_iExpected && (m_iFlags & SLIDER))
	{
		_SliderAction (KPS_START, 0, 0);
		m_bSliding = true;
	}

	return (true);

} // Start

//-----------------------------------------------------------------------------
bool KBAR::Adjust (uint64_t iExpected)
//-----------------------------------------------------------------------------
{
	if (iExpected > m_iSoFar)
	{
		m_iExpected = iExpected;
		return (true);
	}
	else
	{
		return (false);
	}

} // Adjust

//-----------------------------------------------------------------------------
bool KBAR::Move (int64_t iDelta)
//-----------------------------------------------------------------------------
{
	uint64_t iWant = m_iSoFar + iDelta;
	bool  fOK   = true;

	if (iWant > m_iExpected)
	{
		iWant = m_iExpected;
		fOK   = false;
	}

	if ((iWant < m_iSoFar) && (m_iFlags & SLIDER))
	{
		iWant = m_iSoFar; // cannot go backwards
		fOK   = false;
	}

	iDelta = iWant - m_iSoFar;

	if (m_bSliding)
	{
		if (KLog::getInstance().GetLevel())
		{
			kDebugLog (1, "kbar[{}]: {} of {}", GetBar (), m_iSoFar+iDelta, m_iExpected);
		}
		else {
			_SliderAction (KPS_ADD, m_iSoFar, m_iSoFar+iDelta);
		}
	}

	m_iSoFar += iDelta;

	return (fOK);

} // Move

//-----------------------------------------------------------------------------
KString KBAR::GetBar (int chBlank/*=' '*/)
//-----------------------------------------------------------------------------
{
	// initialize bar;
	for (uint32_t ii=0; ii<m_iWidth; ++ii)
	{
		m_sStatic[ii] = chBlank;
	}
	m_sStatic[m_iWidth] = 0;

	double nPercent = ((double)m_iSoFar / (double)m_iExpected);
	if (nPercent > 100.0)
	{
		nPercent = 100.0;
	}
	
	uint32_t iNumBars = (uint32_t) (nPercent * (double)(m_iWidth));

	// show progress:
	for (uint32_t jj=0; jj<iNumBars; ++jj)
	{
		m_sStatic[jj] = m_chDone;
	}

	return (m_sStatic);

} // GetBar

//-----------------------------------------------------------------------------
void KBAR::RepaintSlider ()
//-----------------------------------------------------------------------------
{
	if (m_iFlags & SLIDER)
	{
		_SliderAction (KPS_START, 0, 0);
		_SliderAction (KPS_ADD,   0, m_iSoFar);
	}

} // RepaintSlider

//-----------------------------------------------------------------------------
void KBAR::Finish ()
//-----------------------------------------------------------------------------
{
	if (m_bSliding)
	{
		_SliderAction (KPS_END, m_iSoFar, m_iExpected);
		m_bSliding = false;
	}

	m_iExpected = m_iSoFar;

} // Finish

//-----------------------------------------------------------------------------
void KBAR::Break (KStringView sMsg/*="!!!"*/)
//-----------------------------------------------------------------------------
{
	if (m_bSliding)
	{
		KOut.Format ("{}\n", sMsg);
		m_bSliding = false;
	}

} // Break

//-----------------------------------------------------------------------------
void KBAR::_SliderAction (int iAction, uint64_t iSoFarLast, uint64_t iSoFarNow)
//-----------------------------------------------------------------------------
{
	if (KLog::getInstance().GetLevel()) // progress bar only makes sense when NOT klogging
	{
		return;
	}

	double nPercentLast = ((double)iSoFarLast / (double)m_iExpected);
	if (nPercentLast > 100.0)
	{
		nPercentLast = 100.0;
	}

	double nPercentNow = ((double)iSoFarNow / (double)m_iExpected);
	if (nPercentNow > 100.0)
	{
		nPercentNow = 100.0;
	}

	uint32_t iNumBarsLast = (int) (nPercentLast * (double)(m_iWidth));
	uint32_t iNumBarsNow  = (int) (nPercentNow  * (double)(m_iWidth));
	uint32_t ii;

	switch (iAction)
	{
	case KPS_START:
		KOut.Format("v");
		for (ii=0; ii<m_iWidth; ++ii)
		{
			KOut.Format("_");
		}
		KOut.Format("v\n|");
		break;

	case KPS_ADD:
		for (ii=iNumBarsLast; ii<iNumBarsNow; ++ii)
		{
			KOut.Printf("%c", m_chDone);
		}
		break;

	case KPS_END:
		for (ii=iNumBarsLast; ii<m_iWidth; ++ii)
		{
			KOut.Format(" ");
		}
		KOut.Format("|\n");
	}

} // _SliderAction

} // of namespace dekaf2

