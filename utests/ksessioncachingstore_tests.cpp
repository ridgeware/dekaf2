#include "catch.hpp"
#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>
#include <dekaf2/crypto/auth/bits/ksessioncachingstore.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/time/clock/ktime.h>

using namespace dekaf2;

namespace {

// Build a session record with all fields set, timestamped at tWhen.
KSession::Record MakeRecord(KString sToken, KString sUser, KUnixTime tWhen)
{
	KSession::Record Rec;
	Rec.sToken     = std::move(sToken);
	Rec.sUsername  = std::move(sUser);
	Rec.tCreated   = tWhen;
	Rec.tLastSeen  = tWhen;
	Rec.sClientIP  = "1.2.3.4";
	Rec.sUserAgent = "curl/8.0";
	Rec.sExtra     = "{}";
	return Rec;
}

} // anonymous

TEST_CASE("KSessionCachingStore")
{
	SECTION("Touch is coalesced — backend is not written until Flush")
	{
		auto  backend  = std::make_unique<KSessionMemoryStore>();
		auto* pBackend = backend.get();   // borrowed view to inspect persistence

		KSessionCachingStore cache(std::move(backend));
		REQUIRE ( cache.Initialize() );

		auto tNow = KUnixTime::now();
		REQUIRE ( cache.Create(MakeRecord("tok1", "alice", tNow)) );

		// Create is write-through: backend already has the record
		KSession::Record Direct;
		REQUIRE ( pBackend->Lookup("tok1", &Direct) );
		auto tCreatedLastSeen = Direct.tLastSeen;

		// Touch with a clearly-later timestamp
		auto tLater = tNow + KDuration(std::chrono::hours(1));
		REQUIRE ( cache.Touch("tok1", tLater) );

		// the cache reflects the new last_seen immediately
		KSession::Record Cached;
		REQUIRE ( cache.Lookup("tok1", &Cached) );
		CHECK ( Cached.tLastSeen == tLater );

		// but the BACKEND still holds the old value — the write was coalesced
		REQUIRE ( pBackend->Lookup("tok1", &Direct) );
		CHECK ( Direct.tLastSeen == tCreatedLastSeen );

		// Flush → the one dirty record is persisted
		CHECK ( cache.Flush() == 1 );
		REQUIRE ( pBackend->Lookup("tok1", &Direct) );
		CHECK ( Direct.tLastSeen == tLater );

		// nothing dirty any more
		CHECK ( cache.Flush() == 0 );
	}

	SECTION("Create / UpdateExtra / Erase are write-through")
	{
		auto  backend  = std::make_unique<KSessionMemoryStore>();
		auto* pBackend = backend.get();
		KSessionCachingStore cache(std::move(backend));
		REQUIRE ( cache.Initialize() );

		auto tNow = KUnixTime::now();
		REQUIRE ( cache.Create(MakeRecord("tok1", "alice", tNow)) );

		// UpdateExtra reaches the backend at once (OAuth token rotation case)
		REQUIRE ( cache.UpdateExtra("tok1", R"({"role":3})") );
		KSession::Record Direct;
		REQUIRE ( pBackend->Lookup("tok1", &Direct) );
		CHECK ( Direct.sExtra == R"({"role":3})" );

		// the cache mirrors the new Extra too
		KSession::Record Cached;
		REQUIRE ( cache.Lookup("tok1", &Cached) );
		CHECK ( Cached.sExtra == R"({"role":3})" );

		// Erase reaches the backend at once (logout case)
		REQUIRE ( cache.Erase("tok1") );
		CHECK_FALSE ( pBackend->Lookup("tok1", nullptr) );
		CHECK_FALSE ( cache.Lookup("tok1", nullptr) );
	}

	SECTION("Read cache loads from the backend after a cold start")
	{
		// pre-populate a backend, then wrap a *fresh* caching store around it —
		// simulates a process restart where the cache begins empty.
		auto backend = std::make_unique<KSessionMemoryStore>();
		REQUIRE ( backend->Initialize() );
		auto tNow = KUnixTime::now();
		REQUIRE ( backend->Create(MakeRecord("tok1", "alice", tNow)) );

		KSessionCachingStore cache(std::move(backend));
		REQUIRE ( cache.Initialize() );

		// cold cache, but Lookup must resolve the session via the backend
		KSession::Record Rec;
		REQUIRE ( cache.Lookup("tok1", &Rec) );
		CHECK ( Rec.sUsername == "alice" );

		// a second lookup is now a cache hit and still consistent
		KSession::Record Rec2;
		REQUIRE ( cache.Lookup("tok1", &Rec2) );
		CHECK ( Rec2.sUsername == "alice" );
	}

	SECTION("PurgeExpired flushes dirty, then purges and evicts from cache")
	{
		auto  backend  = std::make_unique<KSessionMemoryStore>();
		auto* pBackend = backend.get();
		KSessionCachingStore cache(std::move(backend));
		REQUIRE ( cache.Initialize() );

		auto tNow = KUnixTime::now();
		REQUIRE ( cache.Create(MakeRecord("fresh", "alice", tNow)) );
		REQUIRE ( cache.Create(MakeRecord("stale", "bob",   tNow - KDuration(std::chrono::hours(10)))) );

		// idle cutoff: not-seen-within-the-last-hour is expired.
		auto tIdleCutoff = tNow - KDuration(std::chrono::hours(1));
		auto tAbsCutoff  = KUnixTime{}; // epoch == "absolute timeout disabled"

		auto iPurged = cache.PurgeExpired(tIdleCutoff, tAbsCutoff);
		CHECK ( iPurged == 1 );

		// "stale" is gone from both cache and backend
		CHECK_FALSE ( cache.Lookup("stale", nullptr) );
		CHECK_FALSE ( pBackend->Lookup("stale", nullptr) );

		// "fresh" survives
		CHECK ( cache.Lookup("fresh", nullptr) );
	}

	SECTION("Dirty last_seen is flushed before the purge decision is made")
	{
		// A session created stale but Touch()ed recently must NOT be purged:
		// the coalesced last_seen has to reach the backend before it decides.
		auto  backend  = std::make_unique<KSessionMemoryStore>();
		auto* pBackend = backend.get();
		KSessionCachingStore cache(std::move(backend));
		REQUIRE ( cache.Initialize() );

		auto tNow   = KUnixTime::now();
		auto tStale = tNow - KDuration(std::chrono::hours(10));
		// last_seen 10h ago on disk
		REQUIRE ( cache.Create(MakeRecord("tok", "alice", tStale)) );
		// but a fresh Touch in RAM (dirty, not yet persisted)
		REQUIRE ( cache.Touch("tok", tNow) );

		// backend still shows the stale last_seen at this point
		KSession::Record Direct;
		REQUIRE ( pBackend->Lookup("tok", &Direct) );
		CHECK ( Direct.tLastSeen == tStale );

		// purge with a 1h idle cutoff — because PurgeExpired flushes first,
		// the fresh last_seen wins and the session is kept.
		auto iPurged = cache.PurgeExpired(tNow - KDuration(std::chrono::hours(1)), KUnixTime{});
		CHECK ( iPurged == 0 );
		CHECK ( cache.Lookup("tok", nullptr) );
	}

	SECTION("End-to-end through KSession on a caching memory backend")
	{
		KSession::Config cfg;
		cfg.bSecure       = false;
		cfg.PurgeInterval = KDuration::zero();   // deterministic: no bg timer
		cfg.IdleTimeout   = std::chrono::hours(1);

		auto backend = std::make_unique<KSessionMemoryStore>();
		KSession Session(std::make_unique<KSessionCachingStore>(std::move(backend)), cfg);
		Session.SetAuthenticator([](KStringView u, KStringView p)
		{
			return u == "alice" && p == "wonder";
		});

		auto sToken = Session.Login("alice", "wonder", "1.2.3.4", "ua");
		REQUIRE ( !sToken.empty() );

		KSession::Record Rec;
		CHECK ( Session.Validate(sToken, &Rec) );
		CHECK ( Rec.sUsername == "alice" );

		CHECK ( Session.Logout(sToken) );
		CHECK_FALSE ( Session.Validate(sToken) );
	}
}
