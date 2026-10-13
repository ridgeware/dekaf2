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

/// @file kredact.h
/// redaction of credentials in text

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/strings/kstring.h>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup core_logging
/// @{

//-----------------------------------------------------------------------------
/// Redacts credentials in a text: a JWT, the value after "Bearer " and after
/// "Authorization:", the value of a parameter whose name denotes a credential
/// (password, secret, api_key, ...), and the password in the userinfo of a URL.
/// Each is replaced by "[redacted]", names and schemes (Bearer, Basic, a scheme
/// that ends with "Key" or "Token", ...) are kept - after "Authorization:", any
/// other first word is redacted together with the word after it, as it may be the
/// scheme of the credentials that follow. KLog redacts
/// every message with it, and it serves as well for a text that is stored or
/// forwarded, such as the error body of a service. A text without credentials
/// costs a few scans for rare characters and no allocation. Credentials in other
/// shapes are not recognized.
/// @param sMessage the text to redact
/// @param sBuffer receives the redacted text, only if something was redacted
/// @return true if sBuffer holds the redacted text, false if sMessage needs no
/// change and sBuffer was left untouched
DEKAF2_PUBLIC
bool kRedactCredentials(KStringView sMessage, KString& sBuffer);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Redacts credentials in a text, as kRedactCredentials(KStringView, KString&)
/// does, and returns a copy also if nothing was redacted
/// @param sMessage the text to redact
/// @return the redacted text
DEKAF2_PUBLIC
KString kRedactCredentials(KStringView sMessage);
//-----------------------------------------------------------------------------

/// @}

DEKAF2_NAMESPACE_END
