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
 //
 */

#pragma once

/// @file kwebpushksqldb.h
/// KSQL-based database backend for KWebPush

#include <dekaf2/web/push/kwebpush.h>

DEKAF2_NAMESPACE_BEGIN

class KSQL;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// KSQL database backend for KWebPush — supports MySQL/MariaDB, SQLite3,
/// and other databases through the KSQL abstraction layer.
/// The caller must keep the KSQL instance alive for the lifetime of this object.
class DEKAF2_PUBLIC KWebPushKSQLDB : public KWebPush::DB
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// @param db reference to an open KSQL connection (caller manages lifetime)
	KWebPushKSQLDB(KSQL& db);

	bool              CreateTables()                                              override;
	bool              StoreVAPIDKey(KStringView sKey, KStringView sValue)         override;
	KString           LoadVAPIDKey(KStringView sKey)                              override;
	bool              StoreSubscription(const KWebPush::Subscription& sub)        override;
	bool              RemoveSubscription(KStringView sEndpoint)                   override;
	bool              RemoveUserSubscriptions(KStringView sUser)                  override;
	std::vector<KWebPush::Subscription> GetSubscriptions(KStringView sUser = {})  override;
	bool              HasSubscriptions()                                          override;
	KString           GetLastError() const                                        override;

//------
private:
//------

	KSQL& m_DB;

}; // KWebPushKSQLDB

DEKAF2_NAMESPACE_END
