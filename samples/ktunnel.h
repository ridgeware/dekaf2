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
class CommonConfig
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
	KUnorderedSet<KString> Secrets;
	KDuration              Timeout        { chrono::seconds(15)        };
	KDuration              ControlPing    { chrono::seconds(60)        };
	KDuration              ConnectTimeout { chrono::seconds(15)        };
	KDuration              PollTimeout    { chrono::milliseconds(2000) };
	uint16_t               iMaxTunnels    { 20 };
	bool                   bQuiet         { false };

//----------
private:
//----------

	void PrintMessage (KStringView sMessage) const;

}; // CommonConfig

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
		LoginTX     = 0,
		LoginRX,
		Helo,
		Ping,
		Pong,
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

	Connection (std::size_t iID, std::function<void(Message&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);

	void        SetDirectStream       (KIOStreamSocket* DirectStream) { m_DirectStream = DirectStream; }

	void        PumpToTunnel          ();
	void        PumpFromTunnel        ();

	/// puts data into the direct connection's queue, may force a Pause frame if queue is too large
	void        SendData              (std::unique_ptr<Message> FromTunnel);
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

	std::queue<std::unique_ptr<Message>> m_MessageQueue;
	std::function<void(Message&)>        m_Tunnel;
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

	std::shared_ptr<Connection> Create (std::size_t iID, std::function<void(Message&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);
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

class ExposedRawServer;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class WebSocketDirection
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	WebSocketDirection(bool bSetMaskTx)
	: m_bMaskTx(bSetMaskTx)
	{
	}

	bool ReadMessage     (KIOStreamSocket& Stream, Message& message);
	bool WriteMessage    (KIOStreamSocket& Stream, Message& message);

//----------
protected:
//----------

	void SetMaskTx       (bool bYesNo)             { m_bMaskTx = bYesNo;      }

//----------
private:
//----------

	bool m_bMaskTx      { false };

}; // WebSocketDirection

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExposedServer : protected WebSocketDirection
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	struct Config : public CommonConfig
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

	void ForwardStream (KIOStreamSocket& Downstream, const KTCPEndPoint& Endpoint, KStringView sInitialData = "");

//----------
protected:
//----------

	void SendMessageToUpstream (Message& message);

	void ControlStreamRX (KIOStreamSocket& Stream);
	void ControlStreamTX (KIOStreamSocket& Stream);
	bool Login           (KIOStreamSocket& Stream);
	void WSLogin         (KWebSocket& WebSocket);

//----------
private:
//----------

	Connections                       m_Connections;
	KThreadSafe<KIOStreamSocket*>     m_ControlStreamTX;
	KThreadSafe<KIOStreamSocket*>     m_ControlStreamRX;
	std::unique_ptr<ExposedRawServer> m_ExposedRawServer;
	const Config&                     m_Config;
	std::promise<void>                m_WaitForRX;
	std::promise<void>                m_Quit;

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
class ProtectedHost : protected WebSocketDirection
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	ProtectedHost (const CommonConfig& Config);

//----------
private:
//----------

	void SendMessageToDownstream (Message& message, bool bThrowIfNoStream = true);
	void TimingCallback          (KUnixTime Time);
	void ConnectToTarget         (std::size_t iID, KTCPEndPoint Target);

	Connections          m_Connections;
	KThreadSafe<std::unique_ptr<KIOStreamSocket>>
	                     m_ControlStreamTX;
	KThreads             m_Threads;
	const CommonConfig&  m_Config;
	KTimer::ID_t         m_TimerID { KTimer::InvalidID };

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
