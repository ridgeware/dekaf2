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
constexpr KStringView KHeader::GET;
constexpr KStringView KHeader::HEAD;
constexpr KStringView KHeader::POST;
constexpr KStringView KHeader::PUT;
constexpr KStringView KHeader::DELETE;
constexpr KStringView KHeader::CONNECT;
constexpr KStringView KHeader::OPTIONS;
constexpr KStringView KHeader::TRACE;
constexpr KStringView KHeader::PATCH;
constexpr KStringView KHeader::SCRIPT_NAME;
constexpr KStringView KHeader::SERVER_NAME;
constexpr KStringView KHeader::SERVER_PORT;
constexpr KStringView KHeader::SERVER_PORT_SECURE;
constexpr KStringView KHeader::SERVER_PROTOCOL;
constexpr KStringView KHeader::SERVER_SOFTWARE;
constexpr KStringView KHeader::WEB_SERVER_API;

#endif

} // dekaf2
