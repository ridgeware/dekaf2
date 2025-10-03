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

#pragma once

#include <dekaf2/koptions.h>
#include <dekaf2/kerror.h>
#include <dekaf2/ktlsstream.h>
#include <dekaf2/ktcpstream.h>
#include <dekaf2/ktcpserver.h>
#include <dekaf2/ksystem.h>
#include <dekaf2/dekaf2.h>
#include <dekaf2/kassociative.h>
#include <dekaf2/kurl.h>
#include <dekaf2/kthreadsafe.h>
#include <dekaf2/krsakey.h>
#include <dekaf2/krsacert.h>
#include <dekaf2/kbuffer.h>
#include <dekaf2/kthreads.h>
#include <dekaf2/kscopeguard.h>
#include <dekaf2/ksourcelocation.h>
#include <dekaf2/krest.h>
#include <dekaf2/khttperror.h>
#include <dekaf2/kwebobjects.h>
#include <dekaf2/kwebsocket.h>
#include <dekaf2/kwebsocketclient.h>
#include <dekaf2/kencode.h>
#include <thread>
#include <future>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
struct CommonConfig
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	KUnorderedSet<KString> Secrets;
	KDuration              Timeout        { chrono::seconds(15)        };
	KDuration              ControlPing    { chrono::seconds(60)        };
	KDuration              ConnectTimeout { chrono::seconds(15)        };
	KDuration              PollTimeout    { chrono::milliseconds(2000) };
	uint16_t               iMaxTunnels    { 20 };
	bool                   bQuiet         { false };

}; // CommonConfig

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExtendedConfig : public CommonConfig
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	template<class... Args>
	void Message (KFormatString<Args...> sFormat, Args&&... args) const
	{
		PrintMessage(kFormat(sFormat, std::forward<Args>(args)...));
	}

	KTCPEndPoint           DefaultTarget;
	KTCPEndPoint           ExposedHost;

//----------
private:
//----------

	void PrintMessage (KStringView sMessage) const;

}; // ExtendedConfig

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the message protocol used on top of the websocket frame, basically adding channels and (own) types
class Message : protected KWebSocket::Frame
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum Type
	{
		Login       = 1,
		Helo,
		Ping,
		Pong,
		Idle,
		Control,
		Connect,
		Data,
		Pause,
		Resume,
		Disconnect,
		None        = -1
	};

	/// default construct
	Message() = default;
	/// construct from discrete parameters
	Message(Type _type, std::size_t iChannel, KString sMessage = KString{});

	/// read from a streamsocket
	void           Read       (KIOStreamSocket& Stream);
	/// write to a streamsocket - if this is in websocket protocol and we are a "client", then
	/// bMask must be set to true, else to false
	void           Write      (KIOStreamSocket& Stream, bool bMask);

	/// returns the message type
	Type           GetType    () const { return static_cast<Type>(m_Preamble[0]); }
	/// returns the message as a ref
	const KString& GetMessage () const { return GetPayload(); }
	/// returns the channel ID this message was sent to
	std::size_t    GetChannel () const;

	/// sets the message type
	void           SetType    (Type _type)           { m_Preamble[0] = _type;                 }
	/// sets the message (payload)
	void           SetMessage (KString sMessage)     { SetPayload(std::move(sMessage), true); }
	/// sets the channel the message is sent to
	void           SetChannel (std::size_t iChannel);

	/// clears the message
	void           clear      ();
	/// returns the payload size
	std::size_t    size       () const { return GetPayload().size(); }

	/// returns max channel number
	static constexpr
	std::size_t    MaxChannel ()       { return (1 << 24) - 1;       }

	/// returns a debug string with core data about the message
	KString        Debug      () const;
	/// prints the message type in ASCII
	KStringView    PrintType  () const;

//----------
protected:
//----------

	std::size_t    GetPreambleSize () const override final;
	char*          GetPreambleBuf  () const override final;

//----------
private:
//----------

	/// prints the message content except for Data and Unknown types
	KStringView    PrintData  () const;
	/// throws and prints a description as of why
	void           Throw      (KString sError, KStringView sFunction = KSourceLocation::current().function_name()) const;

	mutable std::array<char, 4> m_Preamble{};

}; // Message

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Connection
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Connection (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);

	void        SetDirectStream       (KIOStreamSocket* DirectStream) { m_DirectStream = DirectStream; }

	void        PumpToTunnel          ();
	void        PumpFromTunnel        ();

	/// puts data into the direct connection's queue, may force a Pause frame if queue is too large
	void        SendData              (Message&& FromTunnel);
	void        Disconnect            ();

	std::size_t GetID                 () const { return m_iID;      }

	/// pause sending frames for this connection
	void        Pause                 ()       { m_bPaused = true;  }
	/// resume sending frames for this connection
	void        Resume                ();
	/// check if this connection shall pause sending frames
	bool        IsPaused              () const { return m_bPaused;  }

	/// max size for the message queue for one connection
	static constexpr
	std::size_t MaxMessageQueueSize   ()       { return 20;         }

//----------
private:
//----------

	std::queue<Message>                  m_MessageQueue;
	std::function<void(Message&&)>       m_Tunnel;
	KIOStreamSocket*                     m_DirectStream { nullptr };
	std::mutex                           m_QueueMutex;
	std::condition_variable              m_FreshData;
	std::mutex                           m_TunnelMutex;
	std::condition_variable              m_ResumeTunnel;
	std::size_t                          m_iID          { 0 };
	bool                                 m_bRXPaused    { false };
	bool                                 m_bPaused      { false };
	bool                                 m_bQuit        { false };

}; // Connection

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Connections
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Connections(std::size_t iMaxConnections) : m_iMaxConnections(iMaxConnections) {}

	std::shared_ptr<Connection> Create (std::size_t iID, std::function<void(Message&&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);
	std::shared_ptr<Connection> Get    (std::size_t iID, bool bAndRemove);
	bool                        Remove (std::size_t iID);
	bool                        Exists (std::size_t iID);
	std::size_t                 size   () const;

//----------
private:
//----------

	KThreadSafe<KUnorderedMap<std::size_t, std::shared_ptr<Connection>>> m_Connections;

	std::size_t m_iConnection     { 0 };
	std::size_t m_iMaxConnections { 50 };

}; // Connections

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// The web socket protocol requires to XOR the data sent by a "client" to the endpoint that was originally
/// responding as a web "server" (but not into the other direction). This appears erratic, as websockets are
/// supposed to be bidirectional transports with no distinction for the data, but it makes sense at least for
/// non-TLS transports to mask the sent data in a way that a middleware box could not manipulate it in
/// good spirit of seeing HTTP traffic. But this means we need to know the "direction" of the bidirectional
/// transport all the time we access the stream sockets.
class Tunnel
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// construct with bSetMaskTx set to true if this is a "client", else with false
	Tunnel
	(
		const CommonConfig&              Config,
		std::unique_ptr<KIOStreamSocket> Stream,
		bool                             bIsClient
	);

	// dtor
	~Tunnel();

	/// if we are establishing the connection we have to login with user/secret
	bool                        Login             (KStringView sUser, KStringView sSecret);
	/// connect an incoming direct stream with the tunnel and the endpoint at the other side of the tunnel
	std::shared_ptr<Connection> Connect           (KIOStreamSocket* DirectStream, const KTCPEndPoint& Endpoint);
	/// returns the ip address of the opposite tunnel endpoint
	KTCPEndPoint                GetEndPointAddress() const;

	/// run the event handler for this tunnel - may throw or return on error
	void Run             ();

//----------
protected:
//----------

	/// returns true if a stream is set and is good for reading and writing
	bool HaveTunnel      () const;
	/// read a message from the stream, blocks until timeout, then throws
	void ReadMessage     (Message&  message);
	/// write a message to the stream, blocks until timeout, then throws
	void WriteMessage    (Message&& message);
	/// waits for the other tunnel side to connect
	void WaitForLogin    ();
	/// set timeout for the tunnel connection
	void SetTimeout      (KDuration Timeout);
	/// force pings to check the tunnel
	void PingTest        (KUnixTime Time);
	/// connects to an outside target from one tunnel end
	void ConnectToTarget (std::size_t iID, KTCPEndPoint Target);

//----------
private:
//----------

	/// danger!
	KIOStreamSocket* GetStream() { return m_Tunnel.shared()->Stream.get(); }

	void TimerLoop       ();

	struct TunnelEnv
	{
		std::unique_ptr<KIOStreamSocket> Stream;
		KStopTime                        LastTx;
		KStopTime::Clock::time_point     SendIdleNotBefore;
	};

	const CommonConfig&    m_Config;
	KThreadSafe<TunnelEnv> m_Tunnel;
	Connections            m_Connections;
	KThreads               m_Threads;
	KDuration              m_RTT;
	KTimer::ID_t           m_TimerID { KTimer::InvalidID };
	std::thread            m_Timer;
	bool                   m_bIsClient     { false };
	bool                   m_bQuit         { false };

}; // Tunnel

class ExposedRawServer;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExposedServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	struct Config : public ExtendedConfig
	{
		KString  sCertFile;
		KString  sKeyFile;
		KString  sTLSPassword;
		KString  sCipherSuites;
		uint16_t iPort    { 0 };
		uint16_t iRawPort { 0 };
		bool     bPersistCert { false };
	};

	ExposedServer (const Config& config);

	void ForwardStream (KIOStreamSocket& Downstream, const KTCPEndPoint& Endpoint);

//----------
protected:
//----------

	void ControlStream (std::unique_ptr<KIOStreamSocket> Stream);

//----------
private:
//----------

	std::unique_ptr<Tunnel>           m_Tunnel;
	std::unique_ptr<ExposedRawServer> m_ExposedRawServer;
	const Config&                     m_Config;

}; // ExposedServer


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExposedRawServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	template<typename... Args>
	ExposedRawServer (ExposedServer* Exposed, const KTCPEndPoint& Target, Args&&... args)
	: KTCPServer (std::forward<Args>(args)...)
	, m_ExposedServer(*Exposed)
	, m_Target(Target)
	{
	}

//----------
protected:
//----------

	void Session (std::unique_ptr<KIOStreamSocket>& Stream) override final;

//----------
private:
//----------

	ExposedServer& m_ExposedServer;
	KTCPEndPoint   m_Target;

}; // ExposedRawServer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ProtectedHost
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	ProtectedHost (const ExtendedConfig& Config);

//----------
private:
//----------

	const ExtendedConfig&  m_Config;

}; // ProtectedHost

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KTunnel : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	int  Main               (int argc, char** argv);

	/// is this the exposed host?
	bool IsExposed          () const
	{
		return m_Config.ExposedHost.empty();
	}

//----------
private:
//----------

	ExposedServer::Config m_Config;

}; // KTunnel
