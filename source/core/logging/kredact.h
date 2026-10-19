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
/// (password, secret, api_key, ...), also in quotes as in SQL ("set password =
/// 'value'"), the STRING value of a JSON field whose name
/// does ("access_token", "ApiTokens", "client_secret", ...), and the password in
/// the userinfo of a URL.
/// A credential of 16 characters or more KEEPS ITS FIRST AND LAST THREE, as
/// "sk-...redacted...890", so a line still answers which key it was and whether
/// it changed; a shorter one is replaced by "[redacted]" entire, because six
/// characters of an eight character password is the password. Names and schemes
/// (Bearer, Basic, a scheme that ends with "Key" or "Token", ...) are kept - and
/// after "Authorization:", any other first word is redacted together with the
/// word after it, as it may be the scheme of the credentials that follow.
///
/// KLog redacts every message with it, and it serves as well for a text that is
/// stored or forwarded, such as the error body of a service. A text without
/// credentials costs a few scans for rare characters and no allocation.
///
/// Only a STRING value of a JSON field is a credential: '"input_tokens": 1500' is
/// a count, and a name ending in "_key" is an identifier, so neither is touched.
/// Credentials in other shapes are not recognized - notably a key quoted in prose
/// by the service that rejected it ("Incorrect API key provided: sk-...").
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

//-----------------------------------------------------------------------------
/// Redacts credentials in an SQL statement: all that kRedactCredentials()
/// redacts, and in an insert statement with a column list the strings in the
/// columns whose name denotes a credential, in all rows of the statement, as in
/// "insert into USERS (name, password) values ('bob', 'value')". The names are
/// those of JSON fields that kRedactCredentials() recognizes ("password",
/// "api_key", "access_token", ...).
///
/// KSQL, KROW and KSQLite redact each statement with it before they write it
/// into a log or an error message. kRedactCredentials() does not look for insert
/// statements, because KLog calls it for every message, and the search would make
/// every message slower.
/// @param sSQL the statement to redact
/// @param sBuffer receives the redacted statement, only if something was redacted
/// @return true if sBuffer holds the redacted statement, false if sSQL needs no
/// change and sBuffer was left untouched
DEKAF2_PUBLIC
bool kRedactSQLCredentials(KStringView sSQL, KString& sBuffer);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Redacts credentials in an SQL statement, as
/// kRedactSQLCredentials(KStringView, KString&) does, and returns a copy also if
/// nothing was redacted
/// @param sSQL the statement to redact
/// @return the redacted statement
DEKAF2_PUBLIC
KString kRedactSQLCredentials(KStringView sSQL);
//-----------------------------------------------------------------------------

/// @}

DEKAF2_NAMESPACE_END
