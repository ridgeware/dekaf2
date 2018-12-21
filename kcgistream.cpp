
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
std::streamsize KCGIInStream::StreamReader(void* sBuffer, std::streamsize iCount, void* stream_)
//-----------------------------------------------------------------------------
{
	// we do not need to loop the reader, as the streambuf requests bytes in blocks
	// and calls underflow() if more are expected

	if (stream_)
	{
		auto stream   = static_cast<KCGIInStream::Stream*>(stream_);
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
					// replace the 0 at the end of the buffer with the delimiter
					sOutBuf[-1] = '\n';
				}
			}

			return iCount;
		}
	}

	return 0;

} // StreamReader

//-----------------------------------------------------------------------------
bool KCGIInStream::CreateHeader()
//-----------------------------------------------------------------------------
{
	m_Stream.sHeader.clear();

	KString sMethod = kGetEnv(KCGIInStream::REQUEST_METHOD);

	if (sMethod.empty())
	{
		kDebugLog (1, "KCGIIStream: we are not running within a web server...");
		// permitting for comment lines
		m_Stream.chCommentDelimiter = '#';
		return false;
	}

	kDebugLog (1, "KCGI: we are running within a web server that sets the CGI variables...");

	// add method, resource and protocol from env vars
	m_sHeader = sMethod;
	m_sHeader += ' ';
	m_sHeader += kGetEnv(KCGIInStream::REQUEST_URI);
	m_sHeader += ' ';
	m_sHeader += kGetEnv(KCGIInStream::SERVER_PROTOCOL);
	m_sHeader += "\r\n";

	struct CGIVars_t { KStringViewZ sVar; KStringViewZ sHeader; };
	static constexpr CGIVars_t CGIVars[]
	{
		{ KCGIInStream::HTTP_HOST,      KHTTPHeaders::HOST            },
		{ KCGIInStream::CONTENT_TYPE,   KHTTPHeaders::CONTENT_TYPE    },
		{ KCGIInStream::CONTENT_LENGTH, KHTTPHeaders::CONTENT_LENGTH  },
		{ KCGIInStream::REMOTE_ADDR,    KHTTPHeaders::X_FORWARDED_FOR }
	};

	// add headers from env vars
	for (const auto it : CGIVars)
	{
		KString sEnv = kGetEnv(it.sVar);
		if (!sEnv.empty())
		{
			m_sHeader += it.sHeader;
			m_sHeader += ": ";
			m_sHeader += sEnv;
			m_sHeader += "\r\n";
		}
	}

	// final end of header line
	m_sHeader += "\r\n";

	// point string view into the constructed header
	m_Stream.sHeader = m_sHeader;

	return true;

} // CreateHeader

//-----------------------------------------------------------------------------
KCGIInStream::KCGIInStream(std::istream& istream)
//-----------------------------------------------------------------------------
    : base_type(&m_StreamBuf)
{
	m_Stream.istream = &istream;

	CreateHeader();

} // ctor


#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringViewZ KCGIInStream::AUTH_PASSWORD;
constexpr KStringViewZ KCGIInStream::AUTH_TYPE;
constexpr KStringViewZ KCGIInStream::AUTH_USER;
constexpr KStringViewZ KCGIInStream::CERT_COOKIE;
constexpr KStringViewZ KCGIInStream::CERT_FLAGS;
constexpr KStringViewZ KCGIInStream::CERT_ISSUER;
constexpr KStringViewZ KCGIInStream::CERT_KEYSIZE;
constexpr KStringViewZ KCGIInStream::CERT_SECRETKEYSIZE;
constexpr KStringViewZ KCGIInStream::CERT_SERIALNUMBER;
constexpr KStringViewZ KCGIInStream::CERT_SERVER_ISSUER;
constexpr KStringViewZ KCGIInStream::CERT_SERVER_SUBJECT;
constexpr KStringViewZ KCGIInStream::CERT_SUBJECT;
constexpr KStringViewZ KCGIInStream::CF_TEMPLATE_PATH;
constexpr KStringViewZ KCGIInStream::CONTENT_LENGTH;
constexpr KStringViewZ KCGIInStream::CONTENT_TYPE;
constexpr KStringViewZ KCGIInStream::CONTEXT_PATH;
constexpr KStringViewZ KCGIInStream::GATEWAY_INTERFACE;
constexpr KStringViewZ KCGIInStream::HTTPS;
constexpr KStringViewZ KCGIInStream::HTTPS_KEYSIZE;
constexpr KStringViewZ KCGIInStream::HTTPS_SECRETKEYSIZE;
constexpr KStringViewZ KCGIInStream::HTTPS_SERVER_ISSUER;
constexpr KStringViewZ KCGIInStream::HTTPS_SERVER_SUBJECT;
constexpr KStringViewZ KCGIInStream::HTTP_ACCEPT;
constexpr KStringViewZ KCGIInStream::HTTP_ACCEPT_ENCODING;
constexpr KStringViewZ KCGIInStream::HTTP_ACCEPT_LANGUAGE;
constexpr KStringViewZ KCGIInStream::HTTP_CONNECTION;
constexpr KStringViewZ KCGIInStream::HTTP_COOKIE;
constexpr KStringViewZ KCGIInStream::HTTP_HOST;
constexpr KStringViewZ KCGIInStream::HTTP_REFERER;
constexpr KStringViewZ KCGIInStream::HTTP_USER_AGENT;
constexpr KStringViewZ KCGIInStream::QUERY_STRING;
constexpr KStringViewZ KCGIInStream::REMOTE_ADDR;
constexpr KStringViewZ KCGIInStream::REMOTE_HOST;
constexpr KStringViewZ KCGIInStream::REMOTE_USER;
constexpr KStringViewZ KCGIInStream::REQUEST_METHOD;
constexpr KStringViewZ KCGIInStream::REQUEST_URI;
constexpr KStringViewZ KCGIInStream::SCRIPT_NAME;
constexpr KStringViewZ KCGIInStream::SERVER_NAME;
constexpr KStringViewZ KCGIInStream::SERVER_PORT;
constexpr KStringViewZ KCGIInStream::SERVER_PORT_SECURE;
constexpr KStringViewZ KCGIInStream::SERVER_PROTOCOL;
constexpr KStringViewZ KCGIInStream::SERVER_SOFTWARE;
constexpr KStringViewZ KCGIInStream::WEB_SERVER_API;

#endif
} // of namespace dekaf2

