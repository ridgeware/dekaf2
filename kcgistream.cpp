
/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2018, Ridgeware, Inc.
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
#include "kctype.h"

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

		// lazy creation of header from CGI env vars
		stream->CreateHeader();

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
			// we do not have to skip comment lines, so read all in one part
			stream->istream->read(sOutBuf, iRemain);
			return stream->istream->gcount() + (iCount - iRemain);
		}
		else
		{
			// we have to skip comment lines, therefore buffer all in a temporary
			// string and copy into the output if not a comment, and loop if by
			// skipping comment lines we reduced the real output size

			KString sBuf;

			while (iRemain > 0)
			{
				// make room for the requested buffer size
				sBuf.resize_uninitialized(iRemain);

				auto iRead = stream->istream->read(&sBuf[0], iRemain).gcount();

				if (!iRead)
				{
					// no more data - EOF
					break;
				}

				if (iRead < iRemain)
				{
					// shrink the buffer to the real input size
					sBuf.resize(iRead);
				}

				KStringView svBuf(sBuf);

				while (!svBuf.empty())
				{
					if (stream->bAtStartOfLine)
					{
						stream->bIsComment = (svBuf.front() == stream->chCommentDelimiter);
					}

					auto pos = svBuf.find('\n');

					if (pos == KStringView::npos)
					{
						// last fragment, no EOL
						stream->bAtStartOfLine = false;
						pos = svBuf.size();
					}
					else
					{
						stream->bAtStartOfLine = true;
						++pos;
					}

					if (!stream->bIsComment)
					{
						// copy the current line from the input sv
						std::memcpy(sOutBuf, svBuf.data(), pos);
						iRemain -= pos;
						sOutBuf += pos;
					}

					// and remove the current line from the input sv
					svBuf.remove_prefix(pos);
				}

			}

			return iCount - iRemain;
		}
	}

	return 0;

} // StreamReader

//-----------------------------------------------------------------------------
KString KCGIInStream::ConvertHTTPHeaderNameToCGIVar(KStringView sHeadername)
//-----------------------------------------------------------------------------
{
	KString sCGI;
	sCGI.reserve(sHeadername.size() + 5);

	sCGI = "HTTP_";

	for (auto ch : sHeadername)
	{
		if (KASCII::kIsAlNum(ch))
		{
			sCGI += KASCII::kToUpper(ch);
		}
		else if (ch == '-')
		{
			sCGI += '_';
		}
	}

	return sCGI;

} // ConvertHTTPHeaderNameToCGIVar

//-----------------------------------------------------------------------------
KString KCGIInStream::ConvertCGIVarToHTTPHeaderName(KStringView sCGIVar)
//-----------------------------------------------------------------------------
{
	KString sHeaderName;

	sCGIVar.remove_prefix("HTTP_");

	sHeaderName.reserve(sCGIVar.size());

	bool bIsFirst { true };

	for (auto ch : sCGIVar)
	{
		if (KASCII::kIsAlNum(ch))
		{
			if (bIsFirst)
			{
				sHeaderName += KASCII::kToUpper(ch);
				bIsFirst = false;
			}
			else
			{
				sHeaderName += KASCII::kToLower(ch);
			}
		}
		else if (ch == '_')
		{
			sHeaderName += '-';
			bIsFirst = true;
		}
	}

	return sHeaderName;

} // ConvertCGIVarToHTTPHeaderName

//-----------------------------------------------------------------------------
void KCGIInStream::Stream::AddCGIVar(KString sCGIVar, KString sHTTPHeader)
//-----------------------------------------------------------------------------
{
	m_AdditionalCGIVars.push_back({std::move(sCGIVar), std::move(sHTTPHeader)});

} // AddCGIVar

//-----------------------------------------------------------------------------
void KCGIInStream::Stream::CreateHeader()
//-----------------------------------------------------------------------------
{
	if (bIsCreated)
	{
		return;
	}

	bIsCreated = true;

	auto sMethod = kGetEnv(KCGIInStream::REQUEST_METHOD);

	if (sMethod.empty())
	{
		kDebug (1, "we are not running within a web server...");
		// permitting for comment lines
		chCommentDelimiter = '#';
		return;
	}

	kDebug (1, "we are running within a web server that sets the CGI variables...");

	// add method, resource and protocol from env vars
	m_sHeader = sMethod;
	m_sHeader += ' ';
	m_sHeader += kGetEnv(KCGIInStream::REQUEST_URI);
	m_sHeader += ' ';
	m_sHeader += kGetEnv(KCGIInStream::SERVER_PROTOCOL);
	kDebug(1, "{}", m_sHeader);
	m_sHeader += "\r\n";

	struct CGIVars_t { KStringViewZ sVar; KStringViewZ sHeader; };
	static constexpr CGIVars_t CGIVars[]
	{
		{ KCGIInStream::HTTP_ACCEPT,          KHTTPHeaders::ACCEPT          },
		{ KCGIInStream::HTTP_ACCEPT_ENCODING, KHTTPHeaders::ACCEPT_ENCODING },
		{ KCGIInStream::HTTP_ACCEPT_LANGUAGE, KHTTPHeaders::ACCEPT_LANGUAGE },
		{ KCGIInStream::HTTP_AUTHORIZATION,   KHTTPHeaders::AUTHORIZATION   },
		{ KCGIInStream::HTTP_CONNECTION,      KHTTPHeaders::CONNECTION      },
		{ KCGIInStream::HTTP_COOKIE,          KHTTPHeaders::COOKIE          },
		{ KCGIInStream::HTTP_HOST,            KHTTPHeaders::HOST            },
		{ KCGIInStream::HTTP_REFERER,         KHTTPHeaders::REFERER         },
		{ KCGIInStream::HTTP_USER_AGENT,      KHTTPHeaders::USER_AGENT      },
		{ KCGIInStream::CONTENT_TYPE,         KHTTPHeaders::CONTENT_TYPE    },
		{ KCGIInStream::CONTENT_LENGTH,       KHTTPHeaders::CONTENT_LENGTH  },
		{ KCGIInStream::REMOTE_ADDR,          KHTTPHeaders::X_FORWARDED_FOR },
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

	// add non-standard headers from env vars
	for (const auto& it : m_AdditionalCGIVars)
	{
		KString sEnv = kGetEnv(it.first);
		if (!sEnv.empty())
		{
			m_sHeader += it.second;
			m_sHeader += ": ";
			m_sHeader += sEnv;
			m_sHeader += "\r\n";
		}
	}

	// final end of header line
	m_sHeader += "\r\n";

	// point string view into the constructed header
	sHeader = m_sHeader;

} // CreateHeader

//-----------------------------------------------------------------------------
KCGIInStream::KCGIInStream(std::istream& istream)
//-----------------------------------------------------------------------------
    : base_type(&m_StreamBuf)
	, m_Stream(&istream)
{
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
constexpr KStringViewZ KCGIInStream::HTTP_AUTHORIZATION;
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

