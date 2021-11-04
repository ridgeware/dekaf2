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
#include "ktimer.h"
#include "kcrashexit.h"
#include "kwriter.h"
#include "kcountingstreambuf.h"
#include "krow.h"
#include "ktime.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
void KRESTServer::Options::AddHeader(KHTTPHeader Header, KString sValue)
//-----------------------------------------------------------------------------
{
	ResponseHeaders.Add(std::move(Header), std::move(sValue));

} // AddHeader

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
void KRESTServer::VerifyAuthentication(const Options& Options)
//-----------------------------------------------------------------------------
{
	switch (Options.AuthLevel)
	{
		case Options::ALLOW_ALL:
			break;

		case Options::ALLOW_ALL_WITH_AUTH_HEADER:
			if (Request.Headers[KHTTPHeader::AUTHORIZATION].empty())
			{
				throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization header" };
			}
			break;

		case Options::VERIFY_AUTH_HEADER:
			{
				auto& Authorization = Request.Headers[KHTTPHeader::AUTHORIZATION];

				if (Authorization.empty())
				{
					// failure
					throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization header" };
				}

				if (Options.Authenticators.empty())
				{
					kWarning("authenticator list is empty");
				}
				else
				{
					if (m_AuthToken.Check(Authorization, Options.Authenticators, Options.sAuthScope))
					{
						// success
						SetAuthenticatedUser(kjson::GetString(GetAuthToken(), "sub"));
						return;
					}
				}
				// failure
				throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization" };
			}
			break;
	}

} // VerifyAuthentication


//-----------------------------------------------------------------------------
int KRESTServer::VerifyPerThreadKLogToHeader(const Options& Options)
//-----------------------------------------------------------------------------
{
	static constexpr KStringView s_sHeaderLoggingHelp {
		"supported header logging commands:\n"
		" -level <n> (or only <n>) :: set logging level (1..3)\n"
		" -E, -egrep <expression>  :: match regular expression to log\n"
		" -grep <substring>        :: match substring to log\n"
		" -out <headers|log|json>  :: output target for this thread, default = headers\n"
	};

	int  iKLogLevel { 0 };

	auto it = Request.Headers.find(Options.KLogHeader);

	if (it != Request.Headers.end())
	{
		enum PARTYPE { NONE, START, LEVEL, OUT, GREP, EGREP, HELP };

#ifdef DEKAF2_HAS_FROZEN
		static constexpr auto s_Option = frozen::make_unordered_map<KStringView, PARTYPE>(
#else
		static const std::unordered_map<KStringView, PARTYPE> s_Option
#endif
		{
			{ "-level", LEVEL },
			{ "-out"  , OUT   },
			{ "-E"    , EGREP },
			{ "-egrep", EGREP },
			{ "-grep" , GREP  },
			{ "-h"    , HELP  },
			{ "-help" , HELP  },
			{ "--help", HELP  }
		}
#ifdef DEKAF2_HAS_FROZEN
		)
#endif
		; // do not erase..

		PARTYPE iPType { START };

		bool bToKLog    { false };
		bool bToJSON    { false };
		bool bEGrep     { false };
		bool bHelp      { false };
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
					else if (itArg->second == HELP)
					{
						bHelp = true;
						if (!iKLogLevel)
						{
							iKLogLevel = 1;
						}
					}
					else
					{
						iPType = itArg->second;
					}
					break;
				}

				case LEVEL:
					iKLogLevel = sArg.UInt16();

					if (iKLogLevel <= 0 || iKLogLevel > 3)
					{
						bValid = false;
					}
					iPType = NONE;
					break;

				case OUT:
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
			KLog::getInstance().LogThisThreadToResponseHeaders(iKLogLevel, Response, Options.KLogHeader.Serialize());
			kDebug(3, "per-thread {} logging, level {}", "response header", iKLogLevel);
			if (bHelp)
			{
				kDebugLog(1, s_sHeaderLoggingHelp);
			}
#else
			kDebug(2, "request to switch {} logging on, but compiled without support", "response header");
			return;
#endif
		}

		if (sGrep)
		{
			KLog::getInstance().LogThisThreadWithGrepExpression(bEGrep, sGrep);
		}
	}

	return iKLogLevel;

} // VerifyPerThreadKLogToHeader

//-----------------------------------------------------------------------------
void KRESTServer::Parse(const Options& Options)
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

			// nobody wants stack traces in the klog when hackers throw crappy json (and attacks)
			// at their rest server.  so we need to turn off stack traces while we attempt to
			// parse incoming json from the wire:
			bool bResetFlag = KLog::getInstance().ShowStackOnJsonError (false);

			// parse the content into json.rx
			KString sError;

			kDebug (2, "parsing JSON request");
			if (!kjson::Parse(json.rx, KHTTPServer::InStream(), sError))
			{
				kDebug (2, "request body is not JSON: {}", sError);
				json.rx.clear();
				if (Options.bThrowIfInvalidJson)
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat ("invalid JSON: {}", sError) };
				}
			}
			else
			{
				kDebug (2, "request body successfully parsed as JSON");

				if (Options.bRecordRequest)
				{
					// dump the json to record it
					m_sRequestBody = json.rx.dump(-1);
				}
			}

			// after we are done parsing the incoming json from the wire,
			// restore stack traces for failures in the json that application may
			// form while processing a request:
			KLog::getInstance().ShowStackOnJsonError (bResetFlag);
		}
		break;

		case KRESTRoute::XML:
			// read input as XML
			kDebug (2, "parsing XML request");
			if (!xml.rx.Parse(KHTTPServer::InStream(), true))
			{
				kDebug (2, "request body is not XML");
				xml.rx.clear();
				if (Options.bThrowIfInvalidJson)
				{
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "invalid XML" };
				}
			}
			else
			{
				kDebug (2, "request body successfully parsed as XML");

				if (Options.bRecordRequest)
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
bool KRESTServer::Execute(const Options& Options, const KRESTRoutes& Routes)
//-----------------------------------------------------------------------------
{
	// provide a pointer to the current set of routes
	this->Routes = &Routes;
	// and options
	this->pOptions = &Options;

	try
	{
		m_iRound = 0;

		for (;;)
		{
			kDebug (3, "keepalive round {}", m_iRound + 1);
			kSetCrashContext("KRestServer");

			clear();

			// per default we output JSON
			Response.Headers.Add(KHTTPHeader::CONTENT_TYPE, KMIME::JSON);

			// add additional response headers
			for (auto& it : Options.ResponseHeaders)
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
				if (!Options.TimerHeader.empty() || Options.TimingCallback)
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
				if (KHTTPServer::Error().empty())
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
					kDebug (1, "read error: {}", KHTTPServer::Error());
					throw KHTTPError { KHTTPError::H4xx_BADREQUEST, KHTTPServer::Error() };
				}
			}

			if (Request.Method == KHTTPMethod::INVALID)
			{
				kDebug (2, "invalid request method: {}", Request.RequestLine.GetMethod());
				throw KHTTPError { KHTTPError::H4xx_BADMETHOD, kFormat("invalid request method: {}", Request.RequestLine.GetMethod()) };
			}

			if (!Options.bServiceIsReady)
			{
				throw KHTTPError { KHTTPError::H5xx_UNAVAILABLE, "service unavailable" };
			}

			Response.SetStatus(200, "OK");
			Response.sHTTPVersion = "HTTP/1.1";

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::RECEIVE);
			}

			kDebug (2, "incoming: {} {}", Request.Method.Serialize(), Request.Resource.Path);

			if (Options.PreRouteCallback)
			{
				Options.PreRouteCallback(*this);
			}

			KString sURLPath = Request.Resource.Path.get();

			// try to remove_prefix, do not complain if not existing
			sURLPath.remove_prefix(Options.sBaseRoute);

			// check if we have rewrite rules for the request path
			Routes.RewritePath(sURLPath);

			// check if we have redirect rules for the request path
			if (Routes.RedirectPath(sURLPath) > 0)
			{
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

			// find the right route
			Route = &Routes.FindRoute(RequestPath, Request.Resource.Query, Options.bCheckForWrongMethod);

			if (!Route->Callback)
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
			}

			if (Route->Option(KRESTRoute::Options::GENERIC_AUTH))
			{
				if (Options.AuthCallback)
				{
					SetAuthenticatedUser(Options.AuthCallback(*this));
				}
				else
				{
					kDebug(2, "no auth callback for: {}", Route->sRoute);
				}
			}

			bool bSSOAccepted { false };

			// OPTIONS method is allowed without Authorization header (it is used to request
			// for Authorization permission)
			if (Route->Option(KRESTRoute::Options::SSO_AUTH)
				&& Request.Method != KHTTPMethod::OPTIONS)
			{
				// check if this route permits other authentication methods (probably triggered
				// in PreRouteCallback()) and has confirmed a valid user
				if (!Route->Option(KRESTRoute::Options::GENERIC_AUTH) || GetAuthenticatedUser().empty())
				{
					VerifyAuthentication(Options);
					bSSOAccepted = true;
				}
			}

			if (Options.PostRouteCallback)
			{
				Options.PostRouteCallback(*this);
			}

			if (!bSSOAccepted
				&& Route->Option(KRESTRoute::Options::GENERIC_AUTH)
				&& Request.Method != KHTTPMethod::OPTIONS
				&& GetAuthenticatedUser().empty())
			{
				// generic auth was requested, but neither SSO nor any other authentication method
				// resulted in a user name
				throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization" };
			}

			// switch header logging only after authorization (but not for OPTIONS, as it is
			// not authenticated..)
			if (!Options.KLogHeader.empty() && Request.Method != KHTTPMethod::OPTIONS)
			{
				if (VerifyPerThreadKLogToHeader(Options) > 1)
				{
					kDebug (2, "Request: {} {} {}", Request.Method.Serialize(), Request.Resource.Path, Request.sHTTPVersion);
					// output headers for this thread
					for (const auto& Header : Request.Headers)
					{
						kDebug(2, "Header: {}: {}", Header.first, Header.second);
					}
				}
			}

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::ROUTE);
			}

			if (Request.HasContent(Request.Method == KHTTPMethod::GET))
			{
				Parse(Options);
			}

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::PARSE);
			}

			// we store this count() twice, because some routes may read the
			// request themselves, but we want to assure we have a value should
			// an exception be thrown
			m_iRequestBodyLength = Request.Count();

			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// debug info
			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			kDebug (1, KLog::DASH);
			kDebug (1, "{}: {}", GetRequestMethod(), GetRequestPath());
			kDebug (1, KLog::DASH);

			kSetCrashContext (kFormat ("{}: {}\nHost: {} Remote IP: {}",
									   Request.Method.Serialize(),
									   Request.Resource.Serialize(),
									   Options.sServername,
									   Request.GetRemoteIP())
							  );

			// check that we are still connected to the remote end
			ThrowIfDisconnected();

			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			// call the application method to handle this request:
			// - - - - - - - - - - - - - - - - - - - - - - - - - - -
			Route->Callback(*this);

			kAppendCrashContext("completed route handler");

			Request.close();
			m_iRequestBodyLength = Request.Count();

			if (m_Timers)
			{
				m_Timers->StoreInterval(Timer::PROCESS);
			}

			// We offer a keep-alive if the client did not explicitly
			// request a close. We only allow for a limited amount
			// of keep-alive rounds, as this blocks one thread of
			// execution and could lead to a DoS if an attacker would
			// hold as many connections as we have simultaneous threads.
			m_bKeepAlive = !m_bIsStreaming
						&& (Options.Out == HTTP)
						&& (m_iRound+1 < Options.iMaxKeepaliveRounds)
						&& Request.HasKeepAlive();

			Output(Options);

			Options.Logger.Log(*this);

			if (Options.bRecordRequest)
			{
				RecordRequestForReplay(Options);
			}

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
				if (Options.Out == HTTP)
				{
					if (!Request.HasKeepAlive())
					{
						kDebug (2, "keep-alive not supported by client - closing connection in round {}", m_iRound);
					}
					else
					{
						kDebug (2, "no further keep-alive allowed - closing connection in round {}", m_iRound);
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
		ErrorHandler(ex, Options);
	}

	Options.Logger.Log(*this);

	if (Options.bRecordRequest)
	{
		RecordRequestForReplay(Options);
	}

	return false;

} // Execute

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
void KRESTServer::WriteHeaders(const Options& Options)
//-----------------------------------------------------------------------------
{
	if (!Options.KLogHeader.empty())
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

		if (!Options.TimerHeader.empty())
		{
			// add a custom header that marks execution time for this request
			Response.Headers.Set (Options.TimerHeader,
								  KString::to_string(Options.bMicrosecondTimerHeader
													 ? m_Timers->microseconds()
													 : m_Timers->milliseconds()));
		}
	}

	Response.Headers.Set (KHTTPHeader::CONNECTION, m_bKeepAlive ? "keep-alive" : "close");

	{
		// the headers get written directly to the unfiltered stream,
		// therefore we have to count them outside of the filter pipeline
		KCountingOutputStreamBuf OutputCounter(Response.UnfilteredStream());

		// writes response headers to output
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

	if (!this->pOptions)
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, "config error" };
	}

	if (this->pOptions->Out != HTTP)
	{
		throw KHTTPError { KHTTPError::H5xx_NOTIMPL, "streaming mode only allowed in HTTP output mode" };
	}

	// do not signal nor perform a keep alive connection
	m_bKeepAlive = false;

	if (!bWriteHeaders)
	{
		return;
	}

	// Compression can be a problem in streaming mode because we cannot reliably flush
	// the output then. Also, we may not know the media type in advance, and hence
	// switch compression on for already compressed media..
	ConfigureCompression(this->pOptions->bAllowCompression && bAllowCompressionIfPossible);

	WriteHeaders(*this->pOptions);

	m_bIsStreaming = true;

} // Stream

//-----------------------------------------------------------------------------
void KRESTServer::Output(const Options& Options)
//-----------------------------------------------------------------------------
{
	if (m_bIsStreaming)
	{
		// nothing to do here, we are at end of stream apparently
		return;
	}

	ThrowIfDisconnected();

	bool bOutputContent { true };

	// do not create a response for 202 and 3xxs
	if (Response.GetStatusCode() == KHTTPError::H2xx_NO_CONTENT ||
		Response.GetStatusCode() / 100 == 3)
	{
		m_iContentLength = 0;
		bOutputContent   = false;
		Response.Headers.Remove(KHTTPHeader::CONTENT_TYPE);
	}

	// only allow output compression if this is HTTP mode and if we allow compression and have content
	ConfigureCompression(Options.Out == HTTP && Options.bAllowCompression && bOutputContent);

	kDebug (1, "HTTP-{}: {}", Response.iStatusCode, Response.sStatusString);

	switch (Options.Out)
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
					if (!json.tx.empty() || Route->Parser == KRESTRoute::JSON)
					{
						json.tx["message"] = std::move(m_sMessage);
					}
				}

				if (m_JsonLogger && !m_JsonLogger->empty() && !Options.KLogHeader.empty())
				{
					if ((!json.tx.empty() || Route->Parser == KRESTRoute::JSON)
						&& json.tx.is_object())
					{
						json.tx[Options.KLogHeader.Serialize()] = std::move(*m_JsonLogger);
					}
					else
					{
						kDebug(1, "cannot log to json output as output is not a json object");
					}
				}

				if (!json.tx.empty())
				{
					kDebug (2, "serializing JSON response");
					sContent = json.tx.dump(m_iJSONPrint, '\t');

					// ensure that all JSON responses end in a newline:
					if (!sContent.ends_with('\n'))
					{
						sContent += '\n';
					}

					kDebug (2, "JSON response has {} bytes", sContent.length());
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

					// ensure that all XML responses end in a newline:
					if (!sContent.ends_with('\n'))
					{
						sContent += '\n';
					}

					kDebug (2, "XML response has {} bytes", sContent.length());
				}

				m_iContentLength = sContent.length();
			}

			WriteHeaders(Options);

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
					kDebugLog (3, sContent);
					Write(sContent);
				}
			}

			if (m_Timers)
			{
				m_Timers->StoreInterval(SEND);

				if (Options.TimingCallback)
				{
					Options.TimingCallback(*this, *m_Timers.get());
				}
			}

			// we have to force the output pipeline to close to reliably
			// flush all content
			Response.close();

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
					json.tx["message"] = std::move(m_sMessage);
				}

				if (!json.tx.empty())
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

			if (!Options.KLogHeader.empty())
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
			if (DEKAF2_UNLIKELY(!m_sRawOutput.empty()))
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

			if (!Options.KLogHeader.empty())
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
void KRESTServer::ErrorHandler(const std::exception& ex, const Options& Options)
//-----------------------------------------------------------------------------
{
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
	Response.sHTTPVersion = "HTTP/1.1";

	KStringViewZ sError = ex.what();

	kDebug (1, "HTTP-{}: {}\n{}",  Response.iStatusCode, Response.sStatusString, sError);

	if (m_bIsStreaming)
	{
		return;
	}

	m_iContentLength = 0;

	// do not compress/chunk error messages
	ConfigureCompression(false);

	KJSON EmptyJSON;

	if (!Options.KLogHeader.empty())
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

	switch (Options.Out)
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
					// write the error message as a HTML page if there is no
					// JSON error output and the content type is HTML
					sContent = kFormat(R"(<html><head>HTTP Error {}</head><body><h2>{} {}</h2></body></html>)",
									   Response.GetStatusCode(),
									   Response.GetStatusCode(),
									   sError.empty() ? Response.GetStatusString().ToView() : sError);
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

				if (!Options.TimerHeader.empty())
				{
					// add a custom header that marks execution time for this request
					Response.Headers.Add (Options.TimerHeader, KString::to_string(m_Timers->milliseconds()));
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

				if (Options.TimingCallback)
				{
					Options.TimingCallback(*this, *m_Timers.get());
				}
			}

			// we have to force the output pipeline to close to reliably
			// flush all content
			Response.close();

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
		m_bLostConnection = true;
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
		case KHTTPError::H2xx_ACCEPTED:     Response.SetStatus (202, "ACCEPTED");               break;
		case KHTTPError::H2xx_UPDATED:      Response.SetStatus (201, "UPDATED");                break;
		case KHTTPError::H2xx_DELETED:      Response.SetStatus (201, "DELETED");                break;
		case KHTTPError::H2xx_NO_CONTENT:   Response.SetStatus (204, "NO CONTENT");             break;
		case KHTTPError::H2xx_ALREADY:      Response.SetStatus (208, "ALREADY DONE");           break;

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
		KString sRecord;
		KOutStringStream oss(sRecord);

		if (!kFileExists(Options.sRecordFile))
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
		static auto RecordStream = kOpenOutStream(Options.sRecordFile, std::ios::app);

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
	m_TempDir.clear();
	m_bIsStreaming         = false;
	// do not clear m_Timer, the main Execute loop takes care of it

	m_iJSONPrint =
#ifdef NDEBUG
		iJSONTerse;
#else
		iJSONPretty;
#endif
	m_iXMLPrint =
#ifdef NDEBUG
		iXMLTerse;
#else
		iXMLPretty;
#endif

} // clear

// our empty route..
const KRESTRoute KRESTServer::s_EmptyRoute({}, false, "/empty", "", nullptr, KRESTRoute::NOREAD);

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
constexpr std::array<KRESTServer::TimerLabel, KRESTServer::SEND + 1> KRESTServer::Timers;
#endif

} // end of namespace dekaf2
