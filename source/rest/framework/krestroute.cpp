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

#include <dekaf2/rest/framework/krestroute.h>
#include <dekaf2/rest/framework/krestserver.h>
#include <dekaf2/http/server/khttperror.h>
#include <dekaf2/rest/serving/kwebserver.h>
#include <dekaf2/rest/serving/kcompressioncache.h>
#include <dekaf2/rest/serving/kwebdav.h>
#include <dekaf2/system/filesystem/kfilesystem.h>
#include <dekaf2/data/json/kjson.h>
#include <dekaf2/time/clock/ktime.h>
#include <dekaf2/time/duration/kduration.h>

#ifdef DEKAF2_IS_WINDOWS
// Windows has a DELETE macro in winnt.h which interferes with
// dekaf2::KHTTPMethod::DELETE (macros are evil!)
	#ifdef DELETE
		#undef DELETE
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

namespace {

// the defaults for the compression settings of a web server route
constexpr KDuration DefaultCompressionDeadline { chrono::seconds(3) };
constexpr uint64_t  DefaultCompressionMinSize  { 1024 }; // bytes

// the interval of the sweeps of a compression cache
constexpr KDuration CompressionSweepInterval { chrono::hours(6) };

//-----------------------------------------------------------------------------
uint64_t GetConfigValue(const KJSON& jConfig, KStringViewZ sKey, uint64_t iDefault)
//-----------------------------------------------------------------------------
{
	return jConfig.contains(sKey.c_str()) ? kjson::GetUInt(jConfig, sKey) : iDefault;

} // GetConfigValue

//-----------------------------------------------------------------------------
/// reads a duration from the configuration, which gives it in seconds
KDuration GetConfigDuration(const KJSON& jConfig, KStringViewZ sKey, KDuration Default)
//-----------------------------------------------------------------------------
{
	return jConfig.contains(sKey.c_str()) ? KDuration(chrono::seconds(kjson::GetUInt(jConfig, sKey))) : Default;

} // GetConfigDuration

//-----------------------------------------------------------------------------
/// returns the path of a file relative to the document root, with / as separator, or an
/// empty string if the file is not below the document root
KString GetRelativePath(KStringView sFileSystemPath, KStringView sDocumentRoot)
//-----------------------------------------------------------------------------
{
	if (!sFileSystemPath.remove_prefix(sDocumentRoot))
	{
		return {};
	}

	KString sRelPath = sFileSystemPath;

#ifdef DEKAF2_IS_WINDOWS
	// KFileServer joins the document root and the request path with the native separator
	sRelPath.Replace('\\', '/');
#endif

	sRelPath.remove_prefix('/');

	return sRelPath;

} // GetRelativePath

//-----------------------------------------------------------------------------
/// the cache entry for a request, or the compression for a new entry
struct CacheSelection
//-----------------------------------------------------------------------------
{
	KCompressionCache::Entry Entry;
	/// the compression of the entry - with State::Miss the compression for a new entry,
	/// NONE if the response could not be sent without a Content-Length
	KHTTPCompression::COMP   Compression { KHTTPCompression::NONE };
	KString                  sRelPath;
};

//-----------------------------------------------------------------------------
/// selects the cache entry for a request
/// @return false if the file is not compressed from the cache
bool SelectFromCache(KCompressionCache& Cache, KRESTServer& HTTP, KWebServer& WebServer, CacheSelection& Selection)
//-----------------------------------------------------------------------------
{
	const auto& jConfig = HTTP.Route->Config;
	const auto& Stat    = WebServer.GetFileStat();

	auto iMinSize = GetConfigValue(jConfig, "compression_min_size", DefaultCompressionMinSize);
	auto iMaxSize = GetConfigValue(jConfig, "compression_max_size", 0);

	if (!Stat.IsFile() || Stat.Size() < iMinSize || (iMaxSize && Stat.Size() > iMaxSize))
	{
		return false;
	}

	// GetMIMEType() returns a const reference, and IsCompressible() is not const
	KMIME MIME = WebServer.GetMIMEType(true);

	if (!MIME.IsCompressible())
	{
		return false;
	}

	auto Supported = KCompressionCache::GetSupportedCompressors();
	// with a Content-Length every accepted compression can be sent
	auto Accepted  = HTTP.Request.AcceptedCompressors(false) & Supported;
	// a new entry is streamed chunked after the deadline, which only a complete HTTP
	// response on a connection we own permits
	auto Streamable = (HTTP.GetOptions().Out == KRESTServer::HTTP || HTTP.GetOptions().Out == KRESTServer::NPH)
	                ? HTTP.Request.AcceptedCompressors(true) & Supported
	                : KHTTPCompression::COMP{};

	if (!Accepted)
	{
		return false;
	}

	Selection.sRelPath = GetRelativePath(WebServer.GetFileSystemPath(), HTTP.Route->sDocumentRoot);

	if (Selection.sRelPath.empty())
	{
		return false;
	}

	const auto& sDocumentRoot = HTTP.Route->sDocumentRoot;

	auto Target = KHTTPCompression::GetBestCompressor(Streamable);
	auto Best   = KHTTPCompression::GetBestCompressor(Accepted);

	if (Target != KHTTPCompression::NONE)
	{
		Selection.Entry       = Cache.Lookup(sDocumentRoot, Selection.sRelPath, Stat, Target);
		Selection.Compression = Target;
	}

	if (Selection.Entry.Status == KCompressionCache::State::Miss && Best != Target)
	{
		// an existing entry with a compression that must not be sent chunked (like zstd
		// for Safari below version 27), or any entry for a client without chunked
		// transfer - both are fine with a Content-Length
		auto Entry = Cache.Lookup(sDocumentRoot, Selection.sRelPath, Stat, Best);

		if (Entry.Status == KCompressionCache::State::Hit)
		{
			Selection.Entry       = std::move(Entry);
			Selection.Compression = Best;
		}
	}

	return Selection.Entry.Status != KCompressionCache::State::Failed;

} // SelectFromCache

//-----------------------------------------------------------------------------
/// sends a cache entry
/// @return false if the entry cannot be opened
bool SendCacheEntry(KRESTServer& HTTP, const KCompressionCache::Entry& Entry, KHTTPCompression::COMP Compression, bool bHeadersOnly)
//-----------------------------------------------------------------------------
{
	std::unique_ptr<KInFile> File;

	if (!bHeadersOnly)
	{
		File = std::make_unique<KInFile>(Entry.sPath);

		if (!File->is_open())
		{
			// a sweep or Forget() removed the entry after the lookup
			kDebug(2, "cannot open cache entry: {}", Entry.sPath);
			return false;
		}
	}

	HTTP.Response.Headers.Set(KHTTPHeader::CONTENT_ENCODING, KHTTPCompression::ToString(Compression));
	HTTP.Response.AddVary(KHTTPHeader::ACCEPT_ENCODING);
	// the entry is compressed already, the output filter must not compress it again
	HTTP.AllowCompression(false);

	if (bHeadersOnly)
	{
		HTTP.SetContentLengthToOutput(Entry.iSize);
	}
	else
	{
		HTTP.SetStreamToOutput(std::move(File), Entry.iSize, /*bAllowCompression=*/false);
	}

	return true;

} // SendCacheEntry

//-----------------------------------------------------------------------------
/// sends a file uncompressed, without compression on the fly
void SendUncompressed(KRESTServer& HTTP, KWebServer& WebServer, bool bHeadersOnly)
//-----------------------------------------------------------------------------
{
	if (bHeadersOnly)
	{
		HTTP.SetContentLengthToOutput(WebServer.GetFileSize());
	}
	else
	{
		HTTP.SetStreamToOutput(WebServer.GetStreamForReading(), WebServer.GetFileSize(), /*bAllowCompression=*/false);
	}

} // SendUncompressed

} // end of anonymous namespace

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
		 || Method == Path.Method
		 || (Method.empty() && (!Path.Method.IsWebDAV() || Option.Has(Options::WEBDAV)))
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
std::size_t KRESTRoutes::GetRouteIndex(const KRESTRoute& Route) const
//-----------------------------------------------------------------------------
{
	// std::less gives a total pointer order also for pointers into different
	// objects, where the builtin comparison would be unspecified
	std::less<const KRESTRoute*> Before;

	const auto* pRoute = &Route;
	const auto* pFirst = m_Routes.data();
	const auto* pLast  = pFirst + m_Routes.size();

	if (!Before(pRoute, pFirst) && Before(pRoute, pLast))
	{
		return static_cast<std::size_t>(pRoute - pFirst);
	}

	return npos;

} // GetRouteIndex

//-----------------------------------------------------------------------------
KRESTRoutes::RouteBuilder KRESTRoutes::AddRoute(KString sRoute)
//-----------------------------------------------------------------------------
{
	return RouteBuilder(*this, std::move(sRoute));

} // AddRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::AddWebServer(KString sWWWDir, KString sRoute, KWebServerPermissions Permissions, KJSON jConfig)
//-----------------------------------------------------------------------------
{
	m_WebServerPermissions = std::move(Permissions);

	// a permission set for /private has to cover /Private where the file system treats
	// both as the same directory - probe the document root, unless the config decides
	m_WebServerPermissions.SetCaseInsensitivePaths(jConfig.contains("case_insensitive")
	                                               ? kjson::GetBool(jConfig, "case_insensitive")
	                                               : kIsCaseInsensitiveFileSystem(sWWWDir));

	if (!jConfig.contains("parser"))
	{
		jConfig["parser"] = "NOREAD";
	}

	// mark the config so the WebServer callback knows to use permissions
	jConfig["use_permissions"] = true;

	kDebug(2, "route : {}\nwww   : {}\nconfig: {}", sRoute, sWWWDir, jConfig.dump());

	AddCompressionCache(jConfig, sWWWDir);

	// register a single catch-all route (empty method matches any) - the permission check happens at request time
	m_Routes.push_back(KRESTRoute(KHTTPMethod{KHTTPMethod::INVALID}, false, std::move(sRoute), std::move(sWWWDir), *this, &KRESTRoutes::WebServer, std::move(jConfig)));

} // AddWebServer

//-----------------------------------------------------------------------------
void KRESTRoutes::AddWebDAV(KString sWWWDir, KString sRoute, KWebServerPermissions Permissions, KJSON jConfig)
//-----------------------------------------------------------------------------
{
	m_WebServerPermissions = std::move(Permissions);

	// a permission set for /private has to cover /Private where the file system treats
	// both as the same directory - probe the document root, unless the config decides
	m_WebServerPermissions.SetCaseInsensitivePaths(jConfig.contains("case_insensitive")
	                                               ? kjson::GetBool(jConfig, "case_insensitive")
	                                               : kIsCaseInsensitiveFileSystem(sWWWDir));

	if (!jConfig.contains("parser"))
	{
		jConfig["parser"] = "NOREAD";
	}

	// mark the config so the WebServer callback knows to use permissions
	jConfig["use_permissions"] = true;

	kDebug(2, "WebDAV route : {}\nwww          : {}\nconfig       : {}", sRoute, sWWWDir, jConfig.dump());

	// GET and HEAD of a WebDAV route go through WebServer()
	AddCompressionCache(jConfig, sWWWDir);

	// register a single catch-all route (empty method matches any) - the permission check happens at request time
	m_Routes.push_back(KRESTRoute(KHTTPMethod{KHTTPMethod::INVALID}, KRESTRoute::Options{KRESTRoute::Options::WEBDAV}, std::move(sRoute), std::move(sWWWDir), *this, &KRESTRoutes::WebDAVHandler, std::move(jConfig)));

} // AddWebDAV

//-----------------------------------------------------------------------------
void KRESTRoutes::WebDAVHandler(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	switch (HTTP.RequestPath.Method)
	{
		case KHTTPMethod::PROPFIND:
		case KHTTPMethod::PROPPATCH:
		case KHTTPMethod::MKCOL:
		case KHTTPMethod::COPY:
		case KHTTPMethod::MOVE:
		case KHTTPMethod::DELETE:
		case KHTTPMethod::OPTIONS:
		case KHTTPMethod::LOCK:
		case KHTTPMethod::UNLOCK:
		{
			// resolve permissions
			KStringView sUser;

			if (kjson::GetBool(HTTP.Route->Config, "use_permissions"))
			{
				const auto& sPath = HTTP.Request.Resource.Path.get();
				const auto& sAuth = HTTP.GetAuthenticatedUser();

				if (!m_WebServerPermissions.IsAllowed(sAuth, HTTP.RequestPath.Method, sPath))
				{
					if (m_WebServerPermissions.HasUsers() && sAuth.empty())
					{
						throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "not authorized" };
					}
					throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, kFormat("method {} not permitted for path: {}", HTTP.RequestPath.Method, sPath) };
				}

				sUser = sAuth;
			}

			KWebDAV::Serve(HTTP,
			               HTTP.Route->sDocumentRoot,
			               HTTP.RequestPath.sRoute,
			               HTTP.Route->sRoute,
			               m_WebServerPermissions,
			               sUser,
			               GetCompressionCache(HTTP.Route->Config));
			break;
		}

		default:
			// delegate standard HTTP methods to the regular WebServer handler
			WebServer(HTTP);
			break;
	}

} // WebDAVHandler

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
		if (it.RegexFrom.Replace(sPath, it.sTo) > 0)
		{
			kDebug(1, "matched {}, changed to {}", it.RegexFrom.Pattern(), sPath);
			++iRewrites;
		}
		else
		{
			kDebug(3, "{} does not match {}", it.RegexFrom.Pattern(), sPath);
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

	kDebug (2, "looking up: {} {}{}" , Path.Method.Serialize(), Path.sRoute, bIsWebSocket ? " (websocket)" : "");

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
	if (GetDefaultRoute().Callback)
	{
		kDebug (2, "not found, returning default route");
		return GetDefaultRoute();
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
void KRESTRoutes::AddCompressionCache(const KJSON& jConfig, const KString& sDocumentRoot)
//-----------------------------------------------------------------------------
{
	const auto& sCacheDirectory = kjson::GetStringRef(jConfig, "compression_cache");

	if (sCacheDirectory.empty())
	{
		return;
	}

	auto* pCache = GetCompressionCache(jConfig);

	if (!pCache)
	{
		m_CompressionCaches.emplace_back(sCacheDirectory, std::make_shared<KCompressionCache>(sCacheDirectory));
		pCache = m_CompressionCaches.back().second.get();
	}

	// changes of files that bypass the server leave stale entries behind
	pCache->SweepRegularly(sDocumentRoot, CompressionSweepInterval);

} // AddCompressionCache

//-----------------------------------------------------------------------------
KCompressionCache* KRESTRoutes::GetCompressionCache(const KJSON& jConfig) const
//-----------------------------------------------------------------------------
{
	const auto& sCacheDirectory = kjson::GetStringRef(jConfig, "compression_cache");

	if (sCacheDirectory.empty())
	{
		return nullptr;
	}

	for (const auto& Cache : m_CompressionCaches)
	{
		if (Cache.first == sCacheDirectory)
		{
			return Cache.second.get();
		}
	}

	return nullptr;

} // GetCompressionCache

//-----------------------------------------------------------------------------
bool KRESTRoutes::ServeFromCompressionCache(KRESTServer& HTTP, KWebServer& WebServer, bool bHeadersOnly) const
//-----------------------------------------------------------------------------
{
	auto* pCache = GetCompressionCache(HTTP.Route->Config);

	if (!pCache || !HTTP.GetOptions().bAllowCompression)
	{
		return false;
	}

	// with a compression cache a static file is sent from the cache or uncompressed,
	// but never compressed on the fly
	CacheSelection Selection;

	if (!SelectFromCache(*pCache, HTTP, WebServer, Selection))
	{
		SendUncompressed(HTTP, WebServer, bHeadersOnly);
		return true;
	}

	if (Selection.Entry.Status == KCompressionCache::State::Miss &&
	    Selection.Compression  != KHTTPCompression::NONE &&
	    !bHeadersOnly)
	{
		auto Compression = Selection.Compression;
		auto Deadline    = GetConfigDuration(HTTP.Route->Config, "compression_deadline", DefaultCompressionDeadline);

		// compress the file into the cache - when this takes longer than the deadline, the
		// compressed data is sent at the same time, chunked
		Selection.Entry = pCache->Get(HTTP.Route->sDocumentRoot,
		                              Selection.sRelPath,
		                              WebServer.GetFileStat(),
		                              Compression,
		                              Deadline,
		                              [&HTTP, Compression]() -> KOutStream*
		{
			HTTP.Response.Headers.Set   (KHTTPHeader::CONTENT_ENCODING , KHTTPCompression::ToString(Compression));
			HTTP.Response.Headers.Set   (KHTTPHeader::TRANSFER_ENCODING, "chunked");
			HTTP.Response.Headers.Remove(KHTTPHeader::CONTENT_LENGTH);
			HTTP.Response.AddVary(KHTTPHeader::ACCEPT_ENCODING);
			// the data is compressed already, the output filter only adds the chunked framing
			HTTP.AllowCompression(false);
			HTTP.Stream(/*bAllowCompressionIfPossible=*/false);
			return &HTTP.OutStream();
		});
	}

	switch (Selection.Entry.Status)
	{
		case KCompressionCache::State::Hit:
			if (!SendCacheEntry(HTTP, Selection.Entry, Selection.Compression, bHeadersOnly))
			{
				SendUncompressed(HTTP, WebServer, bHeadersOnly);
			}
			return true;

		case KCompressionCache::State::Transmitted:
			// writes the end of the chunked transfer
			HTTP.Response.Flush();
			// the headers are sent already, the length is for the statistics and the log
			HTTP.SetContentLengthToOutput(Selection.Entry.iSize);
			return true;

		case KCompressionCache::State::Aborted:
			// the response is incomplete - closing the connection without the end of the
			// chunked transfer tells the client
			HTTP.Response.Headers.Set(KHTTPHeader::CONNECTION, "close");
			return true;

		case KCompressionCache::State::Miss:
			// a HEAD request, or a client without chunked transfer: no new entry
		case KCompressionCache::State::Negative:
		case KCompressionCache::State::Busy:
		case KCompressionCache::State::Failed:
			SendUncompressed(HTTP, WebServer, bHeadersOnly);
			return true;
	}

	return false;

} // ServeFromCompressionCache

//-----------------------------------------------------------------------------
bool KRESTRoutes::WouldBeCompressed(KRESTServer& HTTP, KWebServer& WebServer) const
//-----------------------------------------------------------------------------
{
	if (!HTTP.GetOptions().bAllowCompression || !WebServer.GetFileStat().IsFile())
	{
		return false;
	}

	if (auto* pCache = GetCompressionCache(HTTP.Route->Config))
	{
		CacheSelection Selection;

		if (!SelectFromCache(*pCache, HTTP, WebServer, Selection))
		{
			return false;
		}

		switch (Selection.Entry.Status)
		{
			case KCompressionCache::State::Hit:
				return true;

			case KCompressionCache::State::Miss:
				// a GET compresses the file and streams it
				return Selection.Compression != KHTTPCompression::NONE;

			case KCompressionCache::State::Negative:
			case KCompressionCache::State::Busy:
			case KCompressionCache::State::Failed:
			case KCompressionCache::State::Transmitted:
			case KCompressionCache::State::Aborted:
				return false;
		}

		return false;
	}

	// compression on the fly, see KHTTPServer::EnableCompressionIfPossible()
	KMIME MIME = WebServer.GetMIMEType(true);

	return MIME.IsCompressible() && !HTTP.Request.SupportedCompression().empty();

} // WouldBeCompressed

//-----------------------------------------------------------------------------
void KRESTRoutes::WebServer(KRESTServer& HTTP)
//-----------------------------------------------------------------------------
{
	// reject methods that are not supported by the web server early,
	// before any permission checks or file system access
	switch (HTTP.RequestPath.Method)
	{
		case KHTTPMethod::GET:
		case KHTTPMethod::HEAD:
		case KHTTPMethod::POST:
		case KHTTPMethod::PUT:
		case KHTTPMethod::DELETE:
		case KHTTPMethod::OPTIONS:
			break;

		default:
			throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("method {} not supported", HTTP.RequestPath.Method.Serialize()) };
	}

	kDebug(2, "config: {}", HTTP.Route->Config.dump());

	bool bWithAutoIndex;
	bool bWithUpload;

	if (kjson::GetBool(HTTP.Route->Config, "use_permissions"))
	{
		// resolve permissions for the authenticated user and the request path
		const auto& sPath = HTTP.Request.Resource.Path.get();
		const auto& sUser = HTTP.GetAuthenticatedUser();

		if (!m_WebServerPermissions.IsAllowed(sUser, HTTP.RequestPath.Method, sPath))
		{
			if (m_WebServerPermissions.HasUsers() && sUser.empty())
			{
				throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "not authorized" };
			}
			throw KHTTPError { KHTTPError::H4xx_FORBIDDEN, kFormat("method {} not permitted for path: {}", HTTP.RequestPath.Method, sPath) };
		}

		auto iPerms  = m_WebServerPermissions.Resolve(sUser, sPath);
		bWithAutoIndex = (iPerms & KWebServerPermissions::Browse) != 0;
		bWithUpload    = (iPerms & KWebServerPermissions::Write)  != 0;

		kDebug(2, "permissions for user '{}' on '{}': {}", sUser, sPath, KWebServerPermissions::SerializePermissions(iPerms));
	}
	else
	{
		// legacy mode: use the old boolean config flags
		// we have to use the KJSONv1 style here to support older systems
		// (but that bears no performance penalty)
		bWithAutoIndex = kjson::GetBool(HTTP.Route->Config, "autoindex");
		bWithUpload    = kjson::GetBool(HTTP.Route->Config, "upload"   );
	}

	kDebug(2, "auto index: {}", bWithAutoIndex);
	kDebug(2, "upload: {}", bWithUpload);

	KWebServer WebServer(HTTP.GetTempDirReference(), HTTP.Route->Config);

	// uploads and deletions remove the cache entries of the changed files
	WebServer.SetCompressionCache(GetCompressionCache(HTTP.Route->Config));

	if (bWithUpload && (HTTP.RequestPath.Method == KHTTPMethod::POST ||
	                    HTTP.RequestPath.Method == KHTTPMethod::PUT) )
	{
		WebServer.SetInputStream(HTTP.InStream());
	}

	bool bHadTrailingSlash = HTTP.Request.Resource.Path.get().back() == '/';

	KHTTPMethod ResultMethod;

	try
	{
		ResultMethod = WebServer.Serve
		(
			HTTP.Route->sDocumentRoot,
			HTTP.RequestPath.sRoute,
			bHadTrailingSlash,
			bWithAutoIndex,
			bWithUpload,
			HTTP.Route->sRoute,
			HTTP.RequestPath.Method,
			HTTP.Request,
			HTTP.Response,
			[this](KHTTPMethod Method, KStringView sPath)
			{
				return CheckForWrongMethod(KRESTPath(Method, sPath));
			}
		);
	}
	catch (const KHTTPError& ex)
	{
		// a 304 carries the Vary header of the 200 response it replaces (RFC 9110 15.4.5)
		if (ex.GetHTTPStatusCode() == KHTTPError::H304_NOT_MODIFIED && WouldBeCompressed(HTTP, WebServer))
		{
			HTTP.Response.AddVary(KHTTPHeader::ACCEPT_ENCODING);
		}

		throw;
	}

	if (!WebServer.IsValid())
	{
		// this should have been thrown already by KWebServer, probably
		// more precisely
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "bad request" };
	}

	HTTP.SetStatus(WebServer.GetStatus());

	switch (ResultMethod)
	{
		case KHTTPMethod::HEAD:
			if (WebServer.IsAdHocIndex() ||
			    WebServer.GetStatus() != KHTTPError::H2xx_OK ||
			    !ServeFromCompressionCache(HTTP, WebServer, /*bHeadersOnly=*/true))
			{
				HTTP.SetContentLengthToOutput(WebServer.GetFileSize());
			}
			break;

		case KHTTPMethod::GET:
		{
			if (WebServer.IsAdHocIndex())
			{
				HTTP.SetRawOutput(WebServer.GetAdHocIndex());
			}
			else if (WebServer.GetStatus() == KHTTPError::H2xx_OK &&
			         ServeFromCompressionCache(HTTP, WebServer, /*bHeadersOnly=*/false))
			{
				// sent from the compression cache, or uncompressed
			}
			else
			{
				// a partial response is not compressed: its Content-Range counts the
				// bytes of the uncompressed file
				HTTP.SetStreamToOutput(WebServer.GetStreamForReading(),
				                       WebServer.GetFileSize(),
				                       /*bAllowCompression=*/WebServer.GetStatus() != KHTTPError::H2xx_PARTIAL_CONTENT);
			}
			break;
		}

		case KHTTPMethod::POST:
			// we should actually not get this method back
			break;

		case KHTTPMethod::PUT:
			HTTP.SetMessage(kFormat("received file: {}", HTTP.RequestPath.sRoute));
			break;

		case KHTTPMethod::DELETE:
			HTTP.SetMessage(kFormat("deleted file: {}", HTTP.RequestPath.sRoute));
			break;

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
