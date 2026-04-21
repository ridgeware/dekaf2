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

#include <dekaf2/net/udp/kudpsocket.h>
#include <dekaf2/net/address/kresolve.h>
#include <dekaf2/core/logging/klog.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KUDPSocket::KUDPSocket(KDuration Timeout)
//-----------------------------------------------------------------------------
: m_Timeout(Timeout)
{
	ClearTimer();
	CheckTimer();
}

//-----------------------------------------------------------------------------
KUDPSocket::KUDPSocket(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
: m_Timeout(Options.GetTimeout())
{
	ClearTimer();
	CheckTimer();
	Connect(Endpoint, Options);
}

//-----------------------------------------------------------------------------
KUDPSocket::~KUDPSocket()
//-----------------------------------------------------------------------------
{
	Disconnect();
}

//-----------------------------------------------------------------------------
bool KUDPSocket::OpenSocket(KStreamOptions::Family Family)
//-----------------------------------------------------------------------------
{
	if (m_Socket.is_open())
	{
		return true;
	}

	boost::system::error_code ec;

	switch (Family)
	{
		case KStreamOptions::Family::IPv4:
			m_Socket.open(boost::asio::ip::udp::v4(), ec);
			break;

		case KStreamOptions::Family::IPv6:
			m_Socket.open(boost::asio::ip::udp::v6(), ec);
			break;

		case KStreamOptions::Family::Any:
		default:
			m_Socket.open(boost::asio::ip::udp::v6(), ec);

			if (!ec)
			{
				// enable dual-stack (accept both IPv4 and IPv6)
				boost::asio::ip::v6_only v6only(false);
				m_Socket.set_option(v6only, ec);

				if (ec)
				{
					kDebug(2, "cannot set dual-stack on UDP socket: {}", ec.message());
					ec.clear();
				}
			}
			break;
	}

	if (ec)
	{
		return SetError(kFormat("cannot open UDP socket: {}", ec.message()));
	}

	return true;

} // OpenSocket

//-----------------------------------------------------------------------------
bool KUDPSocket::Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options)
//-----------------------------------------------------------------------------
{
	m_Timeout = Options.GetTimeout();
	m_Endpoint = Endpoint;

	auto& sHostname = Endpoint.Domain.get();

	auto hosts = KResolve::ResolveUDP(sHostname, Endpoint.Port.get(), Options.GetFamily(), m_IOService, m_ec);

	if (!Good())
	{
		return SetError(kFormat("cannot resolve {}: {}", Endpoint, m_ec.message()));
	}

	// get the first resolved endpoint to determine the protocol
#if (DEKAF2_CLASSIC_ASIO)
	// Boost < 1.66: resolve() returns a forward iterator directly; the range
	// is terminated by a default-constructed iterator.
	auto it = hosts;
	decltype(it) ie;
#else
	auto it = hosts.begin();
	auto ie = hosts.end();
#endif

	if (it == ie)
	{
		return SetError(kFormat("no addresses found for {}", Endpoint));
	}

	// basic_resolver_entry::endpoint() exists in both classic (Boost < 1.66)
	// and modern asio; dereferencing the iterator directly yields the entry,
	// not the endpoint.
	auto resolved_endpoint = it->endpoint();

	// open socket with matching protocol
	if (resolved_endpoint.protocol() == boost::asio::ip::udp::v4())
	{
		if (!OpenSocket(KStreamOptions::Family::IPv4))
		{
			return false;
		}
	}
	else
	{
		if (!OpenSocket(KStreamOptions::Family::IPv6))
		{
			return false;
		}
	}

	m_Socket.async_connect(resolved_endpoint,
	[&](const boost::system::error_code& ec)
	{
		m_ec = ec;
	});

	kDebug(3, "trying to connect UDP to {}", Endpoint);

	RunTimed();

	if (!Good() || !m_Socket.is_open())
	{
		return SetError(kFormat("cannot connect UDP to {}: {}", Endpoint, m_ec.message()));
	}

	kDebug(2, "UDP connected to {}", Endpoint);

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KUDPSocket::Disconnect()
//-----------------------------------------------------------------------------
{
	if (m_Socket.is_open())
	{
		boost::system::error_code ec;

		m_Socket.shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);

		if (ec && ec.value() != boost::asio::error::not_connected)
		{
			kDebug(2, "error shutting down UDP socket: {}", ec.message());
		}

		m_Socket.close(ec);

		if (ec)
		{
			kDebug(2, "error closing UDP socket: {}", ec.message());
			return false;
		}

		kDebug(2, "UDP disconnected from: {}", m_Endpoint);
	}

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
bool KUDPSocket::Bind(uint16_t iPort, KStreamOptions::Family Family)
//-----------------------------------------------------------------------------
{
	if (!OpenSocket(Family))
	{
		return false;
	}

	boost::system::error_code ec;

	// allow address reuse for multicast
	m_Socket.set_option(boost::asio::socket_base::reuse_address(true), ec);

	endpoint_type ep;

	switch (Family)
	{
		case KStreamOptions::Family::IPv4:
			ep = endpoint_type(boost::asio::ip::address_v4::any(), iPort);
			break;

		case KStreamOptions::Family::IPv6:
		case KStreamOptions::Family::Any:
		default:
			ep = endpoint_type(boost::asio::ip::address_v6::any(), iPort);
			break;
	}

	m_Socket.bind(ep, ec);

	if (ec)
	{
		return SetError(kFormat("cannot bind UDP socket to port {}: {}", iPort, ec.message()));
	}

	kDebug(2, "UDP socket bound to port {}", iPort);

	return true;

} // Bind

//-----------------------------------------------------------------------------
std::streamsize KUDPSocket::Send(const void* pBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	std::streamsize iSent { 0 };

	m_Socket.async_send(boost::asio::buffer(pBuffer, iCount),
	[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		m_ec  = ec;
		iSent = bytes_transferred;
	});

	RunTimed();

	if (!Good())
	{
		SetError(kFormat("cannot send UDP data: {}", m_ec.message()));
		return -1;
	}

	return iSent;

} // Send

//-----------------------------------------------------------------------------
std::streamsize KUDPSocket::Send(KStringView sData)
//-----------------------------------------------------------------------------
{
	return Send(sData.data(), static_cast<std::streamsize>(sData.size()));

} // Send

//-----------------------------------------------------------------------------
std::streamsize KUDPSocket::Receive(void* pBuffer, std::streamsize iCount)
//-----------------------------------------------------------------------------
{
	std::streamsize iReceived { 0 };

	m_Socket.async_receive(boost::asio::buffer(pBuffer, iCount),
	[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		m_ec      = ec;
		iReceived = bytes_transferred;
	});

	RunTimed();

	if (!Good())
	{
		if (m_ec.value() != boost::asio::error::eof)
		{
			SetError(kFormat("cannot receive UDP data: {}", m_ec.message()));
		}
		return -1;
	}

	return iReceived;

} // Receive

//-----------------------------------------------------------------------------
KString KUDPSocket::Receive(std::size_t iMaxSize)
//-----------------------------------------------------------------------------
{
	KString sBuffer;
	sBuffer.resize_uninitialized(iMaxSize);

	auto iReceived = Receive(sBuffer.data(), static_cast<std::streamsize>(iMaxSize));

	if (iReceived < 0)
	{
		return {};
	}

	sBuffer.resize(static_cast<std::size_t>(iReceived));

	return sBuffer;

} // Receive

//-----------------------------------------------------------------------------
std::streamsize KUDPSocket::SendTo(const void* pBuffer, std::streamsize iCount, const endpoint_type& Endpoint)
//-----------------------------------------------------------------------------
{
	if (!OpenSocket(Endpoint.protocol() == boost::asio::ip::udp::v4()
	                ? KStreamOptions::Family::IPv4
	                : KStreamOptions::Family::IPv6))
	{
		return -1;
	}

	std::streamsize iSent { 0 };

	m_Socket.async_send_to(boost::asio::buffer(pBuffer, iCount), Endpoint,
	[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		m_ec  = ec;
		iSent = bytes_transferred;
	});

	RunTimed();

	if (!Good())
	{
		SetError(kFormat("cannot send UDP data to {}:{}: {}", Endpoint.address().to_string(), Endpoint.port(), m_ec.message()));
		return -1;
	}

	return iSent;

} // SendTo

//-----------------------------------------------------------------------------
std::streamsize KUDPSocket::SendTo(KStringView sData, const endpoint_type& Endpoint)
//-----------------------------------------------------------------------------
{
	return SendTo(sData.data(), static_cast<std::streamsize>(sData.size()), Endpoint);

} // SendTo

//-----------------------------------------------------------------------------
std::streamsize KUDPSocket::ReceiveFrom(void* pBuffer, std::streamsize iCount, endpoint_type& Endpoint)
//-----------------------------------------------------------------------------
{
	std::streamsize iReceived { 0 };

	m_Socket.async_receive_from(boost::asio::buffer(pBuffer, iCount), Endpoint,
	[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
	{
		m_ec      = ec;
		iReceived = bytes_transferred;
	});

	RunTimed();

	if (!Good())
	{
		if (m_ec.value() != boost::asio::error::eof)
		{
			SetError(kFormat("cannot receive UDP data: {}", m_ec.message()));
		}
		return -1;
	}

	return iReceived;

} // ReceiveFrom

//-----------------------------------------------------------------------------
KString KUDPSocket::ReceiveFrom(std::size_t iMaxSize, endpoint_type& Endpoint)
//-----------------------------------------------------------------------------
{
	KString sBuffer;
	sBuffer.resize_uninitialized(iMaxSize);

	auto iReceived = ReceiveFrom(sBuffer.data(), static_cast<std::streamsize>(iMaxSize), Endpoint);

	if (iReceived < 0)
	{
		return {};
	}

	sBuffer.resize(static_cast<std::size_t>(iReceived));

	return sBuffer;

} // ReceiveFrom

//-----------------------------------------------------------------------------
bool KUDPSocket::JoinMulticastGroup(KStringView sMulticastAddress)
//-----------------------------------------------------------------------------
{
	boost::system::error_code ec;

#if (DEKAF2_CLASSIC_ASIO)
	// Boost < 1.66 does not have boost::asio::ip::make_address; the legacy
	// name is boost::asio::ip::address::from_string.
	auto address = boost::asio::ip::address::from_string(KString(sMulticastAddress).c_str(), ec);
#else
	auto address = boost::asio::ip::make_address(KString(sMulticastAddress).c_str(), ec);
#endif

	if (ec)
	{
		return SetError(kFormat("invalid multicast address '{}': {}", sMulticastAddress, ec.message()));
	}

	m_Socket.set_option(boost::asio::ip::multicast::join_group(address), ec);

	if (ec)
	{
		return SetError(kFormat("cannot join multicast group '{}': {}", sMulticastAddress, ec.message()));
	}

	kDebug(2, "joined multicast group {}", sMulticastAddress);

	return true;

} // JoinMulticastGroup

//-----------------------------------------------------------------------------
bool KUDPSocket::LeaveMulticastGroup(KStringView sMulticastAddress)
//-----------------------------------------------------------------------------
{
	boost::system::error_code ec;

#if (DEKAF2_CLASSIC_ASIO)
	auto address = boost::asio::ip::address::from_string(KString(sMulticastAddress).c_str(), ec);
#else
	auto address = boost::asio::ip::make_address(KString(sMulticastAddress).c_str(), ec);
#endif

	if (ec)
	{
		return SetError(kFormat("invalid multicast address '{}': {}", sMulticastAddress, ec.message()));
	}

	m_Socket.set_option(boost::asio::ip::multicast::leave_group(address), ec);

	if (ec)
	{
		return SetError(kFormat("cannot leave multicast group '{}': {}", sMulticastAddress, ec.message()));
	}

	kDebug(2, "left multicast group {}", sMulticastAddress);

	return true;

} // LeaveMulticastGroup

//-----------------------------------------------------------------------------
bool KUDPSocket::SetMulticastTTL(uint8_t iTTL)
//-----------------------------------------------------------------------------
{
	boost::system::error_code ec;

	m_Socket.set_option(boost::asio::ip::multicast::hops(iTTL), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set multicast TTL: {}", ec.message()));
	}

	return true;

} // SetMulticastTTL

//-----------------------------------------------------------------------------
bool KUDPSocket::SetMulticastLoopback(bool bEnable)
//-----------------------------------------------------------------------------
{
	boost::system::error_code ec;

	m_Socket.set_option(boost::asio::ip::multicast::enable_loopback(bEnable), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set multicast loopback: {}", ec.message()));
	}

	return true;

} // SetMulticastLoopback

//-----------------------------------------------------------------------------
bool KUDPSocket::is_open() const
//-----------------------------------------------------------------------------
{
	return m_Socket.is_open();

} // is_open

//-----------------------------------------------------------------------------
bool KUDPSocket::SetBroadcast(bool bEnable)
//-----------------------------------------------------------------------------
{
	boost::system::error_code ec;

	m_Socket.set_option(boost::asio::socket_base::broadcast(bEnable), ec);

	if (ec)
	{
		return SetError(kFormat("cannot set broadcast: {}", ec.message()));
	}

	return true;

} // SetBroadcast

//-----------------------------------------------------------------------------
KUDPSocket::native_socket_type KUDPSocket::GetNativeSocket()
//-----------------------------------------------------------------------------
{
	return m_Socket.lowest_layer().native_handle();

} // GetNativeSocket

//-----------------------------------------------------------------------------
void KUDPSocket::ResetTimer()
//-----------------------------------------------------------------------------
{
#if DEKAF2_CLASSIC_ASIO
	m_Timer.expires_from_now(boost::posix_time::milliseconds(m_Timeout.milliseconds().count()));
#else
	m_Timer.expires_after(m_Timeout.milliseconds());
#endif

} // ResetTimer

//-----------------------------------------------------------------------------
void KUDPSocket::ClearTimer()
//-----------------------------------------------------------------------------
{
#if DEKAF2_CLASSIC_ASIO
	m_Timer.expires_at(boost::posix_time::pos_infin);
#else
	m_Timer.expires_at(chrono::steady_clock::now() + chrono::years(10));
#endif

} // ClearTimer

//-----------------------------------------------------------------------------
void KUDPSocket::CheckTimer()
//-----------------------------------------------------------------------------
{
#if DEKAF2_CLASSIC_ASIO
	if (m_Timer.expires_at() <= boost::asio::deadline_timer::traits_type::now())
#else
	if (m_Timer.expiry() <= chrono::steady_clock::now())
#endif
	{
		boost::system::error_code ec;
		m_Socket.close(ec);
		ClearTimer();
		kDebug(2, "UDP connection timeout ({}): {}", m_Timeout, m_Endpoint);
	}

	m_Timer.async_wait(std::bind(&KUDPSocket::CheckTimer, this));

} // CheckTimer

//-----------------------------------------------------------------------------
void KUDPSocket::RunTimed()
//-----------------------------------------------------------------------------
{
	ResetTimer();

	try
	{
		m_ec = boost::asio::error::would_block;
		do
		{
			m_IOService.run_one();
		}
		while (m_ec == boost::asio::error::would_block);
	}
	catch (const boost::system::system_error& e)
	{
		kDebug(1, "UDP socket error: {}", e.code().message());
	}

	ClearTimer();

} // RunTimed

DEKAF2_NAMESPACE_END
