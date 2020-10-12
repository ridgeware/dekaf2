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
#include "khttperror.h"
#include "ktimer.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
bool KWget (KStringView sURL, const KString& sOutfile, const KJSON& Options/*=KJSON{}*/)
//-----------------------------------------------------------------------------
{
	// TODO: someday support wget's cli interface in Options:
	//   {
	//       {"-s", ""},
	//       {"-k", ""},
	//       etc.
	//   }

	KWebClient http;
	KString sResponse = http.HttpRequest (sURL);
	if (http.HttpFailure())
	{
		return false;
	}

	if (!kWriteFile (sOutfile, sResponse))
	{
		return false;
	}

	return true;

} // KWget

//-----------------------------------------------------------------------------
KWebClient::KWebClient(bool bVerifyCerts/*=false*/)
//-----------------------------------------------------------------------------
: KHTTPClient(bVerifyCerts)
{
	AutoConfigureProxy(true);
}

//-----------------------------------------------------------------------------
bool KWebClient::HttpRequest (KOutStream& OutStream, KURL URL, KHTTPMethod RequestMethod/* = KHTTPMethod::GET*/, KStringView svRequestBody/* = ""*/, KMIME MIME/* = KMIME::JSON*/)
//-----------------------------------------------------------------------------
{
	uint16_t iHadRedirects = 0;

	// placeholder for a web form we may generate from query parms
	KString sWWWForm;

	if (m_bQueryToWWWFormConversion &&
		svRequestBody.empty() &&
		!URL.Query.empty() &&
		RequestMethod != KHTTPMethod::GET &&
		RequestMethod != KHTTPMethod::OPTIONS &&
		RequestMethod != KHTTPMethod::HEAD &&
		RequestMethod != KHTTPMethod::CONNECT)
	{
		// we automatically create a www form as body data if there are
		// query parms but no post data (and the method is not GET)
		URL.Query.Serialize(sWWWForm);
		URL.Query.clear();
		// now point post data into the form
		svRequestBody = sWWWForm;
		MIME = KMIME::WWW_FORM_URLENCODED;
		kDebug(2, "created urlencoded form body from query parms");
	}

	KStopWatch TransferTime(KStopWatch::Halted);
	KStopWatch ConnectTime(KStopWatch::Halted);
	uint16_t iRetry { 0 };

	for(;;)
	{
		ConnectTime.resume();

		bool bReuseConnection = AlreadyConnected(URL);

		if (bReuseConnection || Connect(URL))
		{
			ConnectTime.halt();

			if (Resource(URL, RequestMethod))
			{
				if (m_bAcceptCookies)
				{
					// remove any cookie header if set
					Request.Headers.Remove(KHTTPHeaders::COOKIE);

					KString sCookie = m_Cookies.Serialize(URL);

					if (!sCookie.empty())
					{
						AddHeader(KHTTPHeaders::COOKIE, sCookie);
					}
				}

				TransferTime.resume();
				
				if (SendRequest (svRequestBody, MIME))
				{
					Read (OutStream);

					TransferTime.halt();
				}
				else
				{
					TransferTime.halt();

					if (Response.Fail())
					{
						// check for error 598 - NETWORK READ/WRITE ERROR,
						// allow one retry, but only if it was a reused connection
						if (Response.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT &&
							!iRetry++ &&
							bReuseConnection)
						{
							kDebug(2, "retrying connection");
							// allow one connection retry - we might have run into
							// a keepalive connection and the other end shut it down
							// just before we sent data
							continue;
						}
					}
				}
			}
			else
			{
				ConnectTime.halt();
			}
		}

		if (!CheckForRedirect(URL, RequestMethod))
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

	auto iConnectTime  = ConnectTime.elapsed<std::chrono::milliseconds>();
	auto iTransferTime = TransferTime.elapsed<std::chrono::milliseconds>();
	auto iTotalTime    = iConnectTime.count() + iTransferTime.count();

	kDebug(2, "connect {} ms, transfer {} ms, total {} ms", iConnectTime.count(), iTransferTime.count(), iConnectTime.count() + iTransferTime.count());

	if (m_iWarnIfOverMilliseconds &&
		m_TimingCallback &&
		iTotalTime > m_iWarnIfOverMilliseconds)
	{
		KString sSummary = kFormat ("{}: {}, took {} msecs (connect {}, transfer {})",
			RequestMethod.Serialize(),
			URL.Serialize(),
			kFormNumber (iTotalTime),
			kFormNumber (iConnectTime.count()),
			kFormNumber (iTransferTime.count()));
		m_TimingCallback (*this, iTotalTime, sSummary);
	}

	if (m_pServiceSummary)
	{
		if (!kjson::Exists (*m_pServiceSummary, "http"))
		{
			(*m_pServiceSummary)["http"] = KJSON::array();
		}
		(*m_pServiceSummary)["http"] += {
			{ "request_method",  RequestMethod.Serialize()  },
			{ "url",             URL.Serialize()            },
			{ "bytes_body",      svRequestBody.size()       },
			{ "msecs_connect",   iConnectTime.count()       },
			{ "msecs_transfer",  iTransferTime.count()      },
			{ "msecs_total",     iTotalTime                 },
			{ "response_code",   Response.GetStatusCode()   },
			{ "response_string", Response.GetStatusString() }
		};
	}

	if (!HttpSuccess() && Error().empty())
	{
		SetError(Response.GetStatusString());

		if (svRequestBody)
		{
			kDebug(2, "{} {}\n{}", RequestMethod.Serialize(), URL.KResource::Serialize(), svRequestBody);
		}

		kDebug(2, "{} {} from URL {}", Response.iStatusCode, Response.sStatusString, URL.Serialize());
	}

	if (m_bAcceptCookies && HttpSuccess())
	{
		// check for Set-Cookie headers
		const auto Range = Response.Headers.equal_range(KHTTPHeaders::SET_COOKIE);

		for (auto it = Range.first; it != Range.second; ++it)
		{
			// add each cookie
			m_Cookies.Parse(URL, it->second);
		}
	}

	return HttpSuccess();

} // HttpRequest

//-----------------------------------------------------------------------------
KString KWebClient::HttpRequest (KURL URL, KHTTPMethod RequestMethod/* = KHTTPMethod::GET*/, KStringView svRequestBody/* = ""*/, KMIME MIME/* = KMIME::JSON*/)
//-----------------------------------------------------------------------------
{
	KString sResponse;
	KOutStringStream oss(sResponse);
	HttpRequest(oss, URL, RequestMethod, svRequestBody, MIME);

	if (!sResponse.empty())
	{
		kDebug(2, sResponse);
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
KString kHTTPPost(KURL URL, KStringView svPostData, KMIME Mime)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP(/* bVerifyCerts = */ true);
	return HTTP.Post(std::move(URL), svPostData, std::move(Mime));

} // kHTTPPost

} // end of namespace dekaf2
