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

#include "khttpclient.h"
#include "khttperror.h"
#include "kbase64.h"
#include "kmessagedigest.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPClient::BasicAuthenticator::BasicAuthenticator(KString _sUsername, KString _sPassword)
//-----------------------------------------------------------------------------
: sUsername(std::move(_sUsername))
, sPassword(std::move(_sPassword))
{
}

//-----------------------------------------------------------------------------
const KString& KHTTPClient::BasicAuthenticator::GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody)
//-----------------------------------------------------------------------------
{
	if (sResponse.empty())
	{
		sResponse = "Basic ";
		sResponse += KBase64::Encode(sUsername + ":" + sPassword, false /* == no linebreaks */);
	}
	return sResponse;

} // BasicAuthenticator::GetAuthHeader

//-----------------------------------------------------------------------------
KHTTPClient::DigestAuthenticator::DigestAuthenticator(KString _sUsername,
													  KString _sPassword,
													  KString _sRealm,
													  KString _sNonce,
													  KString _sOpaque,
													  KString _sQoP)
//-----------------------------------------------------------------------------
: BasicAuthenticator(std::move(_sUsername), std::move(_sPassword))
, sRealm(std::move(_sRealm))
, sNonce(std::move(_sNonce))
, sOpaque(std::move(_sOpaque))
, sQoP(std::move(_sQoP))
{
}

//-----------------------------------------------------------------------------
const KString& KHTTPClient::DigestAuthenticator::GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody)
//-----------------------------------------------------------------------------
{
	/*
	 Authorization: Digest username="Mufasa",
		 realm="testrealm@host.com",
		 nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
		 uri="/dir/index.html",
		 qop=auth,
		 nc=00000001,
		 cnonce="0a4f113b",
		 response="6629fae49393a05397450978507c4ef1",
		 opaque="5ccc069c403ebaf9f0171e9517f40e41"
	 */

	uint32_t iCNonce    = kRandom();
	KString sCNonce     = KString::to_hexstring(iCNonce, true, false);
	KString sNonceCount = KString::to_hexstring(++iNonceCount);
	sNonceCount.PadLeft(8, '0');

	KMD5 HA1;
	KMD5 HA2;
	KMD5 Response;

	HA1 = sUsername;
	HA1 += ":";
	HA1 += sPassword;

	// different calculation for auth-int
	// auth or any: HA2 = MD5(method:digestURI)
	// auth-int:    HA2 = MD5(method:digestURI:MD5(entityBody))

	HA2 = Request.Method.Serialize();
	HA2 += ":";
	HA2 += Request.Resource.Path;

	if (sQoP == "auth-int")
	{
		HA2 += ":";
		HA2 += KMD5(sBody).Digest();
	}

	bool bSimpleResponse = sQoP != "auth" && sQoP != "auth-int";

	Response = HA1.Digest();
	Response += ":";
	Response += sNonce;
	Response += ":";

	if (!bSimpleResponse)
	{
		Response += sNonceCount;
		Response += ":";
		Response += sCNonce;
		Response += ":";
		Response += sQoP;
		Response += ":";
	}

	Response += HA2.Digest();

	if (bSimpleResponse)
	{
		sResponse = kFormat(
				   "Digest"
				   " username=\"{}\","
				   " realm=\"{}\","
				   " nonce=\"{}\","
				   " uri=\"{}\","
				   " response=\"{}\","
				   " opaque=\"{}\"",
				   sUsername,
				   sRealm,
				   sNonce,
				   Request.Resource.Path, // uri
				   Response.Digest(),
				   sOpaque
				);
	}
	else
	{
		sResponse = kFormat(
				   "Digest"
				   " username=\"{}\","
				   " realm=\"{}\","
				   " nonce=\"{}\","
				   " uri=\"{}\","
				   " qop={},"
				   " nc={},"
				   " cnonce=\"{}\","
				   " response=\"{}\","
				   " opaque=\"{}\"",
				   sUsername,
				   sRealm,
				   sNonce,
				   Request.Resource.Path, // uri
				   sQoP,
				   sNonceCount, // nc
				   sCNonce,
				   Response.Digest(),
				   sOpaque
				);
	}

	return sResponse;

} // DigestAuthenticator::GetAuthHeader


//-----------------------------------------------------------------------------
void KHTTPClient::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	m_sError.clear();
	m_bRequestCompression = true;
	m_Authenticator.reset();

	// do not reset m_bUseHTTPProxyProtocol here - it stays valid until
	// the setup of a new connection
	// same for m_sForcedHost
	// same for m_iMaxRedirects
	// same for m_bLastResponseFailed

} // clear

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(bool bVerifyCerts)
//-----------------------------------------------------------------------------
: m_bVerifyCerts(bVerifyCerts)
{
} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(const KURL& url, KHTTPMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
: m_bVerifyCerts(bVerifyCerts)
{
	if (Connect(url))
	{
		Resource(url, std::move(method));
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
: m_bVerifyCerts(bVerifyCerts)
{
	if (Connect(url, Proxy))
	{
		Resource(url, std::move(method));
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(std::unique_ptr<KConnection> stream)
//-----------------------------------------------------------------------------
{
	Connect(std::move(stream));

} // Ctor

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(std::unique_ptr<KConnection> Connection)
//-----------------------------------------------------------------------------
{
	SetError(KStringView{});

	m_Connection = std::move(Connection);

	if (!m_Connection)
	{
		// Reset the streams in the filters, as they may now
		// point into a deleted connection object from a
		// previous connection!
		Response.ResetInputStream();
		Request.ResetOutputStream();

		return SetError("KConnection is invalid");
	}

	// reset status code and string
	Response.SetStatus(0, "");

	// immediately set the filter streams to the new object
	// (see comment above)
	Response.SetInputStream(m_Connection->Stream());
	Request.SetOutputStream(m_Connection->Stream());

	m_Connection->SetTimeout(m_Timeout);

	(*m_Connection)->SetReaderEndOfLine('\n');
	(*m_Connection)->SetReaderLeftTrim("");
	(*m_Connection)->SetReaderRightTrim("\r\n");
	(*m_Connection)->SetWriterEndOfLine("\r\n");

	// this is a new connection, so initially assume no proxying
	m_bUseHTTPProxyProtocol	= false;

	if (!m_Connection->Good())
	{
		return SetError(m_Connection->Error());
	}

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::FilterByNoProxyList(const KURL& url, KStringView sNoProxy)
//-----------------------------------------------------------------------------
{
	for (auto sDomain : sNoProxy.Split())
	{
		if (sDomain.front() == '.')
		{
			if (url.Domain.get().ends_with(sDomain))
			{
				return false;
			}
		}
		else if (url.Domain.get() == sDomain)
		{
			return false;
		}
	}

	return true;

} // FilterByNoProxyList

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(const KURL& url)
//-----------------------------------------------------------------------------
{
	if (!m_Proxy.empty())
	{
		return Connect(url, m_Proxy);
	}

	if (AlreadyConnected(url))
	{
		return true;
	}

	if (m_bAutoProxy)
	{
		// check a comma delimited list of domains that shall not be proxied,
		// like "localhost,127.0.0.1,.example.com,www.nosite.org"
		if (FilterByNoProxyList(url, kGetEnv("NO_PROXY")))
		{
			// which protocol?
			bool bIsHTTPS = url.Protocol == url::KProtocol::HTTPS ||
			                (url::KProtocol::UNDEFINED && url.Port == "443");

			// try to read proxy setup from environment
			KURL Proxy(kGetEnv(bIsHTTPS ? "HTTPS_PROXY" : "HTTP_PROXY"));

			if (!Proxy.empty())
			{
				return Connect(url, Proxy);
			}
		}
	}

	return Connect(KConnection::Create(url, false, m_bVerifyCerts));

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(const KURL& url, const KURL& Proxy)
//-----------------------------------------------------------------------------
{
	if (Proxy.empty())
	{
		return Connect(KConnection::Create(url, false, m_bVerifyCerts));
	}

	// which protocol on which connection segment?
	bool bTargetIsHTTPS = url.Protocol   == url::KProtocol::HTTPS || (url::KProtocol::UNDEFINED && url.Port   == "443");
	bool bProxyIsHTTPS  = Proxy.Protocol == url::KProtocol::HTTPS || (url::KProtocol::UNDEFINED && Proxy.Port == "443");

	if (!bTargetIsHTTPS)
	{
		// check if we are already connected to this proxy (for all HTTP targets
		// it is our connection endpoint)
		if (AlreadyConnected(Proxy))
		{
			// we can reuse an existing non-HTTPS proxy connection
			return true;
		}
	}
	else
	{
		// check if we are already connected to the target server (for all CONNECTed
		// targets it is our connection endpoint)
		if (AlreadyConnected(url))
		{
			return true;
		}
	}

	kDebug(2, "connecting via proxy {}", Proxy.Serialize());

	// Connect the proxy. Use a TLS connection if either proxy or target is HTTPS.
	if (!Connect(KConnection::Create(Proxy, bProxyIsHTTPS || bTargetIsHTTPS, m_bVerifyCerts)))
	{
		// error is already set
		return false;
	}

	if (bTargetIsHTTPS)
	{
		// Connect to the proxy in either HTTP or HTTPS and request a transparent
		// tunnel to the target with CONNECT, then start the TLS handshake.
		//
		// For a HTTP proxy connection we can reuse the initially not yet
		// handshaked TLS connection to the proxy to just do the TLS handshake
		// once the proxy has established the tunnel.
		//
		// This mode is supported for proxies inside a local and protected network.
		// Using it for proxies on the internet would be defeating the purpose.
		//
		// For a HTTPS proxy we have to wrap a new TLS connection to the target
		// into the outer TLS connection to the proxy, and also use late TLS
		// handshaking to first talk to the proxy server.

		if (bProxyIsHTTPS)
		{
			// Make the existing connect to the proxy the tunnel for the inner
			// connection to the target TLS server (and let it own the outer
			// stream for later release).
			//
			// We are optimistic and already mark the target URL as our new
			// connection endpoint.
			m_Connection = std::make_unique<KSSLConnection>(m_Connection.release()->Stream(), url);
		}

		// We first have to send our CONNECT request in plain text..
		m_Connection->SetManualTLSHandshake(true);

		// and here it comes:
		if (!Resource(url, KHTTPMethod::CONNECT))
		{
			return false;
		}

		if (!SendRequest())
		{
			return false;
		}

		if (Response.iStatusCode != 200)
		{
			return SetError(kFormat("proxy server returned {} {}", Response.iStatusCode, Response.sStatusString));
		}

		// end of header, start TLS
		if (!m_Connection->StartManualTLSHandshake())
		{
			return SetError(m_Connection->Error());
		}

		// remove all headers from the outer proxy connection
		clear();

		// that's it.. we do not have to set any special flag, as from now on
		// all communication is simply passed through a transparent tunnel to
		// the target server
		return true;
	}

	// Target is not HTTPS - we can simply use the proxy extension of the HTTP
	// protocol - we only have to modify the request a little bit - we pass this
	// information on to the request methods by setting a flag
	//
	// Notice that in this mode the connection to the proxy server is
	// automatically run in TLS mode if requested by the protocol part
	// of the proxy URL

	m_bUseHTTPProxyProtocol = true;

	return true;

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::Disconnect()
//-----------------------------------------------------------------------------
{
	if (!m_Connection)
	{
		return SetError("no connection to disconnect");
	}

	m_Connection.reset();

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
KHTTPClient& KHTTPClient::SetTimeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	m_Timeout = iSeconds;

	if (m_Connection)
	{
		m_Connection->SetTimeout(iSeconds);
	}

	return *this;

} // SetTimeout

//-----------------------------------------------------------------------------
bool KHTTPClient::Resource(const KURL& url, KHTTPMethod method)
//-----------------------------------------------------------------------------
{
	if (url.empty())
	{
		return SetError("URL is empty");
	};

	Request.Method = method;
	Request.sHTTPVersion = "HTTP/1.1";

	bool bIsConnect { false };

	if (DEKAF2_UNLIKELY(method == KHTTPMethod::CONNECT))
	{
		// for HTTPS proxying the CONNECT query has the server
		// domain and port
		Request.Endpoint = url;
		bIsConnect = true;
		AddHeader(KHTTPHeader::PROXY_CONNECTION, "keep-alive");
	}
	else if (m_bUseHTTPProxyProtocol)
	{
		// for HTTP proxying the resource request string has to include
		// the server domain and port
		Request.Endpoint = url;
		Request.Resource = url;
		AddHeader(KHTTPHeader::PROXY_CONNECTION, "keep-alive");
	}
	else
	{
		// for everything else it's only the path and the query
		Request.Resource = url;
	}

	// make sure we always have a valid path set
	if (!bIsConnect && Request.Resource.Path.empty())
	{
		Request.Resource.Path.set("/");
	}

	return SetHostHeader(url, bIsConnect);

} // Resource

//-----------------------------------------------------------------------------
bool KHTTPClient::SetHostHeader(const KURL& url, bool bForcePort)
//-----------------------------------------------------------------------------
{
	if (!m_sForcedHost.empty())
	{
		AddHeader(KHTTPHeader::HOST, m_sForcedHost);
	}
	else if (url.Domain.empty())
	{
		return SetError("Domain is empty");
	}
	else if (!bForcePort
		&& (url.Port.empty()
		|| (url.Protocol == url::KProtocol::HTTP  && url.Port ==  "80")
		|| (url.Protocol == url::KProtocol::HTTPS && url.Port == "443")))
	{
		// domain alone is sufficient for standard ports
		AddHeader(KHTTPHeader::HOST, url.Domain.Serialize());
	}
	else
	{
		// build "domain:port"
		KString sHost;
		url.Domain.Serialize(sHost);
		url.Port.Serialize(sHost);

		AddHeader(KHTTPHeader::HOST, sHost);
	}

	return true;

} // SetHostHeader

//-----------------------------------------------------------------------------
KHTTPClient& KHTTPClient::BasicAuthentication(KString sUsername,
											  KString sPassword)
//-----------------------------------------------------------------------------
{
	m_Authenticator = std::make_unique<BasicAuthenticator>(std::move(sUsername),
														   std::move(sPassword));
	return *this;

} // BasicAuthentication

//-----------------------------------------------------------------------------
KHTTPClient& KHTTPClient::DigestAuthentication(KString sUsername,
											   KString sPassword,
											   KString sRealm,
											   KString sNonce,
											   KString sOpaque,
											   KString sQoP)
//-----------------------------------------------------------------------------
{
	m_Authenticator = std::make_unique<DigestAuthenticator>(std::move(sUsername),
															std::move(sPassword),
															std::move(sRealm),
															std::move(sNonce),
															std::move(sOpaque),
															std::move(sQoP));
	return *this;

} // DigestAuthentication

//-----------------------------------------------------------------------------
KHTTPClient& KHTTPClient::ClearAuthentication()
//-----------------------------------------------------------------------------
{
	Request.Headers.Remove(KHTTPHeader::AUTHORIZATION);
	m_Authenticator.reset();
	return *this;

} // ClearAuthentication

//-----------------------------------------------------------------------------
bool KHTTPClient::SendRequest(KStringView svPostData, KMIME Mime)
//-----------------------------------------------------------------------------
{
	Response.clear();

	// remove remaining automatic headers from previous requests
	Request.Headers.Remove(KHTTPHeader::CONTENT_LENGTH);
	Request.Headers.Remove(KHTTPHeader::CONTENT_TYPE);

	if (Request.Resource.empty() &&
		Request.Method != KHTTPMethod::CONNECT)
	{
		return SetError("no resource");
	}

	if (!m_Connection || !m_Connection->Good())
	{
		return SetNetworkError(false, "no stream");
	}

	if (Request.Method != KHTTPMethod::HEAD &&
		Request.Method != KHTTPMethod::OPTIONS &&
		Request.Method != KHTTPMethod::CONNECT)
	{
		if (Request.Method != KHTTPMethod::GET ||
			!svPostData.empty())
		{
			// We allow sending body data for GET requests as well, as a few
			// applications expect doing so. It is not generally advisable due
			// to proxy issues though.
			AddHeader(KHTTPHeader::CONTENT_LENGTH, KString::to_string(svPostData.size()));

			if (!svPostData.empty())
			{
				AddHeader(KHTTPHeader::CONTENT_TYPE, Mime.Serialize());
			}
		}
	}
	else
	{
		if (!svPostData.empty())
		{
			kDebug(1, "cannot send body data with {} request, data removed", Request.Method.Serialize())
			svPostData.clear();
		}
	}

	if (m_Authenticator)
	{
		AddHeader(KHTTPHeader::AUTHORIZATION, m_Authenticator->GetAuthHeader(Request, svPostData));
	}

	if (m_bRequestCompression && Request.Method != KHTTPMethod::CONNECT)
	{
		AddHeader(KHTTPHeader::ACCEPT_ENCODING, "gzip, bzip2, deflate");
	}

	if (!Request.Serialize()) // this sends the request headers to the remote server
	{
		kDebugLog(2, "cannot write header");
		return SetNetworkError(false, Request.Error());
	}

	if (!svPostData.empty())
	{
		kDebug(2, "sending {} bytes of body with mime '{}'", svPostData.size(), Mime.Serialize());
		kDebug(3, svPostData);
		if (Request.Write(svPostData) != svPostData.size())
		{
			kDebug(2, "cannot write body");
			return SetNetworkError(false, Request.Error());
		}
	}

	if (!m_Connection->Good())
	{
		return SetNetworkError(false, "write error");
	}

	return Parse();
}

//-----------------------------------------------------------------------------
bool KHTTPClient::Serialize()
//-----------------------------------------------------------------------------
{
	if (!Request.Serialize())
	{
		return SetNetworkError(false, Request.Error());
	}

	return true;

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPClient::StatusIsRedirect() const
//-----------------------------------------------------------------------------
{
	switch (Response.GetStatusCode())
	{
		case KHTTPError::H303_SEE_OTHER:
		case KHTTPError::H301_MOVED_PERMANENTLY:
		case KHTTPError::H302_MOVED_TEMPORARILY:
		case KHTTPError::H307_TEMPORARY_REDIRECT:
		case KHTTPError::H308_PERMANENT_REDIRECT:
			return true;

		default:
			return false;
	}
	return false;

} // StatusIsRedirect

//-----------------------------------------------------------------------------
bool KHTTPClient::Parse()
//-----------------------------------------------------------------------------
{
	Request.close();

	m_bKeepAlive = true;

	if (!Response.Parse())
	{
		return SetNetworkError(true, Response.Error().empty() ? m_Connection->Error() : Response.Error());
	}

	// make sure also a network read error triggers a meaningful status
	// code / string (Response.Good() calls Response.Fail() and ensures this)
	if (!Response.Good() && !StatusIsRedirect())
	{
		// we do not close the connection right here because inheriting
		// classes may still want to read the response body, but we mark
		// the failure and will not allow a reuse of the connection
		m_bKeepAlive = false;
		kDebug(2, "mark instance as failed");
	}
	else
	{
		// check Response for keepalive
		m_bKeepAlive = Response.HasKeepAlive();
	}

	kDebug(2, "HTTP-{} {}", Response.GetStatusCode(), Response.GetStatusString());

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPClient::CheckForRedirect(KURL& URL, KHTTPMethod& RequestMethod)
//-----------------------------------------------------------------------------
{
	// check for redirections
	switch (Response.GetStatusCode())
	{
		case KHTTPError::H303_SEE_OTHER:
			// a 303 response always forces a method change to GET
			if (RequestMethod != KHTTPMethod::GET)
			{
				kDebug(1, "303 redirect changes method from {} to GET", RequestMethod.Serialize());
				RequestMethod = KHTTPMethod::GET;
			}
			DEKAF2_FALLTHROUGH;

		case KHTTPError::H301_MOVED_PERMANENTLY: // other than some browsers we do not switch the method to
		case KHTTPError::H302_MOVED_TEMPORARILY: // GET when receiving a 301 or 302 response
		case KHTTPError::H307_TEMPORARY_REDIRECT:
		case KHTTPError::H308_PERMANENT_REDIRECT:
			{
				KURL Redirect = Response.Headers[KHTTPHeader::LOCATION];

				if (!Redirect.empty())
				{
					if (Redirect.Query.empty())
					{
						Redirect.Query = URL.Query;
					}
					if (Redirect.Protocol.empty())
					{
						Redirect.Protocol = URL.Protocol;
					}
					if (Redirect.Port.empty() && !URL.Port.empty() && Redirect.Domain.empty())
					{
						Redirect.Port = URL.Port;
					}
					if (Redirect.Domain.empty())
					{
						Redirect.Domain = URL.Domain;
					}
					// we deliberately drop username and password in a redirection

					kDebug(1, "{} redirect from {} to {}",
						   Response.GetStatusCode(),
						   URL.Serialize(),
						   Redirect.Serialize());

					URL = std::move(Redirect);

					// and follow the redirection
					return true;
				}

				SetError(kFormat("invalid {} header in {} redirection: {}",
								 KHTTPHeader(KHTTPHeader::LOCATION),
								 Response.GetStatusCode(),
								 Response.Headers[KHTTPHeader::LOCATION]));
			}
			break;

		default:
			break;
	}

	// no redirection
	return false;

} // CheckForRedirect

//-----------------------------------------------------------------------------
bool KHTTPClient::AlreadyConnected(const KTCPEndPoint& EndPoint) const
//-----------------------------------------------------------------------------
{
	if (!m_bKeepAlive || !m_Connection || !m_Connection->Good())
	{
		return false;
	}

	if (EndPoint == m_Connection->EndPoint())
	{
		kDebug(2, "already connected to {}", m_Connection->EndPoint().Serialize());
		return true;
	}

	return false;

} // AlreadyConnected

//-----------------------------------------------------------------------------
bool KHTTPClient::SetError(KString sError) const
//-----------------------------------------------------------------------------
{
	m_sError = std::move(sError);
	kDebug(1, m_sError);
	return false;

} // SetError

//-----------------------------------------------------------------------------
bool KHTTPClient::SetNetworkError(bool bRead, KString sError)
//-----------------------------------------------------------------------------
{
	if (!Response.GetStatusCode() || Response.Good())
	{
		Response.SetStatus(KHTTPError::H5xx_READTIMEOUT,
						   kFormat("NETWORK {} ERROR", bRead ? "READ" : "WRITE"));
	}

	m_bKeepAlive = false;

	return SetError(std::move(sError));

} // SetNetworkError

} // end of namespace dekaf2
