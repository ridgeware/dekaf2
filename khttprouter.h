/*
//-----------------------------------------------------------------------------//
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
#include "khttp_method.h"
#include "kstring.h"
#include "kstringview.h"

/// @file khttprouter.h
/// HTTP server router layer implementation - associating callbacks
/// with specific URL paths

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPRouter : public KHTTPServer
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	virtual ~KHTTPRouter();

	using RouteCallback = std::function<void(void)>;

	struct StaticRoute
	{
		KStringView Method;      // e.g. GET, or empty for all
		KStringView sRoute;      // e.g. "/employee/:id/address" or "/help"
		RouteCallback Callback;
	};

	struct Route
	{
		Route(KHTTPMethod _Method, const KString& _sRoute, RouteCallback _Callback);

		KHTTPMethod Method;  	// e.g. GET, or empty for all
		KString sRoute;       	// e.g. "/employee/:id/address" or "/help"
		RouteCallback Callback;
	};

	// forward constructors
	using KHTTPServer::KHTTPServer;

	/// handler for one request
	bool Execute();

	bool SetBaseRoute(KStringView sBaseRoute);
	void SetStaticRouteTable(const StaticRoute* StaticRoutes);
	bool AddRoute(const Route& _Route);
	bool AddRoute(KStringView sRoute, RouteCallback Callback, const KHTTPMethod& Method = KHTTPMethod(""));
	void SetDefaultRoute(RouteCallback Callback);

//------
private:
//------

	KString m_sBaseRoute;    // e.g. "/HR/v1"

	const StaticRoute* m_StaticRoutes;

	using Routes = std::vector<Route>;

	Routes m_Routes;

	RouteCallback m_DefaultRoute;

//------
protected:
//------

	virtual void ErrorHandler();

	Routes::const_iterator FindRoute(KStringView sPath) const;


}; // KHTTPRouter


} // end of namespace dekaf2
