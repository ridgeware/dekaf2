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
KRESTRoute::KRESTRoute(KHTTPMethod _Method, KString _sRoute, RESTCallback _Callback)
//-----------------------------------------------------------------------------
	: Method(_Method)
	, sRoute(_sRoute)
	, Callback(_Callback)
{
	bHasParameters = sRoute.Contains("/:");
	SplitURL(vURLParts, sRoute);

} // KRESTRoute

//-----------------------------------------------------------------------------
size_t KRESTRoute::SplitURL(URLParts& Parts, KStringView sURLPath)
//-----------------------------------------------------------------------------
{
	Parts.clear();
	Parts.reserve(2);
	sURLPath.remove_prefix("/");
	return kSplit(Parts, sURLPath, "/", "");

} // SplitURL

//-----------------------------------------------------------------------------
bool KRESTRoutes::AddRoute(const KRESTRoute& _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(_Route);
	return true;

} // AddRoute

//-----------------------------------------------------------------------------
bool KRESTRoutes::AddRoute(KRESTRoute&& _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(std::move(_Route));
	return true;

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
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTRoute& Route, Parameters& Params) const
//-----------------------------------------------------------------------------
{
	for (const auto& it : m_Routes)
	{
		if (it.Method.empty() || it.Method == Route.Method)
		{
			if (!it.bHasParameters)
			{
				// this is a plain route - we do not check part by part
				if (DEKAF2_UNLIKELY(Route.sRoute.StartsWith(it.sRoute)))
				{
					// now check if this is a full match or if the match ends in a slash
					if (it.sRoute.size() == Route.sRoute.size() || Route.sRoute[it.sRoute.size()] == '/')
					{
						return it;
					}
				}
			}
			else
			{
				// we have parameters, check part for part of the route
				if (it.vURLParts.size() <= Route.vURLParts.size())
				{
					Params.clear();
					auto req = Route.vURLParts.cbegin();
					bool bFound { true };

					for (auto part : it.vURLParts)
					{
						if (DEKAF2_LIKELY(part != *req))
						{
							if (DEKAF2_UNLIKELY(part.front() == ':'))
							{
								// this is a variable
								auto sName = part;
								// remove the colon
								sName.remove_prefix(1);
								// and add the value to our temporary query parms
								Params.push_back({sName, *req});
							}
							else
							{
								// not found
								bFound = false;
								break;
							}
						}
						// found, continue comparison
						++req;
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

	throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("unknown address {}", Route.sRoute) };

} // FindRoute

//-----------------------------------------------------------------------------
const KRESTRoute& KRESTRoutes::FindRoute(const KRESTRoute& Route, url::KQuery& Params) const
//-----------------------------------------------------------------------------
{
	Parameters parms;

	auto& ret = FindRoute(Route, parms);

	// add all variables from the path into the request query
	for (const auto& qp : parms)
	{
		Params->Add(qp.first, qp.second);
	}

	return ret;

} // FindRoute

//-----------------------------------------------------------------------------
bool KRESTServer::Execute(const KRESTRoutes& Routes, KStringView sBaseRoute, OutputType Out)
//-----------------------------------------------------------------------------
{
	try
	{
		json.clear();
		Response.clear();
		Response.SetStatus(200, "OK");
		Response.sHTTPVersion = Request.sHTTPVersion;

		KStringView sURLPath = Request.Resource.Path;

		// remove_prefix is true when called with empty argument or with matching argument
		if (!sURLPath.remove_prefix(sBaseRoute))
		{
			// bad prefix
			throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("url does not start with {}", sBaseRoute) };
		}

		auto Route = Routes.FindRoute(KRESTRoute(Request.Method, sURLPath), Request.Resource.Query);

		if (!Route.Callback)
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
		}

		if (Request.Headers.Get(KHTTPHeaders::CONTENT_TYPE) == KMIME::JSON)
		{
			// parse the content into json.rx
			KString sError;
			if (!kjson::Parse(json.rx, Request.FilteredStream(), sError))
			{
				throw KHTTPError { KHTTPError::H4xx_BADREQUEST, sError };
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
				// we output JSON
				Response.Headers.Set(KHTTPHeaders::CONTENT_TYPE, KMIME::JSON);

				// the content:
				if (!m_sMessage.empty())
				{
					json.tx["message"] = std::move(m_sMessage);
				}

				sContent = json.tx.dump(json_pretty, '\t');

				// ensure that all JSON responses end in a newline:
				sContent += '\n';
			}

			// compute and set the Content-Length header:
			Response.Headers.Set(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(sContent.length()));

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
					tjson["message"] = std::move(m_sMessage);
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
				Response.UnfilteredStream() << json.tx.dump(json_pretty, '\t');
			}
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

	SetStatus( xex ? xex->GetRawStatusCode() : KHTTPError::H5xx_ERROR);

	KStringView sError = ex.what();

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
			Response.Headers.Set(KHTTPHeaders::CONNECTION, "close");

			// writes full response and headers to output
			Response.Serialize();

			// finally, output the content:
			kDebug (1, "{}", sContent);
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

} // end of namespace dekaf2
