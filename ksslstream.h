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
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include "kstring.h"
#include "kstream.h"

namespace dekaf2
{

namespace KSSL_detail
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// IOStream device that speaks SSL but can also speak non-SSL. Equivalent to
/// a std::streambuf, but not derived.
class KSSLInOutStreamDevice : public boost::iostreams::device<boost::iostreams::bidirectional>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base_type = boost::iostreams::device<boost::iostreams::bidirectional>;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KSSLInOutStreamDevice(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& Stream, bool bUseSSL, const int& iTimeoutMilliseconds) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void handshake(boost::asio::ssl::stream_base::handshake_type role);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::streamsize read(char* s, std::streamsize n) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::streamsize write(const char* s, std::streamsize n) noexcept;
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	enum POLLSTATE
	{
		POLL_FAILURE = 0,
		POLL_SUCCESS = 1,
		POLL_LAST    = 2
	};

	//-----------------------------------------------------------------------------
	POLLSTATE timeout(bool bForReading);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	size_t read_with_timeout(char* s, size_t n) noexcept;
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& m_Stream;
	const int& m_iTimeoutMilliseconds;
	bool m_bUseSSL;
	bool m_bNeedHandshake;
	bool m_bFailed { false };

};  // KSSLIOStreamDevice


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream implementation with SSL/TLS encryption and timeout.
class KSSLIOStream : public boost::iostreams::stream<KSSLInOutStreamDevice>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = boost::iostreams::stream<KSSLInOutStreamDevice>;

	enum { DEFAULT_TIMEOUT = 1 * 30 };

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Construcs an unconnected stream
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
		return m_Socket.lowest_layer();
	}

//----------
private:
//----------

	boost::asio::io_service m_IO_Service;
	boost::asio::ssl::context m_Context;
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_Socket;
#if (BOOST_VERSION < 106600)
	boost::asio::ip::tcp::resolver::iterator m_ConnectedHost;
#else
	boost::asio::ip::tcp::endpoint m_ConnectedHost;
#endif
	int m_iTimeoutMilliseconds;

};

} // end of namespace KSSL_detail

/// SSL stream based on boost::iostreams and asio::ssl
using KSSLStream = KReaderWriter<KSSL_detail::KSSLIOStream>;

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream();
//-----------------------------------------------------------------------------

// fwd declaration
class KTCPEndPoint;

//-----------------------------------------------------------------------------
std::unique_ptr<KSSLStream> CreateKSSLStream(const KTCPEndPoint& EndPoint, bool bVerifyCerts, bool bAllowSSLv2v3);
//-----------------------------------------------------------------------------


}

