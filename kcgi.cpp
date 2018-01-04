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
{
	FCGX_Init();
	FCGX_InitRequest (&m_FcgiRequest, 0, 0);	

#ifdef ATTEMPT_FCGI
	BackupStreams ();
#endif

} // constructor

//-----------------------------------------------------------------------------
KCGI::~KCGI()
//-----------------------------------------------------------------------------
{
#ifdef ATTEMPT_FCGI
	RestoreStreams ();
#endif

} // destructor

//-----------------------------------------------------------------------------
KString KCGI::GetVar (KStringView sEnvironmentVariable, const char* sDefaultValue/*=""*/)
//-----------------------------------------------------------------------------
{
    KString sEnvVar (sEnvironmentVariable); // TODO:KEEF: figure out how to cast a KStringView to a const char*, without doing this!
    KString sReturnMe = NULL;

	if (IsFCGI()) {
        sReturnMe = FCGX_GetParam (sEnvVar.c_str(), m_FcgiRequest.envp);
	}
	else {
        sReturnMe = getenv (sEnvVar.c_str());
	}

    if (sReturnMe == NULL) {
        sReturnMe = sDefaultValue;
	}

	return (sReturnMe);  // <-- creates KString on return

} // KCGI::GetVar

//-----------------------------------------------------------------------------
void KCGI::BackupStreams ()
//-----------------------------------------------------------------------------
{
#ifdef ATTEMPT_FCGI
	// save standard streams:
	if (!m_pBackupCIN) {
        m_pBackupCIN   = std::cin.rdbuf();
	}
	if (!m_pBackupCOUT) {
        m_pBackupCOUT  = std::cout.rdbuf();
	}
	if (!m_pBackupCERR) {
        m_pBackupCERR  = std::cerr.rdbuf();
	}
#endif

} // BackupStreams

//-----------------------------------------------------------------------------
void KCGI::RestoreStreams ()
//-----------------------------------------------------------------------------
{
#ifdef ATTEMPT_FCGI
	if (m_pBackupCIN) {
        std::cin.rdbuf(m_pBackupCIN);
	}
	if (m_pBackupCOUT) {
        std::cin.rdbuf(m_pBackupCOUT);
	}
	if (m_pBackupCERR) {
        std::cin.rdbuf(m_pBackupCERR);
	}
#endif

} // RestoreStreams

//-----------------------------------------------------------------------------
bool KCGI::ReadHeaders ()
//-----------------------------------------------------------------------------
{
	// TODO: FCGI (only coded for CGI right now)

	kDebug (1, "KCGI: reading headers and post data...");

	int  iLineNo = 0;
	bool bHeaders = true;
	enum { MAXLINE = 10000 }; // maxlen of any particular header
	char szLine[MAXLINE+1];
	while (fgets (szLine, MAXLINE, stdin)) // CGI only
	{
		if (++iLineNo == 1) {
			// GET /ApiTranslate?foo=bar HTTP/1.0
			KASCII::ktrimright(szLine);
			kDebug (1, "KCGI: status line: {}", szLine);
			char* b1 = strchr(szLine,' ');
			if (!b1) {
				kDebug (1, "KCGI: malformed status line: {}", szLine);
				return (false);
			}
			*b1 = 0;
			m_sRequestMethod = KASCII::ktrimleft(KASCII::ktrimright(szLine));
			m_sRequestMethod.ToUpper();
			char* uri = (char*) KASCII::ktrimleft(b1+1);
			char* b2  = strchr(uri,' ');
			if (!b2) {
				kDebug (1, "KCGI: malformed status line: {}", szLine);
				return (false);
			}
			*b2 = 0;
			m_sRequestURI   = KASCII::ktrimleft(KASCII::ktrimright(uri));
			m_sHttpProtocol = KASCII::ktrimleft(KASCII::ktrimright(b2+1));

			char* hook = strchr(uri,'?');
			if (hook) {
				m_sQueryString = hook+1;
			}

			kDebug (1, "KCGI: method = {}", m_sRequestMethod);
			kDebug (1, "KCGI: uri    = {}", m_sRequestURI);
			kDebug (1, "KCGI: proto  = {}", m_sHttpProtocol);
			kDebug (1, "KCGI: query  = {}", m_sQueryString);
		}
		else if (bHeaders)
		{
			KASCII::ktrimright(szLine);

			// User-Agent: whatever
			if (!*szLine) {
				return (true); // newline at end of headers
			}
			else
			{
				char* szColon = strchr(szLine,':');
				if (!szColon) {
					kDebug (1, "KCGI: malformed header: no colon: {}", szLine);
					return (false); // malformed headers
				}
				*szColon = 0;
				const char* szRHS = KASCII::ktrimleft(KASCII::ktrimright(szColon + 1));
				kDebug (1, "KCGI: header: {}={}", szLine, szRHS);
				m_Headers.Add (szLine, szRHS);
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

	enum { MAXCHUNK = 10000 }; // maxlen of any particular header
	char szChunk[MAXCHUNK+1];
	while (fgets (szChunk, MAXCHUNK, stdin)) // CGI only
	{
		m_sPostData += szChunk; // could be binary data: do not trim
	}

	return (true);

} // ReadPostData

//-----------------------------------------------------------------------------
bool KCGI::GetNextRequest ()
//-----------------------------------------------------------------------------
{
	++m_iNumRequests;

	m_bIsFCGI = false; //ATTEMPT_FCGI (getenv(KString(KCGI::FCGI_WEB_SERVER_ADDRS).c_str()) != nullptr); // TODO: I don't think this test works.

	if ((m_iNumRequests == 1) || (m_bIsFCGI && FCGX_Accept_r(&m_FcgiRequest)))
	{
		// in case we are running within a web server that sets these:
		m_sRequestMethod = GetVar (KCGI::REQUEST_METHOD);
		m_sRequestURI    = GetVar (KCGI::REQUEST_URI);
		m_sQueryString	 = GetVar (KCGI::QUERY_STRING);

		// if environment vars not set, expect them in the input stream:
		if (m_sRequestMethod.empty() && !ReadHeaders()) {
			return (false);
		}

		m_sRequestPath   = m_sRequestURI;
		m_sRequestPath.ClipAt("?");

		kSplitPairs (
			/*Container= */ m_QueryParms,
			/*Buffer=*/		m_sQueryString,
			/*PairDelim=*/	'=',
			/*Delim=*/		"&"
		);

		if (!ReadPostData()) {
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

	return (false);

} // GetNextRequest

} // dekaf2
