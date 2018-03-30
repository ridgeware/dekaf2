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
    : KHTTPServer(Stream)
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
void KCGI::SkipComments(KInStream& Stream, char chCommentDelim)
//-----------------------------------------------------------------------------
{
	// check if we have leading comment lines, and skip them
	while (Stream.InStream().get() == chCommentDelim)
	{
		KString sLine;
		if (!Stream.ReadLine(sLine))
		{
			break;
		}
	}
	Stream.InStream().unget();

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
		Request.Method           = sRM.ToView();
		Request.Resource         = GetVar(KCGI::REQUEST_URI);
		Request.Resource.Query   = GetVar(KCGI::QUERY_STRING);
		Request.HTTPVersion      = GetVar(KCGI::SERVER_PROTOCOL);
		Request.Headers.Set(KHTTPHeaders::HOST,           GetVar(KCGI::HTTP_HOST));
		Request.Headers.Set(KHTTPHeaders::CONTENT_TYPE,   GetVar(KCGI::CONTENT_TYPE));
		Request.Headers.Set(KHTTPHeaders::CONTENT_LENGTH, GetVar(KCGI::CONTENT_LENGTH));
		Request.Headers.Set(KHTTPHeaders::FROM,           GetVar(KCGI::REMOTE_ADDR));

		// make sure the input filter knows these settings
		Request.KInHTTPFilter::Parse(Request);
	}
	else if (!Request.Parse())
	{
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

constexpr KStringView KHeader::FCGI_WEB_SERVER_ADDRS;

#endif

} // dekaf2
