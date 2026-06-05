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

/// @file ksessioncachingstore.h
/// Caching decorator for KSession::Store — keeps an in-RAM read cache over a
/// persistent backend and coalesces the high-frequency LastSeen writes.

#include <dekaf2/crypto/auth/ksession.h>
#include <dekaf2/threading/primitives/kthreadsafe.h>
#include <dekaf2/containers/associative/kassociative.h> // KUnorderedMap (heterogeneous KStringView lookup)
#include <mutex>
#include <memory>

DEKAF2_NAMESPACE_BEGIN

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Caching decorator around another KSession::Store.
///
/// Wraps a persistent backend (KSessionSQLiteStore, KSessionKSQLStore, or a
/// custom one) and adds two things the bare backends lack:
///
///  1. **A RAM read cache.** Lookup() serves from an in-process hash map
///     (a KUnorderedMap, so a KStringView lookup constructs no temporary key);
///     only a cache miss (e.g. the first request for a token after a restart)
///     falls through to the backend, which then populates the cache.
///     Subsequent validations of the same token are pure-RAM.
///
///  2. **Write-coalescing for Touch().** KSession::Validate() rewrites the
///     LastSeen timestamp on *every* authenticated request — for a service
///     with high-frequency polling endpoints that is one storage write per poll.
///     Since LastSeen feeds only the idle-timeout decision (measured in minutes-to-hours),
///     sub-second persistence precision is wasted I/O. This store updates LastSeen in
///     RAM immediately (so the idle-timeout check in Validate() stays exact) and marks the entry dirty;
///     the dirty values are flushed to the backend in a batch only when PurgeExpired()
///     runs (i.e. on the cadence of KSession's existing purge timer, default every
///     5 minutes) and once more in the destructor for a clean shutdown.
///
/// All other operations — Create (login), Erase (logout), EraseAllFor,
/// UpdateExtra (e.g. OAuth token rotation) — are **write-through**: they hit
/// the backend immediately, because they are infrequent and must survive a
/// restart promptly. The cache is updated in lock-step so it never diverges
/// from the backend for these fields.
///
/// @par Durability of LastSeen across a crash
/// Because LastSeen flushes lazily, an *unclean* shutdown can lose up to one
/// purge-interval's worth of LastSeen advances. The effect is benign and
/// always in the safe direction: on restart a session looks *slightly more
/// idle* than it really was, biasing toward requiring a fresh login rather
/// than keeping a stale session alive. Login / logout / Extra are never
/// affected (they are write-through).
///
/// @par Thread safety
/// All public methods are thread-safe. The hot path — Lookup() cache-hit and
/// Touch() — is pure RAM under a single mutex; only the infrequent
/// write-through operations and the periodic flush hold the lock across
/// backend I/O.
///
/// @par Memory footprint and cache sizing
/// The cache holds one entry per session it has seen (via Create or a Lookup
/// miss) and drops entries only on logout (Erase / EraseAllFor) and expiry
/// (PurgeExpired). It therefore converges to roughly the set of *currently
/// live* sessions — there is intentionally **no** size cap and **no** LRU.
/// That is deliberate for this access pattern: session validation has weak
/// temporal locality (each request touches one user's session), so a
/// capped/LRU cache smaller than the live set would mostly thrash
/// (evict → reload from the backend), and per-read LRU bookkeeping would tax
/// the very hot path we keep cheap. To avoid thrashing you would have to size
/// it ≈ the live set anyway — i.e. just cache everything, which is what this
/// store does.
///
/// Budget RAM accordingly: roughly (number of live sessions) × (record size).
/// For OIDC / BFF sessions the dominant term is the Extra payload, which holds
/// the access/refresh/id tokens (often several KB each). This assumes session
/// creation is authenticated and therefore operationally bounded. If a
/// deployment's live-session set is unbounded or will not fit in RAM, either
/// do not place this decorator in front of the disk backend (use the bare
/// backend), or extend it with a hard cap and a cheap eviction policy
/// (random / FIFO of *clean* entries — not move-to-front LRU, which is the
/// wrong fit here).
class DEKAF2_PUBLIC KSessionCachingStore : public KSession::Store
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	/// @param Backend the persistent store to cache in front of (ownership
	/// transferred). Must be non-null.
	explicit KSessionCachingStore(std::unique_ptr<KSession::Store> Backend);

	/// Flushes pending LastSeen updates to the backend before destruction.
	~KSessionCachingStore() override;

	bool              Initialize   () override;
	bool              Create       (const KSession::Record& Rec) override;
	bool              Lookup       (KStringView sToken, KSession::Record* pOut) override;
	bool              Touch        (KStringView sToken, KUnixTime tLastSeen) override;
	bool              UpdateExtra  (KStringView sToken, KStringView sExtra) override;
	bool              Erase        (KStringView sToken) override;
	std::size_t       EraseAllFor  (KStringView sUsername) override;
	std::size_t       ListFor      (KStringView sUsername, std::vector<KSession::Record>& Out) override;
	std::size_t       PurgeExpired (KUnixTime tOldestLastSeen, KUnixTime tOldestCreated) override;
	std::size_t       Count        () const override;
	KString           GetLastError () const override;

	/// Persist all pending (coalesced) LastSeen updates to the backend now.
	/// Called automatically by PurgeExpired() and the destructor; exposed so
	/// an application can force a flush before a planned shutdown.
	/// @returns the number of records flushed
	std::size_t       Flush        ();

//------
private:
//------

	struct CacheEntry
	{
		KSession::Record Rec;
		bool             bLastSeenDirty { false };
	};

	// The cache and the backend together form one coherent, mutex-guarded
	// state: every operation that touches the backend must also atomically
	// reconcile the cache, so both live behind a single lock. A plain
	// std::mutex is used deliberately — the hot path (Lookup + Touch) always
	// writes (Touch updates LastSeen), so a shared_mutex would buy no read
	// parallelism here, only overhead.
	struct SharedState
	{
		KUnorderedMap<KString, CacheEntry> Cache;
		std::unique_ptr<KSession::Store>   Backend;
	};

	// mutable so the const accessors (Count/GetLastError) can take the lock
	mutable KThreadSafe<SharedState, std::mutex> m_State;

}; // KSessionCachingStore

DEKAF2_NAMESPACE_END
