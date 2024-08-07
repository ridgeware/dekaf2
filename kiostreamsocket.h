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

#include "kdefinitions.h"
#include "kstringview.h"
#include "kstreamoptions.h"
#include "kurl.h"
#include "kerror.h"
#include "bits/kasio.h"
#include <iostream>

#if DEKAF2_IS_WINDOWS
	#include <winsock2.h>
#else
	#include <poll.h>
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// base class for the std::iostream based internet stream classes
class KIOStreamSocket : public KErrorBase, public KReaderWriter<std::iostream>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KReaderWriter<std::iostream>;

//----------
public:
//----------

	using native_tls_handle_type     = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::native_handle_type;
#if (DEKAF2_CLASSIC_ASIO)
	using native_socket_type         = boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::stream_socket_service<boost::asio::ip::tcp>>::native_handle_type;
	using resolver_results_tcp_type  = boost::asio::ip::tcp::resolver::iterator;
	using resolver_endpoint_tcp_type = resolver_results_tcp_type;
	using resolver_results_udp_type  = boost::asio::ip::udp::resolver::iterator;
	using resolver_endpoint_udp_type = resolver_results_udp_type;
#else
	using native_socket_type         = boost::asio::basic_socket<boost::asio::ip::tcp>::native_handle_type;
	using resolver_results_tcp_type  = boost::asio::ip::tcp::resolver::results_type;
	using resolver_endpoint_tcp_type = boost::asio::ip::tcp::resolver::endpoint_type;
	using resolver_results_udp_type  = boost::asio::ip::udp::resolver::results_type;
	using resolver_endpoint_udp_type = boost::asio::ip::udp::resolver::endpoint_type;
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

	virtual bool is_open() const = 0;

	/// tests for a closed connection of the remote side by trying to peek one byte
	virtual bool IsDisconnected() = 0;

	/// Gets the underlying OS level native socket of the stream
	virtual native_socket_type GetNativeSocket() = 0;

	virtual bool Good() const = 0;

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
	bool IsReadReady()                           { return CheckIfReady(POLLIN,  m_Timeout); }
	/// can we read from this stream? Returns with false after specified timeout
	bool IsReadReady(KDuration Timeout)          { return CheckIfReady(POLLIN,  Timeout  ); }
	/// can we write to this stream? Returns with false after general timeout
	bool IsWriteReady()                          { return CheckIfReady(POLLOUT, m_Timeout); }
	/// can we write to this stream? Returns with false after specified timeout
	bool IsWriteReady(KDuration Timeout)         { return CheckIfReady(POLLOUT, Timeout  ); }
	/// check any ::poll() flag with the general timeout
	bool CheckIfReady(int what)                  { return CheckIfReady(what,    m_Timeout); }
	/// check any ::poll() flag with the specified timeout
	bool CheckIfReady(int what, KDuration Timeout);

	// ------ static factory methods -------

	// this interface uses KURL instead of KTCPEndPoint to allow construction like "https://www.abc.de" - otherwise the protocol would be lost..
	/// create a stream socket according to protocol and options
	static std::unique_ptr<KIOStreamSocket> 
	Create(const KURL& URL, bool bForceTLS = false, KStreamOptions Options = KStreamOptions::None);
	/// create a stream socket around any std::iostream
	static std::unique_ptr<KIOStreamSocket> 
	Create(std::iostream& IOStream);

	// ------ static resolver methods -------

	/// lookup a hostname, returns list of found addresses
	static resolver_results_tcp_type 
	ResolveTCP(
		KStringViewZ sHostname,
		uint16_t iPort,
		KStreamOptions::Family Family,
		boost::asio::io_service& IOService,
		boost::system::error_code& ec
	);

	/// lookup a hostname, returns list of found addresses
	static resolver_results_udp_type 
	ResolveUDP(
		KStringViewZ sHostname,
		uint16_t iPort,
		KStreamOptions::Family Family,
		boost::asio::io_service& IOService,
		boost::system::error_code& ec
	);

	/// reverse lookup an IP address, returns list of found addresses
	static resolver_results_tcp_type 
	ReverseLookup(
		KStringViewZ sIPAddr,
		boost::asio::io_service& IOService,
		boost::system::error_code& ec
	);

	/// print one endpoint address into a string
	static KString 
	PrintResolvedAddress(
#if (DEKAF2_CLASSIC_ASIO)
		resolver_endpoint_tcp_type endpoint
#else
		const resolver_endpoint_tcp_type& endpoint
#endif
	);

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

//----------
private:
//----------

	KDuration    m_Timeout;
	KTCPEndPoint m_UnresolvedEndpoint;
	KTCPEndPoint m_EndpointAddress;

}; // KIOStreamSocket



//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// adaptor class to wrap any std::iostream into a KIOStreamSocket
class KIOStreamSocketAdaptor : public KIOStreamSocket
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

DEKAF2_NAMESPACE_END
