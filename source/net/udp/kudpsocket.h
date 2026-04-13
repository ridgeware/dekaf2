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

/// @file kudpsocket.h
/// provides a message-oriented UDP socket with timeout support

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/net/tcp/bits/kasio.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/time/duration/kduration.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_udp
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A message-oriented UDP socket with timeout support. Supports both
/// connected mode (Send/Receive with a default endpoint) and unconnected
/// mode (SendTo/ReceiveFrom with per-call endpoints), as well as multicast.
class DEKAF2_PUBLIC KUDPSocket : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using asio_socket_type = boost::asio::ip::udp::socket;
	using endpoint_type    = boost::asio::ip::udp::endpoint;
#if (DEKAF2_CLASSIC_ASIO)
	using native_socket_type = boost::asio::basic_socket<boost::asio::ip::udp, boost::asio::datagram_socket_service<boost::asio::ip::udp>>::native_handle_type;
#else
	using native_socket_type = boost::asio::basic_socket<boost::asio::ip::udp>::native_handle_type;
#endif

	static constexpr std::size_t s_iDefaultMaxDatagramSize = 1472; // 1500 MTU - 20 IP - 8 UDP

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected and unbound UDP socket.
	/// @param Timeout
	/// Timeout for any I/O. Defaults to 15 seconds.
	KUDPSocket(KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected UDP socket (client mode).
	/// @param Endpoint
	/// The remote endpoint to connect to
	/// @param Options
	/// set options like IPv4 or IPv6, and the timeout
	KUDPSocket(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KUDPSocket();
	//-----------------------------------------------------------------------------

	KUDPSocket(const KUDPSocket&) = delete;
	KUDPSocket& operator=(const KUDPSocket&) = delete;
	KUDPSocket(KUDPSocket&&) = delete;
	KUDPSocket& operator=(KUDPSocket&&) = delete;

	// --- connected mode (call Connect() first, or use the endpoint ctor) ---

	//-----------------------------------------------------------------------------
	/// Connect to a remote endpoint (sets the default destination for Send/Receive).
	/// @param Endpoint
	/// The remote endpoint to connect to
	/// @param Options
	/// set options like IPv4 or IPv6, and the timeout
	/// @return true on success
	bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect (remove the default endpoint association).
	/// @return true on success
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send data to the connected endpoint.
	/// @param sData the data to send
	/// @return the number of bytes sent, or -1 on error
	std::streamsize Send(KStringView sData);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send data to the connected endpoint.
	/// @param pBuffer pointer to the data buffer
	/// @param iCount number of bytes to send
	/// @return the number of bytes sent, or -1 on error
	std::streamsize Send(const void* pBuffer, std::streamsize iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Receive a datagram from the connected endpoint.
	/// @param pBuffer pointer to the receive buffer
	/// @param iCount maximum number of bytes to receive
	/// @return the number of bytes received, or -1 on error
	std::streamsize Receive(void* pBuffer, std::streamsize iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Receive a datagram from the connected endpoint into a string.
	/// @param iMaxSize maximum datagram size to receive, defaults to max UDP payload
	/// @return the received data as a KString (empty on error)
	KString Receive(std::size_t iMaxSize = 65507);
	//-----------------------------------------------------------------------------

	// --- unconnected mode ---

	//-----------------------------------------------------------------------------
	/// Bind to a local port for receiving datagrams.
	/// @param iPort the local port to bind to
	/// @param Family address family (Any, IPv4, IPv6)
	/// @return true on success
	bool Bind(uint16_t iPort, KStreamOptions::Family Family = KStreamOptions::Family::Any);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send a datagram to a specific endpoint.
	/// @param sData the data to send
	/// @param Endpoint the destination endpoint
	/// @return the number of bytes sent, or -1 on error
	std::streamsize SendTo(KStringView sData, const endpoint_type& Endpoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send a datagram to a specific endpoint.
	/// @param pBuffer pointer to the data buffer
	/// @param iCount number of bytes to send
	/// @param Endpoint the destination endpoint
	/// @return the number of bytes sent, or -1 on error
	std::streamsize SendTo(const void* pBuffer, std::streamsize iCount, const endpoint_type& Endpoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Receive a datagram, returning the sender's endpoint.
	/// @param pBuffer pointer to the receive buffer
	/// @param iCount maximum number of bytes to receive
	/// @param Endpoint [out] will be set to the sender's endpoint
	/// @return the number of bytes received, or -1 on error
	std::streamsize ReceiveFrom(void* pBuffer, std::streamsize iCount, endpoint_type& Endpoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Receive a datagram into a string, returning the sender's endpoint.
	/// @param iMaxSize maximum datagram size to receive, defaults to max UDP payload
	/// @param Endpoint [out] will be set to the sender's endpoint
	/// @return the received data as a KString (empty on error)
	KString ReceiveFrom(std::size_t iMaxSize, endpoint_type& Endpoint);
	//-----------------------------------------------------------------------------

	// --- multicast ---

	//-----------------------------------------------------------------------------
	/// Join a multicast group.
	/// @param sMulticastAddress the multicast group address
	/// @return true on success
	bool JoinMulticastGroup(KStringView sMulticastAddress);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Leave a multicast group.
	/// @param sMulticastAddress the multicast group address
	/// @return true on success
	bool LeaveMulticastGroup(KStringView sMulticastAddress);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the multicast TTL (time to live / hop count).
	/// @param iTTL the TTL value (default 1 = local subnet)
	/// @return true on success
	bool SetMulticastTTL(uint8_t iTTL);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Enable or disable multicast loopback (receiving own multicast packets).
	/// @param bEnable true to enable loopback
	/// @return true on success
	bool SetMulticastLoopback(bool bEnable);
	//-----------------------------------------------------------------------------

	// --- state and options ---

	//-----------------------------------------------------------------------------
	/// Is the socket open?
	bool is_open() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Is the socket in a good state?
	bool Good() const { return m_ec.value() == 0; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout.
	/// @param Timeout the new timeout
	void SetTimeout(KDuration Timeout) { m_Timeout = Timeout; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the current I/O timeout.
	KDuration GetTimeout() const { return m_Timeout; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the maximum datagram size for the stream adapter.
	/// @param iSize the maximum payload size per datagram
	void SetMaxDatagramSize(std::size_t iSize) { m_iMaxDatagramSize = iSize; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the maximum datagram size.
	std::size_t GetMaxDatagramSize() const { return m_iMaxDatagramSize; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Enable or disable the SO_BROADCAST socket option.
	/// @param bEnable true to enable broadcast
	/// @return true on success
	bool SetBroadcast(bool bEnable);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the underlying ASIO socket.
	asio_socket_type& GetAsioSocket() { return m_Socket; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the native OS socket handle.
	native_socket_type GetNativeSocket();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the endpoint we are connected or bound to.
	const KTCPEndPoint& GetEndPoint() const { return m_Endpoint; }
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	void ResetTimer();
	void ClearTimer();
	void CheckTimer();
	void RunTimed();
	bool OpenSocket(KStreamOptions::Family Family);

	boost::asio::io_service m_IOService { 1 };
	asio_socket_type        m_Socket    { m_IOService };
#if DEKAF2_CLASSIC_ASIO
	boost::asio::deadline_timer m_Timer { m_IOService };
#else
	boost::asio::steady_timer   m_Timer { m_IOService };
#endif
	boost::system::error_code   m_ec;
	KDuration                   m_Timeout;
	std::size_t                 m_iMaxDatagramSize { s_iDefaultMaxDatagramSize };
	KTCPEndPoint                m_Endpoint;

}; // KUDPSocket

/// @}

DEKAF2_NAMESPACE_END
