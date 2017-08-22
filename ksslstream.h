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

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include "kstring.h"
#include "kstream.h"

using namespace boost::asio;

namespace dekaf2
{

namespace KSSL_detail
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// IOStream device that speaks SSL but can also speak non-SSL
class KSSLInOutStreamDevice : public boost::iostreams::device<boost::iostreams::bidirectional>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	using base_type = boost::iostreams::device<boost::iostreams::bidirectional>;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KSSLInOutStreamDevice(ssl::stream<ip::tcp::socket>& Stream, bool bUseSSL = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void handshake(ssl::stream_base::handshake_type role);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::streamsize read(char* s, std::streamsize n);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	std::streamsize write(const char* s, std::streamsize n);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	ssl::stream<ip::tcp::socket>& m_Stream;
	bool m_bUseSSL;
	bool m_bNeedHandshake;

};  // KSSLIOStreamDevice


//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KSSLIOStream : public boost::iostreams::stream<KSSLInOutStreamDevice>
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = boost::iostreams::stream<KSSLInOutStreamDevice>;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KSSLIOStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KSSLIOStream(const char* sServer, const char* sPort, bool bVerifyCerts);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KSSLIOStream(const KString& sServer, const KString& sPort, bool bVerifyCerts);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	~KSSLIOStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void SetSSLCertificate(const char* sCert, const char* sPem);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline void SetSSLCertificate(const KString& sCert, const KString sPem)
	//-----------------------------------------------------------------------------
	{
		SetSSLCertificate(sCert.c_str(), sPem.c_str());
	}

	//-----------------------------------------------------------------------------
	bool connect(const char* sServer, const char* sPort, bool bVerifyCerts);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	inline bool connect(const KString& sServer, const KString& sPort, bool bVerifyCerts)
	//-----------------------------------------------------------------------------
	{
		return connect(sServer.c_str(), sPort.c_str(), bVerifyCerts);
	}

	//-----------------------------------------------------------------------------
	boost::asio::basic_socket<ip::tcp, stream_socket_service<ip::tcp> >& GetTCPSocket()
	//-----------------------------------------------------------------------------
	{
		return m_Socket.lowest_layer();
	}

//----------
private:
//----------

	io_service m_IO_Service;
	ssl::context m_Context;
	ssl::stream<ip::tcp::socket> m_Socket;
	ip::tcp::resolver::iterator m_ConnectedHost;

};

} // end of namespace KSSL_detail

/// SSL stream based on boost::iostreams and asio::ssl
using KSSLStream = KReaderWriter<KSSL_detail::KSSLIOStream>;

}

