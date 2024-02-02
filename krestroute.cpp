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
#include "kjson.h"
#include "ktime.h"
#include "kduration.h"

DEKAF2_NAMESPACE_BEGIN

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
KRESTRoute::KRESTRoute(KHTTPMethod _Method, class Options _Options, KString _sRoute, KString _sDocumentRoot, RESTCallback _Callback, ParserType _Parser)
//-----------------------------------------------------------------------------
	: detail::KRESTAnalyzedPath(std::move(_Method), std::move(_sRoute))
	, Callback(std::move(_Callback))
	, sDocumentRoot(std::move(_sDocumentRoot))
	, Parser(_Parser)
	, Option(_Options)
{
} // KRESTRoute

//-----------------------------------------------------------------------------
bool KRESTRoute::Matches(const KRESTPath& Path, Parameters* Params, bool bCompareMethods, bool bCheckWebservers, bool bIsWebSocket) const
//-----------------------------------------------------------------------------
{
	if ((!bCompareMethods || bIsWebSocket == Option.Has(Options::WEBSOCKET)) &&
		(!bCompareMethods || Method.empty() || Method == Path.Method)        &&
		(bCheckWebservers || sDocumentRoot.empty()))
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
								Params->emplace_back(part, *req);
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
								Params->emplace_back(sName, *req);
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
KRESTRoutes::KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute, KString sDocumentRoot, KRESTRoute::Options Options)
//-----------------------------------------------------------------------------
	: m_DefaultRoute(KRESTRoute(KHTTPMethod{}, Options, "/", std::move(sDocumentRoot), std::move(DefaultRoute)))
{
}

//-----------------------------------------------------------------------------
void KRESTRoutes::AddRoute(KRESTRoute _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(std::move(_Route));

} // AddRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::AddRewrite(KHTTPRewrite _Rewrite)
//-----------------------------------------------------------------------------
{
	m_Rewrites.push_back(std::move(_Rewrite));

} // AddRewrite

//-----------------------------------------------------------------------------
void KRESTRoutes::AddRedirect(KHTTPRewrite _Redirect)
//-----------------------------------------------------------------------------
{
	m_Redirects.push_back(std::move(_Redirect));

} // AddRewrite

//-----------------------------------------------------------------------------
void KRESTRoutes::SetDefaultRoute(KRESTRoute::RESTCallback Callback, KRESTRoute::Options Options, KRESTRoute::ParserType Parser)
//-----------------------------------------------------------------------------
{
	m_DefaultRoute.Callback = std::move(Callback);
	m_DefaultRoute.Parser = Parser;
	m_DefaultRoute.Option = Options;

} // SetDefaultRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::clear()
//-----------------------------------------------------------------------------
{
	m_Routes.clear();
	m_Rewrites.clear();
	m_DefaultRoute.Callback = nullptr;

} // clear

//-----------------------------------------------------------------------------
std::size_t KRESTRoutes::RegexMatchPath(KStringRef& sPath, const Rewrites& Rewrites)
//-----------------------------------------------------------------------------
{
	std::size_t iRewrites { 0 };

	for (const auto& it : Rewrites)
	{
		kDebug(2, "evaluating {} to match {} > {}", sPath, it.RegexFrom.Pattern(), it.sTo);

		if (it.RegexFrom.Replace(sPath, it.sTo) > 0)
		{
			kDebug(1, "matched, changed path to {}", sPath);

			++iRewrites;
		}
	}

	return iRewrites;

} // RegexMatchPath

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
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTPath& Path, Parameters& Params, bool bIsWebSocket, bool bCheckForWrongMethod) const
//-----------------------------------------------------------------------------
{
	if (bCheckForWrongMethod && Path.Method == KHTTPMethod::INVALID)
	{
		kDebug (2, "invalid request method");
		throw KHTTPError { KHTTPError::H4xx_BADMETHOD, "invalid request method" };
	}

	kDebug (2, "looking up: {} {}" , Path.Method.Serialize(), Path.sRoute);

	// check for a matching route
	for (const auto& it : m_Routes)
	{
		kDebug (3, "evaluating: {} {}{}" , it.Method.Serialize(), it.sRoute, bIsWebSocket ? " (websocket)" : "");
		if (it.Matches(Path, &Params, true, true, bIsWebSocket))
		{
			kDebug (2, "     found: {} {}{}", it.Method.Serialize(), it.sRoute, bIsWebSocket ? " (websocket)" : "");
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
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTPath& Path, url::KQuery& Params, bool bIsWebSocket, bool bCheckForWrongMethod) const
//-----------------------------------------------------------------------------
{
	Parameters parms;

	auto& ret = FindRoute(Path, parms, bIsWebSocket, bCheckForWrongMethod);

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
					HTTP.Request.Resource.Path.get().back() == '/');

	if (FileServer.RedirectAsDirectory())
	{
		// redirect
		KString sRedirect = HTTP.Request.Resource.Path.get();
		sRedirect += '/';

		HTTP.Response.Headers.Remove(KHTTPHeader::CONTENT_TYPE);
		HTTP.Response.Headers.Set(KHTTPHeader::LOCATION, std::move(sRedirect));

		if (HTTP.Request.Method == KHTTPMethod::GET || HTTP.Request.Method == KHTTPMethod::HEAD)
		{
			HTTP.Response.SetStatus(KHTTPError::H301_MOVED_PERMANENTLY);
		}
		else
		{
			HTTP.Response.SetStatus(KHTTPError::H308_PERMANENT_REDIRECT);
		}
	}
	else if (FileServer.Exists())
	{
		HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, FileServer.GetMIMEType(true).Serialize());

		auto tIfModifiedSince = kParseHTTPTimestamp(HTTP.Request.Headers.Get(KHTTPHeader::IF_MODIFIED_SINCE));
		auto tLastModified    = FileServer.GetFileStat().ModificationTime();

		if (tLastModified <= tIfModifiedSince)
		{
			HTTP.Response.SetStatus(KHTTPError::H304_NOT_MODIFIED);
		}
		else
		{
			HTTP.Response.Headers.Set(KHTTPHeader::LAST_MODIFIED   , KHTTPHeader::DateToString(tLastModified));
			HTTP.SetStreamToOutput(FileServer.GetStreamForReading(), FileServer.GetFileStat().Size());
		}
	}
	else
	{
		// This file does not exist.. now check if we should better return
		// a REST error code, or an HTTP server error code

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

//-----------------------------------------------------------------------------
KJSON KRESTRoutes::GetRouterStats() const
//-----------------------------------------------------------------------------
{
	KJSON Stats = KJSON::array();

	for (const auto& Route : m_Routes)
	{
		auto Statistics = Route.Statistics.shared().get();

		if (Statistics.Durations.empty())
		{
			// no data for this route
			continue;
		}

		// the first timer should have the max rounds
		auto iRounds = Statistics.Durations.Rounds(0);

		if (iRounds)
		{
			KJSON jUSecs {
				{ "total" , Statistics.Durations.duration().microseconds().count() / iRounds  }
			};

			for (const auto& it : KRESTServer::Timers)
			{
				jUSecs.push_back({ it.sLabel, Statistics.Durations[it.Value].average().microseconds().count() });
			}

			KJSON jRoute {
				{ "method"   , Route.Method.Serialize() },
				{ "route"    , Route.sRoute             },
				{ "requests" , iRounds                  },
				{ "bytes"    , {
					{ "total", {
						{ "rx"   , Statistics.iRxBytes  },
						{ "tx"   , Statistics.iTxBytes  }
					}},
					{ "avg"  , {
						{ "rx"   , Statistics.iRxBytes / iRounds },
						{ "tx"   , Statistics.iTxBytes / iRounds }
					}}
				}},
				{ "usecs"    , std::move(jUSecs)        }
			};

			Stats.push_back(jRoute);
		}
	}

	return Stats;

} // GetRouterStats

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

DEKAF2_NAMESPACE_END
