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

/// @file ksslstream.h
/// provides an implementation of std::iostreams supporting SSL/TLS

#include "bits/kasio.h"
#include "kstring.h"
#include "kstream.h" // TODO remove
#include "kstreambuf.h"
#include "kurl.h"

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Holds a configured ssl context - will be used in the constructor of KSSLIOStream
class DEKAF2_PUBLIC KSSLContext
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs an SSL context
	KSSLContext(bool bIsServer = false, bool bVerifyCerts = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this stream object as a server, set its SSL certificate files here (PEM format)
	/// The cert file may contain an appended certificate chain as well, and the key if sKey is empty.
	/// It is however important to have the cert first in the file.
	bool LoadSSLCertificates(KStringViewZ sCert, KStringViewZ sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this stream object as a server, set its SSL certificate buffers here (strings in PEM format)
	/// The cert string buffer may contain an appended certificate chain as well, and the key if sKey is empty.
	/// It is however important to have the cert first in the buffer.
	bool SetSSLCertificates(KStringView sCert, KStringView sKey, KStringView sPassword = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this stream object as a server, set the pre-computed DH key exchange primes here (string in PEM format)
	bool SetDHPrimes(KStringView sDHPrimes = KStringView{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the allowed Cipher suites, separated by colons (check the OpenSSL documentation for names)
	bool SetAllowedCipherSuites(KStringView sCipherSuites);
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
	bool GetVerify() const
	//-----------------------------------------------------------------------------
	{
		return m_bVerify;
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
	bool SetError(KString sError);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	std::string PasswordCallback(std::size_t max_length, boost::asio::ssl::context::password_purpose purpose) const;
	//-----------------------------------------------------------------------------

	KString m_sError;
#if (BOOST_VERSION < 106600)
	static boost::asio::io_service s_IO_Service;
#endif
	boost::asio::ssl::context m_Context;
	boost::asio::ssl::stream_base::handshake_type m_Role;
	std::string m_sPassword;
	bool m_bVerify;

}; // KSSLContext


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<typename StreamType>
struct KAsioSSLStream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	//-----------------------------------------------------------------------------
	KAsioSSLStream(KSSLContext& Context, int _iSecondsTimeout = 15, bool _bManualHandshake = false)
	//-----------------------------------------------------------------------------
	: SSLContext       { Context }
	, IOService        { 1 }
	, Socket           { IOService, Context.GetContext() }
	, Timer            { IOService }
	, iSecondsTimeout  { _iSecondsTimeout }
	, bManualHandshake { _bManualHandshake }
	{
		ClearTimer();
		CheckTimer();

	} // ctor

	//-----------------------------------------------------------------------------
	~KAsioSSLStream()
	//-----------------------------------------------------------------------------
	{
		Disconnect();

	} // dtor

	//-----------------------------------------------------------------------------
	/// disconnect the stream
	bool Disconnect()
	//-----------------------------------------------------------------------------
	{
		if (Socket.next_layer().is_open())
		{
			boost::system::error_code ec;

			Socket.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

			if (ec)
			{
				// do not display the shutdown error message when the socket has
				// already been disconnected
				if (ec.value() != boost::asio::error::not_connected)
				{
					kDebug(2, "error shutting down socket: {}", ec.message());
				}
				return false;
			}

			Socket.next_layer().close(ec);

			if (ec)
			{
				kDebug(2, "error closing socket: {}", ec.message());
				return false;
			}

			kDebug(2, "disconnected from: {}", sEndpoint);
		}

		return true;

	} // Disconnect

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	bool IsDisconnected()
	//-----------------------------------------------------------------------------
	{
		uint16_t buffer;

		Socket.next_layer().receive(boost::asio::buffer(&buffer, 1), Socket.next_layer().message_peek, ec);

		if (ec == boost::asio::error::would_block)
		{
			// open but no data
			ec.clear();
			return false;
		}
		else if (!ec)
		{
			// open and data
			return false;
		}

		// ec == boost::asio::error::eof would signal a closed socket,
		// but we treat all other errors as disconnected as well
		return true;

	} // IsDisconnected

	//-----------------------------------------------------------------------------
	void ResetTimer()
	//-----------------------------------------------------------------------------
	{
		Timer.expires_from_now(boost::posix_time::seconds(iSecondsTimeout));
	}

	//-----------------------------------------------------------------------------
	void ClearTimer()
	//-----------------------------------------------------------------------------
	{
		Timer.expires_at(boost::posix_time::pos_infin);
	}

	//-----------------------------------------------------------------------------
	void CheckTimer()
	//-----------------------------------------------------------------------------
	{
		if (Timer.expires_at() <= boost::asio::deadline_timer::traits_type::now())
		{
			boost::system::error_code ignored_ec;
			Socket.next_layer().close(ignored_ec);
			Timer.expires_at(boost::posix_time::pos_infin);
			kDebug (1, "Connection timeout ({} seconds): {}", iSecondsTimeout, sEndpoint);
		}

		Timer.async_wait(boost::bind(&KAsioSSLStream<StreamType>::CheckTimer, this));

	} // CheckTimer

	//-----------------------------------------------------------------------------
	void RunTimed()
	//-----------------------------------------------------------------------------
	{
		ResetTimer();

		ec = boost::asio::error::would_block;
		do
		{
			IOService.run_one();
		}
		while (ec == boost::asio::error::would_block);

		ClearTimer();

	} // RunTimed

	KSSLContext& SSLContext;
	boost::asio::io_service IOService;
	StreamType Socket;
	dekaf2::KString sEndpoint;
	boost::asio::deadline_timer Timer;
	boost::system::error_code ec;
	int iSecondsTimeout;
	bool bNeedHandshake { true };
	bool bManualHandshake { false };

}; // KAsioSSLStream


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream SSL/TLS implementation with timeout.
class DEKAF2_PUBLIC KSSLIOStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::iostream;

//----------
public:
//----------

	using asiostream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

	enum { DEFAULT_TIMEOUT = 1 * 15 };

	//-----------------------------------------------------------------------------
	/// Constructs a client with default parameters (no certificate verification)
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	/// @param bManualHandshake
	/// if true the SSL handshake is only started on manual request. This allows to
	/// start a communication without SSL, and then switch to SSL at a later time.
	KSSLIOStream(int iSecondsTimeout = DEFAULT_TIMEOUT,
				 bool bManualHandshake = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected stream - KSSLContext holds most properties
	/// @param Context
	/// A KSSLContext which defines role (server/client) and cert verification
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	/// @param bManualHandshake
	/// if true the SSL handshake is only started on manual request. This allows to
	/// start a communication without SSL, and then switch to SSL at a later time.
	KSSLIOStream(KSSLContext& Context,
				 int iSecondsTimeout = DEFAULT_TIMEOUT,
				 bool bManualHandshake = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected stream as a client and with default parameters (no
	/// certificate verification)
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 15.
	/// @param bManualHandshake
	/// if true the SSL handshake is only started on manual request. This allows to
	/// start a communication without SSL, and then switch to SSL at a later time.
	KSSLIOStream(const KTCPEndPoint& Endpoint,
				 int iSecondsTimeout = DEFAULT_TIMEOUT,
				 bool bManualHandshake = false);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructs and closes a stream
	~KSSLIOStream() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set I/O timeout in seconds.
	bool Timeout(int iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	bool Connect(const KTCPEndPoint& Endpoint);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// std::iostream interface to open a stream. Delegates to connect()
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	bool open(const KTCPEndPoint& Endpoint)
	//-----------------------------------------------------------------------------
	{
		return Connect(Endpoint);
	}

	//-----------------------------------------------------------------------------
	/// Disconnect the stream
	bool Disconnect()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Disconnect();
	}

	//-----------------------------------------------------------------------------
	/// Disconnect the stream
	bool close()
	//-----------------------------------------------------------------------------
	{
		return Disconnect();
	}

	//-----------------------------------------------------------------------------
	bool is_open() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.next_layer().is_open();
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	bool IsDisconnected()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.IsDisconnected();
	}

	//-----------------------------------------------------------------------------
	/// Upgrade connection from TCP to TCP over TLS. Returns true on success. Can also
	/// be used to force a handshake before any IO is triggered.
	bool StartManualTLSHandshake();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Switch to manual handshake, only possible before any data has been read or
	/// written
	bool SetManualTLSHandshake(bool bYesno = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the underlying TCP socket of the stream
	/// @return
	/// The TCP socket of the stream (wrapped into ASIO's basic_socket<> template)
#if (BOOST_VERSION < 106600)
	boost::asio::basic_socket<boost::asio::ip::tcp, boost::asio::stream_socket_service<boost::asio::ip::tcp> >& GetTCPSocket()
#else
	boost::asio::basic_socket<boost::asio::ip::tcp>& GetTCPSocket()
#endif
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket.lowest_layer();
	}

	//-----------------------------------------------------------------------------
	/// Gets the ASIO socket of the stream, e.g. to move it to another place ..
	asiostream& GetAsioSocket()
	//-----------------------------------------------------------------------------
	{
		return m_Stream.Socket;
	}

	//-----------------------------------------------------------------------------
	bool Good() const
	//-----------------------------------------------------------------------------
	{
		return m_Stream.ec.value() == 0;
	}

	//-----------------------------------------------------------------------------
	KString Error() const
	//-----------------------------------------------------------------------------
	{
		KString sError;

		if (!Good())
		{
			sError = m_Stream.ec.message();
		}

		return sError;
	}

//----------
private:
//----------

	KAsioSSLStream<asiostream> m_Stream;

	KBufferedStreamBuf m_SSLStreamBuf{&SSLStreamReader, &SSLStreamWriter, &m_Stream, &m_Stream};

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	DEKAF2_PRIVATE
	static std::streamsize SSLStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	DEKAF2_PRIVATE
	static std::streamsize SSLStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	static bool handshake(KAsioSSLStream<asiostream>* stream);
	//-----------------------------------------------------------------------------

};


/// SSL stream based on std::iostream and asio::ssl
using KSSLStream = KReaderWriter<KSSLIOStream>;

// there is nothing special with a tcp ssl client
using KSSLClient = KSSLStream;

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KSSLStream> CreateKSSLServer(KSSLContext& Context);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KSSLClient> CreateKSSLClient(bool bVerifyCerts = false, int iSecondsTimeout = KSSLIOStream::DEFAULT_TIMEOUT);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KSSLClient> CreateKSSLClient(const KTCPEndPoint& EndPoint, bool bVerifyCerts = false, bool bManualHandshake = false, int iSecondsTimeout = KSSLIOStream::DEFAULT_TIMEOUT);
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END


