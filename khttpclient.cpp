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
#include "kstring.h"
#include "ksystem.h"

DEKAF2_NAMESPACE_BEGIN

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
		HA2 += KMD5(sBody).HexDigest();
	}

	bool bSimpleResponse = sQoP != "auth" && sQoP != "auth-int";

	Response = HA1.HexDigest();
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

	Response += HA2.HexDigest();

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
				   Response.HexDigest(),
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
				   Response.HexDigest(),
				   sOpaque
				);
	}

	return sResponse;

} // DigestAuthenticator::GetAuthHeader

//-----------------------------------------------------------------------------
bool KHTTPClient::DigestAuthenticator::NeedsContentData() const
//-----------------------------------------------------------------------------
{
	return sQoP == "auth-int";

} // DigestAuthenticator::NeedsContentData

#if DEKAF2_HAS_NGHTTP2
//-----------------------------------------------------------------------------
KHTTPClient::HTTP2Session::HTTP2Session(KTLSStream& TLSStream)
//-----------------------------------------------------------------------------
: Session(TLSStream, true)
, StreamBuf(HTTP2StreamReader, this)
, IStream(&StreamBuf)
, InStream(IStream)
{
	InStream.SetReaderEndOfLine ('\n');
	InStream.SetReaderLeftTrim  ("");
	InStream.SetReaderRightTrim ("\r\n");

} // HTTP2Session::ctor
#endif

#if DEKAF2_HAS_NGHTTP3
//-----------------------------------------------------------------------------
KHTTPClient::HTTP3Session::HTTP3Session(KQuicStream& QuicStream)
//-----------------------------------------------------------------------------
: Session(QuicStream, true)
, StreamBuf(HTTP3StreamReader, this)
, IStream(&StreamBuf)
, InStream(IStream)
{
	InStream.SetReaderEndOfLine ('\n');
	InStream.SetReaderLeftTrim  ("");
	InStream.SetReaderRightTrim ("\r\n");

} // HTTP3Session::ctor
#endif

//-----------------------------------------------------------------------------
void KHTTPClient::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	ClearError();
	m_Authenticator.reset();
	m_bHaveHostSet = false;

	// do not reset m_bUseHTTPProxyProtocol here - it stays valid until
	// the setup of a new connection, and
	// m_sForcedHost
	// m_iMaxRedirects
	// m_bRequestCompression
	// remain valid until destruction

} // clear

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
: m_StreamOptions(Options)
{
} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(const KURL& url, KHTTPMethod method, KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
: m_StreamOptions(Options)
{
	if (Connect(url))
	{
		Resource(url, std::move(method));
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method, KHTTPStreamOptions Options)
//-----------------------------------------------------------------------------
: m_StreamOptions(Options)
{
	if (Connect(url, Proxy))
	{
		Resource(url, std::move(method));
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(std::unique_ptr<KIOStreamSocket> stream)
//-----------------------------------------------------------------------------
{
	Connect(std::move(stream));

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::~KHTTPClient()
//-----------------------------------------------------------------------------
{
	// we call Disconnect() to make sure the http2 object is destructed before
	// the connection object (as the former references the latter)
	Disconnect();

} // Dtor

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(std::unique_ptr<KIOStreamSocket> Connection)
//-----------------------------------------------------------------------------
{
	ClearError();

	// clear the response object, otherwise a previous
	// status would prevail if not overwritten by a new connection
	Response.clear();

	m_Connection = std::move(Connection);

	if (!m_Connection || !m_Connection->Good())
	{
		Request.Reset();
		Response.Reset();

		if (m_Connection && !m_Connection->Good())
		{
			return SetError(m_Connection->Error());
		}

		return SetError("KConnection is invalid");
	}

	m_Connection->SetReaderEndOfLine('\n');
	m_Connection->SetReaderLeftTrim("");
	m_Connection->SetReaderRightTrim("\r\n");
	m_Connection->SetWriterEndOfLine("\r\n");

	// immediately set the filter streams to the new object
	Request.SetOutputStream(*m_Connection);
	Response.SetInputStream(*m_Connection);

	// this is a new connection, so initially assume no proxying
	m_bUseHTTPProxyProtocol	= false;
	// and HTTP/1.1
	Request .SetHTTPVersion(KHTTPVersion::http11);
	Response.SetHTTPVersion(KHTTPVersion::http11);

#if DEKAF2_HAS_NGHTTP3
	if (m_StreamOptions.IsSet(KHTTPStreamOptions::RequestHTTP3))
	{
		auto QuicStream = dynamic_cast<KQuicStream*>(m_Connection.get());

		if (QuicStream)
		{
			// Quic forces the handshake during the connection stage
			// check if we negotiated HTTP/3
			auto sALPN = QuicStream->GetALPN();

			if (sALPN == "h3")
			{
				kDebug(2, "switching to HTTP/3");
				m_HTTP3 = std::make_unique<HTTP3Session>(*QuicStream);

				if (!m_HTTP3->Session.GetLastError().empty())
				{
					return SetError(m_HTTP3->Session.CopyLastError());
				}

				Request .SetHTTPVersion(KHTTPVersion::http3);
				Response.SetHTTPVersion(KHTTPVersion::http3);
				Response.SetInputStream(m_HTTP3->InStream);
			}
			else return SetError("server did not accept HTTP/3 request");
		}
		else return SetError("not a KQuicStream");
	}
#endif

#if DEKAF2_HAS_NGHTTP2
	m_HTTP2.reset();

	if (m_StreamOptions.IsSet(KHTTPStreamOptions::RequestHTTP2))
	{
		auto TLSStream = dynamic_cast<KTLSStream*>(m_Connection.get());

		if (TLSStream)
		{
			// force the handshake right now, we need it before
			// we try to send our first data (the request) to
			// know if we must switch to HTTP/2
			m_Connection->StartManualTLSHandshake();

			// check if we negotiated HTTP/2
			auto sALPN = m_Connection->GetALPN();

			if (sALPN == "h2")
			{
				kDebug(2, "switching to HTTP/2");
				m_HTTP2 = std::make_unique<HTTP2Session>(*TLSStream);

				if (!m_HTTP2->Session.GetLastError().empty())
				{
					return SetError(m_HTTP2->Session.CopyLastError());
				}

				Request .SetHTTPVersion(KHTTPVersion::http2);
				Response.SetHTTPVersion(KHTTPVersion::http2);
				Response.SetInputStream(m_HTTP2->InStream);
			}
			else if (m_StreamOptions.IsSet(KHTTPStreamOptions::FallBackToHTTP1) == false)
			{
				return SetError("wanted a HTTP/2 connection, but got only HTTP/1.1");
			}
		}
		else if (m_StreamOptions.IsSet(KHTTPStreamOptions::FallBackToHTTP1) == false)
		{
			return SetError("not a KTLSStream");
		}
	}
#endif

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
			                (url::KProtocol::UNDEFINED && url.Port.get() == 443);

			// try to read proxy setup from environment
			auto sProxy = kGetEnv(bIsHTTPS ? "HTTPS_PROXY" : "HTTP_PROXY");

			if (!sProxy.empty())
			{
				KURL Proxy(sProxy);

				if (!Proxy.empty())
				{
					return Connect(url, Proxy);
				}
			}
		}
	}

	return Connect(KIOStreamSocket::Create(url, false, m_StreamOptions));

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(const KURL& url, const KURL& Proxy)
//-----------------------------------------------------------------------------
{
	if (Proxy.empty())
	{
		return Connect(KIOStreamSocket::Create(url, false, m_StreamOptions));
	}

	// which protocol on which connection segment?
	bool bTargetIsHTTPS = url.Protocol   == url::KProtocol::HTTPS || (url::KProtocol::UNDEFINED && url.Port.get()   == 443);
	bool bProxyIsHTTPS  = Proxy.Protocol == url::KProtocol::HTTPS || (url::KProtocol::UNDEFINED && Proxy.Port.get() == 443);

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
	if (!Connect(KIOStreamSocket::Create(Proxy, bProxyIsHTTPS || bTargetIsHTTPS, m_StreamOptions)))
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

			m_Connection->SetProxiedEndPoint(url);
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
	m_HTTP2.reset();
	m_Connection.reset();

	return true;

} // Disconnect

//-----------------------------------------------------------------------------
KHTTPClient& KHTTPClient::SetVerifyCerts(bool bYesNo)
//-----------------------------------------------------------------------------
{
	if (bYesNo)
	{
		m_StreamOptions.Set(KHTTPStreamOptions::VerifyCert);
	}
	else
	{
		m_StreamOptions.Unset(KHTTPStreamOptions::VerifyCert);
	}

	return *this;

} // SetVerifyCerts

//-----------------------------------------------------------------------------
/// Shall the server Certs be verified?
bool KHTTPClient::GetVerifyCerts() const
//-----------------------------------------------------------------------------
{
	return m_StreamOptions.IsSet(KHTTPStreamOptions::VerifyCert);
}

//-----------------------------------------------------------------------------
KHTTPClient& KHTTPClient::SetTimeout(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	// just in case we're not yet connected..
	m_StreamOptions.SetTimeout(Timeout);
	// if we're connected, set the timeout in the stream class
	if (m_Connection)
	{
		m_Connection->SetTimeout(Timeout);
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
#if DEKAF2_HAS_NGHTTP2 || DEKAF2_HAS_NGHTTP3
	m_RequestURL   = url;
#endif

	bool bIsConnect { false };

	if (DEKAF2_UNLIKELY(method == KHTTPMethod::CONNECT))
	{
		// for HTTPS proxying the CONNECT query has the server
		// domain and port
		Request.Endpoint = url;

		if (Request.Endpoint.empty())
		{
			return SetError("Endpoint is empty with CONNECT method");
		}

		bIsConnect = true;
		AddHeader(KHTTPHeader::PROXY_CONNECTION, "keep-alive");
	}
	else if (m_bUseHTTPProxyProtocol)
	{
		// for HTTP proxying the resource request string has to include
		// the server domain and port
		Request.Endpoint = url;
		Request.Resource = url;

		if (Request.Endpoint.empty())
		{
			return SetError("Endpoint is empty in HTTP proxy mode");
		}

		// resource may be empty..

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
	if (m_bHaveHostSet)
	{
		kDebug(2, "host already set by user to: host: {}", Request.Headers.Get(KHTTPHeader::HOST));
	}
	else if (!m_sForcedHost.empty())
	{
		kDebug(2, "host forced by user to: host: {}", m_sForcedHost);
		Request.Headers.Set(KHTTPHeader::HOST, m_sForcedHost);
	}
	else if (url.Domain.empty())
	{
		return SetError("Domain is empty");
	}
	else if (!bForcePort
		&& (url.Port.empty()
		|| (url.Protocol == url::KProtocol::HTTP  && url.Port.get() ==  80)
		|| (url.Protocol == url::KProtocol::HTTPS && url.Port.get() == 443)))
	{
		// domain alone is sufficient for standard ports
		Request.Headers.Set(KHTTPHeader::HOST, url.Domain.Serialize());
	}
	else
	{
		// build "domain:port"
		KString sHost;
		url.Domain.Serialize(sHost);
		url.Port.Serialize(sHost);

		Request.Headers.Set(KHTTPHeader::HOST, sHost);
	}

	return true;

} // SetHostHeader

//-----------------------------------------------------------------------------
/// Adds a request header for the next request
KHTTPClient& KHTTPClient::AddHeader(KHTTPHeader Header, KStringView svValue)
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(Header == KHTTPHeader::HOST))
	{
		m_bHaveHostSet = true;
	}

	Request.Headers.Set(std::move(Header), svValue);
	return *this;

} // AddHeader

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
/// Request response compression. Default is true.
KHTTPClient& KHTTPClient::RequestCompression(bool bYesNo, KStringView sCompressors)
//-----------------------------------------------------------------------------
{
	m_bRequestCompression = bYesNo;
	m_sCompressors.clear();

	for (const auto sCompressor : sCompressors.Split(",;"))
	{
		// check validity of compressor name
		if (KHTTPCompression::FromString(sCompressor) != KHTTPCompression::NONE)
		{
			if (!m_sCompressors.empty())
			{
				m_sCompressors += ", ";
			}
			m_sCompressors += sCompressor;
		}
	}

	if (m_bRequestCompression && !m_sCompressors.empty())
	{
		kDebug(2, "selecting new compressors: {}", m_sCompressors);
	}

	return *this;

} // RequestCompression

//-----------------------------------------------------------------------------
bool KHTTPClient::SetupAutomaticHeaders(KStringView* svPostData, KInStream* PostDataStream, std::size_t iBodySize, const KMIME& Mime)
//-----------------------------------------------------------------------------
{
	// remove remaining automatic headers from previous requests
	Request.Headers.Remove(KHTTPHeader::CONTENT_LENGTH);
	Request.Headers.Remove(KHTTPHeader::CONTENT_TYPE);

	if (Request.GetHTTPVersion() == KHTTPVersion::none)
	{
		// the request's http version got reset - make sure it's at the right value
#if DEKAF2_HAS_NGHTTP3
		Request.SetHTTPVersion(m_HTTP3 ? KHTTPVersion::http3 : KHTTPVersion::http11);
#endif
#if DEKAF2_HAS_NGHTTP2
		if (Request.GetHTTPVersion() != KHTTPVersion::http3)
		{
			Request.SetHTTPVersion(m_HTTP2 ? KHTTPVersion::http2 : KHTTPVersion::http11);
		}
#else
		Request.SetHTTPVersion(KHTTPVersion::http11);
#endif
	}

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
		// We allow sending body data for GET requests as well, as a few
		// applications expect doing so. It is not generally advisable due
		// to proxy issues though.
		if (iBodySize == npos)
		{
			// the http version is already set, as, if http2 is permitted, the negotiation
			// happens during the initial connect
			if ((Request.GetHTTPVersion() & KHTTPVersion::http11) == KHTTPVersion::http11)
			{
				// http2/3 do not support chunking (as it is built into the protocol)
				AddHeader(KHTTPHeader::TRANSFER_ENCODING, "chunked");
			}
		}
		else if (iBodySize > 0 || Request.Method != KHTTPMethod::GET)
		{
			AddHeader(KHTTPHeader::CONTENT_LENGTH, KString::to_string(iBodySize));
		}

		if (Mime != KMIME::NONE && (iBodySize > 0 || Request.Method != KHTTPMethod::GET || PostDataStream))
		{
			AddHeader(KHTTPHeader::CONTENT_TYPE, Mime.Serialize());
		}
	}
	else
	{
		if (iBodySize > 0)
		{
			kDebug(1, "cannot send body data with {} request, data ignored", Request.Method.Serialize())
			iBodySize = 0;
		}
	}

	if (m_Authenticator)
	{
		if (m_Authenticator->NeedsContentData())
		{
			if (!svPostData)
			{
				// we cannot authenticate a stream if the content needs to be part of the authentication
				return SetError("cannot authenticate stream with given authentication method");
			}
			AddHeader(KHTTPHeader::AUTHORIZATION, m_Authenticator->GetAuthHeader(Request, *svPostData));
		}
		else
		{
			AddHeader(KHTTPHeader::AUTHORIZATION, m_Authenticator->GetAuthHeader(Request, ""));
		}
	}

	if (m_bRequestCompression && Request.Method != KHTTPMethod::CONNECT)
	{
		AddHeader(KHTTPHeader::ACCEPT_ENCODING,
				  m_sCompressors.empty() ? KHTTPCompression::GetCompressors() : m_sCompressors.ToView());
	}

	// and make sure we always have a user agent set
	if (Request.Headers.Get(KHTTPHeader::USER_AGENT).empty())
	{
		Request.Headers.Set(KHTTPHeader::USER_AGENT, "dekaf/" DEKAF_VERSION );
	}

	return true;

} // SetupAutomaticHeaders

//-----------------------------------------------------------------------------
bool KHTTPClient::SendRequest(KStringView* svPostData, KInStream* PostDataStream, std::size_t len, const KMIME& Mime)
//-----------------------------------------------------------------------------
{
	Response.clear();

	if (!SetupAutomaticHeaders(svPostData, PostDataStream, len, Mime))
	{
		return false;
	}

#ifdef DEKAF2_HAS_NGHTTP2
	if (m_HTTP2)
	{
		// send the request via http2
		std::unique_ptr<KDataProvider> DataProvider;

		if (len > 0)
		{
			if (svPostData)
			{
				DataProvider = std::make_unique<KViewProvider>(*svPostData);
			}
			else if (PostDataStream)
			{
				if (len != npos) return SetError("API error");
				DataProvider = std::make_unique<KIStreamProvider>(*PostDataStream);
			}
			else
			{
				return SetError("API error");
			}
		}

		m_HTTP2->StreamID = m_HTTP2->Session.SubmitRequest(m_RequestURL,
														   Request.Method,
														   Request,
														   std::move(DataProvider),
														   Response);

		if (m_HTTP2->StreamID < 0)
		{
			return SetError(m_HTTP2->Session.CopyLastError());
		}

		return Parse();
	}
	else
#endif

#ifdef DEKAF2_HAS_NGHTTP3
	if (m_HTTP3)
	{
		// send the request via http3
		std::unique_ptr<KDataProvider> DataProvider;

		if (len > 0)
		{
			if (svPostData)
			{
				kDebug(1, "using a KViewProvider");
				DataProvider = std::make_unique<KViewProvider>(*svPostData);
			}
			else if (PostDataStream)
			{
				if (len != npos) return SetError("API error");
				kDebug(1, "using a KIStreamProvider");
				DataProvider = std::make_unique<KIStreamProvider>(*PostDataStream);
			}
			else
			{
				return SetError("API error");
			}
		}

		m_HTTP3->StreamID = m_HTTP3->Session.SubmitRequest(m_RequestURL,
														   Request.Method,
														   Request,
														   std::move(DataProvider),
														   Response);

		if (m_HTTP3->StreamID < 0)
		{
			return SetError(m_HTTP3->Session.CopyLastError());
		}

		return Parse();
	}
#endif

	// send the request headers to the remote server
	if (!Serialize())
	{
		kDebugLog(2, "cannot write header");
		return false;
	}

	if (len > 0)
	{
		if (kWouldLog(2))
		{
			if (len != npos)
			{
				kDebug(2, "sending {} bytes of body with mime '{}'", len, Mime);

				if (svPostData)
				{
					kDebug(3, svPostData->Left(4096));
				}
			}
			else
			{
				kDebug(2, "sending streamed body with mime '{}'", Mime);
			}
		}

		// write the content, either from a string view or a stream
		auto iWrote = (svPostData) ? Request.Write(*svPostData) : Request.Write(*PostDataStream, len);

		if (iWrote != len)
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

} // SendRequest

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
	if ((Request.GetHTTPVersion() & (KHTTPVersion::http2 | KHTTPVersion::http3)) == 0)
	{
		Request.Flush();
	}

	m_bKeepAlive = true;

	if (!Response.Parse())
	{
		return SetNetworkError(true, m_Connection->HasError() ? m_Connection->GetLastError() : Response.GetLastError() );
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

#ifdef DEKAF2_HAS_NGHTTP2
//-----------------------------------------------------------------------------
std::streamsize KHTTPClient::HTTP2StreamReader(void* buf, std::streamsize size, void* ptr)
//-----------------------------------------------------------------------------
{
	auto StreamInfo = static_cast<HTTP2Session*>(ptr);
	return StreamInfo->Session.ReadData(StreamInfo->StreamID, buf, size);

} // HTTP2StreamReader
#endif

#ifdef DEKAF2_HAS_NGHTTP3
//-----------------------------------------------------------------------------
std::streamsize KHTTPClient::HTTP3StreamReader(void* buf, std::streamsize size, void* ptr)
//-----------------------------------------------------------------------------
{
	auto StreamInfo = static_cast<HTTP3Session*>(ptr);
	return StreamInfo->Session.ReadData(StreamInfo->StreamID, buf, size);

} // HTTP2StreamReader
#endif

//-----------------------------------------------------------------------------
bool KHTTPClient::CheckForRedirect(KURL& URL, KHTTPMethod& RequestMethod, bool bNoHostChange)
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
					// any user set host headers are invalid if the redirection included
					// domain, port, or protocol
					if ((!Redirect.Domain.empty()   && Redirect.Domain   != URL.Domain) ||
						(!Redirect.Port.empty()     && Redirect.Port     != URL.Port) ||
					    (!Redirect.Protocol.empty() && Redirect.Protocol != URL.Protocol))
					{
						m_bHaveHostSet = false;
						m_sForcedHost.clear();
					}

					if (Redirect.Protocol.empty())
					{
						Redirect.Protocol = URL.Protocol;
					}
					else if (bNoHostChange)
					{
						return SetError("not allowed to redirect protocol");
					}
					if (Redirect.Query.empty())
					{
						Redirect.Query = URL.Query;
					}
					if (Redirect.Port.empty() && !URL.Port.empty() && Redirect.Domain.empty())
					{
						Redirect.Port = URL.Port;
					}
					else if (bNoHostChange)
					{
						return SetError("not allowed to redirect port");
					}
					if (Redirect.Domain.empty())
					{
						Redirect.Domain = URL.Domain;
					}
					else if (bNoHostChange)
					{
						return SetError("not allowed to redirect domain");
					}
					// we deliberately drop username and password in a redirection
					// and therefore do not copy them into the Redirect KURL value

					kDebug(1, "HTTP-{} redirect from {} to {}",
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

	if (EndPoint == m_Connection->GetEndPoint())
	{
		kDebug(2, "already connected to {}", EndPoint);
		return true;
	}

	return false;

} // AlreadyConnected

//-----------------------------------------------------------------------------
bool KHTTPClient::SetNetworkError(bool bRead, KStringViewZ sError)
//-----------------------------------------------------------------------------
{
	if (!Response.GetStatusCode() || Response.Good())
	{
		Response.SetStatus(KHTTPError::H5xx_READTIMEOUT,
						   kFormat("NETWORK {} ERROR", bRead ? "READ" : "WRITE"));
	}

	m_bKeepAlive = false;

	return SetError(sError);

} // SetNetworkError

//-----------------------------------------------------------------------------
const KTCPEndPoint& KHTTPClient::GetConnectedEndpoint() const
//-----------------------------------------------------------------------------
{
	if (m_Connection)
	{
		return m_Connection->GetEndPoint();
	}
	else
	{
		return s_EmptyEndpoint;
	}

} // GetConnectedEndpoint

//-----------------------------------------------------------------------------
const KTCPEndPoint& KHTTPClient::GetEndpointAddress() const
//-----------------------------------------------------------------------------
{
	if (m_Connection)
	{
		return m_Connection->GetEndPointAddress();
	}
	else
	{
		return s_EmptyEndpoint;
	}

} // GetEndpointAddress

KTCPEndPoint KHTTPClient::s_EmptyEndpoint;

DEKAF2_NAMESPACE_END
