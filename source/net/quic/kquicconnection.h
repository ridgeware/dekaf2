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


#pragma once

/// @file kquicconnection.h
/// a QUIC connection on top of ngtcp2, with OpenSSL (>= 3.5) as the TLS backend

#include "kconfiguration.h"

#if DEKAF2_HAS_NGTCP2

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/web/url/kurl.h>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <utility>

// forward declarations into the ngtcp2 and OpenSSL types - the headers
// are only included by the implementation
struct ngtcp2_conn;
struct ngtcp2_vec;
struct ngtcp2_crypto_ossl_ctx;
struct ssl_st;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_quic
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A QUIC client connection. Owns the UDP socket, the ngtcp2 connection state
/// and the TLS session, and runs the event loop: Pump() sends what is due,
/// waits for incoming datagrams or the next ngtcp2 timer, whichever comes
/// first, and feeds received packets into ngtcp2. Stream data flows through
/// a Delegate - the HTTP/3 layer (khttp3::Session) implements it on top of
/// nghttp3, KQuicStream implements it for raw stream I/O.
///
/// ngtcp2 is a state machine, not a socket: it never touches the network
/// itself. This class is the only place that does, which keeps the timing
/// in one loop and makes the congestion control algorithm a setting.
class DEKAF2_PUBLIC KQuicConnection : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the QUIC stream ID type
	using StreamID = int64_t;

	/// the congestion control algorithm for the sending side
	enum class CongestionControl : uint8_t
	{
		Cubic, ///< loss based, the ngtcp2 default - halves the window on every loss
		BBR,   ///< model based (BBRv2) - ignores isolated losses, fills lossy links like satellite uplinks
		Reno   ///< loss based, the RFC 9002 reference
	};

	/// result of one Pump() cycle
	enum class PumpResult : uint8_t
	{
		Error,    ///< the connection is broken or closed, see GetLastError()
		Idle,     ///< nothing was received within the wait time (timers may have run)
		Received  ///< at least one datagram was received
	};

	/// connection statistics, mostly from ngtcp2_conn_info
	struct Info
	{
		KDuration LatestRTT;
		KDuration MinRTT;
		KDuration SmoothedRTT;
		uint64_t  iCongestionWindow { 0 };
		uint64_t  iBytesInFlight    { 0 };
		uint64_t  iPacketsSent      { 0 };
		uint64_t  iBytesSent        { 0 };
		uint64_t  iPacketsReceived  { 0 };
		uint64_t  iBytesReceived    { 0 };
		uint64_t  iPacketsLost      { 0 };
		uint64_t  iBytesLost        { 0 };
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// The interface through which stream data enters and leaves the connection.
	/// All methods are called from inside Pump(). Return values of 0 mean
	/// success, negative values abort the connection with the error message
	/// set through SetDelegateError().
	class DEKAF2_PUBLIC Delegate
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
	public:
		virtual ~Delegate() = default;

		/// stream data arrived. @return the number of bytes the delegate has consumed (they are
		/// credited back to the peer's flow control window right away), or < 0 on error. Bytes not
		/// consumed now must be credited later through KQuicConnection::CreditReceived().
		virtual std::ptrdiff_t OnQuicStreamData     (StreamID id, KStringView sData, bool bFin) = 0;
		/// the peer acknowledged iLen bytes of stream data starting at iOffset - the delegate may free them
		virtual int            OnQuicAckedStreamData(StreamID id, uint64_t iOffset, uint64_t iLen) { return 0; }
		/// the peer opened a stream
		virtual int            OnQuicStreamOpen     (StreamID id) { return 0; }
		/// a stream is closed in both directions
		virtual int            OnQuicStreamClose    (StreamID id, bool bHasAppErrorCode, uint64_t iAppErrorCode) { return 0; }
		/// the peer reset the stream (its sending side)
		virtual int            OnQuicStreamReset    (StreamID id, uint64_t iFinalSize, uint64_t iAppErrorCode) { return 0; }
		/// the peer asks us to stop sending on the stream
		virtual int            OnQuicStopSending    (StreamID id, uint64_t iAppErrorCode) { return 0; }
		/// the peer raised the flow control limit of a stream that was blocked
		virtual int            OnQuicStreamUnblocked(StreamID id) { return 0; }
		/// the peer allows more streams to be opened
		virtual void           OnQuicCanOpenStreams (bool bBidirectional, uint64_t iMaxStreams) {}

		/// supply outgoing stream data: fill up to iMaxVecs vectors, set id (-1 if nothing to send) and
		/// bFin (the data ends the stream). @return the number of vectors filled, or < 0 on error. The
		/// data has to stay valid until OnQuicAckedStreamData() covers it (ngtcp2 does not copy).
		virtual std::ptrdiff_t GetQuicStreamData    (StreamID& id, bool& bFin, ngtcp2_vec* vecs, std::size_t iMaxVecs) { id = -1; bFin = false; return 0; }
		/// iWritten bytes of the data offered by GetQuicStreamData() were accepted for sending
		virtual int            OnQuicWriteOffset    (StreamID id, std::size_t iWritten) { return 0; }
		/// the stream is flow control blocked - do not offer its data again until OnQuicStreamUnblocked()
		virtual void           OnQuicStreamBlocked  (StreamID id) {}
		/// the stream's sending side is already shut down - do not offer its data again
		virtual void           OnQuicStreamShutWrite(StreamID id) {}

	}; // Delegate

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected client connection
	/// @param Context a KTLSContext for Transport::Quic
	KQuicConnection(KTLSContext& Context);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KQuicConnection();
	//-----------------------------------------------------------------------------

	KQuicConnection(const KQuicConnection&) = delete;
	KQuicConnection& operator=(const KQuicConnection&) = delete;
	KQuicConnection(KQuicConnection&&) = delete;
	KQuicConnection& operator=(KQuicConnection&&) = delete;

	//-----------------------------------------------------------------------------
	/// Connects to Endpoint and completes the QUIC handshake. Returns false on error.
	/// @param Endpoint the server to connect to
	/// @param Options certificate verification, address family, timeout
	/// @param sTLSHostname the name to verify and to send as SNI if it differs from the endpoint's domain
	/// @param sALPN the ALPN protocol list in wire format (length prefixed), e.g. "\x02h3"
	bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options, KStringView sTLSHostname = KStringView{}, KStringView sALPN = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Closes the connection, sending CONNECTION_CLOSE with an application error code if the peer
	/// is still reachable, and releases all resources. Safe to call repeatedly.
	void Close(uint64_t iAppErrorCode = 0);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// One event loop cycle: send due packets, wait up to MaxWait for input or the next
	/// ngtcp2 timer, receive, run timers, send again.
	PumpResult Pump(KDuration MaxWait);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Sends whatever is due right now, without waiting for input. Returns false on error.
	bool Flush();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// true after a completed handshake, until the connection is closed
	bool IsConnected() const { return m_Conn != nullptr && m_bHandshakeCompleted && !m_bClosed; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// true if the connection was closed, by either side
	bool IsClosed() const { return m_bClosed; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets the delegate that receives and supplies stream data - may be nullptr
	void SetDelegate(Delegate* delegate) { m_Delegate = delegate; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns the delegate, may be nullptr
	Delegate* GetDelegate() const { return m_Delegate; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// opens a bidirectional stream, returns false if the peer's stream limit is exhausted
	bool OpenBidiStream(StreamID& id);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// opens a unidirectional (sending) stream, returns false if the peer's stream limit is exhausted
	bool OpenUniStream(StreamID& id);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// tells the peer to stop sending on the stream (STOP_SENDING)
	bool ShutdownStreamRead(StreamID id, uint64_t iAppErrorCode);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// abandons our sending side of the stream (RESET_STREAM)
	bool ShutdownStreamWrite(StreamID id, uint64_t iAppErrorCode);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// credits iBytes of previously received but not yet consumed stream data back to the
	/// peer's flow control windows (stream and connection level)
	void CreditReceived(StreamID id, std::size_t iBytes);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// number of unidirectional streams we may still open
	uint64_t GetStreamsUniLeft() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// number of bidirectional streams we may still open
	uint64_t GetStreamsBidiLeft() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the UDP socket, -1 if not connected
	int GetNativeSocket() const { return m_Socket; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the OpenSSL session object, nullptr if not connected
	::ssl_st* GetNativeTLSHandle() const { return m_SSL.get(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the ngtcp2 connection object, nullptr if not connected
	::ngtcp2_conn* GetNativeHandle() const { return m_Conn; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the negotiated ALPN protocol, empty if none
	KStringView GetALPN() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the connected peer address
	const KTCPEndPoint& GetEndPointAddress() const { return m_EndPointAddress; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// connection statistics
	Info GetInfo() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets the congestion control algorithm - only effective before Connect()
	void SetCongestionControl(CongestionControl cc) { m_CongestionControl = cc; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	CongestionControl GetCongestionControl() const { return m_CongestionControl; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// the I/O timeout used by Connect() and for blocked sends
	KDuration GetTimeout() const { return m_Timeout; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetTimeout(KDuration Timeout) { m_Timeout = Timeout; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// stores an error message from inside a delegate callback - the connection fails with it
	void SetDelegateError(KString sError) { m_sDelegateError = std::move(sError); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets the congestion control algorithm for all connections created afterwards
	/// (process wide) - the default is CUBIC
	static void SetDefaultCongestionControl(CongestionControl cc) { s_DefaultCongestionControl = cc; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static CongestionControl GetDefaultCongestionControl() { return s_DefaultCongestionControl; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static KStringView ToString(CongestionControl cc);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// parses "cubic", "bbr", "reno" (case insensitive), returns false if unknown
	static bool FromString(KStringView sName, CongestionControl& cc);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	struct Impl;

	// the ngtcp2 callbacks are free functions in the implementation file
	friend struct KQuicConnectionCallbacks;

	DEKAF2_PRIVATE bool SetupSocket  (const KTCPEndPoint& Endpoint, const KStreamOptions& Options);
	DEKAF2_PRIVATE bool SetupTLS     (KStringView sHostname, KStringView sALPN, bool bVerifyCert);
	DEKAF2_PRIVATE bool SetupQuic    ();
	DEKAF2_PRIVATE bool Handshake    ();
	DEKAF2_PRIVATE bool Read         ();
	DEKAF2_PRIVATE bool Write        ();
	DEKAF2_PRIVATE bool Send         (const uint8_t* data, std::size_t iSize);
	DEKAF2_PRIVATE bool Fail         (int iError);
	DEKAF2_PRIVATE void Teardown     ();
	DEKAF2_PRIVATE void ApplyPendingCredits();
	DEKAF2_PRIVATE int  DelegateError(int iResult);

	// called from the ngtcp2 callbacks
	DEKAF2_PRIVATE int  OnHandshakeCompleted   ();
	DEKAF2_PRIVATE int  OnRecvStreamData       (StreamID id, const uint8_t* data, std::size_t iSize, bool bFin);
	DEKAF2_PRIVATE int  OnAckedStreamDataOffset(StreamID id, uint64_t iOffset, uint64_t iLen);
	DEKAF2_PRIVATE int  OnStreamOpen           (StreamID id);
	DEKAF2_PRIVATE int  OnStreamClose          (StreamID id, bool bHasAppErrorCode, uint64_t iAppErrorCode);
	DEKAF2_PRIVATE int  OnStreamReset          (StreamID id, uint64_t iFinalSize, uint64_t iAppErrorCode);
	DEKAF2_PRIVATE int  OnStreamStopSending    (StreamID id, uint64_t iAppErrorCode);
	DEKAF2_PRIVATE int  OnExtendMaxStreamData  (StreamID id);
	DEKAF2_PRIVATE int  OnExtendMaxStreams     (bool bBidirectional, uint64_t iMaxStreams);

	KTLSContext&                 m_TLSContext;
	std::unique_ptr<Impl>        m_Impl;
	std::unique_ptr<::ssl_st, void(*)(::ssl_st*)>
	                             m_SSL;
	::ngtcp2_conn*               m_Conn        { nullptr };
	::ngtcp2_crypto_ossl_ctx*    m_CryptoCtx   { nullptr };
	Delegate*                    m_Delegate    { nullptr };
	int                          m_Socket      { -1 };
	KDuration                    m_Timeout     { KStreamOptions::GetDefaultTimeout() };
	KTCPEndPoint                 m_EndPointAddress;
	KString                      m_sDelegateError;
	std::vector<std::pair<StreamID, std::size_t>>
	                             m_PendingCredits;
	CongestionControl            m_CongestionControl   { s_DefaultCongestionControl };
	bool                         m_bHandshakeCompleted { false };
	bool                         m_bClosed             { false };
	bool                         m_bInWriteLoop        { false };

	static CongestionControl     s_DefaultCongestionControl;

}; // KQuicConnection

/// @}

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGTCP2
