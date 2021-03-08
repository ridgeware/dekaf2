/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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

#include "krestserver.h"
#include "ktcpserver.h"
#include <csignal>

/// @file krest.h
/// HTTP REST service implementation

namespace dekaf2 {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KREST
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum ServerType { UNDEFINED, HTTP, CGI, FCGI, LAMBDA, CLI, SIMULATE_HTTP
#ifdef DEKAF2_HAS_UNIX_SOCKETS
	                  , UNIX
#endif
					};

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Options for simulation mode
	struct SimulationParms
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// filename to read HTTP request from
		KStringViewZ sFilename;
		/// API to call
		KResource API;
		/// HTTP method to use
		KHTTPMethod Method;
		/// Post body if not read from file
		KString sBody;
		/// Additional request headers
		KHTTPRequestHeaders::KHeaderMap AdditionalRequestHeaders;

	}; // SimulationParms

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// define options for the rest service
	struct Options : public KRESTServer::Options
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// Server type (UNDEFINED, HTTP, CGI, FCGI, LAMBDA, CLI, SIMULATE_HTTP, UNIX)
		ServerType Type { UNDEFINED };
		/// listen port (default none)
		uint16_t iPort { 0 };
		/// max simultaneous connections (default 20)
		uint16_t iMaxConnections { 20 };
		/// timeout in seconds (default 2)
		uint16_t iTimeout { 2 };
		/// signals that will shutdown the server (default SIGINT, SIGTERM)
		std::vector<int> RegisterSignalsForShutdown { SIGINT, SIGTERM };
#ifdef DEKAF2_HAS_UNIX_SOCKETS
		/// unix socket file to listen
		KStringViewZ sSocketFile;
#endif
		/// start as blocking server in same thread or unblocking in separate thread
		bool bBlocking { true };
		/// are the PEM cert and key filenames (default) or buffers with the actual PEMs?
		bool bPEMsAreFilenames { true };
		/// the PEM certificate
		KStringViewZ sCert;
		/// the PEM private key
		KStringViewZ sKey;
		/// Parameters controling the simulation mode
		SimulationParms Simulate;

	}; // Options

	/// handle one REST request, or start REST server in HTTP and UNIX modes
	bool ExecuteRequest(const Options& Options, const KRESTRoutes& Routes);
	/// handle one REST request, read input from file, output to OutStream
	bool ExecuteFromFile(const Options& Options, const KRESTRoutes& Routes, KOutStream& OutStream = KOut);
	/// simulate one REST request in HTTP/CGI mode, call API, output to OutStream
	bool Simulate(const Options& Options, const KRESTRoutes& Routes, const KResource& API, KOutStream& OutStream = KOut);
	/// call either of the three other execution methods depending on Options.Type and sFilenameOrSimulation
	bool Execute(const Options& Options, const KRESTRoutes& Routes);
	/// returns true if no error
	bool Good() const;
	/// returns error description
	const KString& Error() const;
	/// alias for Error()
	const KString& GetLastError() const { return Error(); }
	/// get diagnostics when running with a TCP server
	KThreadPool::Diagnostics GetDiagnostics() const;
	/// Shall we log the shutdown in TCP server mode?
	/// @param callback callback function called at each shutdown thread with some diagnostics
	void RegisterShutdownCallback(KThreadPool::ShutdownCallback callback);

//----------
protected:
//----------

	bool RealExecute(const Options& Options, const KRESTRoutes& Routes, KStream& Stream, KStringView sRemoteIP = "0.0.0.0");
	bool SetError(KStringView sError);

//----------
private:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class RESTServer : public KTCPServer
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//----------
	public:
	//----------

		//-----------------------------------------------------------------------------
		template<typename... Args>
		RESTServer(const KREST::Options& Options, const KRESTRoutes& Routes, Args&&... args)
		//-----------------------------------------------------------------------------
			: KTCPServer(std::forward<Args>(args)...)
			, m_Options(Options)
			, m_Routes(Routes)
		{
		}

		//-----------------------------------------------------------------------------
		void Session (KStream& Stream, KStringView sRemoteEndpoint) override final
		//-----------------------------------------------------------------------------
		{
			KRESTServer Request;

			Request.Accept(Stream, sRemoteEndpoint);
			Request.Execute(m_Options, m_Routes);
			Request.Disconnect();

		} // Session


	//----------
	protected:
	//----------

		const KREST::Options& m_Options;
		const KRESTRoutes&    m_Routes;

	}; // RESTServer

	KString                       m_sError;
	std::unique_ptr<RESTServer>   m_Server;
	KThreadPool::ShutdownCallback m_ShutdownCallback;

}; // KREST

} // end of namespace dekaf2
