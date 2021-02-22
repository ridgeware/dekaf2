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

#include "krestroute.h"
#include "krestserver.h"
#include "khttperror.h"
#include "kfileserver.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KRESTPath::KRESTPath(KHTTPMethod _Method, KString _sRoute)
//-----------------------------------------------------------------------------
	: KHTTPPath(std::move(_sRoute))
	, Method(std::move(_Method))
{
} // KRESTPath

namespace detail {

//-----------------------------------------------------------------------------
KRESTAnalyzedPath::KRESTAnalyzedPath(KHTTPMethod _Method, KString _sRoute)
//-----------------------------------------------------------------------------
	: KHTTPAnalyzedPath(std::move(_sRoute))
	, Method(std::move(_Method))
	, bHasParameters(sRoute.contains("/:") || sRoute.contains("/="))
{
} // KRESTAnalyzedPath

//-----------------------------------------------------------------------------
bool KRESTAnalyzedPath::HasParameter(KStringView sParam) const
//-----------------------------------------------------------------------------
{
	if (bHasParameters)
	{
		for (auto sPart : vURLParts)
		{
			if (sPart.front() == '=')
			{
				sPart.remove_prefix(1);
			}
			if (sPart == sParam)
			{
				return true;
			}
		}
	}

	return false;

} // HasParameter

} // end of namespace detail

//-----------------------------------------------------------------------------
KRESTRoute::KRESTRoute(KHTTPMethod _Method, bool _bAuth, KString _sRoute, KString _sDocumentRoot, RESTCallback _Callback, ParserType _Parser)
//-----------------------------------------------------------------------------
	: detail::KRESTAnalyzedPath(std::move(_Method), std::move(_sRoute))
	, Callback(std::move(_Callback))
	, sDocumentRoot(std::move(_sDocumentRoot))
	, Parser(_Parser)
	, bAuth(_bAuth)
{
} // KRESTRoute

//-----------------------------------------------------------------------------
bool KRESTRoute::Matches(const KRESTPath& Path, Parameters* Params, bool bCompareMethods, bool bCheckWebservers) const
//-----------------------------------------------------------------------------
{
	if ((!bCompareMethods || Method.empty() || Method == Path.Method) && (bCheckWebservers || sDocumentRoot.empty()))
	{
		if (!bHasParameters && !bHasWildCardFragment)
		{
			if (DEKAF2_UNLIKELY(bHasWildCardAtEnd))
			{
				// this is a plain route with a wildcard at the end
				if (DEKAF2_UNLIKELY(Path.sRoute.starts_with(sRoute)))
				{
					// take care that we only match full fragments, not parts of them
					if (Path.sRoute.size() == sRoute.size() || Path.sRoute[sRoute.size()] == '/')
					{
						return true;
					}
				}
			}
			else
			{
				// this is a plain route - we do not check part by part
				if (DEKAF2_UNLIKELY(Path.sRoute == sRoute))
				{
					return true;
				}
			}
		}
		else
		{
			// we have parameters or wildcard fragments, check part by part of the route
			if (!Path.vURLParts.empty() && vURLParts.size() >= Path.vURLParts.size())
			{
				if (Params)
				{
					Params->clear();
				}

				auto req = Path.vURLParts.cbegin();
				bool bFound { true };
				bool bOnlyParms { false };

				for (auto& part : vURLParts)
				{
					if (DEKAF2_UNLIKELY(bOnlyParms))
					{
						// check remaining route fragments for being :parameters or =parameters
						if (part.front() != ':' && part.front() != '=')
						{
							bFound = false;
							break;
						}
						continue;
					}

					if (DEKAF2_LIKELY(part != *req))
					{
						if (DEKAF2_UNLIKELY(part.front() == ':'))
						{
							// this is a variable, add the value to our temporary query parms
							if (Params)
							{
								Params->push_back({part, *req});
							}
						}
						else if (DEKAF2_UNLIKELY(part.front() == '='))
						{
							// this is a variable
							KStringView sName = part;
							// remove the '='
							sName.remove_prefix(1);
							// and add the value to our temporary query parms
							if (Params)
							{
								Params->push_back({sName, *req});
							}
						}
						else if (DEKAF2_LIKELY(part != "*"))
						{
							// this is not a wildcard
							// therefore this route is not matching
							bFound = false;
							break;
						}
					}

					// found, continue comparison
					if (++req == Path.vURLParts.cend())
					{
						// end of Path reached, check if remaining Route
						// fragments are parameters
						bOnlyParms = true;
					}
				}

				if (bFound)
				{
					return true;
				}
			}
		}
	}

	return false;

} // Matches


//-----------------------------------------------------------------------------
KRESTRoutes::KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute, KString sDocumentRoot, bool _bAuth)
//-----------------------------------------------------------------------------
	: m_DefaultRoute(KRESTRoute(KHTTPMethod{}, _bAuth, "/", std::move(sDocumentRoot), std::move(DefaultRoute)))
{
}

//-----------------------------------------------------------------------------
void KRESTRoutes::AddRoute(KRESTRoute _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(std::move(_Route));

} // AddRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::SetDefaultRoute(KRESTRoute::RESTCallback Callback, bool bAuth, KRESTRoute::ParserType Parser)
//-----------------------------------------------------------------------------
{
	m_DefaultRoute.Callback = std::move(Callback);
	m_DefaultRoute.Parser = Parser;
	m_DefaultRoute.bAuth = bAuth;

} // SetDefaultRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::clear()
//-----------------------------------------------------------------------------
{
	m_Routes.clear();
	m_DefaultRoute.Callback = nullptr;

} // clear

//-----------------------------------------------------------------------------
bool KRESTRoutes::CheckForWrongMethod(const KRESTPath& Path) const
//-----------------------------------------------------------------------------
{
	// check if we only missed a route because of a wrong request method
	for (const auto& it : m_Routes)
	{
		// do not test if method was empty (= all would have matched) or OPTIONS
		if (!it.Method.empty() && (it.Method != KHTTPMethod::OPTIONS))
		{
			if (it.Matches(Path, nullptr, false, false))
			{
				return true;
			}
		}
	}

	return false;

} // CheckForWrongMethod

//-----------------------------------------------------------------------------
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTPath& Path, Parameters& Params, bool bCheckForWrongMethod) const
//-----------------------------------------------------------------------------
{
	kDebug (2, "looking up: {} {}" , Path.Method.Serialize(), Path.sRoute);

	// check for a matching route
	for (const auto& it : m_Routes)
	{
		kDebug (3, "evaluating: {} {}" , it.Method.Serialize(), it.sRoute);
		if (it.Matches(Path, &Params, true, true))
		{
			kDebug (2, "     found: {} {}", it.Method.Serialize(), it.sRoute);
			return it;
		}
	}

	// no matching route, return default route if available
	if (m_DefaultRoute.Callback)
	{
		kDebug (2, "not found, returning default route");
		return m_DefaultRoute;
	}

	if (bCheckForWrongMethod)
	{
		if (CheckForWrongMethod(Path))
		{
			kDebug (2, "request method {} not supported for path: {}", Path.Method.Serialize(), Path.sRoute);
			throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("request method {} not supported for path: {}", Path.Method.Serialize(), Path.sRoute) };
		}
	}

	// no match at all
	kDebug (2, "invalid path: {} {}", Path.Method.Serialize(), Path.sRoute);
	throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("invalid path: {} {}", Path.Method.Serialize(), Path.sRoute) };

} // FindRoute

//-----------------------------------------------------------------------------
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTPath& Path, url::KQuery& Params, bool bCheckForWrongMethod) const
//-----------------------------------------------------------------------------
{
	Parameters parms;

	auto& ret = FindRoute(Path, parms, bCheckForWrongMethod);

	// add all variables from the path into the request query
	for (const auto& qp : parms)
	{
		Params->Add(qp.first, qp.second);
	}

	return ret;

} // FindRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::WebServer(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	if (HTTP.Request.Method != KHTTPMethod::GET)
	{
		kDebug(1, "invalid method: {}", HTTP.Request.Method.Serialize());
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("invalid method: {}", HTTP.Request.Method.Serialize()) };
	}

	KFileServer FileServer;

	FileServer.Open(HTTP.Route->sDocumentRoot,
					HTTP.RequestPath.sRoute,
					HTTP.Route->sRoute,
					HTTP.Request.Resource.Path.get());

	if (FileServer.RedirectAsDirectory())
	{
		// redirect
		KString sRedirect = HTTP.Request.Resource.Path.get();
		sRedirect += '/';

		HTTP.Response.SetStatus(KHTTPError::H301_MOVED_PERMANENTLY, "Moved Permanently");
		HTTP.Response.Headers.Remove(KHTTPHeader::CONTENT_TYPE);
		HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, std::move(sRedirect));
	}
	else if (FileServer.Exists())
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, FileServer.GetMIMEType(true));
		HTTP.SetStreamToOutput(FileServer.GetStreamForReading(), FileServer.GetFileSize());
	}
	else
	{
		// This file does not exist.. now check if we should better return
		// a REST error code, or a HTTP server error code

		if (CheckForWrongMethod(HTTP.RequestPath))
		{
			kDebug (2, "request method {} not supported for path: {}", HTTP.RequestPath.Method.Serialize(), HTTP.RequestPath.sRoute);
			throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("request method {} not supported for path: {}", HTTP.RequestPath.Method.Serialize(), HTTP.RequestPath.sRoute) };
		}
		else
		{
			kDebug(1, "Cannot open file: {}", FileServer.GetFileSystemPath());
			HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "file not found" };
		}

	}

} // WebServer

static_assert(std::is_nothrow_move_constructible<KRESTPath>::value,
			  "KRESTPath is intended to be nothrow move constructible, but is not!");

static_assert(std::is_nothrow_move_constructible<detail::KRESTAnalyzedPath>::value,
			  "KRESTAnalyzedPath is intended to be nothrow move constructible, but is not!");

// if std::function is not yet supported by this lib to be nothrow, don't test dependant class
static_assert(!std::is_nothrow_move_constructible<std::function<void(int)>>::value || std::is_nothrow_move_constructible<KRESTRoute>::value,
			  "KRESTRoute is intended to be nothrow move constructible, but is not!");

// if std::function is not yet supported by this lib to be nothrow, don't test dependant class
static_assert(!std::is_nothrow_move_constructible<std::function<void(int)>>::value || std::is_nothrow_move_constructible<KRESTRoutes>::value,
			  "KRESTRoutes is intended to be nothrow move constructible, but is not!");

} // end of namespace dekaf2
