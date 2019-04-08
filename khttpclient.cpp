/*
//-----------------------------------------------------------------------------//
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

namespace dekaf2 {


//-----------------------------------------------------------------------------
void KHTTPClient::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();
	m_sError.clear();
	m_bRequestCompression = true;

	// do not reset m_bUseHTTPProxyProtocol here - it stays valid until
	// the setup of a new connection

} // clear

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(const KURL& url, KHTTPMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	if (Connect(url, bVerifyCerts))
	{
		Resource(url, method);
	}

} // Ctor

//-----------------------------------------------------------------------------
KHTTPClient::KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	if (Connect(url, Proxy, bVerifyCerts))
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
	if (!sNoProxy.empty())
	{
		std::vector<KStringView> NoProxy;

		kSplit(NoProxy, sNoProxy);

		for (auto sDomain : NoProxy)
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
	}

	return true;

} // FilterByNoProxyList

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(const KURL& url, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	if (!m_Proxy.empty())
	{
		return Connect(url, m_Proxy, bVerifyCerts);
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
				return Connect(url, Proxy, bVerifyCerts);
			}
		}
	}

	return Connect(KConnection::Create(url, false, bVerifyCerts));

} // Connect

//-----------------------------------------------------------------------------
bool KHTTPClient::Connect(const KURL& url, const KURL& Proxy, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	if (Proxy.empty())
	{
		return Connect(KConnection::Create(url, false, bVerifyCerts));
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
	if (!Connect(KConnection::Create(Proxy, bProxyIsHTTPS || bTargetIsHTTPS, bVerifyCerts)))
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
void KHTTPClient::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	m_Timeout = iSeconds;

	if (m_Connection)
	{
		m_Connection->SetTimeout(iSeconds);
	}

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
bool KHTTPClient::SendRequest(KStringView svPostData, KMIME Mime)
//-----------------------------------------------------------------------------
{
	Response.clear();

	if (Request.Resource.empty() && Request.Method != KHTTPMethod::CONNECT)
	{
		return SetError("no resource");
	}

	if (!m_Connection || !m_Connection->Good())
	{
		return SetError("no stream");
	}

	if (   Request.Method != KHTTPMethod::GET
		&& Request.Method != KHTTPMethod::HEAD
		&& Request.Method != KHTTPMethod::OPTIONS
		&& Request.Method != KHTTPMethod::CONNECT)
	{
		SetRequestHeader(KHTTPHeaders::CONTENT_LENGTH, KString::to_string(svPostData.size()));

		if (!svPostData.empty())
		{
			SetRequestHeader(KHTTPHeaders::CONTENT_TYPE, Mime);
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

	if (m_bRequestCompression && Request.Method != KHTTPMethod::CONNECT)
	{
		SetRequestHeader(KHTTPHeaders::ACCEPT_ENCODING, "gzip");
	}

	if (!Request.Serialize()) // this sends the request headers to the remote server
	{
		return SetError(Request.Error());
	}

	if (!svPostData.empty())
	{
		kDebug(2, "sending {} bytes of {} data", svPostData.size(), KStringView(Mime));
		Request.Write(svPostData);
		// We only need to flush if we have content data, as Request.Serialize()
		// already flushes after the headers are written.
		// Request.close() closes the output transformations, which flushes their
		// pipeline into the output stream, and then calls flush() on the output
		// stream.
		Request.close();
	}

	if (!m_Connection->Good())
	{
		return SetError("write error");
	}

	return ReadHeader();
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

	return true;

} // Parse

//-----------------------------------------------------------------------------
bool KHTTPClient::ReadHeader()
//-----------------------------------------------------------------------------
{
	if (!Response.Parse())
	{
		SetError(Response.Error());
		return false;
	}

	return true;

} // ReadHeader

//-----------------------------------------------------------------------------
KString KHTTPClient::HttpRequest (const KURL& URL, KStringView sRequestMethod/* = KHTTPMethod::GET*/, KStringView svRequestBody/* = ""*/, KMIME MIME/* = KMIME::JSON*/, bool bVerifyCerts /* = false */)
//-----------------------------------------------------------------------------
{
	KString sResponse;

	if (Connect(URL, bVerifyCerts))
	{
		if (Resource(URL, sRequestMethod))
		{
			if (SendRequest (svRequestBody, MIME))
			{
				Read (sResponse);
			}
		}
	}

	return sResponse;

} // HttpRequest

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


//-----------------------------------------------------------------------------
KString kHTTPGet(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KHTTPClient HTTP;
	HTTP.AutoConfigureProxy(true);
	return HTTP.Get(URL);

} // kHTTPGet

//-----------------------------------------------------------------------------
bool kHTTPHead(const KURL& URL)
//-----------------------------------------------------------------------------
{
	KHTTPClient HTTP;
	HTTP.AutoConfigureProxy(true);
	return HTTP.Head(URL);

} // kHTTPHead

//-----------------------------------------------------------------------------
KString kHTTPPost(const KURL& URL, KStringView svPostData, KStringView svMime)
//-----------------------------------------------------------------------------
{
	KHTTPClient HTTP;
	HTTP.AutoConfigureProxy(true);
	return HTTP.Post(URL, svPostData, svMime);

} // kHTTPPost


} // end of namespace dekaf2
