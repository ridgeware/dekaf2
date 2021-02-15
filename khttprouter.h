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

#include <vector>
#include "khttpserver.h"
#include "khttppath.h"
#include "khttp_method.h"
#include "kstring.h"
#include "kstringview.h"

/// @file khttprouter.h
/// HTTP server router layer implementation - associating callbacks
/// with specific URL paths

namespace dekaf2 {

class KHTTPRouter;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPRoute : public detail::KHTTPAnalyzedPath
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using HTTPCallback = std::function<void(KHTTPRouter&)>;

	/// Construct a HTTP route
	KHTTPRoute(KString _sRoute, KString _sDocumentRoot, HTTPCallback _Callback);

	//-----------------------------------------------------------------------------
	/// Compare this route part by part with a given path, and return true if matching.
	bool Matches(const KHTTPPath& Path) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Default webserver implementation for static pages
	static void WebServer(KHTTPRouter& HTTP);
	//-----------------------------------------------------------------------------

	HTTPCallback Callback;
	KString      sDocumentRoot;

}; // KHTTPRoute

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPRoutes
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using HTTPHandler = void(*)(KHTTPServer& REST);

	struct RouteTable
	{
		KStringView sMethod;
		KStringView sRoute;
		HTTPHandler Handler;
	};

	/// Add a HTTP route
	bool AddRoute(KHTTPRoute _Route);

	template<std::size_t COUNT>
	bool AddRouteTable(const RouteTable (&Routes)[COUNT])
	{
		m_Routes.reserve(m_Routes.size() + COUNT);
		for (size_t i = 0; i < COUNT; ++i)
		{
			if (!AddRoute(KHTTPRoute(Routes[i].sRoute, Routes[i].Handler)))
			{
				return false;
			}
		}
		return true;
	}

	void SetDefaultRoute(KHTTPRoute::HTTPCallback Callback);
	void clear();

	/// throws if no matching route found
	const KHTTPRoute& FindRoute(const KHTTPPath& Path) const;

//------
private:
//------

	using Routes = std::vector<KHTTPRoute>;

	Routes     m_Routes;
	KHTTPRoute m_DefaultRoute;

}; // KHTTPRoutes

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPRouter : public KHTTPServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	virtual ~KHTTPRouter();

	// forward constructors
	using KHTTPServer::KHTTPServer;

	/// handler for one request
	bool Execute(const KHTTPRoutes& Routes, KStringView sBaseRoute);

	const KHTTPRoute* Route { nullptr };

//------
protected:
//------

	virtual void ErrorHandler(const std::exception& ex);

}; // KHTTPRouter


} // end of namespace dekaf2
