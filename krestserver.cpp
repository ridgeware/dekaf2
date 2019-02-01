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

namespace dekaf2 {

//-----------------------------------------------------------------------------
KRESTPath::KRESTPath(KHTTPMethod _Method, KStringView _sRoute)
//-----------------------------------------------------------------------------
	: Method(std::move(_Method))
	, sRoute(std::move(_sRoute))
{
	if (sRoute.front() != '/')
	{
		kWarning("error: route does not start with a slash: {}", sRoute);
	}

	SplitURL(vURLParts, sRoute);

} // KRESTPath

//-----------------------------------------------------------------------------
size_t KRESTPath::SplitURL(URLParts& Parts, KStringView sURLPath)
//-----------------------------------------------------------------------------
{
	Parts.clear();
	Parts.reserve(2);
	sURLPath.remove_prefix("/");
	return kSplit(Parts, sURLPath, "/", "");

} // SplitURL

//-----------------------------------------------------------------------------
KRESTRoute::KRESTRoute(KHTTPMethod _Method, KStringView _sRoute, RESTCallback _Callback)
//-----------------------------------------------------------------------------
	: KRESTPath(std::move(_Method), std::move(_sRoute))
	, Callback(std::move(_Callback))
{
	bHasParameters = sRoute.Contains("/:");

	size_t iCount { 0 };

	for (auto& it : vURLParts)
	{
		++iCount;

		if (it == "*")
		{
			if (iCount == vURLParts.size())
			{
				bHasWildCardAtEnd = true;

				if (!sRoute.remove_suffix("/*"))
				{
					kWarning("cannot remove suffix '/*' from '{}'", sRoute);
				}
			}
			else
			{
				bHasWildCardFragment = true;
			}
			break;
		}
	}
	
} // KRESTRoute

//-----------------------------------------------------------------------------
KRESTRoutes::KRESTRoutes(KRESTRoute::RESTCallback DefaultRoute)
//-----------------------------------------------------------------------------
	: m_DefaultRoute(KRESTRoute(KHTTPMethod{}, KString{}, DefaultRoute))
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
void KRESTRoutes::SetDefaultRoute(KRESTRoute::RESTCallback Callback)
//-----------------------------------------------------------------------------
{
	m_DefaultRoute.Callback = Callback;

} // SetDefaultRoute

//-----------------------------------------------------------------------------
void KRESTRoutes::clear()
//-----------------------------------------------------------------------------
{
	m_Routes.clear();
	m_DefaultRoute.Callback = nullptr;

} // clear

//-----------------------------------------------------------------------------
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTPath& Path, Parameters& Params) const
//-----------------------------------------------------------------------------
{
	for (const auto& it : m_Routes)
	{
		if (it.Method.empty() || it.Method == Path.Method)
		{
			if (!it.bHasParameters && !it.bHasWildCardFragment)
			{
				if (DEKAF2_UNLIKELY(it.bHasWildCardAtEnd))
				{
					// this is a plain route with a wildcard at the end
					if (DEKAF2_UNLIKELY(Path.sRoute.StartsWith(it.sRoute)))
					{
						// take care that we only match full fragments, not parts of them
						if (Path.sRoute.size() == it.sRoute.size() || Path.sRoute[it.sRoute.size()] == '/')
						{
							return it;
						}
					}
				}
				else
				{
					// this is a plain route - we do not check part by part
					if (DEKAF2_UNLIKELY(Path.sRoute == it.sRoute))
					{
						return it;
					}
				}
			}
			else
			{
				// we have parameters or wildcard fragments, check part by part of the route
				if (it.vURLParts.size() >= Path.vURLParts.size())
				{
					Params.clear();
					auto req = Path.vURLParts.cbegin();
					bool bFound { true };
					bool bOnlyParms { false };

					for (auto& part : it.vURLParts)
					{
						if (DEKAF2_UNLIKELY(bOnlyParms))
						{
							// check remaining route fragments for being :parameters
							if (part.front() != ':')
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
								// this is a variable
								KStringView sName = part;
								// remove the colon
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
						return it;
					}
				}
			}
		}

	}

	if (m_DefaultRoute.Callback)
	{
		return m_DefaultRoute;
	}

	throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("unknown address: {} {}", Path.Method.Serialize(), Path.sRoute) };

} // FindRoute

//-----------------------------------------------------------------------------
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTPath& Path, url::KQuery& Params) const
//-----------------------------------------------------------------------------
{
	Parameters parms;

	auto& ret = FindRoute(Path, parms);

	// add all variables from the path into the request query
	for (const auto& qp : parms)
	{
		Params->Add(qp.first, qp.second);
	}

	return ret;

} // FindRoute

//-----------------------------------------------------------------------------
bool KRESTServer::Execute(const KRESTRoutes& Routes, KStringView sBaseRoute, OutputType Out, const ResponseHeaders& Headers)
//-----------------------------------------------------------------------------
{
	try
	{
		clear();

		// we output JSON
		Response.Headers.Add(KHTTPHeaders::CONTENT_TYPE, KMIME::JSON);

		// add additional response headers
		for (auto& it : Headers)
		{
			Response.Headers.Add(it.first, it.second);
		}

		if (!KHTTPServer::Parse())
		{
			throw KHTTPError  { KHTTPError::H4xx_BADREQUEST, KHTTPServer::Error() };
		}

		Response.SetStatus(200, "OK");
		Response.sHTTPVersion = Request.sHTTPVersion;
		Response.sHTTPVersion = "HTTP/1.1";

		KStringView sURLPath = Request.Resource.Path;

		// remove_prefix is true when called with empty argument or with matching argument
		if (!sURLPath.remove_prefix(sBaseRoute))
		{
			// bad prefix
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("url does not start with base route: {} <> {}", sBaseRoute, sURLPath) };
		}

		auto Route = Routes.FindRoute(KRESTPath(Request.Method, sURLPath), Request.Resource.Query);

		if (!Route.Callback)
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
		}

		if (Request.HasContent())
		{
			auto& sContentType = Request.Headers.Get(KHTTPHeaders::CONTENT_TYPE);
			if (sContentType.empty() || sContentType == KMIME::JSON)
			{
				// read body in temp buffer and parse into KJSON struct
				KString sBuffer;
				KHTTPServer::Read(sBuffer);
				sBuffer.TrimRight();
				if (!sBuffer.empty())
				{
					// parse the content into json.rx
					KString sError;
					if (!kjson::Parse(json.rx, sBuffer, sError))
					{
						throw KHTTPError { KHTTPError::H4xx_BADREQUEST, sError };
					}
				}
			}
			else
			{
				// read body and store for later access
				KHTTPServer::Read(m_sRequestBody);
			}
		}

		Route.Callback(*this);

		Output(Out);

		return true;
	}

	catch (const std::exception& ex)
	{
		ErrorHandler(ex, Out);
	}

	return false;

} // Execute

#ifdef NDEBUG
	static constexpr int json_pretty { -1 };
#else
	static constexpr int json_pretty { 1 };
#endif

//-----------------------------------------------------------------------------
void KRESTServer::Output(OutputType Out)
//-----------------------------------------------------------------------------
{
	
	switch (Out)
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
					sContent = json.tx.dump(json_pretty, '\t');

					// ensure that all JSON responses end in a newline:
					sContent += '\n';
				}
			}

			// compute and set the Content-Length header:
			Response.Headers.Set(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(sContent.length()));

#ifndef NDEBUG
			// TODO remove or make configurable
			// enable for debug builds only
			Response.Headers.Add ("X-XAPIS-Milliseconds", "1234");
#endif
			// writes full response and headers to output
			Response.Serialize();

			// finally, output the content:
			kDebug(2, "{}", sContent);
			Response.Write(sContent);
		}
		break;

		case LAMBDA:
		{
			// we output JSON
			Response.Headers.Set(KHTTPHeaders::CONTENT_TYPE, KMIME::JSON);

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
			Response.UnfilteredStream() << tjson.dump(json_pretty, '\t') << "\n";

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
					Response.UnfilteredStream() << json.tx.dump(json_pretty, '\t');
				}
			}
			// finish with a linefeed
			Response.UnfilteredStream().WriteLine();
		}
		break;
	}
}

//-----------------------------------------------------------------------------
void KRESTServer::json_t::clear()
//-----------------------------------------------------------------------------
{
	// a .clear() would not erase the json type information (which is IMHO a bug),
	// therefore we assign a fresh object
	rx = KJSON{};
	tx = KJSON{};

}

//-----------------------------------------------------------------------------
void KRESTServer::ErrorHandler(const std::exception& ex, OutputType Out)
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

	switch (Out)
	{
		case HTTP:
		{
			json.tx = KJSON{};
			if (sError.empty())
			{
				json.tx["message"] = Response.sStatusString;
			}
			else
			{
				json.tx["message"] = sError;
			}

			KString sContent = json.tx.dump(json_pretty, '\t');

			// ensure that all JSON responses end in a newline:
			sContent += '\n';

			// compute and set the Content-Length header:
			Response.Headers.Set(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(sContent.length()));
#ifndef NDEBUG
			// TODO remove or make configurable
			// enable for debug builds only
			Response.Headers.Add ("X-XAPIS-Milliseconds", "1234");
#endif
			Response.Headers.Set(KHTTPHeaders::CONNECTION, "close");

			// writes full response and headers to output
			Response.Serialize();

			// finally, output the content:
			kDebug (2, "{}", sContent);
			Response.Write (sContent);
		}
		break;

		case LAMBDA:
		{
			json.tx = KJSON{};
			json.tx["statusCode"] = Response.iStatusCode;
			json.tx["headers"] = KJSON::object();
			for (const auto& header : Response.Headers)
			{
				json.tx["headers"] += { header.first, header.second };
			}
			json.tx["isBase64Encoded"] = false;
			json.tx["body"] = KJSON::object();
			json.tx["body"] += { "message", sError };
			Response.UnfilteredStream() << json.tx.dump(json_pretty, '\t') << "\n";
		}
		break;

		case CLI:
		{
			Response.UnfilteredStream().FormatLine("{}: {}",
												   Dekaf().GetProgName(),
												   sError.empty() ? Response.sStatusString.ToView()
												                  : sError);
		}
		break;
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
