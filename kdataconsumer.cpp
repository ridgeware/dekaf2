/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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
 */

#include "kdataconsumer.h"
#include "kwrite.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void KDataConsumer::SetFinished ()
//-----------------------------------------------------------------------------
{
	m_bFinished = true;
	CallFinishedCallback();

} // SetFinished

//-----------------------------------------------------------------------------
void KViewConsumer::CallFinishedCallback()
//-----------------------------------------------------------------------------
{
	if (m_Callback)
	{
		m_Callback(*this);
	}

} // CallFinishedCallback

//-----------------------------------------------------------------------------
std::size_t KViewConsumer::Write (const void* buffer , std::size_t size)
//-----------------------------------------------------------------------------
{
	auto iWrote = m_Buffer.append(buffer, size);
	
	if (iWrote != size)
	{
		SetError("buffer too small for requested output");
	}

	return iWrote;

} // Write

//-----------------------------------------------------------------------------
void KStringConsumer::CallFinishedCallback()
//-----------------------------------------------------------------------------
{
	if (m_Callback)
	{
		m_Callback(*this);
	}

} // CallFinishedCallback

//-----------------------------------------------------------------------------
std::size_t KStringConsumer::Write (const void* buffer, std::size_t size)
//-----------------------------------------------------------------------------
{
	m_sBuffer.append(static_cast<const char*>(buffer), size);
	return size;

} // Write

//-----------------------------------------------------------------------------
void KBufferedConsumer::CallFinishedCallback()
//-----------------------------------------------------------------------------
{
	if (m_Callback)
	{
		m_Callback(*this);
	}

} // CallFinishedCallback

//-----------------------------------------------------------------------------
void KOStreamConsumer::CallFinishedCallback()
//-----------------------------------------------------------------------------
{
	if (m_Callback)
	{
		m_Callback(*this);
	}

} // CallFinishedCallback

//-----------------------------------------------------------------------------
std::size_t KOStreamConsumer::Write (const void* buffer, std::size_t size)
//-----------------------------------------------------------------------------
{
	auto iWrote = kWrite(m_OStream, buffer, size);

	if (iWrote != size)
	{
		SetError("could not write to stream");
	}

	return iWrote;

} // Write

//-----------------------------------------------------------------------------
void KCallbackConsumer::CallFinishedCallback()
//-----------------------------------------------------------------------------
{
	if (m_WriteCallback)
	{
		m_WriteCallback(this, 0);
	}

} // CallFinishedCallback

//-----------------------------------------------------------------------------
std::size_t KCallbackConsumer::Write (const void* buffer, std::size_t size)
//-----------------------------------------------------------------------------
{
	if (!size || !m_WriteCallback) return 0;

	auto iWrote = m_WriteCallback(buffer, size);

	if (iWrote != size)
	{
		SetError("could not write to stream");
	}

	return iWrote;

} // Write

DEKAF2_NAMESPACE_END
