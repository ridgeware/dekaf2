/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
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
*/


#include <dekaf2/net/quic/kquicstream.h>

#if DEKAF2_HAS_NGTCP2

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <ngtcp2/ngtcp2.h>
#include <algorithm>
#include <cstring>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
/// the default client context, constructed on first use - as a file-scope
/// static it would build an SSL context (and initialize OpenSSL) before
/// main() in every program, whether it uses QUIC or not
static KTLSContext& KQuicClientContext()
//-----------------------------------------------------------------------------
{
	static KTLSContext s_KQuicClientContext { false, KTLSContext::Transport::Quic };
	return s_KQuicClientContext;
}

// ------------------------------------------------------------------------
// RawDelegate
// ------------------------------------------------------------------------

//-----------------------------------------------------------------------------
void KQuicStream::RawDelegate::Reset()
//-----------------------------------------------------------------------------
{
	m_sTX.clear();
	m_sRX.clear();
	m_iTXBase    = 0;
	m_iWritten   = 0;
	m_StreamID   = -1;
	m_bBlocked   = false;
	m_bEOF       = false;
	m_bShutWrite = false;

} // Reset

//-----------------------------------------------------------------------------
bool KQuicStream::RawDelegate::OpenStream()
//-----------------------------------------------------------------------------
{
	if (m_StreamID >= 0)
	{
		return true;
	}

	if (!m_Stream.GetConnection().OpenBidiStream(m_StreamID))
	{
		return m_Stream.SetError(m_Stream.GetConnection().CopyLastError());
	}

	return true;

} // OpenStream

//-----------------------------------------------------------------------------
std::ptrdiff_t KQuicStream::RawDelegate::OnQuicStreamData(KQuicConnection::StreamID id, KStringView sData, bool bFin)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		m_sRX += sData;

		if (bFin)
		{
			m_bEOF = true;
		}
	}

	// data of other streams is dropped (and credited)
	return static_cast<std::ptrdiff_t>(sData.size());

} // OnQuicStreamData

//-----------------------------------------------------------------------------
int KQuicStream::RawDelegate::OnQuicAckedStreamData(KQuicConnection::StreamID id, uint64_t iOffset, uint64_t iLen)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		// ngtcp2 reports acknowledged data in order
		auto iAckedEnd = iOffset + iLen;

		if (iAckedEnd > m_iTXBase)
		{
			auto iDrop = std::min(static_cast<std::size_t>(iAckedEnd - m_iTXBase), m_sTX.size());
			m_sTX.erase(0, iDrop);
			m_iTXBase += iDrop;
		}
	}

	return 0;

} // OnQuicAckedStreamData

//-----------------------------------------------------------------------------
int KQuicStream::RawDelegate::OnQuicStreamClose(KQuicConnection::StreamID id, bool bHasAppErrorCode, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		m_bEOF       = true;
		m_bShutWrite = true;
	}

	return 0;

} // OnQuicStreamClose

//-----------------------------------------------------------------------------
int KQuicStream::RawDelegate::OnQuicStreamReset(KQuicConnection::StreamID id, uint64_t iFinalSize, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		m_bEOF = true;
	}

	return 0;

} // OnQuicStreamReset

//-----------------------------------------------------------------------------
int KQuicStream::RawDelegate::OnQuicStreamUnblocked(KQuicConnection::StreamID id)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		m_bBlocked = false;
	}

	return 0;

} // OnQuicStreamUnblocked

//-----------------------------------------------------------------------------
std::ptrdiff_t KQuicStream::RawDelegate::GetQuicStreamData(KQuicConnection::StreamID& id, bool& bFin, ngtcp2_vec* vecs, std::size_t iMaxVecs)
//-----------------------------------------------------------------------------
{
	id   = -1;
	bFin = false;

	if (m_StreamID < 0 || m_bBlocked || m_bShutWrite || !iMaxVecs)
	{
		return 0;
	}

	auto iPending = Pending();

	if (!iPending)
	{
		return 0;
	}

	id           = m_StreamID;
	vecs[0].base = reinterpret_cast<uint8_t*>(m_sTX.data() + (m_iWritten - m_iTXBase));
	vecs[0].len  = iPending;

	return 1;

} // GetQuicStreamData

//-----------------------------------------------------------------------------
int KQuicStream::RawDelegate::OnQuicWriteOffset(KQuicConnection::StreamID id, std::size_t iWritten)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		m_iWritten += iWritten;
	}

	return 0;

} // OnQuicWriteOffset

//-----------------------------------------------------------------------------
void KQuicStream::RawDelegate::OnQuicStreamBlocked(KQuicConnection::StreamID id)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		m_bBlocked = true;
	}

} // OnQuicStreamBlocked

//-----------------------------------------------------------------------------
void KQuicStream::RawDelegate::OnQuicStreamShutWrite(KQuicConnection::StreamID id)
//-----------------------------------------------------------------------------
{
	if (id == m_StreamID)
	{
		m_bShutWrite = true;
	}

} // OnQuicStreamShutWrite

// ------------------------------------------------------------------------
// KQuicStream
// ------------------------------------------------------------------------

//-----------------------------------------------------------------------------
bool KQuicStream::IsDisconnected()
//-----------------------------------------------------------------------------
{
	return !is_open() || m_Connection.IsClosed();

} // IsDisconnected

//-----------------------------------------------------------------------------
bool KQuicStream::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	// QUIC handshakes in Connect() - we just return if the stream is good (no errors)
	return Good() && m_Connection.IsConnected();
}

//-----------------------------------------------------------------------------
bool KQuicStream::SetTLSHostname(KStringView sHostname)
//-----------------------------------------------------------------------------
{
	if (m_Connection.IsConnected())
	{
		kDebug(2, "TLS handshake already done, cannot set hostname: {}", sHostname);
		return false;
	}

	m_sTLSHostname = sHostname;

	return true;

} // SetTLSHostname

//-----------------------------------------------------------------------------
bool KQuicStream::SetRequestHTTP3()
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_NGHTTP3
	// allow ALPN negotiation for HTTP/3 if this is a client
	if (GetContext().GetRole() == boost::asio::ssl::stream_base::client)
	{
		if (m_Connection.IsConnected())
		{
			kDebug(1, "cannot request HTTP/3 after the handshake");
			return false;
		}

		m_sALPN = KStringView("\x02h3", 3);
		kDebug(2, "requesting ALPN h3");
		return true;
	}
	else
	{
		kDebug(1, "HTTP/3 is only supported in client mode");
	}
#else  // of DEKAF2_HAS_NGHTTP3
	kDebug(2, "HTTP/3 is not supported by this build");
#endif // of DEKAF2_HAS_NGHTTP3

	return false;

} // SetRequestHTTP3

//-----------------------------------------------------------------------------
std::streamsize KQuicStream::direct_read_some(void* sBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	if (iCount <= 0 || m_Connection.GetDelegate() != &m_Raw)
	{
		return 0;
	}

	auto tStart = chrono::steady_clock::now();

	for (;;)
	{
		if (!m_Raw.m_sRX.empty())
		{
			auto iCopy = std::min(static_cast<std::size_t>(iCount), m_Raw.m_sRX.size());
			std::memcpy(sBuffer, m_Raw.m_sRX.data(), iCopy);
			m_Raw.m_sRX.erase(0, iCopy);
			return static_cast<std::streamsize>(iCopy);
		}

		if (m_Raw.m_bEOF || m_Connection.IsClosed())
		{
			return 0;
		}

		KDuration Remaining = GetTimeout() - KDuration(chrono::steady_clock::now() - tStart);

		if (Remaining <= KDuration::zero())
		{
			SetError("read timed out");
			return 0;
		}

		if (m_Connection.Pump(Remaining) == KQuicConnection::PumpResult::Error)
		{
			SetError(m_Connection.CopyLastError());
			return 0;
		}
	}

} // direct_read_some

//-----------------------------------------------------------------------------
std::streamsize KQuicStream::direct_write_some(const void* sBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	if (iCount <= 0 || m_Connection.GetDelegate() != &m_Raw)
	{
		return 0;
	}

	if (!m_Raw.OpenStream())
	{
		return 0;
	}

	m_Raw.m_sTX.append(static_cast<const char*>(sBuffer), static_cast<std::size_t>(iCount));

	// keep the unacknowledged data bounded - pump until the send buffer drained
	// below a limit, which also gives ngtcp2 the chance to send
	constexpr std::size_t iMaxBuffered = 4 * 1024 * 1024;

	auto tStart = chrono::steady_clock::now();

	for (;;)
	{
		if (m_Raw.m_sTX.size() < iMaxBuffered)
		{
			if (!m_Connection.Flush())
			{
				SetError(m_Connection.CopyLastError());
				return 0;
			}
			break;
		}

		KDuration Remaining = GetTimeout() - KDuration(chrono::steady_clock::now() - tStart);

		if (Remaining <= KDuration::zero())
		{
			SetError("write timed out");
			return 0;
		}

		if (m_Connection.Pump(Remaining) == KQuicConnection::PumpResult::Error)
		{
			SetError(m_Connection.CopyLastError());
			return 0;
		}
	}

	return iCount;

} // direct_write_some

//-----------------------------------------------------------------------------
std::streamsize KQuicStream::QuicStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead { 0 };

	if (stream_)
	{
		auto& Quic = *static_cast<KQuicStream*>(stream_);

		iRead = Quic.direct_read_some(sBuffer, iCount);
	}

	return iRead;

} // QuicStreamReader

//-----------------------------------------------------------------------------
std::streamsize KQuicStream::QuicStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote { 0 };

	if (stream_)
	{
		auto& Quic = *static_cast<KQuicStream*>(stream_);

		iWrote = Quic.direct_write_some(sBuffer, iCount);
	}

	return iWrote;

} // QuicStreamWriter

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream()
//-----------------------------------------------------------------------------
: KQuicStream(KQuicClientContext(), KStreamOptions::GetDefaultTimeout())
{
}

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream(KTLSContext& Context, KDuration Timeout)
//-----------------------------------------------------------------------------
: base_type(&m_QuicStreamBuf, Timeout)
, m_TLSContext(Context)
, m_Connection(Context)
, m_Raw(*this)
{
	m_Connection.SetTimeout(Timeout);
	m_Connection.SetDelegate(&m_Raw);
}

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream(KTLSContext& Context,
                         const KTCPEndPoint& Endpoint,
                         KStreamOptions Options)
//-----------------------------------------------------------------------------
: KQuicStream(Context, Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
KQuicStream::KQuicStream(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
: KQuicStream(KQuicClientContext(), Options.GetTimeout())
{
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
bool KQuicStream::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	// allow re-use of a previously disconnected stream
	ResetDisconnectingState();
	m_Raw.Reset();

	if (m_Connection.GetDelegate() == nullptr)
	{
		m_Connection.SetDelegate(&m_Raw);
	}

	SetTimeout(Options.GetTimeout());
	SetUnresolvedEndPoint(Endpoint);

	if (GetContext().GetRole() != boost::asio::ssl::stream_base::client)
	{
		return SetError("QUIC is only supported in client mode");
	}

	if (Options.IsSet(KStreamOptions::RequestHTTP3))
	{
		SetRequestHTTP3();
	}

	if (!m_Connection.Connect(Endpoint, Options, m_sTLSHostname, m_sALPN))
	{
		return SetError(m_Connection.CopyLastError());
	}

	SetEndPointAddress(m_Connection.GetEndPointAddress());

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KQuicStream::Disconnect()
//-----------------------------------------------------------------------------
{
	// Signal disconnecting first - this sets an atomic flag AND wakes any
	// pending poll() calls in other threads.
	SignalDisconnecting();

	m_Connection.Close(0);
	m_Raw.Reset();

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
std::unique_ptr<KQuicStream> CreateKQuicServer(KTLSContext& Context, KDuration Timeout)
//-----------------------------------------------------------------------------
{
	kDebug(1, "QUIC servers are not supported");
	return std::make_unique<KQuicStream>(Context, Timeout);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KQuicClient> CreateKQuicClient()
//-----------------------------------------------------------------------------
{
	return std::make_unique<KQuicClient>(KQuicClientContext(), KStreamOptions::GetDefaultTimeout());
}

//-----------------------------------------------------------------------------
std::unique_ptr<KQuicClient> CreateKQuicClient(const KTCPEndPoint& EndPoint,
                                               KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	auto Client = CreateKQuicClient();

	Client->Connect(EndPoint, Options);

	return Client;

} // CreateKQuicClient

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGTCP2
