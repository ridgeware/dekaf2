
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

#include "kcgistream.h"
#include "klog.h"
#include "ksystem.h"
#include "khttp_header.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
std::streamsize KCGIIStream::StreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	if (stream_)
	{
		auto stream   = static_cast<KCGIIStream::Stream*>(stream_);
		char* sOutBuf = static_cast<char*>(sBuffer);
		auto iRemain  = iCount;

		if (!stream->sHeader.empty())
		{
			// copy from the header buffer into the stream buffer
			auto iCopy = std::min(static_cast<KStringView::size_type>(iRemain), stream->sHeader.size());
			std::memcpy(sOutBuf, stream->sHeader.data(), iCopy);
			stream->sHeader.remove_prefix(iCopy);
			iRemain -= iCopy;

			if (!iRemain)
			{
				return iCount;
			}

			// adjust output pointer
			sOutBuf += iCopy;
		}

		if (!stream->chCommentDelimiter)
		{
			stream->istream->read(sOutBuf, iRemain);
			return stream->istream->gcount();
		}
		else
		{
			while (iRemain > 0)
			{
				stream->istream->getline(sOutBuf, iRemain);
				auto iRead = stream->istream->gcount();
				if (!iRead)
				{
					// this is eof
					return iCount - iRemain;
				}

				if (*sOutBuf != stream->chCommentDelimiter)
				{
					// valid line..
					iRemain -= iRead;
					sOutBuf += iRead;
					sOutBuf[-1] = '\n';
				}
			}

			return iCount;
		}
	}

	return 0;

} // StreamReader

//-----------------------------------------------------------------------------
bool KCGIIStream::CreateHeader()
//-----------------------------------------------------------------------------
{
	m_Stream.sHeader.clear();

	KString sMethod = kGetEnv(KCGIIStream::REQUEST_METHOD);

	if (sMethod.empty())
	{
		kDebugLog (1, "KCGIIStream: we are not running within a web server...");
		return false;
	}

	kDebugLog (1, "KCGI: we are running within a web server that sets the CGI variables...");

	// add method, resource and protocol from env vars
	m_sHeader = sMethod;
	m_sHeader += ' ';
	m_sHeader += kGetEnv(KCGIIStream::REQUEST_URI);
	m_sHeader += ' ';
	m_sHeader += kGetEnv(KCGIIStream::SERVER_PROTOCOL);
	m_sHeader += "\r\n";

	struct CGIVars_t { KStringViewZ sVar; KStringViewZ sHeader; };
	static constexpr CGIVars_t CGIVars[]
	{
		{ KCGIIStream::HTTP_HOST,      KHTTPHeaders::HOST            },
		{ KCGIIStream::CONTENT_TYPE,   KHTTPHeaders::CONTENT_TYPE    },
		{ KCGIIStream::CONTENT_LENGTH, KHTTPHeaders::CONTENT_LENGTH  },
		{ KCGIIStream::REMOTE_ADDR,    KHTTPHeaders::X_FORWARDED_FOR }
	};

	// add headers from env vars
	for (const auto it : CGIVars)
	{
		m_sHeader += it.sHeader;
		m_sHeader += ": ";
		m_sHeader += kGetEnv(it.sVar);
		m_sHeader += "\r\n";
	}

	// final end of header line
	m_sHeader += "\r\n";

	// point string view into the constructed header
	m_Stream.sHeader = m_sHeader;

	return true;

} // CreateHeader

//-----------------------------------------------------------------------------
KCGIIStream::KCGIIStream(std::istream& istream)
//-----------------------------------------------------------------------------
    : base_type(&m_StreamBuf)
{
	m_Stream.istream = &istream;

	CreateHeader();

} // ctor


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringViewZ KCGIIStream::AUTH_PASSWORD;
constexpr KStringViewZ KCGIIStream::AUTH_TYPE;
constexpr KStringViewZ KCGIIStream::AUTH_USER;
constexpr KStringViewZ KCGIIStream::CERT_COOKIE;
constexpr KStringViewZ KCGIIStream::CERT_FLAGS;
constexpr KStringViewZ KCGIIStream::CERT_ISSUER;
constexpr KStringViewZ KCGIIStream::CERT_KEYSIZE;
constexpr KStringViewZ KCGIIStream::CERT_SECRETKEYSIZE;
constexpr KStringViewZ KCGIIStream::CERT_SERIALNUMBER;
constexpr KStringViewZ KCGIIStream::CERT_SERVER_ISSUER;
constexpr KStringViewZ KCGIIStream::CERT_SERVER_SUBJECT;
constexpr KStringViewZ KCGIIStream::CERT_SUBJECT;
constexpr KStringViewZ KCGIIStream::CF_TEMPLATE_PATH;
constexpr KStringViewZ KCGIIStream::CONTENT_LENGTH;
constexpr KStringViewZ KCGIIStream::CONTENT_TYPE;
constexpr KStringViewZ KCGIIStream::CONTEXT_PATH;
constexpr KStringViewZ KCGIIStream::GATEWAY_INTERFACE;
constexpr KStringViewZ KCGIIStream::HTTPS;
constexpr KStringViewZ KCGIIStream::HTTPS_KEYSIZE;
constexpr KStringViewZ KCGIIStream::HTTPS_SECRETKEYSIZE;
constexpr KStringViewZ KCGIIStream::HTTPS_SERVER_ISSUER;
constexpr KStringViewZ KCGIIStream::HTTPS_SERVER_SUBJECT;
constexpr KStringViewZ KCGIIStream::HTTP_ACCEPT;
constexpr KStringViewZ KCGIIStream::HTTP_ACCEPT_ENCODING;
constexpr KStringViewZ KCGIIStream::HTTP_ACCEPT_LANGUAGE;
constexpr KStringViewZ KCGIIStream::HTTP_CONNECTION;
constexpr KStringViewZ KCGIIStream::HTTP_COOKIE;
constexpr KStringViewZ KCGIIStream::HTTP_HOST;
constexpr KStringViewZ KCGIIStream::HTTP_REFERER;
constexpr KStringViewZ KCGIIStream::HTTP_USER_AGENT;
constexpr KStringViewZ KCGIIStream::QUERY_STRING;
constexpr KStringViewZ KCGIIStream::REMOTE_ADDR;
constexpr KStringViewZ KCGIIStream::REMOTE_HOST;
constexpr KStringViewZ KCGIIStream::REMOTE_USER;
constexpr KStringViewZ KCGIIStream::REQUEST_METHOD;
constexpr KStringViewZ KCGIIStream::REQUEST_URI;
constexpr KStringViewZ KCGIIStream::SCRIPT_NAME;
constexpr KStringViewZ KCGIIStream::SERVER_NAME;
constexpr KStringViewZ KCGIIStream::SERVER_PORT;
constexpr KStringViewZ KCGIIStream::SERVER_PORT_SECURE;
constexpr KStringViewZ KCGIIStream::SERVER_PROTOCOL;
constexpr KStringViewZ KCGIIStream::SERVER_SOFTWARE;
constexpr KStringViewZ KCGIIStream::WEB_SERVER_API;

constexpr KStringViewZ KCGIIStream::FCGI_WEB_SERVER_ADDRS;

#endif
} // of namespace dekaf2

