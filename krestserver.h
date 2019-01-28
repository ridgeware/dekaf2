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
#include "kstring.h"
#include "kstringview.h"
#include "kurl.h"
#include "kjson.h"

/// @file krestserver.h
/// HTTP REST server implementation

namespace dekaf2 {

class KRESTServer;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KRESTRoute
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using Function = void(*)(KRESTServer& REST);

	template<class Object>
	using MemberFunction = void(Object::*)(KRESTServer&);

	using RESTCallback = std::function<void(KRESTServer&)>;

	using URLParts = std::vector<KStringView>;

	/// Construct a REST route. Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class
	KRESTRoute(KHTTPMethod _Method, KStringView _sRoute, RESTCallback _Callback = nullptr);

	/// Construct a REST route on an object member function. Notice that _sRoute is a KStringView, and the pointed-to
	/// string must stay visible during the lifetime of this class. Also, the object reference
	/// must stay valid throughout the lifetime of this class (it is a reference on a constructed
	/// object which method will be called)
	template<class Object>
	KRESTRoute(KHTTPMethod _Method, KStringView _sRoute, Object& object, MemberFunction<Object> _Callback)
	: KRESTRoute(_Method, _sRoute, std::bind(_Callback, &object, std::placeholders::_1))
	{
	}

	static size_t SplitURL(URLParts& Parts, KStringView sURLPath);

	KHTTPMethod Method;  	// e.g. GET, or empty for all
	KStringView sRoute;     // e.g. "/employee/:id/address" or "/help"
	RESTCallback Callback;
	URLParts vURLParts;
	bool bHasParameters { false };

//------
private:
//------

}; // KRESTRoute

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KRESTRoutes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	// prototype for a handler function table
	struct FunctionTable
	{
		KStringView sMethod;
		KStringView sRoute;
		KRESTRoute::Function Handler;
	};

	// prototype for a handler object member function table
	template<class Object>
	struct MethodTable
	{
		KStringView sMethod;
		KStringView sRoute;
		KRESTRoute::MemberFunction<Object> Handler;
	};

	using Parameters = std::vector<std::pair<KStringView, KStringView>>;

	KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute = nullptr);

	/// Add a REST route. Notice that _Route contains a KStringView, of which the pointed-to
	/// string must stay visible during the lifetime of this class
	bool AddRoute(const KRESTRoute& _Route);

	/// Add a REST route. Notice that _Route contains a KStringView, of which the pointed-to
	/// string must stay visible during the lifetime of this class
	bool AddRoute(KRESTRoute&& _Route);

	/// Add routes from a table of route and handler function definitions
	template<std::size_t COUNT>
	bool AddFunctionTable(const FunctionTable (&Routes)[COUNT])
	{
		m_Routes.reserve(m_Routes.size() + COUNT);
		for (size_t i = 0; i < COUNT; ++i)
		{
			if (!AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].sRoute, Routes[i].Handler)))
			{
				return false;
			}
		}
		return true;
	}

	/// Add routes from a table of route and handler object member function definitions
	template<class Object, std::size_t COUNT>
	bool AddMethodTable(Object& object, const MethodTable<Object> (&Routes)[COUNT])
	{
		m_Routes.reserve(m_Routes.size() + COUNT);
		for (size_t i = 0; i < COUNT; ++i)
		{
			if (!AddRoute(KRESTRoute(Routes[i].sMethod, Routes[i].sRoute, object, Routes[i].Handler)))
			{
				return false;
			}
		}
		return true;
	}

	void SetDefaultRoute(KRESTRoute::RESTCallback Callback);
	void clear();

	/// throws KHTTPError if no matching route found - fills additonal params in Path into Params
	const KRESTRoute& FindRoute(const KRESTRoute& Route, Parameters& Params) const;
	/// throws KHTTPError if no matching route found - fills additional params in Path into Params
	const KRESTRoute& FindRoute(const KRESTRoute& Route, url::KQuery& Params) const;

//------
private:
//------

	using Routes = std::vector<KRESTRoute>;

	Routes m_Routes;
	KRESTRoute m_DefaultRoute;

}; // KRESTRoutes


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KRESTServer : public KHTTPServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum OutputType	{ HTTP, LAMBDA, CLI };

	// forward constructors
	using KHTTPServer::KHTTPServer;

	//-----------------------------------------------------------------------------
	/// handler for one request
	bool Execute(const KRESTRoutes& Routes, KStringView sBaseRoute = KStringView{}, OutputType Out = HTTP);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	const KString& GetRequestMethod() const
	//-----------------------------------------------------------------------------
	{
		return Request.Method;
	}

	//-----------------------------------------------------------------------------
	const KString& GetRequestPath() const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Path.get();
	}

	//-----------------------------------------------------------------------------
	const url::KQueryParms& GetQueryParms() const
	//-----------------------------------------------------------------------------
	{
		return Request.Resource.Query.get();
	}

	//-----------------------------------------------------------------------------
	void SetStatus(int iCode);
	//-----------------------------------------------------------------------------

//	uint64_t GetMilliseconds() const { return (m_timer.elapsed() / (1000 * 1000)); }

	//-----------------------------------------------------------------------------
	void SetRawOutput(KStringView sRaw)
	//-----------------------------------------------------------------------------
	{
		m_sRawOutput = sRaw;
	}

	//-----------------------------------------------------------------------------
	void AddRawOutput(KStringView sRaw)
	//-----------------------------------------------------------------------------
	{
		m_sRawOutput += sRaw;
	}

	//-----------------------------------------------------------------------------
	void SetMessage(KStringView sMessage)
	//-----------------------------------------------------------------------------
	{
		m_sMessage = sMessage;
	}

	struct json_t
	{
		KJSON rx;
		KJSON tx;

		void clear();
	};

	json_t json;


//------
protected:
//------

	virtual void Output(OutputType Out = HTTP);

	virtual void ErrorHandler(const std::exception& ex, OutputType Out = HTTP);

//------
private:
//------

	KString m_sMessage;
	KString m_sRawOutput;

}; // KRESTServer

} // end of namespace dekaf2
