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

#include <dekaf2/net/util/ktunnel.h>
#include <dekaf2/util/cli/koptions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/system/os/ksystem.h>
#include <dekaf2/web/url/kurl.h>
#include <dekaf2/crypto/rsa/krsakey.h>
#include <dekaf2/crypto/rsa/krsacert.h>
#include <dekaf2/rest/framework/krest.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/web/objects/kwebobjects.h>
#include <dekaf2/http/websocket/kwebsocket.h>
#include <dekaf2/http/websocket/kwebsocketclient.h>
#include <dekaf2/crypto/encoding/kencode.h>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>

using namespace dekaf2;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class ExtendedConfig : public KTunnel::Config
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
	bool                   bNoTLS         { false };
	bool                   bQuiet         { false };

//----------
private:
//----------

	void PrintMessage (KStringView sMessage) const;

}; // ExtendedConfig

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

	std::shared_ptr<KTunnel>          m_Tunnel;
	std::mutex                        m_TunnelMutex;
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

	/// Run the reconnect loop (blocking). Returns cleanly when Shutdown() is
	/// called from a signal handler or service-control handler.
	void Run      ();

	/// Request a graceful shutdown of the reconnect loop. Thread-safe and
	/// safe to call from a signal / service-control handler: sets the quit
	/// flag, tears down the currently active KTunnel if any (so its
	/// blocking Run() returns), and wakes the inter-retry sleep.
	void Shutdown ();

//----------
private:
//----------

	const ExtendedConfig&           m_Config;
	std::atomic<bool>               m_bQuit            { false };
	std::mutex                      m_Mutex;
	std::condition_variable         m_CV;
	KTunnel*                        m_pCurrentTunnel   { nullptr };

}; // ProtectedHost

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class Tunnel : public KErrorBase
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
