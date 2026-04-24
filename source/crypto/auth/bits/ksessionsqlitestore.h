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

#pragma once

/// @file ksessionsqlitestore.h
/// KSQLite-based storage backend for KSession.

#include <dekaf2/crypto/auth/ksession.h>

#if DEKAF2_HAS_SQLITE3

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// SQLite storage backend for KSession. Persists sessions in a single
/// table (default name "KSESSION_SESSIONS") that is created on first use
/// if it does not exist.
class DEKAF2_PUBLIC KSessionSQLiteStore : public KSession::Store
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// @param sDatabase path to the SQLite database file (will be created
	/// if it does not exist)
	/// @param sTableName name of the sessions table — useful when sharing
	/// the database with other components (default "KSESSION_SESSIONS")
	explicit KSessionSQLiteStore(KString sDatabase,
	                             KString sTableName = "KSESSION_SESSIONS");

	bool              Initialize   () override;
	bool              Create       (const KSession::Record& Rec) override;
	bool              Lookup       (KStringView sToken, KSession::Record* pOut) override;
	bool              Touch        (KStringView sToken, KUnixTime tLastSeen) override;
	bool              Erase        (KStringView sToken) override;
	std::size_t       EraseAllFor  (KStringView sUsername) override;
	std::size_t       PurgeExpired (KUnixTime tOldestLastSeen, KUnixTime tOldestCreated) override;
	std::size_t       Count        () const override;
	KString           GetLastError () const override { return m_sError; }

//------
private:
//------

	KString         m_sDatabase;
	KString         m_sTableName;
	mutable KString m_sError;

}; // KSessionSQLiteStore

DEKAF2_NAMESPACE_END

#endif // DEKAF2_HAS_SQLITE3
