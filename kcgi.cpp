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
#include "ksystem.h"



namespace dekaf2 {

//-----------------------------------------------------------------------------
KCGI::KCGI()
//-----------------------------------------------------------------------------
    : KCGI(KInOut)
{
} // constructor

//-----------------------------------------------------------------------------
KCGI::KCGI(KStream& Stream)
//-----------------------------------------------------------------------------
    : KHTTPServer(Stream, GetVar(REMOTE_ADDR)) // TODO check for FCGI init when calling GetVar..
{

#ifdef DEKAF2_WITH_FCGI
	FCGX_Init();
	FCGX_InitRequest (&m_FcgiRequest, 0, 0);
#endif

}

//-----------------------------------------------------------------------------
KCGI::~KCGI()
//-----------------------------------------------------------------------------
{
} // destructor

//-----------------------------------------------------------------------------
void KCGI::clear()
//-----------------------------------------------------------------------------
{
	Request.clear();
	Response.clear();

} // init

//-----------------------------------------------------------------------------
KString KCGI::GetVar (KStringViewZ sEnvironmentVariable, KStringViewZ sDefaultValue/*=""*/)
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
void KCGI::SkipComments(KInStream& Stream, char chCommentDelim)
//-----------------------------------------------------------------------------
{
	// check if we have leading comment lines, and skip them
	while (Stream.Read() == chCommentDelim)
	{
		KString sLine;
		if (!Stream.ReadLine(sLine))
		{
			break;
		}
	}
	Stream.UnRead();

} // SkipComments

//-----------------------------------------------------------------------------
bool KCGI::Parse(char chCommentDelim)
//-----------------------------------------------------------------------------
{
	if (chCommentDelim)
	{
		// skip leading comments
		SkipComments(Request.UnfilteredStream(), chCommentDelim);
	}

	clear();

	++m_iNumRequests;

	m_bIsCGI  = false;
	m_bIsFCGI = false; //DEKAF2_WITH_FCGI (getenv(KString(KCGI::FCGI_WEB_SERVER_ADDRS).c_str()) != nullptr); // TODO: I don't think this test works.

	KString sRM = GetVar (KCGI::REQUEST_METHOD);
	if (!sRM.empty())
	{
		m_bIsCGI = true;
		// we are running within a web server that sets these:
		Request.Method         = sRM.ToView();
		Request.Resource       = GetVar(KCGI::REQUEST_URI);
		Request.sHTTPVersion   = GetVar(KCGI::SERVER_PROTOCOL);
		Request.Headers.Set(KHTTPHeaders::HOST,            GetVar(KCGI::HTTP_HOST));
		Request.Headers.Set(KHTTPHeaders::CONTENT_TYPE,    GetVar(KCGI::CONTENT_TYPE));
		Request.Headers.Set(KHTTPHeaders::CONTENT_LENGTH,  GetVar(KCGI::CONTENT_LENGTH));
		Request.Headers.Set(KHTTPHeaders::X_FORWARDED_FOR, GetVar(KCGI::REMOTE_ADDR));

		// make sure the input filter knows these settings
		Request.KInHTTPFilter::Parse(Request);
	}
	else if (!Request.Parse())
	{
		m_sError = Request.Error();
		return (false);
	}

	// TODO separate post reader from header reader and offer a stream interface for it

	KString sLine;
	while (Request.ReadLine(sLine))
	{
		if (chCommentDelim && !sLine.empty() && sLine.front() == chCommentDelim) {
			kDebug (2, "KCGI: skipping comment line: {}", sLine);
			continue;
		}
		m_sPostData += sLine;
		m_sPostData += "\n";
	}

	kDebug (1, "KCGI: request#{}: {} {}, {} headers, {} query parms, {} bytes post data",
			m_iNumRequests,
			GetRequestMethod(),
			GetRequestPath(),
			GetRequestHeaders().size(),
			GetQueryParms().size(),
			m_sPostData.length());

	return (true);	// true ==> we got a request

} // Parse

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE

constexpr KStringViewZ KCGI::AUTH_PASSWORD;
constexpr KStringViewZ KCGI::AUTH_TYPE;
constexpr KStringViewZ KCGI::AUTH_USER;
constexpr KStringViewZ KCGI::CERT_COOKIE;
constexpr KStringViewZ KCGI::CERT_FLAGS;
constexpr KStringViewZ KCGI::CERT_ISSUER;
constexpr KStringViewZ KCGI::CERT_KEYSIZE;
constexpr KStringViewZ KCGI::CERT_SECRETKEYSIZE;
constexpr KStringViewZ KCGI::CERT_SERIALNUMBER;
constexpr KStringViewZ KCGI::CERT_SERVER_ISSUER;
constexpr KStringViewZ KCGI::CERT_SERVER_SUBJECT;
constexpr KStringViewZ KCGI::CERT_SUBJECT;
constexpr KStringViewZ KCGI::CF_TEMPLATE_PATH;
constexpr KStringViewZ KCGI::CONTENT_LENGTH;
constexpr KStringViewZ KCGI::CONTENT_TYPE;
constexpr KStringViewZ KCGI::CONTEXT_PATH;
constexpr KStringViewZ KCGI::GATEWAY_INTERFACE;
constexpr KStringViewZ KCGI::HTTPS;
constexpr KStringViewZ KCGI::HTTPS_KEYSIZE;
constexpr KStringViewZ KCGI::HTTPS_SECRETKEYSIZE;
constexpr KStringViewZ KCGI::HTTPS_SERVER_ISSUER;
constexpr KStringViewZ KCGI::HTTPS_SERVER_SUBJECT;
constexpr KStringViewZ KCGI::HTTP_ACCEPT;
constexpr KStringViewZ KCGI::HTTP_ACCEPT_ENCODING;
constexpr KStringViewZ KCGI::HTTP_ACCEPT_LANGUAGE;
constexpr KStringViewZ KCGI::HTTP_CONNECTION;
constexpr KStringViewZ KCGI::HTTP_COOKIE;
constexpr KStringViewZ KCGI::HTTP_HOST;
constexpr KStringViewZ KCGI::HTTP_REFERER;
constexpr KStringViewZ KCGI::HTTP_USER_AGENT;
constexpr KStringViewZ KCGI::QUERY_STRING;
constexpr KStringViewZ KCGI::REMOTE_ADDR;
constexpr KStringViewZ KCGI::REMOTE_HOST;
constexpr KStringViewZ KCGI::REMOTE_USER;
constexpr KStringViewZ KCGI::REQUEST_METHOD;
constexpr KStringViewZ KCGI::REQUEST_URI;
constexpr KStringViewZ KCGI::SCRIPT_NAME;
constexpr KStringViewZ KCGI::SERVER_NAME;
constexpr KStringViewZ KCGI::SERVER_PORT;
constexpr KStringViewZ KCGI::SERVER_PORT_SECURE;
constexpr KStringViewZ KCGI::SERVER_PROTOCOL;
constexpr KStringViewZ KCGI::SERVER_SOFTWARE;
constexpr KStringViewZ KCGI::WEB_SERVER_API;

constexpr KStringViewZ KCGI::FCGI_WEB_SERVER_ADDRS;

#endif

} // dekaf2
