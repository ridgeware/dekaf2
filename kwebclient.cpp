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
#include "kduration.h"
#include "kstringutils.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KWget (KStringView sURL, KStringViewZ sOutfile, const KJSON& Options/*=KJSON{}*/)
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
KWebClient::KWebClient(KHTTPStreamOptions Options /*=KHTTPStreamOptions{}*/)
//-----------------------------------------------------------------------------
: KHTTPClient(Options)
{
	AutoConfigureProxy(true);
}

//-----------------------------------------------------------------------------
bool KWebClient::HttpRequest2Host (KOutStream& OutStream, const KURL& HostURL, KURL RequestURL, KHTTPMethod RequestMethod/* = KHTTPMethod::GET*/, KStringView sRequestBody/* = ""*/, KMIME MIME/* = KMIME::JSON*/)
//-----------------------------------------------------------------------------
{
	// placeholder for a web form we may generate from query parms
	KString sWWWForm;

	if (m_bQueryToWWWFormConversion           &&
		sRequestBody.empty()                  &&
		!RequestURL.Query.empty()             &&
		RequestMethod != KHTTPMethod::GET     &&
		RequestMethod != KHTTPMethod::OPTIONS &&
		RequestMethod != KHTTPMethod::HEAD    &&
		RequestMethod != KHTTPMethod::CONNECT)
	{
		// we automatically create a www form as body data if there are
		// query parms but no post data (and the method is not GET)
		RequestURL.Query.Serialize(sWWWForm);
		RequestURL.Query.clear();
		// now point post data into the form
		sRequestBody = sWWWForm;
		MIME = KMIME::WWW_FORM_URLENCODED;
		kDebug(3, "created urlencoded form body from query parms");
	}

	KStopWatch ConnectTime  (KStopWatch::Halted);
	KStopWatch TransmitTime (KStopWatch::Halted);
	KStopWatch ReceiveTime  (KStopWatch::Halted);

	std::size_t iRead      { 0 };
	uint16_t    iRedirects { 0 };
	uint16_t    iRetries   { 0 };

	// do we have a separate URL to connect to?
	bool bHaveSeparateConnectURL = !HostURL.empty();

	// avoid a copy, use a ref
	const KURL& ConnectURL = bHaveSeparateConnectURL ? HostURL : RequestURL;

	for(;;)
	{
		ConnectTime.resume();

		bool bReuseConnection = AlreadyConnected(ConnectURL);

		if (bReuseConnection || Connect(ConnectURL))
		{
			ConnectTime.halt();

			if (Resource(RequestURL, RequestMethod))
			{
				if (m_bAcceptCookies)
				{
					// remove any cookie header if set
					Request.Headers.Remove(KHTTPHeader::COOKIE);

					KString sCookie = m_Cookies.Serialize(RequestURL);

					if (!sCookie.empty())
					{
						AddHeader(KHTTPHeader::COOKIE, sCookie);
					}
				}

				TransmitTime.resume();
				
				if (SendRequest (sRequestBody, MIME))
				{
					TransmitTime.halt();

					if (RequestMethod != KHTTPMethod::HEAD &&
						RequestMethod != KHTTPMethod::TRACE)
					{
						ReceiveTime.resume();
						iRead += Read (OutStream);
						ReceiveTime.halt();
					}
				}
				else
				{
					TransmitTime.halt();

					if (Response.Fail())
					{
						// check for error 598 - NETWORK READ/WRITE ERROR,
						// allow one retry, but only if it was a reused connection
						if (Response.GetStatusCode() == KHTTPError::H5xx_READTIMEOUT &&
							!iRetries++ &&
							bReuseConnection)
						{
							if (m_bAllowOneRetry)
							{
								kDebug(3, "retrying connection");
								// allow one connection retry - we might have run into
								// a keepalive connection and the other end shut it down
								// just before we sent data
								continue;
							}
							kDebug(3, "connection retry disabled");
						}
					}
				}
			}
		}
		else
		{
			ConnectTime.halt();
		}

		if (!CheckForRedirect(RequestURL, RequestMethod, /*bNoHostChange=*/bHaveSeparateConnectURL))
		{
			break;
		}

		if (iRedirects++ >= m_iMaxRedirects)
		{
			SetError(kFormat("number of redirects ({}) exceeds max redirection limit of {}",
							 iRedirects,
							 m_iMaxRedirects));
			break;
		}
		// else loop into the redirection
	}

	KDuration TotalTime = ConnectTime.elapsed() + TransmitTime.elapsed() + ReceiveTime.elapsed();

	kDebug(3, "connect {}, transmit {}, receive {}, total {}", ConnectTime.elapsed(), TransmitTime.elapsed(), ReceiveTime.elapsed(), TotalTime);

	if (m_pServiceSummary)
	{
		if (!kjson::Exists (*m_pServiceSummary, "http"))
		{
			(*m_pServiceSummary)["http"] = KJSON::array();
		}
		(*m_pServiceSummary)["http"] += {
			{ "request_method",      RequestMethod.Serialize()  },
			{ "url",                 RequestURL.Serialize()     },
			{ "bytes_request_body",  sRequestBody.size()        },
			{ "bytes_response_body", iRead                      },
			{ "error_string",        Error()                    },
			{ "msecs_connect",       ConnectTime .elapsed().milliseconds().count() },
			{ "msecs_transmit",      TransmitTime.elapsed().milliseconds().count() },
			{ "msecs_receive",       ReceiveTime .elapsed().milliseconds().count() },
			{ "msecs_total",         TotalTime             .milliseconds().count() },
			{ "num_redirects",       iRedirects                 },
			{ "num_retries",         iRetries                   },
			{ "response_code",       Response.GetStatusCode()   },
			{ "response_string",     Response.GetStatusString() }
		};
	}

	if (m_iWarnIfOverMilliseconds > KDuration::zero() &&
		m_TimingCallback &&
		TotalTime > m_iWarnIfOverMilliseconds)
	{
		KString sSummary = kFormat ("{}: {}, took {} (connect {}, transmit {}, receive {})",
			RequestMethod,
			RequestURL,
			TotalTime,
			ConnectTime.elapsed(),
			TransmitTime.elapsed(),
			ReceiveTime.elapsed()
		);
		m_TimingCallback (*this, TotalTime, sSummary);
	}

	if (HttpSuccess())
	{
		if (m_bAcceptCookies)
		{
			// check for Set-Cookie headers
			const auto Range = Response.Headers.equal_range(KHTTPHeader::SET_COOKIE);

			for (auto it = Range.first; it != Range.second; ++it)
			{
				// add each cookie
				m_Cookies.Parse(RequestURL, it->second);
			}
		}

		return true; // return with success..
	}
	else // HttpSuccess() -> false
	{
		if (!HasError())
		{
			SetError(Response.GetStatusString());
		}

		if (kWouldLog(3))
		{
			if (!sRequestBody.empty())
			{
				if (!kIsBinary(sRequestBody))
				{
					kDebug(3, "{} {}\n{}", RequestMethod.Serialize(), RequestURL.KResource::Serialize(), sRequestBody.LeftUTF8(1000));
				}
			}
			else
			{
				kDebug(3, "{} {}", RequestMethod.Serialize(), RequestURL.KResource::Serialize());
			}

			kDebug(3, "{} {} from URL {}", Response.iStatusCode, Response.sStatusString, RequestURL.Serialize());
		}

		return false; // return with failure
	}

} // HttpRequest2Host

//-----------------------------------------------------------------------------
KString KWebClient::HttpRequest2Host (const KURL& HostURL, KURL URL, KHTTPMethod RequestMethod/* = KHTTPMethod::GET*/, KStringView sRequestBody/* = ""*/, const KMIME& MIME/* = KMIME::JSON*/)
//-----------------------------------------------------------------------------
{
	KString sResponse;
	KOutStringStream oss(sResponse);
	HttpRequest2Host(oss, HostURL, std::move(URL), RequestMethod, sRequestBody, MIME);

	if (kWouldLog(3))
	{
		if (!sResponse.empty())
		{
			if (!kIsBinary(sResponse))
			{
				kDebug(3, kLimitSizeUTF8(sResponse, 2048));
			}
		}
		kDebug(3, "received {} bytes", sResponse.size());
	}

	return sResponse;

} // HttpRequest2Host

//-----------------------------------------------------------------------------
KString kHTTPGet(KURL URL, KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP(Options);
	return HTTP.Get(std::move(URL));

} // kHTTPGet

//-----------------------------------------------------------------------------
bool kHTTPHead(KURL URL, KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP(Options);
	return HTTP.Head(std::move(URL));

} // kHTTPHead

//-----------------------------------------------------------------------------
KString kHTTPPost(KURL URL, KStringView svPostData, const KMIME& Mime, KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
{
	KWebClient HTTP(Options);
	return HTTP.Post(std::move(URL), svPostData, Mime);

} // kHTTPPost

DEKAF2_NAMESPACE_END
