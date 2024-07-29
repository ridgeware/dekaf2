/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2024, Ridgeware, Inc.
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

/// @file kquicstream.h
/// provides an implementation of std::iostreams supporting QUIC

#include "kconfiguration.h"

#if DEKAF2_HAS_OPENSSL_QUIC

#include "ktlscontext.h"
#include "kstring.h"
#include "kstreambuf.h"
#include "kurl.h"
#include "kstreamoptions.h"
#include "bits/kunique_deleter.h"
#include "kiostreamsocket.h"
#include <openssl/ssl.h>

DEKAF2_NAMESPACE_BEGIN

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// std::iostream QUIC implementation with timeout.
class DEKAF2_PUBLIC KQuicStream : public KIOStreamSocket
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
	using base_type = KIOStreamSocket;

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected client stream
	/// @param iSecondsTimeout
	/// Timeout for any I/O. Defaults to 15 seconds.
	KQuicStream();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs an unconnected stream
	/// @param Context
	/// A KTLSContext which defines role (server/client). Custom certs and crypto suites
	/// will also be defined with the KTLSContext.
	/// @param iSecondsTimeout
	/// Timeout for any I/O. Defaults to 15 seconds.
	KQuicStream(KTLSContext& Context, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected client stream
	/// @param Context
	/// A KTLSContext which defines role (server/client). Custom certs and crypto suites
	/// will also be defined with the KTLSContext.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	KQuicStream(KTLSContext& Context, const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Constructs a connected client stream, using the default client context
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	KQuicStream(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{});
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Connects a given server as a client.
	/// @param Endpoint
	/// KTCPEndPoint as the server to connect to - can be constructed from
	/// a variety of inputs, like strings or KURL
	/// @param Options
	/// set options like certificate verification, manual TLS handshake, HTTP2 request, and the timeout
	virtual bool Connect(const KTCPEndPoint& Endpoint, KStreamOptions Options = KStreamOptions{}) override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Disconnect the stream
	virtual bool Disconnect() override final
	//-----------------------------------------------------------------------------
	{
		return false; // TODO
	}

	//-----------------------------------------------------------------------------
	virtual bool is_open() const override final
	//-----------------------------------------------------------------------------
	{
		return m_NativeSocket >= 0;
	}

	//-----------------------------------------------------------------------------
	/// tests for a closed connection of the remote side by trying to peek one byte
	virtual bool IsDisconnected() override final
	//-----------------------------------------------------------------------------
	{
		return false; // TODO m_Stream.IsDisconnected();
	}

	//-----------------------------------------------------------------------------
	/// Upgrade connection from TCP to TCP over TLS. Returns true on success. Can also
	/// be used to force a handshake before any IO is triggered.
	virtual bool StartManualTLSHandshake() override final;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Gets the underlying OS level native socket of the stream
	virtual native_socket_type GetNativeSocket() override final
	//-----------------------------------------------------------------------------
	{
		return m_NativeSocket;
	}

	//-----------------------------------------------------------------------------
	/// Gets the underlying openssl handle of the stream
	virtual native_tls_handle_type GetNativeTLSHandle() override final
	//-----------------------------------------------------------------------------
	{
		return m_SSL.get();
	}

	//-----------------------------------------------------------------------------
	/// Gets the KTLSContext used in construction
	const KTLSContext& GetContext() const
	//-----------------------------------------------------------------------------
	{
		return m_TLSContext;
	}

	//-----------------------------------------------------------------------------
	std::streamsize direct_read_some(void* sBuffer, std::streamsize iCount);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get a reference to self
	KQuicStream& GetKQuicStream()
	//-----------------------------------------------------------------------------
	{
		return *this;
	}

	//-----------------------------------------------------------------------------
	virtual bool Good() const override final
	//-----------------------------------------------------------------------------
	{
		return Error().empty();
	}

	//-----------------------------------------------------------------------------
	/// request to switch to HTTP3
	/// @returns true if protocol request is permitted
	bool SetRequestHTTP3();
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf reader
	DEKAF2_PRIVATE
	static std::streamsize QuicStreamReader(void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// this is the custom streambuf writer
	DEKAF2_PRIVATE
	static std::streamsize QuicStreamWriter(const void* sBuffer, std::streamsize iCount, void* stream);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_PRIVATE
	bool Handshake();
	//-----------------------------------------------------------------------------

	KTLSContext&           m_TLSContext;
	KUniquePtr<::SSL, ::SSL_free>
	                       m_SSL;
	native_socket_type     m_NativeSocket   { -1    };
	bool                   m_bNeedHandshake { true  };

	KBufferedStreamBuf     m_QuicStreamBuf { &QuicStreamReader, &QuicStreamWriter, this, this };

}; // KQuicStream


/// Quic stream based on std::iostream and asio::ssl
//using KQuicStream = KReaderWriter<KQuicStream>;
using KQuicStream = KQuicStream;

// there is nothing special with a quic client
using KQuicClient = KQuicStream;

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KQuicStream> CreateKQuicServer(KTLSContext& Context, KDuration Timeout = KStreamOptions::GetDefaultTimeout());
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KQuicClient> CreateKQuicClient();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_PUBLIC
std::unique_ptr<KQuicClient> CreateKQuicClient(const KTCPEndPoint& EndPoint, KStreamOptions Options);
//-----------------------------------------------------------------------------

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_OPENSSL_QUIC
