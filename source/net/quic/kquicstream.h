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


#pragma once

/// @file kquicstream.h
/// provides an implementation of std::iostreams supporting QUIC

#include "kconfiguration.h"

#if DEKAF2_HAS_NGTCP2

#include <dekaf2/net/quic/kquicconnection.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/io/streams/kstreambuf.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/net/util/kiostreamsocket.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_quic
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream QUIC implementation with timeout. Wraps a KQuicConnection
/// (ngtcp2) and exposes one bidirectional stream as an iostream. HTTP/3
/// (khttp3::Session) takes the connection over and drives the streams itself.
class DEKAF2_PUBLIC KQuicStream : public KIOStreamSocket
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KIOStreamSocket;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected client stream with the default client context
	KQuicStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected stream
	/// @param Context
	/// A KTLSContext for Transport::Quic (client role)
	/// @param Timeout
	/// Timeout for any I/O. Defaults to 15 seconds.
	KQuicStream(KTLSContext& Context, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected client stream
	/// @param Context
	/// A KTLSContext for Transport::Quic (client role)
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, HTTP/3 request, and the timeout
	KQuicStream(KTLSContext& Context, const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected client stream, using the default client context
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, HTTP/3 request, and the timeout
	KQuicStream(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	KQuicStream(const KQuicStream&) = delete;
	KQuicStream& operator=(const KQuicStream&) = delete;
	KQuicStream(KQuicStream&&) = delete;
	KQuicStream& operator=(KQuicStream&&) = delete;

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client, and completes the QUIC handshake.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, HTTP/3 request, and the timeout
	virtual bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{}) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect the stream
	virtual bool Disconnect() override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual bool is_open() const override final
	//-----------------------------------------------------------------------------
	{
		return m_Connection.GetNativeSocket() >= 0;
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection
	virtual bool IsDisconnected() override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// QUIC handshakes in Connect() - this only reports the state
	virtual bool StartManualTLSHandshake() override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Name the host to talk to when it differs from the host connected to, e.g. when
	/// connecting to an IP address. Used for SNI and the certificate check. Only possible
	/// before Connect(), which handshakes
	virtual bool SetTLSHostname(KStringView sHostname) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// The name set with SetTLSHostname(), empty if the connected host is used
	virtual KStringView GetTLSHostname() const override final { return m_sTLSHostname; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the underlying OS level native socket of the stream
	virtual native_socket_type GetNativeSocket() override final
	//-----------------------------------------------------------------------------
	{
		return m_Connection.GetNativeSocket();
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying openssl handle of the stream
	virtual native_tls_handle_type GetNativeTLSHandle() override final
	//-----------------------------------------------------------------------------
	{
		return m_Connection.GetNativeTLSHandle();
	}

	//-----------------------------------------------------------------------------
	/// Gets the KTLSContext used in construction
	const KTLSContext& GetContext() const
	//-----------------------------------------------------------------------------
	{
		return m_TLSContext;
	}

	//-----------------------------------------------------------------------------
	/// Gets the QUIC connection
	KQuicConnection& GetConnection()
	//-----------------------------------------------------------------------------
	{
		return m_Connection;
	}

	//-----------------------------------------------------------------------------
	std::streamsize direct_read_some(void* sBuffer, std::streamsize iCount) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::streamsize direct_write_some(const void* sBuffer, std::streamsize iCount) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a reference to self
	KQuicStream& GetKQuicStream()
	//-----------------------------------------------------------------------------
	{
		return *this;
	}

	//-----------------------------------------------------------------------------
	virtual bool Good() const override final
	//-----------------------------------------------------------------------------
	{
		return !HasError();
	}

	//-----------------------------------------------------------------------------
	/// request to switch to HTTP3 - only effective before Connect()
	/// @returns true if protocol request is permitted
	bool SetRequestHTTP3();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets the congestion control algorithm - only effective before Connect()
	void SetCongestionControl(KQuicConnection::CongestionControl cc)
	//-----------------------------------------------------------------------------
	{
		m_Connection.SetCongestionControl(cc);
	}

//----------
private:
//----------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the delegate for raw stream I/O through the iostream interface: one
	/// bidirectional stream, a send buffer that is kept until acknowledged,
	/// and a receive buffer
	class DEKAF2_PRIVATE RawDelegate : public KQuicConnection::Delegate
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:
		RawDelegate(KQuicStream& Stream) : m_Stream(Stream) {}

		virtual std::ptrdiff_t OnQuicStreamData     (KQuicConnection::StreamID id, KStringView sData, bool bFin) override;
		virtual int            OnQuicAckedStreamData(KQuicConnection::StreamID id, uint64_t iOffset, uint64_t iLen) override;
		virtual int            OnQuicStreamClose    (KQuicConnection::StreamID id, bool bHasAppErrorCode, uint64_t iAppErrorCode) override;
		virtual int            OnQuicStreamReset    (KQuicConnection::StreamID id, uint64_t iFinalSize, uint64_t iAppErrorCode) override;
		virtual int            OnQuicStreamUnblocked(KQuicConnection::StreamID id) override;
		virtual std::ptrdiff_t GetQuicStreamData    (KQuicConnection::StreamID& id, bool& bFin, ngtcp2_vec* vecs, std::size_t iMaxVecs) override;
		virtual int            OnQuicWriteOffset    (KQuicConnection::StreamID id, std::size_t iWritten) override;
		virtual void           OnQuicStreamBlocked  (KQuicConnection::StreamID id) override;
		virtual void           OnQuicStreamShutWrite(KQuicConnection::StreamID id) override;

		bool          OpenStream();
		void          Reset();
		std::size_t   Pending() const { return m_iTXBase + m_sTX.size() - m_iWritten; }

		KQuicStream&              m_Stream;
		KString                   m_sTX;                  ///< unacknowledged and unsent data
		KString                   m_sRX;                  ///< received, not yet read data
		uint64_t                  m_iTXBase    { 0 };     ///< stream offset of m_sTX[0]
		uint64_t                  m_iWritten   { 0 };     ///< stream offset of the next unsent byte
		KQuicConnection::StreamID m_StreamID   { -1 };
		bool                      m_bBlocked   { false };
		bool                      m_bEOF       { false };
		bool                      m_bShutWrite { false };

	}; // RawDelegate

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	DEKAF2_PRIVATE
	static std::streamsize QuicStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	DEKAF2_PRIVATE
	static std::streamsize QuicStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	KTLSContext&           m_TLSContext;
	KQuicConnection        m_Connection;
	RawDelegate            m_Raw;
	KString                m_sTLSHostname;
	KString                m_sALPN;

	KBufferedStreamBuf     m_QuicStreamBuf { &QuicStreamReader, &QuicStreamWriter, this, this };

}; // KQuicStream


// there is nothing special with a quic client
using KQuicClient = KQuicStream;

//-----------------------------------------------------------------------------
/// QUIC servers are not supported yet - this returns an unconnected client stream
DEKAF2_PUBLIC
std::unique_ptr<KQuicStream> CreateKQuicServer(KTLSContext& Context, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KQuicClient> CreateKQuicClient();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KQuicClient> CreateKQuicClient(const KTCPEndPoint& EndPoint, KStreamOptions Options);
//-----------------------------------------------------------------------------


/// @}

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGTCP2
