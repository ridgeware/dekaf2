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
#include "kwebserver.h"
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
	, m_bHasParameters(sRoute.contains("/:") || sRoute.contains("/="))
{
} // KRESTAnalyzedPath

//-----------------------------------------------------------------------------
bool KRESTAnalyzedPath::HasParameter(KStringView sParam) const
//-----------------------------------------------------------------------------
{
	if (m_bHasParameters)
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
KRESTRoute::KRESTRoute(KHTTPMethod _Method, class Options _Options, KString _sRoute, KString _sDocumentRoot, RESTCallback _Callback, KJSON _Config)
//-----------------------------------------------------------------------------
	: detail::KRESTAnalyzedPath(std::move(_Method), std::move(_sRoute))
	, Callback(std::move(_Callback))
	, sDocumentRoot(std::move(_sDocumentRoot))
	, Option(_Options)
	, Config(std::move(_Config))
{
	auto it = Config.find("parser");

	if (it == Config.end())
	{
		it = Config.find("Parser");
	}

	if (it != Config.end())
	{
		if (it.value().is_string())
		{
#if !DEKAF2_KJSON2_IS_DISABLED
			// get_ref() is private in KJSON2, so convert to KJSON1
			auto& sParser = it.value().ToBase().get_ref<const KString&>();
#else
			auto& sParser = it.value().get_ref<const KString&>();
#endif

			switch (sParser.CaseHash())
			{
				case "JSON"_casehash:
					Parser = ParserType::JSON;
					break;
				case "PLAIN"_casehash:
					Parser = ParserType::PLAIN;
					break;
				case "XML"_casehash:
					Parser = ParserType::XML;
					break;
				case "WWWFORM"_casehash:
					Parser = ParserType::WWWFORM;
					break;
				case "NOREAD"_casehash:
					Parser = ParserType::NOREAD;
					break;
			}
		}
	}

} // KRESTRoute

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
		(!bCompareMethods 
		 || Method.empty()
		 || Method == Path.Method
		 || (Path.Method == KHTTPMethod::HEAD && Method == KHTTPMethod::GET)) &&
		(bCheckWebservers || sDocumentRoot.empty()))
	{
		if (!m_bHasParameters && !m_bHasWildCardFragment)
		{
			if (DEKAF2_UNLIKELY(m_bHasWildCardAtEnd))
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
KRESTRoutes::RouteBuilder::RouteBuilder(KRESTRoutes& Routes, KString sRoute)
//-----------------------------------------------------------------------------
: m_Routes(Routes)
, m_sRoute(std::move(sRoute))
{
}

//-----------------------------------------------------------------------------
KRESTRoutes::RouteBuilder::~RouteBuilder()
//-----------------------------------------------------------------------------
{
	AddRoute();
}

//-----------------------------------------------------------------------------
void KRESTRoutes::RouteBuilder::AddRoute(bool bKeepSettings)
//-----------------------------------------------------------------------------
{
	if (bKeepSettings)
	{
		m_Routes.AddRoute(KRESTRoute(m_Verb, m_Options, m_sRoute, std::move(m_Callback), m_Parser));
	}
	else
	{
		m_Routes.AddRoute(KRESTRoute(m_Verb, std::move(m_Options), std::move(m_sRoute), std::move(m_Callback), m_Parser));
	}

} // AddRoute

//-----------------------------------------------------------------------------
KRESTRoutes::RouteBuilder& KRESTRoutes::RouteBuilder::SetCallback(KHTTPMethod Method, KRESTRoute::RESTCallback Callback)
//-----------------------------------------------------------------------------
{
	if (m_Callback)
	{
		// callback is already set, finish the last route declararion here and start a new one
		AddRoute(true);
	}
	
	m_Verb     = Method;
	m_Callback = std::move(Callback);

	return *this;

} // SetCallback

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
KRESTRoutes::RouteBuilder KRESTRoutes::AddRoute(KString sRoute)
//-----------------------------------------------------------------------------
{
	return RouteBuilder(*this, std::move(sRoute));

} // AddRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::AddWebServer(KString sWWWDir, KString sRoute, bool bWithAdHocIndex, bool bAllowUpload)
//-----------------------------------------------------------------------------
{
	KJSON Config {{ "parser", "NOREAD" }};

	if (bWithAdHocIndex)
	{
		Config.push_back({"autoindex", true});
	}

	if (bAllowUpload)
	{
		Config.push_back({"upload", true});
	}

	kDebug(2, "route : {}\nwww   : {}\nconfig: {}", sRoute, sWWWDir, Config.dump());

	if (bAllowUpload)
	{
		m_Routes.push_back(KRESTRoute("GET"    , false, sRoute           , sWWWDir           , *this, &KRESTRoutes::WebServer, Config));
		m_Routes.push_back(KRESTRoute("PUT"    , false, sRoute           , sWWWDir           , *this, &KRESTRoutes::WebServer, Config));
		m_Routes.push_back(KRESTRoute("DELETE" , false, sRoute           , sWWWDir           , *this, &KRESTRoutes::WebServer, Config));
		m_Routes.push_back(KRESTRoute("POST"   , false, std::move(sRoute), std::move(sWWWDir), *this, &KRESTRoutes::WebServer, std::move(Config)));
	}
	else
	{
		m_Routes.push_back(KRESTRoute("GET"    , false, std::move(sRoute), std::move(sWWWDir), *this, &KRESTRoutes::WebServer, std::move(Config)));
	}

} // AddWebServer

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
		kDebug (3, "evaluating: {:<7} {}{}" , it.Method.Serialize(), it.sRoute, bIsWebSocket ? " (websocket)" : "");
		if (it.Matches(Path, &Params, true, true, bIsWebSocket))
		{
			kDebug (2, "     found: {:<7} {}{}", it.Method.Serialize(), it.sRoute, bIsWebSocket ? " (websocket)" : "");
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
	KWebServer WebServer;

	bool bHadTrailingSlash = HTTP.Request.Resource.Path.get().back() == '/';

	WebServer.Serve
	(
		HTTP.Route->sDocumentRoot,
		HTTP.RequestPath.sRoute,
		bHadTrailingSlash,
		kjson::GetBool(HTTP.Route->Config, "autoindex"),
		kjson::GetBool(HTTP.Route->Config, "upload"),
		HTTP.Route->sRoute,
		HTTP.RequestPath.Method,
		HTTP.Request,
		HTTP.Response,
		[this](KHTTPMethod Method, KStringView sPath)
		{
			return CheckForWrongMethod(KRESTPath(Method, sPath));
		}
	);

	if (!WebServer.IsValid())
	{
		// this should have been thrown already by KWebServer, probably
		// more precisely
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "bad request" };
	}

	HTTP.SetStatus(WebServer.GetStatus());

	switch (HTTP.RequestPath.Method)
	{
		case KHTTPMethod::HEAD:
			HTTP.SetContentLengthToOutput(WebServer.GetFileSize());
			break;

		case KHTTPMethod::GET:
		{
			if (WebServer.IsAdHocIndex())
			{
				HTTP.SetRawOutput(WebServer.GetAdHocIndex());
			}
			else
			{
				HTTP.SetStreamToOutput(WebServer.GetStreamForReading(), WebServer.GetFileSize());
			}
			break;
		}

		case KHTTPMethod::POST:
		{
			// check if we have a boundary string in the content-type header
			KStringView sBoundary = HTTP.Request.Headers.Get(KHTTPHeader::CONTENT_TYPE);

			auto pos = sBoundary.find(';');

			if (pos != KStringView::npos)
			{
				sBoundary.remove_prefix(pos + 1);
				sBoundary.Trim();

				if (sBoundary.remove_prefix("boundary="))
				{
					if (sBoundary.remove_prefix('"'))
					{
						sBoundary.remove_suffix('"');
					}
					else if (sBoundary.remove_prefix('\''))
					{
						sBoundary.remove_suffix('\'');
					}
				}
				else
				{
					sBoundary.clear();
				}
			}
			else
			{
				sBoundary.clear();
			}

			KMIME Mime = HTTP.Request.ContentType();

			if (!sBoundary.empty() || Mime.Serialize().starts_with("multipart/"))
			{
				KTempDir TempDir;

				// this is a multipart encoded request
				KMIMEReceiveMultiPartFormData Receiver(TempDir.Name(), sBoundary);

				if (!Receiver.ReadFromStream(HTTP.InStream()))
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, Receiver.Error() };
				}

				for (auto& File : Receiver.GetFiles())
				{
					// move the files from the temp location into the upload folder
					if (File.GetCompleted())
					{
						auto sFrom = kFormat("{}{}{}", TempDir.Name(), kDirSep, File.GetFilename());
						auto sTo   = kFormat("{}{}{}", HTTP.Route->sDocumentRoot, kDirSep, File.GetFilename());
						kMove(sFrom, sTo);
					}
					else
					{
						kDebug(2, "skipping incomplete upload file: {}", File.GetFilename());
					}
				}

				if (!bHadTrailingSlash)
				{
					// this is probably never used, but we keep it for completeness:
					// remove last path component (the filename)
					HTTP.RequestPath.sRoute           = kDirname(HTTP.RequestPath.sRoute);
					HTTP.Request.Resource.Path.get()  = HTTP.RequestPath.sRoute;
					HTTP.Request.Resource.Path.get() += '/';
				}
				// and show the new list of files:
				// change the POST into a GET request
				HTTP.RequestPath.Method = KHTTPMethod::GET;
				// and call us recursively
				this->WebServer(HTTP);
				// exit the switch here, the POST was successful
				break;
			}
		}
		// fallthrough into the PUT case, the POST was not a multipart encoding
		DEKAF2_FALLTHROUGH;

		case KHTTPMethod::PUT:
			if (bHadTrailingSlash == false)
			{
				// this is a plain PUT (or POST) of data, the file name is taken 
				// from the last part of the URL
				auto sName = kMakeSafeFilename(kBasename(HTTP.RequestPath.sRoute));

				if (sName.empty())
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing target name in URL" };
				}

				KTempDir TempDir;

				auto sFrom = kFormat("{}{}{}", TempDir.Name(), kDirSep, sName);
				auto sTo   = kFormat("{}{}{}", HTTP.Route->sDocumentRoot, kDirSep, sName);

				KOutFile OutFile(sFrom);

				if (!OutFile.is_open())
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "cannot open file" };
				}

				if (!OutFile.Write(HTTP.InStream()).Good())
				{
					throw KHTTPError { KHTTPError::H5xx_ERROR, "cannot write file" };
				}

				OutFile.close();

				kDebug(2, "received {} for file: {}", kFormBytes(kFileSize(sFrom)), sName);

				kMove(sFrom, sTo);

				HTTP.SetMessage(kFormat("received file: {}", HTTP.RequestPath.sRoute));
			}
			else
			{
				throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "missing target name in URL" };
			}
			// this is a REST request, do not return the index file but simply return
			break;

		case KHTTPMethod::DELETE:
		{
			auto sName = kFormat("{}{}{}", HTTP.Route->sDocumentRoot, kDirSep, HTTP.RequestPath.sRoute);

			kDebug(2, "deleting file: {}", HTTP.RequestPath.sRoute);

			if (!kRemove(sName, KFileTypes::ALL))
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, "cannot remove file" };
			}

			HTTP.SetMessage(kFormat("deleted file: {}", HTTP.RequestPath.sRoute));
			break;
		}

		default:
			throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "method not supported" };
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
