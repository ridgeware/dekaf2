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

/// @file kdtlsserver.h
/// provides a DTLS server with per-peer session management and cookie exchange

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/net/tls/ktlscontext.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/time/duration/kduration.h>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>

typedef struct ssl_st SSL;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_udp
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A DTLS server. Listens on a UDP port, manages per-peer DTLS sessions with
/// cookie exchange for DoS protection, and dispatches decrypted datagrams to
/// a callback.
class DEKAF2_PUBLIC KDTLSServer : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Callback for received decrypted datagrams.
	/// @param sData the decrypted datagram content
	/// @param sPeerAddress the peer's address as "ip:port"
	/// @param SSL the peer's SSL handle (for sending responses via SSL_write)
	using DatagramCallback = std::function<void(KStringView sData, KStringView sPeerAddress, SSL* SSL)>;

	//-----------------------------------------------------------------------------
	/// Construct a DTLS server.
	/// @param iPort the port to bind to
	/// @param bStoreNewCerts if new (ephemeral) certs are created, should they be persisted?
	KDTLSServer(uint16_t iPort, bool bStoreNewCerts = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KDTLSServer();
	//-----------------------------------------------------------------------------

	KDTLSServer(const KDTLSServer&) = delete;
	KDTLSServer& operator=(const KDTLSServer&) = delete;
	KDTLSServer(KDTLSServer&&) = delete;
	KDTLSServer& operator=(KDTLSServer&&) = delete;

	//-----------------------------------------------------------------------------
	/// Load TLS certificates from files (.pem format).
	bool LoadTLSCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set TLS certificates from strings (.pem format).
	bool SetTLSCertificates(KStringView sCert, KStringView sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Start the DTLS server.
	/// @param Callback function to call for each decrypted datagram
	/// @param bBlock if true, blocks until Stop() is called
	/// @return true on success
	bool Start(DatagramCallback Callback, bool bBlock = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Stop the server.
	bool Stop();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Is the server running?
	bool IsRunning() const { return m_bRunning; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the port we are bound to.
	uint16_t GetPort() const { return m_iPort; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the TLS context (e.g. for additional configuration).
	KTLSContext& GetTLSContext() { return m_TLSContext; }
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	DEKAF2_PRIVATE
	void RunLoop(DatagramCallback Callback);

	KTLSContext                   m_TLSContext { true, KTLSContext::Transport::DTls };
	std::unique_ptr<std::thread>  m_Thread;
	int                           m_Socket      { -1    };
	uint16_t                      m_iPort       { 0     };
	std::atomic<bool>             m_bRunning    { false };
	std::atomic<bool>             m_bQuit       { false };

}; // KDTLSServer

/// @}

DEKAF2_NAMESPACE_END
