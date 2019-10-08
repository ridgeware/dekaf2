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
KRestClient::KRestClient(KURL URL, bool bVerifyCerts)
//-----------------------------------------------------------------------------
: KWebClient(bVerifyCerts)
, m_URL(std::move(URL))
{
	// check that path ends with a slash
	if (m_URL.Path.get().back() != '/')
	{
		m_URL.Path.get() += '/';
	}
	// do not clear the query part - we will use it if it is set

	m_URL.Fragment.clear();

	// per default, allow proxy configuration through environment
	KHTTPClient::AutoConfigureProxy(true);

} // ctor

//-----------------------------------------------------------------------------
void KRestClient::clear()
//-----------------------------------------------------------------------------
{
	m_sVerb.clear();
	m_sPath.clear();
	m_Query.clear();
	m_ec = nullptr;
	base::clear();

} // clear

//-----------------------------------------------------------------------------
KRestClient& KRestClient::Verb(KString sVerb)
//-----------------------------------------------------------------------------
{
	m_sVerb = std::move(sVerb);
	return *this;

} // Verb

//-----------------------------------------------------------------------------
KRestClient& KRestClient::Path(KString sPath)
//-----------------------------------------------------------------------------
{
	m_sPath = std::move(sPath);
	return *this;

} // Path

//-----------------------------------------------------------------------------
KRestClient& KRestClient::SetQuery(url::KQuery Query)
//-----------------------------------------------------------------------------
{
	m_Query = std::move(Query);
	return *this;

} // SetQuery

//-----------------------------------------------------------------------------
KRestClient& KRestClient::AddQuery(url::KQuery Query)
//-----------------------------------------------------------------------------
{
	for (auto& it : Query.get())
	{
		m_Query.get().Add(std::move(it.first), std::move(it.second));
	}
	return *this;

} // AddQuery

//-----------------------------------------------------------------------------
KRestClient& KRestClient::AddQuery(KString sName, KString sValue)
//-----------------------------------------------------------------------------
{
	m_Query.get().Add(std::move(sName), std::move(sValue));
	return *this;

} // AddQuery

//-----------------------------------------------------------------------------
KString KRestClient::NoExceptRequest (KStringView sBody, KMIME mime) noexcept
//-----------------------------------------------------------------------------
{
	KURL URL { m_URL };
	URL.Path.get() += m_sPath;

	for (const auto& it : m_Query.get())
	{
		URL.Query.get().Add(it.first, it.second);
	}

	auto sResponse = KWebClient::HttpRequest(URL, m_sVerb, sBody, mime);

	clear();

	return sResponse;

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

		return ThrowOrReturn (KHTTPError { GetStatusCode(), kFormat("{} {}: HTTP-{} {}", m_sVerb, m_sPath, GetStatusCode(), sError) }, std::move(sResponse));
	}

	return sResponse;

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
KJSON KJsonRestClient::Request (const KJSON& json, KMIME Mime)
//-----------------------------------------------------------------------------
{
	KString sResponse;

	try
	{
		sResponse = KRestClient::NoExceptRequest(json.empty() ? "" : json.dump(iPretty), Mime);
	}
	catch (const KJSON::exception& ex)
	{
		return ThrowOrReturn (KHTTPError { KHTTPError::H5xx_ERROR, kFormat("bad tx json: {}", ex.what()) });
	}

	KJSON jResponse;
	KString sError;

	if (!kjson::Parse(jResponse, sResponse, sError))
	{
		return ThrowOrReturn (KHTTPError { KHTTPError::H5xx_ERROR, kFormat("bad rx json: {}", sError) });
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

		return ThrowOrReturn (KHTTPError { GetStatusCode(), kFormat("{} {}: HTTP-{} {}", m_sVerb, m_sPath, GetStatusCode(), sError) }, std::move(jResponse));
	}

	return jResponse;

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
