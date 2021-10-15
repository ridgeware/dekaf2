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

	// do not clear the query part - we will use it if it is set
	m_URL.Fragment.clear();

	KHTTPClient::VerifyCerts(bVerifyCerts);

	return *this;

} // SetURL

//-----------------------------------------------------------------------------
void KRestClient::clear()
//-----------------------------------------------------------------------------
{
	m_Verb.clear();
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

} // ResetAfterRequest

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
KRestClient& KRestClient::Verb(KHTTPMethod Verb)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	m_Verb = Verb;
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
KRestClient& KRestClient::AddHeader (KHTTPHeader Header, KStringView sValue)
//-----------------------------------------------------------------------------
{
	ResetAfterRequest();
	base::AddHeader(std::move(Header), sValue);
	return *this;

} // AddHeader

//-----------------------------------------------------------------------------
bool KRestClient::NoExceptRequest (KOutStream& OutStream, KStringView sBody, const KMIME& mime) noexcept
//-----------------------------------------------------------------------------
{
	if (m_URL.Protocol != url::KProtocol::UNIX)
	{
		KURL URL { m_URL };

		if (!m_sPath.empty())
		{
			if (m_sPath.front() != '/')
			{
				if (URL.Path.get().back() != '/')
				{
					URL.Path.get() += '/';
				}
			}

			URL.Path.get() += m_sPath;
		}

		if (!m_Query.empty())
		{
			URL.Query += m_Query;
		}
		
		m_bNeedReset = true;

		return KWebClient::HttpRequest(OutStream, URL, m_Verb, sBody, mime);
	}
	else
	{
		// for unix socket connections we technically need two URLs,
		// one with the socket path (which is a file system path) and
		// one with the request path (which is a URL path)
		KURL RequestURL;
		RequestURL.Domain.get() = "localhost";
		RequestURL.Path.get() = '/';
		RequestURL.Path.get() += m_sPath;
		RequestURL.Query += m_Query;
		m_bNeedReset = true;

		return KWebClient::HttpRequest(OutStream, m_URL, RequestURL, m_Verb, sBody, mime);
	}

} // NoExceptRequest

//-----------------------------------------------------------------------------
bool KRestClient::Request (KOutStream& OutStream, KStringView sBody, const KMIME& mime)
//-----------------------------------------------------------------------------
{
	if (!NoExceptRequest(OutStream, sBody, mime))
	{
		KString sError;

		if (!Error().empty())
		{
			sError = Error();
		}

		return ThrowOrReturn (KHTTPError { GetStatusCode(), kFormat("{} {}: HTTP-{} {} from {}", m_Verb.Serialize(), m_sPath, GetStatusCode(), sError, m_URL.Serialize()) });
	}

	return true;

} // Request

//-----------------------------------------------------------------------------
bool KRestClient::Request (KOutStream& OutStream, const KMIMEMultiPart& MultiPart)
//-----------------------------------------------------------------------------
{
	return Request(OutStream, MultiPart.Serialize(true), MultiPart.ContentType());

} // Request

//-----------------------------------------------------------------------------
KString KRestClient::Request (KStringView sBody, const KMIME& mime)
//-----------------------------------------------------------------------------
{
	KString sResponse;
	KOutStringStream oss(sResponse);
	Request(oss, sBody, mime);
	return sResponse;

} // Request

//-----------------------------------------------------------------------------
KString KRestClient::Request (const KMIMEMultiPart& MultiPart)
//-----------------------------------------------------------------------------
{
	return Request(MultiPart.Serialize(true), MultiPart.ContentType());

} // Request

//-----------------------------------------------------------------------------
bool KRestClient::ThrowOrReturn(KHTTPError&& ec, bool bRetval)
//-----------------------------------------------------------------------------
{
	if (m_ec)
	{
		*m_ec = std::move(ec);
		return bRetval;
	}
	else
	{
		throw std::move(ec);
	}

} // ThrowOrReturn

//-----------------------------------------------------------------------------
KString KJsonRestClient::DefaultErrorCallback(const KJSON& jResponse, KStringView sErrorProperties)
//-----------------------------------------------------------------------------
{
	KString sError;

	// our default message parse

	KJSON::const_iterator it = jResponse.end();

	for (auto sProp : sErrorProperties.Split())
	{
		it = jResponse.find(sProp);

		if (it != jResponse.end() && !it->empty())
		{
			kDebug(2, "found value for error property: {}", sProp);
			// found one
			break;
		}
	}

	if (it != jResponse.end())
	{
		if (it->is_array() && it->size() == 1)
		{
			sError = kjson::Print(*it->begin());
		}
		else
		{
			sError = kjson::Print(*it);
		}
		kDebug(1, sError);
	}

	return sError;

} // DefaultErrorCallback

//-----------------------------------------------------------------------------
KJSON KJsonRestClient::RequestAndParseResponse (KStringView sRequest, const KMIME& Mime)
//-----------------------------------------------------------------------------
{
	KString sResponse;
	KOutStringStream oss(sResponse);
	KRestClient::NoExceptRequest(oss, sRequest, Mime);

	KJSON jResponse;
	KString sError;
	bool bBadJson { false };

	if (!kjson::Parse(jResponse, sResponse, sError))
	{
		if (HttpSuccess())
		{
			// only throw on bad JSON if this is a 200 response, else return the
			// primary error
			return ThrowOrReturn (KHTTPError { KHTTPError::H5xx_ERROR, kFormat("bad rx json: {}", sError) });
		}

		bBadJson = true;
	}

	if (!HttpSuccess())
	{
		KString sError;

		if (!Error().empty())
		{
			sError = Error();
		}

		if (!bBadJson)
		{
			KString sDetailedError;

			if (m_ErrorCallback)
			{
				sDetailedError = m_ErrorCallback(jResponse);
			}
			else
			{
				sDetailedError = DefaultErrorCallback(jResponse);
			}

			sDetailedError.Trim();

			if (!sDetailedError.empty())
			{
				if (!sError.empty())
				{
					sError += ", ";
				}
				sError += sDetailedError;
			}
		}

		return ThrowOrReturn (KHTTPError { GetStatusCode(), kFormat("{} {}: HTTP-{} {} from {}", m_Verb.Serialize(), m_sPath, GetStatusCode(), sError, m_URL.Serialize()) }, std::move(jResponse));
	}

	return jResponse;

} // RequestAndParseResponse

//-----------------------------------------------------------------------------
KJSON KJsonRestClient::Request (const KJSON& json, const KMIME& Mime)
//-----------------------------------------------------------------------------
{
	try
	{
		return RequestAndParseResponse(json.empty() ? KString{} : json.dump(iPretty), Mime);
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
bool KJsonRestClient::Request (KOutStream& OutStream, const KJSON& json, const KMIME& Mime)
//-----------------------------------------------------------------------------
{
	try
	{
		return KRestClient::NoExceptRequest(OutStream, json.empty() ? "" : json.dump(iPretty), Mime);
	}
	catch (const KJSON::exception& ex)
	{
		ThrowOrReturn (KHTTPError { KHTTPError::H5xx_ERROR, kFormat("bad tx json: {}", ex.what()) });
		return false;
	}

} // Request

//-----------------------------------------------------------------------------
bool KJsonRestClient::Request (KOutStream& OutStream, const KMIMEMultiPart& MultiPart)
//-----------------------------------------------------------------------------
{
	return KRestClient::NoExceptRequest(OutStream, MultiPart.Serialize(true), MultiPart.ContentType());

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
