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

#include "khttprouter.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPRouter::Route::Route(KHTTPMethod _Method, const KString& _sRoute, RouteCallback _Callback)
//-----------------------------------------------------------------------------
	: Method(_Method)
	, sRoute(_sRoute)
	, Callback(_Callback)
{
}

//-----------------------------------------------------------------------------
KHTTPRouter::~KHTTPRouter()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KHTTPRouter::Execute()
//-----------------------------------------------------------------------------
{

	return false;

} // Execute

//-----------------------------------------------------------------------------
bool KHTTPRouter::SetBaseRoute(KStringView sBaseRoute)
//-----------------------------------------------------------------------------
{
	m_sBaseRoute = m_sBaseRoute;

	return true;

} // SetBaseRoute

//-----------------------------------------------------------------------------
void KHTTPRouter::SetStaticRouteTable(const StaticRoute* StaticRoutes)
//-----------------------------------------------------------------------------
{
	m_StaticRoutes = StaticRoutes;

} // SetStaticRouteTable

//-----------------------------------------------------------------------------
bool KHTTPRouter::AddRoute(const Route& _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(_Route);
	return true;

} // AddRoute

//-----------------------------------------------------------------------------
bool KHTTPRouter::AddRoute(KStringView sRoute, RouteCallback Callback, const KHTTPMethod& Method)
//-----------------------------------------------------------------------------
{
	return AddRoute(Route(Method, sRoute, Callback));

} // AddRoute

//-----------------------------------------------------------------------------
void KHTTPRouter::SetDefaultRoute(RouteCallback Callback)
//-----------------------------------------------------------------------------
{
	m_DefaultRoute = Callback;

} // SetDefaultRoute

//-----------------------------------------------------------------------------
void KHTTPRouter::ErrorHandler()
//-----------------------------------------------------------------------------
{

} // ErrorHandler

//-----------------------------------------------------------------------------
KHTTPRouter::Routes::const_iterator KHTTPRouter::FindRoute(KStringView sPath) const
//-----------------------------------------------------------------------------
{

	return m_Routes.end();

} // FindRoute

} // end of namespace dekaf2
