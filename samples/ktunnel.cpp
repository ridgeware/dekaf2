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
// more TLS data connections as requested by the exposed KTunnel, typically
// on request of incoming TCP connections to the latter, being either raw
// or TLS or whatever encoded - the connection is fully transparent.
//
// The protected KTunnel connects to the final target host, as requested by
// the exposed KTunnel (setup per forwarded connection).
//
// The exposed KTunnel opens one TLS communications port for internal
// communication, and one or more TCP forward ports for different targets.
//
//                                  control connection
//                                   <-----TLS--|--<
//                                  /           |   \
//  client >--TCP/TLS--> KTunnel <-+   firewall |    <- KTunnel >--TCP/TLS--> target(s)
//              data    (exposed)   \           |   / (protected)   data
//                                   <-----TLS--|--<
//                                  data connection(s)

#include "ktunnel.h"

using namespace dekaf2;

static constexpr KStringView sMagic            = { "\r\n\r\nkTuNnEl\n" };
static constexpr KStringView sControlStream    = { "control" };
static constexpr KStringView sDataStream       = { "data"    };
static constexpr KStringView sConnectStream    = { "connect" };
static constexpr KStringView sPing             = { "ping"    };
static constexpr KStringView sPong             = { "pong"    };
static constexpr KStringView sHelo             = { "helo"    };
static constexpr KStringView sStart            = { "start"   };
static constexpr std::size_t iMaxControlLine   = 100;
static constexpr uint16_t    iBitsForNewRSAKey = 4096;

//-----------------------------------------------------------------------------
void Connection::DataPump(KIOStreamSocket& Left, KIOStreamSocket& Right, bool bWriteToUpstream)
//-----------------------------------------------------------------------------
{
	auto Guard = KSimpleScopeGuard([this, &Left, &Right]
	{
		if (!m_bShutdown)
		{
			// disconnect
			Left.Disconnect();
			Right.Disconnect();

			// set the flag to shutdown
			m_bShutdown = true;
		}
	});

	// read until eof
	KReservedBuffer<4096> Data;

	KStopTime LastData;

	for (;;)
	{
		auto Events = Left.CheckIfReady(POLLIN, m_Config.PollTimeout);

		if (m_bShutdown)
		{
			// the other half has already finished
			break;
		}

		if (!Events)
		{
			if (LastData.elapsed() > m_Config.Timeout)
			{
				// eof/timeout
				break;
			}
		}
		else if ((Events & POLLIN) == POLLIN)
		{
			auto iRead = Left.direct_read_some(Data.CharData(), Data.capacity());

			if (iRead > 0)
			{
				Data.resize(iRead);

				if (!Right.Write(Data.data(), Data.size()).Flush())
				{
					// write error
					return;
				}
			}

			Data.reset();
		}

		if ((Events & (POLLHUP | POLLERR)) != 0)
		{
			break;
		}
	}

} // Connection::DataPump

//-----------------------------------------------------------------------------
Connection::Connection(std::size_t iID, KIOStreamSocket& Downstream, const CommonConfig& Config)
: m_Downstream(&Downstream)
, m_Config(Config)
, m_iID(iID)
//-----------------------------------------------------------------------------
{
	if (!m_iID) return;

} // Connection::Connection

//-----------------------------------------------------------------------------
Connection::Connection(std::size_t iID, KIOStreamSocket& Downstream, KIOStreamSocket& Upstream, const CommonConfig& Config)
: m_Config(Config)
, m_iID(iID)
//-----------------------------------------------------------------------------
{
	if (!m_iID) return;

	// create one thread per direction for the data pump
	std::thread Other(&Connection::DataPump, this, std::ref(Upstream), std::ref(Downstream), true );

	// the other half we let run in the current thread
	DataPump(Downstream, Upstream, false);

	// wait until the other thread joins
	Other.join();

} // Connection::Connection

//-----------------------------------------------------------------------------
bool Connection::WaitForUpstreamAndRun(KDuration ConnectTimeout)
//-----------------------------------------------------------------------------
{
	if (!m_Downstream)
	{
		return false;
	}

	auto HaveUpstream = m_HaveUpstream.get_future();

	if (HaveUpstream.wait_for(ConnectTimeout) != std::future_status::ready)
	{
		return false;
	}

	auto UpstreamDone = m_UpstreamDone.get_future();

	// now run the downstream side

	DataPump(*m_Downstream, *m_Upstream, false);

	m_DownstreamDone.set_value();

	// wait for max 1 minutes ..
	UpstreamDone.wait_for(chrono::minutes(1));

	return true;

} // WaitForUpstreamAndRun

//-----------------------------------------------------------------------------
bool Connection::SetUpstreamAndRun(KIOStreamSocket& Upstream)
//-----------------------------------------------------------------------------
{
	if (!m_Downstream)
	{
		return false;
	}

	m_Upstream = &Upstream;

	m_HaveUpstream.set_value();

	auto DownstreamDone = m_DownstreamDone.get_future();

	DataPump (*m_Upstream, *m_Downstream, true);

	m_UpstreamDone.set_value();

	// wait for max 1 minutes ..
	DownstreamDone.wait_for(chrono::minutes(1));

	return true;

} // SetUpstreamAndRun

//-----------------------------------------------------------------------------
// ConnectHostnames will only return after the connection is ended on both sides, therefore
// call it in its own thread
void Connection::ConnectHostnames(std::size_t iID, KTCPEndPoint DownstreamHost, KTCPEndPoint UpstreamHost, const CommonConfig& Config)
//-----------------------------------------------------------------------------
{
	KStreamOptions Options;
	Options.SetTimeout(Config.Timeout);

	kDebug(1, "connecting to exposed {} for channel ID {}.. ", DownstreamHost, iID);

	std::unique_ptr<KIOStreamSocket> Downstream = CreateKTLSClient(DownstreamHost, Options);

	if (!Downstream->Good())
	{
		kPrintLine("failed to connect to downstream host {} for channel ID {}", DownstreamHost, iID);
		return;
	}

	Downstream->SetTimeout(Config.Timeout);
	Downstream->WriteLine(kFormat("{}{}\n{} {}", sMagic, *Config.Secrets.begin(), sDataStream, iID)).Flush();

	kDebug(1, "connecting to target {} for channel ID {}.. ", UpstreamHost, iID);

	std::unique_ptr<KIOStreamSocket> Upstream = CreateKTCPStream(UpstreamHost, Options);

	if (!Upstream->Good())
	{
		kPrintLine("failed to connect to upstream host {} for channel ID {}", UpstreamHost, iID);
		return;
	}

	Upstream->SetTimeout(Config.Timeout);

	kDebug(1, "connections established");

	Connection NewConnection(iID, *Downstream.get(), *Upstream.get(), Config);

} // Connection::Connect

//-----------------------------------------------------------------------------
KString ExposedServer::ReadLine(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	KString sLine;

	if (!Stream.ReadLine(sLine, iMaxControlLine))
	{
		sLine.clear();
	}

	return sLine;

} // ReadLine

//-----------------------------------------------------------------------------
bool ExposedServer::WriteLine(KIOStreamSocket& Stream, KStringView sLine)
//-----------------------------------------------------------------------------
{
	Stream.WriteLine(sLine).Flush();

	return Stream.KOutStream::Good();

} // WriteLine

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
		WriteLine(Stream, sHelo);
	}

	kDebug(1, "opened control stream");

	auto Guard = KSimpleScopeGuard([]
	{
		kPrintLine("closed control stream");
	});

	for (;;)
	{
		// send a ping every 60 seconds
		kSleep(m_Config.ControlPing);

		auto ControlStream = m_ControlStream.unique();

		auto& Control = *(ControlStream.get());

		// check if the stream is still good
		if (!Control.KInStream::Good() || !Control.KOutStream::Good())
		{
			kPrintLine("broken control stream - disconnecting");
			// no
			return;
		}

		WriteLine(Control, sPing);

		auto sResponse = ReadLine(Control);

		if (sResponse != sPong)
		{
			kPrintLine("broken control stream - disconnecting");
			// error
			return;
		}
	}

} // ControlStream

//-----------------------------------------------------------------------------
bool ExposedServer::DataStream(KIOStreamSocket& Upstream, std::size_t iID)
//-----------------------------------------------------------------------------
{
	// this is the stream between the two ktunnel instances, initiated by
	// the second, in downstream direction - this follows the initial
	// connect of the downstream client to our TCP listener (which had
	// triggered a control command to the protected host, which then
	// has opened this connection to our TLS listener)

	try
	{
		Upstream.SetTimeout(m_Config.Timeout);

		kDebug(1, "connecting data stream {}..", iID);
		// will throw if connection ID is not found
		auto Conn = GetConnection(iID, true);
		kDebug(1, "data stream {} connected!", iID);

		// register the upstream with the connection id
		Conn->SetUpstreamAndRun(Upstream);
	}
	catch(const KError& ex)
	{
		kPrintLine(">> {}", ex.what());
		return false;
	}

	return true;

} // DataStream

//-----------------------------------------------------------------------------
bool ExposedServer::ForwardStream(KIOStreamSocket& Downstream, const KTCPEndPoint& Endpoint, KStringView sInitialData)
//-----------------------------------------------------------------------------
{
	// this is initially the stream between the client and the first ktunnel instance,
	// initiating the data connection from the second instance to this one, then
	// pumping data from this instance to the second (> upstream)

	kDebug(1, "incoming connection for target {}", Endpoint);

	if (m_Connections.shared()->size() >= m_Config.iMaxTunnels)
	{
		kDebug(1, "max tunnel limit of {} reached - refusing new forward connection", m_Config.iMaxTunnels);
		Downstream.WriteLine("max tunnel limit reached").Flush();
		return false;
	}

	if (Endpoint.empty() || Endpoint.Port.get() == 0)
	{
		kPrintLine("missing target endpoint definition with domain:port");
		return false;
	}

	// is already set from KTCPServer ?
	Downstream.SetTimeout(m_Config.Timeout);

	std::size_t iConnectionID = ++m_iConnection;
	// overflow?
	if (!iConnectionID) iConnectionID = ++m_iConnection;

	auto Guard = KSimpleScopeGuard([iConnectionID]
	{
		kDebug(1, "closed forward stream {}", iConnectionID);
	});

	auto Conn = AddConnection(iConnectionID, Downstream);

	kDebug(1, "requesting reverse forward stream {}", iConnectionID);

	{
		// establish a reverse data connection
		auto Control = m_ControlStream.unique();

		if (!Control.get())
		{
			kPrintLine("have forward request, but no control stream");
			return false;
		}

		WriteLine(*Control.get(), kFormat("{} {} {}", sStart, iConnectionID, Endpoint));
	}


	Conn->WaitForUpstreamAndRun(m_Config.ConnectTimeout);

	// TODO maybe later.. write the initial incoming data (if any) to upstream
//	Conn.GetUpstream().WriteLine(sInitialData).Flush();

	return true;

} // ForwardStream

//-----------------------------------------------------------------------------
bool ExposedServer::CheckSecret(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	auto sSecret = ReadLine(Stream);

	if (m_Config.Secrets.empty() || !m_Config.Secrets.contains(sSecret))
	{
		kDebug(1, "invalid secret: {}", sSecret);
		return false;
	}

	kDebug(1, "successful login with secret: {}", sSecret);

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
bool ExposedServer::CheckConnect(KIOStreamSocket& Stream, const KTCPEndPoint& ConnectTo)
//-----------------------------------------------------------------------------
{
	return ForwardStream(Stream, ConnectTo, "");

} // CheckConnect

//-----------------------------------------------------------------------------
bool ExposedServer::CheckCommand(KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	auto sLine = ReadLine(Stream);

	auto sCommand = kGetWord(sLine);

	switch (sCommand.Hash())
	{
		case sControlStream.Hash():
			ControlStream(Stream);
			break;

		case sDataStream.Hash():
			return DataStream(Stream, sLine.UInt64());

		case sConnectStream.Hash():
			return CheckConnect(Stream, sLine);

		default:
			kDebug(1, "invalid input command: {} {}", sCommand, sLine);
			return false;
	}

	return true;

} // CheckCommand

//-----------------------------------------------------------------------------
// handles one incoming connection - could be
// - a control session, starting with #magic"secret\ncontrol\n"
// - a data    session, starting with #magic"secret\ndata <ID>\n"
// - a connect session, starting with #magic"secret\nconnect [hostname:port]\n"
//                       (which is basically a raw tcp stream to forward
//                       that configures its final endpoint itself, much
//                       like a http proxy connect)
// - a raw tcp stream to forward, starting with none of the above - only
//                       realistic if TLS on the tunnel is disabled
void ExposedServer::Session (KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	CheckMagic(Stream) &&
		CheckSecret(Stream) &&
			CheckCommand(Stream);
}

//-----------------------------------------------------------------------------
std::shared_ptr<Connection> ExposedServer::AddConnection(std::size_t iID, KIOStreamSocket& Downstream)
//-----------------------------------------------------------------------------
{
	return m_Connections.unique()->insert({ iID, std::make_shared<Connection>(iID, Downstream, m_Config) }).first->second;
}

//-----------------------------------------------------------------------------
std::shared_ptr<Connection> ExposedServer::GetConnection(std::size_t iID, bool bAndRemove)
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

	throw KError(kFormat("no connection with ID {}", iID));

} // GetConnection

//-----------------------------------------------------------------------------
bool ExposedServer::RemoveConnection(std::size_t iID)
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
// handles one incoming connection
// - a raw tcp stream to forward via TLS to the protected host
void ExposedRawServer::Session (KIOStreamSocket& Stream)
//-----------------------------------------------------------------------------
{
	m_ExposedServer.ForwardStream(Stream, m_Target);
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
		m_Config.iMaxTunnels + 1
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

				kPrintLine("cert expired at {:%F %T} - will generate new self-signed cert", MyCert.ValidUntil());

				if (kRename(m_sCertFile, kFormat("{}.bak", m_sCertFile)))
				{
					kPrintLine("renamed old cert to {}.bak", m_sCertFile);
				}
			}
			else
			{
				kPrintLine("cert valid until {:%F %T}", MyCert.ValidUntil());
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
				kPrintLine("new private key with {} bits created: {}", iBitsForNewRSAKey, m_sKeyFile);
			}
			else
			{
				kPrintLine("ephemeral private key with {} bits created", iBitsForNewRSAKey);
			}
		}
		else
		{
			// load key from file
			if (!MyKey.Load(m_sKeyFile, m_sTLSPassword)) throw KError(MyKey.GetLastError());
			kPrintLine("loaded private key from: {}", m_sKeyFile);
		}

		// create a self signed cert
		KRSACert MyCert;
		if (!MyCert.Create(MyKey, "localhost", "US", "", chrono::months(12))) throw (MyCert.GetLastError());

		if (m_bGenerateCert)
		{
			if (!MyCert.Save(m_sCertFile)) throw (MyCert.GetLastError());
			kPrintLine("new cert created: {}", m_sCertFile);
		}
		else
		{
			kPrintLine("ephemeral cert created");
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

	kPrintLine("listening on port {} for control connection, and on port {} for forward data connections", m_iPort, m_iRawPort);

	// listen on TLS
	m_ExposedServer->Start(m_Config.Timeout, /* bBlock= */ false);

	// listen on TCP
	m_ExposedRawServer->Start(m_Config.Timeout, /* bBlock= */ true);

} // StartExposedHost

//-----------------------------------------------------------------------------
void KTunnel::StartProtectedHost()
//-----------------------------------------------------------------------------
{
	KStreamOptions Options;
	// connect timeout to exposed host, also wait timeout between two tries
	Options.SetTimeout(m_Config.ConnectTimeout);

	for (;;)
	{
		kPrint("connecting {}..", m_ExposedHost);
		auto Control = CreateKTLSClient(m_ExposedHost, Options);

		if (Control->Good())
		{
			kWrite("\nlogin..");
			Control->SetTimeout(chrono::seconds(15));
			Control->WriteLine(kFormat("{}{}\n{}", sMagic, *m_Config.Secrets.begin(), sControlStream));
			Control->Flush();

			KString sLine;

			if (!Control->ReadLine(sLine, iMaxControlLine))
			{
				kWriteLine( "failed!");
				// error
				return;
			}

			if (sLine != sHelo)
			{
				kWriteLine(" failed!");
				// error
				return;
			}

			kWriteLine("\ncontrol stream opened - now waiting for data connections..");

			// now increase the timeout to allow for a once-per-minute ping
			Control->SetTimeout(m_Config.ControlPing + m_Config.ConnectTimeout);

			for (;;)
			{
				if (!Control->ReadLine(sLine, iMaxControlLine) || sLine.empty())
				{
					// error
					Control->Disconnect();
					break;
				}

				auto sCommand = kGetWord(sLine);

				switch (sCommand.Hash())
				{
					case sPing.Hash():
						kDebug(1, "got ping");
						// respond with pong
						Control->WriteLine(sPong);
						Control->Flush();
						break;

					case sStart.Hash():
					{
						// open a new stream to endpoint, and data stream to exposed host
						kDebug(1, "got start command: {}", sLine);
						auto sID = kGetWord(sLine);
						sLine.Trim();
						KTCPEndPoint TargetHost(sLine);
						m_Threads.Create(Connection::ConnectHostnames, sID.UInt64(), m_ExposedHost, std::move(TargetHost), m_Config);
						break;
					}

					default:
						// error
						kPrintLine("got bad control command: {}", sCommand);
						Control->Disconnect();
						break;
				}
			}
			kPrintLine("lost connection - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}
		else
		{
			kPrintLine("cannot connect - now sleeping for {} and retrying", m_Config.ConnectTimeout);
		}

		kSleep(m_Config.ConnectTimeout);
	}

} // StartProtectedHost

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
	m_ExposedHost          = Options("e,exposed    : exposed host - the host to keep an ongoing control connection to. Expects domain name or IP address. If not defined, then this is the exposed host itself.", "");
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

	// do a final check if all required options were set
	if (!Options.Check()) return 1;

	if (!m_iPort)
	{
		SetError("need valid port number");
	}

	if (m_ExposedHost.Port.empty())
	{
		m_ExposedHost.Port.get() = m_iPort;
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
		kPrintLine("starting as exposed host");

		StartExposedHost();
	}
	else
	{
		kPrintLine("starting as protected host, connecting exposed host at {}", m_ExposedHost);

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
