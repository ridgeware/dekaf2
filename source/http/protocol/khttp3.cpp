/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
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
 //
 */

/*
 * Portions of this code have been written based on the OpenSSL demo code for
 * the HTTP/3 integration of the OpenSSL QUIC layer and nghttp3. The OpenSSL
 * demo code uses the below copyright.
 */

/*
 * Copyright 2023 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <dekaf2/http/protocol/khttp3.h>

#if DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_NGTCP2

#include <dekaf2/core/format/kformat.h>
#include <dekaf2/io/readwrite/kread.h>
#include <dekaf2/io/readwrite/kwrite.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <chrono>
#include <cstring>

// nghttp3 hands out nghttp3_vec, ngtcp2 takes ngtcp2_vec - they are the same layout
static_assert(sizeof(nghttp3_vec) == sizeof(ngtcp2_vec)
              && offsetof(nghttp3_vec, base) == offsetof(ngtcp2_vec, base)
              && offsetof(nghttp3_vec, len ) == offsetof(ngtcp2_vec, len ),
              "nghttp3_vec and ngtcp2_vec must have the same layout");

DEKAF2_NAMESPACE_BEGIN

namespace khttp3
{


//-----------------------------------------------------------------------------
Stream::Stream(
	Session&                   Session,
	KURL                       url,
	KHTTPMethod                Method,
	const KHTTPRequestHeaders& RequestHeaders,
	KHTTPResponseHeaders&      ResponseHeaders
)
//-----------------------------------------------------------------------------
: Stream(Session, Type::Request)
{
	m_RequestHeaders  = &RequestHeaders;
	m_ResponseHeaders = &ResponseHeaders;
	m_URI             = std::move(url);
	m_Method          = Method;
}

//-----------------------------------------------------------------------------
Stream::Stream(Session& session, Type type)
//-----------------------------------------------------------------------------
: m_Session(session)
, m_Type(type)
{
	bool bOK;

	if (type == Type::Request)
	{
		bOK = m_Session.GetConnection().OpenBidiStream(m_StreamID);
	}
	else
	{
		bOK = m_Session.GetConnection().OpenUniStream(m_StreamID);
	}

	if (!bOK)
	{
		SetError(kFormat("could not create QUIC stream: {}", m_Session.GetConnection().GetLastError()));
	}
	else
	{
		kDebug(4, "[stream {}] created", GetStreamID());
	}

} // ctor

//-----------------------------------------------------------------------------
Stream::Stream(Session& session, ID StreamID, Type type)
//-----------------------------------------------------------------------------
: m_Session(session)
, m_StreamID(StreamID)
, m_Type(type)
{
	kDebug(4, "[stream {}] accepted", GetStreamID());
}

//-----------------------------------------------------------------------------
KStringView Stream::GetScheme()
//-----------------------------------------------------------------------------
{
	return m_URI.Protocol.getProtocolName();

} // GetScheme

//-----------------------------------------------------------------------------
KStringView Stream::GetAuthority()
//-----------------------------------------------------------------------------
{
	if (m_sAuthority.empty())
	{
		m_sAuthority  = m_URI.Domain;

		if (m_URI.Port.get() != 0)
		{
			m_sAuthority += ':';
			m_sAuthority += m_URI.Port.Serialize();
		}
	}

	return m_sAuthority;

} // GetAuthority

//-----------------------------------------------------------------------------
KStringView Stream::GetPath()
//-----------------------------------------------------------------------------
{
	if (m_sPath.empty())
	{
		m_URI.Path.Serialize(m_sPath);

		if (m_sPath.empty())
		{
			m_sPath = "/";
		}
		else
		{
			m_URI.Query.WantStartSeparator();
			m_URI.Query.Serialize(m_sPath);
		}
	}

	return m_sPath;

} // GetPath

//-----------------------------------------------------------------------------
void Stream::Close ()
//-----------------------------------------------------------------------------
{
	m_bIsClosed = true;

	if (m_DataConsumer)
	{
		m_DataConsumer->SetFinished();
	}

} // Close

//-----------------------------------------------------------------------------
int Stream::AddResponseHeader  (ID id, KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (!IsHeadersComplete())
	{
		if (m_ResponseHeaders)
		{
			if (sName == ":status")
			{
				kDebug(2, "[stream {}] setting HTTP response status to {}", id, sValue.UInt16());
				m_ResponseHeaders->SetStatus(sValue.UInt16());
				m_ResponseHeaders->SetHTTPVersion(KHTTPVersion::http3);
			}
			else
			{
				kDebug(2, "[stream {}] {}: {}", id, sName, sValue);
				m_ResponseHeaders->Headers.Add(sName, sValue);
			}
		}
		else
		{
			SetError(kFormat("stream {} has no response header struct set", id));
			return 1;
		}
	}

	return 0;

} // AddResponseHeader

//-----------------------------------------------------------------------------
int Stream::Reset(int64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	// nghttp3 asks us to abandon our sending side of this stream
	m_Session.GetConnection().ShutdownStreamWrite(GetStreamID(), static_cast<uint64_t>(iAppErrorCode));

	return 0;

} // Reset

//-----------------------------------------------------------------------------
int Stream::StopSending(int64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	// nghttp3 asks us to tell the peer to stop sending on this stream
	m_Session.GetConnection().ShutdownStreamRead(GetStreamID(), static_cast<uint64_t>(iAppErrorCode));

	return 0;

} // StopSending

//-----------------------------------------------------------------------------
int Stream::ReadFromDataProvider(KStringView& sBuffer, uint32_t* iPFlags)
//-----------------------------------------------------------------------------
{
	*iPFlags = 0;

	if (!m_DataProvider || m_DataProvider->IsEOF())
	{
		*iPFlags = NGHTTP3_DATA_FLAG_EOF;
		kDebug(4, "EOF set");
		return 0;
	}

	if (!m_DataProvider->Read(sBuffer))
	{
		// this data provider has no persistent data, we need to buffer it
		// until it gets acked
		auto Buffer = (m_IdleBuffer) ? std::move(m_IdleBuffer) : std::make_unique<BufferedData>();

		Buffer->Data.resize(m_DataProvider->Read(Buffer->Data.data(), Buffer->Data.capacity()));
		Buffer->iLastPos = m_iTotalTXData + Buffer->Data.size();

		sBuffer = KStringView(Buffer->Data.data(), Buffer->Data.size());

		m_TXBuffer.push_back(std::move(Buffer));
	}

	if (sBuffer.empty())
	{
		*iPFlags = NGHTTP3_DATA_FLAG_EOF;
		return 0;
	}

	m_iTotalTXData += sBuffer.size();

	return 1;

} // ReadFromDataProvider

//-----------------------------------------------------------------------------
int Stream::AckedStreamData(std::size_t iReceived)
//-----------------------------------------------------------------------------
{
	m_iTotalAckedTXData += iReceived;

	kDebug(4, "[stream {}] ACKed: {} (+{})", GetStreamID(), m_iTotalAckedTXData, iReceived);

	for (auto it = m_TXBuffer.begin(); it != m_TXBuffer.end();)
	{
		if (it->get()->iLastPos <= m_iTotalAckedTXData)
		{
			m_IdleBuffer = std::move(*it);
			m_IdleBuffer->clear();
			it = m_TXBuffer.erase(it);
		}
		else
		{
			break;
		}
	}

	return 0;

} // AckedStreamData

//-----------------------------------------------------------------------------
std::size_t Stream::AddData(KStringView sData)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] received {} bytes", GetStreamID(), sData.size());

	if (m_DataConsumer)
	{
		m_DataConsumer->Write(sData.data(), sData.size());
		return sData.size();
	}

	auto iConsumed  = m_RXBuffer.append(sData);
	auto iRemaining = sData.size() - iConsumed;

	if (iRemaining)
	{
		// the spill buffer holds data that the application has not yet taken -
		// its flow control credit is given back when it drains, which keeps a
		// fast sender from growing the buffer without bounds
		m_RXSpillBuffer.append(sData.data() + iConsumed, iRemaining);
	}

	return iConsumed;

} // AddData

//-----------------------------------------------------------------------------
void Stream::SetReceiveBuffer(KBuffer buffer)
//-----------------------------------------------------------------------------
{
	m_RXBuffer = buffer;

	if (!m_RXSpillBuffer.empty())
	{
		auto iCopied = m_RXBuffer.append(m_RXSpillBuffer);
		m_RXSpillBuffer.erase(0, iCopied);
		// now the peer may send that much again
		m_Session.CreditReceived(GetStreamID(), iCopied);
	}

} // SetReceiveBuffer


//-----------------------------------------------------------------------------
void Stream::Block()
//-----------------------------------------------------------------------------
{
	if (!m_bIsBlocked)
	{
		kDebug(4, "[stream {}] setting stream to block", GetStreamID());
		m_bIsBlocked = true;
	}

	::nghttp3_conn_block_stream(m_Session.GetNGHTTP3_Session(), GetStreamID());
	AddWaitFor(WaitFor::Writes);

} // Block

//-----------------------------------------------------------------------------
void Stream::Unblock()
//-----------------------------------------------------------------------------
{
	if (m_bIsBlocked)
	{
		kDebug(4, "[stream {}] setting stream to unblock", GetStreamID());
		m_bIsBlocked = false;
	}

	::nghttp3_conn_unblock_stream(m_Session.GetNGHTTP3_Session(), GetStreamID());
	DelWaitFor(WaitFor::Writes);

} // Unblock


//-----------------------------------------------------------------------------
Session::Session(KQuicStream& QuicConnection, bool bIsClient)
//-----------------------------------------------------------------------------
: m_KQuicStream(QuicConnection)
, m_Connection(QuicConnection.GetConnection())
{
	if (!m_Connection.IsConnected())
	{
		SetError("QUIC connection is not established");
		return;
	}

	/*
	 * HTTP/3 requires a couple of unidirectional management streams: a control
	 * stream and two QPACK streams for each side of a connection. The peer's
	 * transport parameters are known once the handshake completed - which
	 * KQuicConnection::Connect() waits for - so we can check here whether it
	 * lets us open them.
	 */
	if (m_Connection.GetStreamsUniLeft() < 3)
	{
		SetError("peer does not allow at least 3 unidirectional streams");
		return;
	}

	nghttp3_callbacks callbacks;
	std::memset(&callbacks, 0, sizeof(nghttp3_callbacks));

	// setup our callbacks for nghttp3
	callbacks.recv_header       = on_recv_header;
	callbacks.end_headers       = on_end_headers;
	callbacks.recv_data         = on_recv_data;
	callbacks.end_stream        = on_end_stream;
	callbacks.stream_close      = on_stream_close;
	callbacks.stop_sending      = on_stop_sending;
	callbacks.reset_stream      = on_reset_stream;
	callbacks.deferred_consume  = on_deferred_consume;
	callbacks.acked_stream_data = on_acked_stream_data;

	// create default settings
	nghttp3_settings settings;
	std::memset(&settings, 0, sizeof(nghttp3_settings));
	nghttp3_settings_default(&settings);

	// create the HTTP/3 client state
	auto ec = ::nghttp3_conn_client_new(&m_Session, &callbacks, &settings, nullptr, this);

	if (ec < 0)
	{
		SetError(kFormat("cannot create nghttp3 connection: {}", ::nghttp3_strerror(ec)), ec);
		return;
	}

	if (!m_Connection.OpenUniStream(m_ControlStreamID)
		|| !m_Connection.OpenUniStream(m_QPackEncStreamID)
		|| !m_Connection.OpenUniStream(m_QPackDecStreamID))
	{
		SetError(kFormat("cannot open the HTTP/3 control streams: {}", m_Connection.GetLastError()));
		return;
	}

	/*
	 * Tell the HTTP/3 stack which stream IDs are used for our outgoing control
	 * and QPACK streams. Note that we don't have to tell the HTTP/3 stack what
	 * IDs are used for incoming streams as this is inferred automatically from
	 * the stream type byte which starts every incoming unidirectional stream,
	 * so it will autodetect the correct stream IDs for the incoming control and
	 * QPACK streams initiated by the server.
	 */
	ec = ::nghttp3_conn_bind_control_stream(m_Session, m_ControlStreamID);

	if (ec < 0)
	{
		SetError(kFormat("cannot bind nghttp3 control stream: {}", ::nghttp3_strerror(ec)), ec);
		return;
	}

	ec = ::nghttp3_conn_bind_qpack_streams(m_Session, m_QPackEncStreamID, m_QPackDecStreamID);

	if (ec < 0)
	{
		SetError(kFormat("cannot bind nghttp3 QPACK streams: {}", ::nghttp3_strerror(ec)), ec);
		return;
	}

	// from now on all stream data of the connection is routed through this session
	m_Connection.SetDelegate(this);

} // ctor

//-----------------------------------------------------------------------------
Session::~Session()
//-----------------------------------------------------------------------------
{
	if (m_Connection.GetDelegate() == this)
	{
		m_Connection.SetDelegate(nullptr);

		// there is no HTTP/3 without this session - end the QUIC connection
		// with the proper application error code, the peer logs it otherwise
		if (m_Connection.IsConnected())
		{
			m_Connection.Close(NGHTTP3_H3_NO_ERROR);
		}
	}

	m_Streams.clear();

	if (m_Session)
	{
		::nghttp3_conn_del(m_Session);
	}

} // dtor


//-----------------------------------------------------------------------------
bool Session::HaveOpenRequestStreams(bool bWithResponses) const
//-----------------------------------------------------------------------------
{
	for (auto& Stream : m_Streams)
	{
		if (Stream.second->GetType() == Stream::Type::Request)
		{
			if (bWithResponses) return true;
			if (Stream.second->IsHeadersComplete() == false) return true;
		}
	}
	return false;

} // HaveOpenRequestStreams


//-----------------------------------------------------------------------------
void Session::PurgeStreams()
//-----------------------------------------------------------------------------
{
	for (auto it = m_Streams.begin(); it != m_Streams.end();)
	{
		if (it->second->CanDelete())
		{
			kDebug(4, "[stream {}] will be purged", it->first);
			it = m_Streams.erase(it);
		}
		else
		{
			++it;
		}
	}

} // PurgeStreams

//-----------------------------------------------------------------------------
KQuicConnection::PumpResult Session::Pump(KDuration MaxWait)
//-----------------------------------------------------------------------------
{
	auto Result = m_Connection.Pump(MaxWait);

	if (Result == KQuicConnection::PumpResult::Error && !HasError())
	{
		SetError(m_Connection.GetLastError().empty() ? KStringViewZ("QUIC connection failed") : m_Connection.GetLastError());
	}

	return Result;

} // Pump

//-----------------------------------------------------------------------------
bool Session::Run(bool bWithResponses)
//-----------------------------------------------------------------------------
{
	// the event loop: pump the QUIC connection until all request streams
	// are done (or their headers, if bWithResponses is false). The timeout
	// restarts with every received datagram - ngtcp2's own timers may
	// fire more often than that without any progress on the streams.
	auto Timeout = GetTimeout();
	auto tLast   = chrono::steady_clock::now();

	for (;;)
	{
		if (HasError())
		{
			return false;
		}

		PurgeStreams();

		if (!HaveOpenRequestStreams(bWithResponses))
		{
			return true;
		}

		KDuration Remaining = Timeout - KDuration(chrono::steady_clock::now() - tLast);

		if (Remaining <= KDuration::zero())
		{
			kDebug(1, "connection timed out");
			return SetError("connection timed out");
		}

		auto Result = Pump(Remaining);

		if (Result == KQuicConnection::PumpResult::Error)
		{
			return false;
		}
		else if (Result == KQuicConnection::PumpResult::Received)
		{
			tLast = chrono::steady_clock::now();
		}
	}

} // Run

//-----------------------------------------------------------------------------
Stream::ID Session::NewStream(std::unique_ptr<Stream> Stream)
//-----------------------------------------------------------------------------
{
	if (Stream->HasError())
	{
		SetError(Stream->CopyLastError());
		return -1;
	}

	auto StreamID = Stream->GetStreamID();

	if (!AddStream(StreamID, std::move(Stream)))
	{
		return -1;
	}

	return StreamID;

} // NewStream

//-----------------------------------------------------------------------------
Stream* Session::GetStream(Stream::ID StreamID)
//-----------------------------------------------------------------------------
{
	auto it = m_Streams.find(StreamID);

	if (it == m_Streams.end())
	{
		// do not make this an error - it happens after closing a stream
		// and a following check if the stream is at eof
		kDebug(4, "cannot find stream ID {}", StreamID);
		return nullptr;
	}

	return it->second.get();

} // GetStream

//-----------------------------------------------------------------------------
bool Session::AddStream(Stream::ID StreamID, std::unique_ptr<Stream> Stream)
//-----------------------------------------------------------------------------
{
	if (!m_Streams.emplace(StreamID, std::move(Stream)).second)
	{
		return SetError(kFormat("cannot add stream {} to session", StreamID));
	}

	return true;

} // AddStream

//-----------------------------------------------------------------------------
bool Session::DeleteStream(Stream::ID StreamID)
//-----------------------------------------------------------------------------
{
	if (m_Streams.erase(StreamID) == 1)
	{
		kDebug(4, "[stream {}] deleted stream", StreamID);
		return true;
	}

	return SetError(kFormat("unknown stream id: {}", StreamID));

} // DeleteStream

//-----------------------------------------------------------------------------
Stream::ID Session::NewRequest (std::unique_ptr<Stream> Stream)
//-----------------------------------------------------------------------------
{
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// convert from KHTTPHeaders into a nghttp3_nv struct
	struct http3header : public nghttp3_nv
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		http3header(KStringView sName, KStringView sValue, bool bNoCopyName = true, bool bNoCopyValue = true)
		{
			name     = reinterpret_cast<uint8_t*>(const_cast<char*>(sName .data()));
			value    = reinterpret_cast<uint8_t*>(const_cast<char*>(sValue.data()));
			namelen  = sName .size();
			valuelen = sValue.size();
			flags    = bNoCopyName  ? NGHTTP3_NV_FLAG_NO_COPY_NAME  : NGHTTP3_NV_FLAG_NONE;
			flags   |= bNoCopyValue ? NGHTTP3_NV_FLAG_NO_COPY_VALUE : NGHTTP3_NV_FLAG_NONE;
		}

	}; // http3header

	auto StreamID       = Stream->GetStreamID();
	auto RequestHeaders = Stream->GetRequestHeaders();

	if (!RequestHeaders)
	{
		SetError("no request headers set");
		return -1;
	}

	// we prefer to get the authority from the host header
	KStringView sAuthority = RequestHeaders->Headers.Get(KHTTPHeader::HOST);

	if (sAuthority.empty())
	{
		sAuthority = Stream->GetAuthority();
	}

	if (m_sAuthority.empty())
	{
		m_sAuthority = sAuthority;
	}
	else if (m_sAuthority != sAuthority)
	{
		SetError(kFormat("registered authority '{}' does not match new request: '{}'", m_sAuthority, sAuthority));
		return -1;
	}

	std::vector<nghttp3_nv> Headers;
	Headers.reserve(RequestHeaders->Headers.size() + 4);

	Headers.push_back(http3header { ":method"    , Stream->GetMethod    () });
	Headers.push_back(http3header { ":scheme"    , Stream->GetScheme    () });
	Headers.push_back(http3header { ":authority" , m_sAuthority           });
	// !! copy the path - if it is < 15 chars it would move out of scope when Stream is moved!
	Headers.push_back(http3header { ":path"      , Stream->GetPath      (), true, false });

	// add all 'normal' headers (with lowercase names)
	for (const auto& Header : RequestHeaders->Headers)
	{
		// drop non-http/3 headers
		if (DEKAF2_UNLIKELY(
			Header.first == KHTTPHeader::CONNECTION        ||
			Header.first == KHTTPHeader::KEEP_ALIVE        ||
			Header.first == KHTTPHeader::PROXY_CONNECTION  ||
			Header.first == KHTTPHeader::TRANSFER_ENCODING ||
			Header.first == KHTTPHeader::UPGRADE))
		{
			// log all dropped headers, except the dropped HOST header
			// - we pushed it into the :authority pseudo header
			kDebug(2, "dropping non-HTTP/3 header: {}: {}", Header.first, Header.second);
		}
		else if (Header.first != KHTTPHeader::HOST)
		{
			Headers.push_back(http3header(Header.first.Serialize(), Header.second));
		}
	}

	// this struct will be COPIED with nghttp3_submit_request // TODO check if that is still true!!
	nghttp3_data_reader Data;

	auto SendData = Stream->GetDataProvider();

	if (SendData && !SendData->IsEOF())
	{
		Data.read_data  = on_data_source_read;
	}
	else
	{
		Data.read_data  = nullptr;
	}

	// we better store the stream before we submit the request - in a multithreaded
	// environment it could fire callbacks before we're done here
	if (!AddStream(StreamID, std::move(Stream)))
	{
		return -1;
	}

	// Stream is now moved away

	// submit the request
	auto ec = nghttp3_conn_submit_request(
		m_Session,
		StreamID,
		Headers.data(), Headers.size(),
		Data.read_data ? &Data : nullptr,
		nullptr
	);

	if (ec < 0) 
	{
		SetError(kFormat("cannot submit HTTP/3 request: {}", ::nghttp3_strerror(ec)), ec);
		return -1;
	}

	if (kWouldLog(2))
	{
		// output all headers for debugging
		for (const auto& Header : Headers)
		{
			kDebug(2, "[stream {}] {}: {}", StreamID, ToView(Header.name, Header.namelen), ToView(Header.value, Header.valuelen));
		}
	}

	if (Data.read_data)
	{
		kDebug(4, "[stream {}] we have request data to send", StreamID);
	}

	return StreamID;

} // NewRequest

//-----------------------------------------------------------------------------
Stream::ID Session::NewStream(
	KURL                           url,
	KHTTPMethod                    Method,
	const KHTTPHeaders&            RequestHeaders,
	std::unique_ptr<KDataProvider> SendData,
	KHTTPResponseHeaders&          ResponseHeaders,
	std::unique_ptr<KDataConsumer> ReceiveData
)
//-----------------------------------------------------------------------------
{
	auto stream = std::make_unique<Stream>(*this, std::move(url), Method, RequestHeaders, ResponseHeaders);

	stream->SetDataProvider(std::move(SendData));
	stream->SetDataConsumer(std::move(ReceiveData));

	return NewRequest(std::move(stream));

} // NewStream

//-----------------------------------------------------------------------------
Stream::ID SingleStreamSession::SubmitRequest(
	KURL                           url,
	KHTTPMethod                    Method,
	const KHTTPRequestHeaders&     RequestHeaders,
	std::unique_ptr<KDataProvider> SendData,
	KHTTPResponseHeaders&          ResponseHeaders
)
//-----------------------------------------------------------------------------
{
	auto stream = std::make_unique<Stream>(GetSession(), std::move(url), Method, RequestHeaders, ResponseHeaders);

	stream->SetDataProvider(std::move(SendData));

	auto StreamID = NewRequest(std::move(stream));

	if (StreamID < 0)
	{
		return -1;
	}

	// finally send all the prepared request data
	// and receive the response headers, but do not wait for the response body here!
	Run(false);

	return StreamID;

} // SubmitRequest

//-----------------------------------------------------------------------------
std::streamsize SingleStreamSession::ReadData(Stream::ID StreamID, void* data, std::size_t len)
//-----------------------------------------------------------------------------
{
	// this method is used by the streamreader inside KHTTPClient to grab data from the single
	// http/3 request stream into the std::streambuf struct that serves underneath the iostream
	// interface of KHTTPClient
	if (!len)
	{
		return 0;
	}

	auto Stream = GetStream(StreamID);

	if (!Stream)
	{
		return -1;
	}

	if (!Stream->IsHeadersComplete() || !data)
	{
		return -1;
	}

	// this also drains the spill buffer into the new receive buffer
	Stream->SetReceiveBuffer( { data, len } );

	std::streamsize iRead = Stream->GetReceiveBuffer().size();

	if (iRead == 0 && !Stream->IsClosed())
	{
		// nothing waiting - pump the connection until data arrives, the stream ends, or we time out
		auto Timeout = GetTimeout();
		auto tLast   = chrono::steady_clock::now();

		for (;;)
		{
			KDuration Remaining = Timeout - KDuration(chrono::steady_clock::now() - tLast);

			if (Remaining <= KDuration::zero())
			{
				kDebug(1, "[stream {}] connection timed out", StreamID);
				SetError("connection timed out");
				break;
			}

			auto Result = Pump(Remaining);

			if (Result == KQuicConnection::PumpResult::Error)
			{
				break;
			}
			else if (Result == KQuicConnection::PumpResult::Received)
			{
				tLast = chrono::steady_clock::now();
			}

			iRead = Stream->GetReceiveBuffer().size();

			if (iRead > 0 || Stream->IsClosed())
			{
				break;
			}
		}
	}

	// the caller's buffer is only valid during this call - further data goes to the spill buffer
	Stream->SetReceiveBuffer( KBuffer{} );

	if (len != static_cast<std::size_t>(iRead))
	{
		kDebug(4, "[stream {}] requested {}, got {} bytes", StreamID, len, iRead);
	}

	return iRead;

} // ReadData




//-----------------------------------------------------------------------------
int Session::OnReceiveHeader(Stream::ID StreamID, KStringView sName, KStringView sValue, uint8_t iFlags)
//-----------------------------------------------------------------------------
{
//	kDebug(4, "[stream {}] receive header", StreamID);
	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		return Stream->AddResponseHeader(StreamID, sName, sValue);
	}

	return 0;

} // OnReceiveHeader

//-----------------------------------------------------------------------------
int Session::OnEndHeaders(Stream::ID StreamID, int fin)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] headers complete", StreamID);
	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		Stream->SetHeadersComplete();
	}

	return 0;

} // OnEndHeaders

//-----------------------------------------------------------------------------
int Session::OnReceiveData(Stream::ID StreamID, KStringView sData)
//-----------------------------------------------------------------------------
{
	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		// only the bytes taken right away are credited now, the rest when the spill buffer drains
		CreditReceived(StreamID, Stream->AddData(sData));
	}
	else
	{
		CreditReceived(StreamID, sData.size());
	}

	return 0;

} // OnReceiveData

//-----------------------------------------------------------------------------
int Session::OnEndStream(Stream::ID StreamID)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] stream ended", StreamID);

	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		Stream->Close();
	}

	return 0;

} // OnEndStream

//-----------------------------------------------------------------------------
int Session::OnStreamClose(Stream::ID StreamID, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] stream closed", StreamID);

	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		Stream->Close();
	}

	return 0;

} // OnStreamClose

//-----------------------------------------------------------------------------
int Session::OnStopSending(Stream::ID StreamID, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] stop sending", StreamID);

	// nghttp3 wants the peer to stop sending on this stream
	m_Connection.ShutdownStreamRead(StreamID, iAppErrorCode);

	return 0;

} // OnStopSending

//-----------------------------------------------------------------------------
int Session::OnResetStream(Stream::ID StreamID, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] reset stream", StreamID);
	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		return Stream->Reset(iAppErrorCode);
	}

	return 0;

} // OnResetStream

//-----------------------------------------------------------------------------
int Session::OnDeferredConsume(Stream::ID StreamID, std::size_t iConsumed)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] deferred consume: {} bytes", StreamID, iConsumed);
	// nghttp3 consumed data it had buffered internally - give the credit back to the peer
	CreditReceived(StreamID, iConsumed);

	return 0;

} // OnDeferredConsume

//-----------------------------------------------------------------------------
::nghttp3_ssize Session::OnDataSourceRead(
	Stream::ID StreamID,
	KStringView& sBuffer,
	std::size_t iMaxBuffers,
	uint32_t* iPFlags
)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] data source read", StreamID);
	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		return Stream->ReadFromDataProvider(sBuffer, iPFlags);
	}

	return 0;

} // OnDataSourceRead

//-----------------------------------------------------------------------------
int Session::OnAckedStreamData (Stream::ID StreamID, std::size_t iTotalReceived)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] acked {} bytes", StreamID, iTotalReceived);
	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		Stream->AckedStreamData(iTotalReceived);
	}

	return 0;

} // OnAckedStreamData


// -------------------------------------------------------------------
// the KQuicConnection::Delegate interface: ngtcp2 <-> nghttp3
// -------------------------------------------------------------------

//-----------------------------------------------------------------------------
int Session::DelegateError(KString sError)
//-----------------------------------------------------------------------------
{
	SetError(sError);
	m_Connection.SetDelegateError(std::move(sError));
	return -1;

} // DelegateError

//-----------------------------------------------------------------------------
std::ptrdiff_t Session::OnQuicStreamData(KQuicConnection::StreamID id, KStringView sData, bool bFin)
//-----------------------------------------------------------------------------
{
	/*
	 * This function is confusingly named as it is named from nghttp3's
	 * 'perspective'; it is used to pass data *into* the HTTP/3 stack which
	 * has been received from the network. nghttp3 consumes all of it: the
	 * return value covers the framing, the DATA frame payload is reported
	 * through the recv_data callback (and credited there).
	 */
	auto iRead = ::nghttp3_conn_read_stream(
		m_Session,
		id,
		reinterpret_cast<const uint8_t*>(sData.data()), sData.size(),
		bFin ? 1 : 0
	);

	if (iRead < 0)
	{
		DelegateError(kFormat("nghttp3 failed to process incoming data: {}", ::nghttp3_strerror(static_cast<int>(iRead))));
		return -1;
	}

	if (bFin)
	{
		kDebug(4, "[stream {}] stream finished", id);
	}

	return iRead;

} // OnQuicStreamData

//-----------------------------------------------------------------------------
int Session::OnQuicAckedStreamData(KQuicConnection::StreamID id, uint64_t iOffset, uint64_t iLen)
//-----------------------------------------------------------------------------
{
	// nghttp3 may now free the buffers, and reports it upwards through acked_stream_data
	auto ec = ::nghttp3_conn_add_ack_offset(m_Session, id, iLen);

	if (ec != 0 && ec != NGHTTP3_ERR_STREAM_NOT_FOUND)
	{
		return DelegateError(kFormat("nghttp3 rejected ack offset: {}", ::nghttp3_strerror(ec)));
	}

	return 0;

} // OnQuicAckedStreamData

//-----------------------------------------------------------------------------
int Session::OnQuicStreamClose(KQuicConnection::StreamID id, bool bHasAppErrorCode, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	auto ec = ::nghttp3_conn_close_stream(m_Session, id, bHasAppErrorCode ? iAppErrorCode : NGHTTP3_H3_NO_ERROR);

	if (ec != 0 && ec != NGHTTP3_ERR_STREAM_NOT_FOUND)
	{
		return DelegateError(kFormat("nghttp3 cannot close stream: {}", ::nghttp3_strerror(ec)));
	}

	return 0;

} // OnQuicStreamClose

//-----------------------------------------------------------------------------
int Session::OnQuicStreamReset(KQuicConnection::StreamID id, uint64_t iFinalSize, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	// the peer will not send more on this stream
	auto ec = ::nghttp3_conn_shutdown_stream_read(m_Session, id);

	if (ec != 0 && ec != NGHTTP3_ERR_STREAM_NOT_FOUND)
	{
		return DelegateError(kFormat("nghttp3 cannot shutdown stream read: {}", ::nghttp3_strerror(ec)));
	}

	return 0;

} // OnQuicStreamReset

//-----------------------------------------------------------------------------
int Session::OnQuicStopSending(KQuicConnection::StreamID id, uint64_t iAppErrorCode)
//-----------------------------------------------------------------------------
{
	// the peer does not want more from us on this stream
	::nghttp3_conn_shutdown_stream_write(m_Session, id);

	return 0;

} // OnQuicStopSending

//-----------------------------------------------------------------------------
int Session::OnQuicStreamUnblocked(KQuicConnection::StreamID id)
//-----------------------------------------------------------------------------
{
	auto Stream = GetStream(id);

	if (Stream)
	{
		Stream->Unblock();
	}
	else
	{
		::nghttp3_conn_unblock_stream(m_Session, id);
	}

	return 0;

} // OnQuicStreamUnblocked

//-----------------------------------------------------------------------------
std::ptrdiff_t Session::GetQuicStreamData(KQuicConnection::StreamID& id, bool& bFin, ngtcp2_vec* vecs, std::size_t iMaxVecs)
//-----------------------------------------------------------------------------
{
	/*
	 * Get a number of send vectors from the HTTP/3 engine.
	 *
	 * Note that this function is confusingly named as it is named from
	 * nghttp3's 'perspective': this outputs pointers to data which nghttp3
	 * wants to *write* to the network. The data stays valid until nghttp3
	 * gets the ack offset for it (OnQuicAckedStreamData).
	 */
	int fin { 0 };
	id = -1;

	auto iVecs = ::nghttp3_conn_writev_stream(m_Session, &id, &fin, reinterpret_cast<nghttp3_vec*>(vecs), iMaxVecs);

	if (iVecs < 0)
	{
		DelegateError(kFormat("nghttp3 failed to produce outgoing data: {}", ::nghttp3_strerror(static_cast<int>(iVecs))));
		return -1;
	}

	bFin = (fin != 0);

	if (iVecs > 0 && kWouldLog(4))
	{
		kDebug(4, "[stream {}] offering {} bytes{}", id, ::nghttp3_vec_len(reinterpret_cast<nghttp3_vec*>(vecs), iVecs), bFin ? " with FIN" : "");
	}

	return iVecs;

} // GetQuicStreamData

//-----------------------------------------------------------------------------
int Session::OnQuicWriteOffset(KQuicConnection::StreamID id, std::size_t iWritten)
//-----------------------------------------------------------------------------
{
	/*
	 * Tell nghttp3 we have consumed the data it output when we
	 * called writev_stream, otherwise subsequent calls to
	 * writev_stream will output the same data. Also called with 0
	 * when a FIN without data was written.
	 */
	auto ec = ::nghttp3_conn_add_write_offset(m_Session, id, iWritten);

	if (ec != 0 && ec != NGHTTP3_ERR_STREAM_NOT_FOUND)
	{
		return DelegateError(kFormat("nghttp3 rejected write offset: {}", ::nghttp3_strerror(ec)));
	}

	return 0;

} // OnQuicWriteOffset

//-----------------------------------------------------------------------------
void Session::OnQuicStreamBlocked(KQuicConnection::StreamID id)
//-----------------------------------------------------------------------------
{
	// flow control: tell nghttp3 to stop generating data for this stream until
	// the peer raises the limit (OnQuicStreamUnblocked)
	auto Stream = GetStream(id);

	if (Stream)
	{
		Stream->Block();
	}
	else
	{
		::nghttp3_conn_block_stream(m_Session, id);
	}

} // OnQuicStreamBlocked

//-----------------------------------------------------------------------------
void Session::OnQuicStreamShutWrite(KQuicConnection::StreamID id)
//-----------------------------------------------------------------------------
{
	// our sending side is gone, nghttp3 must not offer data for it anymore
	::nghttp3_conn_shutdown_stream_write(m_Session, id);

} // OnQuicStreamShutWrite


namespace {

KStringView RCBufToView(nghttp3_rcbuf* buf)
{
	auto bbuf = nghttp3_rcbuf_get_buf(buf);
	return KStringView(reinterpret_cast<char*>(bbuf.base), bbuf.len);
}

} // end of anonymous namespace

int Session::on_recv_header(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	int32_t token,
	nghttp3_rcbuf* name, nghttp3_rcbuf* value,
	uint8_t flags,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnReceiveHeader(stream_id, RCBufToView(name), RCBufToView(value), flags);
}

int Session::on_end_headers(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	int fin,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnEndHeaders(stream_id, fin);
}

int Session::on_recv_data(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	const uint8_t* data, size_t datalen,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnReceiveData(stream_id, KStringView(reinterpret_cast<const char*>(data), datalen));
}

int Session::on_end_stream(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnEndStream(stream_id);
}

int Session::on_stream_close(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	uint64_t app_error_code,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnStreamClose(stream_id, app_error_code);
}

int Session::on_stop_sending(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	uint64_t app_error_code,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnStopSending(stream_id, app_error_code);
}

int Session::on_reset_stream(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	uint64_t app_error_code,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnResetStream(stream_id, app_error_code);
}

int Session::on_deferred_consume(
	nghttp3_conn* h3conn,
	Stream::ID stream_id,
	size_t consumed,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnDeferredConsume(stream_id, consumed);
}

nghttp3_ssize Session::on_data_source_read(
	nghttp3_conn* conn,
	Stream::ID stream_id,
	nghttp3_vec* vec, size_t veccnt,
	uint32_t* pflags,
	void* conn_user_data,
	void* stream_user_data
)
{
	KStringView sBuffer;
	auto ec = ToThis(conn_user_data)->OnDataSourceRead(stream_id, sBuffer, veccnt, pflags);

	if (ec > 0)
	{
		if (DEKAF2_UNLIKELY(ec > 1))
		{
			kDebug(1, "can only return 1 or 0 in OnDataSourceRead()");
			ec = 1;
		}
		if (DEKAF2_UNLIKELY(ec > static_cast<nghttp3_ssize>(veccnt)))
		{
			kDebug(1, "veccnt is zero");
			ec = NGHTTP3_ERR_CALLBACK_FAILURE;
		}
		else
		{
			vec->base = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(sBuffer.data()));
			vec->len  = sBuffer.size();
		}
	}

	return ec;
}

int Session::on_acked_stream_data(
	nghttp3_conn* conn,
	Stream::ID stream_id,
	uint64_t datalen,
	void* conn_user_data,
	void* stream_user_data
)
{
	return ToThis(conn_user_data)->OnAckedStreamData(stream_id, datalen);
}

} // end of namespace khttp3

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGHTTP3
