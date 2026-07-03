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

#include <dekaf2/crypto/auth/bits/ksessioncachingstore.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KSessionCachingStore::KSessionCachingStore(std::unique_ptr<KSession::Store> Backend)
//-----------------------------------------------------------------------------
{
	// not set through the init list: gcc 7 warns on the aggregate's {} for the
	// cache map selecting boost multi_index's explicit default constructor
	m_State.unique()->Backend = std::move(Backend);

} // ctor

//-----------------------------------------------------------------------------
KSessionCachingStore::~KSessionCachingStore()
//-----------------------------------------------------------------------------
{
	// persist any pending LastSeen advances on clean shutdown
	Flush();

} // dtor

//-----------------------------------------------------------------------------
bool KSessionCachingStore::Initialize()
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	if (!State->Backend)
	{
		return false;
	}

	return State->Backend->Initialize();

} // Initialize

//-----------------------------------------------------------------------------
bool KSessionCachingStore::Create(const KSession::Record& Rec)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	// write-through: a freshly-created session must survive a restart at once
	if (!State->Backend->Create(Rec))
	{
		return false;
	}

	// mirror into the cache, not dirty (it is already persisted verbatim)
	State->Cache[Rec.sToken] = CacheEntry{ Rec, /*bLastSeenDirty=*/false };

	return true;

} // Create

//-----------------------------------------------------------------------------
bool KSessionCachingStore::Lookup(KStringView sToken, KSession::Record* pOut)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	auto it = State->Cache.find(sToken);

	if (it != State->Cache.end())
	{
		// cache hit — pure RAM, the hot path
		if (pOut)
		{
			*pOut = it->second.Rec;
		}
		return true;
	}

	// cache miss — fall through to the backend (e.g. first touch of a
	// pre-restart session) and populate the cache for next time. The
	// loaded record is not dirty: its LastSeen matches what is on disk.
	KSession::Record Rec;

	if (!State->Backend->Lookup(sToken, &Rec))
	{
		return false;
	}

	auto res = State->Cache.emplace(Rec.sToken, CacheEntry{ Rec, /*bLastSeenDirty=*/false });

	if (pOut)
	{
		*pOut = res.first->second.Rec;
	}

	return true;

} // Lookup

//-----------------------------------------------------------------------------
bool KSessionCachingStore::Touch(KStringView sToken, KUnixTime tLastSeen)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	auto it = State->Cache.find(sToken);

	if (it != State->Cache.end())
	{
		// the hot path: update RAM and defer the disk write. The in-RAM
		// LastSeen is authoritative for the idle-timeout check in
		// KSession::Validate(), so the timeout stays exact despite the
		// deferred persistence.
		it->second.Rec.tLastSeen  = tLastSeen;
		it->second.bLastSeenDirty = true;
		return true;
	}

	// Not cached. KSession::Validate() Lookup()s (which populates the cache)
	// right before it Touch()es, so a miss here is rare: it only happens when
	// Touch is called without a prior Lookup, or when the entry was evicted in
	// the gap between the two (the lock is released between the two Store calls),
	// e.g. by a concurrent PurgeExpired/Erase. Write straight through so the
	// LastSeen update is never lost — the cost is one synchronous backend write
	// on a cold, rare path, which is acceptable.
	return State->Backend->Touch(sToken, tLastSeen);

} // Touch

//-----------------------------------------------------------------------------
bool KSessionCachingStore::UpdateExtra(KStringView sToken, KStringView sExtra)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	// write-through: Extra typically carries rotated OAuth/OIDC tokens that
	// must not be lost across a restart
	if (!State->Backend->UpdateExtra(sToken, sExtra))
	{
		return false;
	}

	auto it = State->Cache.find(sToken);

	if (it != State->Cache.end())
	{
		it->second.Rec.sExtra = sExtra;
	}

	return true;

} // UpdateExtra

//-----------------------------------------------------------------------------
bool KSessionCachingStore::Erase(KStringView sToken)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	// write-through: a logout must take effect across a restart immediately.
	// Drop the cache entry even if it had a pending dirty LastSeen — the
	// session is gone, so the unflushed timestamp is irrelevant.
	const bool bErased = State->Backend->Erase(sToken);

	auto it = State->Cache.find(sToken);
	if (it != State->Cache.end())
	{
		State->Cache.erase(it);
	}

	return bErased;

} // Erase

//-----------------------------------------------------------------------------
std::size_t KSessionCachingStore::EraseAllFor(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	const std::size_t iErased = State->Backend->EraseAllFor(sUsername);

	for (auto it = State->Cache.begin(); it != State->Cache.end(); )
	{
		if (it->second.Rec.sUsername == sUsername)
		{
			it = State->Cache.erase(it);
		}
		else
		{
			++it;
		}
	}

	return iErased;

} // EraseAllFor

//-----------------------------------------------------------------------------
std::size_t KSessionCachingStore::ListFor(KStringView sUsername, std::vector<KSession::Record>& Out)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	// the backend holds every session (Create writes through), so it is the
	// authoritative source for the full list
	const std::size_t iBefore = Out.size();
	State->Backend->ListFor(sUsername, Out);

	// overlay any coalesced (not-yet-flushed) LastSeen updates the cache holds,
	// so the listing reflects the most recent activity even before a Flush
	for (std::size_t i = iBefore; i < Out.size(); ++i)
	{
		auto it = State->Cache.find(Out[i].sToken);
		if (it != State->Cache.end() && it->second.bLastSeenDirty
		    && it->second.Rec.tLastSeen > Out[i].tLastSeen)
		{
			Out[i].tLastSeen = it->second.Rec.tLastSeen;
		}
	}

	return Out.size() - iBefore;

} // ListFor

//-----------------------------------------------------------------------------
std::size_t KSessionCachingStore::PurgeExpired(KUnixTime tOldestLastSeen,
                                               KUnixTime tOldestCreated)
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	// 1) flush pending LastSeen so the backend purges against current data.
	//    This is where the coalesced writes land — on the cadence of
	//    KSession's purge timer, not per request.
	for (auto& Pair : State->Cache)
	{
		auto& Entry = Pair.second;
		if (Entry.bLastSeenDirty)
		{
			State->Backend->Touch(Pair.first, Entry.Rec.tLastSeen);
			Entry.bLastSeenDirty = false;
		}
	}

	// 2) let the backend purge authoritatively (deletes from persistent store)
	const std::size_t iPurged = State->Backend->PurgeExpired(tOldestLastSeen, tOldestCreated);

	// 3) evict the same expired entries from the cache, applying the identical
	//    predicate the backends use (epoch cutoff == "timeout disabled").
	const bool bCheckIdle = tOldestLastSeen.ok();   // the epoch == "timeout disabled"
	const bool bCheckAbs  = tOldestCreated.ok();

	for (auto it = State->Cache.begin(); it != State->Cache.end(); )
	{
		const auto& Rec = it->second.Rec;

		const bool bExpired =
			(bCheckIdle && Rec.tLastSeen < tOldestLastSeen) ||
			(bCheckAbs  && Rec.tCreated  < tOldestCreated);

		if (bExpired)
		{
			it = State->Cache.erase(it);
		}
		else
		{
			++it;
		}
	}

	return iPurged;

} // PurgeExpired

//-----------------------------------------------------------------------------
std::size_t KSessionCachingStore::Count() const
//-----------------------------------------------------------------------------
{
	// the backend is authoritative — the cache may hold only a lazily-loaded
	// subset of the live sessions
	auto State = m_State.unique();
	return State->Backend ? State->Backend->Count() : 0;

} // Count

//-----------------------------------------------------------------------------
KString KSessionCachingStore::GetLastError() const
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();
	return State->Backend ? State->Backend->GetLastError()
	                      : KString("KSessionCachingStore: no backend");

} // GetLastError

//-----------------------------------------------------------------------------
std::size_t KSessionCachingStore::Flush()
//-----------------------------------------------------------------------------
{
	auto State = m_State.unique();

	std::size_t iFlushed = 0;

	for (auto& Pair : State->Cache)
	{
		auto& Entry = Pair.second;
		if (Entry.bLastSeenDirty)
		{
			State->Backend->Touch(Pair.first, Entry.Rec.tLastSeen);
			Entry.bLastSeenDirty = false;
			++iFlushed;
		}
	}

	return iFlushed;

} // Flush

DEKAF2_NAMESPACE_END
