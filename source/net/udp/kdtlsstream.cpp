/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

#include <dekaf2/net/udp/kdtlsstream.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KDTLSStream::KDTLSStream(KDuration Timeout, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
: KReaderWriter<std::iostream>(&m_StreamBuf)
, m_Socket(Timeout)
, m_StreamBuf(m_Socket, iMaxDatagramSize)
{
}

//-----------------------------------------------------------------------------
KDTLSStream::KDTLSStream(KTLSContext& Context, KDuration Timeout, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
: KReaderWriter<std::iostream>(&m_StreamBuf)
, m_Socket(Context, Timeout)
, m_StreamBuf(m_Socket, iMaxDatagramSize)
{
}

//-----------------------------------------------------------------------------
KDTLSStream::KDTLSStream(const KTCPEndPoint& Endpoint, KStreamOptions Options, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
: KReaderWriter<std::iostream>(&m_StreamBuf)
, m_Socket(Endpoint, Options)
, m_StreamBuf(m_Socket, iMaxDatagramSize)
{
	if (!m_Socket.Good())
	{
		SetError(m_Socket.Error());
	}
}

//-----------------------------------------------------------------------------
KDTLSStream::KDTLSStream(KTLSContext& Context, const KTCPEndPoint& Endpoint, KStreamOptions Options, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
: KReaderWriter<std::iostream>(&m_StreamBuf)
, m_Socket(Context, Endpoint, Options)
, m_StreamBuf(m_Socket, iMaxDatagramSize)
{
	if (!m_Socket.Good())
	{
		SetError(m_Socket.Error());
	}
}

//-----------------------------------------------------------------------------
KDTLSStream::~KDTLSStream()
//-----------------------------------------------------------------------------
{
	// flush remaining data
	flush();
}

//-----------------------------------------------------------------------------
bool KDTLSStream::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	if (!m_Socket.Connect(Endpoint, Options))
	{
		return SetError(m_Socket.Error());
	}

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KDTLSStream::Disconnect()
//-----------------------------------------------------------------------------
{
	flush();
	return m_Socket.Disconnect();

} // Disconnect

//-----------------------------------------------------------------------------
void KDTLSStream::SetMaxDatagramSize(std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
{
	m_StreamBuf.SetMaxDatagramSize(iMaxDatagramSize);

} // SetMaxDatagramSize

//-----------------------------------------------------------------------------
std::unique_ptr<KDTLSStream> CreateKDTLSStream(KDuration Timeout, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KDTLSStream>(Timeout, iMaxDatagramSize);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KDTLSStream> CreateKDTLSStream(const KTCPEndPoint& EndPoint, KStreamOptions Options, std::size_t iMaxDatagramSize)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KDTLSStream>(EndPoint, Options, iMaxDatagramSize);
}

DEKAF2_NAMESPACE_END
