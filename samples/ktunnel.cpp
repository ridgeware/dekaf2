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
// (like mobile IP networks) that does not allow incoming connections).
//
// The protected KTunnel connects via TLS to the exposed KTunnel, and opens
// more TLS data streams inside the TLS connection as requested by the exposed
// KTunnel, typically on request of incoming TCP connections to the latter, being
// either raw or TLS or whatever encoded - the connection is fully transparent.
//
// The protected KTunnel connects to the final target host, as requested by
// the exposed KTunnel (setup per forwarded connection).
//
// The exposed KTunnel opens one TLS communications port for internal
// communication, and one or more TCP forward ports for different targets.
//
//                                    control stream
//                                   <-----TLS--|--<
//                                  /           |   \ (protected)
//  client >--TCP/TLS--> KTunnel <-+   firewall |    <- KTunnel >--TCP/TLS--> target(s)
//              data    (exposed)   \           |   /               data
//                                   <-----TLS--|--<
//                                    data stream(s)

// The control and data streams between the tunnel endpoints are multiplexed
// by a simple protocol over two TLS connections, initiated from upstream:
// - any message block starts with a 16 bit integer telling
//   the length of the following message, in network order (MSB first)
//   NOT including the first 5 bytes of the message (which are always constant)
// - the length is followed by an 8 bit message type
// - the message type is followed by a 16 bit channel ID, in network order
//   (MSB first)
// - channel ID 0 is the control channel
// - channel IDs from 1 upward are used for end-to-end tcp connections
// message types are:
// - Login, Control, Connect, Data, Disconnect
// - Login messages bear endpoint name and its secret as name:secret,
//   much like in basic auth
// - Control messages are used to keep the tunnel alive when there is no
//   other communication, and to check for its health
// - Connect messages contain a string with the endpoint address to connect
//   to, either as domain:port or ipv4:port or [ipv6]:port, note the [] around
//   the ipv6 address part
// - Data messages contain the payload data
// - Disconnect messages indicate a disconnect from one side to the other

// Due to issues with OpenSSL in multithreaded up/down streams we use
// two separate streams, one for upstream, the other for downstream.
// Those streams can transport up to 65000 tunneled streams.
// Both are initiated from the protected host.

#include "ktunnel.h"

using namespace dekaf2;

static constexpr KStringView sMagic            = { "\r\n\r\nkTuNnEl\n" };
static constexpr KStringView sPing             = { "ping"    };
static constexpr KStringView sPong             = { "pong"    };
static constexpr KStringView sHelo             = { "helo"    };
static constexpr uint16_t    iBitsForNewRSAKey = 4096;

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
Message::Message(Type _type, std::size_t iChannel, KStringView sMessage)
//-----------------------------------------------------------------------------
: m_sMessage(sMessage)
, m_iChannel(iChannel)
, m_Type(_type)
{
}

//-----------------------------------------------------------------------------
uint8_t Message::ReadByte(KIOStreamSocket& Stream) const
//-----------------------------------------------------------------------------
{
	auto iChar = Stream.Read();

	if (iChar == std::istream::traits_type::eof())
	{
		auto sError = kFormat("{}: at end of file: {}", Stream.GetEndPointAddress(), Stream.GetLastError());
		kDebug(1, sError);
		throw KError(sError);
	}

	return static_cast<uint8_t>(iChar);

} // ReadByte

//-----------------------------------------------------------------------------
void Message::WriteByte (KIOStreamSocket& Stream, uint8_t iByte) const
//-----------------------------------------------------------------------------
{
	if (!Stream.Write(iByte).Good())
	{
		auto sError = kFormat("cannot write to {}", Stream.GetEndPointAddress());
		kDebug(1, sError);
		throw KError(sError);
	}

} // WriteByte

//-----------------------------------------------------------------------------
KStringView Message::PrintType() const
//-----------------------------------------------------------------------------
{
	switch (GetType())
	{
		case Login:      return "Login";
		case LoginTX:    return "LoginTX";
		case Control:    return "Control";
		case Connect:    return "Connect";
		case Data:       return "Data";
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
		case Login:      return GetMessage();
		case LoginTX:    return GetMessage();
		case Control:    return GetMessage();
		case Connect:    return GetMessage();
		case Data:       return "";
		case Disconnect: return GetMessage();
		case None:       return "";
	}

	return "";

} // Message::PrintData

//-----------------------------------------------------------------------------
void Message::Read(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	clear();

	std::size_t iLength;
	iLength  = ReadByte(Stream);
	iLength *= 256;
	iLength += ReadByte(Stream);

	m_Type = static_cast<Type>(ReadByte(Stream));

	if (m_Type > Disconnect)
	{
		auto sErr = kFormat("{}: bad message type: {}", Stream.GetEndPointAddress(), m_Type);
		kDebug(1, sErr);
		throw KError(sErr);
	}

	m_iChannel  = ReadByte(Stream);
	m_iChannel *= 256;
	m_iChannel += ReadByte(Stream);

	auto iRead = Stream.Read(m_sMessage, iLength);

	if (iRead != iLength)
	{
		auto sErr = kFormat("could not read message: only {} of {} bytes", iRead, iLength);
		kDebug(1, sErr);
		throw KError(sErr);
	}

} // Read

//-----------------------------------------------------------------------------
void Message::Write(KIOStreamSocket& Stream) const
//-----------------------------------------------------------------------------
{
	if (m_Type > Disconnect)
	{
		throw KError("invalid type");
	}

	if (m_sMessage.size() > std::numeric_limits<uint16_t>::max())
	{
		throw KError(kFormat("message too long: {} bytes", m_sMessage.size()));
	}

	WriteByte(Stream, (m_sMessage.size() / 256) & 0xff);
	WriteByte(Stream,  m_sMessage.size() & 0xff );
	WriteByte(Stream,  m_Type );
	WriteByte(Stream, (m_iChannel / 256) & 0xff);
	WriteByte(Stream,  m_iChannel        & 0xff);

	if (!Stream.Write( m_sMessage ).Good())
	{
		throw KError(kFormat("cannot write to {}", Stream.GetEndPointAddress()));
	}

} // Write

//-----------------------------------------------------------------------------
void Message::clear()
//-----------------------------------------------------------------------------
{
	*this = Message();
}

//-----------------------------------------------------------------------------
void Connection::PumpToTunnel()
//-----------------------------------------------------------------------------
{
	if (!m_DirectStream)
	{
		return;
	}

	// read until eof
	KReservedBuffer<4096> Data;

	for (;;)
	{
		auto iEvents = m_DirectStream->CheckIfReady(POLLIN | POLLHUP, m_DirectStream->GetTimeout(), true);

		if (iEvents == 0)
		{
			// timeout
			kDebug(2, "timeout from {}", m_DirectStream->GetEndPointAddress())
			break;
		}
		else if ((iEvents & POLLIN) != 0)
		{
			auto iRead = m_DirectStream->direct_read_some(Data.CharData(), Data.capacity());
			kDebug(3, "{}: read {} chars", m_DirectStream->GetEndPointAddress(), iRead);

			if (iRead > 0)
			{
				Data.resize(iRead);
				m_Tunnel(Message(Message::Data, m_iID, KStringView(Data.data(), Data.size())));
			}
		}

		if ((iEvents & POLLHUP) != 0)
		{
			// disconnected
			kDebug(2, "got disconnected from {}", m_DirectStream->GetEndPointAddress())
			break;
		}
		else if ((iEvents & (POLLERR | POLLNVAL)) != 0)
		{
			// error
			kDebug(2, "got error from {}", m_DirectStream->GetEndPointAddress())
			break;
		}

		Data.reset();
	}

	m_Tunnel(Message(Message::Disconnect, m_iID));

} // Connection::PumpToTunnel

//-----------------------------------------------------------------------------
void Connection::PumpFromTunnel()
//-----------------------------------------------------------------------------
{
	if (!m_DirectStream || !m_Tunnel)
	{
		return;
	}

	for (;!m_bQuit;)
	{
		std::unique_lock<std::mutex> Lock(m_QueueMutex);

		for (;!m_MessageQueue.empty();)
		{
			auto Message = m_MessageQueue.front().get();

			if (Message)
			{
				if (Message->GetType() == Message::Data)
				{
					if (!m_DirectStream || m_DirectStream->Write(Message->GetMessage()).Flush().Good() == false)
					{
						throw KError("cannot write to direct stream");
					}
				}
				else if (Message->GetType() == Message::Disconnect)
				{
					Disconnect();
				}
			}

			m_MessageQueue.pop_front();
		}

		if (!m_bQuit)
		{
			m_FreshData.wait(Lock);
		}
	}

} // Connection::PumpFromTunnel

//-----------------------------------------------------------------------------
void Connection::SendData(std::shared_ptr<Message> FromTunnel)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(m_QueueMutex);
		m_MessageQueue.push_back(std::move(FromTunnel));
	}

	m_FreshData.notify_one();

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
Connection::Connection (std::size_t iID, std::function<void(const Message&)> TunnelSend, KIOStreamSocket* DirectStream)
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
std::shared_ptr<Connection> Connections::Create(std::function<void(const Message&)> TunnelSend, std::size_t iID, KIOStreamSocket* DirectStream)
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

	if (iID == 0)
	{
		iID = ++m_iConnection;

		// overflow?
		if (iID > std::numeric_limits<uint16_t>::max())
		{
			// we only count in 16 bit due to the protocol and then reuse the IDs;
			iID = m_iConnection = 1;
		}
	}
	else if (iID > std::numeric_limits<uint16_t>::max())
	{
		kDebug(1, "illegal ID value {}", iID);
		return {};
	}

	auto pair = Connections->insert({ iID, std::make_shared<Connection>(iID, TunnelSend, DirectStream) });

	if (!pair.second)
	{
		kDebug(1, "connection ID {} already existing", iID);
		return {};
	}

	return pair.first->second;

} // Connections::Create

//-----------------------------------------------------------------------------
std::shared_ptr<Connection> Connections::Get(std::size_t iID, bool bAndRemove)
//-----------------------------------------------------------------------------
{
	auto Connections = m_Connections.unique();

	auto it = Connections->find(iID);

	if (it != Connections->end())
	{
		auto Connection = it->second;

		if (bAndRemove)
		{
			Connections->erase(it);
		}

		return Connection;
	}

	return {};

} // GetConnection

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

} // RemoveConnection

//-----------------------------------------------------------------------------
void ExposedServer::SendMessageToUpstream(const Message& message)
//-----------------------------------------------------------------------------
{
	auto Control = m_ControlStream.unique();

	if (!Control)
	{
		throw KError("control stream is nullptr on write attempt");
	}

	message.Debug(2);

	*(Control.get()) << message; // flushes itself

} // ExposedServer::SendMessageToUpstream

//-----------------------------------------------------------------------------
void ExposedServer::ControlStream(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	Stream.SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

	{
		auto Control = m_ControlStream.unique();

		if (Control.get() != nullptr)
		{
			kDebug(2, "already had control stream");
		}

		Control.get() = &Stream;

		// finally say hi
		Stream << Message(Message::Control, 0, sHelo); // flushes itself
	}

	m_Config.Message("opened control stream from {}", Stream.GetEndPointAddress());

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [&Stream,this]() noexcept
	{
		// make sure the write control stream is safely removed
		m_ControlStream.unique().get() = nullptr;
		m_Config.Message("closed control stream from {}", Stream.GetEndPointAddress());
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	// reset the promises
	m_WaitForTX = std::promise<void>();
	m_Quit      = std::promise<void>();

	auto Wait = m_WaitForTX.get_future();
	auto Quit = m_Quit.get_future();

	if (Wait.wait_for(chrono::seconds(15)) != std::future_status::ready)
	{
		kDebug(1, "waited too long for TX control stream");
		return;
	}

	// and wait until the other thread is done
	Quit.get();
	kDebug(3, "closing control stream");

} // ControlStream

//-----------------------------------------------------------------------------
void ExposedServer::ControlStreamTX(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	Stream.SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

	// keep a free pointer on the stream, we will only use it for reading
	auto* Reader = &Stream;

	{
		auto Control = m_ControlStreamTX.unique();

		if (Control.get() != nullptr)
		{
			kDebug(2, "already had TX control stream");
		}

		Control.get() = &Stream;

		// finally say hi
		Stream << Message(Message::Control, 0, sHelo); // flushes itself
	}

	m_Config.Message("opened TX control stream from {}", Stream.GetEndPointAddress());

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [&Stream,this]() noexcept
	{
		// make sure the write control stream is safely removed
		m_ControlStreamTX.unique().get() = nullptr;
		m_Config.Message("closed TX control stream from {}", Stream.GetEndPointAddress());

		m_Quit.set_value();
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	m_WaitForTX.set_value();

	std::unique_ptr<Message> FromUpstream;

	for (;;)
	{
		if (!FromUpstream)
		{
			FromUpstream = std::make_unique<Message>();
		}

		*Reader >> *FromUpstream;

		FromUpstream->Debug(3);

		switch (FromUpstream->GetType())
		{
			case Message::Login:
			case Message::LoginTX:
				throw KError("received login in an established connection");

			case Message::Control:
				if (FromUpstream->GetChannel() != 0)
				{
					throw KError(kFormat("received control message on channel {}", FromUpstream->GetChannel()));
				}
				if (FromUpstream->GetMessage() == sPing)
				{
					SendMessageToUpstream(Message(Message::Control, 0, sPong));
				}
				else if (FromUpstream->GetMessage() == sPong)
				{
					// do nothing
				}
				break;

			case Message::Connect:
				throw KError("received connect message from upstream");

			case Message::Data:
			{
				// find the connection and give it the data
				auto Connection = m_Connections.Get(FromUpstream->GetChannel(), false);

				if (!Connection)
				{
					SendMessageToUpstream(Message(Message::Disconnect, FromUpstream->GetChannel()));
				}
				else
				{
					Connection->SendData(std::move(FromUpstream));
				}
				break;
			}

			case Message::Disconnect:
			{
				// find the connection and disconnect it
				auto Connection = m_Connections.Get(FromUpstream->GetChannel(), true);

				if (Connection)
				{
					// put the disconnect into the queue
					Connection->SendData(std::move(FromUpstream));
				}
				break;
			}

			case Message::None:
				throw KError("received invalid message type");
		}
	}

} // ControlStreamTX

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

	auto Connection = m_Connections.Create(std::bind(&ExposedServer::SendMessageToUpstream, this, std::placeholders::_1), 0, &Downstream);

	if (!Connection)
	{
		throw KError("cannot create new connection");
	}

	auto iID = Connection->GetID();

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [iID, &Downstream]() noexcept
	{
		kDebug(1, "closed forward stream {} from {}", iID, Downstream.GetEndPointAddress());
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	kDebug(1, "requesting forward connection {} to {}", iID, Endpoint);

	SendMessageToUpstream(Message(Message::Connect, iID, Endpoint.Serialize()));

	if (!sInitialData.empty())
	{
		// write the initial incoming data (if any) to upstream
		SendMessageToUpstream(Message(Message::Data, iID, sInitialData));
	}

	auto pump = std::thread(&Connection::PumpFromTunnel, Connection.get());

	Connection->PumpToTunnel();

	pump.join();

} // ForwardStream

//-----------------------------------------------------------------------------
bool ExposedServer::CheckSecret(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	Message Login;

	Stream >> Login;

	bool bIsTX = Login.GetType() == Message::LoginTX;

	if (!bIsTX && Login.GetType() != Message::Login)
	{
		return false;
	}

	auto& sMessage = Login.GetMessage();

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
		kDebug(1, "invalid secret from {}: {}", Stream.GetEndPointAddress(), sSecret);
		return false;
	}

	kDebug(1, "successful login from {} ({}) with secret: {}", Stream.GetEndPointAddress(), sName, sSecret);

	if (bIsTX)
	{
		ControlStreamTX(Stream);
	}
	else
	{
		ControlStream(Stream);
	}

	return true;

} // CheckSecret

//-----------------------------------------------------------------------------
bool ExposedServer::CheckMagic(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	// check for the magic - if not existing this is an incoming client
	// connection for the exposed host with raw payload
	for (auto it = sMagic.begin(); it != sMagic.end(); ++it)
	{
		auto ch = Stream.Read();

		if (ch == std::istream::traits_type::eof())
		{
			// eof reached
			return false;
		}

		if (ch != *it)
		{
			// this is a raw payload stream
			// TODO we may later consider to start a REST server here to configure the
			// connections
//			ForwardStream(Stream, m_Config.DefaultTarget, sMagic.Left(it - sMagic.begin()));
			// stop further processing when stream has ended
			return false;
		}
	}

	return true;

} // CheckMagic

//-----------------------------------------------------------------------------
// handles one incoming connection - check if it is a control session starting with #magic"
void ExposedServer::Session (KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	if (CheckMagic(Stream))
	{
		CheckSecret(Stream);
	}
}

//-----------------------------------------------------------------------------
// handles one incoming connection
// - a raw tcp stream to forward via TLS to the protected host
void ExposedRawServer::Session (KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	m_ExposedServer.ForwardStream(Stream, m_Target);
}

//-----------------------------------------------------------------------------
void ProtectedHost::SendMessageToDownstream(const Message& message)
//-----------------------------------------------------------------------------
{
	auto Control = m_ControlStreamTX.unique();

	// check for nullptr
	if (!Control)
	{
		throw KError("control stream is nullptr on write attempt");
	}

	message.Debug(2);

	*(Control.get()) << message;

} // ProtectedHost::SendMessageToDownstream

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
	auto Control = m_ControlStreamTX.unique();

	// check for nullptr
	if (!Control)
	{
		// simply return, do not throw!
		return;
	}

	*(Control.get()) << Message(Message::Control, 0, sPing); // flushes itself

} // ProtectedHost::TimingCallback

//-----------------------------------------------------------------------------
void ProtectedHost::Run()
//-----------------------------------------------------------------------------
{
	KStreamOptions Options;
	// connect timeout to exposed host, also wait timeout between two tries
	Options.SetTimeout(m_Config.ConnectTimeout);

	for (;;)
	{
		try
		{
			m_Config.Message("connecting {}..", m_Config.ExposedHost);
			auto Control = CreateKTLSClient(m_Config.ExposedHost, Options);

			if (Control->Good())
			{
				Control->SetTimeout(m_Config.ConnectTimeout);
				Control->Write(sMagic);
				*Control << Message(Message::Login, 0, *m_Config.Secrets.begin());

				{
					// wait for the response
					Message Response;
					*Control >> Response;

					if (Response.GetType() != Message::Control &&
						Response.GetChannel() != 0 &&
						Response.GetMessage() != sHelo)
					{
						// error
						m_Config.Message("login failed!");
						return;
					}
				}

				m_Config.Message("control stream opened - now waiting for data streams");

				// now increase the timeout to allow for a once-per-minute ping
				Control->SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

				auto ControlTX = CreateKTLSClient(m_Config.ExposedHost, Options);

				if (!ControlTX->Good())
				{
					throw KError("cannot establish second control connection");
				}
				ControlTX->SetTimeout(m_Config.ConnectTimeout);
				ControlTX->Write(sMagic);
				*ControlTX << Message(Message::LoginTX, 0, *m_Config.Secrets.begin());

				{
					// wait for the response
					Message Response;
					*ControlTX >> Response;

					if (Response.GetType() != Message::Control &&
						Response.GetChannel() != 0 &&
						Response.GetMessage() != sHelo)
					{
						// error
						m_Config.Message("login failed!");
						return;
					}
				}

				// set the shared write instance
				m_ControlStreamTX.unique().get() = ControlTX.get();

				// we'll only ever *read* now through Control in this thread

				std::unique_ptr<Message> FromControl;

				for (;Control->Good();)
				{
					if (!FromControl)
					{
						FromControl = std::make_unique<Message>();
					}

					*Control >> *FromControl;

					FromControl->Debug(3);

					kDebug(2, "open connections: {}", m_Connections.size());

					switch (FromControl->GetType())
					{
						case Message::Control:
							if (FromControl->GetChannel() != 0)
							{
								kDebug(1, "bad channel for control message: {}", FromControl->GetChannel());
								Control->Disconnect();
								break;
							}
							else if (FromControl->GetMessage() == sPing)
							{
								// respond with pong
								SendMessageToDownstream(Message(Message::Control, 0, sPong));
							}
							else if (FromControl->GetMessage() == sPong)
							{
								// do nothing
								kDebug(3, "received a pong");
							}
							break;

						case Message::Connect:
						{
							// open a new stream to endpoint, and data stream to exposed host
							auto iID = FromControl->GetChannel();
							KTCPEndPoint TargetHost(FromControl->GetMessage());
							kDebug(3, "got connect request: {} for channel {}", TargetHost, iID);

							auto Connection = m_Connections.Create(std::bind(&ProtectedHost::SendMessageToDownstream, this, std::placeholders::_1), iID, nullptr);

							if (Connection)
							{
								m_Threads.Create(&ProtectedHost::ConnectToTarget, this, iID, std::move(TargetHost));
							}
							else
							{
								SendMessageToDownstream(Message(Message::Disconnect, FromControl->GetChannel()));
							}

							break;
						}

						case Message::Data:
						{
							auto Connection = m_Connections.Get(FromControl->GetChannel(), false);

							if (!Connection)
							{
								SendMessageToDownstream(Message(Message::Disconnect, FromControl->GetChannel()));
							}
							else
							{
								Connection->SendData(std::move(FromControl));
							}
							break;
						}

						case Message::Disconnect:
						{
							auto Connection = m_Connections.Get(FromControl->GetChannel(), false);

							if (Connection)
							{
								// put the disconnect into the queue
								Connection->SendData(std::move(FromControl));
							}
							break;
						}

						case Message::Login:
						case Message::LoginTX:
						case Message::None:
							kDebug(1, "bad message type");
							Control->Disconnect();
							break;
					}
				}
				// make sure the write control stream is safely removed
				m_ControlStreamTX.unique().get() = nullptr;

				m_Config.Message("lost connection - now sleeping for {} and retrying", m_Config.ConnectTimeout);
			}
			else
			{
				m_Config.Message("cannot connect - now sleeping for {} and retrying", m_Config.ConnectTimeout);
			}

			kSleep(m_Config.ConnectTimeout);
		}
		catch(const std::exception& ex)
		{
			m_Config.Message("{}", ex.what());
		}
	}

} // ProtectedHost::Run

//-----------------------------------------------------------------------------
ProtectedHost::ProtectedHost(const CommonConfig& Config)
//-----------------------------------------------------------------------------
: m_Connections(Config.iMaxTunnels)
, m_Config(Config)
, m_TimerID(Dekaf::getInstance().GetTimer().CallEvery(m_Config.ControlPing, std::bind(&ProtectedHost::TimingCallback, this, std::placeholders::_1)))
{
}

//-----------------------------------------------------------------------------
void KTunnel::StartExposedHost()
//-----------------------------------------------------------------------------
{
	if (!m_bGenerateCert && !m_sCertFile.empty() && !kNonEmptyFileExists(m_sCertFile))
	{
		throw KError(kFormat("cert file not found: {}", m_sCertFile));
	}

	if (!m_bGenerateCert && !m_sKeyFile.empty() && !kNonEmptyFileExists(m_sKeyFile))
	{
		throw KError(kFormat("key file not found: {}", m_sKeyFile));
	}

	m_ExposedServer = std::make_unique<ExposedServer>
	(
		m_Config,
		m_iPort,
		true,
		m_Config.iMaxTunnels + 2 // we have two control connections
	);

	if (!m_sAllowedCipherSuites.empty())
	{
		m_ExposedServer->SetAllowedCipherSuites(m_sAllowedCipherSuites);
	}

	if (kNonEmptyFileExists(m_sCertFile))
	{
		// load and check validity
		KRSACert MyCert;

		if (!MyCert.Load(m_sCertFile))
		{
			throw KError(kFormat("cannot load cert: {}", MyCert.GetLastError()));
		}
		else
		{
			auto now = KUnixTime::now();

			if (MyCert.ValidFrom() > now)
			{
				throw KError(kFormat("cert not yet valid: {:%F %T}", MyCert.ValidFrom()));
			}
			else if (MyCert.ValidUntil() < (now + chrono::weeks(1)))
			{
				m_bGenerateCert = true;

				m_Config.Message("cert expired at {:%F %T} - will generate new self-signed cert", MyCert.ValidUntil());

				if (kRename(m_sCertFile, kFormat("{}.bak", m_sCertFile)))
				{
					m_Config.Message("renamed old cert to {}.bak", m_sCertFile);
				}
			}
			else
			{
				m_Config.Message("cert valid until {:%F %T}", MyCert.ValidUntil());
			}
		}
	}

	if (!kNonEmptyFileExists(m_sCertFile))
	{
		KRSAKey MyKey;

		if (!kNonEmptyFileExists(m_sKeyFile))
		{
			// create a new private key
			if (!MyKey.Create(iBitsForNewRSAKey))
			{
				throw KError(MyKey.GetLastError());
			}

			if (m_bGenerateCert)
			{
				if (!MyKey.Save(m_sKeyFile, true, m_sTLSPassword)) throw KError(MyKey.GetLastError());
				m_Config.Message("new private key with {} bits created: {}", iBitsForNewRSAKey, m_sKeyFile);
			}
			else
			{
				m_Config.Message("ephemeral private key with {} bits created", iBitsForNewRSAKey);
			}
		}
		else
		{
			// load key from file
			if (!MyKey.Load(m_sKeyFile, m_sTLSPassword)) throw KError(MyKey.GetLastError());
			m_Config.Message("loaded private key from: {}", m_sKeyFile);
		}

		// create a self signed cert
		KRSACert MyCert;
		if (!MyCert.Create(MyKey, "localhost", "US", "", chrono::months(12))) throw KError(MyCert.GetLastError());

		if (m_bGenerateCert)
		{
			if (!MyCert.Save(m_sCertFile)) throw KError(MyCert.GetLastError());
			m_Config.Message("new cert created: {}", m_sCertFile);
		}
		else
		{
			m_Config.Message("ephemeral cert created");
		}

		// set the cert
		m_ExposedServer->SetTLSCertificates(MyCert.GetPEM(), MyKey.GetPEM(true, m_sTLSPassword), m_sTLSPassword);
	}
	else
	{
		// load the cert
		m_ExposedServer->LoadTLSCertificates(m_sCertFile, m_sKeyFile, m_sTLSPassword);
	}

	// start the server to forward raw data
	m_ExposedRawServer = std::make_unique<ExposedRawServer>
	(
		m_ExposedServer.get(),
		m_Config.DefaultTarget,
		m_iRawPort,
		false,
		m_Config.iMaxTunnels
	);

	m_Config.Message("listening on port {} for control connection, and on port {} for forward data connections", m_iPort, m_iRawPort);

	// listen on TLS
	m_ExposedServer->Start(m_Config.Timeout, /* bBlock= */ false);

	// listen on TCP
	m_ExposedRawServer->Start(m_Config.Timeout, /* bBlock= */ true);

} // StartExposedHost

//-----------------------------------------------------------------------------
void KTunnel::StartProtectedHost()
//-----------------------------------------------------------------------------
{
	ProtectedHost Protected(m_Config);

	Protected.Run();

} // ProtectedHost

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

	KStringView sSecrets;

	// define cli options
	m_Config.ExposedHost   = Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
	m_iPort                = Options("p,port       : port number to listen at for TLS connections (if exposed host), or connect to (if protected host) - defaults to 443.", 443);
	m_iRawPort             = Options("f,forward    : port number to listen at for raw TCP connections that will be forwarded (if exposed host)", 0);
	sSecrets               = Options("s,secret     : if exposed host, takes comma separated list of secrets for login by protected hosts, or one single secret if this is the protected host.").String();
	m_Config.DefaultTarget = Options("t,target     : if exposed host, takes the domain:port of a default target, if no other target had been specified in the incoming data connect", "");
	m_Config.iMaxTunnels   = Options("m,maxtunnels : if exposed host, maximum number of tunnels to open, defaults to 10 - if protected host, the setting has no effect.", 10);
	m_Config.Timeout       = chrono::seconds(Options("to,timeout <seconds> : data connection timeout in seconds (default 30)", 30));
	m_sCertFile            = Options("cert <file>  : if exposed host, TLS certificate filepath (.pem) - if option is unused an ephemeral cert is created (self-signed, in memory)", "");
	m_sKeyFile             = Options("key <file>   : if exposed host, TLS private key filepath (.pem) - if option is unused an ephemeral key is created (in memory)", "");
	m_sTLSPassword         = Options("tlspass <pass> : if exposed host, TLS certificate password, if any", "");
	m_sAllowedCipherSuites = Options("ciphers <suites> : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
	m_bGenerateCert        = Options("generate     : if exposed host, save private key and cert to disk for next usage if files are not existing", false);
	m_Config.bQuiet        = Options("q,quiet      : do not output status to stdout", false);

	// do a final check if all required options were set
	if (!Options.Check()) return 1;

	if (!m_iPort)
	{
		SetError("need valid port number");
	}

	if (m_Config.ExposedHost.Port.empty())
	{
		m_Config.ExposedHost.Port.get() = m_iPort;
	}

	if (!IsExposed() && (!m_sCertFile.empty() || !m_sKeyFile.empty()))
	{
		SetError("the protected host may not have a cert or key option");
	}

	// split and store list of secrets
	for (auto& sSecret : sSecrets.Split())
	{
		m_Config.Secrets.insert(sSecret);
	}

	if (m_Config.Secrets.empty()) SetError("need at least one (shared) secret");
	if (!m_Config.iMaxTunnels)    SetError("maxttunnels should be at least 1");

	if (IsExposed())
	{
		if (m_Config.DefaultTarget.empty()) SetError("exposed host needs a default target");
		if (m_Config.DefaultTarget.Port.get() == 0) SetError("endpoind needs an explicit port number");
		if (!m_iRawPort) SetError("exposed host needs a forward port being configured");
		if (m_bGenerateCert && (m_sKeyFile.empty() || m_sCertFile.empty())) SetError("to generate key and cert file, both need to be named with the -key and -cert options");
		m_Config.Message("starting as exposed host");

		StartExposedHost();
	}
	else
	{
		m_Config.Message("starting as protected host, connecting exposed host at {}", m_Config.ExposedHost);

		StartProtectedHost();
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
