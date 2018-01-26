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

#pragma once

#include "kstring.h"
#include "kstringview.h"
#include "kprops.h"
#include "ksplit.h"
#include <iostream>
#include <fcgiapp.h>
#include <fcgio.h>

//#define ATTEMPT_FCGI

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A common interface class for both CGI and FCGI requests.
class KCGI
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{
//------
public:
//------
	static constexpr KStringView AUTH_PASSWORD           = "AUTH_PASSWORD";
	static constexpr KStringView AUTH_TYPE               = "AUTH_TYPE";
	static constexpr KStringView AUTH_USER               = "AUTH_USER";
	static constexpr KStringView CERT_COOKIE             = "CERT_COOKIE";
	static constexpr KStringView CERT_FLAGS              = "CERT_FLAGS";
	static constexpr KStringView CERT_ISSUER             = "CERT_ISSUER";
	static constexpr KStringView CERT_KEYSIZE            = "CERT_KEYSIZE";
	static constexpr KStringView CERT_SECRETKEYSIZE      = "CERT_SECRETKEYSIZE";
	static constexpr KStringView CERT_SERIALNUMBER       = "CERT_SERIALNUMBER";
	static constexpr KStringView CERT_SERVER_ISSUER      = "CERT_SERVER_ISSUER";
	static constexpr KStringView CERT_SERVER_SUBJECT     = "CERT_SERVER_SUBJECT";
	static constexpr KStringView CERT_SUBJECT            = "CERT_SUBJECT";
	static constexpr KStringView CF_TEMPLATE_PATH        = "CF_TEMPLATE_PATH";
	static constexpr KStringView CONTENT_LENGTH          = "CONTENT_LENGTH";
	static constexpr KStringView CONTENT_TYPE            = "CONTENT_TYPE";
	static constexpr KStringView CONTEXT_PATH            = "CONTEXT_PATH";
	static constexpr KStringView GATEWAY_INTERFACE       = "GATEWAY_INTERFACE";
	static constexpr KStringView HTTPS                   = "HTTPS";
	static constexpr KStringView HTTPS_KEYSIZE           = "HTTPS_KEYSIZE";
	static constexpr KStringView HTTPS_SECRETKEYSIZE     = "HTTPS_SECRETKEYSIZE";
	static constexpr KStringView HTTPS_SERVER_ISSUER     = "HTTPS_SERVER_ISSUER";
	static constexpr KStringView HTTPS_SERVER_SUBJECT    = "HTTPS_SERVER_SUBJECT";
	static constexpr KStringView HTTP_ACCEPT             = "HTTP_ACCEPT";
	static constexpr KStringView HTTP_ACCEPT_ENCODING    = "HTTP_ACCEPT_ENCODING";
	static constexpr KStringView HTTP_ACCEPT_LANGUAGE    = "HTTP_ACCEPT_LANGUAGE";
	static constexpr KStringView HTTP_CONNECTION         = "HTTP_CONNECTION";
	static constexpr KStringView HTTP_COOKIE             = "HTTP_COOKIE";
	static constexpr KStringView HTTP_HOST               = "HTTP_HOST";
	static constexpr KStringView HTTP_REFERER            = "HTTP_REFERER";
	static constexpr KStringView HTTP_USER_AGENT         = "HTTP_USER_AGENT";
	static constexpr KStringView QUERY_STRING            = "QUERY_STRING";
	static constexpr KStringView REMOTE_ADDR             = "REMOTE_ADDR";
	static constexpr KStringView REMOTE_HOST             = "REMOTE_HOST";
	static constexpr KStringView REMOTE_USER             = "REMOTE_USER";
    static constexpr KStringView REQUEST_METHOD          = "REQUEST_METHOD";
    static constexpr KStringView REQUEST_URI             = "REQUEST_URI";
    static constexpr KStringView SCRIPT_NAME             = "SCRIPT_NAME";
	static constexpr KStringView SERVER_NAME             = "SERVER_NAME";
	static constexpr KStringView SERVER_PORT             = "SERVER_PORT";
	static constexpr KStringView SERVER_PORT_SECURE      = "SERVER_PORT_SECURE";
	static constexpr KStringView SERVER_PROTOCOL         = "SERVER_PROTOCOL";
	static constexpr KStringView SERVER_SOFTWARE         = "SERVER_SOFTWARE";
	static constexpr KStringView WEB_SERVER_API          = "WEB_SERVER_API";

	static constexpr KStringView GET                     = "GET";               // legal RHS of REQUEST_METHOD
	static constexpr KStringView HEAD                    = "HEAD";              // legal RHS of REQUEST_METHOD
	static constexpr KStringView POST                    = "POST";              // legal RHS of REQUEST_METHOD
	static constexpr KStringView PUT                     = "PUT";               // legal RHS of REQUEST_METHOD
	static constexpr KStringView DELETE                  = "DELETE";            // legal RHS of REQUEST_METHOD
	static constexpr KStringView CONNECT                 = "CONNECT";           // legal RHS of REQUEST_METHOD
	static constexpr KStringView OPTIONS                 = "OPTIONS";           // legal RHS of REQUEST_METHOD
	static constexpr KStringView TRACE                   = "TRACE";             // legal RHS of REQUEST_METHOD
	static constexpr KStringView PATCH                   = "PATCH";             // legal RHS of REQUEST_METHOD

	static constexpr KStringView FCGI_WEB_SERVER_ADDRS   = "FCGI_WEB_SERVER_ADDRS";

	//static bool IsWebRequest(); -- not sure this will work

	KCGI();
	~KCGI();
	KString     GetVar (KStringView sEnvironmentVariable, const char* sDefaultValue="");
	bool        GetNextRequest ();
	bool        ReadHeaders ();
	bool        ReadPostData ();
	bool        IsFCGI()   { return (m_bIsFCGI); }

    //std::streambuf* CIN()    { IsFCGI() ? m_FcgiRequest.in  : std::cin  }
    //std::streambuf* COUT()   { IsFCGI() ? m_FcgiRequest.out : std::cout }
    //std::streambuf* CERR()   { IsFCGI() ? m_FcgiRequest.err : std::cerr }

	void        BackupStreams ();
	void        RestoreStreams ();

	/// incoming http request method: GET, POST, etc.
	KString      m_sRequestMethod;

	/// incoming URL including the query string
	KString      m_sRequestURI;

	/// incoming URL with query string trimmed
	KString      m_sRequestPath;

	/// incoming http protocol and version as defined in status header
	KString      m_sHttpProtocol;

	/// query string (name=value&...)
	KString      m_sQueryString;

	/// raw, unprocessed incomiong POST data
	KString      m_sPostData; // aka body

	/// incoming request headers
    KProps <KString, KString, /*order-matters=*/false, /*unique-keys=*/false> m_Headers;

	/// incoming query parms off request URI
    KProps <KString, KString, /*order-matters=*/false, /*unique-keys=*/false> m_QueryParms;

	void init () {
	    m_sRequestMethod.clear();
	    m_sRequestURI.clear();
	    m_sRequestPath.clear();
	    m_sHttpProtocol.clear();
		m_sPostData.clear();
		m_Headers.clear();
		m_QueryParms.clear();
	}



//----------
protected:
//----------

//----------
private:
//----------
	unsigned int      m_iNumRequests = 0;
	FCGX_Request      m_FcgiRequest;
	bool              m_bIsFCGI      = false;
#ifdef ATTEMPT_FCGI
	std::streambuf*   m_pBackupCIN   = nullptr;
	std::streambuf*   m_pBackupCOUT  = nullptr;
	std::streambuf*   m_pBackupCERR  = nullptr;
#endif

}; // class KCGI

} // end of namespace dekaf2

