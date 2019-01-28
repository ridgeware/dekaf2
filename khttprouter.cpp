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
KHTTPRoute::KHTTPRoute(KStringView _sRoute, HTTPCallback _Callback)
//-----------------------------------------------------------------------------
	: sRoute(std::move(_sRoute))
	, Callback(std::move(_Callback))
{
	if (sRoute.front() != '/')
	{
		kWarning("error: route does not start with a slash: {}", sRoute);
	}
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
		Response.clear();

		if (!KHTTPServer::Parse())
		{
			throw KHTTPError  { KHTTPError::H4xx_BADREQUEST, KHTTPServer::Error() };
		}

		Response.SetStatus(200, "OK");
		Response.sHTTPVersion = Request.sHTTPVersion;

		KStringView sURLPath = Request.Resource.Path;

		// remove_prefix is true when called with empty argument or with matching argument
		if (!sURLPath.remove_prefix(sBaseRoute))
		{
			// bad prefix
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("url does not start with base route: {} <> {}", sBaseRoute, sURLPath) };
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
	const KHTTPError* xex = dynamic_cast<const KHTTPError*>(&ex);

	if (xex)
	{
		Response.SetStatus(xex->GetHTTPStatusCode(), xex->GetHTTPStatusString());
	}
	else
	{
		Response.SetStatus(KHTTPError::H5xx_ERROR, "INTERNAL SERVER ERROR");
	}

	KStringViewZ sError = ex.what();

	KString sContent = R"(<html><head>Error</head><body><h2>)";

	if (sError.empty())
	{
		sContent += Response.sStatusString;
	}
	else
	{
		sContent += sError;
	}

	// close the html body
	sContent += R"(</h2></body></html>\r\n)";

	// compute and set the Content-Length header:
	Response.Headers.Set(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(sContent.length()));
	Response.Headers.Set(KHTTPHeaders::CONTENT_TYPE, KMIME::HTML_UTF8);
	Response.Headers.Set(KHTTPHeaders::CONNECTION, "close");

	// write full response and headers to output
	Response.Serialize();

	// finally, output the content:
	kDebug (2, "{}", sContent);
	Response.Write (sContent);

} // ErrorHandler

} // end of namespace dekaf2
