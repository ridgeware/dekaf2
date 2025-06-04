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

#include "khttp2.h"

#if DEKAF2_HAS_NGHTTP2

#include "kformat.h"
#include "kwrite.h"

#define NGHTTP2_NO_SSIZE_T 1
#include <nghttp2/nghttp2.h>

#if (NGHTTP2_VERSION_NUM < 0x013d00)
	// Unfortunately, just when implementing nghttp2 support in dekaf2,
	// the authors decided to change the interface to avoid an issue
	// with ::ssize_t on IBM mainframes. They changed ALL function
	// names that use ::ssize_t ..
	// So currently on MacOS with homebrew, the new function names are
	// to be used, and on Linux the old (because the distributions are
	// all still at v1.59 or less).
	#define DEKAF2_OLD_NGHTTP2_VERSION 1
#endif

#if !DEKAF2_OLD_NGHTTP2_VERSION
static_assert(std::is_same<nghttp2_ssize, dekaf2::khttp2::nghttp2_ssize>::value, "nghttp2_ssize != ptrdiff_t");
#endif

DEKAF2_NAMESPACE_BEGIN

namespace khttp2
{

//-----------------------------------------------------------------------------
Stream::Stream(KURL url, KHTTPMethod Method, KHTTPResponseHeaders& ResponseHeaders)
//-----------------------------------------------------------------------------
: m_ResponseHeaders(ResponseHeaders)
, m_URI(std::move(url))
, m_Method(Method)
{
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
void Stream::SetReceiveBuffer(KBuffer buffer)
//-----------------------------------------------------------------------------
{
	if (!m_RXSpillBuffer.empty())
	{
		auto iCopied = buffer.append(m_RXSpillBuffer.data(), m_RXSpillBuffer.size());
		m_RXSpillBuffer.erase(0, iCopied);
	}

	m_RXBuffer = buffer;

} // SetReceiveBuffer

//-----------------------------------------------------------------------------
void Stream::Receive(KConstBuffer data)
//-----------------------------------------------------------------------------
{
	if (m_DataConsumer)
	{
		m_DataConsumer->Write(data.data(), data.size());
	}
	else
	{
		auto iConsumed  = m_RXBuffer.append(data);
		auto iRemaining = data.size() - iConsumed;

		if (iRemaining)
		{
			m_RXSpillBuffer.append(data.CharData() + iConsumed, iRemaining);
		}
	}

} // Receive

//-----------------------------------------------------------------------------
void Stream::AddResponseHeader(ID id, KStringView sName, KStringView sValue)
//-----------------------------------------------------------------------------
{
	if (!IsHeadersComplete())
	{
		if (sName == ":status")
		{
			kDebug(2, "[stream {}] setting HTTP response status to {}", id, sValue.UInt16());
			m_ResponseHeaders.SetStatus(sValue.UInt16());
			m_ResponseHeaders.SetHTTPVersion(KHTTPVersion::http2);
		}
		else
		{
			kDebug(2, "[stream {}] {}: {}", id, sName, sValue);
			m_ResponseHeaders.Headers.Add(sName, sValue);
		}
	}

} // AddResponseHeader

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
nghttp2_ssize Session::OnReceiveCallback(
	nghttp2_session* session,
	uint8_t* buf, size_t length,
	int flags,
	void* user_data
)
//-----------------------------------------------------------------------------
{
	return ToThis(user_data)->OnReceive(KBuffer(buf, length), flags);
}

//-----------------------------------------------------------------------------
nghttp2_ssize Session::OnSendCallback(
	nghttp2_session* session,
	const uint8_t* data, size_t length,
	int flags,
	void* user_data
)
//-----------------------------------------------------------------------------
{
	return ToThis(user_data)->OnSend(KConstBuffer(data, length), flags);
}

//-----------------------------------------------------------------------------
int Session::OnHeaderCallback(
	nghttp2_session* session,
	const void* frame,
	const uint8_t* name, size_t namelen,
	const uint8_t* value, size_t valuelen,
	uint8_t flags,
	void* user_data
)
//-----------------------------------------------------------------------------
{
	return ToThis(user_data)->OnHeader(frame, ToView(name, namelen), ToView(value, valuelen), flags);
}

//-----------------------------------------------------------------------------
int Session::OnBeginHeadersCallback(
	nghttp2_session* session,
	const void* frame,
	void* user_data
//-----------------------------------------------------------------------------
)
{
	return ToThis(user_data)->OnBeginHeaders(frame);
}

//-----------------------------------------------------------------------------
int Session::OnFrameRecvCallback(
	nghttp2_session* session,
	const void* frame,
	void *user_data
)
//-----------------------------------------------------------------------------
{
	return ToThis(user_data)->OnFrameRecv(frame);
}

//-----------------------------------------------------------------------------
int Session::OnDataChunkRecvCallback(
	nghttp2_session* session,
	uint8_t flags, Stream::ID stream_id,
	const uint8_t* data, size_t len,
	void* user_data
)
//-----------------------------------------------------------------------------
{
	return ToThis(user_data)->OnDataChunkRecv(flags, stream_id, KConstBuffer(data, len));
}

//-----------------------------------------------------------------------------
int Session::OnStreamCloseCallback(
	nghttp2_session* session,
	Stream::ID stream_id, uint32_t error_code,
	void* user_data
)
//-----------------------------------------------------------------------------
{
	return ToThis(user_data)->OnStreamClose(stream_id, error_code);
}

//-----------------------------------------------------------------------------
nghttp2_ssize Session::OnDataSourceReadCallback(
	nghttp2_session *session,
	Stream::ID stream_id,
	uint8_t* buf, size_t length,
	uint32_t* data_flags, void* source, // nghttp2_data_source*
	void *user_data
)
//-----------------------------------------------------------------------------
{
	if (!source) return NGHTTP2_ERR_CALLBACK_FAILURE;

	KDataProvider* Data = static_cast<KDataProvider*>(static_cast<nghttp2_data_source*>(source)->ptr);

	*data_flags = NGHTTP2_DATA_FLAG_NONE;

	std::size_t iRead { 0 };

	if (Data->NoCopy())
	{
		*data_flags |= NGHTTP2_DATA_FLAG_NO_COPY;

		iRead = Data->CalcNextReadSize(length);

		if (Data->IsEOFAfter(iRead))
		{
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		}

	}
	else
	{
		iRead = Data->Read(buf, length);

		if (Data->IsEOF())
		{
			*data_flags |= NGHTTP2_DATA_FLAG_EOF;
		}
	}

	kDebug(4, "[stream {}] read {} bytes from data source{}", stream_id, iRead,
		   (*data_flags & NGHTTP2_DATA_FLAG_EOF) ? ", EOF reached" : ""
	);

	return static_cast<nghttp2_ssize>(iRead);

} // OnDataSourceReadCallback

//-----------------------------------------------------------------------------
int Session::OnSendDataCallback(
	nghttp2_session* session,
	void* frame,
	const uint8_t* framehd, size_t length,
	void* source,
	void* user_data
)
//-----------------------------------------------------------------------------
{
	if (!source) return NGHTTP2_ERR_CALLBACK_FAILURE;
	KDataProvider* Data = static_cast<KDataProvider*>(static_cast<nghttp2_data_source*>(source)->ptr);
	return ToThis(user_data)->OnSendData(frame, framehd, length, *Data);

} // OnSendDataCallback

//-----------------------------------------------------------------------------
Session::Session(KTLSStream& TLSStream, bool bIsClient)
//-----------------------------------------------------------------------------
: m_TLSStream(TLSStream)
{
	nghttp2_session_callbacks* callbacks;
	nghttp2_session_callbacks_new                             (&callbacks);
#if DEKAF2_OLD_NGHTTP2_VERSION
	nghttp2_session_callbacks_set_send_callback               (callbacks, OnSendCallback         );
	nghttp2_session_callbacks_set_recv_callback               (callbacks, OnReceiveCallback      );
#else
	nghttp2_session_callbacks_set_send_callback2              (callbacks, OnSendCallback         );
	nghttp2_session_callbacks_set_recv_callback2              (callbacks, OnReceiveCallback      );
#endif
	nghttp2_session_callbacks_set_send_data_callback          (callbacks, reinterpret_cast<nghttp2_send_data_callback>       (OnSendDataCallback    ));
	nghttp2_session_callbacks_set_on_frame_recv_callback      (callbacks, reinterpret_cast<nghttp2_on_frame_recv_callback>   (OnFrameRecvCallback   ));
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback (callbacks, OnDataChunkRecvCallback);
	nghttp2_session_callbacks_set_on_stream_close_callback    (callbacks, OnStreamCloseCallback  );
	nghttp2_session_callbacks_set_on_header_callback          (callbacks, reinterpret_cast<nghttp2_on_header_callback>       (OnHeaderCallback      ));
	nghttp2_session_callbacks_set_on_begin_headers_callback   (callbacks, reinterpret_cast<nghttp2_on_begin_headers_callback>(OnBeginHeadersCallback));
	nghttp2_session_client_new                                (&m_Session, callbacks, this       );
	nghttp2_session_callbacks_del                             (callbacks);

	if (bIsClient)
	{
		SendClientConnectionHeader();
	}
}

//-----------------------------------------------------------------------------
Session::~Session()
//-----------------------------------------------------------------------------
{
	if (m_Session)
	{
		nghttp2_session_terminate_session(m_Session, NGHTTP2_NO_ERROR);
		nghttp2_session_del(m_Session);
	}

} // dtor

//-----------------------------------------------------------------------------
KStringView Session::TranslateError(int iError)
//-----------------------------------------------------------------------------
{
	switch (iError)
	{
		case NGHTTP2_ERR_NOMEM:                   return "out of memory";
		case NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE: return "maximum stream ID reached";
		case NGHTTP2_ERR_INVALID_ARGUMENT:        return "invalid argument";
		case NGHTTP2_ERR_PROTO:                   return "server session cannot send request";
		case NGHTTP2_ERR_CALLBACK_FAILURE:        return "callback reported failure";
		default:                                  return "unknown";
	}

	return "";

} // TranslateError

//-----------------------------------------------------------------------------
KStringView Session::TranslateFrameType(uint8_t FrameType)
//-----------------------------------------------------------------------------
{
	switch (FrameType)
	{
		case NGHTTP2_DATA:            return "Data";
		case NGHTTP2_HEADERS:         return "Headers";
		case NGHTTP2_PRIORITY:        return "Priority";
		case NGHTTP2_RST_STREAM:      return "RstStream";
		case NGHTTP2_SETTINGS:        return "Settings";
		case NGHTTP2_PUSH_PROMISE:    return "PushPromise";
		case NGHTTP2_PING:            return "Ping";
		case NGHTTP2_GOAWAY:          return "GoAway";
		case NGHTTP2_WINDOW_UPDATE:   return "WindowUpdate";
		case NGHTTP2_CONTINUATION:    return "Continuation";
		case NGHTTP2_ALTSVC:          return "AltSvc";
#if !DEKAF2_OLD_NGHTTP2_VERSION
		case NGHTTP2_ORIGIN:          return "Origin";
		case NGHTTP2_PRIORITY_UPDATE: return "PriorityUpdate";
#endif
		default:                      return "Unknown";
	}

	return "";

} // TranslateFrameType

//-----------------------------------------------------------------------------
Stream* Session::GetStream(Stream::ID StreamID)
//-----------------------------------------------------------------------------
{
	auto it = m_Streams.find(StreamID);

	if (it == m_Streams.end())
	{
		SetError(kFormat("cannot find stream ID {}", StreamID));
		return nullptr;
	}

	return &it->second;

} // GetStream

//-----------------------------------------------------------------------------
bool Session::AddStream(Stream::ID StreamID, Stream Stream)
//-----------------------------------------------------------------------------
{
	return m_Streams.emplace(StreamID, std::move(Stream)).second;

} // AddStream

//-----------------------------------------------------------------------------
bool Session::CloseStream(Stream::ID StreamID)
//-----------------------------------------------------------------------------
{
	auto Stream = GetStream(StreamID);

	if (Stream)
	{
		Stream->Close();
		return true;
	}

	return false;

} // CloseStream

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
nghttp2_ssize Session::OnReceive (KBuffer data, int flags)
//-----------------------------------------------------------------------------
{
	auto iRead = m_TLSStream.direct_read_some(data.CharData(), data.capacity());
	data.resize(iRead);
	kDebug(4, "direct TLS read: {}", iRead);
	return iRead;

} // OnReceive

//-----------------------------------------------------------------------------
nghttp2_ssize Session::OnSend (KConstBuffer data, int flags)
//-----------------------------------------------------------------------------
{
	kDebug(4, "sending data frame of size {} to TLS", data.size());

	auto iWrote = kWrite(m_TLSStream, data.CharData(), data.size());

	if (m_TLSStream.bad() || iWrote != data.size())
	{
		SetError("write error");
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	return iWrote;

} // OnSend

//-----------------------------------------------------------------------------
int Session::OnSendData (void* frame, const uint8_t* framehd, size_t length, KDataProvider& source)
//-----------------------------------------------------------------------------
{
	nghttp2_frame* Frame = static_cast<nghttp2_frame*>(frame);

	if (kWrite(m_TLSStream, framehd, 9) != 9)
	{
		SetError("write error");
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	if (Frame->data.padlen > 0)
	{
		kDebug(4, "[stream {}] padlen {}", Frame->hd.stream_id, Frame->data.padlen);
		char ch = char(Frame->data.padlen - 1);
		kWrite(m_TLSStream, ch);
	}

	kDebug(4, "[stream {}] sending data frame of size {}", Frame->hd.stream_id, length);

	if (source.Read(m_TLSStream, length) != static_cast<std::size_t>(length))
	{
		SetError("write error");
		return NGHTTP2_ERR_CALLBACK_FAILURE;
	}

	if (Frame->data.padlen > 1)
	{
		kDebug(4, "[stream {}] send padding of size {}", Frame->hd.stream_id, Frame->data.padlen - 1);

		std::array<char, 100> zeroes;
		std::size_t iPad = Frame->data.padlen - 1;

		for (;iPad > 0;)
		{
			auto iHave = std::min(iPad, zeroes.size());
			
			if (kWrite(m_TLSStream, zeroes.data(), iHave) != iHave)
			{
				SetError("write error");
				return NGHTTP2_ERR_CALLBACK_FAILURE;
			}

			iPad -= iHave;
		}
	}

	return 0;

} // OnSendData

//-----------------------------------------------------------------------------
int Session::OnHeader (const void* frame, KStringView sName, KStringView sValue, uint8_t flags)
//-----------------------------------------------------------------------------
{
	auto Frame = static_cast<const nghttp2_frame*>(frame);

	if (Frame->hd.type == NGHTTP2_HEADERS && Frame->headers.cat == NGHTTP2_HCAT_RESPONSE)
	{
		auto Stream = GetStream(Frame->hd.stream_id);

		if (Stream)
		{
			Stream->AddResponseHeader(Frame->hd.stream_id, sName, sValue);
		}
		else
		{
			SetError(kFormat("[stream {}] got headers for invalid stream id", Frame->hd.stream_id));
			return NGHTTP2_PROTOCOL_ERROR;
		}
	}

	return NGHTTP2_NO_ERROR;

} // OnHeader

//-----------------------------------------------------------------------------
int Session::OnBeginHeaders (const void* frame)
//-----------------------------------------------------------------------------
{
	auto Frame = static_cast<const nghttp2_frame*>(frame);

	if (Frame->hd.type == NGHTTP2_HEADERS && Frame->headers.cat == NGHTTP2_HCAT_RESPONSE)
	{
		kDebug(4, "[stream {}] response header start", Frame->hd.stream_id);
	}

	return NGHTTP2_NO_ERROR;

} // OnBeginHeaders

//-----------------------------------------------------------------------------
int Session::OnFrameRecv (const void* frame)
//-----------------------------------------------------------------------------
{
	auto Frame = static_cast<const nghttp2_frame*>(frame);

	kDebug(4, "[stream {}] frame type: {}, category {}", Frame->hd.stream_id, TranslateFrameType(Frame->hd.type), Frame->headers.cat);

	if (Frame->hd.type == NGHTTP2_HEADERS && Frame->headers.cat == NGHTTP2_HCAT_RESPONSE)
	{
		kDebug(4, "[stream {}] all headers received", Frame->hd.stream_id);

		auto Stream = GetStream(Frame->hd.stream_id);

		if (Stream)
		{
			kDebug(4, "[stream {}] setting headers complete", Frame->hd.stream_id);
			Stream->SetHeadersComplete();
		}
	}
	else if (Frame->hd.flags & NGHTTP2_FLAG_END_STREAM)
	{
		kDebug(4, "[stream {}] last frame of stream", Frame->hd.stream_id);
	}

	return NGHTTP2_NO_ERROR;

} // OnFrameRecv

//-----------------------------------------------------------------------------
int Session::OnDataChunkRecv (uint8_t flags, Stream::ID stream_id, KConstBuffer data)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] received {} bytes", stream_id, data.size());

	auto Stream = GetStream(stream_id);

	if (Stream)
	{
		Stream->Receive(data);
	}
	else
	{
		SetError(kFormat("[stream {}] not found - dropping {} RX bytes", stream_id, data.size()));
		return NGHTTP2_PROTOCOL_ERROR;
	}

	return NGHTTP2_NO_ERROR;

} // OnDataChunkRecv

//-----------------------------------------------------------------------------
int Session::OnStreamClose (Stream::ID stream_id, uint32_t error_code)
//-----------------------------------------------------------------------------
{
	kDebug(4, "[stream {}] closed with error_code={}", stream_id, error_code);

	if (!CloseStream(stream_id))
	{
		return NGHTTP2_PROTOCOL_ERROR;
	}

	return NGHTTP2_NO_ERROR;

} // OnStreamClose

//-----------------------------------------------------------------------------
bool Session::SendClientConnectionHeader()
//-----------------------------------------------------------------------------
{
	nghttp2_settings_entry iv[1] = {{ NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100 }};

	/* client 24 bytes magic string will be sent by nghttp2 library */
	int iReturn = nghttp2_submit_settings(m_Session, NGHTTP2_FLAG_NONE, iv, (sizeof(iv) / sizeof(iv[0])));

	if (iReturn != 0)
	{
		return SetError(kFormat("Could not submit settings: {}", nghttp2_strerror(iReturn)));
	}

	return true;

} // SendClientConnectionHeader

//-----------------------------------------------------------------------------
Stream::ID Session::NewRequest (Stream Stream,
                                const KHTTPHeaders& RequestHeaders,
                                std::unique_ptr<KDataProvider> SendData)
//-----------------------------------------------------------------------------
{
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// convert from KHTTPHeaders into a nghttp2_nv struct
	struct http2header : public nghttp2_nv
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		http2header(KStringView sName, KStringView sValue, bool bNoCopyName = true, bool bNoCopyValue = true)
		{
			name     = reinterpret_cast<uint8_t*>(const_cast<char*>(sName .data()));
			value    = reinterpret_cast<uint8_t*>(const_cast<char*>(sValue.data()));
			namelen  = sName .size();
			valuelen = sValue.size();
			flags    = bNoCopyName  ? NGHTTP2_NV_FLAG_NO_COPY_NAME  : NGHTTP2_NV_FLAG_NONE;
			flags   |= bNoCopyValue ? NGHTTP2_NV_FLAG_NO_COPY_VALUE : NGHTTP2_NV_FLAG_NONE;
		}

	}; // http2header

	// we prefer to get the authority from the host header
	KStringView sAuthority = RequestHeaders.Headers.Get(KHTTPHeader::HOST);

	if (sAuthority.empty())
	{
		sAuthority = Stream.GetAuthority();
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

	std::vector<nghttp2_nv> Headers;
	Headers.reserve(RequestHeaders.Headers.size() + 4);

	Headers.push_back(http2header { ":method"    , Stream.GetMethod    () });
	Headers.push_back(http2header { ":scheme"    , Stream.GetScheme    () });
	Headers.push_back(http2header { ":authority" , m_sAuthority           });
	// !! copy the path - if it is < 15 chars it would move out of scope when Stream is moved!
	Headers.push_back(http2header { ":path"      , Stream.GetPath      (), true, false });

	// add all 'normal' headers (with lowercase names)
	for (const auto& Header : RequestHeaders.Headers)
	{
		// drop non-http/2 headers
		if (DEKAF2_UNLIKELY(
			Header.first == KHTTPHeader::CONNECTION        ||
			Header.first == KHTTPHeader::KEEP_ALIVE        ||
			Header.first == KHTTPHeader::PROXY_CONNECTION  ||
			Header.first == KHTTPHeader::TRANSFER_ENCODING ||
			Header.first == KHTTPHeader::UPGRADE))
		{
			// log all dropped headers, except the dropped HOST header
			// - we pushed it into the :authority pseudo header
			kDebug(2, "dropping non-HTTP/2 header: {}: {}", Header.first, Header.second);
		}
		else if (Header.first != KHTTPHeader::HOST)
		{
			Headers.push_back(http2header(Header.first.Serialize(), Header.second));
		}
	}

	// this struct will be COPIED with nghttp2_submit_request2
#if DEKAF2_OLD_NGHTTP2_VERSION
	nghttp2_data_provider  Data;
#else
	nghttp2_data_provider2 Data;
#endif

	if (SendData && !SendData->IsEOF())
	{
#if DEKAF2_OLD_NGHTTP2_VERSION
		Data.read_callback = reinterpret_cast<nghttp2_data_source_read_callback >(OnDataSourceReadCallback);
#else
		Data.read_callback = reinterpret_cast<nghttp2_data_source_read_callback2>(OnDataSourceReadCallback);
#endif
		Data.source.ptr    = SendData.get();
	}
	else
	{
		Data.source.ptr    = nullptr;
	}

	// submit the request
#if DEKAF2_OLD_NGHTTP2_VERSION
	auto StreamID = nghttp2_submit_request (
#else
	auto StreamID = nghttp2_submit_request2(
#endif
											m_Session,
											nullptr,
											Headers.data(), Headers.size(),
											Data.source.ptr ? &Data : nullptr,
											nullptr);

	if (kWouldLog(2))
	{
		// output all headers for debugging
		for (const auto& Header : Headers)
		{
			kDebug(2, "[stream {}] {}: {}", StreamID, ToView(Header.name, Header.namelen), ToView(Header.value, Header.valuelen));
		}
	}

	if (Data.source.ptr)
	{
		kDebug(4, "[stream {}] we have request data to send", StreamID);
	}

	if (StreamID < 0)
	{
		SetError(kFormat("[stream {}] Could not submit HTTP request: {}", StreamID, nghttp2_strerror(StreamID)));
		return -1;
	}

	Stream.SetDataProvider(std::move(SendData));

	// add the Stream to our local map for opened http2 streams
	if (!AddStream(StreamID, std::move(Stream)))
	{
		SetError(kFormat("[stream {}] already exists", StreamID));
		return -1;
	}

	return StreamID;

} // NewRequest

//-----------------------------------------------------------------------------
Stream::ID Session::NewStream(KURL                           url,
                              KHTTPMethod                    Method,
                              const KHTTPHeaders&            RequestHeaders,
                              std::unique_ptr<KDataProvider> SendData,
                              KHTTPResponseHeaders&          ResponseHeaders,
                              std::unique_ptr<KDataConsumer> ReceiveData)
//-----------------------------------------------------------------------------
{
	// construct this stream's data record
	Stream Stream(std::move(url), Method, ResponseHeaders);

	Stream.SetDataConsumer(std::move(ReceiveData));

	auto StreamID = NewRequest(std::move(Stream), RequestHeaders, std::move(SendData));

	if (StreamID < 0)
	{
		return -1;
	}

	return StreamID;

} // NewStream

//-----------------------------------------------------------------------------
bool Session::SessionSend()
//-----------------------------------------------------------------------------
{
	auto iResult = nghttp2_session_send(m_Session);

	if (iResult)
	{
		return SetError(nghttp2_strerror(iResult));
	}

	m_TLSStream.flush();

	if (!m_TLSStream.good())
	{
		SetError("TLS stream bad");
		return false;
	}

	return true;

} // SessionSend

//-----------------------------------------------------------------------------
bool Session::SessionReceive()
//-----------------------------------------------------------------------------
{
	auto iResult = nghttp2_session_recv(m_Session);

	if (iResult)
	{
		return SetError(nghttp2_strerror(iResult));
	}

	return true;

} // SessionSend

//-----------------------------------------------------------------------------
bool Session::Received(const void* data, std::size_t len)
//-----------------------------------------------------------------------------
{
#if DEKAF2_OLD_NGHTTP2_VERSION
	auto readlen = nghttp2_session_mem_recv (m_Session, static_cast<const uint8_t*>(data), len);
#else
	auto readlen = nghttp2_session_mem_recv2(m_Session, static_cast<const uint8_t*>(data), len);
#endif

	if (readlen < 0)
	{
		return SetError(nghttp2_strerror(static_cast<int>(readlen)));
	}

	if (!SessionSend())
	{
		return false;
	}

	return true;

} // Receive

//-----------------------------------------------------------------------------
bool Session::Run()
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		if (m_Streams.empty())
		{
			return false;
		}
		
		if (!SessionSend())
		{
			return false;
		}

		if (!SessionReceive())
		{
			return false;
		}
	}

	return true;

} // Run

//-----------------------------------------------------------------------------
Stream::ID SingleStreamSession::SubmitRequest(KURL                           url,
                                              KHTTPMethod                    Method,
                                              const KHTTPHeaders&            RequestHeaders,
                                              std::unique_ptr<KDataProvider> SendData,
                                              KHTTPResponseHeaders&          ResponseHeaders)
//-----------------------------------------------------------------------------
{
	// construct this stream's data record
	Stream Stream(std::move(url), Method, ResponseHeaders);

	auto StreamID = NewRequest(std::move(Stream), RequestHeaders, std::move(SendData));

	if (StreamID < 0)
	{
		return -1;
	}

	// finally send all the prepared request data
	if (!SessionSend())
	{
		return -1;
	}

	if (!m_TLSStream.good())
	{
		SetError("TLS stream bad");
		return -1;
	}

	// and receive the response headers
	ReadResponseHeaders(StreamID);

	return StreamID;

} // SubmitRequest

//-----------------------------------------------------------------------------
bool SingleStreamSession::ReadResponseHeaders(Stream::ID StreamID)
//-----------------------------------------------------------------------------
{
	auto Stream = GetStream(StreamID);

	if (!Stream)
	{
		return false;
	}

	std::array<char, Stream::BufferSize> TLSBuffer;

	for (;;)
	{
		if (Stream->IsHeadersComplete())
		{
			return true;
		}

		auto iRead = m_TLSStream.direct_read_some(TLSBuffer.data(), TLSBuffer.size());
		kDebug(4, "[stream {}] direct TLS header read: {}", StreamID, iRead);

		if (iRead <= 0)
		{
			return SetError(kFormat("TLS header read returned {}", iRead));
		}

		if (iRead > 0)
		{
			Received(TLSBuffer.data(), iRead);
		}

	}

	return Stream->IsHeadersComplete();

} // ReadResponseHeaders

//-----------------------------------------------------------------------------
std::streamsize SingleStreamSession::ReadData(Stream::ID StreamID, void* data, std::size_t len)
//-----------------------------------------------------------------------------
{
	if (!len)
	{
		return 0;
	}

	auto Stream = GetStream(StreamID);

	if (!Stream)
	{
		SetError(kFormat("[stream {}] not found! - cannot read {} bytes", StreamID, len));
		return -1;
	}

	if (!Stream->IsHeadersComplete() || !data)
	{
		return -1;
	}

	Stream->SetReceiveBuffer( { data, len } );

	// TODO change this to KReservedBuffer !
	std::array<char, Stream::BufferSize> TLSBuffer;

	nghttp2_ssize iRead { 0 };

	for (;;)
	{
		if (Stream->IsClosed())
		{
			iRead = Stream->GetReceiveBuffer().size();

			if (Stream->HasRXBuffered())
			{
				kDebug(4, "[stream {}] closed, but have still RX data buffered - will not yet delete", StreamID);
			}
			else
			{
				kDebug(4, "[stream {}] closed, no more RX data - will delete and exit", StreamID, iRead);
				DeleteStream(StreamID);
				break;
			}
		}

		if (Stream->GetReceiveBuffer().remaining() == 0)
		{
			iRead = Stream->GetReceiveBuffer().size();
			break;
		}

		auto iReadTLS = m_TLSStream.direct_read_some(TLSBuffer.data(), TLSBuffer.size());
		kDebug(4, "[stream {}] direct TLS data read: {}", StreamID, iReadTLS);

		if (iReadTLS <= 0)
		{
			return SetError(kFormat("TLS data read returned {}", iReadTLS));
			break;
		}

		if (iReadTLS > 0)
		{
			Received(TLSBuffer.data(), iReadTLS);
		}
	}

	if (len != static_cast<std::size_t>(iRead))
	{
		kDebug(4, "[stream {}] requested {}, got {} bytes", StreamID, len, iRead);
	}

	return iRead;

} // ReadData

} // end of namespace khttp2

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGHTTP2
