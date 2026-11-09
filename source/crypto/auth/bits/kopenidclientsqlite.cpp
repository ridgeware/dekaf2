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
 // |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |\|
 // |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
 // |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
 // |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
 // |/|                                                                     |/|
 // |/+---------------------------------------------------------------------+/|
 // |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
 // +-------------------------------------------------------------------------+
 //
 */

#include <dekaf2/crypto/auth/kopenidclient.h>
#include <dekaf2/crypto/auth/ksession.h>   // KSession(KStringViewZ, Config) - defined in ksqlite

#if DEKAF2_HAS_SQLITE3

DEKAF2_NAMESPACE_BEGIN

// ---- KOpenIDClient SQLite convenience constructor ------------------------
// Like the KSQL constructor (kopenidclientksql.cpp, in ksql2), this one lives
// in the ksqlite library, next to the KSessionSQLiteStore it uses, and not in
// kopenidclient.cpp: a static libdekaf2.a comes after libksqlite.a on the link
// line, and a reference from dekaf2 into ksqlite stays unresolved with linkers
// that read each archive once (GNU ld) - "undefined reference to
// KSession::KSession(KStringViewZ, KSession::Config)". It sets no
// authenticator: OIDC sessions are created via
// KSession::CreateTrustedSession() (see kopenidclient.cpp).

//-----------------------------------------------------------------------------
KOpenIDClient::KOpenIDClient(Config config, KStringViewZ sSessionDBPath)
: m_Config(std::move(config))
//-----------------------------------------------------------------------------
{
	// SQLite-backed store at sSessionDBPath
	m_pSession = std::make_shared<KSession>(sSessionDBPath, MakeSessionConfig());

} // KOpenIDClient (standalone, SQLite)

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SQLITE3
