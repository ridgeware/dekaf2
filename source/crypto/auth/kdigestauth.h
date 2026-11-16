/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter (tm)
 //
 // Copyright (c) 2026, Ridgeware, Inc.
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

/// @file kdigestauth.h
/// Digest access authentication (RFC 2617), independent of the protocol that
/// carries it: HTTP, SIP and RTSP all use the same scheme, each with its own
/// request methods and request URIs.

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_auth
/// @{

//-----------------------------------------------------------------------------
/// Computes the credentials for Digest access authentication (RFC 2617,
/// algorithm MD5): the value of an Authorization or Proxy-Authorization
/// header that answers a server's challenge. Not bound to HTTP - a SIP or
/// RTSP client passes its own method and request URI.
/// @param sUsername the user name
/// @param sPassword the password
/// @param sRealm the realm of the challenge
/// @param sNonce the nonce of the challenge
/// @param sOpaque the opaque value of the challenge. It is returned to the
/// server unchanged, and left out if empty: a server that sent none does not
/// expect one
/// @param sQoP the quality of protection to use: "auth", "auth-int", or
/// anything else (typically an empty string) for the response without qop
/// of RFC 2069, which many SIP registrars still ask for. A challenge may
/// offer a list ("auth,auth-int") - the caller picks one
/// @param sMethod the request method, e.g. "GET" or "REGISTER"
/// @param sURI the request URI, exactly as it shall appear in the uri
/// parameter: the server hashes what it reads there
/// @param iNonceCount how often this nonce has been used, this request
/// included (the first use is 1). Only used with a qop
/// @param sCNonce the client nonce, any random string. Only used with a qop
/// @param sBody the request body, only hashed for the qop "auth-int"
/// @return the header value, starting with "Digest "
DEKAF2_NODISCARD DEKAF2_PUBLIC
KString kDigestAuthorization(KStringView sUsername,
                             KStringView sPassword,
                             KStringView sRealm,
                             KStringView sNonce,
                             KStringView sOpaque,
                             KStringView sQoP,
                             KStringView sMethod,
                             KStringView sURI,
                             uint32_t    iNonceCount,
                             KStringView sCNonce,
                             KStringView sBody = KStringView{});
//-----------------------------------------------------------------------------

/// @}

DEKAF2_NAMESPACE_END
