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


#include <dekaf2/net/quic/kquicconnection.h>

#if DEKAF2_HAS_NGTCP2

#include <dekaf2/core/logging/klog.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/net/address/kresolve.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_ossl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <mutex>
#include <array>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the members that need the ngtcp2 types
struct KQuicConnection::Impl
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	ngtcp2_path_storage         Path;
	ngtcp2_crypto_conn_ref      ConnRef;
	ngtcp2_pkt_info             TXPacketInfo;
	std::array<uint8_t, 65536>  RXBuffer;
	std::array<uint8_t, 2048>   TXBuffer;

}; // Impl

namespace {

// flow control windows - the stream windows are above the bandwidth-delay
// product of a 200 Mbit/s link with 100 ms RTT (2.5 MB), the connection
// window leaves room for a few parallel streams
constexpr uint64_t iStreamWindow     = 4 * 1024 * 1024;
constexpr uint64_t iConnectionWindow = 16 * 1024 * 1024;
constexpr uint64_t iMaxStreamWindow  = 8 * 1024 * 1024;
constexpr uint64_t iMaxWindow        = 32 * 1024 * 1024;
constexpr uint64_t iMaxStreams       = 100;
// the idle timeout is a floor - a connection that is neither sending nor
// receiving for this long is dropped by ngtcp2 regardless of our I/O timeout
constexpr KDuration MinIdleTimeout   = chrono::seconds(30);

//-----------------------------------------------------------------------------
/// ngtcp2 timestamps are nanoseconds from a monotonic clock
ngtcp2_tstamp Now()
//-----------------------------------------------------------------------------
{
	return static_cast<ngtcp2_tstamp>(
		chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now().time_since_epoch()).count()
	);
}

//-----------------------------------------------------------------------------
ngtcp2_duration ToNS(KDuration Duration)
//-----------------------------------------------------------------------------
{
	auto ns = Duration.nanoseconds().count();
	return static_cast<ngtcp2_duration>(ns < 0 ? 0 : ns);
}

//-----------------------------------------------------------------------------
KDuration FromNS(ngtcp2_duration ns)
//-----------------------------------------------------------------------------
{
	return KDuration(chrono::nanoseconds(ns));
}

//-----------------------------------------------------------------------------
KString OpenSSLErrors()
//-----------------------------------------------------------------------------
{
	KString sErrors;

	for (auto iError = ::ERR_get_error(); iError; iError = ::ERR_get_error())
	{
		char szBuffer[256];
		::ERR_error_string_n(iError, szBuffer, sizeof(szBuffer));

		if (!sErrors.empty())
		{
			sErrors += "; ";
		}

		sErrors += szBuffer;
	}

	return sErrors;

} // OpenSSLErrors

} // end of anonymous namespace

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the C callbacks for ngtcp2, forwarding to the connection object
struct KQuicConnectionCallbacks
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	static KQuicConnection* ToThis(void* user_data)
	{
		return static_cast<KQuicConnection*>(user_data);
	}

	static ngtcp2_conn* get_conn(ngtcp2_crypto_conn_ref* conn_ref)
	{
		return ToThis(conn_ref->user_data)->m_Conn;
	}

	static int handshake_completed(ngtcp2_conn*, void* user_data)
	{
		return ToThis(user_data)->OnHandshakeCompleted();
	}

	static int recv_stream_data(ngtcp2_conn*, uint32_t flags, int64_t stream_id, uint64_t /*offset*/,
	                            const uint8_t* data, size_t datalen, void* user_data, void* /*stream_user_data*/)
	{
		return ToThis(user_data)->OnRecvStreamData(stream_id, data, datalen, (flags & NGTCP2_STREAM_DATA_FLAG_FIN) != 0);
	}

	static int acked_stream_data_offset(ngtcp2_conn*, int64_t stream_id, uint64_t offset, uint64_t datalen,
	                                    void* user_data, void* /*stream_user_data*/)
	{
		return ToThis(user_data)->OnAckedStreamDataOffset(stream_id, offset, datalen);
	}

	static int stream_open(ngtcp2_conn*, int64_t stream_id, void* user_data)
	{
		return ToThis(user_data)->OnStreamOpen(stream_id);
	}

	static int stream_close(ngtcp2_conn*, uint32_t flags, int64_t stream_id, uint64_t app_error_code,
	                        void* user_data, void* /*stream_user_data*/)
	{
		return ToThis(user_data)->OnStreamClose(stream_id, (flags & NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET) != 0, app_error_code);
	}

	static int stream_reset(ngtcp2_conn*, int64_t stream_id, uint64_t final_size, uint64_t app_error_code,
	                        void* user_data, void* /*stream_user_data*/)
	{
		return ToThis(user_data)->OnStreamReset(stream_id, final_size, app_error_code);
	}

	static int stream_stop_sending(ngtcp2_conn*, int64_t stream_id, uint64_t app_error_code,
	                               void* user_data, void* /*stream_user_data*/)
	{
		return ToThis(user_data)->OnStreamStopSending(stream_id, app_error_code);
	}

	static int extend_max_stream_data(ngtcp2_conn*, int64_t stream_id, uint64_t /*max_data*/,
	                                  void* user_data, void* /*stream_user_data*/)
	{
		return ToThis(user_data)->OnExtendMaxStreamData(stream_id);
	}

	static int extend_max_local_streams_bidi(ngtcp2_conn*, uint64_t max_streams, void* user_data)
	{
		return ToThis(user_data)->OnExtendMaxStreams(true, max_streams);
	}

	static int extend_max_local_streams_uni(ngtcp2_conn*, uint64_t max_streams, void* user_data)
	{
		return ToThis(user_data)->OnExtendMaxStreams(false, max_streams);
	}

	static void rand(uint8_t* dest, size_t destlen, const ngtcp2_rand_ctx*)
	{
		::RAND_bytes(dest, static_cast<int>(destlen));
	}

	static int get_new_connection_id(ngtcp2_conn*, ngtcp2_cid* cid, uint8_t* token, size_t cidlen, void*)
	{
		if (::RAND_bytes(cid->data, static_cast<int>(cidlen)) != 1)
		{
			return NGTCP2_ERR_CALLBACK_FAILURE;
		}

		cid->datalen = cidlen;

		if (::RAND_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN) != 1)
		{
			return NGTCP2_ERR_CALLBACK_FAILURE;
		}

		return 0;
	}

#ifdef DEKAF2_WITH_KLOG
	static void log_printf(void*, const char* format, ...)
	{
		char szBuffer[1024];
		va_list ap;
		va_start(ap, format);
		::vsnprintf(szBuffer, sizeof(szBuffer), format, ap);
		va_end(ap);
		kDebug(4, "ngtcp2: {}", szBuffer);
	}
#endif

}; // KQuicConnectionCallbacks

KQuicConnection::CongestionControl KQuicConnection::s_DefaultCongestionControl { KQuicConnection::CongestionControl::Cubic };

//-----------------------------------------------------------------------------
KQuicConnection::KQuicConnection(KTLSContext& Context)
//-----------------------------------------------------------------------------
: m_TLSContext(Context)
, m_Impl(std::make_unique<Impl>())
, m_SSL(nullptr, &::SSL_free)
{
	m_Impl->ConnRef.get_conn  = &KQuicConnectionCallbacks::get_conn;
	m_Impl->ConnRef.user_data = this;

} // ctor

//-----------------------------------------------------------------------------
KQuicConnection::~KQuicConnection()
//-----------------------------------------------------------------------------
{
	Close(0);

} // dtor

//-----------------------------------------------------------------------------
KStringView KQuicConnection::ToString(CongestionControl cc)
//-----------------------------------------------------------------------------
{
	switch (cc)
	{
		case CongestionControl::Cubic: return "cubic";
		case CongestionControl::BBR:   return "bbr";
		case CongestionControl::Reno:  return "reno";
	}

	return "unknown";

} // ToString

//-----------------------------------------------------------------------------
bool KQuicConnection::FromString(KStringView sName, CongestionControl& cc)
//-----------------------------------------------------------------------------
{
	auto sLower = sName.ToLowerASCII();

	if      (sLower == "cubic") cc = CongestionControl::Cubic;
	else if (sLower == "bbr"  ) cc = CongestionControl::BBR;
	else if (sLower == "reno" ) cc = CongestionControl::Reno;
	else return false;

	return true;

} // FromString

//-----------------------------------------------------------------------------
bool KQuicConnection::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options, KStringView sTLSHostname, KStringView sALPN)
//-----------------------------------------------------------------------------
{
	// allow re-use of a previously closed connection
	Teardown();

	m_bClosed             = false;
	m_bHandshakeCompleted = false;
	m_sDelegateError.clear();
	m_PendingCredits.clear();
	m_Timeout = Options.GetTimeout();

	{
		// the crypto helper library wants a one-time initialization
		static std::once_flag s_Once;
		std::call_once(s_Once, []{ ::ngtcp2_crypto_ossl_init(); });
	}

	if (!SetupSocket(Endpoint, Options))
	{
		Teardown();
		return false;
	}

	KStringView sHostname = Endpoint.Domain.get();

	if (!SetupTLS(sTLSHostname.empty() ? sHostname : sTLSHostname, sALPN, Options.IsSet(KStreamOptions::VerifyCert)))
	{
		Teardown();
		return false;
	}

	if (!SetupQuic())
	{
		Teardown();
		return false;
	}

	if (!Handshake())
	{
		// the error is set, and the connection is closed
		return false;
	}

	Options.ApplySocketOptions(m_Socket, true);

	kDebug(2, "connected to {} {}", "endpoint", GetEndPointAddress());

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KQuicConnection::SetupSocket(const KTCPEndPoint& Endpoint, const KStreamOptions& Options)
//-----------------------------------------------------------------------------
{
	KStringView sHostname = Endpoint.Domain.get();
	KString sIPAddress;

	if (kIsIPv6Address(sHostname, true))
	{
		// this is a ip v6 numerical address in brackets
		sIPAddress = sHostname.ToView(1, sHostname.size() - 2);
	}
	else
	{
		// check the known hostnames - returns either a known IP address, or the original hostname
		sIPAddress = KResolve::GetKnownHostAddress(sHostname, Options.GetFamily());
	}

	kDebug(3, "resolving domain {}", sHostname);

	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family   = Options.GetNativeFamily();
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	struct addrinfo* pAddresses { nullptr };
	auto sPort = Endpoint.Port.Serialize();

	auto iResult = ::getaddrinfo(sIPAddress.c_str(), sPort.c_str(), &hints, &pAddresses);

	if (iResult != 0 || !pAddresses)
	{
		return SetError(kFormat("cannot resolve {}: {}", sHostname, ::gai_strerror(iResult)));
	}

	std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)> Addresses(pAddresses, &::freeaddrinfo);

	kDebug(3, "trying to connect to {} {}", "endpoint", Endpoint);

	const struct addrinfo* ai;

	for (ai = Addresses.get(); ai != nullptr; ai = ai->ai_next)
	{
		m_Socket = ::socket(ai->ai_family, SOCK_DGRAM, IPPROTO_UDP);

		if (m_Socket < 0)
		{
			continue;
		}

		// a connected UDP socket filters incoming datagrams by peer, and reports
		// ICMP unreachable errors through recv()
		if (::connect(m_Socket, ai->ai_addr, ai->ai_addrlen) == 0)
		{
			break;
		}

		::close(m_Socket);
		m_Socket = -1;
	}

	if (m_Socket < 0 || !ai)
	{
		return SetError(kFormat("cannot connect to {}", Endpoint));
	}

	{
		auto iFlags = ::fcntl(m_Socket, F_GETFL, 0);

		if (iFlags < 0 || ::fcntl(m_Socket, F_SETFL, iFlags | O_NONBLOCK) < 0)
		{
			return SetError("cannot switch socket to non-blocking mode");
		}
	}

	struct sockaddr_storage LocalAddress;
	socklen_t iLocalLen = sizeof(LocalAddress);

	if (::getsockname(m_Socket, reinterpret_cast<struct sockaddr*>(&LocalAddress), &iLocalLen) != 0)
	{
		return SetError("cannot get local socket address");
	}

	::ngtcp2_path_storage_init(&m_Impl->Path,
	                           reinterpret_cast<const struct sockaddr*>(&LocalAddress), iLocalLen,
	                           ai->ai_addr, ai->ai_addrlen,
	                           nullptr);

	{
		char szHost[NI_MAXHOST];
		char szPort[NI_MAXSERV];

		if (::getnameinfo(ai->ai_addr, ai->ai_addrlen, szHost, sizeof(szHost), szPort, sizeof(szPort), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
		{
			if (ai->ai_family == AF_INET6)
			{
				m_EndPointAddress = KTCPEndPoint(kFormat("[{}]:{}", szHost, szPort));
			}
			else
			{
				m_EndPointAddress = KTCPEndPoint(kFormat("{}:{}", szHost, szPort));
			}
		}
	}

	kDebug(2, "using endpoint address {}", GetEndPointAddress());

	return true;

} // SetupSocket

//-----------------------------------------------------------------------------
bool KQuicConnection::SetupTLS(KStringView sHostname, KStringView sALPN, bool bVerifyCert)
//-----------------------------------------------------------------------------
{
	m_SSL.reset(::SSL_new(m_TLSContext.GetContext().native_handle()));

	if (!m_SSL)
	{
		return SetError(kFormat("cannot create SSL object: {}", OpenSSLErrors()));
	}

	auto ssl = m_SSL.get();

	// the crypto helper needs to find the ngtcp2 connection from the SSL object
	SSL_set_app_data(ssl, &m_Impl->ConnRef);
	::SSL_set_connect_state(ssl);

	// QUIC mandates TLS 1.3
	if (!::SSL_set_min_proto_version(ssl, TLS1_3_VERSION))
	{
		return SetError("cannot set minimum TLS version 1.3");
	}

	// this registers the OpenSSL QUIC TLS callbacks (SSL_set_quic_tls_cbs) which
	// route the handshake messages through ngtcp2 instead of a BIO
	if (::ngtcp2_crypto_ossl_configure_client_session(ssl) != 0)
	{
		return SetError("cannot configure the TLS session for QUIC");
	}

	if (::ngtcp2_crypto_ossl_ctx_new(&m_CryptoCtx, ssl) != 0)
	{
		return SetError("cannot create the ngtcp2 crypto context");
	}

	if (!sALPN.empty())
	{
		if (::SSL_set_alpn_protos(ssl,
		                          reinterpret_cast<const unsigned char*>(sALPN.data()),
		                          static_cast<unsigned int>(sALPN.size())) != 0)
		{
			return SetError(kFormat("failed to set ALPN protocols: '{}'", kEscapeForLogging(sALPN)));
		}
	}

	// QUIC mandates peer verification, but we still allow to switch it off for testing
	if (bVerifyCert)
	{
		::SSL_set_verify(ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
	}
	else
	{
		::SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr);
	}

	// SNI and the name to verify: the host to talk to, which may differ from the host connected to
	auto sError = KTLSContext::SetClientIdentity(ssl, sHostname, bVerifyCert);

	if (!sError.empty())
	{
		return SetError(std::move(sError));
	}

	return true;

} // SetupTLS

//-----------------------------------------------------------------------------
bool KQuicConnection::SetupQuic()
//-----------------------------------------------------------------------------
{
	ngtcp2_cid dcid;
	ngtcp2_cid scid;
	dcid.datalen = 18;
	scid.datalen = 17;

	if (::RAND_bytes(dcid.data, static_cast<int>(dcid.datalen)) != 1 ||
	    ::RAND_bytes(scid.data, static_cast<int>(scid.datalen)) != 1)
	{
		return SetError("cannot generate connection IDs");
	}

	ngtcp2_callbacks callbacks;
	std::memset(&callbacks, 0, sizeof(callbacks));

	// the handshake and packet protection are done by the crypto helper library
	callbacks.client_initial             = ::ngtcp2_crypto_client_initial_cb;
	callbacks.recv_crypto_data           = ::ngtcp2_crypto_recv_crypto_data_cb;
	callbacks.encrypt                    = ::ngtcp2_crypto_encrypt_cb;
	callbacks.decrypt                    = ::ngtcp2_crypto_decrypt_cb;
	callbacks.hp_mask                    = ::ngtcp2_crypto_hp_mask_cb;
	callbacks.recv_retry                 = ::ngtcp2_crypto_recv_retry_cb;
	callbacks.update_key                 = ::ngtcp2_crypto_update_key_cb;
	callbacks.delete_crypto_aead_ctx     = ::ngtcp2_crypto_delete_crypto_aead_ctx_cb;
	callbacks.delete_crypto_cipher_ctx   = ::ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
	callbacks.get_path_challenge_data    = ::ngtcp2_crypto_get_path_challenge_data_cb;
	callbacks.version_negotiation        = ::ngtcp2_crypto_version_negotiation_cb;
	// the rest is ours
	callbacks.handshake_completed        = &KQuicConnectionCallbacks::handshake_completed;
	callbacks.recv_stream_data           = &KQuicConnectionCallbacks::recv_stream_data;
	callbacks.acked_stream_data_offset   = &KQuicConnectionCallbacks::acked_stream_data_offset;
	callbacks.stream_open                = &KQuicConnectionCallbacks::stream_open;
	callbacks.stream_close               = &KQuicConnectionCallbacks::stream_close;
	callbacks.stream_reset               = &KQuicConnectionCallbacks::stream_reset;
	callbacks.stream_stop_sending        = &KQuicConnectionCallbacks::stream_stop_sending;
	callbacks.extend_max_stream_data     = &KQuicConnectionCallbacks::extend_max_stream_data;
	callbacks.extend_max_local_streams_bidi = &KQuicConnectionCallbacks::extend_max_local_streams_bidi;
	callbacks.extend_max_local_streams_uni  = &KQuicConnectionCallbacks::extend_max_local_streams_uni;
	callbacks.rand                       = &KQuicConnectionCallbacks::rand;
	callbacks.get_new_connection_id      = &KQuicConnectionCallbacks::get_new_connection_id;

	ngtcp2_settings settings;
	::ngtcp2_settings_default(&settings);
	settings.initial_ts        = Now();
	settings.handshake_timeout = ToNS(m_Timeout);
	settings.max_window        = iMaxWindow;
	settings.max_stream_window = iMaxStreamWindow;

	switch (m_CongestionControl)
	{
		case CongestionControl::Cubic: settings.cc_algo = NGTCP2_CC_ALGO_CUBIC; break;
		case CongestionControl::BBR:   settings.cc_algo = NGTCP2_CC_ALGO_BBR;   break;
		case CongestionControl::Reno:  settings.cc_algo = NGTCP2_CC_ALGO_RENO;  break;
	}

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(4))
	{
		settings.log_printf = &KQuicConnectionCallbacks::log_printf;
	}
#endif

	ngtcp2_transport_params params;
	::ngtcp2_transport_params_default(&params);
	params.initial_max_stream_data_bidi_local  = iStreamWindow;
	params.initial_max_stream_data_bidi_remote = iStreamWindow;
	params.initial_max_stream_data_uni         = iStreamWindow;
	params.initial_max_data                    = iConnectionWindow;
	params.initial_max_streams_bidi            = iMaxStreams;
	params.initial_max_streams_uni             = iMaxStreams;
	params.max_idle_timeout                    = ToNS(std::max(MinIdleTimeout, KDuration(m_Timeout * 2)));
	params.active_connection_id_limit          = 7;

	auto iResult = ::ngtcp2_conn_client_new(&m_Conn, &dcid, &scid, &m_Impl->Path.path,
	                                        NGTCP2_PROTO_VER_V1, &callbacks, &settings, &params,
	                                        nullptr, this);
	if (iResult != 0)
	{
		m_Conn = nullptr;
		return SetError(kFormat("cannot create QUIC connection: {}", ::ngtcp2_strerror(iResult)));
	}

	// for the OpenSSL backend the native handle is the crypto context, not the SSL object
	::ngtcp2_conn_set_tls_native_handle(m_Conn, m_CryptoCtx);

	kDebug(3, "QUIC connection created, congestion control {}", ToString(m_CongestionControl));

	return true;

} // SetupQuic

//-----------------------------------------------------------------------------
bool KQuicConnection::Handshake()
//-----------------------------------------------------------------------------
{
	kDebug(3, "starting QUIC handshake");

	auto tStart = chrono::steady_clock::now();

	while (!m_bHandshakeCompleted)
	{
		KDuration Remaining = m_Timeout - KDuration(chrono::steady_clock::now() - tStart);

		if (Remaining <= KDuration::zero())
		{
			Close(0);
			return SetError("QUIC handshake timed out");
		}

		if (Pump(Remaining) == PumpResult::Error)
		{
			// the error is already set
			return false;
		}
	}

#ifdef DEKAF2_WITH_KLOG
	if (kWouldLog(2))
	{
		auto cipher = ::SSL_get_current_cipher(m_SSL.get());
		kDebug(2, "QUIC handshake successful, {}, cipher {}, ALPN '{}', congestion control {}",
		       cipher ? ::SSL_CIPHER_get_version(cipher) : "?",
		       cipher ? ::SSL_CIPHER_get_name(cipher)    : "?",
		       GetALPN(),
		       ToString(m_CongestionControl));
	}
#endif

	return true;

} // Handshake

//-----------------------------------------------------------------------------
KQuicConnection::PumpResult KQuicConnection::Pump(KDuration MaxWait)
//-----------------------------------------------------------------------------
{
	if (!m_Conn || m_bClosed)
	{
		if (!HasError())
		{
			SetError("QUIC connection is not open");
		}
		return PumpResult::Error;
	}

	if (!Write())
	{
		return PumpResult::Error;
	}

	KDuration Wait = (MaxWait < KDuration::zero()) ? KDuration::zero() : MaxWait;

	{
		auto iExpiry = ::ngtcp2_conn_get_expiry(m_Conn);

		if (iExpiry != UINT64_MAX)
		{
			auto iNow = Now();
			KDuration UntilExpiry = (iExpiry > iNow) ? FromNS(iExpiry - iNow) : KDuration::zero();

			if (UntilExpiry < Wait)
			{
				Wait = UntilExpiry;
			}
		}
	}

	auto iPoll = kPoll(m_Socket, POLLIN, Wait);

	if (iPoll < 0)
	{
		Fail(0);
		SetError(kFormat("poll failed: {}", ::strerror(errno)));
		return PumpResult::Error;
	}

	bool bReceived { false };

	if (iPoll > 0)
	{
		if (!Read())
		{
			return PumpResult::Error;
		}

		bReceived = true;
	}

	{
		auto iNow = Now();

		if (::ngtcp2_conn_get_expiry(m_Conn) <= iNow)
		{
			auto iResult = ::ngtcp2_conn_handle_expiry(m_Conn, iNow);

			if (iResult != 0)
			{
				Fail(iResult);
				return PumpResult::Error;
			}
		}
	}

	if (!Write())
	{
		return PumpResult::Error;
	}

	return bReceived ? PumpResult::Received : PumpResult::Idle;

} // Pump

//-----------------------------------------------------------------------------
bool KQuicConnection::Flush()
//-----------------------------------------------------------------------------
{
	if (!m_Conn || m_bClosed)
	{
		return false;
	}

	return Write();

} // Flush

//-----------------------------------------------------------------------------
bool KQuicConnection::Read()
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		auto iRead = ::recv(m_Socket, m_Impl->RXBuffer.data(), m_Impl->RXBuffer.size(), 0);

		if (iRead < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return true;
			}
			else if (errno == EINTR)
			{
				continue;
			}
			else if (errno == ECONNREFUSED)
			{
				// ICMP port unreachable on the connected socket
				m_bClosed = true;
				return SetError(kFormat("{}: connection refused", GetEndPointAddress()));
			}

			return SetError(kFormat("recv failed: {}", ::strerror(errno)));
		}

		if (iRead == 0)
		{
			continue;
		}

		ngtcp2_pkt_info pi;
		std::memset(&pi, 0, sizeof(pi));

		auto iResult = ::ngtcp2_conn_read_pkt(m_Conn, &m_Impl->Path.path, &pi,
		                                      m_Impl->RXBuffer.data(), static_cast<std::size_t>(iRead), Now());

		if (iResult != 0)
		{
			if (iResult == NGTCP2_ERR_DRAINING)
			{
				// the peer sent CONNECTION_CLOSE - report what it said
				m_bClosed = true;
				auto ccerr = ::ngtcp2_conn_get_ccerr(m_Conn);
				KString sError("connection closed by peer");

				if (ccerr)
				{
					switch (ccerr->type)
					{
						case NGTCP2_CCERR_TYPE_TRANSPORT:
							sError += kFormat(", transport error 0x{:x}", ccerr->error_code);
							if (ccerr->error_code >= NGTCP2_CRYPTO_ERROR && ccerr->error_code < NGTCP2_CRYPTO_ERROR + 0x100)
							{
								sError += kFormat(" (TLS alert {})", ccerr->error_code - NGTCP2_CRYPTO_ERROR);
							}
							break;
						case NGTCP2_CCERR_TYPE_APPLICATION:
							sError += kFormat(", application error 0x{:x}", ccerr->error_code);
							break;
						default:
							break;
					}

					if (ccerr->reasonlen)
					{
						sError += kFormat(": {}", KStringView(reinterpret_cast<const char*>(ccerr->reason), ccerr->reasonlen));
					}
				}

				return SetError(sError);
			}
			else if (iResult == NGTCP2_ERR_CLOSING)
			{
				m_bClosed = true;
				return SetError("connection is closing");
			}

			return Fail(iResult);
		}
	}

} // Read

//-----------------------------------------------------------------------------
bool KQuicConnection::Write()
//-----------------------------------------------------------------------------
{
	if (m_bClosed)
	{
		return false;
	}

	auto  ts          = Now();
	auto& Path        = m_Impl->Path.path;
	auto& PacketInfo  = m_Impl->TXPacketInfo;
	auto  iMaxPayload = std::min(::ngtcp2_conn_get_max_tx_udp_payload_size(m_Conn), m_Impl->TXBuffer.size());
	auto  iQuantum    = ::ngtcp2_conn_get_send_quantum(m_Conn);
	std::size_t iSent { 0 };

	std::array<ngtcp2_vec, 16> vecs;

	/*
	 * While NGTCP2_WRITE_STREAM_FLAG_MORE is in use, no other ngtcp2 function may be
	 * called until writev_stream returns 0, a packet, or a fatal error. Flow control
	 * credits from delegate callbacks are therefore queued and applied afterwards.
	 */
	m_bInWriteLoop = true;

	for (;;)
	{
		StreamID       id     { -1 };
		bool           bFin   { false };
		std::ptrdiff_t iVecs  { 0 };

		if (m_Delegate && ::ngtcp2_conn_get_max_data_left(m_Conn) > 0)
		{
			iVecs = m_Delegate->GetQuicStreamData(id, bFin, vecs.data(), vecs.size());

			if (iVecs < 0)
			{
				m_bInWriteLoop = false;
				return Fail(NGTCP2_ERR_CALLBACK_FAILURE);
			}
		}

		uint32_t iFlags = NGTCP2_WRITE_STREAM_FLAG_MORE;

		if (bFin)
		{
			iFlags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
		}

		ngtcp2_ssize iDataLen { -1 };

		auto iWrite = ::ngtcp2_conn_writev_stream(m_Conn, &Path, &PacketInfo,
		                                          m_Impl->TXBuffer.data(), iMaxPayload,
		                                          &iDataLen, iFlags, id,
		                                          vecs.data(), static_cast<std::size_t>(iVecs), ts);
		if (iWrite < 0)
		{
			switch (iWrite)
			{
				case NGTCP2_ERR_STREAM_DATA_BLOCKED:
					// the stream (or connection) flow control window is exhausted
					if (m_Delegate) m_Delegate->OnQuicStreamBlocked(id);
					continue;

				case NGTCP2_ERR_STREAM_SHUT_WR:
				case NGTCP2_ERR_STREAM_NOT_FOUND:
					// our sending side of this stream is gone
					if (m_Delegate) m_Delegate->OnQuicStreamShutWrite(id);
					continue;

				case NGTCP2_ERR_WRITE_MORE:
					// the data went into the packet, and there is room for more
					if (m_Delegate && iDataLen >= 0)
					{
						if (m_Delegate->OnQuicWriteOffset(id, static_cast<std::size_t>(iDataLen)) != 0)
						{
							m_bInWriteLoop = false;
							return Fail(NGTCP2_ERR_CALLBACK_FAILURE);
						}
					}
					continue;

				default:
					m_bInWriteLoop = false;
					return Fail(static_cast<int>(iWrite));
			}
		}

		if (m_Delegate && iDataLen >= 0)
		{
			if (m_Delegate->OnQuicWriteOffset(id, static_cast<std::size_t>(iDataLen)) != 0)
			{
				m_bInWriteLoop = false;
				return Fail(NGTCP2_ERR_CALLBACK_FAILURE);
			}
		}

		if (iWrite == 0)
		{
			// nothing to send, or congestion limited
			break;
		}

		if (!Send(m_Impl->TXBuffer.data(), static_cast<std::size_t>(iWrite)))
		{
			m_bInWriteLoop = false;
			return false;
		}

		iSent += static_cast<std::size_t>(iWrite);

		if (iSent >= iQuantum)
		{
			// pacing: enough for this round
			break;
		}
	}

	::ngtcp2_conn_update_pkt_tx_time(m_Conn, ts);

	m_bInWriteLoop = false;

	ApplyPendingCredits();

	return true;

} // Write

//-----------------------------------------------------------------------------
bool KQuicConnection::Send(const uint8_t* data, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		auto iSent = ::send(m_Socket, data, iSize, 0);

		if (iSent >= 0)
		{
			return true;
		}

		if (errno == EINTR)
		{
			continue;
		}
		else if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// the socket buffer is full - wait until it drains. The packet
			// counts as sent for ngtcp2, we cannot drop it.
			auto iPoll = kPoll(m_Socket, POLLOUT, m_Timeout);

			if (iPoll <= 0)
			{
				return SetError("send timed out");
			}

			continue;
		}
		else if (errno == EMSGSIZE)
		{
			// a path MTU probe that the local stack refuses - ngtcp2 treats
			// the lost probe as a negative answer
			kDebug(3, "EMSGSIZE for {} bytes", iSize);
			return true;
		}
		else if (errno == ECONNREFUSED)
		{
			m_bClosed = true;
			return SetError(kFormat("{}: connection refused", GetEndPointAddress()));
		}

		return SetError(kFormat("send failed: {}", ::strerror(errno)));
	}

} // Send

//-----------------------------------------------------------------------------
bool KQuicConnection::Fail(int iError)
//-----------------------------------------------------------------------------
{
	KString sError;

	if (iError == NGTCP2_ERR_CALLBACK_FAILURE && !m_sDelegateError.empty())
	{
		sError = m_sDelegateError;
	}
	else if (iError == NGTCP2_ERR_CRYPTO)
	{
		sError = kFormat("TLS handshake failed, alert {}", ::ngtcp2_conn_get_tls_alert(m_Conn));

		if (m_SSL)
		{
			auto iVerify = ::SSL_get_verify_result(m_SSL.get());

			if (iVerify != X509_V_OK)
			{
				sError += kFormat(", verify error: {}", ::X509_verify_cert_error_string(iVerify));
			}
		}

		auto sOpenSSL = OpenSSLErrors();

		if (!sOpenSSL.empty())
		{
			sError += ": ";
			sError += sOpenSSL;
		}
	}
	else if (iError != 0)
	{
		sError = ::ngtcp2_strerror(iError);
	}

	if (m_Conn && !m_bClosed
		&& !::ngtcp2_conn_in_closing_period(m_Conn)
		&& !::ngtcp2_conn_in_draining_period(m_Conn))
	{
		// tell the peer that we are gone
		ngtcp2_ccerr ccerr;

		if (iError == NGTCP2_ERR_CRYPTO)
		{
			::ngtcp2_ccerr_set_tls_alert(&ccerr, ::ngtcp2_conn_get_tls_alert(m_Conn), nullptr, 0);
		}
		else if (iError != 0)
		{
			::ngtcp2_ccerr_set_liberr(&ccerr, iError, nullptr, 0);
		}
		else
		{
			::ngtcp2_ccerr_set_application_error(&ccerr, 0, nullptr, 0);
		}

		ngtcp2_pkt_info pi;

		auto iWrite = ::ngtcp2_conn_write_connection_close(m_Conn, &m_Impl->Path.path, &pi,
		                                                   m_Impl->TXBuffer.data(), m_Impl->TXBuffer.size(),
		                                                   &ccerr, Now());
		if (iWrite > 0)
		{
			::send(m_Socket, m_Impl->TXBuffer.data(), static_cast<std::size_t>(iWrite), 0);
		}
	}

	m_bClosed = true;

	if (!sError.empty())
	{
		return SetError(sError);
	}

	return false;

} // Fail

//-----------------------------------------------------------------------------
void KQuicConnection::Close(uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	if (m_Conn && !m_bClosed
		&& !::ngtcp2_conn_in_closing_period(m_Conn)
		&& !::ngtcp2_conn_in_draining_period(m_Conn))
	{
		ngtcp2_ccerr ccerr;
		::ngtcp2_ccerr_set_application_error(&ccerr, iAppErrorCode, nullptr, 0);

		ngtcp2_pkt_info pi;

		auto iWrite = ::ngtcp2_conn_write_connection_close(m_Conn, &m_Impl->Path.path, &pi,
		                                                   m_Impl->TXBuffer.data(), m_Impl->TXBuffer.size(),
		                                                   &ccerr, Now());
		if (iWrite > 0)
		{
			::send(m_Socket, m_Impl->TXBuffer.data(), static_cast<std::size_t>(iWrite), 0);
		}
	}

	m_bClosed = true;

	Teardown();

} // Close

//-----------------------------------------------------------------------------
void KQuicConnection::Teardown()
//-----------------------------------------------------------------------------
{
	if (m_Conn)
	{
#ifdef DEKAF2_WITH_KLOG
		if (kWouldLog(2))
		{
			auto info = GetInfo();
			kDebug(2, "QUIC connection stats: {}, rtt {} (min {}), cwnd {}, sent {} pkts / {} bytes, received {} pkts / {} bytes, lost {} pkts / {} bytes",
			       ToString(m_CongestionControl),
			       info.SmoothedRTT, info.MinRTT, info.iCongestionWindow,
			       info.iPacketsSent, info.iBytesSent,
			       info.iPacketsReceived, info.iBytesReceived,
			       info.iPacketsLost, info.iBytesLost);
		}
#endif
		::ngtcp2_conn_del(m_Conn);
		m_Conn = nullptr;
	}

	if (m_SSL)
	{
		// the crypto helper's callbacks would look up the connection through
		// the app data during SSL_free - it is gone
		SSL_set_app_data(m_SSL.get(), nullptr);
		m_SSL.reset();
	}

	if (m_CryptoCtx)
	{
		::ngtcp2_crypto_ossl_ctx_del(m_CryptoCtx);
		m_CryptoCtx = nullptr;
	}

	if (m_Socket >= 0)
	{
		::close(m_Socket);
		m_Socket = -1;
	}

	m_bHandshakeCompleted = false;

} // Teardown

//-----------------------------------------------------------------------------
bool KQuicConnection::OpenBidiStream(StreamID& id)
//-----------------------------------------------------------------------------
{
	if (!IsConnected())
	{
		return SetError("QUIC connection is not open");
	}

	auto iResult = ::ngtcp2_conn_open_bidi_stream(m_Conn, &id, nullptr);

	if (iResult != 0)
	{
		id = -1;
		return SetError(kFormat("cannot open bidirectional stream: {}", ::ngtcp2_strerror(iResult)));
	}

	kDebug(4, "[stream {}] opened bidirectional stream", id);

	return true;

} // OpenBidiStream

//-----------------------------------------------------------------------------
bool KQuicConnection::OpenUniStream(StreamID& id)
//-----------------------------------------------------------------------------
{
	if (!IsConnected())
	{
		return SetError("QUIC connection is not open");
	}

	auto iResult = ::ngtcp2_conn_open_uni_stream(m_Conn, &id, nullptr);

	if (iResult != 0)
	{
		id = -1;
		return SetError(kFormat("cannot open unidirectional stream: {}", ::ngtcp2_strerror(iResult)));
	}

	kDebug(4, "[stream {}] opened unidirectional stream", id);

	return true;

} // OpenUniStream

//-----------------------------------------------------------------------------
bool KQuicConnection::ShutdownStreamRead(StreamID id, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	if (!m_Conn || m_bClosed)
	{
		return false;
	}

	auto iResult = ::ngtcp2_conn_shutdown_stream_read(m_Conn, 0, id, iAppErrorCode);

	if (iResult != 0 && iResult != NGTCP2_ERR_STREAM_NOT_FOUND)
	{
		kDebug(2, "[stream {}] cannot shutdown read side: {}", id, ::ngtcp2_strerror(iResult));
		return false;
	}

	return true;

} // ShutdownStreamRead

//-----------------------------------------------------------------------------
bool KQuicConnection::ShutdownStreamWrite(StreamID id, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	if (!m_Conn || m_bClosed)
	{
		return false;
	}

	auto iResult = ::ngtcp2_conn_shutdown_stream_write(m_Conn, 0, id, iAppErrorCode);

	if (iResult != 0 && iResult != NGTCP2_ERR_STREAM_NOT_FOUND)
	{
		kDebug(2, "[stream {}] cannot shutdown write side: {}", id, ::ngtcp2_strerror(iResult));
		return false;
	}

	return true;

} // ShutdownStreamWrite

//-----------------------------------------------------------------------------
void KQuicConnection::CreditReceived(StreamID id, std::size_t iBytes)
//-----------------------------------------------------------------------------
{
	if (!iBytes || !m_Conn || m_bClosed)
	{
		return;
	}

	if (m_bInWriteLoop)
	{
		// see Write() - ngtcp2 must not be called now
		m_PendingCredits.emplace_back(id, iBytes);
		return;
	}

	// a closed stream is not an error here
	::ngtcp2_conn_extend_max_stream_offset(m_Conn, id, iBytes);
	::ngtcp2_conn_extend_max_offset(m_Conn, iBytes);

} // CreditReceived

//-----------------------------------------------------------------------------
void KQuicConnection::ApplyPendingCredits()
//-----------------------------------------------------------------------------
{
	if (m_PendingCredits.empty() || !m_Conn || m_bClosed)
	{
		m_PendingCredits.clear();
		return;
	}

	for (const auto& Credit : m_PendingCredits)
	{
		::ngtcp2_conn_extend_max_stream_offset(m_Conn, Credit.first, Credit.second);
		::ngtcp2_conn_extend_max_offset(m_Conn, Credit.second);
	}

	m_PendingCredits.clear();

} // ApplyPendingCredits

//-----------------------------------------------------------------------------
uint64_t KQuicConnection::GetStreamsUniLeft() const
//-----------------------------------------------------------------------------
{
	return m_Conn ? ::ngtcp2_conn_get_streams_uni_left(m_Conn) : 0;
}

//-----------------------------------------------------------------------------
uint64_t KQuicConnection::GetStreamsBidiLeft() const
//-----------------------------------------------------------------------------
{
	return m_Conn ? ::ngtcp2_conn_get_streams_bidi_left(m_Conn) : 0;
}

//-----------------------------------------------------------------------------
KStringView KQuicConnection::GetALPN() const
//-----------------------------------------------------------------------------
{
	if (!m_SSL)
	{
		return {};
	}

	const unsigned char* alpn { nullptr };
	unsigned int alpnlen { 0 };
	::SSL_get0_alpn_selected(m_SSL.get(), &alpn, &alpnlen);

	return { reinterpret_cast<const char*>(alpn), alpnlen };

} // GetALPN

//-----------------------------------------------------------------------------
KQuicConnection::Info KQuicConnection::GetInfo() const
//-----------------------------------------------------------------------------
{
	Info info;

	if (m_Conn)
	{
		ngtcp2_conn_info ci;
		::ngtcp2_conn_get_conn_info(m_Conn, &ci);

		info.LatestRTT         = FromNS(ci.latest_rtt);
		info.MinRTT            = FromNS(ci.min_rtt);
		info.SmoothedRTT       = FromNS(ci.smoothed_rtt);
		info.iCongestionWindow = ci.cwnd;
		info.iBytesInFlight    = ci.bytes_in_flight;
		info.iPacketsSent      = ci.pkt_sent;
		info.iBytesSent        = ci.bytes_sent;
		info.iPacketsReceived  = ci.pkt_recv;
		info.iBytesReceived    = ci.bytes_recv;
		info.iPacketsLost      = ci.pkt_lost;
		info.iBytesLost        = ci.bytes_lost;
	}

	return info;

} // GetInfo

//-----------------------------------------------------------------------------
int KQuicConnection::DelegateError(int iResult)
//-----------------------------------------------------------------------------
{
	if (iResult == 0)
	{
		return 0;
	}

	if (m_sDelegateError.empty())
	{
		m_sDelegateError = "delegate callback failed";
	}

	return NGTCP2_ERR_CALLBACK_FAILURE;

} // DelegateError

//-----------------------------------------------------------------------------
int KQuicConnection::OnHandshakeCompleted()
//-----------------------------------------------------------------------------
{
	m_bHandshakeCompleted = true;
	return 0;
}

//-----------------------------------------------------------------------------
int KQuicConnection::OnRecvStreamData(StreamID id, const uint8_t* data, std::size_t iSize, bool bFin)
//-----------------------------------------------------------------------------
{
	if (!m_Delegate)
	{
		// nobody wants it - give the credit back right away
		CreditReceived(id, iSize);
		return 0;
	}

	auto iConsumed = m_Delegate->OnQuicStreamData(id, KStringView(reinterpret_cast<const char*>(data), iSize), bFin);

	if (iConsumed < 0)
	{
		return DelegateError(-1);
	}

	CreditReceived(id, static_cast<std::size_t>(iConsumed));

	return 0;

} // OnRecvStreamData

//-----------------------------------------------------------------------------
int KQuicConnection::OnAckedStreamDataOffset(StreamID id, uint64_t iOffset, uint64_t iLen)
//-----------------------------------------------------------------------------
{
	return m_Delegate ? DelegateError(m_Delegate->OnQuicAckedStreamData(id, iOffset, iLen)) : 0;
}

//-----------------------------------------------------------------------------
int KQuicConnection::OnStreamOpen(StreamID id)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] opened by peer", id);
	return m_Delegate ? DelegateError(m_Delegate->OnQuicStreamOpen(id)) : 0;
}

//-----------------------------------------------------------------------------
int KQuicConnection::OnStreamClose(StreamID id, bool bHasAppErrorCode, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] closed{}", id, bHasAppErrorCode ? kFormat(" with app error {}", iAppErrorCode) : KString{});
	return m_Delegate ? DelegateError(m_Delegate->OnQuicStreamClose(id, bHasAppErrorCode, iAppErrorCode)) : 0;
}

//-----------------------------------------------------------------------------
int KQuicConnection::OnStreamReset(StreamID id, uint64_t iFinalSize, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] reset by peer, app error {}", id, iAppErrorCode);
	return m_Delegate ? DelegateError(m_Delegate->OnQuicStreamReset(id, iFinalSize, iAppErrorCode)) : 0;
}

//-----------------------------------------------------------------------------
int KQuicConnection::OnStreamStopSending(StreamID id, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] stop sending from peer, app error {}", id, iAppErrorCode);
	return m_Delegate ? DelegateError(m_Delegate->OnQuicStopSending(id, iAppErrorCode)) : 0;
}

//-----------------------------------------------------------------------------
int KQuicConnection::OnExtendMaxStreamData(StreamID id)
//-----------------------------------------------------------------------------
{
	return m_Delegate ? DelegateError(m_Delegate->OnQuicStreamUnblocked(id)) : 0;
}

//-----------------------------------------------------------------------------
int KQuicConnection::OnExtendMaxStreams(bool bBidirectional, uint64_t iMaxStreams)
//-----------------------------------------------------------------------------
{
	if (m_Delegate)
	{
		m_Delegate->OnQuicCanOpenStreams(bBidirectional, iMaxStreams);
	}

	return 0;
}

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGTCP2
