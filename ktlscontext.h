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

#include "bits/kasio.h"
#include "kstring.h"
#include "kstreamoptions.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Holds a configured tls context - will be used in the constructor of KTLSIOStream
class DEKAF2_PUBLIC KTLSContext
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	enum Transport
	{
		Tcp,
		Quic
	};

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
	/// Set the ALPN data. This API expects a vector of KStringViews and transforms it into the internal
	/// ALPN format.
	/// This method is mutually exclusive with SetAllowHTTP2()
	bool SetALPN(const std::vector<KStringView> ALPNs)
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
	/// Get error string, if any
	const KString& Error() const
	//-----------------------------------------------------------------------------
	{
		return m_sError;
	}

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	static ::SSL_CTX* CreateContext(bool bIsServer, Transport transport);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool SetError(KString sError);
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

	KString m_sError;
#if (DEKAF2_CLASSIC_ASIO)
	static boost::asio::io_service s_IO_Service;
#endif
	boost::asio::ssl::context m_Context;
	boost::asio::ssl::stream_base::handshake_type m_Role;
	std::string m_sPassword;

}; // KTLSContext

DEKAF2_NAMESPACE_END


