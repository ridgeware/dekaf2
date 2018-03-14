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

#include "kconnection.h"
#include "ksystem.h"


namespace dekaf2 {

//-----------------------------------------------------------------------------
KProxy::KProxy(const url::KDomain& domain,
               const url::KPort& port,
               KStringView svUser,
               KStringView svPassword)
//-----------------------------------------------------------------------------
    : Domain(domain)
    , Port(port)
    , User(svUser)
    , Password(svPassword)
{
}

//-----------------------------------------------------------------------------
KProxy::KProxy(KStringView svDomainAndPort,
               KStringView svUser,
               KStringView svPassword)
//-----------------------------------------------------------------------------
    : User(svUser)
    , Password(svPassword)
{
	Port.Parse(Domain.Parse(svDomainAndPort));
}

//-----------------------------------------------------------------------------
void KProxy::clear()
//-----------------------------------------------------------------------------
{
	Domain.clear();
	Port.clear();
	User.clear();
	Password.clear();
}

//-----------------------------------------------------------------------------
bool KProxy::LoadFromEnv(KStringView svEnvVar)
//-----------------------------------------------------------------------------
{
	Port.Parse(Domain.Parse(kGetEnv(svEnvVar)));
	return !empty();
}

//-----------------------------------------------------------------------------
KConnection& KConnection::operator=(KStream& Stream)
//-----------------------------------------------------------------------------
{
	if (m_bStreamIsNotOwned)
	{
		m_Stream.release();
	}
	m_bStreamIsNotOwned = true;
	m_Stream.reset(&Stream);
	return *this;
}

//-----------------------------------------------------------------------------
bool KConnection::Connect(const url::KDomain& domain, const url::KPort& port)
//-----------------------------------------------------------------------------
{
	m_sError.clear();

	if (m_bStreamIsNotOwned)
	{
		m_bStreamIsNotOwned = false;
		m_Stream.release();
	}

	m_Endpoint.Domain = domain;
	m_Endpoint.Port = port;

	m_bIsSSL = false;

	kDebug(3, "connecting to {}:{}", m_Endpoint.Domain.Serialize(), m_Endpoint.Port.Serialize());

	m_Stream = CreateKTCPStream(m_Endpoint);

	if (!m_Stream->OutStream().good())
	{
		SetError(kFormat("failed to connect to {}:{}: {}", m_Endpoint.Domain.Serialize(), m_Endpoint.Port.Serialize(), GetStreamError()));
		kDebug(2, Error());
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
void KConnection::Disconnect()
//-----------------------------------------------------------------------------
{
	if (m_Stream)
	{
		kDebug(3, "disconnecting");
	}
	else
	{
		kDebug(1, "disconnecting an unconnected stream");
	}

	if (m_bStreamIsNotOwned)
	{
		m_bStreamIsNotOwned = false;
		m_Stream.release();
	}
	else
	{
		m_Stream.reset();
	}
}

//-----------------------------------------------------------------------------
bool KConnection::ExpiresFromNow(long iSeconds)
//-----------------------------------------------------------------------------
{
	auto TCP = GetTCPStream();
	if (TCP == nullptr)
	{
		return false;
	}

	TCP->expires_from_now(boost::posix_time::seconds(iSeconds));

	return true;

}

//-----------------------------------------------------------------------------
bool KConnection::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	auto SSL = GetSSLStream();
	if (SSL == nullptr)
	{
		return false;
	}

	// set connection timeout once, other than for the
	// non-SSL connections where we have to repeatedly
	// call ExpiresFromNow()
	SSL->Timeout(iSeconds);

	return true;

}

//-----------------------------------------------------------------------------
KTCPStream* KConnection::GetTCPStream()
//-----------------------------------------------------------------------------
{
	if (!IsSSL())
	{
		return static_cast<KTCPStream*>(m_Stream.get());
	}
	return nullptr;

} // GetTCPStream

//-----------------------------------------------------------------------------
KSSLStream* KConnection::GetSSLStream()
//-----------------------------------------------------------------------------
{
	if (IsSSL())
	{
		return static_cast<KSSLStream*>(m_Stream.get());
	}
	return nullptr;

} // GetSSLStream

//-----------------------------------------------------------------------------
KString KConnection::GetStreamError() const
//-----------------------------------------------------------------------------
{
	if (!m_Stream || IsSSL())
	{
		return KString{};
	}

	auto TCP = static_cast<const KTCPStream*>(m_Stream.get());;
	if (TCP == nullptr)
	{
		return KString{};
	}

	return TCP->error().message();

} // GetStreamError

//-----------------------------------------------------------------------------
void KConnection::setConnection(std::unique_ptr<KStream>&& Stream)
//-----------------------------------------------------------------------------
{
	if (m_bStreamIsNotOwned)
	{
		m_bStreamIsNotOwned = false;
		m_Stream.release();
	}
	m_Stream = std::move(Stream);

} // setConnection

//-----------------------------------------------------------------------------
bool KSSLConnection::Connect(const url::KDomain& domain, const url::KPort& port, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	m_bIsSSL = true;
	kDebug(3, "SSL: connecting to {}:{}", domain.Serialize(), port.Serialize());

	KTCPEndPoint EndPoint(domain, port);

	setConnection(CreateKSSLStream(EndPoint, bVerifyCerts));

	if (!Stream().OutStream().good())
	{
		SetError(kFormat("SSL:failed to connect to {}:{}", domain.Serialize(), port.Serialize()));
		kDebug(2, Error());
		return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
std::unique_ptr<KConnection> KConnection::Create(const KURL& URL, bool bForceSSL, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	KConnection Connection;

	url::KPort Port = URL.Port;

	if (Port.empty())
	{
		Port = KString::to_string(URL.Protocol.DefaultPort());
	}

	if (URL.Protocol == url::KProtocol::HTTPS || bForceSSL)
	{
		return std::make_unique<KSSLConnection>(URL.Domain, Port, bVerifyCerts);
	}
	else
	{
		return std::make_unique<KConnection>(URL.Domain, Port);
	}

} // Create

//-----------------------------------------------------------------------------
std::unique_ptr<KConnection> KConnection::Create(const KURL& URL, const KProxy& Proxy, bool bForceSSL, bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	KConnection Connection;

	url::KPort Port;
	url::KDomain Domain;

	if (Proxy.empty())
	{
		Port = URL.Port;
		Domain = URL.Domain;
	}
	else
	{
		Port = Proxy.Port;
		Domain = Proxy.Domain;
	}

	if (Port.empty())
	{
		Port = KString::to_string(URL.Protocol.DefaultPort());
	}

	if (URL.Protocol == url::KProtocol::HTTPS || bForceSSL)
	{
		return std::make_unique<KSSLConnection>(Domain, Port, bVerifyCerts);
	}
	else
	{
		return std::make_unique<KConnection>(Domain, Port);
	}

} // Create

} // end of namespace dekaf2
