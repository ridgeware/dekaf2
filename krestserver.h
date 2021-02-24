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

#include "khttpserver.h"
#include "krestroute.h"
#include "kstring.h"
#include "kstringview.h"
#include "kjson.h"
#include "kxml.h"
#include "ktimer.h"
#include "kopenid.h"
#include <vector>
#include <memory>

/// @file krestserver.h
/// HTTP REST server implementation

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// HTTP REST server with JSON input / output
class KRESTServer : public KHTTPServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum Timer
	{
		RECEIVE   = 0,
		ROUTE     = 1,
		PARSE     = 2,
		PROCESS   = 3,
		SERIALIZE = 4,
		SEND      = 5
	};

	struct TimerLabel
	{
		Timer       Value;
		KStringView sLabel;
	};

	static constexpr std::array<TimerLabel, SEND + 1> Timers
	{{
		{ RECEIVE   , "rx"        },
		{ ROUTE     , "route"     },
		{ PARSE     , "parse"     },
		{ PROCESS   , "process"   },
		{ SERIALIZE , "serialize" },
		{ SEND      , "tx"        }
	}};

	// supported output types:
	// HTTP is standard,
	// LAMBDA is AWS specific,
	// CLI is a test console output
	enum OutputType	{ HTTP, LAMBDA, CLI };

	// forward constructors
	using KHTTPServer::KHTTPServer;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct Options
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		enum AUTH_LEVEL
		{
			ALLOW_ALL,
			ALLOW_ALL_WITH_AUTH_HEADER,
			VERIFY_AUTH_HEADER
		};

		/// Add one header to the list of fixed additional headers
		void AddHeader(KHTTPHeader Header, KStringView sValue);

		/// Set the file name for the json access log
		bool SetJSONAccessLog(KStringViewZ sJSONAccessLogFile);

		/// Fixed route prefix
		KStringView sBaseRoute;
		/// If non-empty creates a header with execution time
		KStringView sTimerHeader;
		/// File to record request into - filename may not change during execution
		KStringViewZ sRecordFile;
		/// Fixed additional headers
		KHTTPHeaders::KHeaderMap ResponseHeaders;
		/// Valid authentication instances for user verification
		KOpenIDProviderList Authenticators;
		/// If non-empty, check that SSO token authorizes one of thse given scopes (comma separated list)
		KStringView sAuthScope;
		/// Allow KLog profiling triggered by a KLOG header?
		KStringView sKLogHeader;
		/// Server name for this instance, will be used in diagnostic output
		KStringView sServername;
		/// DoS prevention - max rounds in keep-alive
		mutable uint16_t iMaxKeepaliveRounds { 10 };
		/// Which of the three output formats?
		mutable OutputType Out { HTTP };
		/// Which authentication level?
		AUTH_LEVEL AuthLevel { ALLOW_ALL };
		/// Shall we record into the sRecordFile? Value is expected to change during execution (could be made an atomic, but we don't care for a few missing records)
		bool bRecordRequest { false };
		/// Shall we throw if the request body contains invalid JSON?
		bool bThrowIfInvalidJson { false };
		/// If no route found, shall we check if that happened because of a wrong request method?
		bool bCheckForWrongMethod { true };
		/// Set a callback function that will receive references to this instance and the TimeKeepers
		/// after termination of one request
		std::function<void(const KRESTServer&, const KDurations&)> TimingCallback;
		/// The stream used for the JSON access log writer - normally set by SetJSONAccessLog()
		std::unique_ptr<KOutStream> JSONLogStream;

	}; // Options

	//-----------------------------------------------------------------------------
	/// handler for one request
	bool Execute(const Options& Options, const KRESTRoutes& Routes);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the content body of a POST or PUT request
	const KString& GetRequestBody() const
	//-----------------------------------------------------------------------------
	{
		return m_sRequestBody;
	}

	//-----------------------------------------------------------------------------
	/// set status code and corresponding default status string
	void SetStatus(int iCode);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set file output (mutually exclusive to other output types)
	bool SetFileToOutput(KStringViewZ sFile);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set file output (mutually exclusive to other output types)
	bool SetStreamToOutput(std::unique_ptr<KInStream> Stream, std::size_t iContentLength);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set raw (non-json) output
	void SetRawOutput(KString sRaw)
	//-----------------------------------------------------------------------------
	{
		m_sRawOutput = std::move(sRaw);
	}

	//-----------------------------------------------------------------------------
	/// add raw (non-json) output to existing output
	void AddRawOutput(KStringView sRaw)
	//-----------------------------------------------------------------------------
	{
		m_sRawOutput += sRaw;
	}

	//-----------------------------------------------------------------------------
	/// get raw (non-json) output as const ref
	const KString& GetRawOutput()
	//-----------------------------------------------------------------------------
	{
		return m_sRawOutput;
	}

	//-----------------------------------------------------------------------------
	/// set output json["message"] string
	void SetMessage(KStringView sMessage)
	//-----------------------------------------------------------------------------
	{
		m_sMessage = sMessage;
	}

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct json_t
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KJSON rx;
		KJSON tx;

		//-----------------------------------------------------------------------------
		/// reset rx and tx JSON
		void clear();
		//-----------------------------------------------------------------------------
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct xml_t
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KXML rx;
		KXML tx;

		//-----------------------------------------------------------------------------
		/// reset rx and tx XML
		void clear();
		//-----------------------------------------------------------------------------
	};

	json_t json;
	xml_t  xml;
	const KRESTRoute*  Route       { &s_EmptyRoute        };
	const KRESTRoutes* Routes      { nullptr              };
 	KRESTPath          RequestPath { KHTTPMethod::GET, "" };

	//-----------------------------------------------------------------------------
	/// get the JSON payload struct of the JWT auth token
	const KJSON& GetAuthToken() const
	//-----------------------------------------------------------------------------
	{
		return m_AuthToken.Payload;
	}

	//-----------------------------------------------------------------------------
	/// check user's identity and access - throws if not permitted
	/// (normally called automatically for routes flagged with SSO)
	void VerifyAuthentication(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the referer url from the header
	const KString& GetReferer() const;
	///----------------------------------------------------------------------------

//------
protected:
//------

	//-----------------------------------------------------------------------------
	/// generate success output
	void Output(const Options& Options, bool bKeepAlive);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// generate error output
	void ErrorHandler(const std::exception& ex, const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write the access log
	void WriteJSONAccessLog(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// check if request shall be recorded, and doing it
	void RecordRequestForReplay(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// check if we shall log this thread's logging output into the response headers
	/// @return the per-thread logging level from 0 (off) to 3
	int VerifyPerThreadKLogToHeader(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// prepare for another round in keep-alive: clear previous request data
	void clear();
	//-----------------------------------------------------------------------------

//------
private:
//------

	static const KRESTRoute s_EmptyRoute;

	KString m_sRequestBody;
	KString m_sMessage;
	KString m_sRawOutput;
	std::unique_ptr<KInStream> m_Stream; // stream that shall be sent
	std::size_t m_iContentLength;        // content length for stream output
	std::size_t m_iRequestBodyLength;    // size of received request body
	KJWT m_AuthToken;
	std::unique_ptr<KJSON> m_JsonLogger;
	std::unique_ptr<KStopDurations> m_Timers;


}; // KRESTServer

} // end of namespace dekaf2
