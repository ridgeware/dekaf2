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
bool KConnection::Connect(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	m_sError.clear();

	if (m_bStreamIsNotOwned)
	{
		m_bStreamIsNotOwned = false;
		m_Stream.release();
	}

	m_Endpoint = Endpoint;

	m_bIsSSL = false;

	kDebug(3, "connecting to {}:{}", m_Endpoint.Domain.Serialize(), m_Endpoint.Port.Serialize());

	m_Stream = CreateKTCPStream(m_Endpoint);

	if (!m_Stream->OutStream().good())
	{
		kDebug(1, "failed to connect to {}:{}: {}", m_Endpoint.Domain.Serialize(), m_Endpoint.Port.Serialize(), Error());
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
bool KConnection::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	if (IsSSL())
	{
		auto SSL = GetSSLStream();
		if (SSL == nullptr)
		{
			return false;
		}

		SSL->Timeout(iSeconds);
	}
	else
	{
		auto TCP = GetTCPStream();
		if (TCP == nullptr)
		{
			return false;
		}

		TCP->Timeout(iSeconds);
	}

	return true;

}

//-----------------------------------------------------------------------------
const KTCPStream* KConnection::GetTCPStream() const
//-----------------------------------------------------------------------------
{
	if (!IsSSL())
	{
		return static_cast<const KTCPStream*>(m_Stream.get());
	}
	return nullptr;

} // GetTCPStream

//-----------------------------------------------------------------------------
const KSSLStream* KConnection::GetSSLStream() const
//-----------------------------------------------------------------------------
{
	if (IsSSL())
	{
		return static_cast<const KSSLStream*>(m_Stream.get());
	}
	return nullptr;

} // GetSSLStream

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
KString KConnection::Error() const
//-----------------------------------------------------------------------------
{
	if (m_Stream)
	{
		if (IsSSL())
		{
			auto SSL = GetSSLStream();
			if (SSL)
			{
				return SSL->Error();
			}
		}
		else
		{
			auto TCP = GetTCPStream();
			if (TCP)
			{
				return TCP->Error();
			}
		}
	}

	return KString{};

} // Error

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
bool KSSLConnection::Connect(const KTCPEndPoint& Endpoint, bool bVerifyCerts, bool bAllowSSLv2v3)
//-----------------------------------------------------------------------------
{
	m_bIsSSL = true;
	m_Endpoint = Endpoint;

	kDebug(3, "SSL: connecting to {}:{}", Endpoint.Domain.Serialize(), Endpoint.Port.Serialize());

	setConnection(CreateKSSLStream(Endpoint, bVerifyCerts, bAllowSSLv2v3));

	if (!Stream().OutStream().good())
	{
		SetError(kFormat("SSL:failed to connect to {}:{}", Endpoint.Domain.Serialize(), Endpoint.Port.Serialize()));
		kDebug(1, Error());
		return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
std::unique_ptr<KConnection> KConnection::Create(const KURL& URL, bool bForceSSL, bool bVerifyCerts, bool bAllowSSLv2v3)
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
		return std::make_unique<KSSLConnection>(KTCPEndPoint(URL.Domain, Port), bVerifyCerts, bAllowSSLv2v3);
	}
	else
	{
		return std::make_unique<KConnection>(KTCPEndPoint(URL.Domain, Port));
	}

} // Create

//-----------------------------------------------------------------------------
std::unique_ptr<KConnection> KConnection::Create(const KURL& URL, const KProxy& Proxy, bool bForceSSL, bool bVerifyCerts, bool bAllowSSLv2v3)
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
		return std::make_unique<KSSLConnection>(KTCPEndPoint(Domain, Port), bVerifyCerts, bAllowSSLv2v3);
	}
	else
	{
		return std::make_unique<KConnection>(KTCPEndPoint(Domain, Port));
	}

} // Create

} // end of namespace dekaf2
