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

#include "khttprouter.h"
#include "khttperror.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPRoute::KHTTPRoute(KString _sRoute, KString _sDocumentRoot, HTTPCallback _Callback)
//-----------------------------------------------------------------------------
	: detail::KHTTPAnalyzedPath(std::move(_sRoute))
	, Callback((!_Callback && !_sDocumentRoot.empty()) ? &KHTTPRoute::WebServer : std::move(_Callback))
	, sDocumentRoot(std::move(_sDocumentRoot))
{
}

//-----------------------------------------------------------------------------
void KHTTPRoute::WebServer(KHTTPRouter& HTTP)
//-----------------------------------------------------------------------------
{
	if (HTTP.Request.Method != KHTTPMethod::GET)
	{
		kDebug(1, "invalid method: {}", HTTP.Request.Method.Serialize());
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, kFormat("invalid method: {}", HTTP.Request.Method.Serialize()) };
	}

	KStringView sResource = HTTP.Request.Resource.Path.get();

	if (!sResource.remove_prefix(HTTP.Route->sRoute) || sResource.front() != '/')
	{
		kDebug(1, "invalid document path (internal error): {}", sResource);
		throw KHTTPError { KHTTPError::H5xx_ERROR, "invalid path" };
	}

	sResource.remove_prefix(1); // the leading slash

	if (!kIsSafePathname(sResource))
	{
		kDebug(1, "invalid document path: {}", sResource);
		throw KHTTPError { KHTTPError::H4xx_BADREQUEST, "invalid path" };
	}

	KString sFileSystemPath = HTTP.Route->sDocumentRoot;
	sFileSystemPath += kDirSep;
	sFileSystemPath += sResource;

	auto MIME = KMIME::CreateByExtension(sFileSystemPath);

	if (MIME == KMIME::NONE)
	{
		MIME = KMIME::CreateByInspection(sFileSystemPath, KMIME::BINARY);
	}

	HTTP.Response.Headers.Add(KHTTPHeader::CONTENT_TYPE, MIME);

//	TBD if (!HTTP.SetFileToOutput(sFileSystemPath))
	{
		kDebug(1, "file not found: {}", sFileSystemPath);
		throw KHTTPError { KHTTPError::H4xx_NOTFOUND, "file not found" };
	}

} // WebServer

//-----------------------------------------------------------------------------
bool KHTTPRoutes::AddRoute(KHTTPRoute _Route)
//-----------------------------------------------------------------------------
{
	m_Routes.push_back(std::move(_Route));
	return true;

} // AddRoute

//-----------------------------------------------------------------------------
void KHTTPRoutes::SetDefaultRoute(KHTTPRoute::HTTPCallback Callback)
//-----------------------------------------------------------------------------
{
	m_DefaultRoute.Callback = Callback;

} // SetDefaultRoute

//-----------------------------------------------------------------------------
void KHTTPRoutes::clear()
//-----------------------------------------------------------------------------
{
	m_Routes.clear();
	m_DefaultRoute.Callback = nullptr;

} // clear

//-----------------------------------------------------------------------------
const KHTTPRoute& KHTTPRoutes::FindRoute(const KHTTPPath& Path) const
//-----------------------------------------------------------------------------
{
	for (const auto& it : m_Routes)
	{
		if (!it.bHasWildCardFragment)
		{
			if (DEKAF2_UNLIKELY(it.bHasWildCardAtEnd))
			{
				// this is a plain route with a wildcard at the end
				if (DEKAF2_UNLIKELY(Path.sRoute.starts_with(it.sRoute)))
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
			// we have wildcard fragments, check part by part of the route
			if (it.vURLParts.size() >= Path.vURLParts.size())
			{
				auto req = Path.vURLParts.cbegin();
				bool bFound { true };
				bool bEndOfPath { false };

				for (auto& part : it.vURLParts)
				{
					if (DEKAF2_UNLIKELY(bEndOfPath))
					{
						// route was longer than path
						bFound = false;
						break;
					}

					if (DEKAF2_LIKELY(part != *req))
					{
						if (DEKAF2_LIKELY(part != "*"))
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
						// end of Path reached, check that no route fragment is missing
						bEndOfPath = true;
					}
				}

				if (bFound)
				{
					return it;
				}
			}
		}
	}

	if (m_DefaultRoute.Callback)
	{
		return m_DefaultRoute;
	}

	// no match at all
	kDebug (2, "invalid path: {}", Path.sRoute);
	throw KHTTPError { KHTTPError::H4xx_NOTFOUND, kFormat("invalid path {}", Path.sRoute) };

} // FindRoute

//-----------------------------------------------------------------------------
KHTTPRouter::~KHTTPRouter()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KHTTPRouter::Execute(const KHTTPRoutes& Routes, KStringView sBaseRoute)
//-----------------------------------------------------------------------------
{
	try
	{
		Response.clear();
		Route = nullptr;

		if (!KHTTPServer::Parse())
		{
			throw KHTTPError  { KHTTPError::H4xx_BADREQUEST, KHTTPServer::Error() };
		}

		Response.SetStatus(200, "OK");
		Response.sHTTPVersion = Request.sHTTPVersion;

		KStringView sURLPath = Request.Resource.Path;

		// try to remove_prefix, do not complain if not existing
		sURLPath.remove_prefix(sBaseRoute);

		// try to remove a trailing / - we treat /path and /path/ as the same address
		sURLPath.remove_suffix("/");

		KHTTPPath RequestedPath(sURLPath);

		Route = &Routes.FindRoute(RequestedPath);

		if (!Route->Callback)
		{
			throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("empty callback for {}", sURLPath) };
		}
		else
		{
			Route->Callback(*this);
		}

		return true;
	}

	catch (const std::exception& ex)
	{
		ErrorHandler(ex);
	}

	return false;

} // Execute

//-----------------------------------------------------------------------------
void KHTTPRouter::ErrorHandler(const std::exception& ex)
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

	KString sContent = R"(<html><head>Error</head><body><h2>)";

	if (sError.empty())
	{
		sContent += Response.sStatusString;
	}
	else
	{
		sContent += sError;
	}

	// close the html body
	sContent += R"(</h2></body></html>\r\n)";

	// compute and set the Content-Length header:
	Response.Headers.Set(KHTTPHeader::CONTENT_LENGTH, KString::to_string(sContent.length()));
	Response.Headers.Set(KHTTPHeader::CONTENT_TYPE, KMIME::HTML_UTF8);
	Response.Headers.Set(KHTTPHeader::CONNECTION, "close");

	// write full response and headers to output
	Response.Serialize();

	// finally, output the content:
	kDebug (2, sContent);
	Response.Write (sContent);

} // ErrorHandler

} // end of namespace dekaf2
