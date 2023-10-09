/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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

#include "kcppcompat.h"
#include "kstring_view.h"
#include "simd/kfindfirstof.h"
#ifdef DEKAF2_X86_64
#ifdef DEKAF2_HAS_MINIFOLLY
#include "../dekaf2.h"
#endif
#endif
#ifdef DEKAF2_HAS_OWN_STRING_VIEW
#include "../kcrashexit.h"
#endif

namespace dekaf2 {

#ifndef __GLIBC__

//-----------------------------------------------------------------------------
void* memrchr(const void* s, int c, size_t n)
//-----------------------------------------------------------------------------
{
#if DEKAF2_FIND_FIRST_OF_USE_SIMD
#ifdef DEKAF2_HAS_MINIFOLLY
	static bool has_sse42 = dekaf2::Dekaf::getInstance().GetCpuId().sse42();
	if (DEKAF2_LIKELY(has_sse42))
#endif
	{
		const char* p = static_cast<const char*>(s);
		char ch = static_cast<char>(c);
		size_t pos = dekaf2::detail::sse::kFindLastOf(dekaf2::KStringView(p, n), dekaf2::KStringView(&ch, 1));
		if (pos != dekaf2::KStringView::npos)
		{
			return const_cast<char*>(p + pos);
		}
		return nullptr;
	}
#endif

	auto p = static_cast<const unsigned char*>(s);
	for (p += n; n > 0; --n)
	{
		if (*--p == c)
		{
			return const_cast<unsigned char*>(p);
		}
	}
	return nullptr;

} // memrchr

//-----------------------------------------------------------------------------
void* memmem(const void* haystack, size_t iHaystackSize, const void *needle, size_t iNeedleSize)
//-----------------------------------------------------------------------------
{
	if (!iNeedleSize || !needle || !haystack)
	{
		// an empty needle matches the start of any haystack
		return const_cast<void*>(haystack);
	}

	auto pHaystack = static_cast<const char*>(haystack);
	auto pNeedle   = static_cast<const char*>(needle);

	for(;iNeedleSize <= iHaystackSize;)
	{
		auto pFound = static_cast<const char*>(std::memchr(pHaystack, pNeedle[0], (iHaystackSize - iNeedleSize) + 1));

		if (DEKAF2_UNLIKELY(!pFound))
		{
			return nullptr;
		}

		// due to aligned loads it is faster to compare the full needle again
		if (std::memcmp(pFound, pNeedle, iNeedleSize) == 0)
		{
			return const_cast<char*>(pFound);
		}

		auto iAdvance = static_cast<size_t>(pFound - pHaystack) + 1;

		pHaystack     += iAdvance;
		iHaystackSize -= iAdvance;
	}

	// no match
	return nullptr;

} // memmem

#endif // of __GLIBC__

#ifdef DEKAF2_HAS_OWN_STRING_VIEW

namespace detail {
namespace stringview {

#ifndef NDEBUG
//-----------------------------------------------------------------------------
void svFailedAssert (const char* sCrashMessage)
//-----------------------------------------------------------------------------
{
	detail::kFailedAssert (sCrashMessage);

} // svFailedAssert
#endif

} // end of namespace stringview
} // end of namespace detail

#endif

} // end of namespace dekaf2
