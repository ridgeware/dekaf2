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
#include "kunixstream.h"
#include "ksslstream.h"
#include "ktcpstream.h"


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
bool KProxy::LoadFromEnv(KStringViewZ svEnvVar)
//-----------------------------------------------------------------------------
{
	Port.Parse(Domain.Parse(kGetEnv(svEnvVar)));
	return !empty();
}

// our fallback stream..
KStringStream KConnection::m_Empty;


//-----------------------------------------------------------------------------
KConnection::KConnection(KConnection&& Connection)
//-----------------------------------------------------------------------------
{
	operator=(std::move(Connection));
}

//-----------------------------------------------------------------------------
KConnection& KConnection::operator=(KConnection&& Connection)
//-----------------------------------------------------------------------------
{
	if (m_bStreamIsNotOwned)
	{
		m_Stream.release();
	}
	m_bStreamIsNotOwned = false;
	m_Stream = std::move(Connection.m_Stream);
	return *this;
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
		// TODO shall we call close() on the stream?
	}
	else
	{
		m_Stream.reset();
	}
}

//-----------------------------------------------------------------------------
bool KConnection::Good() const
//-----------------------------------------------------------------------------
{
	return m_Stream.get() != nullptr
	    && m_Stream->KOutStream::Good()
	    && m_Stream->KInStream::Good();
}

//-----------------------------------------------------------------------------
bool KConnection::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	return false;
}

//-----------------------------------------------------------------------------
bool KConnection::IsTLS() const
//-----------------------------------------------------------------------------
{
	return false;
}

//-----------------------------------------------------------------------------
bool KConnection::SetManualTLSHandshake(bool bYes)
//-----------------------------------------------------------------------------
{
	return false;
}

//-----------------------------------------------------------------------------
bool KConnection::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	return false;
}

//-----------------------------------------------------------------------------
KString KConnection::Error() const
//-----------------------------------------------------------------------------
{
	return KString{};

} // Error

//-----------------------------------------------------------------------------
bool KConnection::setConnection(std::unique_ptr<KStream>&& Stream, const KTCPEndPoint& EndPoint)
//-----------------------------------------------------------------------------
{
	if (m_bStreamIsNotOwned)
	{
		m_bStreamIsNotOwned = false;
		m_Stream.release();
	}
	m_Stream = std::move(Stream);
	m_Endpoint = EndPoint;

	if (Good())
	{
		kDebug(3, "connected to {}:{}", m_Endpoint.Domain.Serialize(), m_Endpoint.Port.Serialize());
		return true;
	}
	else
	{
		kDebug(1, "failed to connect to {}:{}: {}", m_Endpoint.Domain.Serialize(), m_Endpoint.Port.Serialize(), Error());
		return false;
	}

} // setConnection



//-----------------------------------------------------------------------------
bool KTCPConnection::Connect(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	return setConnection(CreateKTCPStream(Endpoint), Endpoint);

} // Connect

//-----------------------------------------------------------------------------
bool KTCPConnection::Good() const
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<const KTCPStream*>(StreamPtr());
	return stream != nullptr && stream->KTCPIOStream::Good();

} // Good

//-----------------------------------------------------------------------------
bool KTCPConnection::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<KTCPStream*>(StreamPtr());
	return stream != nullptr && stream->Timeout(iSeconds);

} // SetTimeout

//-----------------------------------------------------------------------------
KString KTCPConnection::Error() const
//-----------------------------------------------------------------------------
{
	KString sError;

	auto stream = static_cast<const KTCPStream*>(StreamPtr());
	if (stream != nullptr)
	{
		sError = stream->Error();
	}

	return sError;

} // Error

//-----------------------------------------------------------------------------
bool KUnixConnection::Connect(KStringViewZ sSocketFile)
//-----------------------------------------------------------------------------
{
	return setConnection(CreateKUnixStream(sSocketFile), sSocketFile);

} // Connect

//-----------------------------------------------------------------------------
bool KUnixConnection::Good() const
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<const KUnixStream*>(StreamPtr());
	return stream != nullptr && stream->KUnixIOStream::Good();

} // Good

//-----------------------------------------------------------------------------
bool KUnixConnection::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<KUnixStream*>(StreamPtr());
	return stream != nullptr && stream->Timeout(iSeconds);

} // SetTimeout

//-----------------------------------------------------------------------------
KString KUnixConnection::Error() const
//-----------------------------------------------------------------------------
{
	KString sError;

	auto stream = static_cast<const KUnixStream*>(StreamPtr());
	if (stream != nullptr)
	{
		sError = stream->Error();
	}

	return sError;

} // Error


//-----------------------------------------------------------------------------
bool KSSLConnection::Connect(const KTCPEndPoint& Endpoint, bool bVerifyCerts, bool bAllowSSLv2v3)
//-----------------------------------------------------------------------------
{
	return setConnection(CreateKSSLStream(Endpoint, bVerifyCerts, bAllowSSLv2v3), Endpoint);

} // Connect

//-----------------------------------------------------------------------------
bool KSSLConnection::Good() const
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<const KSSLStream*>(StreamPtr());
	return stream != nullptr && stream->KSSLIOStream::Good();

} // Good

//-----------------------------------------------------------------------------
bool KSSLConnection::IsTLS() const
//-----------------------------------------------------------------------------
{
	return true;

} // IsTLS

//-----------------------------------------------------------------------------
bool KSSLConnection::SetManualTLSHandshake(bool bYes)
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<KSSLStream*>(StreamPtr());
	return stream != nullptr && stream->SetManualTLSHandshake(bYes);

} // SetManualTLSHandshake

//-----------------------------------------------------------------------------
bool KSSLConnection::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<KSSLStream*>(StreamPtr());
	return stream != nullptr && stream->StartManualTLSHandshake();

} // StartManualTLSHandshake

//-----------------------------------------------------------------------------
bool KSSLConnection::SetTimeout(long iSeconds)
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<KSSLStream*>(StreamPtr());
	return stream != nullptr && stream->Timeout(iSeconds);

} // SetTimeout

//-----------------------------------------------------------------------------
KString KSSLConnection::Error() const
//-----------------------------------------------------------------------------
{
	KString sError;

	auto stream = static_cast<const KSSLStream*>(StreamPtr());
	if (stream != nullptr)
	{
		sError = stream->Error();
	}

	return sError;

} // Error


//-----------------------------------------------------------------------------
std::unique_ptr<KConnection> KConnection::Create(const KURL& URL, bool bForceSSL, bool bVerifyCerts, bool bAllowSSLv2v3)
//-----------------------------------------------------------------------------
{
	url::KPort Port = URL.Port;

	if (Port.empty())
	{
		Port = KString::to_string(URL.Protocol.DefaultPort());
	}

	if (Port == "443" || URL.Protocol == url::KProtocol::HTTPS || bForceSSL)
	{
		auto C = std::make_unique<KSSLConnection>();
		C->Connect(KTCPEndPoint(URL.Domain, Port), bVerifyCerts, bAllowSSLv2v3);
		return std::move(C);
	}
	else
	{
		auto C = std::make_unique<KTCPConnection>();
		C->Connect(KTCPEndPoint(URL.Domain, Port));
		return std::move(C);
	}

	return std::make_unique<KTCPConnection>();

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

	if (Port == "443" || URL.Protocol == url::KProtocol::HTTPS || bForceSSL)
	{
		auto C = std::make_unique<KSSLConnection>();
		C->Connect(KTCPEndPoint(URL.Domain, Port), bVerifyCerts, bAllowSSLv2v3);
		return std::move(C);
	}
	else
	{
		auto C = std::make_unique<KTCPConnection>();
		C->Connect(KTCPEndPoint(URL.Domain, Port));
		return std::move(C);
	}

	return std::make_unique<KTCPConnection>();

} // Create

} // end of namespace dekaf2
