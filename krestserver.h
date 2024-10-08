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
#include "kduration.h"
#include "kopenid.h"
#include "kfilesystem.h"
#include "kpoll.h"
#include "khttplog.h"
#include "kwebsocket.h"
#include <utility>
#include <vector>
#include <memory>
#include <limits>

/// @file krestserver.h
/// HTTP REST server implementation

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// HTTP REST server with JSON, XML, or plain input / output
class DEKAF2_PUBLIC KRESTServer : public KHTTPServer
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

	enum OutputType
	{
		HTTP,     ///< speak HTTP
		LAMBDA,   ///< AWS specific
		CLI       ///< console type output for testing
	};

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
		void AddHeader(KHTTPHeader Header, KString sValue);
		/// Configure the KHTTPLog object with its Open() method
		KHTTPLog Logger;
		/// Fixed route prefix
		KString sBaseRoute;
		/// If non-empty creates a header with the given name and the milliseconds needed for execution
		KHTTPHeader TimerHeader;
		/// File to record requests into - filename may not change during execution
		KString sRecordFile;
		/// Fixed additional headers
		KHTTPHeaders::KHeaderMap ResponseHeaders;
		/// Valid authentication instances for user verification
		KOpenIDProviderList Authenticators;
		/// If non-empty, check that SSO token authorizes one of the given scopes (comma separated list)
		KString sAuthScope;
		/// Allow KLog profiling triggered by a KLOG header?
		KHTTPHeader KLogHeader;
		/// Server name for this instance, will be used in diagnostic output
		KString sServername;
		/// Set a callback function that will receive references to this instance and the TimeKeepers
		/// after termination of one request
		std::function<void(const KRESTServer&, const KDurations&)> TimingCallback;
		/// Set an authentication callback function that will be called after route matching, but before SSO and before route callbacks.
		/// The flag KRESTRoute::Options::GENERIC_AUTH must be set for the route for this callback being called.
		/// Could be used e.g. for additional authentication, like basic. May throw to abort processing.
		/// Returned string will be stored as the authenticated user's name.
		std::function<KString(KRESTServer&)> AuthCallback;
		/// Set a function that will be called in case of a failed authentication attempt, e.g. to force a redirect to a login page
		std::function<void(KRESTServer&)> FailedAuthCallback;
		/// Set a general purpose callback function that will be called before route matching, and before route callbacks.
		/// Could be used e.g. for additional authentication, like basic, or special routing needs. May throw to abort processing.
		std::function<void(KRESTServer&)> PreRouteCallback;
		/// Set a general purpose callback function that will be called after route matching, and before route callbacks.
		/// Could be used e.g. for additional authentication, like basic. May throw to abort calling the route's callback.
		std::function<void(KRESTServer&)> PostRouteCallback;
		/// DoS prevention - max rounds in keep-alive (default 10)
		mutable uint16_t iMaxKeepaliveRounds { 10 };
		/// Which of the three output formats HTTP, LAMBDA, CLI (default HTTP) ?
		mutable OutputType Out { HTTP };
		/// Which authentication level: ALLOW_ALL, ALLOW_ALL_WITH_AUTH_HEADER, VERIFY_AUTH_HEADER ? (default = ALLOW_ALL)
		AUTH_LEVEL AuthLevel { ALLOW_ALL };
		/// Shall we record into the sRecordFile? Value is expected to change during execution (could be made an atomic, but we don't care for a few missing records)
		bool bRecordRequest { false };
		/// Shall we throw if the request body contains invalid JSON? (default = false)
		bool bThrowIfInvalidJson { false };
		/// If no route found, shall we check if that happened because of a wrong request method? (This requires a second, more costly
		/// scan of the route table just to change the error message slightly) (default = true)
		bool bCheckForWrongMethod { true };
		/// Whenever this value is set to false, the REST server will respond all requests with a HTTP 503 SERVICE UNAVAILABLE
		/// (default = true)
		bool bServiceIsReady { true };
		/// Allow output compression if the MIME type is compressible (default = true)
		bool bAllowCompression { true };
		/// Show timer header in microseconds (default false = milliseconds)
		bool bMicrosecondTimerHeader { false };
		/// Force pretty printing in release builds, too?
		bool bPrettyPrint { false };

	}; // Options

	/// the KRESTRoutes object used for routing
	const KRESTRoutes& m_Routes;
	/// the current set of Options
	const Options&     m_Options;

	//-----------------------------------------------------------------------------
	/// @param Options the options for the KRESTServer
	/// @param Routes the KRESTRoutes object with all routing callbacks
	KRESTServer(const KRESTRoutes& Routes,
				const Options&     Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @param Options the options for the KRESTServer
	/// @param Routes the KRESTRoutes object with all routing callbacks
	KRESTServer(KStream&           Stream,
				KStringView        sRemoteEndpoint,
				url::KProtocol     Proto,
				uint16_t           iPort,
				const KRESTRoutes& Routes,
				const Options&     Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// handler for one request
	/// @return true on success, false in case of error
	bool Execute();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get one query parm value and throw when the value contains possible injection attempts: single, double, backtick and backslash
	/// @param sKey the name of the requested query parm
	/// @return the value for the requested query parm
	DEKAF2_NODISCARD
	const KString& GetQueryParmSafe (KStringView sKey) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get one query parm value and throw when the value contains possible injection attempts: single, double, backtick and backslash
	/// @param sKey the name of the requested query parm
	/// @param sDefault the default return value if there is no value for the key
	/// @return the value for the requested query parm
	DEKAF2_NODISCARD
	KString GetQueryParmSafe (KStringView sKey, KStringView sDefault) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get the content body of a POST or PUT request, only guaranteed to be successful for routes of types
	/// NOREAD, PLAIN and WWWFORM type
	DEKAF2_NODISCARD
	const KString& GetRequestBody() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// set success status code and corresponding default status string - for errors throw a KHTTPError..
	/// @param iCode a HTTP success status code
	void SetStatus(uint16_t iCode, KStringView sOptionalStatusString = KStringView{});
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
	/// @param iContentLength the count of bytes to read, or npos for read until EOF (the default)
	/// @return true if stream is good for reading, false otherwise
	bool SetStreamToOutput(std::unique_ptr<KInStream> Stream, std::size_t iContentLength = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets the expected Content Length to output - this is typically used for a HEAD
	/// request without real response result
	void SetContentLengthToOutput(std::size_t iContentLength)
	//-----------------------------------------------------------------------------
	{
		m_iContentLength = iContentLength;
	}

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
	DEKAF2_NODISCARD
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

	//-----------------------------------------------------------------------------
	/// @return the output json["message"] string
	DEKAF2_NODISCARD
	const KString& GetMessage() const
	//-----------------------------------------------------------------------------
	{
		return m_sMessage;
	}

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct json_t
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// the json input object
		KJSON rx;
		/// the json output object
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
		/// the xml output object
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
	/// the incoming request with method and path
 	KRESTPath          RequestPath { KHTTPMethod::GET, "" };

	//-----------------------------------------------------------------------------
	/// get the JSON payload struct of the JWT auth token
	DEKAF2_NODISCARD
	const KJSON& GetAuthToken() const
	//-----------------------------------------------------------------------------
	{
		return m_AuthToken.Payload;
	}

	//-----------------------------------------------------------------------------
	/// check user's identity and access - throws if not permitted
	/// (normally called automatically for routes flagged with SSO)
	void VerifyAuthentication();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a temporary directory that is guaranteed to exist until this REST request is answered.
	/// All content and the directory will be removed after the REST connection got closed.
	/// @return a string with the path name of the temporary directory
	DEKAF2_NODISCARD
	const KString& GetTempDir()
	//-----------------------------------------------------------------------------
	{
		return m_TempDir.Name();
	}

	//-----------------------------------------------------------------------------
	/// Get a reference on a KTempDir instance that is guaranteed to exist until this REST request is answered.
	/// All content and the directory will be removed after the REST connection got closed.
	/// @return a string with the path name of the temporary directory
	DEKAF2_NODISCARD
	KTempDir& GetTempDirReference()
	//-----------------------------------------------------------------------------
	{
		return m_TempDir;
	}

	//-----------------------------------------------------------------------------
	/// Called from KPoll if connection was disconnected by the remote end
	void SetDisconnected();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return true if the connection is disconnected
	DEKAF2_NODISCARD
	bool IsDisconnected();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Throws if the connection is disconnected
	void ThrowIfDisconnected();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Serialize output with indents or condensed - to be called inside route handlers to set their output format
	/// @param bYesNo if true, output json/xml with indents, if false output condensed (default in non-debug code)
	void PrettyPrint(bool bYesNo);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// @return the length in bytes of the request body
	DEKAF2_NODISCARD
	std::size_t GetRequestBodyLength() const
	//-----------------------------------------------------------------------------
	{
		return m_iRequestBodyLength;
	}

	//-----------------------------------------------------------------------------
	/// @return the length in bytes of the request headers
	DEKAF2_NODISCARD
	std::size_t GetRequestHeaderLength() const
	//-----------------------------------------------------------------------------
	{
		return m_iRequestHeaderLength;
	}

	//-----------------------------------------------------------------------------
	/// @return the length in bytes of the response content (body)
	DEKAF2_NODISCARD
	std::size_t GetContentLength() const
	//-----------------------------------------------------------------------------
	{
		return m_iContentLength;
	}

	//-----------------------------------------------------------------------------
	/// @return the length in bytes of the request header and request body
	DEKAF2_NODISCARD
	std::size_t GetReceivedBytes() const
	//-----------------------------------------------------------------------------
	{
		return GetRequestHeaderLength() + GetRequestBodyLength();
	}

	//-----------------------------------------------------------------------------
	/// @return the length in bytes of the sent bytes
	DEKAF2_NODISCARD
	std::size_t GetSentBytes() const
	//-----------------------------------------------------------------------------
	{
		return m_iTXBytes;
	}

	//-----------------------------------------------------------------------------
	/// @return the count of previous requests on this connection
	DEKAF2_NODISCARD
	uint16_t GetKeepaliveRound() const
	//-----------------------------------------------------------------------------
	{
		return m_iRound;
	}

	//-----------------------------------------------------------------------------
	/// @return true if consecutive requests are permitted on this connection
	DEKAF2_NODISCARD
	bool GetKeepalive() const
	//-----------------------------------------------------------------------------
	{
		return m_bKeepAlive;
	}

	//-----------------------------------------------------------------------------
	/// @return the count in microsecond ticks for a request until the last byte was sent
	DEKAF2_NODISCARD
	chrono::microseconds GetTimeToLastByte() const
	//-----------------------------------------------------------------------------
	{
		return m_Timers ? m_Timers->duration().microseconds() : chrono::microseconds(0);
	}

	//-----------------------------------------------------------------------------
	/// @return true if the connection was lost
	DEKAF2_NODISCARD
	bool GetLostConnection() const
	//-----------------------------------------------------------------------------
	{
		return m_bLostConnection;
	}

	//-----------------------------------------------------------------------------
	/// switch to streaming output - this writes first the headers (if requested), and then leaves the output stream open
	/// @param bAllowCompressionIfPossible switch compression on if possible (only if bWriteHeaders is true, too)
	/// @param bWriteHeaders output headers - if false, neither headers nor end-of-header is written to the output
	void Stream(bool bAllowCompressionIfPossible, bool bWriteHeaders = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets a callback that is called (once) after generating the response for the current request, directly before the
	/// general logging.
	/// @param PostResponseCallback called after generating the response for the current request, also in error cases
	void SetPostResponseCallback(std::function<void(const KRESTServer&)> PostResponseCallback)
	//-----------------------------------------------------------------------------
	{
		m_PostResponseCallback = std::move(PostResponseCallback);
	}

	//-----------------------------------------------------------------------------
	/// @return true if this connection shall be switched to the websocket protocol
	DEKAF2_NODISCARD
	bool SwitchToWebSocket()
	//-----------------------------------------------------------------------------
	{
		return m_bSwitchToWebSocket;
	}

	//-----------------------------------------------------------------------------
	/// sets a callback that will be called every time a websocket frame is received, or the connection is lost
	void SetWebSocketHandler(std::function<void(KWebSocket&)> WebSocketHandler)
	//-----------------------------------------------------------------------------
	{
		m_WebSocketHandlerCallback = std::move(WebSocketHandler);
	}

	//-----------------------------------------------------------------------------
	/// gets the websocket callback
	DEKAF2_NODISCARD
	const std::function<void(KWebSocket&)>& GetWebSocketHandler()
	//-----------------------------------------------------------------------------
	{
		return m_WebSocketHandlerCallback;
	}

//------
protected:
//------

	//-----------------------------------------------------------------------------
	/// parse input (if requested by method and route)
	void Parse();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write headers, called by Stream() and Output()
	void WriteHeaders();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// generate success output
	void Output();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// generate error output
	/// @param ex the exception with the error status
	void ErrorHandler(const std::exception& ex);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// check if request shall be recorded, and doing it
	void RecordRequestForReplay();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// check if we shall log this thread's logging output into the response headers
	/// @return the per-thread logging level from 0 (off) to 3
	DEKAF2_NODISCARD
	int VerifyPerThreadKLogToHeader();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// prepare for another round in keep-alive: clear previous request data
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// run the post response callback if existing, and the logger and reporting facilities
	void RunPostResponse();
	//-----------------------------------------------------------------------------

//------
private:
//------

	static constexpr int iJSONTerse  { -1 };
	static constexpr int iJSONPretty {  1 };
	static constexpr int iXMLTerse   { KXML::NoIndents | KXML::NoLinefeeds };
	static constexpr int iXMLPretty  { KXML::Default };

	static const KRESTRoute s_EmptyRoute;

	KString     m_sRequestBody;
	KString     m_sMessage;
	KString     m_sRawOutput;
	std::unique_ptr<KInStream> m_Stream; // stream that shall be sent
	std::size_t m_iTXBytes;              // size of sent headers and content, after compression
	std::size_t m_iContentLength;        // content length for stream output (before compression)
	std::size_t m_iRequestHeaderLength;  // size of received query and headers
	std::size_t m_iRequestBodyLength;    // size of received request body
	KJWT        m_AuthToken;
	KTempDir    m_TempDir;               // create a KTempDir object
	std::unique_ptr<KJSON> m_JsonLogger;
	std::unique_ptr<KStopDurations> m_Timers;
	std::function<void(const KRESTServer&)> m_PostResponseCallback; // if set, gets called after response generation
	std::function<void(KWebSocket&)> m_WebSocketHandlerCallback; // filled by route handler during upgrade to websocket protocol, will be called every time a frame is received, or the connection is lost
	uint16_t    m_iRound = std::numeric_limits<uint16_t>::max(); // keepalive rounds
	bool        m_bKeepAlive;            // whether connection will be kept alive
	bool        m_bLostConnection;       // whether we lost our peer during flight
	bool        m_bIsStreaming;          // true if we switched to streaming output
	bool        m_bSwitchToWebSocket;    // true if we will switch to the websocket protocol

	int m_iJSONPrint {
#ifdef NDEBUG
		iJSONTerse
#else
		iJSONPretty
#endif
	};
	int m_iXMLPrint {
#ifdef NDEBUG
		iXMLTerse
#else
		iXMLPretty
#endif
	};
	bool m_bIsDisconnected { false };


}; // KRESTServer

DEKAF2_NAMESPACE_END

namespace DEKAF2_FORMAT_NAMESPACE
{

template <>
struct formatter<DEKAF2_PREFIX KRESTServer::Options::AUTH_LEVEL> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KRESTServer::Options::AUTH_LEVEL& AuthLevel, FormatContext& ctx) const
	{
		string_view sVerify;
		switch (AuthLevel)
		{
			case dekaf2::KRESTServer::Options::ALLOW_ALL:                  sVerify = "AllowAll";               break;
			case dekaf2::KRESTServer::Options::ALLOW_ALL_WITH_AUTH_HEADER: sVerify = "AllowAllWithAuthHeader"; break;
			case dekaf2::KRESTServer::Options::VERIFY_AUTH_HEADER:         sVerify = "VerifyAuthHeader";       break;
		}
		return formatter<string_view>::format(sVerify, ctx);
	}
};

} // end of namespace DEKAF2_FORMAT_NAMESPACE
