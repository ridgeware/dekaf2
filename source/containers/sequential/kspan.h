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

#pragma once

/// @file kspan.h
/// Lightweight non-owning view over a contiguous sequence of elements.
/// Equivalent to std::span (C++20) but available from C++14 onwards.
/// When compiled with C++20 or later and std::span is present, KSpan
/// is a thin alias for std::span — zero overhead, full interoperability.

#include <dekaf2/core/types/kdefinitions.h>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <iterator>
#include <array>

#if DEKAF2_HAS_CPP_20
	#if DEKAF2_HAS_INCLUDE(<span>)
		#include <span>
		#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L
			#define DEKAF2_HAS_STD_SPAN 1
		#elif DEKAF2_IS_MACOS
			// macOS libc++ may not define __cpp_lib_span even with C++20
			#define DEKAF2_HAS_STD_SPAN 1
		#endif
	#endif
#endif

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup containers_sequential
/// @{

#ifdef DEKAF2_HAS_STD_SPAN

// ============================================================================
// C++20 path: alias the standard implementation
// ============================================================================

/// Sentinel value indicating a span whose size is determined at run time.
using std::dynamic_extent;

/// Non-owning view over a contiguous sequence of elements.
/// This is a direct alias for std::span when compiled with C++20.
template <typename T, std::size_t Extent = dynamic_extent>
using KSpan = std::span<T, Extent>;

#else // !DEKAF2_HAS_STD_SPAN

// ============================================================================
// C++14/17 path: dekaf2 implementation
// ============================================================================

/// Sentinel value indicating a span whose size is determined at run time.
/// Equivalent to std::dynamic_extent (the largest representable std::size_t).
inline constexpr std::size_t dynamic_extent = static_cast<std::size_t>(-1);

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// @brief Non-owning view over a contiguous sequence of elements.
///
/// KSpan is a lightweight, non-owning reference to a contiguous block of
/// memory. It stores a pointer and a size (two machine words) and provides
/// safe, bounds-aware access with zero overhead compared to raw pointers.
///
/// KSpan can be implicitly constructed from:
///   - a pointer and a count
///   - two pointers (begin/end)
///   - a C-style array
///   - a std::array
///   - any contiguous container with .data() and .size() (e.g. std::vector,
///     std::string, dekaf2::KString)
///
/// @tparam T       element type (may be const-qualified for read-only views)
/// @tparam Extent  static extent (number of elements known at compile time),
///                 or dynamic_extent (default) for a run-time-sized span.
///                 The static extent is accepted for API compatibility with
///                 std::span but does not change the internal representation.
///
/// Example usage:
/// @code
///   std::vector<int> v = {1, 2, 3, 4, 5};
///   KSpan<const int> s = v;                // implicit from vector
///   KSpan<const int> first3 = s.first(3);  // {1, 2, 3}
///
///   std::array<uint8_t, 12> hdr;
///   KSpan<uint8_t> h = hdr;                // implicit from std::array
///
///   void Send(KSpan<const uint8_t> data);  // function parameter — accepts
///   Send(v);                               // vector, array, or pointer+size
/// @endcode
template <typename T, std::size_t Extent = dynamic_extent>
class KSpan
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	// --- standard type aliases ------------------------------------------------

	using element_type    = T;                                ///< the element type, possibly const-qualified
	using value_type      = typename std::remove_cv<T>::type; ///< the element type with cv-qualifiers removed
	using size_type       = std::size_t;                      ///< unsigned size type
	using difference_type = std::ptrdiff_t;                   ///< signed difference type
	using pointer         = T*;                               ///< pointer to element
	using const_pointer   = const T*;                         ///< pointer to const element
	using reference       = T&;                               ///< reference to element
	using const_reference = const T&;                         ///< reference to const element
	using iterator        = pointer;                          ///< iterator type (raw pointer, zero overhead)
	using reverse_iterator = std::reverse_iterator<iterator>; ///< reverse iterator type

	// --- constructors --------------------------------------------------------

	/// Default-construct an empty span (data() == nullptr, size() == 0).
	/// @note Only available for dynamic-extent spans.
	constexpr KSpan() noexcept
	: m_pData(nullptr)
	, m_iSize(0)
	{
	}

	/// Construct a span from a pointer and an element count.
	/// @param pData   pointer to the first element
	/// @param iCount  number of elements in the range
	constexpr KSpan(pointer pData, size_type iCount) noexcept
	: m_pData(pData)
	, m_iSize(iCount)
	{
	}

	/// Construct a span from a half-open pointer range [pFirst, pLast).
	/// @param pFirst  pointer to the first element
	/// @param pLast   pointer past the last element
	constexpr KSpan(pointer pFirst, pointer pLast) noexcept
	: m_pData(pFirst)
	, m_iSize(static_cast<size_type>(pLast - pFirst))
	{
	}

	/// Construct a span from a C-style array.
	/// @tparam N  array size, deduced automatically
	template <std::size_t N>
	constexpr KSpan(element_type (&arr)[N]) noexcept
	: m_pData(arr)
	, m_iSize(N)
	{
	}

	/// Construct a span from a mutable std::array.
	/// @tparam N  array size, deduced automatically
	template <std::size_t N>
	constexpr KSpan(std::array<value_type, N>& arr) noexcept
	: m_pData(arr.data())
	, m_iSize(N)
	{
	}

	/// Construct a span from a const std::array.
	/// @tparam N  array size, deduced automatically
	template <std::size_t N>
	constexpr KSpan(const std::array<value_type, N>& arr) noexcept
	: m_pData(arr.data())
	, m_iSize(N)
	{
	}

	/// Construct a span from any contiguous container that provides
	/// .data() and .size() (e.g. std::vector, std::string, KString).
	///
	/// @tparam Container  a type with .data() returning a pointer convertible
	///                    to T* and .size() returning a size. Must not be a
	///                    raw array or another KSpan (handled by other ctors).
	///
	/// SFINAE ensures this overload is only selected when the container's
	/// data pointer is convertible to our element pointer.
	template <typename Container,
	          typename = typename std::enable_if<
	              !std::is_array<typename std::remove_reference<Container>::type>::value &&
	              !std::is_same<typename std::decay<Container>::type, KSpan>::value &&
	              std::is_convertible<
	                  decltype(std::declval<Container&>().data()),
	                  pointer
	              >::value
	          >::type>
	constexpr KSpan(Container& c) noexcept
	: m_pData(c.data())
	, m_iSize(c.size())
	{
	}

	/// Construct a span from a const contiguous container.
	/// @see the mutable container overload above for details
	template <typename Container,
	          typename = typename std::enable_if<
	              !std::is_array<typename std::remove_reference<Container>::type>::value &&
	              !std::is_same<typename std::decay<Container>::type, KSpan>::value &&
	              std::is_convertible<
	                  decltype(std::declval<const Container&>().data()),
	                  pointer
	              >::value
	          >::type>
	constexpr KSpan(const Container& c) noexcept
	: m_pData(c.data())
	, m_iSize(c.size())
	{
	}

	// --- observers -----------------------------------------------------------

	/// @returns pointer to the first element, or nullptr if empty
	DEKAF2_NODISCARD
	constexpr pointer   data       () const noexcept { return m_pData;              }

	/// @returns the number of elements in the span
	DEKAF2_NODISCARD
	constexpr size_type size       () const noexcept { return m_iSize;              }

	/// @returns the size of the referenced memory region in bytes
	DEKAF2_NODISCARD
	constexpr size_type size_bytes () const noexcept { return m_iSize * sizeof(T);  }

	/// @returns true if the span is empty (size() == 0)
	DEKAF2_NODISCARD
	constexpr bool      empty      () const noexcept { return m_iSize == 0;         }

	// --- element access ------------------------------------------------------

	/// Access element by index (unchecked).
	/// @param i  zero-based index
	/// @returns reference to the element at position @p i
	DEKAF2_NODISCARD
	constexpr reference operator[] (size_type i) const { return m_pData[i];           }

	/// @returns reference to the first element (undefined if empty)
	DEKAF2_NODISCARD
	constexpr reference front      ()            const { return m_pData[0];           }

	/// @returns reference to the last element (undefined if empty)
	DEKAF2_NODISCARD
	constexpr reference back       ()            const { return m_pData[m_iSize - 1]; }

	// --- iterators -----------------------------------------------------------

	/// @returns iterator to the first element
	DEKAF2_NODISCARD
	constexpr iterator         begin  () const noexcept { return m_pData;                   }

	/// @returns iterator past the last element
	DEKAF2_NODISCARD
	constexpr iterator         end    () const noexcept { return m_pData + m_iSize;         }

	/// @returns reverse iterator to the last element
	DEKAF2_NODISCARD
	constexpr reverse_iterator rbegin () const noexcept { return reverse_iterator(end());   }

	/// @returns reverse iterator before the first element
	DEKAF2_NODISCARD
	constexpr reverse_iterator rend   () const noexcept { return reverse_iterator(begin()); }

	// --- subviews ------------------------------------------------------------

	/// @returns a span over the first @p iCount elements
	/// @param iCount  number of elements (must be <= size())
	DEKAF2_NODISCARD
	constexpr KSpan first (size_type iCount) const
	{
		return { m_pData, iCount };
	}

	/// @returns a span over the last @p iCount elements
	/// @param iCount  number of elements (must be <= size())
	DEKAF2_NODISCARD
	constexpr KSpan last (size_type iCount) const
	{
		return { m_pData + m_iSize - iCount, iCount };
	}

	/// @returns a sub-span starting at @p iOffset with @p iCount elements.
	///          If @p iCount is dynamic_extent (the default), the sub-span
	///          extends to the end of the original span.
	/// @param iOffset  start index (must be <= size())
	/// @param iCount   number of elements, or dynamic_extent for "the rest"
	DEKAF2_NODISCARD
	constexpr KSpan subspan(size_type iOffset, size_type iCount = dynamic_extent) const
	{
		return { m_pData + iOffset,
		         iCount == dynamic_extent ? m_iSize - iOffset : iCount };
	}

//----------
private:
//----------

	pointer   m_pData;  ///< pointer to the first element (non-owning)
	size_type m_iSize;  ///< number of elements in the span

}; // KSpan

#endif // DEKAF2_HAS_STD_SPAN

// ============================================================================
// free functions
// ============================================================================

// --- as_bytes / as_writable_bytes -------------------------------------------
// Using uint8_t instead of std::byte — works from C++11 onwards and is the
// more practical choice for e.g. network code.

/// Reinterpret a span of T as a read-only span of raw bytes (uint8_t).
/// The returned span has size() == source.size_bytes().
///
/// @code
///   std::array<int16_t, 100> samples;
///   KSpan<const uint8_t> raw = kAsBytes(KSpan<const int16_t>(samples));
/// @endcode
///
/// @tparam T       element type of the source span
/// @tparam Extent  extent of the source span
/// @param  source  the span to reinterpret
/// @returns a KSpan<const uint8_t> viewing the same memory as raw bytes
template <typename T, std::size_t Extent = dynamic_extent>
DEKAF2_NODISCARD
KSpan<const uint8_t> kAsBytes(KSpan<T, Extent> source) noexcept
{
	return { reinterpret_cast<const uint8_t*>(source.data()), source.size_bytes() };
}

/// Reinterpret a span of non-const T as a writable span of raw bytes (uint8_t).
/// The returned span has size() == source.size_bytes().
///
/// @code
///   std::vector<int16_t> buf(256);
///   KSpan<uint8_t> raw = kAsWritableBytes(KSpan<int16_t>(buf));
///   memset(raw.data(), 0, raw.size());
/// @endcode
///
/// @tparam T       element type of the source span (must not be const-qualified)
/// @tparam Extent  extent of the source span
/// @param  source  the span to reinterpret
/// @returns a KSpan<uint8_t> viewing the same memory as writable raw bytes
template <typename T, std::size_t Extent = dynamic_extent,
          typename = typename std::enable_if<!std::is_const<T>::value>::type>
DEKAF2_NODISCARD
KSpan<uint8_t> kAsWritableBytes(KSpan<T, Extent> source) noexcept
{
	return { reinterpret_cast<uint8_t*>(source.data()), source.size_bytes() };
}

/// @}

DEKAF2_NAMESPACE_END
