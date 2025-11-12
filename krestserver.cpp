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
#include "krestroute.h"
#include "khttperror.h"
#include "dekaf2.h"
#include "kfilesystem.h"
#include "kopenid.h"
#include "kstringstream.h"
#include "kstringutils.h"
#include "kfrozen.h"
#include "kregex.h"
#include "kwebclient.h"
#include "kcrashexit.h"
#include "kwriter.h"
#include "kcountingstreambuf.h"
#include "krow.h"
#include "ktime.h"
#include "kwebsocket.h"
#include "kscopeguard.h"
#include <utility>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
void KRESTServer::Options::AddHeader(KHTTPHeader Header, KString sValue)
//-----------------------------------------------------------------------------
{
	ResponseHeaders.Add(std::move(Header), std::move(sValue));

} // AddHeader

//-----------------------------------------------------------------------------
KRESTServer::KRESTServer(const KRESTRoutes& Routes,
						 const Options&     options)
//-----------------------------------------------------------------------------
: m_Routes(Routes)
, m_Options(options)
{
}

//-----------------------------------------------------------------------------
KRESTServer::KRESTServer(KStream&           Stream,
						 KStringView        sRemoteEndpoint,
						 url::KProtocol     Proto,
						 uint16_t           iPort,
						 const KRESTRoutes& Routes,
						 const Options&     Options)
//-----------------------------------------------------------------------------
: KHTTPServer(Stream, sRemoteEndpoint, std::move(Proto), iPort)
, m_Routes(Routes)
, m_Options(Options)
{
}

//-----------------------------------------------------------------------------
void KRESTServer::SetDisconnected()
//-----------------------------------------------------------------------------
{
	kDebug(1, "remote end disconnected");
	m_bIsDisconnected = true;
}

//-----------------------------------------------------------------------------
bool KRESTServer::IsDisconnected()
//-----------------------------------------------------------------------------
{
	return m_bIsDisconnected;
}

//-----------------------------------------------------------------------------
void KRESTServer::ThrowIfDisconnected()
//-----------------------------------------------------------------------------
{
	if (IsDisconnected())
	{
		kDebug(2, "throwing");
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "remote end disconnected" };
	}
}

//-----------------------------------------------------------------------------
const KString& KRESTServer::GetQueryParmSafe(KStringView sKey) const
//-----------------------------------------------------------------------------
{
	const auto& sValue = GetQueryParm(sKey);

	if (KROW::NeedsEscape(sValue))
	{
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("{}: invalid input string, may not contain any of: {}", sKey, KROW::EscapedCharacters()) };
	}

	return sValue;

} // GetQueryParmSafe

//-----------------------------------------------------------------------------
KString KRESTServer::GetQueryParmSafe(KStringView sKey, KStringView sDefault) const
//-----------------------------------------------------------------------------
{
	KString sValue = GetQueryParmSafe(sKey);

	if (sValue.empty())
	{
		sValue = sDefault;
	}

	return sValue;

} // GetQueryParmSafe

//-----------------------------------------------------------------------------
const KString& KRESTServer::GetRequestBody() const
//-----------------------------------------------------------------------------
{
	return m_sRequestBody;
}

//-----------------------------------------------------------------------------
void KRESTServer::VerifyAuthentication()
//-----------------------------------------------------------------------------
{
	switch (m_Options.AuthLevel)
	{
		case Options::ALLOW_ALL:
			kDebug(2, "ALLOW_ALL");
			SetAuthenticatedUser("anonymous");
			return;

		case Options::ALLOW_ALL_WITH_AUTH_HEADER:
			kDebug(2, "ALLOW_ALL_WITH_AUTH_HEADER");
			if (!Request.Headers.Get(KHTTPHeader::AUTHORIZATION).empty())
			{
				SetAuthenticatedUser("pseudo-auth");
				return;
			}
			else
			{
				SetError(kFormat("empty {} header", KHTTPHeader::AUTHORIZATION));
			}
			break;

		case Options::VERIFY_AUTH_HEADER:
			{
				kDebug(2, "VERIFY_AUTH_HEADER");
				auto& Authorization = Request.Headers.Get(KHTTPHeader::AUTHORIZATION);

				if (!Authorization.empty())
				{
					if (m_Options.Authenticators.empty())
					{
						SetError("SSO: authenticator list is empty");
					}
					else
					{
						KStringView sScope;

						if (!this->Route->Option.Has(KRESTRoute::Options::NO_SSO_SCOPE))
						{
							// set the general SSO scope when NO_SSO_SCOPE is unset
							sScope = m_Options.sAuthScope;
						}

						if (m_AuthToken.Check(Authorization, m_Options.Authenticators, sScope))
						{
							// success
							SetAuthenticatedUser(kjson::GetString(GetAuthToken(), "sub"));
							return;
						}
						else
						{
							SetError(kFormat("SSO: {}", m_AuthToken.Error()));
						}
					}
				}
				else
				{
					SetError(kFormat("SSO: empty {} header", KHTTPHeader::AUTHORIZATION));
				}
			}
			break;
	}

	if (m_Options.FailedAuthCallback)
	{
		m_Options.FailedAuthCallback(*this);
	}

	throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization" };

} // VerifyAuthentication


//-----------------------------------------------------------------------------
int KRESTServer::VerifyPerThreadKLogToHeader()
//-----------------------------------------------------------------------------
{
	int  iKLogLevel { 0 };

#ifdef DEKAF2_WITH_KLOG

#ifdef DEKAF2_KLOG_WITH_TCP
	static constexpr KStringView s_sHeaderLoggingHelp {
		"supported header logging commands:\n"
		" -level <n> (or only <n>) :: set logging level (1..4)\n"
		" -E, -egrep <expression>  :: match regular expression to log\n"
		" -grep <substring>        :: match substring to log\n"
		" -out <headers|log|json>  :: output target for this thread, default = headers\n"
	};
#endif

	auto it = Request.Headers.find(m_Options.KLogHeader);

	if (it != Request.Headers.end())
	{
		enum PARTYPE { NONE, START, LEVEL, OUTPUT, GREP, EGREP, HELP };

#ifdef DEKAF2_HAS_FROZEN
		static constexpr auto s_Option = frozen::make_unordered_map<KStringView, PARTYPE>(
#else
		static const std::unordered_map<KStringView, PARTYPE> s_Option
#endif
		{
			{ "-level", LEVEL  },
			{ "-out"  , OUTPUT },
			{ "-E"    , EGREP  },
			{ "-egrep", EGREP  },
			{ "-grep" , GREP   },
			{ "-h"    , HELP   },
			{ "-help" , HELP   },
			{ "--help", HELP   }
		}
#ifdef DEKAF2_HAS_FROZEN
		)
#endif
		; // do not erase..

		PARTYPE iPType { START };

		bool bToKLog    { false };
		bool bToJSON    { false };
		bool bEGrep     { false };
#ifdef DEKAF2_KLOG_WITH_TCP
		bool bHelp      { false };
#endif
		KString sGrep;

		auto sParts = it->second.Split(", ");

		bool bValid { !sParts.empty() };

		for (auto sArg : sParts)
		{
			if (iPType == START && kIsInteger(sArg))
			{
				iPType = LEVEL;
			}

			switch (iPType)
			{
				case HELP:
				case START:
				case NONE:
				{
					auto itArg = s_Option.find(sArg);
					if (itArg == s_Option.end())
					{
						if (sGrep.empty())
						{
							// this is the naked grep value
							bEGrep = false;
							sGrep = sArg.ToLower();
						}
						else
						{
							bValid = false;
						}

					}
#ifdef DEKAF2_KLOG_WITH_TCP
					else if (itArg->second == HELP)
					{
						bHelp = true;
						if (!iKLogLevel)
						{
							iKLogLevel = 1;
						}
					}
#endif
					else
					{
						iPType = itArg->second;
					}
					break;
				}

				case LEVEL:
					iKLogLevel = sArg.UInt16();

					if (iKLogLevel <= 0 || iKLogLevel > 4)
					{
						bValid = false;
					}
					iPType = NONE;
					break;

				case OUTPUT:
				{
					auto sOpt = sArg.ToLower();

					if (sOpt == "log")
					{
						bToKLog = true;
					}
					else if (sOpt == "json")
					{
						bToJSON = true;
					}
					else if (sOpt == "headers")
					{
						bToKLog = false;
						bToJSON = false;
					}
					else
					{
						bValid = false;
					}
					iPType = NONE;
					break;
				}

				case EGREP:
					bEGrep = true;

					if (sGrep.empty())
					{
						sGrep = sArg.ToLower();
						// now test the grep for valid syntax!
						KRegex RE(sGrep);
						if (!RE.Good())
						{
							// invalid regex, drop it!
							sGrep.clear();
							kDebug(1, "invalid regex: {}", sArg);
						}
					}
					else
					{
						bValid = false;
					}
					iPType = NONE;
					break;

				case GREP:
					bEGrep = false;

					if (sGrep.empty())
					{
						sGrep = sArg.ToLower();
					}
					else
					{
						bValid = false;
					}
					iPType = NONE;
					break;
			}

			if (!bValid)
			{
				break;
			}
		}

		if (!bValid)
		{
			kDebug(2, "ignoring invalid klog header: {}: {}", it->first, it->second);
			return 0;
		}

		if (bToKLog)
		{
			KLog::getInstance().LogThisThreadToKLog(iKLogLevel);
			kDebug(3, "per-thread {} logging, level {}", "klog", iKLogLevel);
		}
		else if (bToJSON)
		{
#ifdef DEKAF2_KLOG_WITH_TCP
			m_JsonLogger = std::make_unique<KJSON>(KJSON::array());
			KLog::getInstance().LogThisThreadToJSON(iKLogLevel, m_JsonLogger.get());
			kDebug(3, "per-thread {} logging, level {}", "JSON", iKLogLevel);
#else
			kDebug(2, "request to switch {} logging on, but compiled without support", "json response");
			return 0;
#endif
		}
		else
		{
#ifdef DEKAF2_KLOG_WITH_TCP
			KLog::getInstance().LogThisThreadToResponseHeaders(iKLogLevel, Response, m_Options.KLogHeader.Serialize());
			kDebug(3, "per-thread {} logging, level {}", "response header", iKLogLevel);
			if (bHelp)
			{
				kDebugLog(1, s_sHeaderLoggingHelp);
			}
#else
			kDebug(2, "request to switch {} logging on, but compiled without support", "response header");
			return 0;
#endif
		}

		if (sGrep)
		{
			KLog::getInstance().LogThisThreadWithGrepExpression(bEGrep, sGrep);
		}
	}

#endif // of DEKAF2_WITH_KLOG

	return iKLogLevel;

} // VerifyPerThreadKLogToHeader

//-----------------------------------------------------------------------------
void KRESTServer::Parse()
//-----------------------------------------------------------------------------
{
	kAppendCrashContext("content parsing", ": ");

	switch (Route->Parser)
	{
		case KRESTRoute::NOREAD:
			break;

		case KRESTRoute::JSON:
		{
			// try to read input as JSON - if it fails just skip

#ifndef DEKAF2_WRAPPED_KJSON
			// nobody wants stack traces in the klog when hackers throw crappy json (and attacks)
			// at their rest server.  so we need to turn off stack traces while we attempt to
			// parse incoming json from the wire:
			bool bResetFlag = KLog::getInstance().ShowStackOnJsonError (false);
#endif
			// parse the content into json.rx
			KString sError;

			kDebug (2, "parsing JSON request");
			if (!kjson::Parse(json.rx, KHTTPServer::InStream(), sError))
			{
				kDebug (2, "request body is not JSON: {}", sError);
				json.rx.clear();
				if (m_Options.bThrowIfInvalidJson)
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat ("invalid JSON: {}", sError) };
				}
			}
			else
			{
				kDebug (2, "request body successfully parsed as JSON");

				if (m_Options.bRecordRequest)
				{
					// dump the json to record it
					m_sRequestBody = json.rx.dump(-1);
				}
			}

#ifndef DEKAF2_WRAPPED_KJSON
			// after we are done parsing the incoming json from the wire,
			// restore stack traces for failures in the json that application may
			// form while processing a request:
			KLog::getInstance().ShowStackOnJsonError (bResetFlag);
#endif
		}
		break;

		case KRESTRoute::XML:
			// read input as XML
			kDebug (2, "parsing XML request");
			if (!xml.rx.Parse(KHTTPServer::InStream(), true))
			{
				kDebug (2, "request body is not XML");
				xml.rx.clear();
				if (m_Options.bThrowIfInvalidJson)
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "invalid XML" };
				}
			}
			else
			{
				kDebug (2, "request body successfully parsed as XML");

				if (m_Options.bRecordRequest)
				{
					// dump the XML to record it
					m_sRequestBody = xml.rx.Serialize(KXML::PrintFlags::Terse);
				}
			}
			break;

		case KRESTRoute::PLAIN:
			// read body and store for later access
			kDebug (2, "reading {} request body", "plain");
			m_sRequestBody = KHTTPServer::Read();
			kDebug (2, "read {} request body with length {} and type {}",
					"plain",
					m_sRequestBody.size(),
					Request.Headers[KHTTPHeader::CONTENT_TYPE]);
			break;

		case KRESTRoute::WWWFORM:
		{
			// read input as urlencoded www form data and append it to the
			// received query parms in the URL
			kDebug (2, "reading {} request body", "www form");
			m_sRequestBody = KHTTPServer::Read();
			kDebug (2, "read {} request body with length {} and type {}",
					"www form",
					m_sRequestBody.size(),
					Request.Headers[KHTTPHeader::CONTENT_TYPE]);
			m_sRequestBody.Trim();
			// operator+=() causes additive parsing for a query component
			Request.Resource.Query += m_sRequestBody;
		}
		break;

	}

} // Parse

//-----------------------------------------------------------------------------
bool KRESTServer::Execute()
//-----------------------------------------------------------------------------
{
	if (m_iRound != std::numeric_limits<uint16_t>::max())
	{
		kDebug(1, "recursive calling is not permitted");
		return false;
	}

	// reset m_iRound at end of scope
	KAtScopeEnd( m_iRound = std::numeric_limits<uint16_t>::max() );

	try
	{
		m_iRound = 0;

		for (;;)
		{
			kDebug (2, "keepalive round {}", m_iRound + 1);
			kSetCrashContext(kFormat("KRestServer, Host: {} Remote IP: {}",
									 m_Options.sServername,
									 Request.GetRemoteIP()));
			clear();

			// per default we output JSON
			Response.Headers.Add(KHTTPHeader::CONTENT_TYPE, KMIME::JSON);

			// add additional response headers
			for (auto& it : m_Options.ResponseHeaders)
			{
				Response.Headers.Add(it.first, it.second);
			}

			bool bOK { false };

			{
				KCountingInputStreamBuf InputCounter(Request.UnfilteredStream());

				bOK = KHTTPServer::Parse();

				// store rx header length
				m_iRequestHeaderLength = InputCounter.Count();
			}

			if (m_iRound == 0)
			{
				// check if we have to start the timers
				if (!m_Options.TimerHeader.empty() || m_Options.TimingCallback)
				{
					m_Timers = std::make_unique<KStopDurations>();
					m_Timers->reserve(Timer::SEND + 1);
				}
			}
			else if (m_Timers)
			{
				// we can only start the timer after the input header
				// parsing completes, as otherwise we would also count
				// the keep-alive wait
				m_Timers->clear();
			}

			if (!bOK)
			{
				if (Error().empty())
				{
					if (m_iRequestHeaderLength > 0)
					{
						kDebug(2, "connection closed during request, reveived {} bytes of header", m_iRequestHeaderLength);
					}
					// close silently
					kDebug (3, "read timeout in keepalive round {}", m_iRound + 1);
					return false;
				}
				else
				{
					kDebug (1, "read error: {}", GetLastError());
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, GetLastError() };
				}
			}

			// set crash context before rewriting any resources, to make sure we get the
			// original input in case of a segfault
			kSetCrashContext (kFormat ("{}: {}\nHost: {} Remote IP: {}",
									   Request.Method.Serialize(),
									   Request.Resource.Serialize(),
									   m_Options.sServername,
									   Request.GetRemoteIP())
							  );

			if (Request.Method == KHTTPMethod::INVALID)
			{
				kDebug (2, "invalid request method: {}", Request.RequestLine.GetMethod());
				throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("invalid request method: {}", Request.RequestLine.GetMethod()) };
			}

			if (!m_Options.bServiceIsReady)
			{
				throw KHTTPError { KHTTPError::H5xx_UNAVAILABLE, "service unavailable" };
			}

			Response.SetStatus(200, "OK");

			if ((Request.GetHTTPVersion() & ~KHTTPVersion::http10) == KHTTPVersion::none)
			{
				// this was either http/1.0 or no version set -
				// force http/1.1 output
				Response.SetHTTPVersion(KHTTPVersion::http11);
			}
			else
			{
				Response.SetHTTPVersion(Request.GetHTTPVersion());
			}

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::RECEIVE);
			}

			kDebug (1, "{:=^50}", " new request ");
			kDebug (2, "incoming: {} {}", Request.Method.Serialize(), Request.Resource.Path);

			if (m_Options.PreRouteCallback)
			{
				m_Options.PreRouteCallback(*this);
			}

			KString sURLPath = Request.Resource.Path.get();

			// try to remove_prefix, do not complain if not existing
			sURLPath.remove_prefix(m_Options.sBaseRoute);

			// check if we have rewrite rules for the request path
			if (m_Routes.RewritePath(sURLPath) > 0)
			{
				kAppendCrashContext(kFormat("path {} to '{}'", "rewritten", sURLPath));
				kDebug(2, "path {} to '{}'", "rewritten", sURLPath);
			}

			// check if we have redirect rules for the request path
			if (m_Routes.RedirectPath(sURLPath) > 0)
			{
				kAppendCrashContext(kFormat("path {} to '{}'", "redirected", sURLPath));
				kDebug(2, "path {} to '{}'", "redirected", sURLPath);

				Response.Headers.Remove(KHTTPHeader::CONTENT_TYPE);
				Response.Headers.Set(KHTTPHeader::LOCATION, sURLPath);

				if (Request.Method == KHTTPMethod::GET || Request.Method == KHTTPMethod::HEAD)
				{
					throw KHTTPError { KHTTPError::H301_MOVED_PERMANENTLY, "MOVED PERMANENTLY" };
				}
				else
				{
					throw KHTTPError { KHTTPError::H308_PERMANENT_REDIRECT, "PERMANENT REDIRECT" };
				}
			}

			// try to remove a trailing / - we treat /path and /path/ as the same address
			if (sURLPath.back() == '/')
			{
				sURLPath.remove_suffix(1);
			}

			RequestPath = KRESTPath(Request.Method, sURLPath);

			m_bSwitchToWebSocket = KWebSocket::CheckForUpgradeRequest(Request, true);

			// find the right route
			Route = &m_Routes.FindRoute(RequestPath, Request.Resource.Query, m_bSwitchToWebSocket, m_Options.bCheckForWrongMethod);

			if (!Route->Callback)
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
			}

			// OPTIONS method is allowed without Authorization header (it is used to request
			// for Authorization permission)
			if (Request.Method != KHTTPMethod::OPTIONS)
			{
				if (Route->Option(KRESTRoute::Options::GENERIC_AUTH))
				{
					if (m_Options.AuthCallback)
					{
						SetAuthenticatedUser(m_Options.AuthCallback(*this));
					}
					else
					{
						kDebug(2, "no auth callback for: {}", Route->sRoute);
					}
				}

				if (Route->Option(KRESTRoute::Options::SSO_AUTH))
				{
					// check if this route permits other authentication methods (probably triggered
					// in AuthCallback()) and has confirmed a valid user
					if (!Route->Option(KRESTRoute::Options::GENERIC_AUTH) || GetAuthenticatedUser().empty())
					{
						// no - use the loaded authenticators
						VerifyAuthentication();
					}
				}

				if (Route->Option(KRESTRoute::Options::GENERIC_AUTH)
					&& GetAuthenticatedUser().empty())
				{
					// generic auth was requested, but neither SSO nor any other authentication method
					// resulted in a username
					if (m_Options.FailedAuthCallback)
					{
						m_Options.FailedAuthCallback(*this);
					}

					throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization" };
				}
			}

			if (m_Options.PostRouteCallback)
			{
				m_Options.PostRouteCallback(*this);
			}

#ifdef DEKAF2_WITH_KLOG
			// switch header logging only after authorization (but not for OPTIONS, as it is
			// not authenticated..)
			if (!m_Options.KLogHeader.empty() && Request.Method != KHTTPMethod::OPTIONS)
			{
				if (VerifyPerThreadKLogToHeader() > 1)
				{
					kDebug (2, "Request: {} {} {}", Request.Method.Serialize(), Request.Resource.Path, Request.GetHTTPVersion());
					// output headers for this thread
					for (const auto& Header : Request.Headers)
					{
						kDebug(2, "Header: {}: {}", Header.first, Header.second);
					}
				}
			}
#endif

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::ROUTE);
			}
/*
			if (Request.Headers.Get(KHTTPHeader::EXPECT) == "100-continue")
			{
			}
*/
			if (Request.HasContent(Request.Method == KHTTPMethod::GET))
			{
				Parse();
			}

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::PARSE);
			}

			// we store this count() twice, because some routes may read the
			// request themselves, but we want to assure we have a value should
			// an exception be thrown
			m_iRequestBodyLength = Request.Count();

			if (m_bSwitchToWebSocket)
			{
				if (m_Options.Out != HTTP)
				{
					throw KWebSocketError { "bad mode: websockets are only allowed in HTTP standalone mode" };
				}

				// upgrade to a websocket connection - compute and set the needed response headers
				// we will still call the route handler, but in general it should only set up
				// the websocket callback, and not add additional output
				const auto& sClientSecKey = Request.Headers.Get(KHTTPHeader::SEC_WEBSOCKET_KEY);
				Response.Headers.Add(KHTTPHeader::SEC_WEBSOCKET_ACCEPT, KWebSocket::GenerateServerSecKeyResponse(sClientSecKey, true));
				Response.Headers.Add(KHTTPHeader::UPGRADE, "websocket");
				Response.SetStatus(KHTTPError::H1xx_SWITCHING_PROTOCOLS);
			}

			// We offer a keep-alive if the client did not explicitly
			// request a close. We only allow for a limited amount
			// of keep-alive rounds, as this blocks one thread of
			// execution and could lead to a DoS if an attacker would
			// hold as many connections as we have simultaneous threads.
			m_bKeepAlive = !m_bSwitchToWebSocket
			            && (m_Options.Out == HTTP)
			            && (m_iRound+1 < m_Options.iMaxKeepaliveRounds)
			            && Request.HasKeepAlive();

			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// debug info
			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			kDebug (1, "{}: {}", GetRequestMethod(), GetRequestPath());

			// check that we are still connected to the remote end
			ThrowIfDisconnected();

			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// call the application method to handle this request:
			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			try
			{
				Route->Callback(*this);

				kAppendCrashContext("completed route handler");
			}
			catch (const KHTTPError& ex)
			{
				kAppendCrashContext(kFormat("completed route handler with exception HTTP-{}: {}", ex.GetHTTPStatusCode(), ex.GetHTTPStatusString()));

				if (ex.GetHTTPStatusCode() < 400)
				{
					// status codes < 400 do not mandate a connection abort,
					// and are well handled in the normal Output()
					Response.SetStatus(ex.GetHTTPStatusCode(), ex.GetHTTPStatusString());
				}
				else
				{
					// everything else breaks the keepalive loop
					throw;
				}
			}

			m_iRequestBodyLength = Request.Count();

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::PROCESS);
			}

			Output();

			RunPostResponse();

			if (m_Timers)
			{
				// add stats for this request
				auto Stats = Route->Statistics.unique();
				Stats->Durations += *m_Timers;
				Stats->iTxBytes  += m_iContentLength;
				Stats->iRxBytes  += m_iRequestBodyLength;
			}

			if (!m_bKeepAlive)
			{
				if (!m_bSwitchToWebSocket && kWouldLog(2))
				{
					if (m_Options.Out == HTTP)
					{
						if (!Request.HasKeepAlive())
						{
							kDebug (2, "keep-alive not requested by client - closing connection in round {}", m_iRound);
						}
						else
						{
							kDebug (2, "no further keep-alive allowed - closing connection in round {}", m_iRound);
						}
					}
				}
				return true;
			}

			// increase keepalive round explicitly here, not before..
			++m_iRound;
		}

		return true;
	}

	catch (const std::exception& ex)
	{
		ErrorHandler(ex);
	}

	RunPostResponse();

	return false;

} // Execute

//-----------------------------------------------------------------------------
void KRESTServer::RunPostResponse()
//-----------------------------------------------------------------------------
{
	if (m_PostResponseCallback)
	{
		m_PostResponseCallback(*this);
		m_PostResponseCallback = nullptr;
	}

	m_Options.Logger.Log(*this);

	if (m_Options.bRecordRequest)
	{
		RecordRequestForReplay();
	}

} // RunPostResponseCallback

//-----------------------------------------------------------------------------
bool KRESTServer::SetStreamToOutput(std::unique_ptr<KInStream> Stream, std::size_t iContentLength)
//-----------------------------------------------------------------------------
{
	if (!Stream || !Stream->Good())
	{
		kDebug(1, "stream is not good for reading");
		return false;
	}

	m_Stream = std::move(Stream);
	m_iContentLength = iContentLength;

	return true;

} // SetStreamToOutput

//-----------------------------------------------------------------------------
bool KRESTServer::SetFileToOutput(KStringViewZ sFile)
//-----------------------------------------------------------------------------
{
	auto iContentLength = kFileSize(sFile);

	if (iContentLength == npos)
	{
		kDebug(1, "file does not exist: {}", sFile);
		return false;
	}

	kDebug(2, "open file: {}", sFile);
	return SetStreamToOutput(std::make_unique<KInFile>(sFile), iContentLength);

} // SetFileToOutput

//-----------------------------------------------------------------------------
void KRESTServer::WriteHeaders()
//-----------------------------------------------------------------------------
{
	if (!m_Options.KLogHeader.empty())
	{
		// finally switch logging off if enabled
		KLog::getInstance().LogThisThreadToKLog(-1);
	}

	if (m_iContentLength != npos)
	{
		Response.Headers.Set(KHTTPHeader::CONTENT_LENGTH, KString::to_string(m_iContentLength));
	}

	if (m_Timers)
	{
		m_Timers->StoreInterval(Timer::SERIALIZE);

		if (!m_Options.TimerHeader.empty())
		{
			// add a custom header that marks execution time for this request
			Response.Headers.Set (m_Options.TimerHeader,
								  KString::to_string(m_Options.bMicrosecondTimerHeader
													 ? m_Timers->duration().microseconds().count()
													 : m_Timers->duration().milliseconds().count()));
		}
	}

	if (Response.GetStatusCode() == KHTTPError::H1xx_SWITCHING_PROTOCOLS)
	{
		Response.Headers.Set(KHTTPHeader::CONNECTION, "Upgrade");
	}
	else
	{
		Response.Headers.Set (KHTTPHeader::CONNECTION, m_bKeepAlive || m_bSwitchToWebSocket ? "keep-alive" : "close");
	}

	{
		// the headers get written directly to the unfiltered stream,
		// therefore we have to count them outside the filter pipeline
		KCountingOutputStreamBuf OutputCounter(Response.UnfilteredStream());

		// writes response headers to output, do not flush, we will have content following
		// immediately
		Serialize();

		m_iTXBytes = OutputCounter.Count();

		// with the next write now the filter pipeline gets kicked off,
		// and the unfiltered stream is only a copy of the real
		// stream used for output, therefore its streambuf is no
		// more updated by writes and can no more be used for counting,
		// so we detach proactively
	}

} // WriteHeaders

//-----------------------------------------------------------------------------
void KRESTServer::Stream(bool bAllowCompressionIfPossible, bool bWriteHeaders)
//-----------------------------------------------------------------------------
{
	if (m_bIsStreaming)
	{
		return;
	}

	ThrowIfDisconnected();

	if (m_Options.Out != HTTP)
	{
		throw KHTTPError { KHTTPError::H5xx_NOTIMPL, "streaming mode only allowed in HTTP output mode" };
	}

	m_bIsStreaming = true;

	if (!bWriteHeaders)
	{
		return;
	}

	// Compression can be a problem in streaming mode because we cannot reliably flush
	// the output then. Also, we may not know the media type in advance, and hence
	// switch compression on for already compressed media..
	ConfigureCompression(m_Options.bAllowCompression && bAllowCompressionIfPossible);

	WriteHeaders();

} // Stream

//-----------------------------------------------------------------------------
void KRESTServer::Output()
//-----------------------------------------------------------------------------
{
	if (m_bIsStreaming)
	{
		// nothing to do here, we are at end of stream apparently
		return;
	}

	ThrowIfDisconnected();

	bool bOutputContent { true };

	// do not create a response body for 202 and 3xxs
	if (Response.GetStatusCode() == KHTTPError::H2xx_NO_CONTENT ||
		Response.GetStatusCode() / 100 == 3)
	{
		m_iContentLength = 0;
		bOutputContent   = false;
		Response.Headers.Remove(KHTTPHeader::CONTENT_TYPE);
	}

	// only allow output compression if this is HTTP mode and if we allow compression and have content
	ConfigureCompression(m_Options.Out == HTTP && m_Options.bAllowCompression && bOutputContent);

    if (Request.Method == KHTTPMethod::HEAD)
	{
		// HEAD requests show all headers as if it were a GET request (therefore we configure
		// compression above before switching the output off. There may even exist a
		// Content-Length header, but the response body has to be empty!
		bOutputContent = false;
	}

	kDebug (1, "HTTP-{}: {}", Response.iStatusCode, Response.sStatusString);

	switch (m_Options.Out)
	{
		case HTTP:
		{
			KString sContent;

			if (!bOutputContent)
			{
				// we do not have content to output (per the HTTP protocol)
			}
			else if (DEKAF2_UNLIKELY(!m_sRawOutput.empty()))
			{
				// we output something else - do not set the content type, the
				// caller should have set it
				sContent = std::move(m_sRawOutput);

				m_iContentLength = sContent.length();
			}
			else if (DEKAF2_LIKELY(m_Stream == nullptr))
			{
				// the content:
				if (!m_sMessage.empty())
				{
					if (!json.tx.is_null() || Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == KMIME::JSON)
					{
						json.tx["message"] = std::move(m_sMessage);
					}
				}

				if (m_JsonLogger && !m_JsonLogger->empty() && !m_Options.KLogHeader.empty())
				{
					if ((!json.tx.is_null() || Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == KMIME::JSON)
						&& (json.tx.is_object() || json.tx.is_null()))
					{
						json.tx[m_Options.KLogHeader.Serialize()] = std::move(*m_JsonLogger);
					}
					else
					{
						kDebug(1, "cannot log to json output as output is not a json object");
					}
				}

				if (!json.tx.is_null())
				{
					kDebug (2, "serializing JSON response");
					sContent = json.tx.dump(m_iJSONPrint, '\t');
				}
				else if (!xml.tx.empty())
				{
					if (Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == KMIME::JSON)
					{
						// only set content-type to flat XML if it has not already been
						// changed by the caller from the default JSON
						Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::XML);
					}

					kDebug (2, "serializing XML response");
					xml.tx.Serialize(sContent, m_iXMLPrint);
				}

				// ensure that all responses end in a newline:
				if (!sContent.empty() && !sContent.ends_with('\n'))
				{
					sContent += '\n';
				}

				m_iContentLength = sContent.length();

				kDebug (2, "response has {} bytes", m_iContentLength);
			}

			WriteHeaders();

			if (bOutputContent)
			{
				// finally, output the content:
				if (m_Stream)
				{
					kDebug(3, "read from stream");

					if (m_iContentLength == npos)
					{
						// account for copied content from a stream as well
						KCountingInputStreamBuf Counter(*m_Stream);

						Write (*m_Stream, m_iContentLength);

						m_iContentLength = Counter.Count();
					}
					else
					{
						Write (*m_Stream, m_iContentLength);
					}
				}
				else
				{
					if (kWouldLog(4))
					{
						if (!kIsBinary(sContent))
						{
							kDebugLog (4, kLimitSizeUTF8(sContent, 2048));
						}
						else
						{
							kDebug (4, "sending {} bytes of binary data", sContent.size());
						}
					}
					else
					{
						kDebug (3, "sending {} bytes", sContent.size());
					}
					Write(sContent);
				}
			}

			if (m_Timers)
			{
				m_Timers->StoreInterval(SEND);

				if (m_Options.TimingCallback)
				{
					m_Options.TimingCallback(*this, *m_Timers);
				}
			}

			// flush all content
			Response.Flush();

			m_iTXBytes += Response.Count();
			kDebug(2, "sent bytes: {}", m_iTXBytes);
		}
		break;

		case LAMBDA:
		{
			KJSON tjson;
			tjson["statusCode"] = Response.iStatusCode;
			tjson["isBase64Encoded"] = false;

			if (!bOutputContent)
			{
				// we do not have content to output (per the HTTP protocol)
			}
			else if (DEKAF2_UNLIKELY(!m_sRawOutput.empty()))
			{
				tjson["body"] = std::move(m_sRawOutput);
			}
			else if (DEKAF2_UNLIKELY(m_Stream != nullptr))
			{
				tjson["body"] = kReadAll(*m_Stream);
			}
			else
			{
				if (!m_sMessage.empty())
				{
					if (!json.tx.is_null() || Route->Parser == KRESTRoute::JSON)
					{
						json.tx["message"] = std::move(m_sMessage);
					}
				}

				if (!json.tx.is_null())
				{
					tjson["body"] = std::move(json.tx);
				}
				else if (!xml.tx.empty())
				{
					if (Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == KMIME::JSON)
					{
						// only set content-type to flat XML if it has not already been
						// changed by the caller from the default JSON
						Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::XML);
					}

					KString sContent;
					xml.tx.Serialize(sContent, m_iXMLPrint);
					tjson["body"] = std::move(sContent);
				}
			}

			if (!m_Options.KLogHeader.empty())
			{
				// finally switch logging off if enabled
				KLog::getInstance().LogThisThreadToKLog(-1);
			}

			KJSON& jheaders = tjson["headers"] = KJSON::object();
			for (const auto& header : Response.Headers)
			{
				jheaders += { header.first, header.second };
			}

			m_iContentLength = kjson::GetStringRef(tjson, "body").size();

			Response.UnfilteredStream() << tjson.dump(m_iJSONPrint, '\t') << "\n";
		}
		break;

		case CLI:
		{
			KCountingOutputStreamBuf Counter(Response.UnfilteredStream());

			if (!bOutputContent)
			{
				// we do not have content to output (per the HTTP protocol)
			}
			else if (DEKAF2_UNLIKELY(!m_sRawOutput.empty()))
			{
				Response.UnfilteredStream().WriteLine(m_sRawOutput);
			}
			else if (DEKAF2_UNLIKELY(m_Stream != nullptr))
			{
				Response.UnfilteredStream().Write(*m_Stream, m_iContentLength);
			}
			else
			{
				if (!m_sMessage.empty())
				{
					if (!json.tx.empty() || Route->Parser == KRESTRoute::JSON)
					{
						json.tx["message"] = std::move(m_sMessage);
					}
				}

				if (!json.tx.empty())
				{
					Response.UnfilteredStream() << json.tx.dump(m_iJSONPrint, '\t');
					// finish with a linefeed (the json serializer does not add one)
					Response.UnfilteredStream().WriteLine();
				}
				else if (!xml.tx.empty())
				{
					xml.tx.Serialize(Response.UnfilteredStream(), m_iXMLPrint);
					// the xml serializer adds a linefeed in default mode, add another one if not
					if (m_iXMLPrint & KXML::NoLinefeeds)
					{
						Response.UnfilteredStream().WriteLine();
					}
				}
			}

			m_iContentLength = Counter.Count();

			if (!m_Options.KLogHeader.empty())
			{
				// finally switch logging off if enabled
				KLog::getInstance().LogThisThreadToKLog(-1);
			}
		}
		break;
	}

	if (!Response.UnfilteredStream().Good())
	{
		kDebug (2, "write error, connection lost");
		m_bLostConnection = true;
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
void KRESTServer::ErrorHandler(const std::exception& ex)
//-----------------------------------------------------------------------------
{
	// avoid switching to the websocket protocol on a failed connection
	m_bSwitchToWebSocket = false;

	if (IsDisconnected())
	{
		kDebug(1, "remote end disconnected");
		return;
	}
	
	auto xex = dynamic_cast<const KHTTPError*>(&ex);

	if (xex)
	{
		Response.SetStatus(xex->GetHTTPStatusCode(), xex->GetHTTPStatusString());
	}
	else
	{
		Response.SetStatus(KHTTPError::H5xx_ERROR);
	}

	// we need to set the HTTP version here explicitly, as we could throw as early
	// that no version is set - which will corrupt headers and body..
	Response.SetHTTPVersion(KHTTPVersion::http11);

	KString sError = ex.what();

	if (sError.empty())
	{
		sError = Response.GetStatusString();
	}

	kDebug (1, "HTTP-{}: {}\n{}",  Response.iStatusCode, Response.sStatusString, sError);

	if (m_bIsStreaming)
	{
		return;
	}

	m_iContentLength = 0;

	// do not compress/chunk error messages
	ConfigureCompression(false);

	KJSON EmptyJSON;

	if (!m_Options.KLogHeader.empty())
	{
		// finally switch logging off if enabled
		KLog::getInstance().LogThisThreadToKLog(-1);

		// and check if there is a json klog output
		auto it = json.tx.find("klog");
		if (it != json.tx.end() && !it->empty())
		{
			// yes, save the klog
			EmptyJSON["klog"] = std::move(*it);
		}
	}

	json.tx = std::move(EmptyJSON);

	switch (m_Options.Out)
	{
		case HTTP:
		{
			KString sContent;

			// do not create a response body for 3xx responses
			if (Response.GetStatusCode() / 100 != 3)
			{
				if (json.tx.empty() &&
					Response.Headers.Get(KHTTPHeader::CONTENT_TYPE) == KMIME::HTML_UTF8)
				{
					// write the error message as an HTML page if there is no
					// JSON error output and the content type is HTML
					// warning: when using clang's std::format, the below throws a format error when
					// the {} and </h2> are directly adjacent (no space).
					sContent = kFormat("<html><head>HTTP Error {}</head><body><h2>{} {} </h2></body></html>\n",
					                   Response.GetStatusCode(),
					                   Response.GetStatusCode(),
					                   sError);
				}
				else
				{
					// write the error message as a json struct
					if (sError.empty())
					{
						json.tx["message"] = Response.sStatusString;
					}
					else
					{
						json.tx["message"] = sError;
					}

					sContent = json.tx.dump(iJSONPretty, '\t');

					// ensure that all JSON responses end in a newline:
					if (!sContent.ends_with('\n'))
					{
						sContent += '\n';
					}
				}
			}
			else
			{
				Response.Headers.Remove(KHTTPHeader::CONTENT_TYPE);
			}

			m_iContentLength = sContent.size();

			// compute and set the Content-Length header:
			Response.Headers.Set(KHTTPHeader::CONTENT_LENGTH, KString::to_string(m_iContentLength));

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::SERIALIZE);

				if (!m_Options.TimerHeader.empty())
				{
					// add a custom header that marks execution time for this request
					Response.Headers.Add (m_Options.TimerHeader, KString::to_string(m_Timers->duration().milliseconds().count()));
				}
			}

			Response.Headers.Set(KHTTPHeader::CONNECTION, "close");

			{
				KCountingOutputStreamBuf OutputCounter(Response.UnfilteredStream());

				// writes response headers to output
				Serialize();

				m_iTXBytes = OutputCounter.Count();
			}

			// finally, output the content:
			kDebug (3, sContent);
			Response.Write (sContent);

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::SEND);

				if (m_Options.TimingCallback)
				{
					m_Options.TimingCallback(*this, *m_Timers);
				}
			}

			// flush all content
			Response.Flush();

			m_iTXBytes += Response.Count();
			kDebug(2, "sent bytes: {}", m_iTXBytes);
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
			Response.UnfilteredStream().FormatLine("{}: {}", Dekaf::getInstance().GetProgName(), sError);
		}
		break;
	}

	if (!Response.UnfilteredStream().Good())
	{
		kDebug (1, "write error, connection lost");
		m_bLostConnection = true;
	}

} // ErrorHandler

//-----------------------------------------------------------------------------
void KRESTServer::SetStatus (uint16_t iCode, KStringView sOptionalStatusString/*=""*/)
//-----------------------------------------------------------------------------
{
	if (iCode < 400)
	{
		Response.SetStatus (KHTTPError::ConvertToRealStatusCode(iCode),
		                    kFirstNonEmpty(sOptionalStatusString, KHTTPError::GetStatusString(iCode)));
	}
	else
	{
		Response.SetStatus (iCode, kFirstNonEmpty (sOptionalStatusString, "INTERNAL SERVER ERROR"));
	}

} // SetStatus

//-----------------------------------------------------------------------------
void KRESTServer::RecordRequestForReplay ()
//-----------------------------------------------------------------------------
{
	if (!m_Options.sRecordFile.empty())
	{
		KString sRecord;
		KOutStringStream oss(sRecord);

		if (!kFileExists(m_Options.sRecordFile))
		{
			// there is a chance that this test races at initial creation of the
			// record file, but it wouldn't matter as the bang header then simply
			// becomes a comment
			oss.WriteLine("#! /bin/bash");
		}

		// we can now write the request into the recording file
		oss.WriteLine();
		oss.FormatLine("# {} :: from IP {}", kFormTimestamp(), Request.GetRemoteIP());

		if (!Response.Good())
		{
			oss.Format("#{}#", Response.GetStatusCode());
		}

		KString sAdditionalHeader;
		if (Request.Method != KHTTPMethod::GET)
		{
			KString sContentType = Request.Headers.Get(KHTTPHeader::CONTENT_TYPE);
			if (sContentType.empty())
			{
				sContentType = KMIME::JSON;
			}
			sAdditionalHeader.Format(" -H '{}: {}'", KHTTPHeader(KHTTPHeader::CONTENT_TYPE), sContentType);

			KString& sReferer = Request.Headers.Get(KHTTPHeader::REFERER);
			if (!sReferer.empty())
			{
				sAdditionalHeader += kFormat(" -H '{}: {}'", KHTTPHeader(KHTTPHeader::REFERER), sReferer);
			}
		}

		oss.Format(R"(curl -i{} -X "{}" "http://localhost{}")",
						  sAdditionalHeader,
						  Request.Method.Serialize(),
						  Request.RequestLine.GetResource());

		KString sPost { GetRequestBody() };

		if (!sPost.empty())
		{
			// remove any line breaks
			sPost.Collapse("\n\r", ' ');
			// escape single quotes for bash
			sPost.Replace("'", "'\\''");
			oss.Write(" -d '");
			oss.Write(sPost);
			oss.Write('\'');
		}

		oss.Flush();

		// Making this a static could lead to wrong output streams if multiple servers
		// were started with different record files. But given that this is an extremely
		// rare condition as the record function is a debug method which should never be
		// used in parallel in real usage, we currently leave the code as is. A remedy
		// would be to move the stream construction into the Options struct, as is done
		// for the JSONAccessLog.
		static auto RecordStream = kOpenOutStream(m_Options.sRecordFile, std::ios::app);

		kLogger(*RecordStream, sRecord);
	}

} // RecordRequestForReplay

//-----------------------------------------------------------------------------
void KRESTServer::PrettyPrint(bool bYesNo)
//-----------------------------------------------------------------------------
{
	m_iJSONPrint = bYesNo ? iJSONPretty : iJSONTerse;
	m_iXMLPrint  = bYesNo ? iXMLPretty  : iXMLTerse;

} // PrettyPrint

//-----------------------------------------------------------------------------
KStringView KRESTServer::GetCookie(KStringView sName)
//-----------------------------------------------------------------------------
{
	auto& Cookies = GetCookies();
	auto it = Cookies.find(sName);
	return (it == Cookies.end()) ? KStringView{} : it->second;

} // GetCookie

//-----------------------------------------------------------------------------
const std::map<KStringView, KStringView>& KRESTServer::GetCookies()
//-----------------------------------------------------------------------------
{
	if (m_RequestCookies == nullptr)
	{
		m_RequestCookies = std::make_unique<std::map<KStringView, KStringView>>();

		const auto Range = Request.Headers.equal_range(KHTTPHeader::COOKIE);

		for (auto it = Range.first; it != Range.second; ++it)
		{
			kSplit(*m_RequestCookies, it->second, ";", "=");
		}
	}

	return *m_RequestCookies;

} // GetCookies

//-----------------------------------------------------------------------------
void KRESTServer::SetCookie(KStringView sName, KStringView sValue, KStringView sOptions)
//-----------------------------------------------------------------------------
{
	if (!sName.empty())
	{
		auto sCookie = kFormat("{}={}", sName, sValue);

		if (!sOptions.empty())
		{
			sCookie += "; ";
			sCookie += sOptions;
		}

		Response.Headers.Add(KHTTPHeader::SET_COOKIE, std::move(sCookie));
	}
	else
	{
		kDebug(2, "cookie can only be set with a valid name");
	}

} // SetCookie

//-----------------------------------------------------------------------------
void KRESTServer::clear()
//-----------------------------------------------------------------------------
{
	KHTTPServer::clear();

	json.clear();
	xml.clear();

	m_sRequestBody.clear();
	m_sMessage.clear();
	m_sRawOutput.clear();
	m_Stream.reset();
	m_iTXBytes             = 0;
	m_iContentLength       = npos;
	m_iRequestHeaderLength = 0;
	m_iRequestBodyLength   = 0;
	m_AuthToken.clear();
	m_JsonLogger.reset();
	m_RequestCookies.reset();
	m_TempDir.clear();
	m_bIsStreaming         = false;
	m_bSwitchToWebSocket   = false;
	m_bKeepWebSocketThread = false;
	// do not clear m_Timers, the main Execute loop takes care of it

	m_iJSONPrint =
#ifdef NDEBUG
		m_Options.bPrettyPrint ? iJSONPretty : iJSONTerse;
#else
		iJSONPretty;
#endif
	m_iXMLPrint =
#ifdef NDEBUG
		m_Options.bPrettyPrint ? iXMLPretty : iXMLTerse;
#else
		iXMLPretty;
#endif

} // clear

// our empty route..
const KRESTRoute KRESTServer::s_EmptyRoute({}, false, "/empty", "", nullptr, KRESTRoute::NOREAD);

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr std::array<KRESTServer::TimerLabel, KRESTServer::SEND + 1> KRESTServer::Timers;
#endif

DEKAF2_NAMESPACE_END
