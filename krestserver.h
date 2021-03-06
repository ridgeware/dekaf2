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
#include "kfilesystem.h"
#include <vector>
#include <memory>

/// @file krestserver.h
/// HTTP REST server implementation

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// HTTP REST server with JSON, XML, or plain input / output
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
	/// Options for the KRESTServer class
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
		/// @param Header the KHTTPHeader to add
		/// @param sValue the value for the header
		void AddHeader(KHTTPHeader Header, KStringView sValue);

		/// Set the file name for the json access log
		/// @param sJSONAccessLogFile the filename for the access log, or "stdout" / "stderr" for console output
		/// @return true if file can be opened, false otherwise
		bool SetJSONAccessLog(KStringViewZ sJSONAccessLogFile);

		/// Fixed route prefix
		KString sBaseRoute;
		/// If non-empty creates a header with the given name and the milliseconds needed for execution
		KString sTimerHeader;
		/// File to record requests into - filename may not change during execution
		KString sRecordFile;
		/// Fixed additional headers
		KHTTPHeaders::KHeaderMap ResponseHeaders;
		/// Valid authentication instances for user verification
		KOpenIDProviderList Authenticators;
		/// If non-empty, check that SSO token authorizes one of thse given scopes (comma separated list)
		KString sAuthScope;
		/// Allow KLog profiling triggered by a KLOG header?
		KString sKLogHeader;
		/// Server name for this instance, will be used in diagnostic output
		KString sServername;
		/// DoS prevention - max rounds in keep-alive (default 10)
		mutable uint16_t iMaxKeepaliveRounds { 10 };
		/// Which of the three output formats HTTP, LAMBDA, CLI (default HTTP) ?
		mutable OutputType Out { HTTP };
		/// Which authentication level: ALLOW_ALL, ALLOW_ALL_WITH_AUTH_HEADER, VERIFY_AUTH_HEADER ?
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
		/// Set a general purpose callback function that will be called after route matching, and before route callbacks.
		/// Could be used e.g. for additional authentication, like basic. May throw to abort calling the route's callback.
		std::function<void(KRESTServer&)> PostRouteCallback;

	}; // Options

	//-----------------------------------------------------------------------------
	/// handler for one request
	/// @param Options the options for the KRESTServer
	/// @param Routes the KRESTRoutes object with all routing callbacks
	/// @return true on success, false in case of error
	bool Execute(const Options& Options, const KRESTRoutes& Routes);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the content body of a POST or PUT request, only guaranteed to be successful for routes of PLAIN type
	const KString& GetRequestBody() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set success status code and corresponding default status string - for errors throw a KHTTPError..
	/// @param iCode a HTTP success status code
	void SetStatus(int iCode);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set file to output (mutually exclusive to other output types)
	/// @param sFile the filename of the file to output
	/// @return true if file exists and can be opened, false otherwise
	bool SetFileToOutput(KStringViewZ sFile);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set stream to output (mutually exclusive to other output types)
	/// @param Stream an open stream to read from
	/// @param iContentLength the count of bytes to read, or npos for read until EOF
	/// @return true if stream is good for reading, false otherwise
	bool SetStreamToOutput(std::unique_ptr<KInStream> Stream, std::size_t iContentLength = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set raw (non-json/non-xml) output
	/// @param sRaw the string to output
	void SetRawOutput(KString sRaw)
	//-----------------------------------------------------------------------------
	{
		m_sRawOutput = std::move(sRaw);
	}

	//-----------------------------------------------------------------------------
	/// add raw (non-json/non-xml) output to existing output
	/// @param sRaw the string to append to the existing raw output
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
	/// @param sMessage the message string
	void SetMessage(KString sMessage)
	//-----------------------------------------------------------------------------
	{
		m_sMessage = std::move(sMessage);
	}

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct json_t
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the json input object
		KJSON rx;
		/// the json output obect
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
		/// the xml input object
		KXML rx;
		/// the xml output obect
		KXML tx;

		//-----------------------------------------------------------------------------
		/// reset rx and tx XML
		void clear();
		//-----------------------------------------------------------------------------
	};

	/// the json input/output object
	json_t json;
	/// the xml input/output object
	xml_t  xml;
	/// the selected KRESTRoute
	const KRESTRoute*  Route       { &s_EmptyRoute        };
	/// the KRESTRoutes object used for routing
	const KRESTRoutes* Routes      { nullptr              };
	/// the incoming request with method and path
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
	/// @param Options the options for the KRESTServer
	void VerifyAuthentication(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a temporary directory that is guaranteed to exist until this REST request is answered.
	/// All content and the directory will be removed after the REST connection got closed.
	const KString& GetTempDir()
	//-----------------------------------------------------------------------------
	{
		return m_TempDir.Name();
	}

//------
protected:
//------

	//-----------------------------------------------------------------------------
	/// parse input (if requested by method and route)
	/// @param Options the options for the KRESTServer
	void Parse(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// generate success output
	/// @param Options the options for the KRESTServer
	/// @param bKeepAlive if false the Connection header will be set to close
	void Output(const Options& Options, bool bKeepAlive);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// generate error output
	/// @param ex the exception with the error status
	/// @param Options the options for the KRESTServer
	void ErrorHandler(const std::exception& ex, const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write the access log
	/// @param Options the options for the KRESTServer
	void WriteJSONAccessLog(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// check if request shall be recorded, and doing it
	/// @param Options the options for the KRESTServer
	void RecordRequestForReplay(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// check if we shall log this thread's logging output into the response headers
	/// @param Options the options for the KRESTServer
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
	KTempDir m_TempDir;                  // create a KTempDir object
	std::unique_ptr<KJSON> m_JsonLogger;
	std::unique_ptr<KStopDurations> m_Timers;


}; // KRESTServer

} // end of namespace dekaf2
