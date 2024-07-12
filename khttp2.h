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

#pragma once

/// @file khttp2.h
/// wraps nghttp2 primitives in C++

#include "kdefinitions.h"

#if DEKAF2_HAS_NGHTTP2

#include "kurl.h"
#include "khttp_method.h"
#include "khttp_header.h"
#include "khttp_response.h"
#include "kstringview.h"
#include "kstring.h"
#include "ktlsstream.h"

#include <cstdint>
#include <unordered_map>
#include <iostream>

// forward declarations into nghttp2 types
struct nghttp2_session;

DEKAF2_NAMESPACE_BEGIN

namespace http2
{

using nghttp2_ssize = ::ssize_t;

class ConstBuffer;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Error class that keeps both an int16_t error and a error description as a string
class Error
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// default ctor
	Error() = default;
	/// construct with iError and sError
	Error(int16_t iError, KString sError = KString{});

	/// is any error set?
	bool            IsSet()       const { return m_Error.get(); }
	/// get the error description if an error is set
	KStringViewZ    GetString()   const { return IsSet() ? m_Error->sError.ToView() : KStringViewZ{}; }
	/// get the error int16_t value if an error is set
	int16_t         GetValue()    const { return IsSet() ? m_Error->iError : 0; }

	/// returns true if an error is set
	operator bool ()              const { return IsSet(); }

//----------
private:
//----------

	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	struct IntError
	//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		IntError() = default;
		IntError(int16_t iError, KString sError = KString{}) : iError(iError), sError(std::move(sError)) {}

		int16_t iError { 0 };
		KString sError;
	};

	std::unique_ptr<IntError> m_Error;

};

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffer class that points to writeable memory, and keeps track of capacity and current fill size.
/// We add conversions to easily interface between a C API and C++ objects
class Buffer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	Buffer () = default;
	/// construct writeable Buffer from data and capacity
	Buffer (void* data, size_type capacity) : m_Data(data), m_iCapacity(capacity) {}

	/// returns buffer as a void pointer
	void*       ToVoidPtr   ()                  { return m_Data;      }
	/// returns buffer as a char pointer
	char*       ToCharPtr   ()                  { return static_cast<char*>(m_Data);          }
	/// returns buffer as a uint8_t pointer
	uint8_t*    ToUInt8Ptr  ()                  { return static_cast<uint8_t*>(m_Data);       }
	/// returns buffer as a KStringView
	KStringView ToView      ()                  { return KStringView{ ToCharPtr(), size() };  }

	/// return the overall size of the buffer
	size_type   size        () const            { return m_iCapacity; }
	/// return true if the overall size is 0
	bool        empty       () const            { return !size();     }
	/// return the filled size of the buffer, which can be smaller than the overall size
	size_type   GetFill     () const            { return m_iFill;     }
	/// return the remaining size of the buffer, which is the difference between the overall size and the filled size
	size_type   Remaining   () const            { return size() - GetFill(); }
	/// append len bytes from a void ptr until buffer is full - returns count of copied bytes
	size_type   Append      (const void* data, size_type len);
	/// append from a ConstBuffer until buffer is full - returns count of copied bytes
	size_type   Append      (ConstBuffer buffer);
	/// append from a KStringView until buffer is full - returns count of copied bytes
	size_type   Append      (KStringView sData) { return Append(sData.data(), sData.size()); }
	/// resets Buffer to empty state
	void        clear       ();

	operator bool() const                       { return m_Data;      }

//----------
protected:
//----------

	void        SetFill     (size_type size)    { m_iFill = size;     }
	void        AddFill     (size_type size)    { m_iFill += size;    }

//----------
private:
//----------

	void*       m_Data      { nullptr };
	size_type   m_iCapacity { 0 };
	size_type   m_iFill     { 0 };

}; // Buffer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffer class that points to const memory and can be used for reading. Used to interface between
/// C API and C++ objects.
class ConstBuffer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	ConstBuffer () = default;
	/// construct readable Buffer from data and len
	ConstBuffer (const void* data, size_type len) : m_Data(data), m_iSize(len) {}
	/// construct readable Buffer from KStringView
	ConstBuffer (KStringView sData)               : m_Data(sData.data()), m_iSize(sData.size()) {}
	/// construct readable Buffer from (writeable) Buffer
	ConstBuffer (Buffer buffer)                   : m_Data(buffer.ToVoidPtr()), m_iSize(buffer.size()) {}

	/// returns buffer as a const void pointer
	const void*     ToVoidPtr  () const { return m_Data;  }
	/// returns buffer as a const char pointer
	const char*     ToCharPtr  () const { return static_cast<const char*>(m_Data);    }
	/// returns buffer as a const uint8_t pointer
	const uint8_t*  ToUInt8Ptr () const { return static_cast<const uint8_t*>(m_Data); }
	/// returns buffer as a KStringView
	KStringView     ToView     () const { return KStringView{ ToCharPtr(), size() };  }

	/// returns size of the buffer
	size_type       size       () const { return m_iSize; }

	operator bool() const               { return m_Data;  }

//----------
private:
//----------

	const void* m_Data  { nullptr };
	size_type   m_iSize { 0 };

}; // ConstBuffer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the interface of a data provider class
class DataProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	DataProvider () = default;
	virtual ~DataProvider ();

	/// if asked to provide size_t bytes, will be called if NoCopy() returns true, @return how many would really be provided
	virtual std::size_t CalcNextReadSize (std::size_t) const { return 0; }
	/// copy size_t bytes into buffer, will be called if NoCopy() returns false @return total copied bytes
	virtual std::size_t Read (void* buffer, std::size_t) = 0;
	/// write size_t bytes into ostream, will be called if NoCopy() returns true, avoiding copying through Read() @return total written bytes
	virtual std::size_t Send (std::ostream&, std::size_t) { return 0; }
	/// @return true if provider can send data without copying
	virtual bool NoCopy      () const = 0;
	/// @return true if provider is at EOF
	virtual bool IsEOF       () const = 0;
	/// @return true if provider would be at EOF after reading size_t bytes, will be called if NoCopy() returns true
	virtual bool IsEOFAfter  (std::size_t) const { return false; }

}; // DataProvider

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a data provider that reads from constant memory without own buffering (a view)
class ViewProvider : public DataProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	ViewProvider () = default;
	ViewProvider (KStringView sView) : m_sView(sView) {}
	ViewProvider (const void* data, std::size_t size) : ViewProvider(KStringView(static_cast<const char*>(data), size)) {}
	ViewProvider (ConstBuffer buffer) : ViewProvider(buffer.ToVoidPtr(), buffer.size()) {}

	/// if asked to provide size_t bytes, will be called if NoCopy() returns true, @return how many would really be provided
	virtual std::size_t CalcNextReadSize (std::size_t size) const override final { return std::min(size, m_sView.size()); }
	/// copy size_t bytes into buffer, will be called if NoCopy() returns false @return total copied bytes
	virtual std::size_t Read (void* buffer, std::size_t size) override final;
	/// write size_t bytes into ostream, will be called if NoCopy() returns true, avoiding copying through Read() @return total written bytes
	virtual std::size_t Send (std::ostream& ostream, std::size_t size) override final;

	/// @return true if provider can send data without copying
	virtual bool NoCopy      ()                 const override final { return true;                   }
	/// @return true if provider is at EOF
	virtual bool IsEOF       ()                 const override final { return m_sView.empty();        }
	/// @return true if provider would be at EOF after reading size_t bytes, will be called if NoCopy() returns true
	virtual bool IsEOFAfter  (std::size_t size) const override final { return m_sView.size() <= size; }

//----------
private:
//----------

	KStringView m_sView;

}; // ViewProvider

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a data provider that reads from buffered memory (a string)
class BufferedProvider : public ViewProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	BufferedProvider () = default;
	BufferedProvider (KString sData) : m_sData(std::move(sData)) { ViewProvider::operator=(KStringView(m_sData)); }
	BufferedProvider (const void* data, std::size_t size) : BufferedProvider(KString(static_cast<const char*>(data), size)) {}
	BufferedProvider (ConstBuffer buffer) : BufferedProvider(buffer.ToVoidPtr(), buffer.size()) {}

//----------
private:
//----------

	KString m_sData;

}; // BufferedProvider

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// a data provider that reads from any std::istream
class IStreamProvider : public DataProvider
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	IStreamProvider(std::istream& istream) : m_IStream(istream) {}

	/// copy size_t bytes into buffer, will be called if NoCopy() returns false @return total copied bytes
	virtual std::size_t Read(void* buffer, std::size_t size) override final;
	/// @return true if provider can send data without copying
	virtual bool NoCopy() const override final { return false;            }
	/// @return true if provider is at EOF
	virtual bool IsEOF () const override final { return m_IStream.eof();  }

//----------
private:
//----------

	std::istream& m_IStream;

}; // IStreamProvider

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// the interface of a data consumer class, taking incoming bytes
class DataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	DataConsumer() = default;
	virtual ~DataConsumer() {}

	/// write size_t bytes into buffer
	virtual std::size_t Write (const void* buffer , std::size_t size) = 0;
	/// marking consumer as completed / finished (all data received)
	void                SetFinished ();
	/// returns true if consumer is finished
	bool                IsFinished  () const { return m_bFinished; }
	/// returns error, if any
	const Error&        GetError    () const { return m_Error;     }

//----------
protected:
//----------

	virtual void        CallFinishedCallback() = 0;
	bool                SetError(Error error) { m_Error = std::move(error); return false; }

//----------
private:
//----------

	Error m_Error;
	bool  m_bFinished { false };

}; // DataConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into a view (external buffer)
class ViewConsumer : public DataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// construct around data and capacity
	ViewConsumer(void* data, std::size_t capacity) : m_Buffer(data, capacity) {}
	/// Construct around data and capacity, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	ViewConsumer(void* data, std::size_t capacity, std::function<void(ViewConsumer&)> callback) : m_Callback(std::move(callback)), m_Buffer(data, capacity) {}
	/// write size_t bytes into view
	virtual std::size_t Write (const void* buffer , std::size_t size) override final;
	/// get total size written into view
	std::size_t size() const { return m_Buffer.GetFill(); }

//----------
protected:
//----------

	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------
	
	std::function<void(ViewConsumer&)> m_Callback { nullptr };
	Buffer m_Buffer;

}; // ViewConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into a KString reference
class StringConsumer : public DataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around a string buffer
	StringConsumer(KString& sBuffer) : m_sBuffer(sBuffer) {}
	/// Construct around a string buffer, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	StringConsumer(KString& sBuffer, std::function<void(StringConsumer&)> callback) : m_Callback(std::move(callback)), m_sBuffer(sBuffer) {}
	/// write size_t bytes into string
	virtual std::size_t Write (const void* buffer , std::size_t) override final;
	/// return reference for the string buffer
	KString& GetData() { return m_sBuffer; }

//----------
protected:
//----------

	virtual void        CallFinishedCallback() override;

//----------
private:
//----------

	std::function<void(StringConsumer&)> m_Callback { nullptr };
	KString& m_sBuffer;

}; // StringConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into an internal buffer (a string)
class BufferedConsumer : public StringConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around an internal string buffer
	BufferedConsumer() : StringConsumer(m_sBuffer) {}
	/// Construct around an internal string buffer, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	BufferedConsumer(std::function<void(BufferedConsumer&)> callback) : StringConsumer(m_sBuffer), m_Callback(std::move(callback)) {}

//----------
protected:
//----------

	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------

	std::function<void(BufferedConsumer&)> m_Callback { nullptr };
	KString m_sBuffer;

}; // BufferedConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing into a std::ostream
class OStreamConsumer : public DataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around a std::ostream
	OStreamConsumer(std::ostream& ostream) : m_OStream(ostream) {}
	/// Construct around a std::ostream, and a callback function that is called when all bytes for the request
	/// have been received. The callback should return without blocking!
	OStreamConsumer(std::ostream& ostream, std::function<void(OStreamConsumer&)> callback) : m_Callback(std::move(callback)), m_OStream(ostream) {}
	/// write size_t bytes into ostream
	virtual std::size_t Write (const void* buffer , std::size_t) override final;

//----------
protected:
//----------

	std::function<void(OStreamConsumer&)> m_Callback { nullptr };
	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------

	std::ostream& m_OStream;

}; // OstreamConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// data consumer class writing through a generic callback
class CallbackConsumer : public DataConsumer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// Construct around a write callback, the callback should return without blocking!
	/// If size is 0 end of file is reached (and the void ptr then points to this class instance)
	CallbackConsumer(std::function<std::size_t(const void*, std::size_t)> callback) : m_WriteCallback(callback) {}
	/// write size_t bytes into callback
	virtual std::size_t Write (const void* buffer , std::size_t) override final;

//----------
protected:
//----------

	std::function<void(CallbackConsumer&)> m_Callback { nullptr };
	virtual void        CallFinishedCallback() override final;

//----------
private:
//----------

	std::function<std::size_t(const void*, std::size_t)> m_WriteCallback;

}; // CallbackConsumer

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Stream describes one request/response stream in a HTTP/2 Session (of which HTTP/2 can have many)
class Stream
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	static constexpr std::size_t BufferSize = 2048;

	/// construct around StreamID, url and ResponseHeaders
	Stream(int32_t StreamID, KURL url, KHTTPMethod Method, KHTTPResponseHeaders& ResponseHeaders);
	/// construct around url and ResponseHeaders - add the StreamID later
	Stream(KURL url, KHTTPMethod Method, KHTTPResponseHeaders& ResponseHeaders) : Stream(-1, std::move(url), Method, ResponseHeaders) {}

	/// set the stream ID after submitting a request
	void          SetStreamID        (int32_t id)     { m_iStreamID = id;   }
	/// get the stream ID
	int32_t       GetStreamID        () const         { return m_iStreamID; }

	/// set the data provider for this Stream's request (if any)
	void          SetDataProvider    (std::unique_ptr<DataProvider> Data) { m_DataProvider = std::move(Data); }
	/// set the data consumer for this Stream's response (if any)
	void          SetDataConsumer    (std::unique_ptr<DataConsumer> Data) { m_DataConsumer = std::move(Data); }

	/// returns the URI's scheme component (https, ..)
	KStringView   GetScheme          ();
	/// returns the domain name, plus the port if not the default port of the scheme
	KStringView   GetAuthority       ();
	/// returns the path and query componend, path prefixed with / (and existing if otherwise empty), query
	/// prefixed with ? if existing
	KStringView   GetPath            ();
	/// returns the Method
	KStringView   GetMethod          () const         { return m_Method.Serialize(); }

	/// adds an incoming response header
	void          AddResponseHeader  (KStringView sName, KStringView sValue);
	/// called once all headers are received
	void          SetHeadersComplete ()               { m_bHeadersComplete = true; }
	/// returns state of all headers received
	bool          IsHeadersComplete  () const         { return m_bHeadersComplete; }
	/// set the buffer that will be filled by the next receive operation
	void          SetReceiveBuffer   (Buffer buffer);
	/// get the receive buffer (to lookup fill and remaining bytes)
	const Buffer& GetReceiveBuffer   () const         { return m_RXBuffer;         }
	/// the stream was closed, set the IsClosed flag, and if set, call the DataConsumer callback
	void          Close              ();
	/// return the close flag
	bool          IsClosed           () const         { return m_bIsClosed;        }
	/// add incoming data to this stream - this is a method normally called by the Session class upon reception
	/// of a data chunk. It is written into the ReceiveBuffer (if existing), and any overflowing data gets stored in
	/// a temporary FIFO buffer (which gets later emptied when a new receive buffer is set)
	void          Receive            (ConstBuffer data);

//----------
private:
//----------

	std::unique_ptr<DataProvider> m_DataProvider;
	std::unique_ptr<DataConsumer> m_DataConsumer;
	KHTTPResponseHeaders& m_ResponseHeaders;
	KString               m_RXSpillBuffer;
	Buffer                m_RXBuffer;
	KURL                  m_URI;
	KString               m_sAuthority;
	KString               m_sPath;
	int32_t               m_iStreamID        { -1    };
	KHTTPMethod           m_Method;
	bool                  m_bHeadersComplete { false };
	bool                  m_bIsClosed        { false };

}; // Stream

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A Session describes one HTTP/2 session, which is the equivalent of one TCP connection.
/// It can have many Streams.
class Session
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// construct a HTTP/2 session around a connected TLS stream (after successful ALPN H2 negotiation)
	Session(KTLSIOStream& TLSStream, bool bIsClient);
	~Session();

	/// Submit a (client) request. Internally, this generates a new Stream object, the handle of which is returned as an int32_t.
	/// This method is _not_ blocking and returns immediately. It can be called multiple times with different requests and
	/// DataConsumer objects for the same authority. Once a DataConsumer object is received, its completion callback is
	/// called.
	int32_t NewStream  (KURL                          url,
						KHTTPMethod                   Method,
						const KHTTPHeaders&           RequestHeaders,
						std::unique_ptr<DataProvider> SendData,
						KHTTPResponseHeaders&         ResponseHeaders,
						std::unique_ptr<DataConsumer> ReceiveData);

	/// after adding streams, run the data pump here until Run() returns false
	bool Run();

	/// return the last error, if any
	const KString& Error () const { return m_sError; }

//----------
protected:
//----------

	int32_t NewRequest (Stream Stream,
						const KHTTPHeaders& RequestHeaders,
						std::unique_ptr<DataProvider> SendData);

	bool    SendClientConnectionHeader ();
	bool    SessionSend         ();
	bool    SessionReceive      ();
	bool    Received            (const void* data, std::size_t len);

	bool    AddStream           (Stream Stream);
	Stream* GetStream           (int32_t StreamID);
	bool    CloseStream         (int32_t StreamID);
	bool    DeleteStream        (int32_t StreamID);

	bool    SetError            (KString sError);

	static KStringView TranslateError(int iError);
	static KStringView TranslateFrameType (uint8_t FrameType);

	KTLSIOStream&    m_TLSStream;

//----------
private:
//----------

	static KStringView ToView(const uint8_t* value, size_t len)
	{
		return { reinterpret_cast<const char*>(value), len };
	}

	nghttp2_ssize OnReceive       (Buffer data, int flags);
	nghttp2_ssize OnSend          (ConstBuffer data, int flags);
	int           OnSendData      (void* frame, const uint8_t* framehd, size_t length, DataProvider& source);
	int           OnHeader        (const void* frame, KStringView sName, KStringView sValue, uint8_t flags);
	int           OnBeginHeaders  (const void* frame);
	int           OnFrameRecv     (const void* frame);
	int           OnDataChunkRecv (uint8_t flags, int32_t stream_id, ConstBuffer data);
	int           OnStreamClose   (int32_t stream_id, uint32_t error_code);

	nghttp2_session* m_Session { nullptr };
	KString          m_sError;
	std::unordered_map<int32_t, Stream> m_Streams;
	KString          m_sAuthority; // the common authority for all Streams managed by this Session

	// -------------------------------------------------------------------
	// the static C style callback functions which translate from C to C++
	// -------------------------------------------------------------------

	static Session* ToThis(void* user_data) 
	{
		return static_cast<Session*>(user_data);
	}

	static nghttp2_ssize OnReceiveCallback(
		nghttp2_session* session,
		uint8_t* buf, size_t length,
		int flags,
		void* user_data
	);

	static nghttp2_ssize OnSendCallback(
		nghttp2_session* session,
		const uint8_t* data, size_t length,
		int flags,
		void* user_data
	);

	static int OnHeaderCallback(
		nghttp2_session* session,
		const void* frame,
		const uint8_t* name, size_t namelen,
		const uint8_t* value, size_t valuelen,
		uint8_t flags,
		void* user_data
	);

	static int OnBeginHeadersCallback(
		nghttp2_session* session,
		const void* frame,
		void* user_data
	);

	static int OnFrameRecvCallback(
		nghttp2_session* session,
		const void* frame,
		void *user_data
	);

	static int OnDataChunkRecvCallback(
		nghttp2_session* session,
		uint8_t flags, int32_t stream_id,
		const uint8_t* data, size_t len,
		void* user_data
	);

	static nghttp2_ssize OnDataSourceReadCallback(
		nghttp2_session *session,
		int32_t stream_id,
		uint8_t* buf, size_t length,
		uint32_t* data_flags, void* source,
		void *user_data
	);

	static int OnSendDataCallback(
		nghttp2_session* session,
		void* frame,
		const uint8_t* framehd, size_t length,
		void* source,
		void* user_data
	);

	static int OnStreamCloseCallback(
		nghttp2_session* session,
		int32_t stream_id, uint32_t error_code,
		void* user_data
	);

}; // Session

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A special type of session in which only one single stream at any time is permitted (multiple streams
/// can be generated consecutively). We use this type of session to adapt HTTP2 to dekaf2's std::iostream
/// KHTTPClient.
class SingleStreamSession : protected Session
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using Session::Session;
	using Session::Error;

	/// Submit a (client) request. Internally, this generates a new Stream object, the handle of which is returned as an int32_t.
	/// This method only returns after having sent request headers and body, and received all response headers.
	int32_t SubmitRequest (KURL                          url,
						   KHTTPMethod                   Method,
						   const KHTTPHeaders&           RequestHeaders,
						   std::unique_ptr<DataProvider> SendData,
						   KHTTPResponseHeaders&         ResponseHeaders);

	/// read more decoded data from a response stream identified by StreamID and store it into the provided buffer - you
	/// need to call this method when you use the SubmitRequest() method without DataConsumer objects. Only one
	/// single stream can be handled in this mode.
	std::streamsize ReadData (int32_t StreamID, void* data, std::size_t len);

//----------
protected:
//----------

	bool    ReadResponseHeaders (int32_t StreamID);

}; // SingleStreamSession

} // end of namespace http2

DEKAF2_NAMESPACE_END

#endif // of DEKAF2_HAS_NGHTTP2
