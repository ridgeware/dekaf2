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
#include <thread>
#include <future>
#include <memory>

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
class Connection
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static void ConnectHostnames (std::size_t iID, KTCPEndPoint DownstreamHost, KTCPEndPoint UpstreamHost, const CommonConfig& Config);

	Connection (std::size_t iID, KIOStreamSocket& Downstream, KIOStreamSocket& Upstream, const CommonConfig& Config);
	Connection (std::size_t iID, KIOStreamSocket& Downstream, const CommonConfig& Config);

	bool        WaitForUpstreamAndRun (KDuration ConnectTimeout);
	bool        SetUpstreamAndRun     (KIOStreamSocket& upstream);

//----------
protected:
//----------

//----------
private:
//----------

	void        DataPump (KIOStreamSocket& Left, KIOStreamSocket& Right, bool bWriteToUpstream);

	KIOStreamSocket*                 m_Downstream { nullptr };
	KIOStreamSocket*                 m_Upstream   { nullptr };
	const CommonConfig&              m_Config;
	std::size_t                      m_iID        { 0 };
	std::promise<void>               m_HaveUpstream;
	std::promise<void>               m_UpstreamDone;
	std::promise<void>               m_DownstreamDone;
	bool                             m_bShutdown  { false };

}; // Connection


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExposedServer : public KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	template<typename... Args>
	ExposedServer (const CommonConfig& config, Args&&... args)
	: KTCPServer (std::forward<Args>(args)...)
	, m_Config(config)
	{
	}

	bool ForwardStream (KIOStreamSocket& Downstream, const KTCPEndPoint& Endpoint, KStringView sInitialData = "");

//----------
protected:
//----------

	KString     ReadLine           (KIOStreamSocket& Stream);
	bool        WriteLine          (KIOStreamSocket& Stream, KStringView sLine);
	std::size_t GetNewConnectionID ();

	void ControlStream (KIOStreamSocket& Stream);
	bool DataStream    (KIOStreamSocket& Upstream, std::size_t iID);
	bool CheckSecret   (KIOStreamSocket& Stream);
	bool CheckMagic    (KIOStreamSocket& Stream);
	bool CheckConnect  (KIOStreamSocket& Stream, const KTCPEndPoint& ConnectTo);
	bool CheckCommand  (KIOStreamSocket& Stream);
	void Session       (KIOStreamSocket& Stream) override final;
 
//----------
private:
//----------

	std::shared_ptr<Connection> AddConnection    (std::size_t iID, KIOStreamSocket& Downstream);
	std::shared_ptr<Connection> GetConnection    (std::size_t iID, bool bAndRemove);
	bool                        RemoveConnection (std::size_t iID);

	KThreadSafe<KUnorderedMap<std::size_t, std::shared_ptr<Connection>>>
	                              m_Connections;
	KThreadSafe<KIOStreamSocket*> m_ControlStream;
	std::atomic<std::size_t>      m_iConnection { 0 };
	const CommonConfig&           m_Config;

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

	void Session (KIOStreamSocket& Stream) override final;

//----------
private:
//----------

	ExposedServer& m_ExposedServer;
	KTCPEndPoint   m_Target;

}; // ExposedRawServer


//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KTunnel : public KErrorBase
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
private:
//----------

	void ExposedHost   ();
	void ProtectedHost ();

//----------
public:
//----------

	int  Main               (int argc, char** argv);

	/// is this the exposed host?
	bool IsExposed          () const
	{
		return m_ExposedHost.empty();
	}

//----------
private:
//----------

	std::unique_ptr<ExposedServer>    m_ExposedServer;
	std::unique_ptr<ExposedRawServer> m_ExposedRawServer;

	KThreads     m_Threads;
	CommonConfig m_Config;
	KTCPEndPoint m_ExposedHost;
	KString      m_sCertFile;
	KString      m_sKeyFile;
	KString      m_sTLSPassword;
	KString      m_sAllowedCipherSuites;
	uint16_t     m_iPort;
	uint16_t     m_iRawPort;
	bool         m_bGenerateCert;

}; // KTunnel
