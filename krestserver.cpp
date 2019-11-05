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

namespace dekaf2 {

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
				throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization" };
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
				throw KHTTPError { KHTTPError::H4xx_NOTAUTH, "no authorization" };
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
		enum PARTYPE { NONE, START, LEVEL, OUT, GREP, EGREP };

#ifdef DEKAF2_HAS_FROZEN
		static constexpr auto s_Option = frozen::make_unordered_map<KStringView, PARTYPE>(
#else
		static const std::unordered_map<KStringView, PARTYPE> s_Option
#endif
		{
			{ "-level", LEVEL },
			{ "-out"  , OUT },
			{ "-E"    , EGREP },
			{ "-egrep", EGREP },
			{ "-grep" , GREP }
		}
#ifdef DEKAF2_HAS_FROZEN
		)
#endif
		; // do not erase..

		PARTYPE iPType { START };

		int  iKLogLevel { 0 };
		bool bToKLog    { false };
		bool bToJSON    { false };
		bool bEGrep     { false };
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
					iPType = itArg->second;
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
						sGrep = sArg;
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
			return;
		}

		if (bToKLog)
		{
			KLog::getInstance().LogThisThreadToKLog(iKLogLevel);
			kDebug(2, "per-thread {} logging, level {}", "klog", iKLogLevel);
		}
		else if (bToJSON)
		{
#ifdef DEKAF2_KLOG_WITH_TCP
			json.tx["klog"] = KJSON::array();
			KLog::getInstance().LogThisThreadToJSON(iKLogLevel, &json.tx["klog"]);
			kDebug(2, "per-thread {} logging, level {}", "JSON", iKLogLevel);
#else
			kDebug(2, "request to switch {} logging on, but compiled without support", "json response");
			return;
#endif
		}
		else
		{
#ifdef DEKAF2_KLOG_WITH_TCP
			KLog::getInstance().LogThisThreadToResponseHeaders(iKLogLevel, Response, Options.sKLogHeader);
			kDebug(2, "per-thread {} logging, level {}", "response header", iKLogLevel);
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

			Response.SetStatus(200, "OK");
			Response.sHTTPVersion = "HTTP/1.1";

			kDebug (2, "incoming: {} {}", Request.Method.Serialize(), Request.Resource.Path);

			KStringView sURLPath = Request.Resource.Path;

			// try to remove_prefix, do not complain if not existing
			sURLPath.remove_prefix(Options.sBaseRoute);

			// try to remove a trailing / - we treat /path and /path/ as the same address
			sURLPath.remove_suffix("/");

			// find the right route
			route = &Routes.FindRoute(KRESTPath(Request.Method, sURLPath), Request.Resource.Query, Options.bCheckForWrongMethod);

			// OPTIONS method is allowed without Authorization header (it is used to request
			// for Authorization permission)
			if (Options.AuthLevel != Options::ALLOW_ALL
				&& route->bAuth
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

			if (!route->Callback)
			{
				throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
			}

			if (Request.Method != KHTTPMethod::GET && Request.HasContent())
			{
				switch (route->Parser)
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
						}
						break;

					case KRESTRoute::PLAIN:
						// read body and store for later access
						kDebug (2, "reading {} request body", "plain");
						m_sRequestBody = KHTTPServer::Read();
						kDebug (2, "read {} request body with length {} and type {}",
								"plain",
								m_sRequestBody.size(),
								Request.Headers[KHTTPHeaders::CONTENT_TYPE]);
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
								Request.Headers[KHTTPHeaders::CONTENT_TYPE]);
						m_sRequestBody.Trim();
						// operator+=() causes additive parsing for a query component
						Request.Resource.Query += m_sRequestBody;
					}
					break;

				}
			}

			// call the route handler
			kDebug (1, "{}: {}", GetRequestMethod(), GetRequestPath());
			route->Callback(*this);

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
	static constexpr int iXMLPretty { KXML::NoIndents | KXML::NoLinefeeds };
#else
	static constexpr int iJSONPretty { 1 };
	static constexpr int iXMLPretty { KXML::Default };
#endif

//-----------------------------------------------------------------------------
void KRESTServer::Output(const Options& Options, bool bKeepAlive)
//-----------------------------------------------------------------------------
{
	// only allow output compression if this is HTTP mode
	ConfigureCompression(Options.Out == HTTP);

	kDebug (1, "HTTP-{}: {}", Response.iStatusCode, Response.sStatusString);

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
					if (!json.tx.empty() || route->Parser == KRESTRoute::JSON)
					{
						json.tx["message"] = std::move(m_sMessage);
					}
				}

				if (!json.tx.empty())
				{
					kDebug (2, "serializing JSON response");
					sContent = json.tx.dump(iJSONPretty, '\t');
					// ensure that all JSON responses end in a newline:
					sContent += '\n';
					kDebug (2, "JSON response has {} bytes", sContent.length());
				}
				else if (!xml.tx.empty())
				{
					Response.Headers.Set(KHTTPHeaders::CONTENT_TYPE, KMIME::XML);

					kDebug (2, "serializing XML response");
					xml.tx.Serialize(sContent, iXMLPretty);
					// ensure that all XML responses end in a newline:
					sContent += '\n';
					kDebug (2, "XML response has {} bytes", sContent.length());
				}
			}

			if (!Options.sKLogHeader.empty())
			{
				// finally switch logging off if enabled
				KLog::getInstance().LogThisThreadToKLog(-1);
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

				if (!json.tx.empty())
				{
					tjson["body"] = std::move(json.tx);
				}
				else if (!xml.tx.empty())
				{
					Response.Headers.Set(KHTTPHeaders::CONTENT_TYPE, KMIME::XML);

					KString sContent;
					xml.tx.Serialize(sContent, iXMLPretty);
					tjson["body"] = std::move(sContent);
				}
			}

			if (!Options.sKLogHeader.empty())
			{
				// finally switch logging off if enabled
				KLog::getInstance().LogThisThreadToKLog(-1);
			}

			KJSON& jheaders = tjson["headers"] = KJSON::object();
			for (const auto& header : Response.Headers)
			{
				jheaders += { header.first, header.second };
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
					if (!json.tx.empty() || route->Parser == KRESTRoute::JSON)
					{
						json.tx["message"] = std::move(m_sMessage);
					}
				}

				if (!json.tx.empty())
				{
					Response.UnfilteredStream() << json.tx.dump(iJSONPretty, '\t');
				}
				else if (!xml.tx.empty())
				{
					xml.tx.Serialize(Response.UnfilteredStream(), iXMLPretty);
				}
			}

			if (!Options.sKLogHeader.empty())
			{
				// finally switch logging off if enabled
				KLog::getInstance().LogThisThreadToKLog(-1);
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
	auto xex = dynamic_cast<const KHTTPError*>(&ex);

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
			kDebug (3, "{}", sContent);
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
		oss.FormatLine("# {} :: from IP {}", kFormTimestamp(), Request.GetBrowserIP());

		if (Response.GetStatusCode() > 299)
		{
			oss.Format("#{}#", Response.GetStatusCode());
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

		oss.Format(R"(curl -i{} -X "{}" "{}")",
						  sAdditionalHeader,
						  Request.Method.Serialize(),
						  URL.Serialize());

		KString sPost { GetRequestBody() };

		if (!sPost.empty())
		{
			sPost.Collapse("\n\r", ' ');
			oss.Write(" -d '");
			oss.Write(sPost);
			oss.Write('\'');
		}

		oss.WriteLine();
		oss.Flush();

		KOutFile RecordFile(Options.sRecordFile, std::ios::app);

		if (!RecordFile.is_open())
		{
			kDebug (3, "cannot open {}", Options.sRecordFile);
			return;
		}

		// write the output in one run to avoid thread segmentation
		RecordFile.Write(sRecord).Flush();
	}

} // RecordRequestForReplay

//-----------------------------------------------------------------------------
void KRESTServer::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	json.clear();
	xml.clear();
	m_sRequestBody.clear();
	m_sMessage.clear();
	m_sRawOutput.clear();

} // clear

// our empty route..
const KRESTRoute KRESTServer::s_EmptyRoute({}, false, "/empty", nullptr, KRESTRoute::NOREAD);

} // end of namespace dekaf2
