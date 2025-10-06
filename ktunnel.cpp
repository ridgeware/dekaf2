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

// The control and data streams between the tunnel endpoints are multiplexed
// by a simple protocol over a single stream.
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
#include "kerror.h"
#include "ktcpstream.h"
#include "dekaf2.h"
#include "kbuffer.h"
#include "kscopeguard.h"
#include "kencode.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KTunnel::Message::Message(Type _type, std::size_t iChannel, KString sMessage)
//-----------------------------------------------------------------------------
: Frame(std::move(sMessage), true)
{
	SetType(_type);
	SetChannel(iChannel);
}

//-----------------------------------------------------------------------------
KStringView KTunnel::Message::PrintType() const
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
KStringView KTunnel::Message::PrintData() const
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
KString KTunnel::Message::Debug() const
//-----------------------------------------------------------------------------
{
	return kFormat("[{}]: {} {} chars: {}", GetChannel(), PrintType(), size(), PrintData());
}

//-----------------------------------------------------------------------------
void KTunnel::Message::SetChannel(std::size_t iChannel)
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
std::size_t KTunnel::Message::GetChannel() const
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
std::size_t KTunnel::Message::GetPreambleSize() const
//-----------------------------------------------------------------------------
{
	return m_Preamble.size();
}

//-----------------------------------------------------------------------------
char* KTunnel::Message::GetPreambleBuf() const
//-----------------------------------------------------------------------------
{
	return m_Preamble.data();
}

//-----------------------------------------------------------------------------
void KTunnel::Message::Read(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	clear();

	if (!Frame::Read(Stream, Stream, false))
	{
		Throw(kFormat("[{}]: cannot read from {}", GetChannel(), Stream.GetEndPointAddress()));
	}

} // Read

//-----------------------------------------------------------------------------
void KTunnel::Message::Write(KIOStreamSocket& Stream, bool bMask)
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
void KTunnel::Message::clear()
//-----------------------------------------------------------------------------
{
	*this = Message();
}

//-----------------------------------------------------------------------------
void KTunnel::Message::Throw(KString sError, KStringView sFunction) const
//-----------------------------------------------------------------------------
{
	KLog::getInstance().debug_fun(1, sFunction, sError);
	throw KError(sError);

} // Throw

//-----------------------------------------------------------------------------
void KTunnel::Connection::Resume()
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_TunnelMutex);

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
void KTunnel::Connection::PumpToTunnel()
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
					std::unique_lock<std::mutex> Lock(m_TunnelMutex);
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
void KTunnel::Connection::PumpFromTunnel()
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
void KTunnel::Connection::SendData(Message&& FromTunnel)
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
void KTunnel::Connection::Disconnect()
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
KTunnel::Connection::Connection (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream)
//-----------------------------------------------------------------------------
: m_Tunnel(std::move(TunnelSend))
, m_DirectStream(DirectStream)
, m_iID(iID)
{
}

//-----------------------------------------------------------------------------
std::size_t KTunnel::Connections::size() const
//-----------------------------------------------------------------------------
{
	return m_Connections.shared()->size();
}

//-----------------------------------------------------------------------------
std::shared_ptr<KTunnel::Connection> KTunnel::Connections::Create(std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream)
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
std::shared_ptr<KTunnel::Connection> KTunnel::Connections::Get(std::size_t iID, bool bAndRemove)
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
bool KTunnel::Connections::Remove(std::size_t iID)
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
bool KTunnel::Connections::Exists(std::size_t iID)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.shared();

	return Connections->find(iID) != Connections->end();

} // Connections::Exists

//-----------------------------------------------------------------------------
KTunnel::KTunnel
(
	Config                           Config,
	std::unique_ptr<KIOStreamSocket> Stream,
	KStringView                      sUser,
	KStringView                      sSecret,
	bool                             bNeverMask
)
//-----------------------------------------------------------------------------
: m_Config(std::move(Config))
, m_Connections(Config.iMaxTunneledConnections)
{
	if (!Stream)
	{
		throw KError("Stream is nullptr in KTunnel::KTunnel()");
	}

	m_Tunnel.unique()->Stream = std::move(Stream);

	auto bIsClient = !sUser.empty() || !sSecret.empty();

	if (!bIsClient)
	{
		WaitForLogin();
	}
	else
	{
		if (!bNeverMask)
		{
			m_bMaskTx = true;
		}

		m_TimerID = Dekaf::getInstance().GetTimer().CallEvery
		(
			m_Config.ControlPing,
			std::bind(&KTunnel::PingTest, this, std::placeholders::_1)
		);

		Login(sUser, sSecret);
	}

	SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

	m_Timer = std::thread(&KTunnel::TimerLoop, this);

} // KTunnel::KTunnel

//-----------------------------------------------------------------------------
KTunnel::~KTunnel()
//-----------------------------------------------------------------------------
{
	m_bQuit = true;

	if (m_TimerID != KTimer::InvalidID)
	{
		Dekaf::getInstance().GetTimer().Cancel(m_TimerID);
	}

	m_Timer.join();
}

//-----------------------------------------------------------------------------
void KTunnel::Connect(KIOStreamSocket* DirectStream, const KTCPEndPoint& ConnectToEndpoint)
//-----------------------------------------------------------------------------
{
	if (!DirectStream || !DirectStream->Good())
	{
		throw KError("invalid stream socket");
	}

	auto Connection = m_Connections.Create(0, [this](Message&& msg){ WriteMessage(std::move(msg)); }, DirectStream);

	if (!Connection)
	{
		throw KError("cannot create new connection");
	}

	WriteMessage(Message(Message::Connect, Connection->GetID(), ConnectToEndpoint.Serialize()));

	auto iID = Connection->GetID();

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [iID, DirectStream]() noexcept
	{
		kDebug(1, "[{}]: closed forward stream from {}", iID, DirectStream->GetEndPointAddress());
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	kDebug(1, "[{}]: requested forward connection to {}", iID, ConnectToEndpoint);

	auto pump = std::thread(&KTunnel::Connection::PumpFromTunnel, Connection.get());

	Connection->PumpToTunnel();

	pump.join();

} // Connect

//-----------------------------------------------------------------------------
bool KTunnel::HaveTunnel() const
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.shared();

	if (!Tunnel->Stream)
	{
		return false;
	}

	return Tunnel->Stream->Good();
}

//-----------------------------------------------------------------------------
void KTunnel::SetTimeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.unique();

	if (!Tunnel->Stream)
	{
		throw KError("no stream");
	}

	Tunnel->Stream->SetTimeout(Timeout);
}

//-----------------------------------------------------------------------------
KTCPEndPoint KTunnel::GetEndPointAddress() const
//-----------------------------------------------------------------------------
{
	auto Tunnel = m_Tunnel.shared();

	if (!Tunnel->Stream)
	{
		throw KError("no stream");
	}

	return Tunnel->Stream->GetEndPointAddress();
}

//-----------------------------------------------------------------------------
void KTunnel::ReadMessage(Message& message)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		{
			auto Stream = GetStream();

			if (!Stream)
			{
				throw KError("no tunnel stream for reading");
			}

			// return latest after set stream timeout
			if (!Stream->IsReadReady())
			{
				throw KError(Stream->GetLastError());
			}
		}

		{
			auto Tunnel = m_Tunnel.unique();

			if (!Tunnel->Stream)
			{
				throw KError("no tunnel stream for reading");
			}

			// return immediately from poll
			if (Tunnel->Stream->IsReadReady(KDuration()))
			{
				message.Read(*Tunnel->Stream);
				kDebug(3, message.Debug());

				break;
			}
		}

		// stream changed state while acquiring lock, repeat waiting
	}

} // KTunnel::ReadMessage

//-----------------------------------------------------------------------------
void KTunnel::WriteMessage(Message&& message)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		{
			auto Stream = GetStream();

			if (!Stream)
			{
				throw KError("no tunnel stream for writing");
			}

			// return latest after set stream timeout
			if (!Stream->IsWriteReady())
			{
				throw KError(Stream->GetLastError());
			}
		}

		{
			auto Tunnel = m_Tunnel.unique();

			if (!Tunnel->Stream)
			{
				throw KError("no tunnel stream for writing");
			}

			// return immediately from poll
			if (Tunnel->Stream->IsWriteReady(KDuration()))
			{
				kDebug(3, message.Debug());
				auto mtype = message.GetType();
				KStopTime Stop;
				message.Write(*Tunnel->Stream, m_bMaskTx);
				kDebug(3, "took {}", Stop.elapsed());

				if (mtype == Message::Data)
				{
					Tunnel->LastTx.clear();
					Tunnel->SendIdleNotBefore = KStopTime::now() + chrono::milliseconds(500);
				}
				else if (mtype == Message::Idle)
				{
					Tunnel->SendIdleNotBefore = KStopTime::now() + chrono::milliseconds(500);
				}

				break;
			}
		}

		// stream changed state while acquiring lock, repeat waiting
	}

} // KTunnel::WriteMessage

//-----------------------------------------------------------------------------
void KTunnel::TimerLoop()
//-----------------------------------------------------------------------------
{
	for (;!m_bQuit;)
	{
		kSleep(chrono::milliseconds(250));

		bool bSendIdle = false;

		{
			auto Tunnel = m_Tunnel.unique();

			if (Tunnel->Stream)
			{
				auto now = KStopTime::now();

				auto LastTx = now - Tunnel->LastTx.startedAt();

				if (LastTx >= chrono::milliseconds(500) && LastTx <= chrono::milliseconds(1600))
				{
					if (Tunnel->SendIdleNotBefore <= now)
					{
						bSendIdle = true;
					}
				}
			}
		}

		if (bSendIdle)
		{
			WriteMessage(Message(Message::Idle, 0));
		}
	}

} // KTunnel::TimerLoop

//-----------------------------------------------------------------------------
void KTunnel::ConnectToTarget(std::size_t iID, KTCPEndPoint Target)
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

} // KTunnel::ConnectToTarget

//-----------------------------------------------------------------------------
bool KTunnel::Login(KStringView sUser, KStringView sSecret)
//-----------------------------------------------------------------------------
{
	WriteMessage(Message(Message::Login, 0, KEncode::Base64(kFormat("{}:{}", sUser, sSecret))));

	// wait for the response
	Message Response;
	ReadMessage(Response);

	if (Response.GetType() != Message::Helo && Response.GetChannel() != 0)
	{
		// error
		return false;
	}

	return true;

} // KTunnel::Login

//-----------------------------------------------------------------------------
void KTunnel::WaitForLogin()
//-----------------------------------------------------------------------------
{
	Message Login;
	ReadMessage(Login);

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
		throw KError(kFormat("invalid secret from {}: {}", GetEndPointAddress(), sSecret));
	}

	// finally say hi
	WriteMessage(Message(Message::Helo, 0));

	kDebug(1, "successful login from {} ({}) with secret", GetEndPointAddress(), sName);

} // KTunnel::WaitForLogin

//-----------------------------------------------------------------------------
void KTunnel::PingTest(KUnixTime Time)
//-----------------------------------------------------------------------------
{
	if (HaveTunnel())
	{
		KStopTime RTT;
		WriteMessage(Message(Message::Ping, 0, kToStringView(RTT)));
	}

} // KTunnel::TimingCallback

//-----------------------------------------------------------------------------
void KTunnel::Run()
//-----------------------------------------------------------------------------
{
	for (;HaveTunnel();)
	{
		Message FromTunnel;
		ReadMessage(FromTunnel);

		kDebug(3, "open connections: {}", m_Connections.size());

		// get a shorthand for the channel
		auto chan = FromTunnel.GetChannel();

		switch (FromTunnel.GetType())
		{
			case Message::Ping:
			{
				// check if the channel is alive on our end
				if (!chan || m_Connections.Exists(chan))
				{
					// respond with pong
					FromTunnel.SetType(Message::Pong);
					WriteMessage(std::move(FromTunnel));
				}
				else
				{
					WriteMessage(Message(Message::Disconnect, chan));
				}


				break;
			}

			case Message::Pong:
				// do nothing, but note RTT
				if (FromTunnel.size() == sizeof(KStopTime))
				{
					auto RTT(kFromStringView<KStopTime>(FromTunnel.GetMessage()));
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
					throw KError(kFormat("[{}]: received control message on non-zero channel", chan));
				}
				// we currently do not exchange control messages
				break;

			case Message::Connect:
			{
				// open a new stream to endpoint, and data stream to exposed host
				KTCPEndPoint TargetHost(FromTunnel.GetMessage());
				kDebug(3, "got connect request: {} for channel {}", TargetHost, chan);

				auto Connection = m_Connections.Create(chan, [this](Message&& msg){ WriteMessage(std::move(msg)); }, nullptr);

				if (Connection)
				{
					m_Threads.Create(&KTunnel::ConnectToTarget, this, chan, std::move(TargetHost));
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
					Connection->SendData(std::move(FromTunnel));
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
					Connection->SendData(std::move(FromTunnel));
				}
				break;
			}

			case Message::Login:
			case Message::Helo:
				throw KError("received login handshake in an established connection");

			case Message::None:
				throw KError("received invalid message type");
		}
	}

} // KTunnel::Run

DEKAF2_NAMESPACE_END
