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

namespace dekaf2 {

#ifdef NDEBUG
constexpr int iPretty = -1;
#else
constexpr int iPretty = 1;
#endif

//-----------------------------------------------------------------------------
KRestClient::KRestClient()
//-----------------------------------------------------------------------------
{
	// per default, allow proxy configuration through environment
	KHTTPClient::AutoConfigureProxy(true);

} // ctor

//-----------------------------------------------------------------------------
KRestClient::KRestClient(KURL URL, bool bVerifyCerts)
//-----------------------------------------------------------------------------
: KRestClient()
{
	SetURL(std::move(URL), bVerifyCerts);

} // ctor

//-----------------------------------------------------------------------------
KRestClient& KRestClient::SetURL(KURL URL, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	m_URL = std::move(URL);

	// check that path ends with a slash
	if (m_URL.Path.get().back() != '/')
	{
		m_URL.Path.get() += '/';
	}
	// do not clear the query part - we will use it if it is set

	m_URL.Fragment.clear();

	KHTTPClient::VerifyCerts(bVerifyCerts);

	return *this;

} // SetURL

//-----------------------------------------------------------------------------
void KRestClient::clear()
//-----------------------------------------------------------------------------
{
	m_sVerb.clear();
	m_sPath.clear();
	m_Query.clear();
	m_ec = nullptr;
	m_bNeedReset = false;
	base::clear();

} // clear

//-----------------------------------------------------------------------------
void KRestClient::ResetAfterRequest()
//-----------------------------------------------------------------------------
{
	if (m_bNeedReset)
	{
		clear();
	}
}

//-----------------------------------------------------------------------------
KRestClient& KRestClient::SetError(KHTTPError& ec)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	m_ec = &ec;
	m_ec->clear();
	return *this;

} // SetError

//-----------------------------------------------------------------------------
KRestClient& KRestClient::Verb(KHTTPMethod sVerb)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	m_sVerb = std::move(sVerb);
	return *this;

} // Verb

//-----------------------------------------------------------------------------
KRestClient& KRestClient::Path(KString sPath)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	m_sPath = std::move(sPath);
	return *this;

} // Path

//-----------------------------------------------------------------------------
KRestClient& KRestClient::SetQuery(url::KQuery Query)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	m_Query = std::move(Query);
	return *this;

} // SetQuery

//-----------------------------------------------------------------------------
KRestClient& KRestClient::AddQuery(url::KQuery Query)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	m_Query += std::move(Query);
	return *this;

} // AddQuery

//-----------------------------------------------------------------------------
KRestClient& KRestClient::AddQuery(KString sName, KString sValue)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
 	m_Query.get().Add(std::move(sName), std::move(sValue));
	return *this;

} // AddQuery

//-----------------------------------------------------------------------------
KRestClient& KRestClient::AddHeader (KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	base::AddHeader(sName, sValue);
	return *this;

} // RestAddHeader

//-----------------------------------------------------------------------------
KString KRestClient::NoExceptRequest (KStringView sBody, KMIME mime) noexcept
//-----------------------------------------------------------------------------
{
	KURL URL { m_URL };
	URL.Path.get() += m_sPath;
	URL.Query += m_Query;
	m_bNeedReset = true;

	return KWebClient::HttpRequest(URL, m_sVerb, sBody, mime);

} // NoExceptRequest

//-----------------------------------------------------------------------------
KString KRestClient::Request (KStringView sBody, KMIME mime)
//-----------------------------------------------------------------------------
{
	auto sResponse = NoExceptRequest(sBody, mime);

	if (!HttpSuccess())
	{
		KString sError;

		if (!Error().empty())
		{
			sError = Error();
		}

		return ThrowOrReturn (KHTTPError { GetStatusCode(), kFormat("{} {}: HTTP-{} {} from {}", m_sVerb.Serialize(), m_sPath, GetStatusCode(), sError, m_URL.Serialize()) }, std::move(sResponse));
	}

	return sResponse;

} // Request

//-----------------------------------------------------------------------------
KString KRestClient::Request (const KMIMEMultiPart& MultiPart)
//-----------------------------------------------------------------------------
{
	return Request(MultiPart.Serialize(true), MultiPart.ContentType());

} // Request

//-----------------------------------------------------------------------------
KString KRestClient::ThrowOrReturn(KHTTPError&& ec, KString&& retval)
//-----------------------------------------------------------------------------
{
	if (m_ec)
	{
		*m_ec = std::move(ec);
		return std::move(retval);
	}
	else
	{
		throw std::move(ec);
	}

} // ThrowOrReturn

//-----------------------------------------------------------------------------
KJSON KJsonRestClient::RequestAndParseResponse (KStringView sRequest, KMIME Mime)
//-----------------------------------------------------------------------------
{
	KString sResponse = KRestClient::NoExceptRequest(sRequest, Mime);

	KJSON jResponse;
	KString sError;

	if (!kjson::Parse(jResponse, sResponse, sError))
	{
		if (HttpSuccess())
		{
			// only throw on bad JSON if this is a 200 response, else return the
			// primary error
			return ThrowOrReturn (KHTTPError { KHTTPError::H5xx_ERROR, kFormat("bad rx json: {}", sError) });
		}
	}

	if (!HttpSuccess())
	{
		KString sError;

		if (!Error().empty())
		{
			sError = Error();
		}

		if (m_ErrorCallback)
		{
			if (!sError.empty())
			{
				sError += " - ";
			}

			sError += m_ErrorCallback(jResponse);
		}

		return ThrowOrReturn (KHTTPError { GetStatusCode(), kFormat("{} {}: HTTP-{} {} from {}", m_sVerb.Serialize(), m_sPath, GetStatusCode(), sError, m_URL.Serialize()) }, std::move(jResponse));
	}

	return jResponse;

} // RequestAndParseResponse

//-----------------------------------------------------------------------------
KJSON KJsonRestClient::Request (const KJSON& json, KMIME Mime)
//-----------------------------------------------------------------------------
{
	try
	{
		return RequestAndParseResponse(json.empty() ? "" : json.dump(iPretty), Mime);
	}
	catch (const KJSON::exception& ex)
	{
		return ThrowOrReturn (KHTTPError { KHTTPError::H5xx_ERROR, kFormat("bad tx json: {}", ex.what()) });
	}

} // Request

//-----------------------------------------------------------------------------
KJSON KJsonRestClient::Request (const KMIMEMultiPart& MultiPart)
//-----------------------------------------------------------------------------
{
	return RequestAndParseResponse(MultiPart.Serialize(true), MultiPart.ContentType());

} // Request

//-----------------------------------------------------------------------------
KJSON KJsonRestClient::ThrowOrReturn(KHTTPError&& ec, KJSON&& retval)
//-----------------------------------------------------------------------------
{
	if (m_ec)
	{
		*m_ec = std::move(ec);
		return std::move(retval);
	}
	else
	{
		throw std::move(ec);
	}

} // ThrowOrReturn

} // end of namespace dekaf2
