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
class Message
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
		Disconnect,
		None        = -1
	};

	Message() = default;
	Message(Type _type, std::size_t iChannel, KStringView sMessage = KStringView{});

	void           Read       (KIOStreamSocket& Stream);
	void           Write      (KIOStreamSocket& Stream) const;

	Type           GetType    () const { return m_Type;     }
	const KString& GetMessage () const { return m_sMessage; }
	std::size_t    GetChannel () const { return m_iChannel; }

	void           SetType    (Type _type)           { m_Type     = _type;               }
	void           SetMessage (KString sMessage)     { m_sMessage = std::move(sMessage); }
	void           SetChannel (std::size_t iChannel) { m_iChannel = iChannel;            }

	void           clear      ();
	std::size_t    size       () const { return m_sMessage.size(); }

	KString        Debug      () const;
	KStringView    PrintType  () const;

//----------
private:
//----------

	KStringView    PrintData  () const;

	void           Throw      (KString sError, KStringView sFunction = KSourceLocation::current().function_name()) const;

	uint8_t        ReadByte   (KIOStreamSocket& Stream) const;
	void           WriteByte  (KIOStreamSocket& Stream, uint8_t iByte) const;

	KString        m_sMessage;
	std::size_t    m_iChannel { 0 };
	Type           m_Type     { None };

}; // Message

//-----------------------------------------------------------------------------
inline KIOStreamSocket& operator <<(KIOStreamSocket& stream, const Message& message)
//-----------------------------------------------------------------------------
{
	message.Write(stream);
	stream.Flush();
	return stream;
}

//-----------------------------------------------------------------------------
inline KIOStreamSocket& operator >>(KIOStreamSocket& stream, Message& message)
//-----------------------------------------------------------------------------
{
	message.Read(stream);
	return stream;
}

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Connection
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Connection (std::size_t iID, std::function<void(const Message&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);

	void        SetDirectStream       (KIOStreamSocket* DirectStream) { m_DirectStream = DirectStream; }

	void        PumpToTunnel          ();
	void        PumpFromTunnel        ();

	void        SendData              (std::unique_ptr<Message> FromTunnel);
	void        Disconnect            ();

	std::size_t GetID                 () const { return m_iID; }

//----------
private:
//----------

	std::queue<std::unique_ptr<Message>> m_MessageQueue;
	std::function<void(const Message&)> m_Tunnel;
	KIOStreamSocket*                    m_DirectStream { nullptr };
	std::mutex                          m_QueueMutex;
	std::condition_variable             m_FreshData;
	std::size_t                         m_iID          { 0 };
	bool                                m_bQuit        { false };

}; // Connection

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Connections
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	Connections(std::size_t iMaxConnections) : m_iMaxConnections(iMaxConnections) {}

	std::shared_ptr<Connection> Create (std::size_t iID, std::function<void(const Message&)> TunnelSend, KIOStreamSocket* DirectStream = nullptr);
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
class ExposedServer
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

	void SendMessageToUpstream (const Message& message);

	void ControlStreamRX (KIOStreamSocket& Stream);
	void ControlStreamTX (KIOStreamSocket& Stream);
	bool Login           (KIOStreamSocket& Stream);

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
class ProtectedHost
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	ProtectedHost (const CommonConfig& Config);

//----------
private:
//----------

	void SendMessageToDownstream (const Message& message, bool bThrowIfNoStream = true);
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
