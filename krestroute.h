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
#include <vector>
#include <memory>

/// @file krestroute.h
/// Primitives for REST routing

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
	/// Construct an analyzed REST path. Notice that _sRoute is a KStringView, and
	/// the pointed-to string must stay visible during the lifetime of this class.
	KRESTAnalyzedPath(KHTTPMethod _Method, KStringView _sRoute);
	//-----------------------------------------------------------------------------

	KHTTPMethod Method;  	// e.g. GET, or empty for all

	/// checks if this path contains a parameter of the given name (:param or =param)
	bool HasParameter(KStringView sParam) const;

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

	enum ParserType { PLAIN, JSON, XML, WWWFORM, NOREAD };

	using Function = void(*)(KRESTServer& REST);

	template<class Object>
	using MemberFunction = void(Object::*)(KRESTServer&);

	using RESTCallback = std::function<void(KRESTServer&)>;

	using Parameters = std::vector<std::pair<KStringView, KStringView>>;

	//-----------------------------------------------------------------------------
	/// Construct a REST route on a function. Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class
	KRESTRoute(KHTTPMethod _Method, bool _bAuth, KStringView _sRoute, RESTCallback _Callback, ParserType _Parser = JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Construct a REST route on an object member function. Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class. Also, the object reference
	/// must stay valid throughout the lifetime of this class (it is a reference on a constructed
	/// object which method will be called)
	template<class Object>
	KRESTRoute(KHTTPMethod _Method, bool _bAuth, KStringView _sRoute, Object& object, MemberFunction<Object> _Callback, ParserType _Parser = JSON)
	//-----------------------------------------------------------------------------
	: KRESTRoute(std::move(_Method), _bAuth, _sRoute, std::bind(_Callback, &object, std::placeholders::_1), _Parser)
	{
	}

	//-----------------------------------------------------------------------------
	/// Compare this route part by part with a given path, and return true if matching.
	/// Params returns the variables in the path.
	bool Matches(const KRESTPath& Path, Parameters& Params, bool bCompareMethods = true) const;
	//-----------------------------------------------------------------------------

	RESTCallback Callback;
	ParserType Parser;
	bool bAuth;

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
		FunctionTable() = default;

		// C++11 needs constructor for initializer list initialization
		constexpr
		FunctionTable(KStringView _sMethod, bool _bAuth, KStringView _sRoute, KRESTRoute::Function _Handler, KRESTRoute::ParserType _Parser = KRESTRoute::JSON)
		: sMethod(_sMethod)
		, bAuth(_bAuth)
		, sRoute(_sRoute)
		, Handler(_Handler)
		, Parser(_Parser)
		{}

		KStringView sMethod;
		bool bAuth;
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
		MemberFunctionTable() = default;

		// C++11 needs constructor for initializer list initialization
		constexpr
		MemberFunctionTable(KStringView _sMethod, bool _bAuth, KStringView _sRoute, KRESTRoute::MemberFunction<Object> _Handler, KRESTRoute::ParserType _Parser = KRESTRoute::JSON)
		: sMethod(_sMethod)
		, bAuth(_bAuth)
		, sRoute(_sRoute)
		, Handler(_Handler)
		, Parser(_Parser)
		{}

		KStringView sMethod;
		bool bAuth;
		KStringView sRoute;
		KRESTRoute::MemberFunction<Object> Handler;
		KRESTRoute::ParserType Parser = KRESTRoute::JSON;
	};

	using Parameters = KRESTRoute::Parameters;

	//-----------------------------------------------------------------------------
	/// ctor
	KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute = nullptr, bool _bAuth = false);
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
			AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].bAuth, Routes[i].sRoute, Routes[i].Handler, Routes[i].Parser));
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
			AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].bAuth, Routes[i].sRoute, object, Routes[i].Handler, Routes[i].Parser));
		}
	}

	//-----------------------------------------------------------------------------
	/// Set default route (matching all path requests not satisfied by other routes)
	void SetDefaultRoute(KRESTRoute::RESTCallback Callback, bool bAuth = false, KRESTRoute::ParserType Parser = KRESTRoute::JSON);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Clear all routes
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Throws KHTTPError if no matching route found - fills additonal params in Path into Params
	const KRESTRoute& FindRoute(const KRESTPath& Path, Parameters& Params, bool bCheckForWrongMethod) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Throws KHTTPError if no matching route found - fills additional params in Path into Params
	const KRESTRoute& FindRoute(const KRESTPath& Path, url::KQuery& Params, bool bCheckForWrongMethod) const;
	//-----------------------------------------------------------------------------

//------
private:
//------

	using Routes = std::vector<KRESTRoute>;

	Routes m_Routes;
	KRESTRoute m_DefaultRoute;

}; // KRESTRoutes

} // end of namespace dekaf2
