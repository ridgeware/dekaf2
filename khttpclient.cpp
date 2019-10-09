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
#include "kbase64.h"
#include "kmessagedigest.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
KHTTPClient::Authenticator::~Authenticator()
//-----------------------------------------------------------------------------
{
} // virtual dtor

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

	KMD5 HA1, HA2, Response;

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
		Resource(url, method);
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
: m_bVerifyCerts(bVerifyCerts)
{
	if (Connect(url, Proxy))
	{
		Resource(url, method);
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
		return SetError("KConnection is invalid");
	}

	if (!m_Connection->Good())
	{
		return SetError(m_Connection->Error());
	}

	// this is a new connection, so initially assume no proxying
	m_bUseHTTPProxyProtocol	= false;

	m_Connection->SetTimeout(m_Timeout);

	(*m_Connection)->SetReaderEndOfLine('\n');
	(*m_Connection)->SetReaderLeftTrim("");
	(*m_Connection)->SetReaderRightTrim("\r\n");
	(*m_Connection)->SetWriterEndOfLine("\r\n");

	Response.SetInputStream(m_Connection->Stream());
	Request.SetOutputStream(m_Connection->Stream());

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
		// get a comma delimited list of domains that shall not be proxied,
		// like "localhost,127.0.0.1,.example.com,www.nosite.org"

		if (FilterByNoProxyList(url, kGetEnv("NO_PROXY")))
		{
			// which protocol?
			bool bIsHTTPS = url.Protocol == url::KProtocol::HTTPS || url.Port == "443";

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
	bool bTargetIsHTTPS = url.Protocol   == url::KProtocol::HTTPS || url.Port   == "443";
	bool bProxyIsHTTPS  = Proxy.Protocol == url::KProtocol::HTTPS || Proxy.Port == "443";

	if (!bTargetIsHTTPS && AlreadyConnected(Proxy))
	{
		// we can reuse an existing non-HTTPS proxy connection
		return true;
	}

	// Connect the proxy. Use a TLS connection if either proxy or target is HTTPS.
	if (!Connect(KConnection::Create(Proxy, bProxyIsHTTPS || bTargetIsHTTPS, m_bVerifyCerts)))
	{
		// error is already set
		return false;
	}

	if (bTargetIsHTTPS && !bProxyIsHTTPS)
	{
		// Connect to the proxy in HTTP and request a transparent tunnel to
		// the target with CONNECT, then start the TLS handshake
		//
		// In this type of configuration we can reuse the initially not yet
		// handshaked TLS connection to the proxy to just do the TLS handshake
		// once the proxy has established the tunnel
		//
		// This mode is supported for proxies inside the local network. Using
		// it for proxies on the internet would be defeating the purpose.

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
	else if (bTargetIsHTTPS && bProxyIsHTTPS)
	{
		// the most difficult case: connect the Proxy with TLS _and_ start a
		// new TLS stream and handshake to the target server inside the proxy
		// connection once it is established

		return SetError("TLS tunneling through a TLS stream not yet supported");
	}
	else if (!bTargetIsHTTPS)
	{
		// for HTTP proxying we only have to modify the request a little bit
		// - we pass this information on to the request methods by setting
		// a flag
		//
		// Notice that in this mode the connection to the proxy server is
		// automatically run in TLS mode if requested by the protocol part
		// of the proxy URL

		m_bUseHTTPProxyProtocol = true;

		return true;
	}

	// this is unreachable code, but clang thinks differently
	return false;

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
		SetRequestHeader(KHTTPHeaders::PROXY_CONNECTION, "keep-alive", true);
	}
	else if (m_bUseHTTPProxyProtocol)
	{
		// for HTTP proxying the resource request string has to include
		// the server domain and port
		Request.Endpoint = url;
		Request.Resource = url;
		SetRequestHeader(KHTTPHeaders::PROXY_CONNECTION, "keep-alive", true);
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
		return SetRequestHeader(KHTTPHeaders::HOST, m_sForcedHost, true);
	}

	if (url.Domain.empty())
	{
		return SetError("Domain is empty");
	}

	// set the host header so that it overwrites a previously set one
	if (!bForcePort
		&& (url.Port.empty()
		|| (url.Protocol == url::KProtocol::HTTP  && url.Port ==  "80")
		|| (url.Protocol == url::KProtocol::HTTPS && url.Port == "443")))
	{
		// domain alone is sufficient for standard ports
		return SetRequestHeader(KHTTPHeaders::HOST, url.Domain.Serialize(), true);
	}
	else
	{
		// build "domain:port"
		KString sHost;
		url.Domain.Serialize(sHost);
		url.Port.Serialize(sHost);
		return SetRequestHeader(KHTTPHeaders::HOST, sHost, true);
	}

} // SetHostHeader

//-----------------------------------------------------------------------------
bool KHTTPClient::SetRequestHeader(KStringView svName, KStringView svValue, bool bOverwrite)
//-----------------------------------------------------------------------------
{
	if (bOverwrite)
	{
		Request.Headers.Set(svName, svValue);
	}
	else
	{
		Request.Headers.Add(svName, svValue);
	}

	return true;

} // RequestHeader

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
	Request.Headers.Remove(KHTTPHeaders::AUTHORIZATION);
	m_Authenticator.reset();
	return *this;

} // ClearAuthentication

//-----------------------------------------------------------------------------
bool KHTTPClient::SendRequest(KStringView svPostData, KMIME Mime)
//-----------------------------------------------------------------------------
{
	Response.clear();

	// remove remaining automatic headers from previous requests
	Request.Headers.Remove(KHTTPHeaders::CONTENT_LENGTH);
	Request.Headers.Remove(KHTTPHeaders::CONTENT_TYPE);

	if (svPostData.size())
	{
		kDebug(2, "send {} bytes of body with mime '{}'", svPostData.size(), Mime);
	}

	if (Request.Resource.empty() &&
		Request.Method != KHTTPMethod::CONNECT)
	{
		return SetError("no resource");
	}

	if (!m_Connection || !m_Connection->Good())
	{
		return SetError("no stream");
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
			SetRequestHeader(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(svPostData.size()), true);

			if (!svPostData.empty())
			{
				SetRequestHeader(KHTTPHeaders::CONTENT_TYPE, Mime, true);
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
		SetRequestHeader(KHTTPHeaders::AUTHORIZATION, m_Authenticator->GetAuthHeader(Request, svPostData), true);
	}

	if (m_bRequestCompression && Request.Method != KHTTPMethod::CONNECT)
	{
		SetRequestHeader(KHTTPHeaders::ACCEPT_ENCODING, "gzip", true);
	}

	if (!Request.Serialize()) // this sends the request headers to the remote server
	{
		return SetError(Request.Error());
	}

	if (!svPostData.empty())
	{
		kDebug(2, "sending {} bytes of '{}' data", svPostData.size(), KStringView(Mime));
		Request.Write(svPostData);
	}

	if (!m_Connection->Good())
	{
		return SetError("write error");
	}

	return Parse();
}

//-----------------------------------------------------------------------------
bool KHTTPClient::Serialize()
//-----------------------------------------------------------------------------
{
	if (!Request.Serialize())
	{
		SetError(Request.Error());
		return false;
	}

	return true;

} // Serialize

//-----------------------------------------------------------------------------
bool KHTTPClient::Parse()
//-----------------------------------------------------------------------------
{
	Request.close();

	if (!Response.Parse())
	{
		if (!Response.Error().empty())
		{
			SetError(Response.Error());
		}
		else
		{
			SetError(m_Connection->Error());
		}
		return false;
	}

	// make sure also a network read error triggers a meaningful
	// status code / string
	Response.Fail();

	kDebug(2, "HTTP-{} {}", Response.GetStatusCode(), Response.GetStatusString());

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPClient::CheckForRedirect(KURL& URL, KStringView& sRequestMethod)
//-----------------------------------------------------------------------------
{
	// check for redirections
	switch (Response.GetStatusCode())
	{
		case 303:
			// a 303 response always forces a method change to GET
			if (sRequestMethod != KHTTPMethod::GET)
			{
				kDebug(1, "303 redirect changes method from {} to GET", sRequestMethod);
				sRequestMethod = KHTTPMethod::GET;
			}
			DEKAF2_FALLTHROUGH;

		case 301: // other than some browsers we do not switch the method to
		case 302: // GET when receiving a 301 or 302 response
		case 307:
		case 308:
			{
				KURL Redirect = Response.Headers[KHTTPHeaders::location];

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
				else
				{
					SetError(kFormat("invalid {} header in {} redirection: {}",
									 KHTTPHeaders::LOCATION,
									 Response.GetStatusCode(),
									 Response.Headers[KHTTPHeaders::location]));
				}
			}
			break;

		default:
			break;
	}

	// no redirection
	return false;

} // CheckForRedirect

//-----------------------------------------------------------------------------
bool KHTTPClient::AlreadyConnected(const KURL& EndPoint) const
//-----------------------------------------------------------------------------
{
	if (!m_Connection || !m_Connection->Good())
	{
		return false;
	}

	return EndPoint == m_Connection->EndPoint();

} // AlreadyConnected

//-----------------------------------------------------------------------------
bool KHTTPClient::SetError(KStringView sError) const
//-----------------------------------------------------------------------------
{
	kDebug(1, "{}", sError);
	m_sError = sError;
	return false;

} // SetError

} // end of namespace dekaf2
