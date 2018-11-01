/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

// large portions of this code are taken from
// https://github.com/JoachimSchurig/CppCDDB/blob/master/asioserver.hpp
// which is under a BSD style open source license

//  Copyright © 2016 Joachim Schurig. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this
//  list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//  this list of conditions and the following disclaimer in the documentation
//  and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

/// @file ktcpserver.h
/// TCP server implementation with SSL/TLS

#include <cinttypes>
#include <thread>
#include <boost/asio/io_service.hpp>
#include "kstream.h"
#include "kstring.h"
#include "kthreadpool.h"

namespace dekaf2
{

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A TCP server implementation supporting SSL/TLS.
class KTCPServer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//-------
public:
//-------

	using self_type     = KTCPServer;

	//-----------------------------------------------------------------------------
	/// Construct a server, but do not yet start it.
	/// @param iPort Port to bind to
	/// @param bSSL If true will use SSL/TLS
	KTCPServer(uint16_t iPort, bool bSSL, uint16_t iMaxConnections = 50);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a server, but do not yet start it.
	/// @param sSocketFile Unix domain socket to bind to
	KTCPServer(KStringView sSocketFile, uint16_t iMaxConnections = 50);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	virtual ~KTCPServer();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KTCPServer(const self_type&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KTCPServer(self_type&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self_type& operator=(const self_type&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self_type& operator=(self_type&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// allow IPv4 connections only
	void v4_Only()
	//-----------------------------------------------------------------------------
	{
		m_bStartIPv4 = true;
		m_bStartIPv6 = false;
	}

	//-----------------------------------------------------------------------------
	/// allow IPv6 connections only
	void v6_Only()
	//-----------------------------------------------------------------------------
	{
		m_bStartIPv4 = false;
		m_bStartIPv6 = true;
	}

	//-----------------------------------------------------------------------------
	/// allow IPv4 and IPv6 connections (the default after construction)
	void v4_And_6()
	//-----------------------------------------------------------------------------
	{
		m_bStartIPv4 = true;
		m_bStartIPv6 = true;
	}

	//-----------------------------------------------------------------------------
	/// Set the SSL/TLS certificate files
	bool LoadSSLCertificates(KStringView sCert, KStringView sKey);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the SSL/TLS certificates as strings
	bool SetSSLCertificates(KStringView sCert, KStringView sKey);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Start the server
	/// @param iTimeoutInSeconds Timeout for I/O operations in seconds (default 300)
	/// @param bBlock If true will only return when server is destructed. If false
	/// starts a server thread and returns immediately.
	bool Start(uint16_t iTimeoutInSeconds = 5 * 60, bool bBlock = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Stop the server
	bool Stop();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Tests if the server is already running
	bool IsRunning() const
	//-----------------------------------------------------------------------------
	{
		return m_ipv6_server || m_ipv4_server;
	}

	//-----------------------------------------------------------------------------
	/// Checks if the given port can be bound to
	static bool IsPortAvailable(uint16_t iPort);
	//-----------------------------------------------------------------------------

//-------
protected:
//-------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// struct that could be expanded by implementing classes to carry on
	/// control parameters for one open session.
	struct Parameters
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		//-----------------------------------------------------------------------------
		virtual ~Parameters();
		//-----------------------------------------------------------------------------

		//-----------------------------------------------------------------------------
		bool terminate{false};
		//-----------------------------------------------------------------------------
	};

	typedef std::unique_ptr<Parameters> param_t;

	//-----------------------------------------------------------------------------
	/// Virtual hook to override with a completely new session management logic
	/// (either calling Accept(), CreateParameters(), Init() and Request() below,
	/// or anything else)
	virtual void Session(KStream& stream, KStringView sRemoteEndPoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Virtual hook that is called immediately after accepting a new stream.
	/// Default does nothing. Could be used to set stream parameters. If
	/// return value is false connection is terminated.
	virtual bool Accepted(KStream& stream, KStringView sRemoteEndPoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Virtual hook to send a init message to the client, directly after
	/// accepting the incoming connection. Default sends nothing.
	virtual KString Init(Parameters& parameters);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Virtual hook to process one line of client requests
	virtual KString Request(KString& qstr, Parameters& parameters);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Request the stream timeout requested for this instance (set it in own
	/// session handlers for reading and writing on the stream)
	inline uint16_t GetTimeout() const
	//-----------------------------------------------------------------------------
	{
		return m_iTimeout;
	}

	//-----------------------------------------------------------------------------
	/// If the derived class needs addtional per-thread control parameters,
	/// define a Parameters class to accomodate those, and return an instance
	/// of this class from CreateParameters()
	virtual param_t CreateParameters();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if the server is in SSL/TLS mode.
	inline bool IsSSL() const
	//-----------------------------------------------------------------------------
	{
		return m_bIsSSL;
	}

//-------
private:
//-------

	//-----------------------------------------------------------------------------
	void TCPServer(bool ipv6);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void UnixServer();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void RunSession(KStream& stream, KString sRemoteEndPoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void StopServerThread(bool ipv6);
	//-----------------------------------------------------------------------------

	boost::asio::io_service m_asio;
	std::unique_ptr<std::thread> m_ipv4_server;
	std::unique_ptr<std::thread> m_ipv6_server;
	std::unique_ptr<KThreadPool> m_ThreadPool;
	KString m_sSocketFile;
	KString m_sCert;
	KString m_sKey;
	uint16_t m_iPort { 0 };
	uint16_t m_iTimeout { 1*30 };
	bool m_bBlock { true };
	bool m_bQuit { false };
	bool m_bStartIPv4 { true };
	bool m_bStartIPv6 { true };
	bool m_bIsSSL { false };
	bool m_bBufferedCerts { false };

}; // KTCPServer

}

