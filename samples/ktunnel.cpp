/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2025, Ridgeware, Inc.
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

// KTunnel can be started either as an exposed host (that can listen on ports
// reachable by one or more clients) or as a protected host (so called because
// it typically will be protected behind a firewall or network configuration
// (like mobile IP networks) that do not allow incoming connections).
//
// The protected KTunnel connects via HTTPS (upgrading to websockets) to the
// exposed KTunnel, and opens more TLS data streams inside the websocket
// connection as requested by the exposed KTunnel, typically on request of
// incoming TCP connections to the latter, being either raw or TLS or whatever
// encoded - the connection is fully transparent.
//
// The protected KTunnel connects to the final target host, as requested by
// the exposed KTunnel (setup per forwarded connection).
//
// The exposed KTunnel opens one TLS communications port for internal
// communication and general REST services, and one or more TCP forward
// ports for different targets.
//
//         e.g. port 1234         e.g. port 443
//                                              |
// client >>--TCP/TLS-->> KTunnel <<----TLS websocket----<< KTunnel >>--TCP/TLS-->> target(s)
//            data in    (exposed)              |         (protected)   data in
//          any protocol                        |                     any protocol
//                                     firewall |
//                       or e.g. mobile network |
//
// The control and data streams between the tunnel endpoints are multiplexed
// by a simple protocol over a TLS websocket connection, initiated from upstream.
// This protocol extends the websocket protocol with the ability to add a detailed
// message type, and the multiplex channel ID.
//
// - the payload data length and other details of the transport is encoded in
//   the standard websocket header, a value of 4 is substracted from the payload
//   length to account for the embedded ktunnel protocol header, which consists of:
// - an 8 bit message type
// - the message type is followed by a 24 bit channel ID, in network order
//   (MSB first)
// - channel ID 0 is the control channel
// - channel IDs from 1 upward are used for end-to-end tcp connections
//
// Message types are:
// - Login, Control, Connect, Data, Pause, Resume, Disconnect, Ping, Pong
// - Login messages bear endpoint "user" name and its secret as name:secret
//   in base64, much like in basic auth
// - Ping/Pong messages are used to keep the tunnel alive when there is no
//   other communication, and to check for its health and for the health of
//   specific channels. The websocket ping is not used.
// - Connect messages contain a string with the endpoint address to connect
//   to, either as domain:port or ipv4:port or [ipv6]:port, note the [] around
//   the ipv6 address part
// - Data messages contain the payload data
// - Disconnect messages indicate a disconnect from one side to the other
// - Pause and Resume messages implement a flow control per individual data
//   channel, telling the respective other end of the channel to pause or
//   resume transmission through the tunnel
//
// The tunnel connection can transport up to 16 million tunneled streams.

#include "ktunnel.h"

using namespace dekaf2;

//-----------------------------------------------------------------------------
void CommonConfig::PrintMessage(KStringView sMessage) const
//-----------------------------------------------------------------------------
{
	if (!bQuiet)
	{
		kWriteLine(sMessage);
	}
	else
	{
		kDebug(1, sMessage);
	}

} // CommonConfig::PrintMessage

//-----------------------------------------------------------------------------
Message::Message(Type _type, std::size_t iChannel, KString sMessage)
//-----------------------------------------------------------------------------
: Frame(std::move(sMessage), true)
{
	SetType(_type);
	SetChannel(iChannel);
}

//-----------------------------------------------------------------------------
KStringView Message::PrintType() const
//-----------------------------------------------------------------------------
{
	switch (GetType())
	{
		case Login:      return "Login";
		case Helo:       return "Helo";
		case Ping:       return "Ping";
		case Pong:       return "Pong";
		case Idle:       return "Idle";
		case Control:    return "Control";
		case Connect:    return "Connect";
		case Data:       return "Data";
		case Pause:      return "Pause";
		case Resume:     return "Resume";
		case Disconnect: return "Disconnect";
		case None:       return "None";
	}

	return "Invalid";

} // Message::PrintType

//-----------------------------------------------------------------------------
KStringView Message::PrintData() const
//-----------------------------------------------------------------------------
{
	switch (GetType())
	{
		case Login:
		case Helo:
		case Ping:
		case Pong:
		case Idle:
		case Control:
		case Connect:
		case Pause:
		case Resume:
		case Disconnect:
			if (!kIsBinary(GetMessage()))
			{
				return GetMessage();
			}
			return "";

		case None:
			return "";

		case Data:
		{
			KStringView sStart = GetMessage().LeftUTF8(20);

			if (!kIsBinary(sStart))
			{
				return sStart;
			}

			return "";
		}
	}

	return "";

} // Message::PrintData

//-----------------------------------------------------------------------------
KString Message::Debug() const
//-----------------------------------------------------------------------------
{
	return kFormat("[{}]: {} {} chars: {}", GetChannel(), PrintType(), size(), PrintData());
}

//-----------------------------------------------------------------------------
void Message::SetChannel(std::size_t iChannel)
//-----------------------------------------------------------------------------
{
	if (iChannel > MaxChannel())
	{
		Throw(kFormat("channel too high: {:x}, max: {:x}", iChannel, MaxChannel()));
	}

	auto it = m_Preamble.begin();
	*++it = (iChannel >> 16) & 0xff;
	*++it = (iChannel >>  8) & 0xff;
	*++it =  iChannel        & 0xff;

} // SetChannel

//-----------------------------------------------------------------------------
std::size_t Message::GetChannel() const
//-----------------------------------------------------------------------------
{
	std::size_t iChannel;
	auto it = m_Preamble.begin();

	iChannel  = *++it;
	iChannel *= 256;
	iChannel += *++it;
	iChannel *= 256;
	iChannel += *++it;

	return iChannel;

} // GetChannel

//-----------------------------------------------------------------------------
std::size_t Message::GetPreambleSize() const
//-----------------------------------------------------------------------------
{
	return m_Preamble.size();
}

//-----------------------------------------------------------------------------
char* Message::GetPreambleBuf() const
//-----------------------------------------------------------------------------
{
	return m_Preamble.data();
}

//-----------------------------------------------------------------------------
void Message::Read(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	clear();

	if (!Frame::Read(Stream, Stream, false))
	{
		Throw(kFormat("[{}]: cannot read from {}", GetChannel(), Stream.GetEndPointAddress()));
	}

} // Read

//-----------------------------------------------------------------------------
void Message::Write(KIOStreamSocket& Stream, bool bMask)
//-----------------------------------------------------------------------------
{
	if (GetType() > Disconnect)
	{
		Throw("invalid type");
	}

	if (!Frame::Write(Stream, bMask))
	{
		Throw(kFormat("[{}]: cannot write to {}", GetChannel(), Stream.GetEndPointAddress()));
	}

	if (bMask)
	{
		// the frame is now toast because of the websocket xoring, better clear it right away
		clear();
	}

} // Write

//-----------------------------------------------------------------------------
void Message::clear()
//-----------------------------------------------------------------------------
{
	*this = Message();
}

//-----------------------------------------------------------------------------
void Message::Throw(KString sError, KStringView sFunction) const
//-----------------------------------------------------------------------------
{
	KLog::getInstance().debug_fun(1, sFunction, sError);
	throw KError(sError);

} // Throw

//-----------------------------------------------------------------------------
void Connection::Resume()
//-----------------------------------------------------------------------------
{
	std::lock_guard Lock(m_TunnelMutex);

	if (!m_bPaused)
	{
		// return right here, do not send notifications
		return;
	}
	// resume
	m_bPaused = false;

	m_ResumeTunnel.notify_one();

} // Resume

//-----------------------------------------------------------------------------
void Connection::PumpToTunnel()
//-----------------------------------------------------------------------------
{
	if (!m_DirectStream)
	{
		return;
	}

	try
	{
		KReservedBuffer<KDefaultCopyBufSize - 12> Data;

		// read until disconnect or timeout
		for (;;)
		{
			KStopTime Stop;
			auto iEvents = m_DirectStream->CheckIfReady(POLLIN | POLLHUP);
			kDebug(3, "[{}]: poll: events: {:b} ({:x}), took {}", GetID(), iEvents, iEvents, Stop.elapsed());

			if (iEvents == 0)
			{
				// timeout
				kDebug(2, "[{}]: timeout from {}", GetID(), m_DirectStream->GetEndPointAddress())
				break;
			}
			else if ((iEvents & POLLIN) != 0)
			{
				if (IsPaused())
				{
					kDebug(2, "[{}]: paused", GetID());
					// we are requested to hold on sending data.. wait on a semaphore
					// until we get woken up again
					std::unique_lock Lock(m_TunnelMutex);
					// 	protect from spurious wakeups by checking IsPaused()
					m_ResumeTunnel.wait(Lock, [this]{ return !IsPaused(); });
					kDebug(2, "[{}]: unpaused", GetID());
					// now poll again to see if our input stream is still available
					continue;
				}

				Stop.clear();
				auto iRead = m_DirectStream->direct_read_some(Data.data(), Data.capacity());
				kDebug(3, "[{}]: {}: read {} chars, took {}", GetID(), m_DirectStream->GetEndPointAddress(), iRead, Stop.elapsed());

				if (iRead > 0)
				{
					Data.resize(iRead);
					Message data(Message::Data, m_iID, KStringView(Data.data(), Data.size()));
					m_Tunnel(std::move(data));
				}
				else
				{
					// timeout or disconnected
					kDebug(2, "[{}]: disconnected from {}", GetID(), m_DirectStream->GetEndPointAddress())
					break;
				}
			}

			if ((iEvents & POLLHUP) != 0)
			{
				// disconnected
				kDebug(2, "[{}]: got disconnected from {}", GetID(), m_DirectStream->GetEndPointAddress())
				break;
			}
			else if ((iEvents & (POLLERR | POLLNVAL)) != 0)
			{
				// error
				kDebug(2, "[{}]: got error from {}", GetID(), m_DirectStream->GetEndPointAddress())
				break;
			}

			Data.reset();
		}

		m_Tunnel(Message(Message::Disconnect, m_iID));
	}
	catch (const std::exception& ex)
	{
		kDebug(1, ex.what());
	}

} // Connection::PumpToTunnel

//-----------------------------------------------------------------------------
void Connection::PumpFromTunnel()
//-----------------------------------------------------------------------------
{
	if (!m_DirectStream || !m_Tunnel)
	{
		throw KError(kFormat("[{}] no stream for channel", GetID()));
	}

	try
	{
		for (;!m_bQuit;)
		{
			std::unique_lock<std::mutex> Lock(m_QueueMutex);

			for (;!m_MessageQueue.empty();)
			{
				// get next message from top
				auto& MessageToDirect = m_MessageQueue.front();

				if (MessageToDirect.GetType() == Message::Data)
				{
					KStopTime Stop;
					// check if we can immediately write data
					if (!m_DirectStream->IsWriteReady(KDuration()))
					{
						// no - get out of the lock
						Lock.unlock();
						// and wait until timeout for write readiness
						if (!m_DirectStream->IsWriteReady())
						{
							throw KError(kFormat("[{}] cannot write to direct stream ({})", GetID(), 1));
						}
						// we can now write again, acquire the lock again
						Lock.lock();
					}
					kDebug(3, "polled for {}", Stop.elapsedAndClear());
					// write the message
					if (m_DirectStream->Write(MessageToDirect.GetMessage()).Flush().Good() == false)
					{
						throw KError(kFormat("[{}] cannot write to direct stream ({})", GetID(), 2));
					}
					kDebug(3, "[{}]: {}: wrote {} chars, took {}", GetID(), m_DirectStream->GetEndPointAddress(), MessageToDirect.size(), Stop.elapsed());
				}
				else if (MessageToDirect.GetType() == Message::Disconnect)
				{
					kDebug(3, "[{}]: got disconnect frame for {}", GetID(), m_DirectStream->GetEndPointAddress());
					Disconnect();
				}
				// remove the message from the top
				m_MessageQueue.pop();

				// check if we had formerly sent a Pause frame, and we have again room
				// in the output queue
				if (m_bRXPaused && m_MessageQueue.size() < MaxMessageQueueSize() / 2)
				{
					// request to resume from the other end of the tunnel
					m_Tunnel(Message(Message::Resume, GetID()));
					m_bRXPaused = false;
				}
			}

			if (!m_bQuit)
			{
				// and wait for new data coming in
				KStopTime Stop;
				m_FreshData.wait(Lock, [this]{ return !m_MessageQueue.empty(); });
				kDebug(3, "[{}]: {}: waited for new data, took {}", GetID(), m_DirectStream->GetEndPointAddress(), Stop.elapsed());
			}
		}
	}
	catch (const std::exception& ex)
	{
		kDebug(1, ex.what());
		// TODO throw?
	}

} // Connection::PumpFromTunnel

//-----------------------------------------------------------------------------
void Connection::SendData(Message&& FromTunnel)
//-----------------------------------------------------------------------------
{
	KStopTime Stop;
	{
		std::lock_guard<std::mutex> Lock(m_QueueMutex);

		m_MessageQueue.push(std::move(FromTunnel));

		kDebug(3, "[{}]: queued {} chars, took {}", GetID(), m_MessageQueue.back().size(), Stop.elapsedAndClear());

		if (!m_bRXPaused && m_MessageQueue.size() >= MaxMessageQueueSize())
		{
			m_Tunnel(Message(Message::Pause, GetID()));
			m_bRXPaused = true;
			kDebug(3, "[{}]: requested pause, took {}", GetID(), Stop.elapsedAndClear());
		}

		m_FreshData.notify_one();

	}
	kDebug(3, "[{}]: notified writer thread, took {}", GetID(), Stop.elapsed());

} // Connection::SendData

//-----------------------------------------------------------------------------
void Connection::Disconnect()
//-----------------------------------------------------------------------------
{
	m_bQuit = true;

	m_FreshData.notify_all();

	if (m_DirectStream)
	{
		m_DirectStream->Disconnect();
	}
}

//-----------------------------------------------------------------------------
Connection::Connection (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream)
//-----------------------------------------------------------------------------
: m_Tunnel(std::move(TunnelSend))
, m_DirectStream(DirectStream)
, m_iID(iID)
{
}

//-----------------------------------------------------------------------------
std::size_t Connections::size() const
//-----------------------------------------------------------------------------
{
	return m_Connections.shared()->size();
}

//-----------------------------------------------------------------------------
std::shared_ptr<Connection> Connections::Create(std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	if (Connections->size() >= m_iMaxConnections)
	{
		kDebug(1, "max tunnel limit of {} reached - refusing new forward connection", m_iMaxConnections);

		if (DirectStream)
		{
			DirectStream->WriteLine("max tunnel limit reached").Flush();
		}

		return {};
	}

	bool bFindFreeID { false };

	std::size_t iIterations { 0 };

	for (;;)
	{
		++iIterations;

		if (iIterations >= Message::MaxChannel())
		{
			kDebug(1, "cannot generate a new ID value");
		}

		if (iID == 0)
		{
			bFindFreeID = true;
			iID = ++m_iConnection;

			// overflow?
			if (iID > Message::MaxChannel())
			{
				iID = m_iConnection = 1;
			}
		}
		else if (iID > Message::MaxChannel())
		{
			kDebug(1, "illegal ID value {}", iID);
			return {};
		}

		auto pair = Connections->insert({ iID, std::make_shared<Connection>(iID, TunnelSend, DirectStream) });

		if (!pair.second)
		{
			if (bFindFreeID)
			{
				continue;
			}

			kDebug(1, "connection ID {} already existing", iID);
			return {};
		}
		else
		{
			return pair.first->second;
		}
	}

} // Connections::Create

//-----------------------------------------------------------------------------
std::shared_ptr<Connection> Connections::Get(std::size_t iID, bool bAndRemove)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	auto it = Connections->find(iID);

	if (it != Connections->end())
	{
		if (!bAndRemove)
		{
			return it->second;
		}

		auto Connection = it->second;

		Connections->erase(it);

		return Connection;
	}

	return {};

} // Connections::GetConnection

//-----------------------------------------------------------------------------
bool Connections::Remove(std::size_t iID)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	auto it = Connections->find(iID);

	if (it != Connections->end())
	{
		Connections->erase(it);

		return true;
	}

	return false;

} // Connections::RemoveConnection

//-----------------------------------------------------------------------------
bool Connections::Exists(std::size_t iID)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.shared();

	return Connections->find(iID) != Connections->end();

} // Connections::Exists

//-----------------------------------------------------------------------------
bool WebSocketDirection::HaveStream() const
//-----------------------------------------------------------------------------
{
	return bool(m_TunnelStream);
}

//-----------------------------------------------------------------------------
void WebSocketDirection::SetStream(std::unique_ptr<KIOStreamSocket> Stream)
//-----------------------------------------------------------------------------
{
	m_TunnelStream = std::move(Stream);
}

//-----------------------------------------------------------------------------
KIOStreamSocket& WebSocketDirection::GetStream()
//-----------------------------------------------------------------------------
{
	if (!m_TunnelStream)
	{
		throw KError("no stream");
	}
	return *m_TunnelStream.get();
}

//-----------------------------------------------------------------------------
void WebSocketDirection::ResetStream()
//-----------------------------------------------------------------------------
{
	m_TunnelStream.reset();
}

//-----------------------------------------------------------------------------
void WebSocketDirection::ReadMessage(Message& message)
//-----------------------------------------------------------------------------
{
	if (!m_TunnelStream)
	{
		throw KError("no tunnel stream for read");
	}

	for (;;)
	{
		// return latest after set stream timeout
		if (!m_TunnelStream->IsReadReady())
		{
			throw KError(m_TunnelStream->GetLastError());
		}

		std::lock_guard StreamLock(m_TLSMutex);

		// return immediately from poll
		if (m_TunnelStream->IsReadReady(KDuration()))
		{
			message.Read(*m_TunnelStream);
			kDebug(3, message.Debug());

			break;
		}

		// stream changed state while acquiring lock, repeat waiting
	}

} // WebSocketDirection::ReadMessage

//-----------------------------------------------------------------------------
void WebSocketDirection::WriteMessage(Message&& message)
//-----------------------------------------------------------------------------
{
	if (!m_TunnelStream)
	{
		throw KError("no tunnel stream for write");
	}

	for (;;)
	{
		// return latest after set stream timeout
		if (!m_TunnelStream->IsWriteReady())
		{
			throw KError(m_TunnelStream->GetLastError());
		}

		std::lock_guard StreamLock(m_TLSMutex);

		// return immediately from poll
		if (m_TunnelStream->IsWriteReady(KDuration()))
		{
			kDebug(3, message.Debug());
			auto mtype = message.GetType();
			KStopTime Stop;
			message.Write(*m_TunnelStream, m_bMaskTx);
			kDebug(3, "took {}", Stop.elapsed());

			if (mtype == Message::Data)
			{
				m_LastTx.clear();
				m_IdleNotBefore = KStopTime::now() + chrono::milliseconds(500);
			}
			else if (mtype == Message::Idle)
			{
//				kDebug(2, "wrote Idle message");
				m_IdleNotBefore = KStopTime::now() + chrono::milliseconds(500);
			}

			break;
		}

		// stream changed state while acquiring lock, repeat waiting
	}

} // WebSocketDirection::WriteMessage

//-----------------------------------------------------------------------------
void WebSocketDirection::TimerLoop()
//-----------------------------------------------------------------------------
{
	for (;!m_bQuit;)
	{
		kSleep(chrono::milliseconds(250));

		if (m_TunnelStream)
		{
			std::unique_lock<std::mutex> Lock(m_TLSMutex);

			auto now = KStopTime::now();

			auto LastTx = now - m_LastTx.startedAt();

			if (LastTx >= chrono::milliseconds(500) && LastTx <= chrono::milliseconds(1600))
			{
				if (m_IdleNotBefore <= now)
				{
					Lock.unlock();
					WriteMessage(Message(Message::Idle, 0));
				}
			}
		}
	}

} // WebSocketDirection::TimerLoop

//-----------------------------------------------------------------------------
void ExposedServer::ControlStream()
//-----------------------------------------------------------------------------
{
	GetStream().SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

	// finally say hi
	WriteMessage(Message(Message::Helo, 0));

	m_Config.Message("[{}]: opened control stream from {}", 0, GetStream().GetEndPointAddress());

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [this]() noexcept
	{
		// make sure the write control stream is safely removed
		m_Config.Message("[{}]: closed control stream from {}", 0, GetStream().GetEndPointAddress());
		ResetStream();
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	for (;;)
	{
		Message FromUpstream;
		ReadMessage(FromUpstream);

		kDebug(3, "open connections: {}", m_Connections.size());

		// get a shorthand for the channel
		auto chan = FromUpstream.GetChannel();

		switch (FromUpstream.GetType())
		{
			case Message::Ping:
				// check if the channel is alive on our end
				if (!chan || m_Connections.Exists(chan))
				{
					FromUpstream.SetType(Message::Pong);
					WriteMessage(std::move(FromUpstream));
				}
				else
				{
					WriteMessage(Message(Message::Disconnect, chan));
				}
				break;

			case Message::Pong:
				// do nothing (should check if payload is that of last Ping sent,
				// but we do not send payloads with Ping)
				break;

			case Message::Idle:
				// really do nothing, this is only meant to keep the data pumps going
				break;

			case Message::Control:
				if (chan != 0)
				{
					throw KError(kFormat("[{}]: received control message on non-zero channel", chan));
				}
				// we currently do not exchange any control data here
				break;

			case Message::Data:
			{
				// find the connection and give it the data
				auto Connection = m_Connections.Get(chan, false);

				if (!Connection)
				{
					WriteMessage(Message(Message::Disconnect, chan));
				}
				else
				{
					Connection->SendData(std::move(FromUpstream));
				}
				break;
			}

			case Message::Pause:
			{
				// find the connection and tell it to pause sending data
				auto Connection = m_Connections.Get(chan, false);

				if (!Connection)
				{
					WriteMessage(Message(Message::Disconnect, chan));
				}
				else
				{
					Connection->Pause();
				}
				break;
			}

			case Message::Resume:
			{
				// find the connection and tell it to resume sending data
				auto Connection = m_Connections.Get(chan, false);

				if (!Connection)
				{
					WriteMessage(Message(Message::Disconnect, chan));
				}
				else
				{
					Connection->Resume();
				}
				break;
			}


			case Message::Disconnect:
			{
				// find the connection and disconnect it
				auto Connection = m_Connections.Get(chan, true);

				if (Connection)
				{
					// put the disconnect into the downstream queue
					Connection->SendData(std::move(FromUpstream));
				}
				break;
			}

			case Message::Connect:
				throw KError("received connect message from upstream");

			case Message::Login:
			case Message::Helo:
				throw KError("received login handshake in an established connection");

			case Message::None:
				throw KError("received invalid message type");
		}
	}

} // ControlStream

//-----------------------------------------------------------------------------
void ExposedServer::ForwardStream(KIOStreamSocket& Downstream, const KTCPEndPoint& Endpoint, KStringView sInitialData)
//-----------------------------------------------------------------------------
{
	// this is the stream between the client and the first ktunnel instance which
	// will be forwarded through the tunnel

	kDebug(1, "incoming connection from {} for target {}", Downstream.GetEndPointAddress(), Endpoint);

	if (Endpoint.empty())
	{
		throw KError("missing target endpoint definition with domain:port");
	}

	// is already set from KTCPServer ?
	Downstream.SetTimeout(m_Config.Timeout);

	auto Connection = m_Connections.Create(0, [this](Message&& msg){ WriteMessage(std::move(msg)); }, &Downstream);

	if (!Connection)
	{
		throw KError("cannot create new connection");
	}

	auto iID = Connection->GetID();

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [iID, &Downstream]() noexcept
	{
		kDebug(1, "[{}]: closed forward stream from {}", iID, Downstream.GetEndPointAddress());
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	kDebug(1, "[{}]: requesting forward connection to {}", iID, Endpoint);

	WriteMessage(Message(Message::Connect, iID, Endpoint.Serialize()));

	if (!sInitialData.empty())
	{
		// write the initial incoming data (if any) to upstream
		WriteMessage(Message(Message::Data, iID, sInitialData));
	}

	auto pump = std::thread(&Connection::PumpFromTunnel, Connection.get());

	Connection->PumpToTunnel();

	pump.join();

} // ForwardStream

//-----------------------------------------------------------------------------
void ExposedServer::Login(KWebSocket& WebSocket)
//-----------------------------------------------------------------------------
{
	Message Login;
	Login.Read(WebSocket.Stream());

	if (Login.GetType() != Message::Login)
	{
		return;
	}

	auto sMessage = KDecode::Base64(Login.GetMessage());

	KString sName;
	KString sSecret;

	auto iPos = sMessage.find(':');

	if (iPos != KString::npos)
	{
		sSecret = sMessage.Mid(iPos + 1);
	}
	else
	{
		sSecret = sMessage;
	}

	if (m_Config.Secrets.empty() || !m_Config.Secrets.contains(sSecret))
	{
		kDebug(1, "invalid secret from {}: {}", WebSocket.Stream().GetEndPointAddress(), sSecret);
		return;
	}

	kDebug(1, "successful login from {} ({}) with secret: {}", WebSocket.Stream().GetEndPointAddress(), sName, sSecret);

	SetStream(WebSocket.MoveStream());

	ControlStream();

} // Login

//-----------------------------------------------------------------------------
ExposedServer::ExposedServer (const Config& config)
//-----------------------------------------------------------------------------
: WebSocketDirection(false) // we are the "server", therefore we do not mask our messages
, m_Connections(config.iMaxTunnels)
, m_Config(config)
{
	KREST::Options Settings;

	Settings.Type                    = KREST::HTTP;
	Settings.iPort                   = m_Config.iPort;
	Settings.iMaxConnections         = m_Config.iMaxTunnels * 2 + 20;
	Settings.iTimeout                = m_Config.Timeout.seconds().count();
	Settings.KLogHeader              = "";
	Settings.bBlocking               = false;
	Settings.bMicrosecondTimerHeader = true;
	Settings.TimerHeader             = "x-microseconds";
	Settings.sCert                   = m_Config.sCertFile;
	Settings.sKey                    = m_Config.sKeyFile;
	Settings.sTLSPassword            = m_Config.sTLSPassword;
	Settings.sAllowedCipherSuites    = m_Config.sCipherSuites;
	Settings.bCreateEphemeralCert    = true;
	Settings.bStoreEphemeralCert     = m_Config.bPersistCert;

	Settings.AddHeader(KHTTPHeader::SERVER, "ktunnel");
	Settings.AddHeader("x-server", "ktunnel");

	KRESTRoutes Routes;

	Routes.AddRoute("/Version").Get([](KRESTServer& HTTP)
	{
		html::Page Page("Version", "en_US");
		Page.Body() += html::Text("ktunnel v1.0");
		HTTP.SetRawOutput(Page.Serialize());
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE , KMIME::HTML_UTF8);

	}).Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute("/Configure").Get([](KRESTServer& HTTP)
	{
		/// TODO

	}).Parse(KRESTRoute::ParserType::NOREAD);

	Routes.AddRoute("/Tunnel").Get([this](KRESTServer& HTTP)
	{
		// set the handler for websockets in the REST server instance
		HTTP.SetWebSocketHandler(std::bind(&ExposedServer::Login, this, std::placeholders::_1));
		// ask for per-thread execution in the REST server instance
		HTTP.SetKeepWebSocketInRunningThread();

	}).Parse(KRESTRoute::ParserType::NOREAD).Options(KRESTRoute::Options::WEBSOCKET);

	// set a default route
	Routes.SetDefaultRoute([](KRESTServer& HTTP)
	{
		if (HTTP.RequestPath.sRoute == "/favicon.ico" ||
			HTTP.RequestPath.sRoute.starts_with("/apple-touch-icon"))
		{
			HTTP.Response.SetStatus(KHTTPError::H4xx_NOTFOUND);
		}
		else if (HTTP.RequestPath.sRoute == "/robots.txt")
		{
			HTTP.SetRawOutput("User-agent: *\nDisallow: /\n");
			HTTP.Response.SetStatus(KHTTPError::H2xx_OK);
			HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::TEXT_UTF8);
		}
		else
		{
			HTTP.Response.SetStatus(KHTTPError::H302_MOVED_TEMPORARILY);
			HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, "/Configure/");
		}
	});

	// create the REST server instance
	KREST Http;

	// and run it
	m_Config.Message("starting TLS REST server on port {}", Settings.iPort);
	if (!Http.Execute(Settings, Routes)) throw KError(Http.Error());

	// start the server to forward raw data
	m_ExposedRawServer = std::make_unique<ExposedRawServer>
	(
		this,
		m_Config.DefaultTarget,
		m_Config.iRawPort,
		false,
		m_Config.iMaxTunnels
	);

	m_Config.Message("listening on port {} for forward data connections", m_Config.iRawPort);

	// listen on TCP
	m_ExposedRawServer->Start(m_Config.Timeout, /* bBlock= */ true);

} // ExposedServer::ExposedServer


//-----------------------------------------------------------------------------
// handles one incoming connection
// - a raw tcp stream to forward via TLS to the protected host
void ExposedRawServer::Session (std::unique_ptr<KIOStreamSocket>& Stream)
//-----------------------------------------------------------------------------
{
	m_ExposedServer.ForwardStream(*Stream, m_Target);
}

//-----------------------------------------------------------------------------
void ProtectedHost::ConnectToTarget(std::size_t iID, KTCPEndPoint Target)
//-----------------------------------------------------------------------------
{
	try
	{
		KStreamOptions Options;
		Options.SetTimeout(m_Config.ConnectTimeout);

		auto Connection = m_Connections.Get(iID, false);

		if (Connection)
		{
			kDebug(2, "connecting {}..", Target);

			auto TargetStream = CreateKTCPStream(Target, Options);

			if (TargetStream->Good())
			{
				Connection->SetDirectStream(TargetStream.get());

				auto pump = std::thread(&Connection::PumpFromTunnel, Connection.get());

				Connection->PumpToTunnel();

				pump.join();
			}
			else
			{
				kDebug(1, "cannot connect to target {}", Target);
			}
			// should this not yet have been done
			Connection->Disconnect();
		}
		else
		{
			kDebug(1, "cannot find connection {}", iID);
		}
	}
	catch (const std::exception& ex)
	{
		kDebug(1, ex.what());
	}

	m_Connections.Remove(iID);

} // ConnectToTarget


//-----------------------------------------------------------------------------
void ProtectedHost::TimingCallback(KUnixTime Time)
//-----------------------------------------------------------------------------
{
	if (HaveStream())
	{
		KStopTime RTT;
		WriteMessage(Message(Message::Ping, 0, kToStringView(RTT)));
	}

} // ProtectedHost::TimingCallback

//-----------------------------------------------------------------------------
ProtectedHost::ProtectedHost(const CommonConfig& Config)
//-----------------------------------------------------------------------------
: WebSocketDirection(true)
, m_Connections(Config.iMaxTunnels)
, m_Config(Config)
, m_TimerID(Dekaf::getInstance().GetTimer().CallEvery(m_Config.ControlPing, std::bind(&ProtectedHost::TimingCallback, this, std::placeholders::_1)))
{
	KHTTPStreamOptions Options;
	// connect timeout to exposed host, also wait timeout between two tries
	Options.SetTimeout(m_Config.ConnectTimeout);
	// only do HTTP/1.1, we haven't implemented websockets for any later protocol
	// (but as we are not a browser that doesn't mean any disadvantage)
	Options.Unset(KStreamOptions::RequestHTTP2);

	for (;;)
	{
		try
		{
			m_Config.Message("connecting {}..", m_Config.ExposedHost);

			KURL ExposedHost;
			ExposedHost.Protocol = url::KProtocol::HTTPS;
			ExposedHost.Domain   = m_Config.ExposedHost.Domain;
			ExposedHost.Port     = m_Config.ExposedHost.Port;
			ExposedHost.Path     = "/Tunnel";

			KWebSocketClient WebSocket(ExposedHost, Options);

			WebSocket.SetTimeout(m_Config.ConnectTimeout);
			WebSocket.SetBinary();

			if (!WebSocket.Connect())
			{
				throw KError("cannot establish first control connection");
			}

			SetStream(std::move(WebSocket.GetStream()));

			{
				WriteMessage(Message(Message::Login, 0, KEncode::Base64(*m_Config.Secrets.begin())));

				// wait for the response
				Message Response;
				ReadMessage(Response);

				if (Response.GetType() != Message::Helo &&
					Response.GetChannel() != 0)
				{
					// error
					m_Config.Message("login failed!");
					return;
				}
			}

			// now increase the timeout to allow for a once-per-minute ping
			GetStream().SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

			m_Config.Message("control stream opened - now waiting for data streams");

			for (;GetStream().Good();)
			{
				Message FromControl;
				ReadMessage(FromControl);

				kDebug(3, "open connections: {}", m_Connections.size());

				// get a shorthand for the channel
				auto chan = FromControl.GetChannel();

				switch (FromControl.GetType())
				{
					case Message::Ping:
					{
						// respond with pong
						FromControl.SetType(Message::Pong);
						WriteMessage(std::move(FromControl));
						break;
					}

					case Message::Pong:
						// do nothing, but note RTT
						if (FromControl.size() == sizeof(KStopTime))
						{
							auto RTT(kFromStringView<KStopTime>(FromControl.GetMessage()));
							m_RTT = RTT.elapsed();
							kDebug(1, "roundtrip: {}", m_RTT);
						}
						break;

					case Message::Idle:
						// really do nothing
						break;

					case Message::Control:
						if (chan != 0)
						{
							kDebug(1, "bad channel for control message: {}", chan);
							GetStream().Disconnect();
							break;
						}
						// we currently do not exchange control messages
						break;

					case Message::Connect:
					{
						// open a new stream to endpoint, and data stream to exposed host
						KTCPEndPoint TargetHost(FromControl.GetMessage());
						kDebug(3, "got connect request: {} for channel {}", TargetHost, chan);

						auto Connection = m_Connections.Create(chan, [this](Message&& msg){ WriteMessage(std::move(msg)); }, nullptr);

						if (Connection)
						{
							m_Threads.Create(&ProtectedHost::ConnectToTarget, this, chan, std::move(TargetHost));
						}
						else
						{
							WriteMessage(Message(Message::Disconnect, chan));
						}

						break;
					}

					case Message::Data:
					{
						auto Connection = m_Connections.Get(chan, false);

						if (!Connection)
						{
							WriteMessage(Message(Message::Disconnect, chan));
						}
						else
						{
							Connection->SendData(std::move(FromControl));
						}
						break;
					}

					case Message::Pause:
					{
						auto Connection = m_Connections.Get(chan, false);

						if (!Connection)
						{
							WriteMessage(Message(Message::Disconnect, chan));
						}
						else
						{
							Connection->Pause();
						}
						break;
					}

					case Message::Resume:
					{
						auto Connection = m_Connections.Get(chan, false);

						if (!Connection)
						{
							WriteMessage(Message(Message::Disconnect, chan));
						}
						else
						{
							Connection->Resume();
						}
						break;
					}

					case Message::Disconnect:
					{
						auto Connection = m_Connections.Get(chan, false);

						if (Connection)
						{
							// put the disconnect into the queue
							Connection->SendData(std::move(FromControl));
						}
						break;
					}

					case Message::Helo:
					case Message::Login:
					case Message::None:
						kDebug(1, "bad message type");
						GetStream().Disconnect();
						break;
				}
			}

			m_Config.Message("lost connection - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}
		catch(const std::exception& ex)
		{
			m_Config.Message("{}", ex.what());
			m_Config.Message("could not connect - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}

		// make sure the write control stream is safely removed
		ResetStream();
		kSleep(m_Config.ConnectTimeout);
	}

} // ProtectedHost::ctor

//-----------------------------------------------------------------------------
int KTunnel::Main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	// initialize with signal handler thread
	KInit(true).SetName("KTUNL").KeepCLIMode();

	// we will throw when setting errors
	SetThrowOnError(true);

	// setup CLI option parsing
	KOptions Options(true, argc, argv, KLog::STDOUT, /*bThrow*/true);

	// define cli options
	m_Config.ExposedHost   = Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
	m_Config.iPort         = Options("p,port       : port number to listen at for TLS connections (if exposed host), or connect to (if protected host) - defaults to 443.", 443);
	m_Config.iRawPort      = Options("f,forward    : port number to listen at for raw TCP connections that will be forwarded (if exposed host)", 0);
	KStringView sSecrets   = Options("s,secret     : if exposed host, takes comma separated list of secrets for login by protected hosts, or one single secret if this is the protected host.").String();
	m_Config.DefaultTarget = Options("t,target     : if exposed host, takes the domain:port of a default target, if no other target had been specified in the incoming data connect", "");
	m_Config.iMaxTunnels   = Options("m,maxtunnels : if exposed host, maximum number of tunnels to open, defaults to 10 - if protected host, the setting has no effect.", 10);
	m_Config.sCertFile     = Options("cert <file>  : if exposed host, TLS certificate filepath (.pem) - if option is unused a self-signed cert is created", "");
	m_Config.sKeyFile      = Options("key <file>   : if exposed host, TLS private key filepath (.pem) - if option is unused a new key is created", "");
	m_Config.sTLSPassword  = Options("tlspass <pass> : if exposed host, TLS certificate password, if any", "");
	m_Config.Timeout       = chrono::seconds(Options("to,timeout <seconds> : data connection timeout in seconds (default 30)", 30));
	m_Config.bPersistCert  = Options("persist      : should a self-signed cert be persisted to disk and reused at next start?", false);
	m_Config.sCipherSuites = Options("ciphers <suites> : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
	m_Config.bQuiet        = Options("q,quiet      : do not output status to stdout", false);

	// do a final check if all required options were set
	if (!Options.Check()) return 1;

	if (!m_Config.iPort)
	{
		SetError("need valid port number");
	}

	if (m_Config.ExposedHost.Port.empty())
	{
		m_Config.ExposedHost.Port.get() = m_Config.iPort;
	}

	if (!IsExposed() && (!m_Config.sCertFile.empty() || !m_Config.sKeyFile.empty()))
	{
		SetError("the protected host may not have a cert or key option");
	}

	// split and store list of secrets
	for (auto& sSecret : sSecrets.Split())
	{
		m_Config.Secrets.insert(sSecret);
	}

	if (m_Config.Secrets.empty()) SetError("need at least one (shared) secret");
	if (!m_Config.iMaxTunnels)    SetError("maxtunnels should be at least 1");

	if (IsExposed())
	{
		if (m_Config.DefaultTarget.empty()) SetError("exposed host needs a default target");
		if (m_Config.DefaultTarget.Port.get() == 0) SetError("endpoind needs an explicit port number");
		if (!m_Config.iRawPort) SetError("exposed host needs a forward port being configured");
		m_Config.Message("starting as exposed host");

		ExposedServer Exposed(m_Config);
	}
	else
	{
		m_Config.Message("starting as protected host, connecting exposed host at {}", m_Config.ExposedHost);

		ProtectedHost Protected(m_Config);
	}

	return 0;

} // Main

//-----------------------------------------------------------------------------
int main(int argc, char** argv)
//-----------------------------------------------------------------------------
{
	try
	{
		return KTunnel().Main (argc, argv);
	}
	catch (const std::exception& ex)
	{
		KErr.FormatLine(">> {}: {}", "ktunnel", ex.what());
	}
	return 1;
}
