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

#include "kconnection.h"
#include "ksystem.h"
#ifdef DEKAF2_HAS_UNIX_SOCKETS
#include "kunixstream.h"
#endif
#include "ksslstream.h"
#include "ktcpstream.h"


DEKAF2_NAMESPACE_BEGIN

namespace {

TLSOptions g_DefaultTLSOptions =
#if DEKAF2_HAS_NGHTTP2
	RequestHTTP2 | FallBackToHTTP1;
#else
	None;
#endif

} // end of anonymous namespace

//-----------------------------------------------------------------------------
TLSOptions kGetTLSDefaults(TLSOptions Options)
//-----------------------------------------------------------------------------
{
	if (Options & DefaultsForHTTP)
	{
		Options &= ~DefaultsForHTTP;
		Options |= g_DefaultTLSOptions;
	}

	return Options;

} // kGetTLSDefaults

//-----------------------------------------------------------------------------
bool kSetTLSDefaults(TLSOptions Options)
//-----------------------------------------------------------------------------
{
	g_DefaultTLSOptions = kGetTLSDefaults(Options);
	return true;

} // kSetTLSDefaults

//-----------------------------------------------------------------------------
KConnection::KConnection(KConnection&& other) noexcept
//-----------------------------------------------------------------------------
{
	operator=(std::move(other));
}

//-----------------------------------------------------------------------------
KConnection& KConnection::operator=(KConnection&& other) noexcept
//-----------------------------------------------------------------------------
{
	if (m_bStreamIsNotOwned)
	{
		m_Stream.release();
	}
	m_bStreamIsNotOwned = other.m_bStreamIsNotOwned;
	m_Stream = std::move(other.m_Stream);
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
	else
	{
		m_bStreamIsNotOwned = true;
	}
	m_Stream.reset(&Stream);
	return *this;
}

//-----------------------------------------------------------------------------
KConnection& KConnection::operator=(KStream&& Stream)
//-----------------------------------------------------------------------------
{
	if (m_bStreamIsNotOwned)
	{
		m_Stream.release();
		m_bStreamIsNotOwned = false;
	}
	m_Stream.reset(&Stream);
	return *this;
}

//-----------------------------------------------------------------------------
void KConnection::Disconnect()
//-----------------------------------------------------------------------------
{
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
bool KConnection::Good() const
//-----------------------------------------------------------------------------
{
	return m_Stream != nullptr
	    && m_Stream->KOutStream::Good()
	    && m_Stream->KInStream::Good();
}

//-----------------------------------------------------------------------------
bool KConnection::SetTimeout(int iSeconds)
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
KSSLIOStream* KConnection::GetUnderlyingTLSStream()
//-----------------------------------------------------------------------------
{
	return nullptr;
}

//-----------------------------------------------------------------------------
KString KConnection::Error() const
//-----------------------------------------------------------------------------
{
	return KString{};

} // Error

//-----------------------------------------------------------------------------
bool KConnection::setConnection(std::unique_ptr<KStream>&& Stream, KTCPEndPoint EndPoint)
//-----------------------------------------------------------------------------
{
	if (m_bStreamIsNotOwned)
	{
		m_Stream.release();
		m_bStreamIsNotOwned = false;
	}
	m_Stream = std::move(Stream);
	m_Endpoint = std::move(EndPoint);

	if (!Good())
	{
		kDebug(1, "failed to connect to {}: {}", m_Endpoint.Serialize(), Error());
		return false;
	}

	return true;

} // setConnection

//-----------------------------------------------------------------------------
bool KTCPConnection::Connect(const KTCPEndPoint& Endpoint, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return setConnection(CreateKTCPStream(Endpoint, iSecondsTimeout), Endpoint);

} // Connect

//-----------------------------------------------------------------------------
bool KTCPConnection::Good() const
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<const KTCPStream*>(StreamPtr());
	return stream != nullptr && stream->KTCPIOStream::Good();

} // Good

//-----------------------------------------------------------------------------
bool KTCPConnection::SetTimeout(int iSeconds)
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

#ifdef DEKAF2_HAS_UNIX_SOCKETS

//-----------------------------------------------------------------------------
bool KUnixConnection::Connect(KStringViewZ sSocketFile, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return setConnection(CreateKUnixStream(sSocketFile, iSecondsTimeout), sSocketFile);

} // Connect

//-----------------------------------------------------------------------------
bool KUnixConnection::Good() const
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<const KUnixStream*>(StreamPtr());
	return stream != nullptr && stream->KUnixIOStream::Good();

} // Good

//-----------------------------------------------------------------------------
bool KUnixConnection::SetTimeout(int iSeconds)
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

#endif // DEKAF2_HAS_UNIX_SOCKETS


//-----------------------------------------------------------------------------
bool KSSLConnection::Connect(const KTCPEndPoint& Endpoint, TLSOptions Options, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
	return setConnection(CreateKSSLClient(Endpoint, Options, iSecondsTimeout), Endpoint);

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
bool KSSLConnection::SetTimeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<KSSLStream*>(StreamPtr());
	return stream != nullptr && stream->Timeout(iSeconds);

} // SetTimeout

//-----------------------------------------------------------------------------
KSSLIOStream* KSSLConnection::GetUnderlyingTLSStream()
//-----------------------------------------------------------------------------
{
	auto stream = static_cast<KSSLStream*>(StreamPtr());
	return stream != nullptr ? &stream->GetKSSLIOStream() : nullptr;
}

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
std::unique_ptr<KConnection> KConnection::Create(const KURL& URL, bool bForceSSL, TLSOptions Options, int iSecondsTimeout)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_HAS_UNIX_SOCKETS
	if (URL.Protocol == url::KProtocol::UNIX)
	{
		auto C = std::make_unique<KUnixConnection>();
		C->Connect(URL.Path.get(), iSecondsTimeout);
		return C;
	}
#endif

	url::KPort Port = URL.Port;

	if (Port.empty())
	{
		Port = KString::to_string(URL.Protocol.DefaultPort());
	}

	if ((url::KProtocol::UNDEFINED && Port.get() == 443) || URL.Protocol == url::KProtocol::HTTPS || bForceSSL)
	{
		auto C = std::make_unique<KSSLConnection>();
		C->Connect(KTCPEndPoint(URL.Domain, Port), Options, iSecondsTimeout);
		return C;
	}
	else // NOLINT: we want the else after return..
	{
		auto C = std::make_unique<KTCPConnection>();
		C->Connect(KTCPEndPoint(URL.Domain, Port), iSecondsTimeout);
		return C;
	}

} // Create

DEKAF2_NAMESPACE_END
