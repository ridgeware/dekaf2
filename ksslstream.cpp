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

#include "ksslstream.h"
#include "klog.h"

namespace dekaf2 {

#if (BOOST_VERSION < 106600)
boost::asio::io_service KSSLContext::s_IO_Service;
#endif

//-----------------------------------------------------------------------------
std::string KSSLContext::PasswordCallback(std::size_t max_length,
						  boost::asio::ssl::context::password_purpose purpose) const
//-----------------------------------------------------------------------------
{
	return m_sPassword;
}

//-----------------------------------------------------------------------------
KSSLContext::KSSLContext(bool bIsServer, bool bVerifyCerts, bool bAllowSSLv3)
//-----------------------------------------------------------------------------
#if (BOOST_VERSION < 106600)
	: m_Context(s_IO_Service, boost::asio::ssl::context::sslv23)
#else
	: m_Context(bIsServer ? boost::asio::ssl::context::tls_server : boost::asio::ssl::context::tls_client)
#endif
	, m_Role(bIsServer ? boost::asio::ssl::stream_base::server : boost::asio::ssl::stream_base::client)
	, m_bVerify(bVerifyCerts)
 {
	boost::asio::ssl::context::options options = boost::asio::ssl::context::default_workarounds;

	options |= boost::asio::ssl::context::single_dh_use;

	if (bAllowSSLv3)
	{
		options |= (boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::sslv3);
	}
	else
	{
		options |= (boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3);
	}

	options |= boost::asio::ssl::context::no_tlsv1;
#if (BOOST_VERSION >= 106600)
	options |= boost::asio::ssl::context::no_tlsv1_1;
#endif

	boost::system::error_code ec;
	m_Context.set_options(options, ec);

	if (ec)
	{
		kDebug(1, "error setting SSL options {}: {}", options, ec.message());
	}

	if (bVerifyCerts)
	{
		m_Context.set_default_verify_paths();
	}

} // ctor

//-----------------------------------------------------------------------------
bool KSSLContext::LoadSSLCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
	m_sPassword.assign(sPassword.data(), sPassword.size());

	boost::system::error_code ec;

	m_Context.set_password_callback(std::bind(&KSSLContext::PasswordCallback, this, std::placeholders::_1, std::placeholders::_2), ec);

	if (ec)
	{
		kDebug(1, "cannot set password callback: {}", ec.message());
		return false;
	}

	m_Context.use_certificate_chain_file(sCert.c_str(), ec);

	if (ec)
	{
		kDebug(1, "cannot set certificate file {}: {}", sCert, ec.message());
		return false;
	}

	m_Context.use_private_key_file(sKey.c_str(), boost::asio::ssl::context::pem, ec);

	if (ec)
	{
		kDebug(1, "cannot set key file {}: {}", sKey, ec.message());
		return false;
	}

	return true;

} // LoadSSLCertificates

//-----------------------------------------------------------------------------
bool KSSLContext::SetSSLCertificates(KStringView sCert, KStringView sKey, KStringView sPassword)
//-----------------------------------------------------------------------------
{
#if (BOOST_VERSION < 105400)

	kDebug(1, "you need to link against at least boost 1.54 for buffered SSL certificates");
	return false;

#else

	m_sPassword.assign(sPassword.data(), sPassword.size());

	boost::system::error_code ec;

	m_Context.set_password_callback(std::bind(&KSSLContext::PasswordCallback, this, std::placeholders::_1, std::placeholders::_2), ec);

	if (ec)
	{
		kDebug(1, "cannot set password callback: {}", ec.message());
		return false;
	}

	m_Context.use_certificate_chain(boost::asio::const_buffer(sCert.data(), sCert.size()), ec);

	if (ec)
	{
		kDebug(1, "cannot set certificate: {}", ec.message());
		return false;
	}

	m_Context.use_private_key(boost::asio::const_buffer(sKey.data(), sKey.size()), boost::asio::ssl::context::pem, ec);

	if (ec)
	{
		kDebug(1, "cannot set key: {}", ec.message());
		return false;
	}

	return true;

#endif

} // SetSSLCertificates

static KSSLContext s_KSSLContextNoVerification { false, false, false };
static KSSLContext s_KSSLContextWithVerification { false, true, false };


//-----------------------------------------------------------------------------
bool KSSLIOStream::handshake(KAsioSSLStream<tcpstream>* stream)
//-----------------------------------------------------------------------------
{
	if (!stream->bNeedHandshake)
	{
		return true;
	}

	stream->bNeedHandshake = false;
	
	stream->Socket.async_handshake(stream->SSLContext.GetRole(),
								   [&](const boost::system::error_code& ec)
	{
		stream->ec = ec;
	});

	stream->RunTimed();

	if (stream->ec.value() != 0 || !stream->Socket.lowest_layer().is_open())
	{
		kDebug(2, "ssl handshake failed: {}", stream->ec.message());
		return false;
	}

	return true;

} // handshake

//-----------------------------------------------------------------------------
bool KSSLIOStream::StartManualTLSHandshake()
//-----------------------------------------------------------------------------
{
	// check if this stream was constructed with the manual handshake flag
	if (m_Stream.bManualHandshake)
	{
		m_Stream.bManualHandshake = false;
	}

	return handshake(&m_Stream);

} // StartManualTLSHandshake

//-----------------------------------------------------------------------------
bool KSSLIOStream::SetManualTLSHandshake(bool bYesno)
//-----------------------------------------------------------------------------
{
	if (m_Stream.bNeedHandshake)
	{
		m_Stream.bManualHandshake = bYesno;
		return true;
	}
	else
	{
		return false;
	}

} // SetManualTLSHandshake

//-----------------------------------------------------------------------------
std::streamsize KSSLIOStream::SSLStreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	std::streamsize iRead{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioSSLStream<tcpstream>*>(stream_);

		if (!stream->bManualHandshake)
		{
			if (!handshake(stream))
			{
				return -1;
			}
		}

		if (!stream->bManualHandshake)
		{
			stream->Socket.async_read_some(boost::asio::buffer(sBuffer, iCount),
			[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
			{
				stream->ec = ec;
				iRead = bytes_transferred;
			});
		}
		else
		{
			stream->Socket.next_layer().async_read_some(boost::asio::buffer(sBuffer, iCount),
			[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
			{
				stream->ec = ec;
				iRead = bytes_transferred;
			});
		}

		stream->RunTimed();

		if (iRead == 0 || stream->ec.value() != 0 || !stream->Socket.lowest_layer().is_open())
		{
			kDebug(3, "cannot read from tls stream: {}", stream->ec.message());
		}
	}

	return iRead;

} // SSLStreamReader

//-----------------------------------------------------------------------------
std::streamsize KSSLIOStream::SSLStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we need to loop the writer, as write_some() has an upper limit (the buffer size) to which
	// it can accept blocks - therefore we repeat the write until we have sent all bytes or
	// an error condition occurs

	std::streamsize iWrote{0};

	if (stream_)
	{
		auto stream = static_cast<KAsioSSLStream<tcpstream>*>(stream_);

		if (!stream->bManualHandshake)
		{
			if (!handshake(stream))
			{
				return -1;
			}
		}

		for (;iWrote < iCount;)
		{
			std::size_t iWrotePart{0};

			if (!stream->bManualHandshake)
			{
				stream->Socket.async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
				[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
				{
					stream->ec = ec;
					iWrotePart = bytes_transferred;
				});
			}
			else
			{
				stream->Socket.next_layer().async_write_some(boost::asio::buffer(static_cast<const char*>(sBuffer) + iWrote, iCount - iWrote),
				[&](const boost::system::error_code& ec, std::size_t bytes_transferred)
				{
					stream->ec = ec;
					iWrotePart = bytes_transferred;
				});
			}

			stream->RunTimed();

			iWrote += iWrotePart;

			if (iWrotePart == 0 || stream->ec.value() != 0 || !stream->Socket.lowest_layer().is_open())
			{
				kDebug(3, "cannot write to tls stream: {}", stream->ec.message());
				break;
			}
		}
	}

	return iWrote;

} // SSLStreamWriter

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream()
//-----------------------------------------------------------------------------
    : base_type(&m_SSLStreamBuf)
	, m_Stream(s_KSSLContextNoVerification, DEFAULT_TIMEOUT, false)
{
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(KSSLContext& Context)
//-----------------------------------------------------------------------------
	: base_type(&m_SSLStreamBuf)
	, m_Stream(Context, DEFAULT_TIMEOUT, false)
{
}

//-----------------------------------------------------------------------------
KSSLIOStream::KSSLIOStream(const KTCPEndPoint& Endpoint, int iSecondsTimeout, bool bManualHandshake)
//-----------------------------------------------------------------------------
    : base_type(&m_SSLStreamBuf)
	, m_Stream(s_KSSLContextNoVerification, iSecondsTimeout, bManualHandshake)
{
	Connect(Endpoint);
}

//-----------------------------------------------------------------------------
KSSLIOStream::~KSSLIOStream()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Timeout(int iSeconds)
//-----------------------------------------------------------------------------
{
	m_Stream.iSecondsTimeout = iSeconds;
	return true;
}

//-----------------------------------------------------------------------------
bool KSSLIOStream::Connect(const KTCPEndPoint& Endpoint)
//-----------------------------------------------------------------------------
{
	m_Stream.bNeedHandshake = true;

	boost::asio::ip::tcp::resolver Resolver(m_Stream.IOService);
	boost::asio::ip::tcp::resolver::query query(Endpoint.Domain.get().c_str(), Endpoint.Port.get().c_str());
	auto hosts = Resolver.resolve(query, m_Stream.ec);

	if (Good())
	{
		if (m_Stream.SSLContext.GetVerify())
		{
			m_Stream.Socket.set_verify_mode(boost::asio::ssl::verify_peer
										  | boost::asio::ssl::verify_fail_if_no_peer_cert);
		}
		else
		{
			m_Stream.Socket.set_verify_mode(boost::asio::ssl::verify_none);
		}

		boost::asio::async_connect(m_Stream.Socket.lowest_layer(),
								   hosts,
								   [&](const boost::system::error_code& ec,
#if (BOOST_VERSION < 106600)
		                                                           boost::asio::ip::tcp::resolver::iterator endpoint)
#else
									   const boost::asio::ip::tcp::endpoint& endpoint)
#endif
		{
			m_ConnectedHost = endpoint;
			m_Stream.ec = ec;
		});

		m_Stream.RunTimed();
	}

	if (!Good() || !m_Stream.Socket.lowest_layer().is_open())
	{
		kDebug(2, "{}", Error());
		return false;
	}

	return true;

} // connect


//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLServer(KSSLContext& Context)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLStream>(Context);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLClient> CreateKSSLClient(bool bVerifyCerts)
//-----------------------------------------------------------------------------
{
	return std::make_unique<KSSLClient>(bVerifyCerts ? s_KSSLContextWithVerification : s_KSSLContextNoVerification);
}

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLClient> CreateKSSLClient(const KTCPEndPoint& EndPoint, bool bVerifyCerts, bool bManualHandshake)
//-----------------------------------------------------------------------------
{
	auto Client = CreateKSSLClient(bVerifyCerts);

	if (bManualHandshake)
	{
		Client->SetManualTLSHandshake(true);
	}

	Client->Connect(EndPoint);

	return Client;

} // CreateKSSLClient

} // end of namespace dekaf2

