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

#include "khttppath.h"
#include "khttp_method.h"
#include "kstring.h"
#include "kstringview.h"
#include "kurl.h"
#include "kjson.h"
#include <vector>
#include <memory>

/// @file krestroute.h
/// Primitives for REST routing

DEKAF2_NAMESPACE_BEGIN

class KRESTServer;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A route (request path) without resource handler. Typically used for the
/// request path in a REST query.
class DEKAF2_PUBLIC KRESTPath : public KHTTPPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using URLParts = std::vector<KStringView>;

	KRESTPath(const KRESTPath&) = default;
	KRESTPath(KRESTPath&&) = default;
	KRESTPath& operator=(const KRESTPath&) = default;
	KRESTPath& operator=(KRESTPath&&) = default;


	//-----------------------------------------------------------------------------
	/// Construct a REST path. Typically used for the request path in a HTTP query.
	/// @param _Method the HTTP method
	/// @param _sRoute the HTTP path, like "/some/path/index.html" or "/documents/*" or "/help"
	KRESTPath(KHTTPMethod _Method, KString _sRoute);
	//-----------------------------------------------------------------------------

	KHTTPMethod Method;  	// e.g. GET, or empty for all
//	KStringView sRoute;     // e.g. "/employee/:id/address" or "/documents/*" or "/help"

}; // KRESTPath

namespace detail {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A REST path object that performs some analysis on the path, to accelerate routing
class DEKAF2_PUBLIC KRESTAnalyzedPath : public KHTTPAnalyzedPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	/// Construct an analyzed REST path.
	/// @param _Method the HTTP method
	/// @param _sRoute the HTTP path, like "/some/path/index.html" or "/documents/*" or "/help"
	KRESTAnalyzedPath(KHTTPMethod _Method, KString _sRoute);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// checks if this path contains a parameter of the given name (:param or =param)
	/// @param sParam the named parameter to check
	/// @return true if the named parameter exists as part of the route, false otherwise
	DEKAF2_NODISCARD
	bool HasParameter(KStringView sParam) const;
	//-----------------------------------------------------------------------------

	KHTTPMethod Method; ///< the method for this path, e.g. GET, or empty for all

	bool m_bHasParameters { false };

}; // KRESTAnalyzedPath

} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A route (request path) to a resource handler
class DEKAF2_PUBLIC KRESTRoute : public detail::KRESTAnalyzedPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	class Options
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		enum OPT
		{
			NONE         = 0,       ///< no options
			SSO_AUTH     = 1 << 0,  ///< requires SSO authentication
			GENERIC_AUTH = 1 << 1,  ///< requires generic authentication (through KRESTServer::Options::AuthCallback)
			NO_SSO_SCOPE = 1 << 2,  ///< do NOT check for SSO scope (from KRESTServer::Options::sAuthScope)
			WEBSOCKET    = 1 << 3   ///< promote into web socket, else fail
		};

		constexpr
		Options() = default;

		constexpr
		Options(bool bSSO) : m_Options(bSSO ? SSO_AUTH : NONE) {} // fallback for old bAuth parameter in route construction

		DEKAF2_CONSTEXPR_14
		Options(std::initializer_list<OPT> il) { for (auto opt : il) Set(opt); }

		DEKAF2_NODISCARD constexpr
		bool Has(OPT opt) const        { return (m_Options & opt) != 0; }

		constexpr
		void Set(OPT opt)              { m_Options |= opt;              }

		constexpr
		bool operator()(OPT opt) const { return Has(opt);               }

	//------
	private:
	//------

		uint16_t m_Options {};

	}; // Options

	enum ParserType { PLAIN, JSON, XML, WWWFORM, NOREAD };

	using Function = void(*)(KRESTServer& REST);

	template<class Object>
	using MemberFunction = void(Object::*)(KRESTServer&);

	using RESTCallback = std::function<void(KRESTServer&)>;

	using Parameters = std::vector<std::pair<KStringView, KStringView>>;

	//-----------------------------------------------------------------------------
	/// Construct a REST route for a free function
	/// @param _Method the HTTP method to match with (or empty method for any method)
	/// @param _Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	/// @param _sRoute a REST route, wildcards allowed: /my/path/*/:user/name
	/// @param _sDocumentRoot the file system path to be used for serving GET requests, or empty
	/// @param _Callback a method that will be called when the route matches the request, may not be empty
	/// @param _Config json config object that will be passed to the callback function, should include a 'parser' property with value PLAIN, JSON, XML, WWWFORM or NOREAD, defaults to JSON
	KRESTRoute(KHTTPMethod _Method, Options _Options, KString _sRoute, KString _sDocumentRoot, RESTCallback _Callback, KJSON _Config);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a REST route for a free function
	/// @param _Method the HTTP method to match with (or empty method for any method)
	/// @param _Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	/// @param _sRoute a REST route, wildcards allowed: /my/path/*/:user/name
	/// @param _sDocumentRoot the file system path to be used for serving GET requests, or empty
	/// @param _Callback a method that will be called when the route matches the request, may not be empty
	/// @param _Parser any of the parser types for input parsing (PLAIN, JSON, XML, WWWFORM) or NOREAD for no parsing, defaults to JSON
	KRESTRoute(KHTTPMethod _Method, Options _Options, KString _sRoute, KString _sDocumentRoot, RESTCallback _Callback, ParserType _Parser = JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a REST route for a free function
	/// @param _Method the HTTP method to match with (or empty method for any method)
	/// @param _Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	/// @param _sRoute a REST route, wildcards allowed: /my/path/*/:user/name
	/// @param _Callback a method that will be called when the route matches the request, may not be empty
	/// @param _Parser any of the parser types for input parsing (PLAIN, JSON, XML, WWWFORM) or NOREAD for no parsing, defaults to JSON
	KRESTRoute(KHTTPMethod _Method, Options _Options, KString _sRoute, RESTCallback _Callback, ParserType _Parser = JSON)
	//-----------------------------------------------------------------------------
	: KRESTRoute(_Method, _Options, std::move(_sRoute), KString{}, _Callback, _Parser)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a REST route for a free function
	/// @param _Method the HTTP method to match with (or empty method for any method)
	/// @param _sRoute a REST route, wildcards allowed: /my/path/*/:user/name
	/// @param _Callback a method that will be called when the route matches the request, may not be empty
	/// @param _Parser any of the parser types for input parsing (PLAIN, JSON, XML, WWWFORM) or NOREAD for no parsing
	KRESTRoute(KHTTPMethod _Method, KString _sRoute, RESTCallback _Callback, ParserType _Parser = JSON)
	//-----------------------------------------------------------------------------
	: KRESTRoute(_Method, Options{}, std::move(_sRoute), KString{}, _Callback, _Parser)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a REST route for an object method. The object reference
	/// must stay valid throughout the lifetime of this class (it is a reference on a constructed
	/// object which method will be called)
	/// @param _Method the HTTP method to match with (or empty method for any method)
	/// @param _Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	/// @param _sRoute a REST route, wildcards allowed: /my/path/*/:user/name
	/// @param _sDocumentRoot the file system path to be used for serving GET requests, or empty
	/// @param _Object the object for the method to be called, may not be empty
	/// @param _Callback the object method that will be called when the route matches the request, may not be empty
	/// @param _Parser any of the parser types for input parsing (PLAIN, JSON, XML, WWWFORM) or NOREAD for no parsing, defaults to JSON
	template<class Object>
	KRESTRoute(KHTTPMethod _Method, Options _Options, KString _sRoute, KString _sDocumentRoot, Object& _Object, MemberFunction<Object> _Callback, ParserType _Parser = JSON)
	//-----------------------------------------------------------------------------
	: KRESTRoute(std::move(_Method), _Options, std::move(_sRoute), std::move(_sDocumentRoot), std::bind(_Callback, &_Object, std::placeholders::_1), _Parser)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a REST route for an object method. The object reference
	/// must stay valid throughout the lifetime of this class (it is a reference on a constructed
	/// object which method will be called)
	/// @param _Method the HTTP method to match with (or empty method for any method)
	/// @param _Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	/// @param _sRoute a REST route, wildcards allowed: /my/path/*/:user/name
	/// @param _sDocumentRoot the file system path to be used for serving GET requests, or empty
	/// @param _Object the object for the method to be called, may not be empty
	/// @param _Callback the object method that will be called when the route matches the request, may not be empty
	/// @param _Config json config object that will be passed to the callback function, should include a 'parser' property with value PLAIN, JSON, XML, WWWFORM or NOREAD, defaults to JSON
	template<class Object>
	KRESTRoute(KHTTPMethod _Method, Options _Options, KString _sRoute, KString _sDocumentRoot, Object& _Object, MemberFunction<Object> _Callback, KJSON _Config)
	//-----------------------------------------------------------------------------
	: KRESTRoute(std::move(_Method), _Options, std::move(_sRoute), std::move(_sDocumentRoot), std::bind(_Callback, &_Object, std::placeholders::_1), _Config)
	{
	}

	//-----------------------------------------------------------------------------
	/// Construct a REST route for an object method. The object reference
	/// must stay valid throughout the lifetime of this class (it is a reference on a constructed
	/// object which method will be called)
	/// @param _Method the HTTP method to match with (or empty method for any method)
	/// @param _Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	/// @param _sRoute a REST route, wildcards allowed: /my/path/*/:user/name
	/// @param _Object the object for the method to be called, may not be empty
	/// @param _Callback the object method that will be called when the route matches the request, may not be empty
	/// @param _Parser any of the parser types for input parsing (PLAIN, JSON, XML, WWWFORM) or NOREAD for no parsing, defaults to JSON
	template<class Object>
	KRESTRoute(KHTTPMethod _Method, Options _Options, KString _sRoute, Object& _Object, MemberFunction<Object> _Callback, ParserType _Parser = JSON)
	//-----------------------------------------------------------------------------
	: KRESTRoute(std::move(_Method), _Options, std::move(_sRoute), KString{}, std::bind(_Callback, &_Object, std::placeholders::_1), _Parser)
	{
	}

	//-----------------------------------------------------------------------------
	/// Compare this route part by part with a given path, and return true if matching.
	/// Params returns the variables in the path.
	/// @param Path the REST path from a request to match with this route
	/// @param Params pointer on a vector of parameters object, if not null will be filled with the found rest path parameters (components starting with : or = )
	/// @return true if the Path matches this route, false otherwise
	DEKAF2_NODISCARD
	bool Matches(const KRESTPath& Path, Parameters* Params = nullptr, bool bCompareMethods = true, bool bCheckWebservers = true, bool bIsWebSocket = false) const;
	//-----------------------------------------------------------------------------

	RESTCallback Callback;        ///< the callback for this route
	KString      sDocumentRoot;   ///< the document root for this route, for web server mode
	ParserType   Parser { JSON }; ///< the requested input parser, defaults to JSON
	Options      Option;          ///< options used before calling the callback
	KJSON        Config;          ///< any configuration to pass on to the callback

}; // KRESTRoute

DEKAF2_ENUM_IS_FLAG(KRESTRoute::Options::OPT)

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Storage object for all routes of a REST server
class DEKAF2_PUBLIC KRESTRoutes
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
		FunctionTable() = default;

		// C++11 needs a constructor for initializer list initialization
		constexpr
		FunctionTable(KStringView _sMethod, KRESTRoute::Options _Options, KStringView _sRoute, KRESTRoute::Function _Handler, KRESTRoute::ParserType _Parser = KRESTRoute::JSON)
		: sMethod(_sMethod)
		, Options(_Options)
		, sRoute(_sRoute)
		, sDocumentRoot(KStringView{})
		, Handler(_Handler)
		, Parser(_Parser)
		{}

		constexpr // create a WebServer item in the function table
		FunctionTable(KStringView _sMethod, KRESTRoute::Options _Options, KStringView _sRoute, KStringView _sDocumentRoot, KRESTRoute::ParserType _Parser = KRESTRoute::JSON)
		: sMethod(_sMethod)
		, Options(_Options)
		, sRoute(_sRoute)
		, sDocumentRoot(_sDocumentRoot)
		, Handler(nullptr)
		, Parser(_Parser)
		{}

		KStringView sMethod;
		KRESTRoute::Options Options;
		KStringView sRoute;
		KStringView sDocumentRoot;
		KRESTRoute::Function Handler;
		KRESTRoute::ParserType Parser = KRESTRoute::JSON;
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// prototype for a handler object member function table
	template<class Object>
	struct MemberFunctionTable
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		MemberFunctionTable() = default;

		// C++11 needs a constructor for initializer list initialization
		constexpr
		MemberFunctionTable(KStringView _sMethod, KRESTRoute::Options _Options, KStringView _sRoute, KRESTRoute::MemberFunction<Object> _Handler, KRESTRoute::ParserType _Parser = KRESTRoute::JSON)
		: sMethod(_sMethod)
		, Options(_Options)
		, sRoute(_sRoute)
		, sDocumentRoot(KStringView{})
		, Handler(_Handler)
		, Parser(_Parser)
		{}

		constexpr // create a WebServer item in the function table
		MemberFunctionTable(KStringView _sMethod, KRESTRoute::Options _Options, KStringView _sRoute, KStringView _sDocumentRoot, KRESTRoute::ParserType _Parser = KRESTRoute::JSON)
		: sMethod(_sMethod)
		, Options(_Options)
		, sRoute(_sRoute)
		, sDocumentRoot(_sDocumentRoot)
		, Handler(nullptr)
		, Parser(_Parser)
		{}

		KStringView sMethod;
		KRESTRoute::Options Options;
		KStringView sRoute;
		KStringView sDocumentRoot;
		KRESTRoute::MemberFunction<Object> Handler;
		KRESTRoute::ParserType Parser = KRESTRoute::JSON;
	};

	using Parameters = KRESTRoute::Parameters;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Helper class to build a route by calling its methods, adding the route at destruction. Normally invoked
	/// through KRESTRoutes.AddRoute("")
	class RouteBuilder
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		using self = RouteBuilder;

		RouteBuilder(KRESTRoutes& Routes, KString sRoute);
		~RouteBuilder();

		/// add a callback for all request methods
		self& Any   (KRESTRoute::RESTCallback Callback)   { return SetCallback(KHTTPMethod{},       std::move(Callback)); }
		/// add a callback for a GET request
		self& Get   (KRESTRoute::RESTCallback Callback)   { return SetCallback(KHTTPMethod::GET,    std::move(Callback)); }
		/// add a callback for a POST request
		self& Post  (KRESTRoute::RESTCallback Callback)   { return SetCallback(KHTTPMethod::POST,   std::move(Callback)); }
		/// add a callback for a PUT request
		self& Put   (KRESTRoute::RESTCallback Callback)   { return SetCallback(KHTTPMethod::PUT,    std::move(Callback)); }
		/// add a callback for a PATCH request
		self& Patch (KRESTRoute::RESTCallback Callback)   { return SetCallback(KHTTPMethod::PATCH,  std::move(Callback)); }
		/// add a callback for a DELETE request
		self& Delete(KRESTRoute::RESTCallback Callback)   { return SetCallback(KHTTPMethod::DELETE, std::move(Callback)); }
		/// add a callback for an OPTIONS request
		self& Options(KRESTRoute::RESTCallback Callback)  { return SetCallback(KHTTPMethod::OPTIONS,std::move(Callback)); }
		/// set options for this route
		self& Options(KRESTRoute::Options Options)        { m_Options = std::move(Options);   return *this; }
		/// set parser type for this route, default is JSON
		self& Parse  (KRESTRoute::ParserType Parser)      { m_Parser  = Parser;               return *this; }

	//------
	private:
	//------

		self& SetCallback(KHTTPMethod Method, KRESTRoute::RESTCallback Callback);
		void AddRoute(bool bKeepSettings = false);

		KRESTRoutes&             m_Routes;
		KHTTPMethod              m_Verb;
		KRESTRoute::RESTCallback m_Callback;
		KRESTRoute::Options      m_Options;
		KString                  m_sRoute;
		KRESTRoute::ParserType   m_Parser                 { KRESTRoute::ParserType::JSON };

	}; // RouteBuilder

	//-----------------------------------------------------------------------------
	/// Construct KRESTRoutes object
	/// @param DefaultRoute default callback if none of the routes matches, defaults to nullptr
	/// @param sDocumentRoot string associated to the default route, defaults to empty string
	/// @param Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute = nullptr, KString sDocumentRoot = KString{}, KRESTRoute::Options Options = KRESTRoute::Options{} );
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a REST route
	/// @param _Route the REST route to add
	void AddRoute(KRESTRoute _Route);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a composed REST route
	/// @param sRoute a REST route, wildcards allowed: /my/path/*/img/*
	/// @returns a compositon helper class, RouteBuilder
	RouteBuilder AddRoute(KString sRoute);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a basic web server for static files
	/// @param sWWWDir the base directory of the web server
	/// @param sRoute a REST route, wildcards allowed: /my/path/*/img/* , minimum ("/*") - no default
	/// @param jConfig json configuration, should include properties parser (ParserType), autoindex (bool), upload (bool), styles (string)
	void AddWebServer(KString sWWWDir, KString sRoute, KJSON jConfig);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a basic web server for static files
	/// @param sWWWDir the base directory of the web server
	/// @param sRoute a REST route, wildcards allowed: /my/path/*/img/* , defaults to all ("/*")
	/// @param bWithAdHocIndex if true the web server generates index.html files automatically - defaults to false
	/// @param bAllowUpload if true, uploads and deletes are permitted - defaults to false
	/// @param sStyles style sheet to use instead of the default style sheet. Either a URL/path (ending with .css)
	/// @param sIndexfile filename to search for as the index file for a repository - default empty, which searches index.html
	/// or plain CSS definitions. Defaults to the empty string, forcing the default style sheet.
	void AddWebServer(KString sWWWDir, KString sRoute = "/*", bool bWithAdHocIndex = false, bool bAllowUpload = false, KStringView sStyles = "", KStringView sIndexfile = "");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a request route rewrite rule
	/// @param _Rewrite the rewrite rule to add
	void AddRewrite(KHTTPRewrite _Rewrite);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add a request route redirect rule
	/// @param _Redirect the redirect rule to add
	void AddRedirect(KHTTPRewrite _Redirect);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Add routes from an array of route and handler function definitions
	template<std::size_t COUNT>
	void AddFunctionTable(const FunctionTable (&Routes)[COUNT])
	//-----------------------------------------------------------------------------
	{
		m_Routes.reserve(m_Routes.size() + COUNT);

		for (size_t i = 0; i < COUNT; ++i)
		{
			if (Routes[i].Handler != nullptr)
			{
				if (Routes[i].sMethod == "WEBSOCKET")
				{
					KRESTRoute::Options Options = Routes[i].Options;
					Options.Set(KRESTRoute::Options::WEBSOCKET);
					AddRoute(KRESTRoute("GET", Options, Routes[i].sRoute, Routes[i].sDocumentRoot, Routes[i].Handler, Routes[i].Parser));
				}
				else
				{
					AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].Options, Routes[i].sRoute, Routes[i].sDocumentRoot, Routes[i].Handler, Routes[i].Parser));
				}
			}
			else
			{
				if (Routes[i].sMethod == "REWRITE")
				{
					AddRewrite(KHTTPRewrite(Routes[i].sRoute, Routes[i].sDocumentRoot));
				}
				else if (Routes[i].sMethod == "REDIRECT")
				{
					AddRedirect(KHTTPRewrite(Routes[i].sRoute, Routes[i].sDocumentRoot));
				}
				else
				{
					AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].Options, Routes[i].sRoute, Routes[i].sDocumentRoot, *this, &KRESTRoutes::WebServer, Routes[i].Parser));
				}
			}
		}
	}

	//-----------------------------------------------------------------------------
	/// Add routes from an array of route and handler object member function definitions
	template<class Object, std::size_t COUNT>
	void AddMemberFunctionTable(Object& object, const MemberFunctionTable<Object> (&Routes)[COUNT])
	//-----------------------------------------------------------------------------
	{
		m_Routes.reserve(m_Routes.size() + COUNT);

		for (size_t i = 0; i < COUNT; ++i)
		{
			if (Routes[i].Handler != nullptr)
			{
				if (Routes[i].sMethod == "WEBSOCKET")
				{
					KRESTRoute::Options Options = Routes[i].Options;
					Options.Set(KRESTRoute::Options::WEBSOCKET);
					AddRoute(KRESTRoute("GET", Options, Routes[i].sRoute, Routes[i].sDocumentRoot, object, Routes[i].Handler, Routes[i].Parser));
				}
				else
				{
					AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].Options, Routes[i].sRoute, Routes[i].sDocumentRoot, object, Routes[i].Handler, Routes[i].Parser));
				}
			}
			else
			{
				if (Routes[i].sMethod == "REWRITE")
				{
					AddRewrite(KHTTPRewrite(Routes[i].sRoute, Routes[i].sDocumentRoot));
				}
				else if (Routes[i].sMethod == "REDIRECT")
				{
					AddRedirect(KHTTPRewrite(Routes[i].sRoute, Routes[i].sDocumentRoot));
				}
				else
				{
					AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].Options, Routes[i].sRoute, Routes[i].sDocumentRoot, *this, &KRESTRoutes::WebServer, Routes[i].Parser));
				}
			}
		}
	}

	//-----------------------------------------------------------------------------
	/// Set default route (matching all path requests not satisfied by other routes)
	/// @param Callback default callback if none of the routes matches
	/// @param Options set various options for this route, e.g. SSO authentication (takes an initializer list for multiple options)
	/// @param Parser any of the parser types for input parsing (PLAIN, JSON, XML, WWWFORM) or NOREAD for no parsing
	void SetDefaultRoute(KRESTRoute::RESTCallback Callback, KRESTRoute::Options Options = KRESTRoute::Options{}, KRESTRoute::ParserType Parser = KRESTRoute::JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get default route (matching all path requests not satisfied by other routes)
	const KRESTRoute& GetDefaultRoute() const
	//-----------------------------------------------------------------------------
	{
		return m_DefaultRoute;
	}

	//-----------------------------------------------------------------------------
	/// Clear all routes
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Rewrite a request path by matching with the rewrite rules - all are applied consecutively in the
	/// order of their definition
	/// @return count of matching rewrite rules that changed the path
	std::size_t RewritePath(KStringRef& sPath) const
	//-----------------------------------------------------------------------------
	{
		return RegexMatchPath(sPath, m_Rewrites);
	}

	//-----------------------------------------------------------------------------
	/// Redirect a request path by matching with the redirect rules - all are applied consecutively in the
	/// order of their definition. Throws a permanent redirect if matching
	std::size_t RedirectPath(KStringRef& sPath) const
	//-----------------------------------------------------------------------------
	{
		return RegexMatchPath(sPath, m_Redirects);
	}

	//-----------------------------------------------------------------------------
	/// Throws KHTTPError if no matching route found - fills additonal params in Path into Params
	/// @param Path the REST path from a request to match with the routes
	/// @param Params ref on a vector of parameters object, will be filled with the found rest path parameters (components starting with : or = )
	/// @param bIsWebSocket requested route is a websocket
	/// @param bCheckForWrongMethod if true, throw a different error message if a route was not matched only because of the request method. Slightly less performant.
	/// @return the found route, if any
	DEKAF2_NODISCARD
	const KRESTRoute& FindRoute(const KRESTPath& Path, Parameters& Params, bool bIsWebSocket, bool bCheckForWrongMethod) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Throws KHTTPError if no matching route found - fills additional params in Path into Params
	/// @param Path the REST path from a request to match with the routes
	/// @param Params ref on a url::KQuery object, will be filled with the found rest path parameters (components starting with : or = )
	/// @param bIsWebSocket requested route is a websocket
	/// @param bCheckForWrongMethod if true, throw a different error message if a route was not matched only because of the request method. Slightly less performant.
	/// @return the found route, if any
	DEKAF2_NODISCARD
	const KRESTRoute& FindRoute(const KRESTPath& Path, url::KQuery& Params, bool bIsWebSocket, bool bCheckForWrongMethod) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Default webserver implementation for static pages, can be used as callback parameter for routes
	void WebServer(KRESTServer& HTTP);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Return detailed statistics on each route
	/// @return a KJSON object with detailed usage and performance statistics on each route
	DEKAF2_NODISCARD
	KJSON GetRouterStats() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Checks if a route was not matched only because of the request method.
	/// @param Path the REST path from a request to match with the routes
	/// @return true if a route exists which differs only in the request method
	DEKAF2_NODISCARD
	bool CheckForWrongMethod(const KRESTPath& Path) const;
	//-----------------------------------------------------------------------------

//------
private:
//------

	using Routes    = std::vector<KRESTRoute>;
	using Rewrites  = std::vector<KHTTPRewrite>;
	using Redirects = std::vector<KHTTPRewrite>;

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	static std::size_t RegexMatchPath(KStringRef& sPath, const Rewrites& Rewrites);
	//-----------------------------------------------------------------------------

	Routes     m_Routes;
	Rewrites   m_Rewrites;
	Redirects  m_Redirects;
	KRESTRoute m_DefaultRoute;

}; // KRESTRoutes

DEKAF2_NAMESPACE_END
