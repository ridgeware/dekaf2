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

/// @file ktlscontext.h
/// provides an implementation of the TLS context object

#include <dekaf2/net/tcp/bits/kasio.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/net/util/kstreamoptions.h>
#include <dekaf2/core/errors/kerror.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

struct ssl_st;

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup net_tls
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Holds a configured tls context - will be used in the constructor of KTLSIOStream
class DEKAF2_PUBLIC KTLSContext : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum Transport
	{
		Tcp,
		Quic,
		DTls
	};

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// the parts of a TLS ClientHello exposed to the SNI callback - the string views
	/// point into the ClientHello and are only valid for the duration of the callback
	struct ClientHello
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		/// returns true if sProtocol is among the ALPN protocols offered by the client
		bool HasALPN(KStringView sProtocol) const;

		/// the SNI hostname, empty if the client sent none
		KStringView              sServerName;
		/// the ALPN protocols offered by the client, empty if none were sent (or if the
		/// TLS library does not expose them, see SetSNICallback())
		std::vector<KStringView> ALPNs;

	}; // ClientHello

	/// callback type for SNI dispatch: receives the ClientHello, and returns the context
	/// to serve the connection with, or nullptr to continue with the hostname lookup and
	/// the default context
	using SNICallback = std::function<std::shared_ptr<KTLSContext>(const ClientHello&)>;

	//-----------------------------------------------------------------------------
	/// Constructs a TLS or QUIC context, depending on the pregenerated context
	KTLSContext(bool bIsServer = false, Transport transport = Transport::Tcp);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this context object for a server, set its TLS certificate files here (PEM format)
	/// The cert file may contain an appended certificate chain as well, and the key if sKey is empty.
	/// It is however important to have the cert first in the file.
	bool LoadTLSCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this context object for a server, set its TLS certificate buffers here (strings in PEM format)
	/// The cert string buffer may contain an appended certificate chain as well, and the key if sKey is empty.
	/// It is however important to have the cert first in the buffer.
	bool SetTLSCertificates(KStringView sCert, KStringView sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this context object for a server, set the pre-computed DH key exchange primes here (string in PEM format)
	bool SetDHPrimes(KStringView sDHPrimes = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the allowed Cipher suites, separated by colons (check the OpenSSL documentation for names)
	bool SetAllowedCipherSuites(KStringView sCipherSuites);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set an additional verify path (the system defaults have been set at construction)
	bool SetAdditionalTLSVerifyPath(KStringView sVerifyPath);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this context object for a server, add a context that serves connections
	/// requesting sHostname via SNI. The selected context contributes certificate, key and
	/// ALPN - protocol versions and ciphers remain those of this context.
	/// @param sHostname the SNI hostname, matched case insensitively. May be a wildcard
	/// like "*.example.com", matching exactly one label. Replaces an existing entry.
	/// @param Context a server mode context
	bool AddSNIContext(KStringView sHostname, std::shared_ptr<KTLSContext> Context);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Remove a context added with AddSNIContext(). Safe to call on a running server -
	/// in-flight handshakes keep the removed context alive until they are done.
	/// @param sHostname the SNI hostname the context was added with
	/// @returns true if an entry was removed
	bool RemoveSNIContext(KStringView sHostname);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the context that SNI dispatch selects for sHostname (exact match first, then
	/// wildcard), or nullptr if none matches (the default context serves the connection)
	std::shared_ptr<KTLSContext> GetSNIContext(KStringView sHostname) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set a callback that is asked before the hostname lookup during SNI dispatch, e.g.
	/// to serve an ACME tls-alpn-01 challenge context depending on the offered ALPN.
	/// With TLS libraries older than OpenSSL 1.1.1 the callback sees an empty ALPN list.
	/// @param Callback the callback, called concurrently from handshake threads
	bool SetSNICallback(SNICallback Callback);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the ALPN data. This API expects a vector of KStringViews and transforms it into the internal
	/// ALPN format. For client contexts this sets the protocols offered to the server. For server
	/// contexts this installs protocol selection in server preference order - a client offering
	/// ALPN without overlap is rejected with a fatal alert (RFC 7301), a client not using ALPN
	/// connects without it.
	/// This method is mutually exclusive with SetAllowHTTP2()
	bool SetALPN(const std::vector<KStringView>& ALPNs)
	//-----------------------------------------------------------------------------
	{
		return SetALPNRaw(KStreamOptions::CreateALPNString(ALPNs));
	}

	//-----------------------------------------------------------------------------
	/// Set the ALPN data. This API expects a string view and transforms it into the internal
	/// ALPN format.
	/// This method is mutually exclusive with SetAllowHTTP2()
	bool SetALPN(KStringView sALPN)
	//-----------------------------------------------------------------------------
	{
		return SetALPNRaw(KStreamOptions::CreateALPNString(sALPN));
	}

	//-----------------------------------------------------------------------------
	/// Allow to switch to HTTP2. This setting can also be applied to the KTLSIOStream class, which may be
	/// more useful for client implementations. For server contexts, it has to be set here though.
	/// This method is mutually exclusive with SetALPN()
	/// @param bAlsoAllowHTTP1 if set to false, only HTTP/2 connections are permitted. Else a fallback on
	/// HTTP/1.1 is permitted. Default is true.
	/// @returns true if protocol selection is permitted
	bool SetAllowHTTP2(bool bAlsoAllowHTTP1 = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	boost::asio::ssl::context& GetContext()
	//-----------------------------------------------------------------------------
	{
		return m_Context;
	}

	//-----------------------------------------------------------------------------
	boost::asio::ssl::stream_base::handshake_type GetRole() const
	//-----------------------------------------------------------------------------
	{
		return m_Role;
	}

	//-----------------------------------------------------------------------------
	/// Set the client side identity on a TLS connection before its handshake: the SNI
	/// hostname and, with bVerifyCert, the name the server certificate is checked against.
	/// An IP address sends no SNI (RFC 6066) and is checked against the certificate's IP
	/// addresses.
	/// @param ssl the connection
	/// @param sHostname the host to talk to, a DNS name or an IP address (IPv6 with or without brackets)
	/// @param bVerifyCert set up the certificate name check
	/// @returns the error description, empty on success
	static KString SetClientIdentity(ssl_st* ssl, KStringView sHostname, bool bVerifyCert);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	static ::SSL_CTX* CreateContext(bool bIsServer, Transport transport);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SetDefaults();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SetALPNRaw(KStringView sALPN);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	std::string PasswordCallback(std::size_t max_length, boost::asio::ssl::context::password_purpose purpose) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// install the TLS library callback for SNI dispatch (once)
	DEKAF2_PRIVATE
	bool InstallSNIDispatch();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// find the context to serve a ClientHello with, or nullptr for the default context
	DEKAF2_PRIVATE
	std::shared_ptr<KTLSContext> ResolveSNI(const ClientHello& Hello) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// SNI dispatch entry point with OpenSSL >= 1.1.1 - sees SNI and ALPN
	DEKAF2_PRIVATE
	static int ClientHelloCallback(ssl_st* ssl, int* al, void* arg);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// SNI dispatch entry point with older TLS libraries - sees only the SNI hostname
	DEKAF2_PRIVATE
	static int ServerNameCallback(ssl_st* ssl, int* al, void* arg);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// server side ALPN protocol selection
	DEKAF2_PRIVATE
	static int ALPNSelectCallback(ssl_st* ssl, const unsigned char** out, unsigned char* outlen,
	                              const unsigned char* in, unsigned int inlen, void* arg);
	//-----------------------------------------------------------------------------

	struct SNIDispatch
	{
		std::unordered_map<KString, std::shared_ptr<KTLSContext>> Hosts;
		SNICallback Callback;
		bool        bInstalled { false };
	};

#if (DEKAF2_CLASSIC_ASIO)
	static boost::asio::io_service s_IO_Service;
#endif
	boost::asio::ssl::context m_Context;
	boost::asio::ssl::stream_base::handshake_type m_Role;
	std::string m_sPassword;
	KThreadSafe<SNIDispatch> m_SNIDispatch;

}; // KTLSContext


/// @}

DEKAF2_NAMESPACE_END


