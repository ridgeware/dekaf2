/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

#include "kwebclient.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KWebClient::KWebClient(bool bVerifyCerts)
//-----------------------------------------------------------------------------
: KHTTPClient(bVerifyCerts)
{
	AutoConfigureProxy(true);
}

//-----------------------------------------------------------------------------
KString KWebClient::HttpRequest (KURL URL, KStringView sRequestMethod/* = KHTTPMethod::GET*/, KStringView svRequestBody/* = ""*/, KMIME MIME/* = KMIME::JSON*/)
//-----------------------------------------------------------------------------
{
	KString sResponse;

	uint16_t iHadRedirects = 0;

	// placeholder for a web form we may generate from query parms
	KString sWWWForm;

	if (svRequestBody.empty() &&
		!URL.Query.empty() &&
		sRequestMethod != KHTTPMethod::GET &&
		sRequestMethod != KHTTPMethod::OPTIONS &&
		sRequestMethod != KHTTPMethod::HEAD &&
		sRequestMethod != KHTTPMethod::CONNECT)
	{
		// we automatically create a www form as body data if there are
		// query parms but no post data (and the method is not GET)
		URL.Query.Serialize(sWWWForm);
		URL.Query.clear();
		// now point post data into the form
		svRequestBody = sWWWForm;
		MIME = KMIME::WWW_FORM_URLENCODED;
	}

	for(;;)
	{
		if (Connect(URL))
		{
			if (Resource(URL, sRequestMethod))
			{
				if (SendRequest (svRequestBody, MIME))
				{
					Read (sResponse);
				}
			}
		}

		if (!CheckForRedirect(URL, sRequestMethod))
		{
			break;
		}

		if (iHadRedirects++ >= m_iMaxRedirects)
		{
			SetError(kFormat("number of redirects ({}) exceeds max redirection limit of {}",
							 iHadRedirects,
							 m_iMaxRedirects));
			break;
		}
		// else loop into the redirection
	}

	if (!HttpSuccess() && Error().empty())
	{
		SetError(Response.GetStatusString());

		if (svRequestBody)
		{
			kDebug(2, "{} {}\n{}", sRequestMethod, URL.KResource::Serialize(), svRequestBody);
		}

		kDebug(2, "{} {} from URL {}", Response.iStatusCode, Response.sStatusString, URL.Serialize());

		if (!sResponse.empty())
		{
			kDebug(2, "{}", sResponse);
		}
	}

	return sResponse;

} // HttpRequest

//-----------------------------------------------------------------------------
KString kHTTPGet(KURL URL)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP(/* bVerifyCerts = */ true);
	return HTTP.Get(std::move(URL));

} // kHTTPGet

//-----------------------------------------------------------------------------
bool kHTTPHead(KURL URL)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP(/* bVerifyCerts = */ true);
	return HTTP.Head(std::move(URL));

} // kHTTPHead

//-----------------------------------------------------------------------------
KString kHTTPPost(KURL URL, KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP(/* bVerifyCerts = */ true);
	return HTTP.Post(std::move(URL), svPostData, svMime);

} // kHTTPPost

} // end of namespace dekaf2
