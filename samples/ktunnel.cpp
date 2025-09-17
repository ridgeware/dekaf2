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
// communication and general REST services, and one or more TCP forward
// ports for different targets.
//
//                                    <downstream<
//                                   <-----TLS--|--<
//                                  /           |   \ (protected)
//  client >--TCP/TLS--> KTunnel <-<   firewall |    <- KTunnel >--TCP/TLS--> target(s)
//              data    (exposed)   \           |   /               data
//                                   <-----TLS--|--<
//                                     >upstream>
//
// The control and data streams between the tunnel endpoints are multiplexed
// by a simple protocol over two TLS connections, initiated from upstream:
//
// - any message block starts with a 16 bit integer telling
//   the length of the following message, in network order (MSB first)
//   NOT including the first 5 bytes of the message (which are always constant)
// - the length is followed by an 8 bit message type
// - the message type is followed by a 16 bit channel ID, in network order
//   (MSB first)
// - channel ID 0 is the control channel
// - channel IDs from 1 upward are used for end-to-end tcp connections
//
// Message types are:
// - Login, Control, Connect, Data, Disconnect
// - Login messages bear endpoint "user" name and its secret as name:secret,
//   much like in basic auth
// - Control messages are used to keep the tunnel alive when there is no
//   other communication, and to check for its health
// - Connect messages contain a string with the endpoint address to connect
//   to, either as domain:port or ipv4:port or [ipv6]:port, note the [] around
//   the ipv6 address part
// - Data messages contain the payload data
// - Disconnect messages indicate a disconnect from one side to the other
//
// We have to use these announced-size messages because OpenSSL has no way
// to reliably tell if on an open read connection there are new bytes available
// for reading. SSL_pending() is supposed to give this information, but does not
// permit to set a timeout, hence polling it is always a tradeoff between cpu load
// and speed. The new SSL_poll() is also not allowing to set a timeout, hence it
// is rather useless outside of QUIC. Therefore, all protocols running on top of
// TLS have to create their own protocol layer to workaround these limitations.
// In HTTPS it is chunked transfer, and in our case it's the message datagrams.
//
// Due to issues with OpenSSL in multithreaded up/down streams we use
// two separate streams, one for upstream, the other for downstream.
// Those streams can transport up to 65000 tunneled streams.
// Both are initiated from the protected host.

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
		case LoginTX:    return "LoginTX";
		case LoginRX:    return "LoginRX";
		case Helo:       return "Helo";
		case Ping:       return "Ping";
		case Pong:       return "Pong";
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
		case LoginTX:
		case LoginRX:
		case Helo:
		case Ping:
		case Pong:
		case Control:
		case Connect:
		case Disconnect: return GetMessage();

		case Data:
		case None:       return "";
	}

	return "";

} // Message::PrintData

//-----------------------------------------------------------------------------
KString Message::Debug() const
//-----------------------------------------------------------------------------
{
	return kFormat("ch: {} sz: {} {}: {}", GetChannel(), size(), PrintType(), PrintData());
}

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
		Throw(kFormat("{}: bad message type: {}", Stream.GetEndPointAddress(), m_Type));
	}

	m_iChannel  = ReadByte(Stream);
	m_iChannel *= 256;
	m_iChannel += ReadByte(Stream);

	auto iRead = Stream.Read(m_sMessage, iLength);

	if (iRead != iLength)
	{
		Throw(kFormat("could not read message: only {} of {} bytes", iRead, iLength));
	}

} // Read

//-----------------------------------------------------------------------------
void Message::Write(KIOStreamSocket& Stream) const
//-----------------------------------------------------------------------------
{
	if (m_Type > Disconnect)
	{
		Throw("invalid type");
	}

	if (m_sMessage.size() > std::numeric_limits<uint16_t>::max())
	{
		Throw(kFormat("message too long: {} bytes", m_sMessage.size()));
	}

	WriteByte(Stream, (m_sMessage.size() / 256) & 0xff);
	WriteByte(Stream,  m_sMessage.size() & 0xff );
	WriteByte(Stream,  m_Type );
	WriteByte(Stream, (m_iChannel / 256) & 0xff);
	WriteByte(Stream,  m_iChannel        & 0xff);

	if (!Stream.Write( m_sMessage ).Good())
	{
		Throw(kFormat("cannot write to {}", Stream.GetEndPointAddress()));
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
void Connection::PumpToTunnel()
//-----------------------------------------------------------------------------
{
	if (!m_DirectStream)
	{
		return;
	}

	try
	{
		KReservedBuffer<4096> Data;

		// read until disconnect or timeout
		for (;;)
		{
			KStopTime Stop;
			auto iEvents = m_DirectStream->CheckIfReady(POLLIN | POLLHUP);
			kDebug(3, "poll: events: {:b} ({:x}), took {}", iEvents, iEvents, Stop.elapsed());

			if (iEvents == 0)
			{
				// timeout
				kDebug(2, "timeout from {}", m_DirectStream->GetEndPointAddress())
				break;
			}
			else if ((iEvents & POLLIN) != 0)
			{
				Stop.clear();
				auto iRead = m_DirectStream->direct_read_some(Data.data(), Data.capacity());
				kDebug(3, "{}: read {} chars, took {}", m_DirectStream->GetEndPointAddress(), iRead, Stop.elapsed());

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
		return;
	}

	try
	{
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

				m_MessageQueue.pop();
			}

			if (!m_bQuit)
			{
				m_FreshData.wait(Lock);
			}
		}
	}
	catch (const std::exception& ex)
	{
		kDebug(1, ex.what());
	}

} // Connection::PumpFromTunnel

//-----------------------------------------------------------------------------
void Connection::SendData(std::unique_ptr<Message> FromTunnel)
//-----------------------------------------------------------------------------
{
	{
		std::lock_guard<std::mutex> Lock(m_QueueMutex);
		m_MessageQueue.push(std::move(FromTunnel));
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
std::shared_ptr<Connection> Connections::Create(std::size_t iID, std::function<void(const Message&)> TunnelSend, KIOStreamSocket* DirectStream)
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
void ExposedServer::SendMessageToUpstream(const Message& message)
//-----------------------------------------------------------------------------
{
	auto Control = m_ControlStreamTX.unique();

	if (!Control)
	{
		throw KError("control stream is nullptr on write attempt");
	}

	kDebug(3, message.Debug());

	*(Control.get()) << message; // flushes itself

} // ExposedServer::SendMessageToUpstream

//-----------------------------------------------------------------------------
void ExposedServer::ControlStreamTX(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	Stream.SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

	{
		auto Control = m_ControlStreamTX.unique();

		if (Control.get() != nullptr)
		{
			kDebug(2, "already had control stream");
		}

		Control.get() = &Stream;

		// finally say hi
		Stream << Message(Message::Helo, 0);
	}

	m_Config.Message("opened control stream from {}", Stream.GetEndPointAddress());

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [&Stream,this]() noexcept
	{
		// make sure the write control stream is safely removed
		m_ControlStreamTX.unique().get() = nullptr;
		m_Config.Message("closed control stream from {}", Stream.GetEndPointAddress());
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	// reset the promises
	m_WaitForRX = std::promise<void>();
	m_Quit      = std::promise<void>();

	auto Wait = m_WaitForRX.get_future();
	auto Quit = m_Quit.get_future();

	if (Wait.wait_for(chrono::seconds(15)) != std::future_status::ready)
	{
		kDebug(1, "waited too long for TX control stream");
		return;
	}

	// and wait until the other thread is done
	Quit.get();
	kDebug(3, "closing control stream");

} // ControlStreamTX

//-----------------------------------------------------------------------------
void ExposedServer::ControlStreamRX(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	Stream.SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

	// finally say hi
	Stream << Message(Message::Helo, 0);

	m_Config.Message("opened TX control stream from {}", Stream.GetEndPointAddress());

	// we use a named lambda because we want compatibility with C++11, which needs
	// a type for KScopeGuard..
	auto namedLambdaGuard = [&Stream,this]() noexcept
	{
		// make sure the write control stream is safely removed
		m_ControlStreamRX.unique().get() = nullptr;
		m_Config.Message("closed TX control stream from {}", Stream.GetEndPointAddress());

		m_Quit.set_value();
	};

	auto Guard = KScopeGuard<decltype(namedLambdaGuard)>(namedLambdaGuard);

	m_WaitForRX.set_value();

	std::unique_ptr<Message> FromUpstream;

	for (;;)
	{
		if (!FromUpstream)
		{
			FromUpstream = std::make_unique<Message>();
		}

		Stream >> *FromUpstream;

		kDebug(3, FromUpstream->Debug());

		// get a shorthand for the channel
		auto chan = FromUpstream->GetChannel();

		switch (FromUpstream->GetType())
		{
			case Message::Ping:
				// check if the channel is alive on our end
				if (!chan || m_Connections.Exists(chan))
				{
					SendMessageToUpstream(Message(Message::Pong, chan));
				}
				else
				{
					SendMessageToUpstream(Message(Message::Disconnect, chan));
				}
				break;

			case Message::Pong:
				// do nothing
				break;

			case Message::Control:
				if (chan != 0)
				{
					throw KError(kFormat("received control message on channel {}", chan));
				}
				// we currently do not exchange any control data here
				break;

			case Message::Data:
			{
				// find the connection and give it the data
				auto Connection = m_Connections.Get(chan, false);

				if (!Connection)
				{
					SendMessageToUpstream(Message(Message::Disconnect, chan));
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

			case Message::LoginTX:
			case Message::LoginRX:
			case Message::Helo:
				throw KError("received login handshake in an established connection");

			case Message::None:
				throw KError("received invalid message type");
		}
	}

} // ControlStreamRX

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

	auto Connection = m_Connections.Create(0, std::bind(&ExposedServer::SendMessageToUpstream, this, std::placeholders::_1), &Downstream);

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
bool ExposedServer::Login(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	Message Login;

	Stream >> Login;

	bool bIsTX = Login.GetType() == Message::LoginRX;

	if (!bIsTX && Login.GetType() != Message::LoginTX)
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
		ControlStreamRX(Stream);
	}
	else
	{
		ControlStreamTX(Stream);
	}

	return true;

} // Login

//-----------------------------------------------------------------------------
ExposedServer::ExposedServer (const Config& config)
//-----------------------------------------------------------------------------
: m_Connections(config.iMaxTunnels)
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
	Settings.sAllowedCipherSuites    = m_Config.sAllowedCipherSuites;
	Settings.bCreateEphemeralCert    = true;

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
		auto* StreamSocket = HTTP.GetStreamSocket();

		if (!StreamSocket) throw KHTTPError(KHTTPError::H5xx_ERROR, "internal error");

		Login(*StreamSocket);

		// fast exit without any further data sent
		HTTP.SetDisconnected();

		throw KHTTPError(KHTTPError::H4xx_GONE, "");

	}).Parse(KRESTRoute::ParserType::NOREAD);

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
void ExposedRawServer::Session (KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	m_ExposedServer.ForwardStream(Stream, m_Target);
}

//-----------------------------------------------------------------------------
void ProtectedHost::SendMessageToDownstream(const Message& message, bool bThrowIfNoStream)
//-----------------------------------------------------------------------------
{
	auto Control = m_ControlStreamTX.unique();

	if (!Control.get())
	{
		if (bThrowIfNoStream)
		{
			throw KError("control stream is nullptr on write attempt");
		}
		else
		{
			return;
		}
	}

	kDebug(3, message.Debug());

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
	SendMessageToDownstream(Message(Message::Ping, 0), false);

} // ProtectedHost::TimingCallback

//-----------------------------------------------------------------------------
ProtectedHost::ProtectedHost(const CommonConfig& Config)
//-----------------------------------------------------------------------------
: m_Connections(Config.iMaxTunnels)
, m_Config(Config)
, m_TimerID(Dekaf::getInstance().GetTimer().CallEvery(m_Config.ControlPing, std::bind(&ProtectedHost::TimingCallback, this, std::placeholders::_1)))
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
				Control->WriteLine("GET /Tunnel HTTP/1.1");
				Control->WriteLine(kFormat("host: {}", m_Config.ExposedHost));
				Control->WriteLine(kFormat("useragent: ktunnel/1.0"));
				Control->WriteLine();
				Control->Flush();

				*Control << Message(Message::LoginTX, 0, *m_Config.Secrets.begin());

				{
					// wait for the response
					Message Response;
					*Control >> Response;

					if (Response.GetType() != Message::Helo &&
						Response.GetChannel() != 0)
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
				ControlTX->WriteLine("GET /Tunnel HTTP/1.1");
				ControlTX->WriteLine(kFormat("host: {}", m_Config.ExposedHost));
				ControlTX->WriteLine(kFormat("useragent: ktunnel/1.0"));
				ControlTX->WriteLine();
				ControlTX->Flush();

				*ControlTX << Message(Message::LoginRX, 0, *m_Config.Secrets.begin());

				{
					// wait for the response
					Message Response;
					*ControlTX >> Response;

					if (Response.GetType() != Message::Helo &&
						Response.GetChannel() != 0)
					{
						// error
						m_Config.Message("login failed!");
						return;
					}
				}

				// set the shared write instance
				m_ControlStreamTX.unique().get() = std::move(ControlTX);

				// we'll only ever *read* now through Control in this thread

				std::unique_ptr<Message> FromControl;

				for (;Control->Good();)
				{
					if (!FromControl)
					{
						FromControl = std::make_unique<Message>();
					}

					*Control >> *FromControl;

					kDebug(3, FromControl->Debug());

					kDebug(2, "open connections: {}", m_Connections.size());

					// get a shorthand for the channel
					auto chan = FromControl->GetChannel();

					switch (FromControl->GetType())
					{
						case Message::Ping:
							// respond with pong TODO check upstream connection
							SendMessageToDownstream(Message(Message::Pong, chan));
							break;

						case Message::Pong:
							// do nothing
							break;

						case Message::Control:
							if (chan != 0)
							{
								kDebug(1, "bad channel for control message: {}", chan);
								Control->Disconnect();
								break;
							}
							// we currently do not exchange control messages
							break;

						case Message::Connect:
						{
							// open a new stream to endpoint, and data stream to exposed host
							KTCPEndPoint TargetHost(FromControl->GetMessage());
							kDebug(3, "got connect request: {} for channel {}", TargetHost, chan);

							auto Connection = m_Connections.Create(chan, std::bind(&ProtectedHost::SendMessageToDownstream, this, std::placeholders::_1, true), nullptr);

							if (Connection)
							{
								m_Threads.Create(&ProtectedHost::ConnectToTarget, this, chan, std::move(TargetHost));
							}
							else
							{
								SendMessageToDownstream(Message(Message::Disconnect, chan));
							}

							break;
						}

						case Message::Data:
						{
							auto Connection = m_Connections.Get(chan, false);

							if (!Connection)
							{
								SendMessageToDownstream(Message(Message::Disconnect, chan));
							}
							else
							{
								Connection->SendData(std::move(FromControl));
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
						case Message::LoginTX:
						case Message::LoginRX:
						case Message::None:
							kDebug(1, "bad message type");
							Control->Disconnect();
							break;
					}
				}

				m_Config.Message("lost connection - now sleeping for {} and retrying", m_Config.ConnectTimeout);
			}
			else
			{
				m_Config.Message("cannot connect - now sleeping for {} and retrying", m_Config.ConnectTimeout);
			}

			// make sure the write control stream is safely removed
			m_ControlStreamTX.unique()->reset();
			kSleep(m_Config.ConnectTimeout);
		}
		catch(const std::exception& ex)
		{
			m_ControlStreamTX.unique()->reset();
			m_Config.Message("{}", ex.what());
		}

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
	m_Config.ExposedHost= Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
	m_Config.iPort     = Options("p,port       : port number to listen at for TLS connections (if exposed host), or connect to (if protected host) - defaults to 443.", 443);
	m_Config.iRawPort  = Options("f,forward    : port number to listen at for raw TCP connections that will be forwarded (if exposed host)", 0);
	KStringView sSecrets      = Options("s,secret     : if exposed host, takes comma separated list of secrets for login by protected hosts, or one single secret if this is the protected host.").String();
	m_Config.DefaultTarget
	                          = Options("t,target     : if exposed host, takes the domain:port of a default target, if no other target had been specified in the incoming data connect", "");
	m_Config.iMaxTunnels= Options("m,maxtunnels : if exposed host, maximum number of tunnels to open, defaults to 10 - if protected host, the setting has no effect.", 10);
	m_Config.Timeout    = chrono::seconds(Options("to,timeout <seconds> : data connection timeout in seconds (default 30)", 30));
	m_Config.sCertFile = Options("cert <file>  : if exposed host, TLS certificate filepath (.pem) - if option is unused a self-signed cert is created", "");
	m_Config.sKeyFile  = Options("key <file>   : if exposed host, TLS private key filepath (.pem) - if option is unused a new key is created", "");
	m_Config.sTLSPassword
	                          = Options("tlspass <pass> : if exposed host, TLS certificate password, if any", "");
	m_Config.sAllowedCipherSuites
	                          = Options("ciphers <suites> : colon delimited list of permitted cipher suites for TLS (check your OpenSSL documentation for values), defaults to \"PFS\", which selects all suites with Perfect Forward Secrecy and GCM or POLY1305", "");
	m_Config.bQuiet     = Options("q,quiet      : do not output status to stdout", false);

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
	if (!m_Config.iMaxTunnels)    SetError("maxttunnels should be at least 1");

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
