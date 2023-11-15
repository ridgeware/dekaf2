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

#include "kstring.h"
#include "kbar.h"
#include "kwriter.h"
#include "ksystem.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KBAR::KBAR (uint64_t iExpected/*=0*/, uint32_t iWidth/*=DEFAULT_WIDTH*/, uint64_t iFlags/*=SLIDER*/, int chDone/*='%'*/, KOutStream& Out/*=KOut*/)
//-----------------------------------------------------------------------------
: m_iFlags(iFlags)
, m_iWidth(iWidth)
, m_iExpected(iExpected)
, m_iSoFar(0)
, m_chDone(chDone)
, m_Out(Out)
, m_bSliding(false)
{
	if (!m_iWidth)
	{
		m_iWidth = kGetTerminalSize().columns;

		if (m_iWidth > 2)
		{
			m_iWidth -= 2;
		}
		else
		{
			kDebug(2, "terminal width too small for bar output");
		}
	}

	if (m_iExpected && (m_iFlags & SLIDER))
	{
		_SliderAction (KPS_START, 0, 0);
		m_bSliding = true;
	}

} // constructor

//-----------------------------------------------------------------------------
void KBAR::SetFlags (uint64_t iFlags)
//-----------------------------------------------------------------------------
{
	m_iFlags = iFlags;
	if (m_iFlags & SLIDER)
	{
		_SliderAction (KPS_START, 0, 0);
		m_bSliding = true;
	}
	else
	{
		m_bSliding = false;
	}

} // SetFlags

//-----------------------------------------------------------------------------
KBAR::~KBAR()
//-----------------------------------------------------------------------------
{
	Finish();
}

//-----------------------------------------------------------------------------
bool KBAR::Start (uint64_t iExpected)
//-----------------------------------------------------------------------------
{
	m_iExpected = iExpected;

	if (m_iExpected && (m_iFlags & SLIDER))
	{
		m_iSoFar = 0;
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
		if (kWouldLog(1))
		{
			kDebugLog (1, "kbar: {:3}%, {} of {}", ((m_iSoFar+iDelta))*100/m_iExpected, m_iSoFar+iDelta, m_iExpected);
		}
		else
		{
			_SliderAction (KPS_ADD, m_iSoFar, m_iSoFar+iDelta);
		}
	}

	m_iSoFar += iDelta;

	return (fOK);

} // Move

//-----------------------------------------------------------------------------
KString KBAR::GetBar (uint16_t iStaticWidth/*=30*/, int chBlank/*=' '*/)
//-----------------------------------------------------------------------------
{
	KString  sBar;

	if (!m_iExpected)
	{
		return sBar;
	}

	double nPercentNow = ((double)m_iSoFar / (double)m_iExpected);

	if (nPercentNow > 100.0)
	{
		nPercentNow = 100.0;
	}

	uint32_t iNumBarsNow  = (int) (nPercentNow  * (double)(iStaticWidth));

	kDebug (1, "{} out of {}, {}%, {} out of {} bars", m_iSoFar, m_iExpected, nPercentNow, iNumBarsNow, iStaticWidth);

	for (uint32_t ii=1; ii<=iStaticWidth; ++ii)
	{
		if (ii <= iNumBarsNow)
		{
			sBar += m_chDone;
		}
		else
		{
			sBar += chBlank;
		}
	}

	return (sBar);

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

	m_iExpected = 0;

} // Finish

//-----------------------------------------------------------------------------
void KBAR::Break (KStringView sMsg/*="!!!"*/)
//-----------------------------------------------------------------------------
{
	if (m_bSliding)
	{
		m_Out.WriteLine (sMsg);
		m_bSliding = false;
	}

} // Break

//-----------------------------------------------------------------------------
void KBAR::_SliderAction (int iAction, uint64_t iSoFarLast, uint64_t iSoFarNow)
//-----------------------------------------------------------------------------
{
	if (kWouldLog(1)) // progress bar only makes sense when NOT klogging
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
		m_Out.Write('v');
		for (ii=0; ii<m_iWidth; ++ii)
		{
			m_Out.Write('_');
		}
		m_Out.Write("v\n|");
		break;

	case KPS_ADD:
		for (ii=iNumBarsLast; ii<iNumBarsNow; ++ii)
		{
			m_Out.Write(m_chDone);
		}
		break;

	case KPS_END:
		for (ii=iNumBarsLast; ii<m_iWidth; ++ii)
		{
			m_Out.Write(' ');
		}
		m_Out.Write("|\n");
	}

	m_Out.Flush();

} // _SliderAction

// ************************ KSharedBar **************************

//-----------------------------------------------------------------------------
bool KSharedBar::Start  (uint64_t iExpected)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return KBAR::Start(iExpected);
}

//-----------------------------------------------------------------------------
bool KSharedBar::Adjust (uint64_t iExpected)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return KBAR::Adjust(iExpected);
}

//-----------------------------------------------------------------------------
bool KSharedBar::Move   (int64_t iDelta)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return KBAR::Move(iDelta);
}

//-----------------------------------------------------------------------------
void KSharedBar::Break  (KStringView sMsg)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	KBAR::Break(sMsg);
}

//-----------------------------------------------------------------------------
void KSharedBar::Finish ()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	KBAR::Finish();
}

//-----------------------------------------------------------------------------
KString KSharedBar::GetBar (uint16_t iStaticWidth, int chBlank)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return KBAR::GetBar(iStaticWidth,chBlank);
}

//-----------------------------------------------------------------------------
uint64_t KSharedBar::GetSoFar() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return KBAR::GetSoFar();
}

//-----------------------------------------------------------------------------
void KSharedBar::RepaintSlider ()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	KBAR::RepaintSlider();
}

} // of namespace dekaf2
