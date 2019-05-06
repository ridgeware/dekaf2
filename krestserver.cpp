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

#include "krestserver.h"
#include "khttperror.h"
#include "dekaf2.h"
#include "kfilesystem.h"
#include "kopenid.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KRESTPath::KRESTPath(KHTTPMethod _Method, KStringView _sRoute)
//-----------------------------------------------------------------------------
	: KHTTPPath(std::move(_sRoute))
	, Method(std::move(_Method))
{
} // KRESTPath

namespace detail {

//-----------------------------------------------------------------------------
KRESTAnalyzedPath::KRESTAnalyzedPath(KHTTPMethod _Method, KStringView _sRoute)
//-----------------------------------------------------------------------------
	: KHTTPAnalyzedPath(std::move(_sRoute))
	, Method(std::move(_Method))
{
	bHasParameters = sRoute.Contains("/:") || sRoute.Contains("/=");

} // KRESTAnalyzedPath

} // end of namespace detail
	
//-----------------------------------------------------------------------------
KRESTRoute::KRESTRoute(KHTTPMethod _Method, KStringView _sRoute, RESTCallback _Callback, ParserType _Parser)
//-----------------------------------------------------------------------------
	: detail::KRESTAnalyzedPath(std::move(_Method), std::move(_sRoute))
	, Callback(std::move(_Callback))
	, Parser(_Parser)
{
} // KRESTRoute

//-----------------------------------------------------------------------------
bool KRESTRoute::Matches(const KRESTPath& Path, Parameters& Params, bool bCompareMethods) const
//-----------------------------------------------------------------------------
{
	if (!bCompareMethods || Method.empty() || Method == Path.Method)
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
			if (vURLParts.size() >= Path.vURLParts.size())
			{
				Params.clear();
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
							Params.push_back({part, *req});
						}
						else if (DEKAF2_UNLIKELY(part.front() == '='))
						{
							// this is a variable
							KStringView sName = part;
							// remove the '='
							sName.remove_prefix(1);
							// and add the value to our temporary query parms
							Params.push_back({sName, *req});
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
KRESTRoutes::KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute)
//-----------------------------------------------------------------------------
	: m_DefaultRoute(KRESTRoute(KHTTPMethod{}, "/", DefaultRoute))
{
}

//-----------------------------------------------------------------------------
void KRESTRoutes::AddRoute(const KRESTRoute& _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(_Route);

} // AddRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::AddRoute(KRESTRoute&& _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(std::move(_Route));

} // AddRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::SetDefaultRoute(KRESTRoute::RESTCallback Callback, KRESTRoute::ParserType Parser)
//-----------------------------------------------------------------------------
{
	m_DefaultRoute.Callback = Callback;
	m_DefaultRoute.Parser = Parser;

} // SetDefaultRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::clear()
//-----------------------------------------------------------------------------
{
	m_Routes.clear();
	m_DefaultRoute.Callback = nullptr;

} // clear

//-----------------------------------------------------------------------------
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTPath& Path, Parameters& Params, bool bCheckForWrongMethod) const
//-----------------------------------------------------------------------------
{
	kDebug (2, "Looking up: {} {}" , Path.Method.Serialize(), Path.sRoute);

	// check for a matching route
	for (const auto& it : m_Routes)
	{
		if (it.Matches(Path, Params, true))
		{
			kDebug (2, "Found: {} {}" , it.Method.Serialize(), it.sRoute);
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
		// now check if we only missed a route because of a wrong request method
		for (const auto& it : m_Routes)
		{
			if (!it.Method.empty() && (it.Method != KHTTPMethod::OPTIONS))
			{
				if (it.Matches(Path, Params, false))
				{
					kDebug (2, "request method {} not supported for path: {}", Path.Method.Serialize(), Path.sRoute);
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("request method {} not supported for path: {}", Path.Method.Serialize(), Path.sRoute) };
				}
			}
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
void KRESTServer::Options::AddHeader(KStringView sHeader, KStringView sValue)
//-----------------------------------------------------------------------------
{
	ResponseHeaders.Add(sHeader, sValue);

} // AddHeader

//-----------------------------------------------------------------------------
void KRESTServer::VerifyAuthentication(const Options& Options)
//-----------------------------------------------------------------------------
{
	switch (Options.AuthLevel)
	{
		case Options::ALLOW_ALL:
			break;

		case Options::ALLOW_ALL_WITH_AUTH_HEADER:
			if (Request.Headers[KHTTPHeaders::AUTHORIZATION].empty())
			{
				throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "no authorization" };
			}
			break;

		case Options::VERIFY_AUTH_HEADER:
			{
				if (Options.Authenticators.empty())
				{
					kWarning("authenticator list is empty");
				}
				else
				{
					auto& Authorization = Request.Headers[KHTTPHeaders::AUTHORIZATION];
					if (!Authorization.empty())
					{
						if (m_AuthToken.Check(Authorization, Options.Authenticators, Options.sAuthScope))
						{
							// success
							return;
						}
					}
				}
				// failure
				throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "no authorization" };
			}
			break;
	}

} // VerifyAuthentication


//-----------------------------------------------------------------------------
void KRESTServer::VerifyPerThreadKLogToHeader(const Options& Options)
//-----------------------------------------------------------------------------
{
	auto it = Request.Headers.find(Options.sKLogHeader);

	if (it != Request.Headers.end())
	{
		bool bValid { false };

		std::vector<KStringView> parts;
		kSplit(parts, it->second, ", ");

		if (parts.size() > 0)
		{
			int iKLogLevel = parts[0].Int16();

			if (iKLogLevel > 0 && iKLogLevel < 4)
			{
				bValid = true;
			}

			bool bToKLog { false };
			bool bToJSON { false };

			if (bValid && parts.size() > 1)
			{
				auto sOpt = parts[1].ToLower();
				
				if (sOpt == "log")
				{
					bToKLog = true;
				}
				else if (sOpt == "json")
				{
					bToJSON = true;
				}
				else
				{
					bValid = false;
				}
			}

			if (bValid)
			{
				if (bToKLog)
				{
					KLog::getInstance().LogThisThreadToKLog(iKLogLevel);
					kDebug(2, "switching per-thread klog logging for this thread on at level {}", iKLogLevel);
				}
				else if (bToJSON)
				{
					json.tx["klog"] = KJSON::array();
					KLog::getInstance().LogThisThreadToJSON(iKLogLevel, &json.tx["klog"]);
					kDebug(2, "switching per-thread JSON logging for this thread on at level {}", iKLogLevel);
				}
				else
				{
					KLog::getInstance().LogThisThreadToResponseHeaders(iKLogLevel, Response, Options.sKLogHeader);
					kDebug(2, "switching per-thread response header logging for this thread on at level {}", iKLogLevel);
				}
			}
		}

		if (!bValid)
		{
			kDebug(2, "ignoring invalid klog header: {}: {}", it->first, it->second);
		}
	}

} // VerifyPerThreadKLogToHeader

//-----------------------------------------------------------------------------
bool KRESTServer::Execute(const Options& Options, const KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
{
	try
	{
		uint16_t iRound { 0 };

		for (;;)
		{
			kDebug (3, "keepalive round {}", iRound + 1);

			clear();

			// per default we output JSON
			Response.Headers.Add(KHTTPHeaders::CONTENT_TYPE, KMIME::JSON);

			// add additional response headers
			for (auto& it : Options.ResponseHeaders)
			{
				Response.Headers.Add(it.first, it.second);
			}

			bool bOK = KHTTPServer::Parse();

			if (iRound > 0 && !Options.sTimerHeader.empty())
			{
				// we can only start the timer after the input header
				// parsing completes, as otherwise we would also count
				// the keep-alive wait
				m_timer.clear();
			}

			if (!bOK)
			{
				if (KHTTPServer::Error().empty())
				{
					// close silently
					kDebug (3, "read timeout in keepalive round {}", iRound + 1);
					return false;
				}
				else
				{
					kDebug (1, "read error: {}", KHTTPServer::Error());
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, KHTTPServer::Error() };
				}
			}

			// OPTIONS method is allowed without Authorization header (it is used to request
			// for Authorization permission)
			if (Options.AuthLevel != Options::ALLOW_ALL
				&& Request.Method != KHTTPMethod::OPTIONS)
			{
				VerifyAuthentication(Options);
			}

			// switch logging only after authorization (but not for OPTIONS, as it is
			// not authenticated..)
			if (!Options.sKLogHeader.empty() && Request.Method != KHTTPMethod::OPTIONS)
			{
				VerifyPerThreadKLogToHeader(Options);
			}

			Response.SetStatus(200, "OK");
			Response.sHTTPVersion = "HTTP/1.1";

			kDebug (2, "incoming: {} {}", Request.Method.Serialize(), Request.Resource.Path);

			KStringView sURLPath = Request.Resource.Path;

			// try to remove_prefix, do not complain if not existing
			sURLPath.remove_prefix(Options.sBaseRoute);

			// find the right route
			auto Route = Routes.FindRoute(KRESTPath(Request.Method, sURLPath), Request.Resource.Query, Options.bCheckForWrongMethod);

			if (!Route.Callback)
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
			}

			if (Request.Method != KHTTPMethod::GET && Request.HasContent() && Route.Parser != KRESTRoute::NOREAD)
			{
				// read body and store for later access
				KHTTPServer::Read(m_sRequestBody);

				kDebug (3, "read request body with length {} and type {}",
						m_sRequestBody.size(),
						Request.Headers[KHTTPHeaders::CONTENT_TYPE]);

				if (Route.Parser == KRESTRoute::JSON)
				{
					// try to read input as JSON - if it fails just skip
					KStringView sBuffer { m_sRequestBody };
					sBuffer.TrimRight();
					if (!sBuffer.empty())
					{
						// parse the content into json.rx
						KString sError;

						// nobody wants stack traces in the klog when hackers throw crappy json (and attacks)
						// at their rest server.  so we need to turn off stack traces while we attempt to
						// parse incoming json from the wire:
						bool bResetFlag = KLog::getInstance().ShowStackOnJsonError (false);

						if (!kjson::Parse(json.rx, sBuffer, sError))
						{
							kDebug (3, "request body is not JSON: {}", sError);
							json.rx.clear();
							if (Options.bThrowIfInvalidJson)
							{
								throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat ("invalid JSON: {}", sError) };
							}
						}
						else
						{
							kDebug (3, "request body successfully parsed as JSON");
						}

						// after we are done parsing the incoming json from the wire,
						// restore stack traces for failures in the json that application may
						// form while processing a request:
						KLog::getInstance().ShowStackOnJsonError (bResetFlag);
					}
				}
				else if (Route.Parser == KRESTRoute::XML)
				{
					throw KHTTPError { KHTTPError::H5xx_NOTIMPL, "XML not yet supported" };
				}
			}

			// call the route handler
			Route.Callback(*this);

			// We offer a keep-alive if the client did not explicitly
			// request a close. We only allow for a limited amount
			// of keep-alive rounds, as this blocks one thread of
			// execution and could lead to a DoS if an attacker would
			// hold as many connections as we have simultaneous threads.
			bool bKeepAlive = (Options.Out == HTTP)
			               && (++iRound < Options.iMaxKeepaliveRounds)
			               && Request.HasKeepAlive();

			Output(Options, bKeepAlive);

			if (Options.bRecordRequest)
			{
				RecordRequestForReplay(Options);
			}

			if (!bKeepAlive)
			{
				if (Options.Out == HTTP)
				{
					kDebug (3, "no keep-alive allowed, or not supported by client - closing connection in round {}", iRound);
				}
				return true;
			}
		}

		return true;
	}

	catch (const std::exception& ex)
	{
		ErrorHandler(ex, Options);
	}

	if (Options.bRecordRequest)
	{
		RecordRequestForReplay(Options);
	}

	return false;

} // Execute

#ifdef NDEBUG
	static constexpr int iJSONPretty { -1 };
#else
	static constexpr int iJSONPretty { 1 };
#endif

//-----------------------------------------------------------------------------
void KRESTServer::Output(const Options& Options, bool bKeepAlive)
//-----------------------------------------------------------------------------
{
	// only allow output compression if this is HTTP mode
	ConfigureCompression(Options.Out == HTTP);

	kDebug (1, "HTTP-{}: {}", Response.iStatusCode, Response.sStatusString);

	if (!Options.sKLogHeader.empty())
	{
		// finally switch logging off if enabled
		KLog::getInstance().LogThisThreadToKLog(-1);
	}

	switch (Options.Out)
	{
		case HTTP:
		{
			KString sContent;

			if (DEKAF2_UNLIKELY(!m_sRawOutput.empty()))
			{
				// we output something else - do not set the content type, the
				// caller should have set it
				sContent = std::move(m_sRawOutput);
			}
			else
			{
				// the content:
				if (!m_sMessage.empty())
				{
					json.tx["message"] = std::move(m_sMessage);
				}

				if (!json.tx.empty())
				{
					sContent = json.tx.dump(iJSONPretty, '\t');

					// ensure that all JSON responses end in a newline:
					sContent += '\n';
				}
			}

			// compute and set the Content-Length header:
			Response.Headers.Set(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(sContent.length()));

			if (!Options.sTimerHeader.empty())
			{
				// add a custom header that marks execution time for this request
				Response.Headers.Add (Options.sTimerHeader, KString::to_string(m_timer.elapsed() / (1000 * 1000)));
			}

			Response.Headers.Set (KHTTPHeaders::CONNECTION, bKeepAlive ? "keep-alive" : "close");

			// writes response headers to output
			Serialize();

			// finally, output the content:
			kDebugLog (3, "{}", sContent);
			Write(sContent);
		}
		break;

		case LAMBDA:
		{
			KJSON tjson;
			tjson["statusCode"] = Response.iStatusCode;
			tjson["headers"] = KJSON::object();
			for (const auto& header : Response.Headers)
			{
				tjson["headers"] += { header.first, header.second };
			}

			tjson["isBase64Encoded"] = false;

			if (DEKAF2_UNLIKELY(!m_sRawOutput.empty()))
			{
				tjson["body"] = std::move(m_sRawOutput);
			}
			else
			{
				if (!m_sMessage.empty())
				{
					json.tx["message"] = std::move(m_sMessage);
				}

				tjson["body"] = std::move(json.tx);
			}
			Response.UnfilteredStream() << tjson.dump(iJSONPretty, '\t') << "\n";
		}
		break;

		case CLI:
		{
			if (DEKAF2_UNLIKELY(!m_sRawOutput.empty()))
			{
				Response.UnfilteredStream().Write(m_sRawOutput);
			}
			else
			{
				if (!m_sMessage.empty())
				{
					json.tx["message"] = std::move(m_sMessage);
				}

				if (!json.tx.empty())
				{
					Response.UnfilteredStream() << json.tx.dump(iJSONPretty, '\t');
				}
			}
			// finish with a linefeed
			Response.UnfilteredStream().WriteLine();
		}
		break;
	}

	if (!Response.UnfilteredStream().Good())
	{
		kDebug (2, "write error, connection lost");
	}

} // Output

//-----------------------------------------------------------------------------
void KRESTServer::json_t::clear()
//-----------------------------------------------------------------------------
{
	// a .clear() would not erase the json type information (which is IMHO a bug),
	// therefore we assign a fresh object
	rx = KJSON{};
	tx = KJSON{};

} // clear

//-----------------------------------------------------------------------------
void KRESTServer::xml_t::clear()
//-----------------------------------------------------------------------------
{
	rx.clear();
	tx.clear();

} // clear

//-----------------------------------------------------------------------------
void KRESTServer::ErrorHandler(const std::exception& ex, const Options& Options)
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

	// we need to set the HTTP version here explicitly, as we could throw as early
	// that no version is set - which will corrupt headers and body..
	Response.sHTTPVersion = "HTTP/1.1";

	KStringViewZ sError = ex.what();

	kDebug (1, "HTTP-{}: {}\n{}",  Response.iStatusCode, Response.sStatusString, sError);

	// do not compress/chunk error messages
	ConfigureCompression(false);

	KJSON EmptyJSON;

	if (!Options.sKLogHeader.empty())
	{
		// finally switch logging off if enabled
		KLog::getInstance().LogThisThreadToKLog(-1);

		auto it = json.tx.find("klog");
		if (it != json.tx.end())
		{
			// save the klog
			EmptyJSON["klog"] = std::move(*it);
		}
	}

	json.tx = std::move(EmptyJSON);

	switch (Options.Out)
	{
		case HTTP:
		{
			if (sError.empty())
			{
				json.tx["message"] = Response.sStatusString;
			}
			else
			{
				json.tx["message"] = sError;
			}

			KString sContent = json.tx.dump(iJSONPretty, '\t');

			// ensure that all JSON responses end in a newline:
			sContent += '\n';

			// compute and set the Content-Length header:
			Response.Headers.Set(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(sContent.length()));

			if (!Options.sTimerHeader.empty())
			{
				// add a custom header that marks execution time for this request
				Response.Headers.Add (Options.sTimerHeader, KString::to_string(m_timer.elapsed() / (1000 * 1000)));
			}

			Response.Headers.Set(KHTTPHeaders::CONNECTION, "close");

			// writes response headers to output
			Serialize();

			// finally, output the content:
			kDebugLog (3, "KREST: {}", sContent);
			Response.Write (sContent);
		}
		break;

		case LAMBDA:
		{
			json.tx["statusCode"] = Response.iStatusCode;
			json.tx["headers"] = KJSON::object();
			for (const auto& header : Response.Headers)
			{
				json.tx["headers"] += { header.first, header.second };
			}
			json.tx["isBase64Encoded"] = false;
			json.tx["body"] = KJSON::object();
			json.tx["body"] += { "message", sError };
			Response.UnfilteredStream() << json.tx.dump(iJSONPretty, '\t') << "\n";
		}
		break;

		case CLI:
		{
			Response.UnfilteredStream().FormatLine("{}: {}",
												   Dekaf::getInstance().GetProgName(),
												   sError.empty() ? Response.sStatusString.ToView()
												                  : sError);
		}
		break;
	}

	if (!Response.UnfilteredStream().Good())
	{
		kDebug (1, "write error, connection lost");
	}

} // ErrorHandler

//-----------------------------------------------------------------------------
void KRESTServer::SetStatus (int iCode)
//-----------------------------------------------------------------------------
{
	switch (iCode)
	{
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// HTTP 200s: success messages
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		case KHTTPError::H2xx_OK:           Response.SetStatus (200, "OK");                     break;
		case KHTTPError::H2xx_CREATED:      Response.SetStatus (201, "CREATED");                break;
		case KHTTPError::H2xx_UPDATED:      Response.SetStatus (201, "UPDATED");                break;
		case KHTTPError::H2xx_DELETED:      Response.SetStatus (201, "DELETED");                break;
		case KHTTPError::H2xx_ALREADY:      Response.SetStatus (200, "ALREADY DONE");           break; // DO NOT USE 204 STATUS (see below)

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// FALL THROUGH: blow up with a 500 error
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		default:                            Response.SetStatus (500, "INTERNAL SERVER ERROR");
			kWarning ("BUG: called with code {}", iCode);
			break;
	}

} // SetStatus

//-----------------------------------------------------------------------------
void KRESTServer::RecordRequestForReplay (const Options& Options)
//-----------------------------------------------------------------------------
{
	if (!Options.sRecordFile.empty())
	{
		bool bWriteHeader = !kFileExists(Options.sRecordFile);

		KOutFile RecordFile(Options.sRecordFile, std::ios::app);

		if (!RecordFile.is_open())
		{
			kDebugLog (3, "KREST: cannot open {}", Options.sRecordFile);
			return;
		}

		if (bWriteHeader)
		{
			RecordFile.WriteLine("#! /bin/bash");
			RecordFile.WriteLine();
		}

		// we can now write the request into the recording file
		RecordFile.WriteLine();
		RecordFile.FormatLine("# {} :: from IP {}", kFormTimestamp(), Request.GetBrowserIP());

		if (Response.GetStatusCode() > 299)
		{
			RecordFile.Format("#{}#", Response.GetStatusCode());
		}

		KURL URL { Request.Resource };
		URL.Protocol = url::KProtocol::HTTP;
		URL.Domain.set("localhost");
		URL.Port.clear();

		KString sAdditionalHeader;
		if (Request.Method != KHTTPMethod::GET)
		{
			KString sContentType = Request.Headers.Get(KHTTPHeaders::content_type);
			if (sContentType.empty())
			{
				sContentType = KMIME::JSON;
			}
			sAdditionalHeader.Format(" -H '{}: {}'", KHTTPHeaders::CONTENT_TYPE, sContentType);
		}

		RecordFile.Format("curl -i{} -X \"{}\" \"{}\"",
						  sAdditionalHeader,
						  Request.Method.Serialize(),
						  URL.Serialize());

		KString sPost { GetRequestBody() };

		if (!sPost.empty())
		{
			sPost.Collapse("\n\r", ' ');
			RecordFile.Write(" -d '");
			RecordFile.Write(sPost);
			RecordFile.Write('\'');
		}

		RecordFile.WriteLine();
	}

} // RecordRequestForReplay

//-----------------------------------------------------------------------------
void KRESTServer::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	json.clear();
	m_sRequestBody.clear();
	m_sMessage.clear();
	m_sRawOutput.clear();

} // clear

} // end of namespace dekaf2
