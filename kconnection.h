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

#pragma once

#include "kstream.h"
#include "ksslstream.h"
#include "kurl.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KProxy
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KProxy() = default;
	KProxy(const KProxy&) = default;
	KProxy(KProxy&&) = default;
	KProxy& operator=(const KProxy&) = default;
	KProxy& operator=(KProxy&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KProxy(const url::KDomain& domain,
	       const url::KPort& port,
	       KStringView svUser = KStringView{},
	       KStringView svPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KProxy(KStringView svDomainAndPort,
	       KStringView svUser = KStringView{},
	       KStringView svPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool LoadFromEnv(KStringView svEnvVar = "http_proxy");
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void clear();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return Domain.empty();
	}

	url::KDomain Domain;
	url::KPort Port;
	KString User;
	KString Password;

}; // KProxy

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KConnection
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//-----------------------------------------------------------------------------
	KConnection() = default;
	KConnection(const KConnection&) = delete;
	KConnection(KConnection&&) = default;
	KConnection& operator=(const KConnection&) = delete;
	KConnection& operator=(KConnection&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KConnection(const url::KDomain& domain, const url::KPort& port)
	//-----------------------------------------------------------------------------
	{
		Connect(domain, port);
	}

	//-----------------------------------------------------------------------------
	KConnection(const KURL& URL)
	//-----------------------------------------------------------------------------
	{
		Connect(URL);
	}

	//-----------------------------------------------------------------------------
	KConnection(KStringView sURL)
	//-----------------------------------------------------------------------------
	{
		Connect(sURL);
	}

	//-----------------------------------------------------------------------------
	KConnection(KStream& Stream) noexcept
	//-----------------------------------------------------------------------------
	    : m_Stream(&Stream)
	    , m_bStreamIsNotOwned(true)
	{
	}

	//-----------------------------------------------------------------------------
	KConnection(KStream&& Stream) noexcept
	//-----------------------------------------------------------------------------
	    :  m_Stream(&Stream)
	    , m_bStreamIsNotOwned(false)
	{
	}


	//-----------------------------------------------------------------------------
	~KConnection()
	//-----------------------------------------------------------------------------
	{
		if (m_bStreamIsNotOwned)
		{
			m_Stream.release();
		}
	}

	//-----------------------------------------------------------------------------
	operator bool()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.get() != nullptr;
	}

	//-----------------------------------------------------------------------------
	KConnection& operator=(KStream& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KStream* get()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.get();
	}

	//-----------------------------------------------------------------------------
	KStream* operator->()
	//-----------------------------------------------------------------------------
	{
		return get();
	}

	//-----------------------------------------------------------------------------
	KStream& operator*()
	//-----------------------------------------------------------------------------
	{
		return *get();
	}

	//-----------------------------------------------------------------------------
	bool Connect(const url::KDomain& domain, const url::KPort& port);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KURL& URL)
	//-----------------------------------------------------------------------------
	{
		return Connect(URL.Domain, URL.Port);
	}

	//-----------------------------------------------------------------------------
	bool Connect(KStringView sURL)
	//-----------------------------------------------------------------------------
	{
		return Connect(KURL(sURL));
	}

	//-----------------------------------------------------------------------------
	void Disonnect();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void setConnection(std::unique_ptr<KStream>&& Stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static std::unique_ptr<KConnection> Create(const KURL& URL, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static std::unique_ptr<KConnection> Create(KStringView URL, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static std::unique_ptr<KConnection> Create(const KURL& URL, const KProxy& Proxy, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

//------
private:
//------

	std::unique_ptr<KStream> m_Stream;
	bool m_bStreamIsNotOwned{false};

};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSSLConnection : public KConnection
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	enum class TLS
	{
		SSLv2,
		SSLv3,
		TLSv1,
		TLSv2
	};

	//-----------------------------------------------------------------------------
	KSSLConnection() = default;
	KSSLConnection(const KSSLConnection&) = delete;
	KSSLConnection(KSSLConnection&&) = default;
	KSSLConnection& operator=(const KSSLConnection&) = delete;
	KSSLConnection& operator=(KSSLConnection&&) = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KSSLConnection(const url::KDomain& domain, const url::KPort& port, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		Connect(domain, port, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	KSSLConnection(const KURL& URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		Connect(URL, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	bool Connect(const url::KDomain& domain, const url::KPort& port, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool Connect(const KURL& URL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return Connect(URL.Domain, URL.Port, bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	bool Connect(KStringView sURL, bool bVerifyCerts = false)
	//-----------------------------------------------------------------------------
	{
		return Connect(KURL(sURL), bVerifyCerts);
	}

};

} // end of namespace dekaf2

