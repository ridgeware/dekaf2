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

/// @file kudpserver.h
/// provides a UDP datagram server with callback dispatch

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/net/udp/kudpsocket.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/time/duration/kduration.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <functional>
#include <thread>
#include <atomic>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_udp
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A UDP datagram server. Binds to a port and dispatches incoming datagrams
/// to a callback function.
class DEKAF2_PUBLIC KUDPServer : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using endpoint_type    = KUDPSocket::endpoint_type;

	/// Callback type for received datagrams.
	/// @param sData the received datagram content
	/// @param Sender the sender's endpoint
	/// @param Socket reference to the underlying KUDPSocket (for sending responses)
	using DatagramCallback = std::function<void(KStringView sData, const endpoint_type& Sender, KUDPSocket& Socket)>;

	//-----------------------------------------------------------------------------
	/// Construct a UDP server.
	/// @param iPort the port to bind to
	/// @param Family address family (Any, IPv4, IPv6)
	KUDPServer(uint16_t iPort, KStreamOptions::Family Family = KStreamOptions::Family::Any);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KUDPServer();
	//-----------------------------------------------------------------------------

	KUDPServer(const KUDPServer&) = delete;
	KUDPServer& operator=(const KUDPServer&) = delete;
	KUDPServer(KUDPServer&&) = delete;
	KUDPServer& operator=(KUDPServer&&) = delete;

	//-----------------------------------------------------------------------------
	/// Start the server. The callback will be invoked for each received datagram.
	/// @param Callback the function to call for each received datagram
	/// @param bBlock if true, blocks until Stop() is called. If false, starts
	/// a background thread and returns immediately.
	/// @return true on success
	bool Start(DatagramCallback Callback, bool bBlock = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Stop the server.
	/// @return true on success
	bool Stop();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Tests if the server is running.
	bool IsRunning() const { return m_bRunning; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the port we are bound to.
	uint16_t GetPort() const { return m_iPort; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the maximum datagram receive size.
	/// @param iSize the maximum receive buffer size
	void SetMaxReceiveSize(std::size_t iSize) { m_iMaxReceiveSize = iSize; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the maximum datagram receive size.
	std::size_t GetMaxReceiveSize() const { return m_iMaxReceiveSize; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a reference to the underlying UDP socket (e.g. for multicast setup).
	KUDPSocket& GetSocket() { return m_Socket; }
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	DEKAF2_PRIVATE
	void RunLoop(DatagramCallback Callback);

	KUDPSocket                    m_Socket;
	std::unique_ptr<std::thread>  m_Thread;
	uint16_t                      m_iPort            { 0     };
	std::size_t                   m_iMaxReceiveSize  { 65507 };
	std::atomic<bool>             m_bRunning         { false };
	std::atomic<bool>             m_bQuit            { false };

}; // KUDPServer

/// @}

DEKAF2_NAMESPACE_END
