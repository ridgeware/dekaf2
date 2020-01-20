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
		void AddHeader(KStringView sHeader, KStringView sValue);

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
		/// If non-empty, check that SSO token authorizes given scope
		KStringView sAuthScope;
		/// Allow KLog profiling triggered by a KLOG header?
		KStringView sKLogHeader;
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

	}; // Options

	//-----------------------------------------------------------------------------
	/// handler for one request
	bool Execute(const Options& Options, const KRESTRoutes& Routes);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// get request method as a string
	const KString& GetRequestMethod() const
	//-----------------------------------------------------------------------------
	{
		return Request.Method;
	}

	//-----------------------------------------------------------------------------
	/// get request path as a string
	const KString& GetRequestPath() const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Path.get();
	}

	//-----------------------------------------------------------------------------
	/// get one query parm value as a const string ref
	const KString& GetQueryParm(KStringView sKey) const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Query.get().Get(sKey);
	}

	//-----------------------------------------------------------------------------
	/// get the content body of a POST or PUT request
	const KString& GetRequestBody() const
	//-----------------------------------------------------------------------------
	{
		return m_sRequestBody;
	}

	//-----------------------------------------------------------------------------
	/// get query parms as a map
	const url::KQueryParms& GetQueryParms() const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Query.get();
	}

	//-----------------------------------------------------------------------------
	/// set status code and corresponding default status string
	void SetStatus(int iCode);
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
	const KRESTRoute* route { &s_EmptyRoute };

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
	/// check if request shall be recorded, and doing it
	void RecordRequestForReplay(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// check if we shall log this thread's logging output into the response headers
	void VerifyPerThreadKLogToHeader(const Options& Options);
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
	KStopTime m_timer;
	KJWT m_AuthToken;
	std::unique_ptr<KJSON> m_JsonLogger;

}; // KRESTServer

} // end of namespace dekaf2
