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

#include <dekaf2/net/udp/kudpserver.h>
#include <dekaf2/core/logging/klog.h>
#include <dekaf2/threading/execution/kthreads.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KUDPServer::KUDPServer(uint16_t iPort, KStreamOptions::Family Family)
//-----------------------------------------------------------------------------
: m_iPort(iPort)
{
	if (!m_Socket.Bind(iPort, Family))
	{
		SetError(m_Socket.Error());
	}
}

//-----------------------------------------------------------------------------
KUDPServer::~KUDPServer()
//-----------------------------------------------------------------------------
{
	Stop();
}

//-----------------------------------------------------------------------------
bool KUDPServer::Start(DatagramCallback Callback, bool bBlock)
//-----------------------------------------------------------------------------
{
	if (!m_Socket.is_open())
	{
		return SetError("UDP server socket is not open - cannot start");
	}

	if (m_bRunning)
	{
		return SetError("UDP server is already running");
	}

	m_bQuit = false;

	if (bBlock)
	{
		kDebug(1, "UDP server starting on port {} (blocking)", m_iPort);
		RunLoop(std::move(Callback));
	}
	else
	{
		kDebug(1, "UDP server starting on port {} (background)", m_iPort);
		m_Thread = std::make_unique<std::thread>(kMakeThread(
			[this, Callback = std::move(Callback)]() mutable
			{
				RunLoop(std::move(Callback));
			}));
	}

	return true;

} // Start

//-----------------------------------------------------------------------------
bool KUDPServer::Stop()
//-----------------------------------------------------------------------------
{
	if (!m_bRunning)
	{
		return true;
	}

	kDebug(1, "UDP server stopping on port {}", m_iPort);

	m_bQuit = true;

	// close the socket to unblock any pending receive
	m_Socket.Disconnect();

	if (m_Thread && m_Thread->joinable())
	{
		m_Thread->join();
		m_Thread.reset();
	}

	m_bRunning = false;

	kDebug(1, "UDP server stopped on port {}", m_iPort);

	return true;

} // Stop

//-----------------------------------------------------------------------------
void KUDPServer::RunLoop(DatagramCallback Callback)
//-----------------------------------------------------------------------------
{
	m_bRunning = true;

	KString sBuffer;
	sBuffer.resize_uninitialized(m_iMaxReceiveSize);

	while (!m_bQuit)
	{
		endpoint_type Sender;

		auto iReceived = m_Socket.ReceiveFrom(sBuffer.data(), static_cast<std::streamsize>(m_iMaxReceiveSize), Sender);

		if (m_bQuit)
		{
			break;
		}

		if (iReceived < 0)
		{
			if (m_Socket.is_open())
			{
				kDebug(2, "UDP receive error: {}", m_Socket.Error());
			}
			continue;
		}

		if (iReceived > 0)
		{
			try
			{
				Callback(KStringView(sBuffer.data(), static_cast<std::size_t>(iReceived)), Sender, m_Socket);
			}
			catch (const std::exception& e)
			{
				kDebug(1, "exception in UDP datagram callback: {}", e.what());
			}
		}
	}

	m_bRunning = false;

} // RunLoop

DEKAF2_NAMESPACE_END
