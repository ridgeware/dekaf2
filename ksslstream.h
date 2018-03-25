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

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "kstring.h"
#include "kstream.h" // TODO remove
#include "kstreambuf.h"

namespace dekaf2
{


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSSLIOStream : public std::iostream
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = std::iostream;

	enum { DEFAULT_TIMEOUT = 1 * 30 };

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected stream
	KSSLIOStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected stream as a client.
	/// @param sServer
	/// Server name or IP address in v4 or v6 notation to connect to, as a string
	/// @param sPort
	/// Port to connect to, as a string
	/// @param bVerifyCerts
	/// If true server certificate will verified
	/// @param bAllowSSLv2v3
	/// If true also connections with SSL versions 2 and 3 will be allowed.
	/// Default is false, only TLS connections will be allowed.
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 60.
	KSSLIOStream(const char* sServer,
				 const char* sPort,
				 bool bVerifyCerts,
				 bool bAllowSSLv2v3,
				 int iSecondsTimeout = DEFAULT_TIMEOUT);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected stream as a client.
	/// @param sServer
	/// Server name or IP address in v4 or v6 notation to connect to, as a string
	/// @param sPort
	/// Port to connect to, as a string
	/// @param bVerifyCerts
	/// If true server certificate will verified
	/// @param bAllowSSLv2v3
	/// If true also connections with SSL versions 2 and 3 will be allowed.
	/// Default is false, only TLS connections will be allowed.
	/// @param iSecondsTimeout
	/// Timeout in seconds for any I/O. Defaults to 60.
	KSSLIOStream(const KString& sServer,
				 const KString& sPort,
				 bool bVerifyCerts,
				 bool bAllowSSLv2v3,
				 int iSecondsTimeout = DEFAULT_TIMEOUT);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Destructs and closes a stream
	~KSSLIOStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this stream object as a server, set it's SSL certificate here
	void SetSSLCertificate(const char* sCert, const char* sPem);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// When using this stream object as a server, set it's SSL certificate here
	inline void SetSSLCertificate(const KString& sCert, const KString& sPem)
	//-----------------------------------------------------------------------------
	{
		SetSSLCertificate(sCert.c_str(), sPem.c_str());
	}

	//-----------------------------------------------------------------------------
	/// Set I/O timeout in seconds.
	bool Timeout(int iSeconds);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param sServer
	/// Server name or IP address in v4 or v6 notation to connect to, as a string
	/// @param sPort
	/// Port to connect to, as a string
	/// @param bVerifyCerts
	/// If true server certificate will verified
	/// @param bAllowSSLv2v3
	/// If true also connections with SSL versions 2 and 3 will be allowed.
	/// Default is false, only TLS connections will be allowed.
	bool connect(const char* sServer, const char* sPort, bool bVerifyCerts, bool bAllowSSLv2v3);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param sServer
	/// Server name or IP address in v4 or v6 notation to connect to, as a string
	/// @param sPort
	/// Port to connect to, as a string
	/// @param bVerifyCerts
	/// If true server certificate will verified
	/// @param bAllowSSLv2v3
	/// If true also connections with SSL versions 2 and 3 will be allowed.
	/// Default is false, only TLS connections will be allowed.
	inline bool connect(const KString& sServer, const KString& sPort, bool bVerifyCerts, bool bAllowSSLv2v3)
	//-----------------------------------------------------------------------------
	{
		return connect(sServer.c_str(), sPort.c_str(), bVerifyCerts, bAllowSSLv2v3);
	}

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

//----------
private:
//----------

	boost::asio::io_service m_IO_Service;
	boost::asio::ssl::context m_Context;
	using tcpstream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

	struct Stream_t
	{
		Stream_t(boost::asio::io_service& ioservice, boost::asio::ssl::context& context)
		: Socket(ioservice, context)
		{}

		tcpstream Socket;
		bool bNeedHandshake  { true };
		int iTimeoutMilliseconds { 30 * 1000 };
	};

	Stream_t m_Stream;

#if (BOOST_VERSION < 106600)
	boost::asio::ip::tcp::resolver::iterator m_ConnectedHost;
#else
	boost::asio::ip::tcp::endpoint m_ConnectedHost;
#endif

	// see comment in KOutputFDStream about the legality
	// to only construct the KStreamBuf here, but to use it in
	// the constructor before
	KStreamBuf m_SSLStreamBuf{&SSLStreamReader, &SSLStreamWriter, &m_Stream, &m_Stream};

	enum POLLSTATE
	{
		POLL_FAILURE = 0,
		POLL_SUCCESS = 1,
		POLL_LAST    = 2
	};

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	static std::streamsize SSLStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	static std::streamsize SSLStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static void handshake(boost::asio::ssl::stream_base::handshake_type role, Stream_t* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static POLLSTATE timeout(bool bForReading, Stream_t* stream);
	//-----------------------------------------------------------------------------

};


/// SSL stream based on std::iostream and asio::ssl
using KSSLStream = KReaderWriter<KSSLIOStream>;

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream();
//-----------------------------------------------------------------------------

// fwd declaration
class KTCPEndPoint;

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream(const KTCPEndPoint& EndPoint, bool bVerifyCerts, bool bAllowSSLv2v3);
//-----------------------------------------------------------------------------

} // end of namespace dekaf2


