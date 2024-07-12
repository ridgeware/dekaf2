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

/// @file khttpclient.h
/// HTTP client implementation - low level

#include "kstring.h"
#include "kstringview.h"
#include "kconnection.h"
#include "khttp_response.h"
#include "khttp_request.h"
#include "khttp_method.h"
#include "kjson.h"
#include "kmime.h"
#include "kurl.h"
#include "kconfiguration.h"

#ifdef DEKAF2_HAS_NGHTTP2
	#include "khttp2.h"
	#include "kstreambuf.h"
	#include <istream>
#endif

#ifdef DEKAF2_IS_WINDOWS
	// Windows has a DELETE macro in winnt.h which interferes with
	// dekaf2::KHTTPMethod::DELETE (macros are evil!)
	#ifdef DELETE
		#undef DELETE
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// low level implementation of a HTTP client
class DEKAF2_PUBLIC KHTTPClient
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

		virtual ~Authenticator() = default;
		/// return the completed auth header as a string
		virtual const KString& GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody) = 0;
		/// check if this authenticator needs the content data to compute
		/// @return true if it needs content data, false otherwise
		virtual bool NeedsContentData() const { return false; }

	}; // Authenticator

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// Basic authentication method
	class BasicAuthenticator : public Authenticator
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		/// default ctor
		BasicAuthenticator() = default;
		/// construct basic authenticator from username and password
		/// @param _sUsername the username for basic auth
		/// @param _sPassword the password for basic auth
		BasicAuthenticator(KString _sUsername, KString _sPassword);
		virtual const KString& GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody) override;

		/// the username
		KString sUsername;
		/// the password
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

		/// default ctor
		DigestAuthenticator() = default;
		/// construct from parameters
		DigestAuthenticator(KString _sUsername,
							KString _sPassword,
							KString _sRealm,
							KString _sNonce,
							KString _sOpaque,
							KString _sQoP);
		virtual const KString& GetAuthHeader(const KOutHTTPRequest& Request, KStringView sBody) override;
		virtual bool NeedsContentData() const override;

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
	/// default ctor
	KHTTPClient(TLSOptions Options = TLSOptions::DefaultsForHTTP);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server and sets method and resource
	KHTTPClient(const KURL& url, KHTTPMethod method = KHTTPMethod::GET, TLSOptions Options = TLSOptions::DefaultsForHTTP);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Ctor, connects to a server via proxy and sets method and resource
	KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method = KHTTPMethod::GET, TLSOptions Options = TLSOptions::DefaultsForHTTP);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// old ctor -- DEPRECATED, use the variant with TLSOptions
	KHTTPClient(bool bVerifyCerts)
	//-----------------------------------------------------------------------------
	: KHTTPClient(BoolToOptions(bVerifyCerts))
	{
	}

	//-----------------------------------------------------------------------------
	/// old ctor -- DEPRECATED, use the variant with TLSOptions
	KHTTPClient(const KURL& url, KHTTPMethod method, bool bVerifyCerts)
	//-----------------------------------------------------------------------------
	: KHTTPClient(url, method, BoolToOptions(bVerifyCerts))
	{
	}

	//-----------------------------------------------------------------------------
	/// old ctor -- DEPRECATED, use the variant with TLSOptions
	KHTTPClient(const KURL& url, const KURL& Proxy, KHTTPMethod method, bool bVerifyCerts)
	//-----------------------------------------------------------------------------
	: KHTTPClient(url, Proxy, method, BoolToOptions(bVerifyCerts))
	{
	}

	//-----------------------------------------------------------------------------
	/// Ctor, takes an existing connection to a server
	KHTTPClient(std::unique_ptr<KConnection> stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient(const KHTTPClient&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move ctor
	KHTTPClient(KHTTPClient&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KHTTPClient& operator=(const KHTTPClient&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// move assignment
	KHTTPClient& operator=(KHTTPClient&&) = delete;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connect with a KConnection object
	bool Connect(std::unique_ptr<KConnection> Connection);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connect with a KURL object (or a string)
	bool Connect(const KURL& url);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connect with a KURL object (or a string) through a KURL proxy (or a string)
 	bool Connect(const KURL& url, const KURL& Proxy);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect connection
	bool Disconnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the resource to be requested - for proxied connections, this also sets the proxied TCP endpoint!
	bool Resource(const KURL& url, KHTTPMethod method = KHTTPMethod::GET);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Adds a request header for the next request
	KHTTPClient& AddHeader(KHTTPHeader Header, KStringView svValue);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Send the HTTP request, read request body from a stream
	/// @param PostDataStream a stream with the POST/PUT data
	/// @param len the length to send, or npos (default) for all available
	/// @param Mime the KMIME type for the request body, defaults to KMIME::TEXT_PLAIN
	/// @return true on success
	bool SendRequest(KInStream& PostDataStream, size_t len = npos, const KMIME& Mime = KMIME::TEXT_PLAIN)
	//-----------------------------------------------------------------------------
	{
		return SendRequest(nullptr, &PostDataStream, len, Mime);
	}

	//-----------------------------------------------------------------------------
	/// Send the HTTP request, read request body from a string view
	/// @param svPostData a string view with the POST/PUT data
	/// @param Mime the KMIME type for the request body, defaults to KMIME::TEXT_PLAIN
	/// @return true on success
	bool SendRequest(KStringView svPostData = KStringView{}, const KMIME& Mime = KMIME::TEXT_PLAIN)
	//-----------------------------------------------------------------------------
	{
		return SendRequest(&svPostData, nullptr, svPostData.size(), Mime);
	}

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
	size_t Write(KInStream& stream, size_t len = npos)
	//-----------------------------------------------------------------------------
	{
		return Request.Write(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Stream into outstream, returns read character count
	size_t Read(KOutStream& stream, size_t len = npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Read(stream, len);
	}

	//-----------------------------------------------------------------------------
	/// Append to sBuffer, returns read character count
	size_t Read(KStringRef& sBuffer, size_t len = npos)
	//-----------------------------------------------------------------------------
	{
		return Response.Read(sBuffer, len);
	}

	//-----------------------------------------------------------------------------
	/// Read one line into sBuffer, including EOL
	bool ReadLine(KStringRef& sBuffer)
	//-----------------------------------------------------------------------------
	{
		return Response.ReadLine(sBuffer);
	}

	//-----------------------------------------------------------------------------
	/// Is the HTTP connection still good?
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Connection && m_Connection->Good();
	}

	//-----------------------------------------------------------------------------
	/// Returns the real connection endpoint (may have changed through redirections etc)
	const KTCPEndPoint& GetConnectedEndpoint() const
	//-----------------------------------------------------------------------------
	{
		static KTCPEndPoint s_EmptyEndpoint;

		return Good() ? m_Connection->EndPoint() : s_EmptyEndpoint;
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
	/// Returns HTTP error string if any
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
	/// Return HTTP status string from last request
	KStringView GetStatusString() const
	//-----------------------------------------------------------------------------
	{
		return Response.GetStatusString();
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
	/// Set extended options for TLS connections, like cert verification, HTTP2, or manual handshaking -
	/// must be set before connection
	self& SetTLSOptions(TLSOptions Options)
	//-----------------------------------------------------------------------------
	{
		m_TLSOptions = kGetTLSDefaults(Options);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Get extended options for TLS connections, like cert verification, HTTP2, or manual handshaking
	TLSOptions GetTLSOptions() const
	//-----------------------------------------------------------------------------
	{
		return m_TLSOptions;
	}

	//-----------------------------------------------------------------------------
	/// Shall the server Certs be verified? -- DEPRECATED, use SetVerifyCerts()
	self& VerifyCerts(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		return SetVerifyCerts(bYesNo);
	}

	//-----------------------------------------------------------------------------
	/// Set server cert verification -
	/// must be set before connection
	self& SetVerifyCerts(bool bYesNo);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get server cert verification setting
	bool GetVerifyCerts() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set a connection timeout in seconds
	self& SetTimeout(int iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Request response compression. Default is true.
	/// @param bYesNo if true, request response compression
	/// @param sCompressors comma separated list of compression algorithms, or empty for all supported
	/// algorithms (zstd, br, xz, lzma, gzip, bzip2, deflate)
	self& RequestCompression(bool bYesNo, KStringView sCompressors = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Uncompress incoming response if compressed? Default is true
	self& AllowUncompression(bool bYesNo)
	//-----------------------------------------------------------------------------
	{
		Response.AllowUncompression(bYesNo);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// Compress outgoing request? Default is true
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
	self& ForceHostHeader(KString sHost)
	//-----------------------------------------------------------------------------
	{
		m_sForcedHost = std::move(sHost);
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
	bool SetError(KString sError) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Mark an IO error
	bool SetNetworkError(bool bRead, KString sError);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if we are already connected to the endpoint
	bool AlreadyConnected(const KTCPEndPoint& EndPoint) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Returns true if url is not exempt from proxying through the comma delimited
	/// sNoProxy list. A leading dot means that only the end of the strings are
	/// compared.
	static bool FilterByNoProxyList(const KURL& url, KStringView sNoProxy);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// is response status code one of those we accept for redirection?
	bool StatusIsRedirect() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// sets a redirect target depending on the returned status code and location
	/// header, possibly changing RequestMethod. If bNoHostChange is set, no change in protocol, domain
	/// and port are allowed
	bool CheckForRedirect(KURL& URL, KHTTPMethod& RequestMethod, bool bNoHostChange = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// send a request either with a stream to read body data from, or a stringview
	bool SendRequest(KStringView* svPostData, KInStream* PostDataStream, std::size_t len, const KMIME& Mime);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static inline TLSOptions BoolToOptions(bool bVerifyCerts)
	//-----------------------------------------------------------------------------
	{
		return bVerifyCerts ? TLSOptions::DefaultsForHTTP | TLSOptions::VerifyCert : TLSOptions::DefaultsForHTTP;
	}

//------
private:
//------

	//-----------------------------------------------------------------------------
	/// sets the Host: header, leaves port out if standard and bForcePort is false
	DEKAF2_PRIVATE bool SetHostHeader(const KURL& url, bool bForcePort = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE bool SetupAutomaticHeaders(KStringView* svPostData, KInStream* PostDataStream, std::size_t iBodySize, const KMIME& Mime);
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_NGHTTP2
	//-----------------------------------------------------------------------------
	// the streambuf reader for KInStreamBuf
	static std::streamsize HTTP2StreamReader(void* buf, std::streamsize size, void* ptr);
	//-----------------------------------------------------------------------------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct HTTP2Session
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		HTTP2Session(KTLSIOStream& TLSStream);

		http2::SingleStreamSession Session;
		KInStreamBuf               StreamBuf;
		std::istream               IStream;
		KInStream                  InStream;
		int32_t                    StreamID { -1 };

	}; // HTTP2Session

	std::unique_ptr<HTTP2Session>  m_HTTP2;
	KURL                           m_RequestURL;
#endif

	std::unique_ptr<KConnection>   m_Connection;
	std::unique_ptr<Authenticator> m_Authenticator;

	mutable KString  m_sError;
	KString          m_sForcedHost;
	KString          m_sCompressors;
	KURL             m_Proxy;
	int              m_Timeout               { 30    };
	TLSOptions       m_TLSOptions            { kGetTLSDefaults(TLSOptions::DefaultsForHTTP) };
	bool             m_bRequestCompression   { true  };
	bool             m_bAutoProxy            { false };
	bool             m_bUseHTTPProxyProtocol { false };
	bool             m_bKeepAlive            { true  };
	bool             m_bHaveHostSet          { false };

//------
public:
//------

	KOutHTTPRequest  Request;
	KInHTTPResponse  Response;

}; // KHTTPClient

DEKAF2_NAMESPACE_END
