/*
//-----------------------------------------------------------------------------//
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

#include "kcgi.h"
#include "kstringutils.h"
#include "kurlencode.h"
#include "ksystem.h"

namespace dekaf2 {

#if !defined(DEKAF2_NO_GCC) && (DEKAF2_GCC_VERSION < 70000)

constexpr KStringView KHeader::AUTH_PASSWORD;
constexpr KStringView KHeader::AUTH_TYPE;
constexpr KStringView KHeader::AUTH_USER;
constexpr KStringView KHeader::CERT_COOKIE;
constexpr KStringView KHeader::CERT_FLAGS;
constexpr KStringView KHeader::CERT_ISSUER;
constexpr KStringView KHeader::CERT_KEYSIZE;
constexpr KStringView KHeader::CERT_SECRETKEYSIZE;
constexpr KStringView KHeader::CERT_SERIALNUMBER;
constexpr KStringView KHeader::CERT_SERVER_ISSUER;
constexpr KStringView KHeader::CERT_SERVER_SUBJECT;
constexpr KStringView KHeader::CERT_SUBJECT;
constexpr KStringView KHeader::CF_TEMPLATE_PATH;
constexpr KStringView KHeader::CONTENT_LENGTH;
constexpr KStringView KHeader::CONTENT_TYPE;
constexpr KStringView KHeader::CONTEXT_PATH;
constexpr KStringView KHeader::GATEWAY_INTERFACE;
constexpr KStringView KHeader::HTTPS;
constexpr KStringView KHeader::HTTPS_KEYSIZE;
constexpr KStringView KHeader::HTTPS_SECRETKEYSIZE;
constexpr KStringView KHeader::HTTPS_SERVER_ISSUER;
constexpr KStringView KHeader::HTTPS_SERVER_SUBJECT;
constexpr KStringView KHeader::HTTP_ACCEPT;
constexpr KStringView KHeader::HTTP_ACCEPT_ENCODING;
constexpr KStringView KHeader::HTTP_ACCEPT_LANGUAGE;
constexpr KStringView KHeader::HTTP_CONNECTION;
constexpr KStringView KHeader::HTTP_COOKIE;
constexpr KStringView KHeader::HTTP_HOST;
constexpr KStringView KHeader::HTTP_REFERER;
constexpr KStringView KHeader::HTTP_USER_AGENT;
constexpr KStringView KHeader::QUERY_STRING;
constexpr KStringView KHeader::REMOTE_ADDR;
constexpr KStringView KHeader::REMOTE_HOST;
constexpr KStringView KHeader::REMOTE_USER;
constexpr KStringView KHeader::REQUEST_METHOD;
constexpr KStringView KHeader::REQUEST_URI;
constexpr KStringView KHeader::SCRIPT_NAME;
constexpr KStringView KHeader::SERVER_NAME;
constexpr KStringView KHeader::SERVER_PORT;
constexpr KStringView KHeader::SERVER_PORT_SECURE;
constexpr KStringView KHeader::SERVER_PROTOCOL;
constexpr KStringView KHeader::SERVER_SOFTWARE;
constexpr KStringView KHeader::WEB_SERVER_API;

constexpr KStringView KHeader::GET;
constexpr KStringView KHeader::HEAD;
constexpr KStringView KHeader::POST;
constexpr KStringView KHeader::PUT;
constexpr KStringView KHeader::DELETE;
constexpr KStringView KHeader::CONNECT;
constexpr KStringView KHeader::OPTIONS;
constexpr KStringView KHeader::TRACE;
constexpr KStringView KHeader::PATCH;

constexpr KStringView KHeader::FCGI_WEB_SERVER_ADDRS;

#endif

//-----------------------------------------------------------------------------
KCGI::KCGI()
//-----------------------------------------------------------------------------
	: m_Reader(std::make_unique<KInStream>(std::cin))
	, m_Writer(std::make_unique<KOutStream>(std::cout))

{
#ifdef DEKAF2_WITH_FCGI
	FCGX_Init();
	FCGX_InitRequest (&m_FcgiRequest, 0, 0);
#endif

} // constructor

//-----------------------------------------------------------------------------
KCGI::~KCGI()
//-----------------------------------------------------------------------------
{
} // destructor

//-----------------------------------------------------------------------------
void KCGI::init (bool bResetStreams)
//-----------------------------------------------------------------------------
{
	m_sError.clear();
	m_sCommentDelim.clear();
	m_sRequestMethod.clear();
	m_sRequestURI.clear();
	m_sRequestPath.clear();
	m_sHttpProtocol.clear();
	m_sPostData.clear();
	m_Headers.clear();
	m_QueryParms.clear();

	if (bResetStreams)
	{
		m_Reader = std::make_unique<KInStream>(std::cin);
	}

} // init

//-----------------------------------------------------------------------------
KString KCGI::GetVar (KStringView sEnvironmentVariable, const char* sDefaultValue/*=""*/)
//-----------------------------------------------------------------------------
{
#ifdef DEKAF2_WITH_FCGI
	if (IsFCGI())
	{
		KString sEnvVar (sEnvironmentVariable);
		auto szRet = FCGX_GetParam (sEnvVar.c_str(), m_FcgiRequest.envp);
		if (szRet == nullptr)
		{
			return sDefaultValue;
		}
		return szRet;
	}
	else
#endif
	{
		return kGetEnv(sEnvironmentVariable, sDefaultValue);
	}

} // KCGI::GetVar

//-----------------------------------------------------------------------------
bool KCGI::ReadHeaders ()
//-----------------------------------------------------------------------------
{
	// TODO: FCGI (only coded for CGI right now)

	kDebug (1, "KCGI: reading headers and post data...");

	int       iRealLines = 0;
	bool      bHeaders = true;
	KString   sLine;

	while (m_Reader->ReadLine(sLine))
	{
		if (!m_sCommentDelim.empty() && sLine.StartsWith(m_sCommentDelim)) {
			kDebug (2, "KCGI: skipping comment line: {}", sLine);
			continue;
		}
		
		if (++iRealLines == 1)
		{
			// GET /ApiTranslate?foo=bar HTTP/1.0
			sLine.TrimRight();
			kDebug (1, "KCGI: status line: {}", sLine);

			auto pos = sLine.find(' ');
			if (!pos || pos == KString::npos)
			{
				m_sError.Format ("KCGI: malformed status line: {}", sLine);
				return (false);
			}

			m_sRequestMethod = sLine.substr(0, pos).Trim().ToUpper();

			KStringView sURI = sLine.ToView(pos+1);
			pos = sURI.find(' ');
			if (!pos || pos == KString::npos)
			{
				m_sError.Format ("KCGI: malformed status line: {}", sLine);
				return (false);
			}

			m_sRequestURI = sURI.substr(0, pos);
			m_sRequestURI.Trim();
			kUrlDecode(m_sRequestURI, /*bPlusAsSpace=*/true);

			m_sHttpProtocol = sURI.substr(pos+1);
			m_sHttpProtocol.Trim();

			pos = m_sRequestURI.find('?');
			if (pos && pos != KString::npos)
			{
				m_sQueryString = m_sRequestURI.substr(pos+1);
			}

			kDebug (1, "KCGI: method = {}", m_sRequestMethod);
			kDebug (1, "KCGI: uri    = {}", m_sRequestURI);
			kDebug (1, "KCGI: proto  = {}", m_sHttpProtocol);
			kDebug (1, "KCGI: query  = {}", m_sQueryString);
		}
		else if (bHeaders)
		{
			sLine.TrimRight();

			// User-Agent: whatever
			if (sLine.empty())
			{
				return (true); // newline at end of headers
			}
			else
			{
				auto pos = sLine.find(':');
				if (!pos || pos == KString::npos)
				{
					m_sError.Format ("KCGI: malformed header: no colon: {}", sLine);
					return (false); // malformed headers
				}

				KStringView sLHS = sLine.ToView(0, pos);
				KStringView sRHS = sLine.ToView(pos + 1);
				kTrim(sLHS);
				kTrim(sRHS);

				kDebug (1, "KCGI: header: {}={}", sLHS, sRHS);

				m_Headers.Add (sLHS, sRHS);
			}
		}
	}

	return (true); // nothing on stdin

} // ReadHeaders

//-----------------------------------------------------------------------------
bool KCGI::ReadPostData ()
//-----------------------------------------------------------------------------
{
	// TODO: not coded for chunking (ignores Content-Length and reads to end of stdin right now)

	KString   sLine;
	while (m_Reader->ReadLine(sLine))
	{
		if (!m_sCommentDelim.empty() && sLine.StartsWith(m_sCommentDelim)) {
			kDebug (2, "KCGI: skipping comment line: {}", sLine);
			continue;
		}
		m_sPostData += sLine;
	}

	return (true);

} // ReadPostData

//-----------------------------------------------------------------------------
bool KCGI::GetNextRequest (KStringView sFilename /*= KStringView{}*/, KStringView sCommentDelim /*= KStringView{}*/)
//-----------------------------------------------------------------------------
{
	if (!sFilename.empty())
	{
		init (false);

		m_Reader = std::make_unique<KInFile>(sFilename);

		if (!m_Reader->InStream().good())
		{
			m_sError.Format ("KCGI: cannot open input file: {}", sFilename);
			return (false);
		}

		m_sCommentDelim = sCommentDelim;
		// TODO: test success and return (false) if failed to read file
	}
	else
	{
		init (true);
	}

	++m_iNumRequests;

	m_bIsFCGI = false; //DEKAF2_WITH_FCGI (getenv(KString(KCGI::FCGI_WEB_SERVER_ADDRS).c_str()) != nullptr); // TODO: I don't think this test works.

	if (m_iNumRequests == 1
#ifdef DEKAF2_WITH_FCGI
	    || (m_bIsFCGI && FCGX_Accept_r(&m_FcgiRequest))
#endif
	    )
	{
		// in case we are running within a web server that sets these:
		m_sRequestMethod = GetVar (KCGI::REQUEST_METHOD);
		m_sRequestURI    = GetVar (KCGI::REQUEST_URI);
		m_sQueryString	 = GetVar (KCGI::QUERY_STRING);

		// if environment vars not set, expect them in the input stream:
		if (m_sRequestMethod.empty() && !ReadHeaders())
		{
			// error message already set in ReadHeaders()
			return (false);
		}

		// We need to perform UrlDecode on the RequestURI and the QueryString.
		kUrlDecode (m_sRequestURI,  /*bPlusAsSpace=*/true);
		kUrlDecode (m_sQueryString, /*bPlusAsSpace=*/true);

		m_sRequestPath = m_sRequestURI.substr(0, m_sRequestURI.find('?'));

		kSplitPairs (
			/*Container= */ m_QueryParms,
			/*Buffer=*/		m_sQueryString,
			/*PairDelim=*/	'=',
			/*Delim=*/		"&"
		);

		if (!ReadPostData())
		{
			// error message already set in ReadPostData()
			return (false);
		}

		kDebug (1, "KCGI: request#{}: {} {}, {} headers, {} query parms, {} bytes post data", 
			m_iNumRequests,
			m_sRequestMethod,
			m_sRequestPath,
			m_Headers.size(),
			m_QueryParms.size(),
			m_sPostData.length());

		return (true);	// true ==> we got a request
	}

	return (false); // no more requests

} // GetNextRequest

} // dekaf2
