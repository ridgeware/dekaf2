/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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

#include <dekaf2/crypto/random/krandom.h>
#include <dekaf2/core/logging/klog.h>
#include <random>
#include <cstdlib>

#if DEKAF2_HAS_GETRANDOM
	#include <limits>
	#include <sys/random.h>
#endif

#if !DEKAF2_HAS_ARC4RANDOM && !DEKAF2_HAS_GETRANDOM
	// no platform CSPRNG primitive available (e.g. an old glibc lacking
	// getrandom() and without arc4random): fall back to OpenSSL's RAND_bytes,
	// which dekaf2 always links. This keeps every random path cryptographically
	// secure and lets us drop the std::mt19937 fallback entirely.
	#include <openssl/rand.h>
#endif

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//---------------------------------------------------------------------------
void kSetRandomSeed()
//---------------------------------------------------------------------------
{
	std::random_device RandDevice;

	srand(RandDevice());
#ifndef DEKAF2_IS_WINDOWS
	srand48(RandDevice());
#ifdef DEKAF2_IS_MACOS
	srandomdev();
#else
	srandom(RandDevice());
#endif
#endif

} // kSetRandomSeed

} // end of namespace detail

//-----------------------------------------------------------------------------
uint32_t kRandom32()
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_ARC4RANDOM
	return arc4random();
#elif DEKAF2_HAS_GETRANDOM
	// zero-initialize so a (rare) CSPRNG failure yields a DEFINED value rather than an
	// indeterminate stack read (UB / potential stale-stack leak).
	uint32_t iValue = 0;
	if (getrandom(&iValue, sizeof(iValue), 0) != static_cast<ssize_t>(sizeof(iValue)))
	{
		kDebug(1, "getrandom() failed");
	}
	return iValue;
#else
	uint32_t iValue = 0;
	if (1 != ::RAND_bytes(reinterpret_cast<unsigned char*>(&iValue), static_cast<int>(sizeof(iValue))))
	{
		kDebug(1, "RAND_bytes() failed");
	}
	return iValue;
#endif

} // kRandom32

//-----------------------------------------------------------------------------
uint64_t kRandom64()
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_ARC4RANDOM
	return static_cast<uint64_t>(arc4random()) | static_cast<uint64_t>(arc4random()) << 32;
#elif DEKAF2_HAS_GETRANDOM
	// zero-initialize so a (rare) CSPRNG failure yields a DEFINED value rather than an
	// indeterminate stack read (UB / potential stale-stack leak).
	uint64_t iValue = 0;
	if (getrandom(&iValue, sizeof(iValue), 0) != static_cast<ssize_t>(sizeof(iValue)))
	{
		kDebug(1, "getrandom() failed");
	}
	return iValue;
#else
	uint64_t iValue = 0;
	if (1 != ::RAND_bytes(reinterpret_cast<unsigned char*>(&iValue), static_cast<int>(sizeof(iValue))))
	{
		kDebug(1, "RAND_bytes() failed");
	}
	return iValue;
#endif

} // kRandom64

//-----------------------------------------------------------------------------
uint32_t kRandom(uint32_t iMin, uint32_t iMax)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_ARC4RANDOM
	return arc4random_uniform(iMax - iMin) + iMin;
#else
	struct RandomGenerator
	{
		using result_type = uint32_t;
		result_type operator()() const noexcept { return kRandom32(); }
		static constexpr result_type min() noexcept { return std::numeric_limits<result_type>::min(); }
		static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }
	};
	RandomGenerator rg;
	std::uniform_int_distribution<uint32_t> uniform_dist(iMin, iMax);
	return uniform_dist(rg);
#endif

} // kRandom

//-----------------------------------------------------------------------------
bool kGetRandom(void* buf, std::size_t iCount)
//-----------------------------------------------------------------------------
{
	if (!buf)
	{
		return false;
	}

#if DEKAF2_HAS_ARC4RANDOM

	arc4random_buf(buf, iCount);

#elif DEKAF2_HAS_GETRANDOM

	if (iCount <= 256)
	{
		getrandom(buf, iCount, 0);
	}
	else
	{
		for(;;)
		{
			ssize_t iGot = getrandom(buf, iCount, 0);

			if (iGot == static_cast<ssize_t>(iCount))
			{
				return true;
			}
			else if (iGot < 0)
			{
				if (errno == EAGAIN || errno == EINTR)
				{
					continue;
				}
				return false;
			}
			else if (iGot < static_cast<ssize_t>(iCount))
			{
				iCount -= iGot;
				buf = (void*)((char*)buf + iGot);
			}
			else
			{
				return false;
			}
		}
	}

#else

	// OpenSSL's CSPRNG fills the whole buffer in one call - no 32-bit looping,
	// and (unlike getrandom) no 256-byte chunk limit to work around.
	if (1 != ::RAND_bytes(static_cast<unsigned char*>(buf), static_cast<int>(iCount)))
	{
		return false;
	}

#endif

	return true;

} // kGetRandom

//-----------------------------------------------------------------------------
KString kGetRandom(std::size_t iCount)
//-----------------------------------------------------------------------------
{
	KString sRandom(iCount, '\0');

	// fail CLOSED: if the CSPRNG fails we must NOT hand back the fixed-length zero
	// string we just allocated - that would be a fully predictable "secret" (the same
	// base64 every time). Return empty instead, so a caller minting a token/handle gets
	// an obviously-unusable value rather than a constant one. (Only the OpenSSL /
	// getrandom paths can fail; arc4random cannot.)
	if (iCount != 0 && !kGetRandom(&sRandom[0], iCount))
	{
		kDebug(1, "CSPRNG failed - returning empty string");
		sRandom.clear();
	}

	return sRandom;

} // kGetRandom

DEKAF2_NAMESPACE_END
