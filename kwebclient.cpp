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

	KStopWatch ConnectTime(KStopWatch::Halted);
	KStopWatch TransmitTime(KStopWatch::Halted);
	KStopWatch ReceiveTime(KStopWatch::Halted);
	std::size_t iRead { 0 };
	uint16_t iRedirects { 0 };
	uint16_t iRetries { 0 };

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
					Request.Headers.Remove(KHTTPHeader::COOKIE);

					KString sCookie = m_Cookies.Serialize(URL);

					if (!sCookie.empty())
					{
						AddHeader(KHTTPHeader::COOKIE, sCookie);
					}
				}

				TransmitTime.resume();
				
				if (SendRequest (svRequestBody, MIME))
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

		if (iRedirects++ >= m_iMaxRedirects)
		{
			SetError(kFormat("number of redirects ({}) exceeds max redirection limit of {}",
							 iRedirects,
							 m_iMaxRedirects));
			break;
		}
		// else loop into the redirection
	}

	auto iConnectTime  = ConnectTime.elapsed<std::chrono::milliseconds>().count();
	auto iTransmitTime = TransmitTime.elapsed<std::chrono::milliseconds>().count();
	auto iReceiveTime  = ReceiveTime.elapsed<std::chrono::milliseconds>().count();
	auto iTotalTime    = iConnectTime + iTransmitTime + iReceiveTime;

	kDebug(2, "connect {} ms, transmit {} ms, receive {} ms, total {} ms", iConnectTime, iTransmitTime, iReceiveTime, iTotalTime);

	if (m_pServiceSummary)
	{
		if (!kjson::Exists (*m_pServiceSummary, "http"))
		{
			(*m_pServiceSummary)["http"] = KJSON::array();
		}
		(*m_pServiceSummary)["http"] += {
			{ "request_method",      RequestMethod.Serialize()  },
			{ "url",                 URL.Serialize()            },
			{ "bytes_request_body",  svRequestBody.size()       },
			{ "bytes_response_body", iRead                      },
			{ "error_string",        Error()                    },
			{ "msecs_connect",       iConnectTime               },
			{ "msecs_transmit",      iTransmitTime              },
			{ "msecs_receive",       iReceiveTime               },
			{ "msecs_total",         iTotalTime                 },
			{ "num_redirects",       iRedirects                 },
			{ "num_retries",         iRetries                   },
			{ "response_code",       Response.GetStatusCode()   },
			{ "response_string",     Response.GetStatusString() }
		};
	}

	if (m_iWarnIfOverMilliseconds &&
		m_TimingCallback &&
		iTotalTime > m_iWarnIfOverMilliseconds)
	{
		KString sSummary = kFormat ("{}: {}, took {} msecs (connect {}, transmit {}, receive {})",
			RequestMethod.Serialize(),
			URL.Serialize(),
			kFormNumber (iTotalTime),
			kFormNumber (iConnectTime),
			kFormNumber (iTransmitTime),
			kFormNumber (iReceiveTime));
		m_TimingCallback (*this, iTotalTime, sSummary);
	}

	if (!HttpSuccess() && Error().empty())
	{
		SetError(Response.GetStatusString());

		if (kWouldLog(2))
		{
			if (!svRequestBody.empty())
			{
				if (!kIsBinary(svRequestBody))
				{
					kDebug(2, "{} {}\n{}", RequestMethod.Serialize(), URL.KResource::Serialize(), svRequestBody.LeftUTF8(1000));
				}
			}
			else
			{
				kDebug(2, "{} {}", RequestMethod.Serialize(), URL.KResource::Serialize());
			}
		}

		kDebug(2, "{} {} from URL {}", Response.iStatusCode, Response.sStatusString, URL.Serialize());
	}

	if (m_bAcceptCookies && HttpSuccess())
	{
		// check for Set-Cookie headers
		const auto Range = Response.Headers.equal_range(KHTTPHeader::SET_COOKIE);

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

	if (kWouldLog(2))
	{
		if (!sResponse.empty())
		{
			if (!kIsBinary(sResponse))
			{
				kDebug(2, sResponse.LeftUTF8(1000));
			}
		}
		kDebug(2, "received {} bytes", sResponse.size());
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
