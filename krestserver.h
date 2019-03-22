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
#include "khttppath.h"
#include "kstring.h"
#include "kstringview.h"
#include "kurl.h"
#include "kjson.h"
#include "kxml.h"
#include "ktimer.h"
#include "kopenid.h"
#include <vector>
#include <memory>

/// @file krestserver.h
/// HTTP REST server implementation

namespace dekaf2 {

class KRESTServer;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A route (request path) without resource handler. Typically used for the
/// request path in a REST query.
class KRESTPath : public KHTTPPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using URLParts = std::vector<KStringView>;

	//-----------------------------------------------------------------------------
	/// Construct a REST path. Typically used for the request path in a HTTP query.
	/// Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class
	KRESTPath(KHTTPMethod _Method, KStringView _sRoute);
	//-----------------------------------------------------------------------------

	KHTTPMethod Method;  	// e.g. GET, or empty for all
//	KStringView sRoute;     // e.g. "/employee/:id/address" or "/documents/*" or "/help"

}; // KRESTPath

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KRESTAnalyzedPath : public KHTTPAnalyzedPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct a HTTP path. Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class
	KRESTAnalyzedPath(KHTTPMethod _Method, KStringView _sRoute);
	//-----------------------------------------------------------------------------

	KHTTPMethod Method;  	// e.g. GET, or empty for all

	bool bHasParameters { false };

}; // KRESTAnalyzedPath

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A route (request path) to a resource handler
class KRESTRoute : public detail::KRESTAnalyzedPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum ParserType { PLAIN, JSON, XML, NOREAD };

	using Function = void(*)(KRESTServer& REST);

	template<class Object>
	using MemberFunction = void(Object::*)(KRESTServer&);

	using RESTCallback = std::function<void(KRESTServer&)>;

	using Parameters = std::vector<std::pair<KStringView, KStringView>>;

	//-----------------------------------------------------------------------------
	/// Construct a REST route on a function. Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class
	KRESTRoute(KHTTPMethod _Method, KStringView _sRoute, RESTCallback _Callback, ParserType _Parser = JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a REST route on an object member function. Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class. Also, the object reference
	/// must stay valid throughout the lifetime of this class (it is a reference on a constructed
	/// object which method will be called)
	template<class Object>
	KRESTRoute(KHTTPMethod _Method, KStringView _sRoute, Object& object, MemberFunction<Object> _Callback, ParserType _Parser = JSON)
	//-----------------------------------------------------------------------------
	: KRESTRoute(_Method, _sRoute, std::bind(_Callback, &object, std::placeholders::_1), _Parser)
	{
	}

	//-----------------------------------------------------------------------------
	/// Compare this route part by part with a given path, and return true if matching.
	/// Params returns the variables in the path.
	bool Matches(const KRESTPath& Path, Parameters& Params, bool bCompareMethods = true) const;
	//-----------------------------------------------------------------------------

	RESTCallback Callback;
	ParserType Parser;

}; // KRESTRoute

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Storage object for all routes of a REST server
class KRESTRoutes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// prototype for a handler function table
	struct FunctionTable
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KStringView sMethod;
		KStringView sRoute;
		KRESTRoute::Function Handler;
		KRESTRoute::ParserType Parser = KRESTRoute::JSON;
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// prototype for a handler object member function table
	template<class Object>
	struct MemberFunctionTable
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		KStringView sMethod;
		KStringView sRoute;
		KRESTRoute::MemberFunction<Object> Handler;
		KRESTRoute::ParserType Parser = KRESTRoute::JSON;
	};

	using Parameters = KRESTRoute::Parameters;

	//-----------------------------------------------------------------------------
	/// ctor
	KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute = nullptr);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a REST route. Notice that _Route contains a KStringView, of which the pointed-to
	/// string must stay visible during the lifetime of this class
	void AddRoute(const KRESTRoute& _Route);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a REST route. Notice that _Route contains a KStringView, of which the pointed-to
	/// string must stay visible during the lifetime of this class
	void AddRoute(KRESTRoute&& _Route);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add routes from a table of route and handler function definitions
	template<std::size_t COUNT>
	void AddFunctionTable(const FunctionTable (&Routes)[COUNT])
	//-----------------------------------------------------------------------------
	{
		m_Routes.reserve(m_Routes.size() + COUNT);
		for (size_t i = 0; i < COUNT; ++i)
		{
			AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].sRoute, Routes[i].Handler, Routes[i].Parser));
		}
	}

	//-----------------------------------------------------------------------------
	/// Add routes from a table of route and handler object member function definitions
	template<class Object, std::size_t COUNT>
	void AddMemberFunctionTable(Object& object, const MemberFunctionTable<Object> (&Routes)[COUNT])
	//-----------------------------------------------------------------------------
	{
		m_Routes.reserve(m_Routes.size() + COUNT);
		for (size_t i = 0; i < COUNT; ++i)
		{
			AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].sRoute, object, Routes[i].Handler, Routes[i].Parser));
		}
	}

	//-----------------------------------------------------------------------------
	/// Set default route (matching all path requests not satisfied by other routes)
	void SetDefaultRoute(KRESTRoute::RESTCallback Callback, KRESTRoute::ParserType Parser = KRESTRoute::JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Clear all routes
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Throws KHTTPError if no matching route found - fills additonal params in Path into Params
	const KRESTRoute& FindRoute(const KRESTPath& Path, Parameters& Params) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Throws KHTTPError if no matching route found - fills additional params in Path into Params
	const KRESTRoute& FindRoute(const KRESTPath& Path, url::KQuery& Params) const;
	//-----------------------------------------------------------------------------

//------
private:
//------

	using Routes = std::vector<KRESTRoute>;

	Routes m_Routes;
	KRESTRoute m_DefaultRoute;

}; // KRESTRoutes


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

		KStringView sBaseRoute;                    // Fixed route prefix
		KStringView sTimerHeader;                  // If non-empty creates a header with execution time
		KStringViewZ sRecordFile;                  // File to record request into - filename may not change during execution
		KHTTPHeaders::KHeaderMap ResponseHeaders;  // Fixed additional headers
		KOpenIDProviderList Authenticators;        // Valid authentication instances for user verification
		KStringView sAuthScope;                    // If non-empty, check that SSO token authorizes given scope
		uint16_t iMaxKeepaliveRounds { 10 };       // DoS prevention - max rounds in keep-alive
		mutable OutputType Out { HTTP };           // Which of the three output formats?
		AUTH_LEVEL AuthLevel { ALLOW_ALL };        // Which authentication level?
		bool bRecordRequest { false };             // Shall we record into the sRecordFile? Value is expected to change during execution (could be made an atomic, but we don't care for a few missing records)
		bool bThrowIfInvalidJson {false };         // Shall we throw if the request body contains invalid JSON

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
	std::unique_ptr<xml_t> xml;

	//-----------------------------------------------------------------------------
	/// get the JSON payload struct of the JWT auth token
	const KJSON& GetAuthToken() const
	//-----------------------------------------------------------------------------
	{
		return m_AuthToken.Payload;
	}

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
	/// check user's identity and access
	void VerifyAuthentication(const Options& Options);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// prepare for another round in keep-alive: clear previous request data
	void clear();
	//-----------------------------------------------------------------------------

//------
private:
//------

	KString m_sRequestBody;
	KString m_sMessage;
	KString m_sRawOutput;
	KStopTime m_timer;
	KJWT m_AuthToken;


}; // KRESTServer

} // end of namespace dekaf2
