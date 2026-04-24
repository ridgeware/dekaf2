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

/// @file ksessionksqlstore.h
/// KSQL-based storage backend for KSession — supports MySQL/MariaDB,
/// PostgreSQL, SQLite3 and other databases through the KSQL abstraction.

#include <dekaf2/crypto/auth/ksession.h>

DEKAF2_NAMESPACE_BEGIN

class KSQL;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KSQL storage backend for KSession. The caller must keep the referenced
/// KSQL instance alive for the lifetime of this object. Not safe for
/// concurrent use across threads with the same KSQL — typically you would
/// give KSession its own KSQL connection.
class DEKAF2_PUBLIC KSessionKSQLStore : public KSession::Store
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// @param db reference to an open KSQL connection (caller manages lifetime)
	/// @param sTableName name of the sessions table (default "KSESSION_SESSIONS")
	explicit KSessionKSQLStore(KSQL& db,
	                           KString sTableName = "KSESSION_SESSIONS");

	bool              Initialize   () override;
	bool              Create       (const KSession::Record& Rec) override;
	bool              Lookup       (KStringView sToken, KSession::Record* pOut) override;
	bool              Touch        (KStringView sToken, KUnixTime tLastSeen) override;
	bool              Erase        (KStringView sToken) override;
	std::size_t       EraseAllFor  (KStringView sUsername) override;
	std::size_t       PurgeExpired (KUnixTime tOldestLastSeen, KUnixTime tOldestCreated) override;
	std::size_t       Count        () const override;
	KString           GetLastError () const override;

//------
private:
//------

	KSQL&   m_DB;
	KString m_sTableName;

}; // KSessionKSQLStore

DEKAF2_NAMESPACE_END
