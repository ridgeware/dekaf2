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

#include "khttprouter.h"
#include "khttperror.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPRoute::KHTTPRoute(KString _sRoute, HTTPCallback _Callback)
//-----------------------------------------------------------------------------
	: sRoute(std::move(_sRoute))
	, Callback(_Callback)
{
}

//-----------------------------------------------------------------------------
bool KHTTPRoutes::AddRoute(const KHTTPRoute& _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(_Route);
	return true;

} // AddRoute

//-----------------------------------------------------------------------------
bool KHTTPRoutes::AddRoute(KHTTPRoute&& _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(std::move(_Route));
	return true;

} // AddRoute

//-----------------------------------------------------------------------------
void KHTTPRoutes::SetDefaultRoute(KHTTPRoute::HTTPCallback Callback)
//-----------------------------------------------------------------------------
{
	m_DefaultRoute.Callback = Callback;

} // SetDefaultRoute

//-----------------------------------------------------------------------------
void KHTTPRoutes::clear()
//-----------------------------------------------------------------------------
{
	m_Routes.clear();
	m_DefaultRoute.Callback = nullptr;

} // clear

//-----------------------------------------------------------------------------
const KHTTPRoute& KHTTPRoutes::FindRoute(const KHTTPRoute& Route) const
//-----------------------------------------------------------------------------
{
	for (const auto& it : m_Routes)
	{
		if (DEKAF2_UNLIKELY(Route.sRoute == it.sRoute))
		{
			return it;
		}
	}

	if (m_DefaultRoute.Callback)
	{
		return m_DefaultRoute;
	}

	throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("unknown address {}", Route.sRoute) };

} // FindRoute

//-----------------------------------------------------------------------------
KHTTPRouter::~KHTTPRouter()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KHTTPRouter::Execute(const KHTTPRoutes& Routes, KStringView sBaseRoute)
//-----------------------------------------------------------------------------
{
	try
	{
		KStringView sURLPath = Request.Resource.Path;

		// remove_prefix is true when called with empty argument or with matching argument
		if (!sURLPath.remove_prefix(sBaseRoute))
		{
			// bad prefix
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("url does not start with {}", sBaseRoute) };
		}

		auto Route = Routes.FindRoute(KHTTPRoute(sURLPath));

		if (!Route.Callback)
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
		}

		Route.Callback(*this);

		return true;
	}

	catch (const std::exception& ex)
	{
		ErrorHandler(ex);
	}

	return false;

} // Execute

//-----------------------------------------------------------------------------
void KHTTPRouter::ErrorHandler(const std::exception& ex)
//-----------------------------------------------------------------------------
{

} // ErrorHandler

} // end of namespace dekaf2
