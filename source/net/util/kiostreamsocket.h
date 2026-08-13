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

/// @file kiostreamsocket.h
/// provides common stream methods for all internet streams

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/net/util/kpoll.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/net/tcp/bits/kasio.h>
#include <atomic>
#include <iostream>

#if DEKAF2_IS_WINDOWS
	#include <winsock2.h>
#else
	#include <poll.h>
#endif

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_util
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// base class for the std::iostream based internet stream classes
///
/// ### One thread at a time
/// A stream may be driven by ONE thread at any point in time - which thread
/// that is may change, with proper synchronization on the handover. Reads
/// and writes share a single event loop and a single completion state, so
/// even one reader plus one writer in parallel consume each other's
/// completions - the losing operation is silently dropped, and its
/// completion may scribble over a stack frame that has already returned.
/// Concurrent operations trip an assert in debug builds and a kWarning in
/// release builds.
/// The one sanctioned cross-thread call is Disconnect() (or
/// SignalDisconnecting()), to unblock a thread that sits in a read.
///
/// For protocols with server side push (one thread mostly reading, others
/// occasionally writing) do not share one stream: either use one connection
/// per direction, or a single thread that multiplexes over IsReadReady() -
/// see KStreamOptions::CancelOnTimeout for repeatable read timeouts.
class DEKAF2_PUBLIC KIOStreamSocket : public KErrorBase, public KReaderWriter<std::iostream>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KReaderWriter<std::iostream>;

//----------
public:
//----------

	using native_tls_handle_type     = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::native_handle_type;
#if (DEKAF2_CLASSIC_ASIO)
	using native_socket_type         = boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::stream_socket_service<boost::asio::ip::tcp>>::native_handle_type;
#else
	using native_socket_type         = boost::asio::basic_socket<boost::asio::ip::tcp>::native_handle_type;
#endif

	KIOStreamSocket(std::streambuf* StreamBuf, KDuration Timeout = KStreamOptions::GetDefaultTimeout())
	: base_type(StreamBuf), m_Timeout(Timeout) {}
	
	// ------ the pure virtual methods to be implemented by each child -------

	/// Connects a given server as a client.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	virtual bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{}) = 0;

	/// Disconnect the stream
	virtual bool Disconnect() = 0;

	/// is this stream open
	virtual bool is_open() const = 0;

	/// tests for a closed connection of the remote side by trying to peek one byte
	virtual bool IsDisconnected() = 0;

	/// gets the underlying OS level native socket of the stream
	virtual native_socket_type GetNativeSocket() = 0;

	/// is this stream good for reading and writing?
	virtual bool Good() const = 0;

	/// read directly from the stream, not using the std::streambuf hierarchy - returns only as many characters as
	/// are immediately available - do not mix these reads with streambuf based reads, they are not synchronized
	virtual std::streamsize direct_read_some(void* sBuffer, std::streamsize iCount) = 0;

	/// Check if this stream is currently in the process of being disconnected
	/// (from any thread). Returns true after a Disconnect() started - on any
	/// thread - and before the optional state reset on a reconnect.
	///
	/// An event loop must ask this before it polls the stream's descriptor: a
	/// clean Disconnect() leaves no error behind, so Good() may still be true,
	/// while the descriptor is closed and its number may already have been
	/// handed to another socket.
	bool IsDisconnecting() const
	{
		return m_bDisconnecting.load(std::memory_order_acquire);
	}

	/// Write directly to the stream, not using the std::streambuf hierarchy - writes only as many characters as
	/// the stream accepts right now and returns that count, which may be less than iCount (or 0). Do not mix
	/// these writes with streambuf based writes, they are not synchronized.
	///
	/// This is the write counterpart of direct_read_some(), and the primitive for event loops that carry both
	/// directions on one thread: unlike Write()/kWrite(), which loop until everything is out (and flag a short
	/// write as a stream error), a partial write here is a normal result - keep the remainder and come back
	/// after the next poll. That keeps a stalled peer from blocking the reading direction.
	virtual std::streamsize direct_write_some(const void* sBuffer, std::streamsize iCount) = 0;

	// ------ the virtual methods that can be implemented by a child -------

	/// Gets the underlying openssl handle of the stream
	virtual native_tls_handle_type GetNativeTLSHandle();

	/// Is this a requested TLS connection (this does not mean that TLS is in action right now
	virtual bool IsTLS() const { return false; };

	/// Upgrade connection from TCP to TCP over TLS. Returns true on success. Can also
	/// be used to force a handshake before any IO is triggered.
	virtual bool StartManualTLSHandshake();

	/// Switch to manual handshake, only possible before any data has been read or
	/// written
	virtual bool SetManualTLSHandshake(bool bYes);

	/// Set the endpoint address when in server mode
	virtual void SetConnectedEndPointAddress(const KTCPEndPoint& Endpoint);

	virtual ~KIOStreamSocket();

	// ------ non-virtual base class methods -------

	/// Set I/O timeout.
	bool SetTimeout(KDuration timeout)
	{
		m_Timeout = timeout;
		return Timeout(timeout);
	}

	/// Set TCP_NODELAY (disable Nagle's algorithm) on the underlying socket
	/// @param bYesNo true to disable Nagle's algorithm
	/// @returns false on failure
	bool SetNoDelay(bool bYesNo)
	{
		return kSetTCPNoDelay(GetNativeSocket(), bYesNo);
	}

	/// Get the I/O timeout
	KDuration GetTimeout() const { return m_Timeout; }

	/// std::iostream interface to open a stream. Delegates to Connect()
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	bool open(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{})
	{
		return Connect(Endpoint, Options);
	}

	/// returns the unresolved endpoint (= the host name given to Connect())
	const KTCPEndPoint& GetEndPoint() const 
	{
		return m_UnresolvedEndpoint;
	}

	/// returns the connected endpoint (= the ip address or the unix socket after Connect())
	const KTCPEndPoint& GetEndPointAddress() const
	{
		return m_EndpointAddress;
	}

	/// set the unresolved endpoint, only useful if this stream is proxied to a new endpoint, otherwise don't touch
	void SetProxiedEndPoint(KTCPEndPoint Endpoint) 
	{
		SetUnresolvedEndPoint(std::move(Endpoint));
	}

	/// Disconnect the stream
	bool close() { return Disconnect(); }

	/// For TLS and Quic streams: Set the ALPN data. This API expects a vector of KStringViews and transforms it into the internal
	/// ALPN format.
	/// This method is mutually exclusive with SetAllowHTTP2()
	bool SetALPN(const std::vector<KStringView> ALPNs)
	{
		return SetALPNRaw(KStreamOptions::CreateALPNString(ALPNs));
	}

	/// For TLS and Quic streams: Set the ALPN data. This API expects a string view and transforms it into the internal
	/// ALPN format.
	/// This method is mutually exclusive with SetAllowHTTP2()
	bool SetALPN(KStringView sALPN)
	{
		return SetALPNRaw(KStreamOptions::CreateALPNString(sALPN));
	}
	
	/// For TLS and Quic streams: Get the Application Layer Protocol Negotiation after the TLS handshake
	KStringView GetALPN();

	/// can we read from this stream? Returns with false after general timeout
	bool IsReadReady()                           { return CheckIfReady(POLLIN,  m_Timeout, true); }
	/// can we read from this stream? Returns with false after specified timeout
	bool IsReadReady(KDuration Timeout, bool bTimeoutIsAnError = false)
	                                { return CheckIfReady(POLLIN,  Timeout , bTimeoutIsAnError ); }
	/// can we write to this stream? Returns with false after general timeout
	bool IsWriteReady()                          { return CheckIfReady(POLLOUT, m_Timeout, true); }
	/// can we write to this stream? Returns with false after specified timeout
	bool IsWriteReady(KDuration Timeout, bool bTimeoutIsAnError = false)
	                                { return CheckIfReady(POLLOUT, Timeout , bTimeoutIsAnError ); }
	/// check any ::poll() flag with the general timeout - returns 0 or the event(s) that triggered
	int CheckIfReady(int what)                   { return CheckIfReady(what,    m_Timeout, true); }
	/// check any ::poll() flag with the specified timeout - returns 0 or the event(s) that triggered
	int CheckIfReady(int what, KDuration Timeout, bool bTimeoutIsAnError = false);

	// ------ static factory methods -------

	// this interface uses KURL instead of KTCPEndPoint to allow construction like "https://www.abc.de" - otherwise the protocol would be lost..
	/// create a stream socket according to protocol and options
	static std::unique_ptr<KIOStreamSocket> 
	Create(const KURL& URL, bool bForceTLS = false, KStreamOptions Options = KStreamOptions::None);
	/// create a stream socket around any std::iostream
	static std::unique_ptr<KIOStreamSocket> 
	Create(std::iostream& IOStream);

//----------
protected:
//----------

	// ------ service methods for children -------

	/// query the last ssl error description and set it as error
	bool SetSSLError();

	/// sets the prepared sALPN data for the TLS or Quic stream
	bool SetALPNRaw(KStringView sALPN);

	/// hook for children to update a changed timeout, may be needed if timing is not controlled by this base class
	virtual bool Timeout(KDuration Timeout);

	/// set the original, unresolved end point as given to the constructor or to Connect()
	void SetUnresolvedEndPoint(KTCPEndPoint Endpoint) 
	{
		m_UnresolvedEndpoint = std::move(Endpoint);
	}
	/// set the connected IP address
	void SetEndPointAddress(KTCPEndPoint Endpoint)
	{
		m_EndpointAddress = std::move(Endpoint);
	}

	/// Get the poll interruptor to wake up blocking poll() calls
	/// Used by derived classes to interrupt poll when disconnecting
	KPollInterruptor& GetPollInterruptor() { return m_Interruptor; }

	/// Signal that the stream is being disconnected and wake any blocked
	/// poll() calls. This is the entry point derived classes must call at
	/// the beginning of their Disconnect() implementation. It sets an
	/// atomic flag that is checked by CheckIfReady() to prevent callers
	/// from re-entering poll() between Wake() and the actual socket close.
	/// Idempotent - safe to call multiple times.
	void SignalDisconnecting()
	{
		m_bDisconnecting.store(true, std::memory_order_release);
		m_Interruptor.Wake();
	}

	/// Reset the disconnecting state. Derived classes should call this
	/// at the beginning of a successful Connect() on a previously used
	/// stream to allow polling again on the reconnected socket.
	void ResetDisconnectingState()
	{
		m_bDisconnecting.store(false, std::memory_order_release);
	}

//----------
private:
//----------

	KDuration             m_Timeout;
	KTCPEndPoint          m_UnresolvedEndpoint;
	KTCPEndPoint          m_EndpointAddress;
	KPollInterruptor      m_Interruptor;     ///< Used to wake poll() from another thread
	std::atomic<bool>     m_bDisconnecting { false }; ///< Set by SignalDisconnecting() to abort in-flight and future poll()s

}; // KIOStreamSocket



//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// adaptor class to wrap any std::iostream into a KIOStreamSocket
class DEKAF2_PUBLIC KIOStreamSocketAdaptor : public KIOStreamSocket
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KIOStreamSocket;

//----------
public:
//----------

	KIOStreamSocketAdaptor(std::iostream& IOStream) : base_type(IOStream.rdbuf()) {}

	// implement dummies for all abstract methods
	virtual bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{}) override final { return false; };
	virtual bool Disconnect() override final { return false; }
	virtual bool is_open() const override final { return good(); }
	virtual bool IsDisconnected() override final { return !is_open(); }
	virtual native_socket_type GetNativeSocket() override final { return -1; }
	virtual bool Good() const override final { return good(); }
	virtual std::streamsize direct_read_some(void* sBuffer, std::streamsize iCount) override final { return 0; };
	virtual std::streamsize direct_write_some(const void* sBuffer, std::streamsize iCount) override final { return 0; };

}; // KStreamSocketAdaptor

// helper for old code expecting KConnection::Create()
namespace KConnection
{
inline std::unique_ptr<KIOStreamSocket>
Create(const KURL& URL, bool bForceTLS = false, KStreamOptions Options = KStreamOptions::None)
{
	return KIOStreamSocket::Create(URL, bForceTLS, Options);
}

} // end of namespace KConnection


/// @}

DEKAF2_NAMESPACE_END
