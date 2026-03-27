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

/// @file kconnectionlimiter.h
/// Per-key concurrent connection limiter with RAII guard

#include "kdefinitions.h"
#include "kstring.h"
#include "kstringview.h"
#include "kerror.h"
#include "kassociative.h"
#include <mutex>

DEKAF2_NAMESPACE_BEGIN

class KConnectionLimiter;

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Per-key concurrent connection limiter. Thread-safe. When iMaxConnections
/// is 0 (the default), the limiter is completely disabled and Acquire() is
/// a no-op with zero cost. Returns an RAII Guard that automatically releases
/// the connection slot on destruction.
class DEKAF2_PUBLIC KConnectionLimiter : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//------
public:
//------

	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// RAII guard that releases a connection slot on destruction.
	/// A valid guard evaluates to true, an invalid one (limit exceeded) to false.
	class Guard
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{

	//------
	public:
	//------

		/// default ctor - creates an empty (valid but no-op) guard
		Guard() = default;

		/// move ctor
		Guard(Guard&& other) noexcept
		: m_Limiter(other.m_Limiter)
		, m_sKey(std::move(other.m_sKey))
		, m_bIsValid(other.m_bIsValid)
		{
			other.m_Limiter = nullptr;
			other.m_bIsValid = false;
		}

		/// move assignment
		Guard& operator=(Guard&& other) noexcept
		{
			if (this != &other)
			{
				Release();
				m_Limiter  = other.m_Limiter;
				m_sKey     = std::move(other.m_sKey);
				m_bIsValid = other.m_bIsValid;
				other.m_Limiter  = nullptr;
				other.m_bIsValid = false;
			}
			return *this;
		}

		/// dtor - releases the connection slot
		~Guard() { Release(); }

		/// @return true if this guard holds a valid connection slot
		explicit operator bool() const { return m_bIsValid; }

		Guard(const Guard&) = delete;
		Guard& operator=(const Guard&) = delete;

	//------
	private:
	//------

		friend class KConnectionLimiter;

		Guard(KConnectionLimiter* limiter, KString sKey, bool bValid)
		: m_Limiter(limiter)
		, m_sKey(std::move(sKey))
		, m_bIsValid(bValid)
		{
		}

		void Release();

		KConnectionLimiter* m_Limiter  { nullptr };
		KString             m_sKey;
		bool                m_bIsValid { false };

	}; // Guard

	/// default constructor - limiter is disabled (zero cost)
	KConnectionLimiter() = default;

	/// construct an active connection limiter
	/// @param iMaxConnectionsPerKey maximum concurrent connections per key
	KConnectionLimiter(uint16_t iMaxConnectionsPerKey);

	/// check if this limiter is enabled
	DEKAF2_NODISCARD
	bool IsEnabled() const { return m_iMaxConnections > 0; }

	/// acquire a connection slot for the given key.
	/// @return a Guard that is valid (true) if under limit, invalid (false) if limit exceeded.
	/// When invalid, the error message is available via GetLastError().
	/// With SetThrowOnError(true), throws KException instead of returning an invalid guard.
	/// @param sKey the connection key (e.g. client IP address)
	DEKAF2_NODISCARD
	Guard Acquire(KStringView sKey)
	{
		if (!IsEnabled()) return Guard();
		return AcquireImpl(sKey);
	}

	/// @return the number of tracked keys
	DEKAF2_NODISCARD
	std::size_t GetKeyCount() const;

	/// @return the current connection count for a specific key
	DEKAF2_NODISCARD
	uint16_t GetConnectionCount(KStringView sKey) const;

	/// @return the total number of active connections across all keys
	DEKAF2_NODISCARD
	std::size_t GetTotalConnections() const;

//------
private:
//------

	friend class Guard;

	Guard AcquireImpl(KStringView sKey);
	void  ReleaseImpl(const KString& sKey);

	mutable std::mutex             m_Mutex;
	KUnorderedMap<KString, uint16_t> m_Connections;
	uint16_t                       m_iMaxConnections { 0 };

}; // KConnectionLimiter

DEKAF2_NAMESPACE_END
