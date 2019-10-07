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

#pragma once

#include "kstringview.h"
#include "kconnection.h"
#include "khttp_response.h"
#include "khttp_request.h"
#include "khttp_method.h"
#include "kmime.h"
#include "kurl.h"

/// @file khttpclient.h
/// HTTP client implementation - low level

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KHTTPClient
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	using self = KHTTPClient;

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// ABC for authenticators
	class Authenticator
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		virtual ~Authenticator();
		virtual const KString& GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody) = 0;

	}; // Authenticator

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Basic authentication method
	class BasicAuthenticator : public Authenticator
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		BasicAuthenticator() = default;
		BasicAuthenticator(KString _sUsername, KString _sPassword);
		virtual const KString& GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody) override;

		KString sUsername;
		KString sPassword;

	//------
	protected:
	//------

		KString sResponse;

	}; // BasicAuthenticator

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Digest access authentication method
	class DigestAuthenticator : public BasicAuthenticator
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		DigestAuthenticator() = default;
		DigestAuthenticator(KString _sUsername,
							KString _sPassword,
							KString _sRealm,
							KString _sNonce,
							KString _sOpaque,
							KString _sQoP);
		virtual const KString& GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody) override;

		KString sRealm;
		KString sNonce;
		KString sOpaque;
		KString sQoP { "auth" };

	//------
	protected:
	//------

		uint16_t iNonceCount { 0 };

	}; // DigestAuthenticator

	//-----------------------------------------------------------------------------
	KHTTPClient(bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server and sets method and resource
	KHTTPClient(const KURL& url, KHTTPMethod method = KHTTPMethod::GET, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server via proxy and sets method and resource
	KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method = KHTTPMethod::GET, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, takes an existing connection to a server
	KHTTPClient(std::unique_ptr<KConnection> stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient(const KHTTPClient&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient(KHTTPClient&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient& operator=(const KHTTPClient&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient& operator=(KHTTPClient&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(std::unique_ptr<KConnection> Connection);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KURL& url);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KURL& url, const KURL& Proxy);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the resource to be requested
	bool Resource(const KURL& url, KHTTPMethod method = KHTTPMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Adds a request header for the next request
	bool SetRequestHeader(KStringView svName, KStringView svValue, bool bOverwrite = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SendRequest(KStringView svPostData = KStringView{}, KMIME Mime = KMIME::TEXT_PLAIN);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// write request headers (and setup the filtered output stream)
	bool Serialize();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// receive response and headers (and setup the filtered input stream)
	bool Parse();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// POST/PUT from stream
	size_t Write(KInStream& stream, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Request.Write(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Stream into outstream
	size_t Read(KOutStream& stream, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Read(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Append to sBuffer
	size_t Read(KString& sBuffer, size_t len = KString::npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Read(sBuffer, len);
	}

	//-----------------------------------------------------------------------------
	/// Read one line into sBuffer, including EOL
	bool ReadLine(KString& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Response.ReadLine(sBuffer);
	}

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Connection && m_Connection->Good();
	}

	//-----------------------------------------------------------------------------
	/// evaluates the http status code after a request and returns true 200 >= code <= 299
	bool HttpSuccess() const
	//-----------------------------------------------------------------------------
	{
		return Response.Good();
	}

	//-----------------------------------------------------------------------------
	/// evaluates the http status code after a request and returns true if !HttpSuccess()
	bool HttpFailure() const
	//-----------------------------------------------------------------------------
	{
		return !(HttpSuccess());
	}

	//-----------------------------------------------------------------------------
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

	//-----------------------------------------------------------------------------
	/// Clear all headers, resource, and error. Keep connection
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Return HTTP status code from last request
	uint16_t GetStatusCode() const
	//-----------------------------------------------------------------------------
	{
		return Response.iStatusCode;
	}

	//-----------------------------------------------------------------------------
	/// Set proxy server for all subsequent connects
	self& SetProxy(KURL Proxy)
	//-----------------------------------------------------------------------------
	{
		m_Proxy = std::move(Proxy);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Shall the server Certs be verified?
	self& VerifyCerts(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bVerifyCerts = bYesNo;
		return *this;
	}

	//-----------------------------------------------------------------------------
	self& SetTimeout(int iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self& RequestCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		m_bRequestCompression = bYesNo;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// uncompress incoming response?
	self& AllowUncompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Response.AllowUncompression(bYesNo);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// compress outgoing request?
	self& AllowCompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Request.AllowCompression(bYesNo);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Allow auto configuration of proxy server from environment variables?
	self& AutoConfigureProxy(bool bYes = true)
	//-----------------------------------------------------------------------------
	{
		m_bAutoProxy = bYes;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Allows to manually configure a host header that is not derived from the
	/// connected URL
	self& ForceHostHeader(KStringView sHost)
	//-----------------------------------------------------------------------------
	{
		m_sForcedHost = sHost;
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Create a derived authentication object that shall be used for authentication
	self& Authentication(std::unique_ptr<Authenticator> _Authenticator)
	//-----------------------------------------------------------------------------
	{
		m_Authenticator = std::move(_Authenticator);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Forces basic authentication header
	self& BasicAuthentication(KString sUsername,
							  KString sPassword);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Forces digest authentication header
	self& DigestAuthentication(KString sUsername,
							   KString sPassword,
							   KString sRealm,
							   KString sNonce,
							   KString sOpaque,
							   KString sQoP = "auth");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Removes previously set authentication header
	self& ClearAuthentication();
	//-----------------------------------------------------------------------------

//------
protected:
//------
 
	//-----------------------------------------------------------------------------
	/// Set an error string
	bool SetError(KStringView sError) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if we are already connected to the endpoint in URL
	bool AlreadyConnected(const KURL& URL) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if url is not exempt from proxying through the comma delimited
	/// sNoProxy list. A leading dot means that only the end of the strings are
	/// compared.
	static bool FilterByNoProxyList(const KURL& url, KStringView sNoProxy);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool CheckForRedirect(KURL& URL, KStringView& sRequestMethod);
	//-----------------------------------------------------------------------------

//------
private:
//------

	//-----------------------------------------------------------------------------
	bool SetHostHeader(const KURL& url, bool bForcePort = false);
	//-----------------------------------------------------------------------------

	std::unique_ptr<KConnection> m_Connection;
	std::unique_ptr<Authenticator> m_Authenticator;
	mutable KString m_sError;
	KString m_sForcedHost;
	KURL m_Proxy;
	int  m_Timeout { 30 };
	bool m_bVerifyCerts { false };
	bool m_bRequestCompression { true };
	bool m_bAutoProxy { false };
	bool m_bUseHTTPProxyProtocol { false };

//------
public:
//------

	KOutHTTPRequest Request;
	KInHTTPResponse Response;

}; // KHTTPClient

} // end of namespace dekaf2
