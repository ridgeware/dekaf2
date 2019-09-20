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

#include "krestclient.h"
#include "khttperror.h"
#include "kjson.h"

using namespace dekaf2;

#ifdef NDEBUG
constexpr int iPretty = -1;
#else
constexpr int iPretty = 1;
#endif


//-----------------------------------------------------------------------------
KRestClient::KRestClient(KURL URL, bool bVerifyCerts)
//-----------------------------------------------------------------------------
: m_URL(std::move(URL))
, m_bVerifyCerts(bVerifyCerts)
{
	// check that path ends with a slash
	if (m_URL.Path.get().back() != '/')
	{
		m_URL.Path.get() += '/';
	}
	m_URL.Query.clear();
	m_URL.Fragment.clear();

	// per default, allow proxy configuration through environment
	KHTTPClient::AutoConfigureProxy(true);

} // ctor

//-----------------------------------------------------------------------------
KString KRestClient::Request(KStringView sPath, KStringView sVerb, KStringView sBody, KMIME mime)
//-----------------------------------------------------------------------------
{
	KURL URL { m_URL };
	URL.Path.get() += sPath;

	return KHTTPClient::HttpRequest(URL, sVerb, sBody, mime, m_bVerifyCerts);

} // Request

//-----------------------------------------------------------------------------
KJSON KJsonRestClient::Request (KStringView sPath, KStringView sVerb, const KJSON& json)
//-----------------------------------------------------------------------------
{
	auto sResponse = KRestClient::Request(sPath, sVerb, json.empty() ? "" : json.dump(iPretty), KMIME::JSON);

	KJSON jResponse;
	KString sError;

	if (!kjson::Parse(jResponse, sResponse, sError))
	{
		throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("bad json: {}", sError) };
	}

	if (!Good())
	{
		KString sError;

		if (!Error().empty())
		{
			sError = Error();
			sError += " - ";
		}

		if (m_ErrorCallback)
		{
			sError += m_ErrorCallback(jResponse);

		}

		throw KHTTPError { KHTTPError::H5xx_ERROR, kFormat("{} {}: {}", sVerb, sPath, sError) };
	}
	return jResponse;

} // Request


