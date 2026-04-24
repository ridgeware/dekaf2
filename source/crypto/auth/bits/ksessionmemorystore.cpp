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

#include <dekaf2/crypto/auth/bits/ksessionmemorystore.h>

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
bool KSessionMemoryStore::Initialize()
//-----------------------------------------------------------------------------
{
	// nothing to do — the container is ready-to-use
	return true;

} // Initialize

//-----------------------------------------------------------------------------
bool KSessionMemoryStore::Create(const KSession::Record& Rec)
//-----------------------------------------------------------------------------
{
	if (Rec.sToken.empty())
	{
		m_sError = "empty token";
		return false;
	}

	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto result = m_Sessions.emplace(Rec.sToken, Rec);

	if (!result.second)
	{
		m_sError = "token collision (should be impossible with 256-bit random tokens)";
		return false;
	}

	return true;

} // Create

//-----------------------------------------------------------------------------
bool KSessionMemoryStore::Lookup(KStringView sToken, KSession::Record* pOut)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	// KString lookup on unordered_map<KString, ...> — use KString(sToken)
	auto it = m_Sessions.find(KString(sToken));

	if (it == m_Sessions.end())
	{
		return false;
	}

	if (pOut)
	{
		*pOut = it->second;
	}

	return true;

} // Lookup

//-----------------------------------------------------------------------------
bool KSessionMemoryStore::Touch(KStringView sToken, KUnixTime tLastSeen)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	auto it = m_Sessions.find(KString(sToken));

	if (it == m_Sessions.end())
	{
		return false;
	}

	it->second.tLastSeen = tLastSeen;
	return true;

} // Touch

//-----------------------------------------------------------------------------
bool KSessionMemoryStore::Erase(KStringView sToken)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	return m_Sessions.erase(KString(sToken)) > 0;

} // Erase

//-----------------------------------------------------------------------------
std::size_t KSessionMemoryStore::EraseAllFor(KStringView sUsername)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	std::size_t iErased = 0;

	for (auto it = m_Sessions.begin(); it != m_Sessions.end(); )
	{
		if (it->second.sUsername == sUsername)
		{
			it = m_Sessions.erase(it);
			++iErased;
		}
		else
		{
			++it;
		}
	}

	return iErased;

} // EraseAllFor

//-----------------------------------------------------------------------------
std::size_t KSessionMemoryStore::PurgeExpired(KUnixTime tOldestLastSeen,
                                              KUnixTime tOldestCreated)
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	std::size_t iErased = 0;

	// A cutoff at epoch (tOldestLastSeen == KUnixTime{}) means "disabled"
	// — only purge when the corresponding timeout is configured.
	const bool bCheckIdle = tOldestLastSeen.to_time_t() > 0;
	const bool bCheckAbs  = tOldestCreated.to_time_t()  > 0;

	for (auto it = m_Sessions.begin(); it != m_Sessions.end(); )
	{
		bool bExpired = false;

		if (bCheckIdle && it->second.tLastSeen < tOldestLastSeen)
		{
			bExpired = true;
		}
		else if (bCheckAbs && it->second.tCreated < tOldestCreated)
		{
			bExpired = true;
		}

		if (bExpired)
		{
			it = m_Sessions.erase(it);
			++iErased;
		}
		else
		{
			++it;
		}
	}

	return iErased;

} // PurgeExpired

//-----------------------------------------------------------------------------
std::size_t KSessionMemoryStore::Count() const
//-----------------------------------------------------------------------------
{
	std::lock_guard<std::mutex> Lock(m_Mutex);

	return m_Sessions.size();

} // Count

DEKAF2_NAMESPACE_END
