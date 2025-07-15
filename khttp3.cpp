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

#include "khttp3.h"

#if DEKAF2_HAS_NGHTTP3 && DEKAF2_HAS_OPENSSL_QUIC

#include "kformat.h"
#include "kread.h"
#include "kwrite.h"
#include "kpoll.h"
#include <nghttp3/nghttp3.h>

DEKAF2_NAMESPACE_BEGIN

namespace khttp3
{

//-----------------------------------------------------------------------------
uint64_t SSL_Poll(uint64_t what, ::SSL* ssl)
//-----------------------------------------------------------------------------
{
	SSL_POLL_ITEM poll;
	poll.desc   = SSL_as_poll_descriptor(ssl);
	poll.events = what;

	constexpr struct timeval timeout = []
	{
		struct timeval tv;
		tv.tv_sec  = 0;
		tv.tv_usec = 0;
		return tv;
	}();

	size_t iResults = 0;

	if (::SSL_poll(
				   &poll,
				   1,
				   sizeof(SSL_POLL_ITEM),
				   &timeout,
				   0,
				   &iResults
				   ))
	{
		return poll.revents;
	}

	kDebug(4, "SSL_poll failed, iResults = {}, revents = {}", iResults, poll.revents);

	return 0;

} // Poll

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
	uint64_t iFlags = SSL_STREAM_FLAG_ADVANCE;

	if (type != Type::Request)
	{
		iFlags |= SSL_STREAM_FLAG_UNI;
	}

	m_QuicStream = KUniquePtr<::SSL, ::SSL_free>(::SSL_new_stream(m_Session.GetQuicConnection(), iFlags));

	if (!m_QuicStream)
	{
		SetError("could not create QUIC stream object");
	}
	else
	{
		m_StreamID = ::SSL_get_stream_id(m_QuicStream.get());
		kDebug(4, "[stream {}] created at fd {}", GetStreamID(), ::SSL_get_fd(m_QuicStream.get()));
	}

} // ctor

//-----------------------------------------------------------------------------
Stream::Stream(Session& session, ::SSL* QuicStream, Type type)
//-----------------------------------------------------------------------------
: m_Session(session)
, m_QuicStream(KUniquePtr<::SSL, SSL_free>(QuicStream))
, m_Type(type)
{
	if (!m_QuicStream)
	{
		SetError("QUIC stream object is invalid");
	}
	else
	{
		m_StreamID = ::SSL_get_stream_id(m_QuicStream.get());
		kDebug(4, "[stream {}] accepted at fd {}", GetStreamID(), ::SSL_get_fd(m_QuicStream.get()));
	}
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

		if (m_URI.Port != 0)
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
	if (m_QuicStream)
	{
		SSL_STREAM_RESET_ARGS args = {0};

		if (!::SSL_stream_reset(m_QuicStream.get(), &args, sizeof(args))) return 1;
	}

	return 0;

} // Reset

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
int Stream::AddData(KStringView sData)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] received {} bytes", GetStreamID(), sData.size());

	if (m_DataConsumer)
	{
		m_DataConsumer->Write(sData.data(), sData.size());
	}
	else
	{
		auto iConsumed  = m_RXBuffer.append(sData);
		auto iRemaining = sData.size() - iConsumed;

		if (iRemaining)
		{
			m_RXSpillBuffer.append(sData.data() + iConsumed, iRemaining);
		}
	}

	return 0;

} // AddData

//-----------------------------------------------------------------------------
void Stream::SetReceiveBuffer(KBuffer buffer)
//-----------------------------------------------------------------------------
{
	if (!m_RXSpillBuffer.empty())
	{
		auto iCopied = buffer.append(m_RXSpillBuffer);
		m_RXSpillBuffer.erase(0, iCopied);
	}

	m_RXBuffer = buffer;

} // SetReceiveBuffer

//-----------------------------------------------------------------------------
nghttp3_ssize Stream::ReceiveFromQuic(bool bOnce)
//-----------------------------------------------------------------------------
{
	DelWaitFor(WaitFor::Reads);

	for (;;)
	{
		if (IsClosed()
			|| !m_QuicStream /* If we already did STOP_SENDING, ignore this stream. */
			/* If this is a write-only stream, there is no read data to check. */
			|| ::SSL_get_stream_read_state(m_QuicStream.get()) == SSL_STREAM_STATE_WRONG_DIR)
		{
			return 0;
		}

		/*
		 * Pump data from OpenSSL QUIC to the HTTP/3 stack by calling SSL_read_ex
		 * to get received data and passing it to nghttp3 using
		 * nghttp3_conn_read_stream. Note that this function is confusingly
		 * named and inputs data to the HTTP/3 stack.
		 */
		if (m_BufferFromQuic.empty())
		{
			// need more data
			m_BufferFromQuic.reset();
			std::size_t num_bytes;
			auto ec = ::SSL_read_ex(m_QuicStream.get(), m_BufferFromQuic.data(), m_BufferFromQuic.remaining(), &num_bytes);

			if (ec <= 0)
			{
				num_bytes = 0;

				auto iError = ::SSL_get_error(m_QuicStream.get(), ec);

				if (iError == SSL_ERROR_WANT_READ)
				{
					// treat this as no error
					AddWaitFor(WaitFor::Reads);
				}
#if 0
				else if (iError == SSL_ERROR_WANT_WRITE)
				{
					// treat this as no error
					AddWaitFor(WaitFor::Writes);
				}
#endif
				else if (iError == SSL_ERROR_ZERO_RETURN)
				{
					m_bDoneReceivedFin = true;

					// Stream concluded normally. Pass FIN to HTTP/3 stack.
					ec = static_cast<int>(::nghttp3_conn_read_stream(m_Session.GetNGHTTP3_Session(), GetStreamID(), nullptr, 0, /*fin=*/1));

					if (ec < 0)
					{
						SetError(kFormat("cannot pass FIN to nghttp3: {}", ::nghttp3_strerror(ec)), ec);
						return ec;
					}

					kDebug(4, "[stream {}] stream finished", m_StreamID);
				}
				else if (::SSL_get_stream_read_state(m_QuicStream.get()) == SSL_STREAM_STATE_RESET_REMOTE)
				{
					// Stream was reset by peer
					uint64_t aec;

					if (!::SSL_get_stream_read_error_code(m_QuicStream.get(), &aec))
					{
						return -1;
					}

					ec = ::nghttp3_conn_close_stream(m_Session.GetNGHTTP3_Session(), GetStreamID(), aec);

					if (ec < 0)
					{
						SetError(kFormat("cannot mark stream as reset: {}", ::nghttp3_strerror(ec)), ec);
						return ec;
					}

					kDebug(4, "[stream {}] stream was reset (closed) by peer", m_StreamID);
					m_bDoneReceivedFin = true;
				}
				else
				{
					// Other error
					kDebug(4, "[stream {}] unknown error", m_StreamID);
					return -2;
				}
			}

			m_BufferFromQuic.resize(num_bytes);
		}

		if (m_BufferFromQuic.empty())
		{
			return 0;
		}

		kAssert(m_Session.GetConsumedAppData() == 0, "consumed app data > 0");

		/*
		 * This function is confusingly named as it is named from nghttp3's
		 * 'perspective'; it is used to pass data *into* the HTTP/3 stack which
		 * has been received from the network.
		 */
		auto ec2 = ::nghttp3_conn_read_stream(
			m_Session.GetNGHTTP3_Session(),
			GetStreamID(),
			m_BufferFromQuic.UInt8Data(), m_BufferFromQuic.size(),
			/*fin=*/0
		);

		if (ec2 < 0)
		{
			SetError(kFormat("nghttp3 failed to process incoming data: {}",
							nghttp3_strerror(static_cast<int>(ec2))), ec2);
			return ec2;
		}

		/*
		 * read_stream reports the data it consumes from us in two different
		 * ways; the non-application data is returned as a number of bytes 'ec'
		 * above, but the number of bytes of application data has to be recorded
		 * by our callback. We sum the two to determine the total number of
		 * bytes which nghttp3 consumed.
		 */
		std::size_t consumed = ec2 + m_Session.GetConsumedAppData();
		kAssert(consumed <= m_BufferFromQuic.size(), "consumed > buffer size");

		auto iConsumed = m_BufferFromQuic.consume(consumed);

		m_Session.ClearConsumedAppData();

		if (iConsumed != consumed)
		{
			SetError(kFormat("tried to consume more data than available: {}, {}", consumed, iConsumed));
			return -1;
		}

		if (bOnce && ec2 > 0)
		{
			kDebug(4, "[stream {}] returning with bOnce and read bytes to stream: {}", m_StreamID, ec2);
			return ec2;
		}
	}

} // ReceiveFromQuic

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
bool Stream::SendToQuic(const nghttp3_vec* vecs, std::size_t num_vecs, bool bFin)
//-----------------------------------------------------------------------------
{
	DelWaitFor(WaitFor::Writes);
	auto StreamID = GetStreamID();
	auto iTotalLen = ::nghttp3_vec_len(vecs, num_vecs);
	kDebug(4, "[stream {}] writing {} bytes", StreamID, iTotalLen);
	/*
	 * we let SSL_write_ex2(3) to conclude the stream for us (send FIN)
	 * after all data are written.
	 */
	uint64_t iFlags = bFin ? SSL_WRITE_FLAG_CONCLUDE : 0;

	std::size_t total_written { 0 };

	for (std::size_t i = 0; i < num_vecs; ++i)
	{
		if (vecs[i].len == 0)
		{
			continue;
		}

		std::size_t written { 0 };

retry:
		if (IsClosed())
		{
			/* Already did STOP_SENDING and threw away stream, ignore */
			written = vecs[i].len;
			kDebug(4, "[stream {}] already closed", StreamID);
		}
		else if (!::SSL_write_ex2(
			GetQuicStream(),
			vecs[i].base, vecs[i].len,
			iFlags,
			&written
		))
		{
			auto iError = ::SSL_get_error(GetQuicStream(), 0);

			if (iError == SSL_ERROR_WANT_WRITE)
			{
				// before blocking this stream, just poll if it became
				// available again (as this is a common case)
				if (Poll(SSL_POLL_EVENT_W) == SSL_POLL_EVENT_W)
				{
					// yes, retry sending the vector (typically
					// now from a different internal offset in
					// the Quic engine, if the buffer was large)
					goto retry;
				}

				// no, stream is really not ready to receive output
				written = 0;
				bFin = false; // we could not write the last frame
				/*
				 * We have filled our send buffer so tell nghttp3 to stop
				 * generating more data; we have to do this explicitly.
				 */
				Block();
			}
			else
			{
				return SetError(kFormat("writing HTTP/3 data to network failed, error: {}", iError));
			}
		}
		else
		{
			/*
			 * Tell nghttp3 it can resume generating more data in case we
			 * previously called block_stream.
			 */

			// actually we never get here when in blocked state because we
			// would never get vectors from nghttp3_conn_writev_stream()
			// as long as the stream is in blocked state..
			Unblock();
		}

		total_written += written;

		if (written > 0)
		{
			/*
			 * Tell nghttp3 we have consumed the data it output when we
			 * called writev_stream, otherwise subsequent calls to
			 * writev_stream will output the same data.
			 */
			auto ec = ::nghttp3_conn_add_write_offset(m_Session.GetNGHTTP3_Session(), StreamID, written);

			if (ec < 0)
			{
				return false;
			}

			/*
			 * Tell nghttp3 it can free the buffered data because we will
			 * not need it again. In our case we can always do this right
			 * away because we copy the data into our QUIC send buffers
			 * rather than simply storing a reference to it.
			 */
			ec = ::nghttp3_conn_add_ack_offset(m_Session.GetNGHTTP3_Session(), StreamID, written);

			if (ec < 0)
			{
				return false;
			}
		}
	}

	if (bFin && total_written == iTotalLen)
	{
		if (iTotalLen == 0)
		{
			/*
			 * As a special case, if nghttp3 requested to write a
			 * zero-length stream with a FIN, we have to tell it we did this
			 * by calling add_write_offset(0).
			 */
			auto ec = ::nghttp3_conn_add_write_offset(m_Session.GetNGHTTP3_Session(), StreamID, 0);

			if (ec < 0)
			{
				return false;
			}
		}
	}

	return true;

} // SendToQuic

//-----------------------------------------------------------------------------
Session::Session(KQuicStream& QuicConnection, bool bIsClient)
//-----------------------------------------------------------------------------
: m_KQuickStream(QuicConnection)
, m_SSL(QuicConnection.GetNativeTLSHandle())
{
	/*
	 * We use the QUIC stack in non-blocking mode so that we can react to
	 * incoming data on different streams, and e.g. incoming streams initiated
	 * by a server, as and when events occur.
	 */

	// this sets the native handle to non-blocking. it has probably
	// already been done in the connection creation, as the OpenSSL QUIC
	// implementation requires it. Our KQuicStream class always uses the
	// same BIO for read and write, therefore it is sufficient to only
	// operate on the rbio here.
	::BIO_set_nbio(::SSL_get_rbio(GetQuicConnection()), 1);

	// this sets non-blocking mode to the QUIC stack. The default for
	// new connections is blocking, so it is imperative to switch the mode
	if (!  ::SSL_set_blocking_mode(GetQuicConnection(), 0)
		|| ::SSL_get_blocking_mode(GetQuicConnection()))
	{
		SetError("cannot switch to non-blocking mode");
		return;
	}

	/*
	 * Disable default stream mode and create all streams explicitly. Each QUIC
	 * stream will be represented by its own QUIC stream SSL object (QSSO). This
	 * also automatically enables us to accept incoming streams (see
	 * SSL_set_incoming_stream_policy(3)).
	 */
	if (!::SSL_set_default_stream_mode(GetQuicConnection(), SSL_DEFAULT_STREAM_MODE_NONE))
	{
		SetError("failed to configure default stream mode");
		return;
	}

	/*
	 * HTTP/3 requires a couple of unidirectional management streams: a control
	 * stream and some QPACK state management streams for each side of a
	 * connection. These are the instances on our side (with us sending); the
	 * server will also create its own equivalent unidirectional streams on its
	 * side, which we handle subsequently as they come in (see SSL_accept_stream
	 * in the event handling code below).
	 */
	auto ControlStreamID  = CreateStream(Stream::Type::Control    );
	if (ControlStreamID  < 0) return; // error is already set
	auto QPackEncStreamID = CreateStream(Stream::Type::QPackEncode);
	if (QPackEncStreamID < 0) return; // error is already set
	auto QPackDecStreamID = CreateStream(Stream::Type::QPackDecode);
	if (QPackDecStreamID < 0) return; // error is already set

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
		SetError(kFormat("cannot create nghttp3 connection: {}", ::nghttp3_strerror(ec), ec));
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
	ec = ::nghttp3_conn_bind_control_stream(m_Session, ControlStreamID);

	if (ec < 0)
	{
		SetError(kFormat("cannot bind nghttp3 control stream: {}", ::nghttp3_strerror(ec)), ec);
		return;
	}

	ec = ::nghttp3_conn_bind_qpack_streams(m_Session, QPackEncStreamID, QPackDecStreamID);

	if (ec < 0)
	{
		SetError(kFormat("cannot bind nghttp3 QPACK streams: {}", ::nghttp3_strerror(ec)), ec);
		return;
	}

} // ctor

//-----------------------------------------------------------------------------
Session::~Session()
//-----------------------------------------------------------------------------
{
	m_Streams.clear();

	if (m_Session)
	{
		::nghttp3_conn_del(m_Session);
	}

} // dtor

//-----------------------------------------------------------------------------
bool Session::HandleEvents(bool bWithResponses)
//-----------------------------------------------------------------------------
{
	/*
	 * We handle events by doing six things:
	 *
	 * 1. Call SSL_handle_events() to allow network timing
	 * 2. Handle new incoming streams
	 * 3. Pump outgoing data from the HTTP/3 stack to the QUIC engine
	 * 4. Pump incoming data from the QUIC engine to the HTTP/3 stack
	 * 5. Remove all Stream objects that can be deleted at this point in time
	 * 6. Check if there are Stream objects in blocked state that can be sent
	 *    to again
	 */

	for (;;)
	{
		// 1. SSL_handle_events
		::SSL_handle_events(GetQuicConnection());

		// 2. Check for new incoming streams
		for (;;)
		{
			auto Stream = ::SSL_accept_stream(GetQuicConnection(), SSL_ACCEPT_STREAM_NO_BLOCK);

			if (!Stream)
			{
				break;
			}

			// add the new stream into our stream map
			if (!AcceptStream(Stream))
			{
				return false;
			}
		}

		// 3. Pump outgoing data from HTTP/3 engine to QUIC
		for (;;)
		{
			std::array<nghttp3_vec, 8> vecs;
			Stream::ID StreamID;
			int fin;

			/*
			 * Get a number of send vectors from the HTTP/3 engine.
			 *
			 * Note that this function is confusingly named as it is named from
			 * nghttp3's 'perspective': this outputs pointers to data which nghttp3
			 * wants to *write* to the network.
			 */
			auto ec = ::nghttp3_conn_writev_stream(m_Session, &StreamID, &fin, vecs.data(), vecs.size());

			if (ec < 0)
			{
				return SetError("internal error");
			}

			if (ec == 0)
			{
				break;
			}

			auto Stream = GetStream(StreamID);

			if (!Stream)
			{
				return SetError(kFormat("no stream for ID {}", StreamID));
			}

			if (!Stream->SendToQuic(vecs.data(), ec, fin))
			{
				return SetError("send to quick failed");
			}
		}

		// 4. Pump incoming data from QUIC to HTTP/3 engine
		for (auto& Stream : m_Streams)
		{
			if (bWithResponses
				|| Stream.second->GetType() != Stream::Type::Request
				|| Stream.second->IsHeadersComplete() == false)
			{
				if (Stream.second->ReceiveFromQuic(/*bOnce*/false) < 0)
				{
					return SetError("receive from quick failed");
				}
			}
		}

		// 5. Check for Stream objects to remove from our store,
		// and 6. for streams that were in blocked state, but now
		// are good for writing again
		bool bStopLooping { true };

		for (auto it = m_Streams.begin(); it != m_Streams.end();)
		{
			if (it->second->CanDelete())
			{
				kDebug(4, "[stream {}] will be purged", it->first);
				it = m_Streams.erase(it);
			}
			else
			{
				if (it->second->IsBlocked())
				{
					if (it->second->Poll(SSL_POLL_EVENT_W) & SSL_POLL_EVENT_W)
					{
						// we can write more data!
						it->second->Unblock();
						bStopLooping = false;
					}
				}

				++it;
			}
		}

		if (bStopLooping)
		{
			// only return if we did not unblock at least one 
			// stream in this iteration
			break;
		}
	}

	return true;

} // HandleEvents

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
bool Session::IsReadReady(KDuration Timeout)
//-----------------------------------------------------------------------------
{
	// use SSL_Poll first, it does not (yet) have a timeout though
	auto iSSLPoll = SSL_Poll(POLLIN, GetQuicConnection());

	if (iSSLPoll > 0)
	{
		// yes, have session data waiting
		return true;
	}

	if (::SSL_pending(GetQuicConnection()) > 0)
	{
		return true;
	}

	// until SSL_poll() will offer a timeout version we have to use
	// the native ::poll to check for arriving input data
	auto iPoll = kPoll(GetKQuicStream().GetNativeSocket(), POLLIN, Timeout);

	if (iPoll == 0)
	{
		// timed out, no events
		return false;
	}
	else if (iPoll < 0)
	{
		return SetErrnoError("error during poll: ");
	}

	// data available
	return true;

} // IsReadReady

//-----------------------------------------------------------------------------
bool Session::Run(bool bWithResponses)
//-----------------------------------------------------------------------------
{
	// event loop with timing
	for (;;)
	{
		if (!HandleEvents(bWithResponses))
		{
			if (!HasError()) SetError("cannot handle events");
			return false;
		}

		if (!HaveOpenRequestStreams(bWithResponses))
		{
			return true;
		}

		struct timeval tv;
		int is_infinite;

		if (!::SSL_get_event_timeout(GetQuicConnection(), &tv, &is_infinite))
		{
			if (!HasError()) SetError("cannot get SSL timeouts");
			return false;
		}

		auto Timeout = GetTimeout();
		bool bCloseWithTimeout { true };

		if (!is_infinite)
		{
			auto tNext = chrono::seconds(tv.tv_sec) + chrono::microseconds(tv.tv_usec);

			if (tNext < Timeout)
			{
				bCloseWithTimeout = false;
				Timeout = tNext;
			}
		}

		if (!IsReadReady(Timeout))
		{
			if (bCloseWithTimeout)
			{
				kDebug(1, "connection timed out");
				return false;
			}
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
Stream::ID Session::CreateStream(Stream::Type type)
//-----------------------------------------------------------------------------
{
	return NewStream(std::make_unique<Stream>(*this, type));
}

//-----------------------------------------------------------------------------
Stream::ID Session::AcceptStream(::SSL* QuicStream)
//-----------------------------------------------------------------------------
{
	return NewStream(std::make_unique<Stream>(*this, QuicStream, Stream::Type::Incoming));
}

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
		return SetError("cannot add stream {} to session", StreamID);
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

	auto QuicStream = Stream->GetQuicStream();

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
		QuicStream
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

	Stream->SetReceiveBuffer( { data, len } );

	std::streamsize iRead = Stream->GetReceiveBuffer().size();
	// check if we have already enough bytes in the receive buffer..

	if (len > static_cast<std::size_t>(iRead))
	{
		// no, get more
		for(;;)
		{
			if (Stream->IsClosed())
			{
				return iRead;
			}

			::SSL_handle_events(GetQuicConnection());

			if (Stream->ReceiveFromQuic(/*bOnce*/true) < 0)
			{
				return 0;
			}

			iRead = Stream->GetReceiveBuffer().size();

			if (iRead > 0)
			{
				break;
			}

			if (!IsReadReady(GetTimeout()))
			{
				kDebug(1, "connection timed out");
				return iRead;
			}
		}
	}

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
//	kDebug(4, "[stream {}] receive data: {} bytes", StreamID, sData.size());
	AddConsumedAppData(sData.size());

	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		return Stream->AddData(sData);
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
	// TODO check if this should rather be -=
	AddConsumedAppData(iConsumed);

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
