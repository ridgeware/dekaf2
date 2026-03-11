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

#include "krandom.h"
#include <random>
#include <cstdlib>

#if DEKAF2_HAS_GETRANDOM
	#include <sys/random.h>
#endif

#if !DEKAF2_HAS_ARC4RANDOM
	#include "kthreadsafe.h"
#endif

DEKAF2_NAMESPACE_BEGIN

namespace detail {

#if !DEKAF2_HAS_ARC4RANDOM
KThreadSafe<std::mt19937> g_Dekaf2Random;
#endif

//---------------------------------------------------------------------------
void kSetRandomSeed()
//---------------------------------------------------------------------------
{
	std::random_device RandDevice;

#if !DEKAF2_HAS_ARC4RANDOM
	g_Dekaf2Random.unique()->seed(RandDevice());
#endif

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
	uint32_t iValue;
	getrandom(&iValue, sizeof(iValue), 0);
	return iValue;
#else
	return detail::g_Dekaf2Random.unique().get()();
#endif

} // kRandom32

//-----------------------------------------------------------------------------
uint64_t kRandom64()
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_ARC4RANDOM
	return static_cast<uint64_t>(arc4random()) | static_cast<uint64_t>(arc4random()) << 32;
#elif DEKAF2_HAS_GETRANDOM
	uint64_t iValue;
	getrandom(&iValue, sizeof(iValue), 0);
	return iValue;
#else
	auto rand = detail::g_Dekaf2Random.unique();
	return static_cast<uint64_t>((*rand)()) | static_cast<uint64_t>((*rand)()) << 32;
#endif

} // kRandom64

//-----------------------------------------------------------------------------
uint32_t kRandom(uint32_t iMin, uint32_t iMax)
//-----------------------------------------------------------------------------
{
#if DEKAF2_HAS_ARC4RANDOM
	auto iRange = iMax - iMin;
	return arc4random_uniform(iRange) + iMin;
#else
	std::uniform_int_distribution<uint32_t> uniform_dist(iMin, iMax);
	return uniform_dist(detail::g_Dekaf2Random.unique().get());
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

	auto* it = static_cast<uint8_t*>(buf);
	auto* ie = it + iCount;

	while (ie - it >= 4)
	{
		uint32_t r = kRandom32();
		*it++ = r & 0xff;
		*it++ = (r >>= 8) & 0xff;
		*it++ = (r >>= 8) & 0xff;
		*it++ = (r >>= 8) & 0xff;
	}

	if (ie - it > 0)
	{
		uint32_t r = kRandom32();
		while (ie - it > 0)
		{
			*it++ = r & 0xff;
			r >>= 8;
		}
	}

#endif

	return true;

} // kGetRandom

//-----------------------------------------------------------------------------
KString kGetRandom(std::size_t iCount)
//-----------------------------------------------------------------------------
{
	KString sRandom(iCount, '\0');
	kGetRandom(&sRandom[0], iCount);
	return sRandom;

} // kGetRandom

DEKAF2_NAMESPACE_END
